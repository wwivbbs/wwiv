/****************************************************************************
*																			*
*							cryptlib PKCS #11 Routines						*
*						Copyright Peter Gutmann 1998-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "pkcs11_api.h"
  #include "dev_mech.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "device/pkcs11_api.h"
  #include "enc_dec/asn1.h"
  #include "mechs/dev_mech.h"
#endif /* Compiler-specific includes */

/* Define the following to generate conventional/MAC keys inside the PKCS 
   #11 device rather than in cryptlib.  Note that this imposes a number of
   restrictions on the use of encryption keys, see the note for
   cipherGenerateKey() for more details */

#define USE_HW_KEYGEN

/* Some devices use extended login mechanisms to get around PKCS #11's
   somewhat-simplistic username-and-password model, if we detect the
   presence of the extended mechanism we enable extended-login
   functionality */

#if 0		/* Private vendor #1's extensions for token login */
#define CKU_EXTENDED		3
/* When CKU_EXTENDED is used, the data passed to C_Login() is no longer a
   straight string but the following structured value */
typedef struct {
	CK_UTF8CHAR_PTR	*username;	/* User name */
	CK_ULONG name_len;			/* Length of user name */
	CK_VOID_PTR context;		/* Token-specific context data */
	CK_ULONG context_size;		/* Size of context data */
	} CK_EXTENDED_LOGIN;
#endif /* 0 */

#ifdef CKU_EXTENDED
  #define USE_EXTENDED_LOGIN
#endif /* CKU_EXTENDED */

#ifdef USE_PKCS11

/* The max. number of drivers we can work with and the max.number of slots
   per driver */

#define MAX_PKCS11_DRIVERS		5
#define MAX_PKCS11_SLOTS		16

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Map a PKCS #11-specific error to a cryptlib error */

CHECK_RETVAL \
int pkcs11MapError( const CK_RV errorCode,
					IN_STATUS const int defaultError )
	{
	REQUIRES( cryptStatusError( defaultError ) );

	switch( ( int ) errorCode )
		{
		case CKR_OK:
			return( CRYPT_OK );

		case CKR_HOST_MEMORY:
		case CKR_DEVICE_MEMORY:
			return( CRYPT_ERROR_MEMORY );

		case CKR_DEVICE_ERROR:
		case CKR_DEVICE_REMOVED:
		case CKR_TOKEN_NOT_PRESENT:
			return( CRYPT_ERROR_SIGNALLED );

		case CKR_PIN_INCORRECT:
		case CKR_PIN_INVALID:
		case CKR_PIN_LEN_RANGE:
		case CKR_PIN_EXPIRED:
		case CKR_PIN_LOCKED:
			return( CRYPT_ERROR_WRONGKEY );

		case CKR_DATA_INVALID:
		case CKR_ENCRYPTED_DATA_INVALID:
		case CKR_WRAPPED_KEY_INVALID:
			return( CRYPT_ERROR_BADDATA );

		case CKR_SIGNATURE_INVALID:
			return( CRYPT_ERROR_SIGNATURE );

		case CKR_KEY_NOT_WRAPPABLE:
		case CKR_KEY_UNEXTRACTABLE:
		case CKR_TOKEN_WRITE_PROTECTED:
		case CKR_INFORMATION_SENSITIVE:
			return( CRYPT_ERROR_PERMISSION );

		case CKR_DATA_LEN_RANGE:
		case CKR_ENCRYPTED_DATA_LEN_RANGE:
		case CKR_SIGNATURE_LEN_RANGE:
		case CKR_UNWRAPPING_KEY_SIZE_RANGE:
		case CKR_WRAPPING_KEY_SIZE_RANGE:
		case CKR_WRAPPED_KEY_LEN_RANGE:
			return( CRYPT_ERROR_OVERFLOW );

		case CKR_SESSION_EXISTS:
		case CKR_SESSION_READ_ONLY_EXISTS:
		case CKR_SESSION_READ_WRITE_SO_EXISTS:
		case CKR_USER_ALREADY_LOGGED_IN:
		case CKR_USER_ANOTHER_ALREADY_LOGGED_IN:
		case CKR_CRYPTOKI_NOT_INITIALIZED:
			return( CRYPT_ERROR_INITED );

		case CKR_USER_NOT_LOGGED_IN:
		case CKR_USER_PIN_NOT_INITIALIZED:
		case CKR_CRYPTOKI_ALREADY_INITIALIZED:
			return( CRYPT_ERROR_NOTINITED );

		case CKR_RANDOM_NO_RNG:
			return( CRYPT_ERROR_RANDOM );

		case CKR_OPERATION_ACTIVE:
			return( CRYPT_ERROR_TIMEOUT );

		case CKR_TOKEN_NOT_RECOGNIZED:
			return( CRYPT_ERROR_NOTFOUND );
		}

	return( defaultError );
	}

/* Extract the time from a PKCS #11 tokenInfo structure */

STDC_NONNULL_ARG( ( 1 ) ) \
time_t getTokenTime( const CK_TOKEN_INFO *tokenInfo )
	{
	STREAM stream;
	BYTE buffer[ 32 + 8 ];
	time_t theTime = MIN_TIME_VALUE + 1;
	int length = DUMMY_INIT, status;

	assert( isReadPtr( tokenInfo, sizeof( CK_TOKEN_INFO ) ) );

	/* Convert the token time to an ASN.1 time string that we can read using
	   the standard ASN.1 routines by writing a dummy time value and inserting 
	   the token's time string in its place */
	sMemOpen( &stream, buffer, 32 );
	status = writeGeneralizedTime( &stream, theTime, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( 0 );
	memcpy( buffer + 2, tokenInfo->utcTime, 14 );
	sMemConnect( &stream, buffer, length );
	status = readGeneralizedTime( &stream, &theTime );
	sMemDisconnect( &stream );
	
	return( ( cryptStatusOK( status ) ) ? theTime : 0 );
	}

/* Get access to the PKCS #11 device associated with a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getContextDeviceInfo( IN_HANDLE const CRYPT_HANDLE iCryptContext,
						  OUT_HANDLE_OPT CRYPT_DEVICE *iCryptDevice, 
						  OUT_OPT_PTR PKCS11_INFO **pkcs11InfoPtrPtr )
	{
	CRYPT_DEVICE iLocalDevice;
	DEVICE_INFO *deviceInfo;
	int cryptStatus;

	assert( isWritePtr( iCryptDevice, sizeof( CRYPT_DEVICE ) ) );
	assert( isWritePtr( pkcs11InfoPtrPtr, sizeof( PKCS11_INFO * ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Clear return values */
	*iCryptDevice = CRYPT_ERROR;
	*pkcs11InfoPtrPtr = NULL;

	/* Get the the device associated with this context */
	cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_GETDEPENDENT, 
								   &iLocalDevice, OBJECT_TYPE_DEVICE );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Get the PKCS #11 information from the device information */
	cryptStatus = krnlAcquireObject( iLocalDevice, OBJECT_TYPE_DEVICE, 
									 ( void ** ) &deviceInfo, 
									 CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	*iCryptDevice = iLocalDevice;
	*pkcs11InfoPtrPtr = deviceInfo->devicePKCS11;

	return( CRYPT_OK );
	}

/* Create and try and use a dummy object to check for various PKCS #11 
   driver bugs */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkDriverBugs( const PKCS11_INFO *pkcs11Info )
	{
	static const CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	const CK_KEY_TYPE type = CKK_DES;
	static const CK_BBOOL bFalse = FALSE, bTrue = TRUE;
	const CK_ATTRIBUTE keyTemplate[] = {
		{ CKA_CLASS, ( CK_VOID_PTR ) &class, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bFalse, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_ENCRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE, "12345678", 8 }	/* Dummy key value */
		};
	const CK_MECHANISM mechanism = { CKM_DES_ECB, NULL, 0 };
	CK_OBJECT_HANDLE hObject;
	CK_RV status;

	assert( isReadPtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	/* Try and create the sort of object that'd normally require a login.  
	   This can fail for reasons other than driver bugs (for example DES 
	   isn't supported for this token type) so we only check for the 
	   specific error code returned by a login bug */
	status = C_CreateObject( pkcs11Info->hSession, 
							 ( CK_ATTRIBUTE_PTR ) keyTemplate, 7, &hObject );
	if( status == CKR_USER_NOT_LOGGED_IN )
		{
		DEBUG_DIAG(( "PKCS #11 driver bug detected, attempt to log in to "
					 "the device apparently succeeded but logged-on "
					 "operation failed with CKR_USER_NOT_LOGGED_IN" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTINITED );
		}
	ENSURES( hObject != CK_OBJECT_NONE );

	/* Try and use the object to encrypt data (or at least call the pre-
	   encrypt call, which should be enough to shake out most bugs) */
	status = C_EncryptInit( pkcs11Info->hSession, 
							( CK_MECHANISM_PTR ) &mechanism, hObject );
	C_DestroyObject( pkcs11Info->hSession, hObject );
	if( status != CKR_OK )
		{
		DEBUG_DIAG(( "PKCS #11 driver bug detected, attempt to use object "
					 "in logged-in device failed with error code %lX, this "
					 "can happen when using C_InitToken() rather than the "
					 "proprietary vendor-supplied utility to initialise "
					 "the device", status ));
		assert( DEBUG_WARN );
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Device Init/Shutdown/Device Control Routines			*
*																			*
****************************************************************************/

/* Handle device control functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int controlFunction( INOUT DEVICE_INFO *deviceInfo,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
							IN_BUFFER_OPT( dataLength ) void *data, 
							IN_LENGTH_SHORT_Z const int dataLength,
							INOUT_OPT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	CK_RV status;
	PKCS11_INFO *pkcs11Info = deviceInfo->devicePKCS11;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isAttribute( type ) || isInternalAttribute( type ) );

	/* Handle token present/active checks */
	if( type == CRYPT_DEVINFO_LOGGEDIN )
		{
		CK_TOKEN_INFO tokenInfo;
		CK_SLOT_INFO slotInfo;

		/* Check whether the user is still logged in.  This is rather 
		   problematic because some devices can't detect a token removal, 
		   and if they do they often can't report it to the driver.  It's 
		   also possible in some devices to remove the token and re-insert 
		   it later without that being regarded as logging out (or you can 
		   remove the smart card and insert your frequent flyer card and 
		   it's still regarded as a card present).  In addition if the 
		   reader supports its own authentication mechanisms (even if it 
		   forces a logout if the token is removed) it's possible for the 
		   user to reinsert the token and reauthenticate themselves and it 
		   appears as if they never logged out.  In fact the only totally 
		   foolproof way to detect a token removal/change is to try and use 
		   the token to perform a crypto operation, which is a rather 
		   suboptimal detection mechanism.

		   Because of this, the best that we can do here is check the token-
		   present flag and report a token-changed error if it's not set.  
		   In addition since some devices only do a minimal check with
		   C_GetSlotInfo() (e.g. checking whether a microswitch is held
		   open by something in the slot, see above) we first call
		   C_GetTokenInfo(), which has a greater chance of actually trying
		   to access the token, before we call C_GetSlotInfo().

		   If there's a problem reported, we don't perform an implicit 
		   shutdown since the user may choose to re-authenticate to the 
		   device or perform some other action that we have no control over 
		   in response to the token-removed notification */
		status = C_GetTokenInfo( pkcs11Info->slotID, &tokenInfo );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_SIGNALLED ) );
		status = C_GetSlotInfo( pkcs11Info->slotID, &slotInfo );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_SIGNALLED ) );
		if( !( slotInfo.flags & CKF_TOKEN_PRESENT ) )
			return( CRYPT_ERROR_SIGNALLED );

		return( CRYPT_OK );
		}

	/* Handle user authorisation */
	if( type == CRYPT_DEVINFO_AUTHENT_USER || \
		type == CRYPT_DEVINFO_AUTHENT_SUPERVISOR )
		{
#ifdef USE_EXTENDED_LOGIN
		const CK_USER_TYPE userType = CKU_EXTENDED;
#else
		const CK_USER_TYPE userType = \
				( type == CRYPT_DEVINFO_AUTHENT_USER ) ? CKU_USER : CKU_SO;
#endif /* USE_EXTENDED_LOGIN */
		
		/* Make sure that the PIN is within range */
		if( dataLength < pkcs11Info->minPinSize || \
			dataLength > pkcs11Info->maxPinSize )
			return( CRYPT_ARGERROR_NUM1 );

		/* If the user is already logged in, log them out before we try
		   logging in with a new authentication value */
		if( deviceInfo->flags & DEVICE_LOGGEDIN )
			{
			C_Logout( pkcs11Info->hSession );
			deviceInfo->flags &= ~DEVICE_LOGGEDIN;
			}

		/* Authenticate the user to the device */
		status = C_Login( pkcs11Info->hSession, userType, 
						  ( CK_CHAR_PTR ) data,
						  ( CK_ULONG ) dataLength );
		if( status != CKR_OK )
			{
			int cryptStatus;

			/* The check for CKR_USER_ALREADY_LOGGED_IN is logical since we 
			   may already be logged in from another session, however 
			   several buggy drivers return CKR_USER_ALREADY_LOGGED_IN 
			   without actually logging the user in so that all further 
			   operations fail with CKR_USER_NOT_LOGGED_IN.  To try and
			   detect this, if we get a CKR_USER_ALREADY_LOGGED_IN we try
			   and create the sort of object that's likely to require a
			   login and use that to see whether we're really logged in or
			   not */
			if( status != CKR_USER_ALREADY_LOGGED_IN )
				return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
			cryptStatus = checkDriverBugs( pkcs11Info );
			return( cryptStatusError( cryptStatus ) ? \
					cryptStatus : CRYPT_ERROR_FAILED );
			}

		/* The device is now ready for use */
		deviceInfo->flags |= DEVICE_LOGGEDIN;
		return( CRYPT_OK );
		}

	/* Handle authorisation value changes.  The initialise SO/user PIN 
	   functionality is a bit awkward in that it has to fill the gap between 
	   C_InitToken() (which usually sets the SSO PIN but may also take an
	   initialisation PIN and leave the token in a state where the only valid
	   operation is to set the SSO PIN) and C_SetPIN() (which can only set the 
	   SSO PIN for the SSO or the user PIN for the user).  Setting the user 
	   PIN by the SSO, which is usually required to perform any useful (non-
	   administrative) function with the token, requires the special-case 
	   C_InitPIN().  In addition we can't speculatively set the user PIN to 
	   be the same as the SSO PIN (which would be useful because in most 
	   cases the user *is* the SSO, thus ensuring that the device behaves as 
	   expected when the user isn't even aware that there are SSO and user 
	   roles) because devices that implement an FSM for initialisation will 
	   move into an undesired state once the SSO -> user change is triggered.

	   The FSM for initialisation on devices that perform a multi-stage
	   bootstrap and require all of the various intialisation functions to
	   be used one after the other (e.g. Fortezza) is:

			uninitialised/zeroised
					v
				C_InitToken			(enter init or SSO PIN)
					v
				initialised
					v
				C_SetPIN			(change init PIN -> SSO PIN)
					v
			  SSO initialised
					v
				C_InitPIN			(set user PIN)
					v
			  user initialised
					v
				C_Logout
				C_Login				(move from SO -> user state)

		The final logout/login is only needed with some tokens, in others
		the move to user state is automatic once the user PIN is set by the
		SO */
	if( type == CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR )
		{
		/* Make sure that the PIN is within range */
		if( dataLength < pkcs11Info->minPinSize || \
			dataLength > pkcs11Info->maxPinSize )
			return( CRYPT_ARGERROR_NUM1 );

		/* Make sure that there's an SSO PIN present from a previous device
		   initialisation */
		if( pkcs11Info->defaultSSOPinLen <= 0 )
			{
			setErrorInfo( deviceInfo, CRYPT_DEVINFO_INITIALISE, 
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}

		/* Change the SSO PIN from the initialisation PIN.  Once we've done 
		   this we clear the initial SSO PIN, since it's no longer valid in 
		   the new state */
		status = C_SetPIN( pkcs11Info->hSession, pkcs11Info->defaultSSOPin,
						   pkcs11Info->defaultSSOPinLen, 
						   ( CK_CHAR_PTR ) data, ( CK_ULONG ) dataLength );
		zeroise( pkcs11Info->defaultSSOPin, CRYPT_MAX_TEXTSIZE );
		pkcs11Info->defaultSSOPinLen = 0;
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
		}
	if( type == CRYPT_DEVINFO_SET_AUTHENT_USER )
		{
		/* Make sure that the PIN is within range */
		if( dataLength < pkcs11Info->minPinSize || \
			dataLength > pkcs11Info->maxPinSize )
			return( CRYPT_ARGERROR_NUM1 );

		status = C_InitPIN( pkcs11Info->hSession, ( CK_CHAR_PTR ) data, 
							( CK_ULONG ) dataLength );
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
		}

	/* Handle initialisation and zeroisation */
	if( type == CRYPT_DEVINFO_INITIALISE || \
		type == CRYPT_DEVINFO_ZEROISE )
		{
		CK_SESSION_HANDLE hSession;
		CK_CHAR label[ 32 + 8 ];
		int cryptStatus;

		/* Make sure that the PIN is within range */
		if( dataLength < pkcs11Info->minPinSize || \
			dataLength > pkcs11Info->maxPinSize )
			return( CRYPT_ARGERROR_NUM1 );

		/* If there's a session active with the device, log out and terminate
		   the session, since the token initialisation will reset this */
		if( pkcs11Info->hSession != CK_OBJECT_NONE )
			{
			C_Logout( pkcs11Info->hSession );
			C_CloseSession( pkcs11Info->hSession );
			pkcs11Info->hSession = CK_OBJECT_NONE;
			}

		/* Initialise/clear the device, setting the initial SSO PIN */
		memset( label, ' ', 32 );
		status = C_InitToken( pkcs11Info->slotID, 
							  ( CK_CHAR_PTR ) data,
							  ( CK_ULONG ) dataLength, label );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

		/* Reopen the session with the device */
		status = C_OpenSession( pkcs11Info->slotID,
								CKF_RW_SESSION | CKF_SERIAL_SESSION,
								NULL_PTR, NULL_PTR, &hSession );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_OPEN ) );
		ENSURES( hSession != CK_OBJECT_NONE );
		pkcs11Info->hSession = hSession;

		/* If it's a straight zeroise, we're done */
		if( type == CRYPT_DEVINFO_ZEROISE )
			return( CRYPT_OK );

		/* We're initialising it, log in as supervisor.  In theory we could 
		   also set the initial user PIN to the same as the SSO PIN at this
		   point because the user usually won't be aware of the presence of
		   an SSO role or the need to set a PIN for it, but this can run into
		   problems with tokens that only allow the user PIN to be modified
		   by the SSO after they've set it for the first time, so if the user
		   *is* aware of the existence of an SSO role then once they log in
		   as SSO they can no longer set the user PIN */
		status = C_Login( pkcs11Info->hSession, CKU_SO,
						  ( CK_CHAR_PTR ) data, ( CK_ULONG ) dataLength );
		if( status != CKR_OK )
			{
			C_Logout( pkcs11Info->hSession );
			C_CloseSession( pkcs11Info->hSession );
			pkcs11Info->hSession = CK_OBJECT_NONE;
			return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
			}

		/* Remember the default SSO PIN for use with a future C_SetPIN() */
		memcpy( pkcs11Info->defaultSSOPin, data, dataLength );
		pkcs11Info->defaultSSOPinLen = dataLength;

		/* A number of PKCS #11 devices can't actually be initialised 
		   through C_InitToken() but require a vendor-supplied proprietary
		   application to do this, apparently succeeding with C_InitToken() 
		   but then failing in various strange and unpredictable ways later
		   on.  We try and detect this situation here by creating a dummy
		   object in the device and trying to use it */
		cryptStatus = checkDriverBugs( pkcs11Info );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );

		/* We're logged in and ready to go */
		deviceInfo->flags |= DEVICE_LOGGEDIN;
		return( CRYPT_OK );
		}

	/* Handle high-reliability time */
	if( type == CRYPT_IATTRIBUTE_TIME )
		{
		CK_TOKEN_INFO tokenInfo;
		time_t *timePtr = ( time_t * ) data, theTime;

		/* Get the token's time, returned as part of the token information 
		   structure */
		status = C_GetTokenInfo( pkcs11Info->slotID, &tokenInfo );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_SIGNALLED ) );
		if( ( theTime = getTokenTime( &tokenInfo ) ) <= MIN_TIME_VALUE )
			return( CRYPT_ERROR_NOTAVAIL );
		*timePtr = theTime;
		return( CRYPT_OK );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*						 	Capability Interface Routines					*
*																			*
****************************************************************************/

/* Encrypt, decrypt */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int genericEncrypt( const PKCS11_INFO *pkcs11Info, 
						   const CONTEXT_INFO *contextInfoPtr,
						   const CK_MECHANISM *pMechanism, 
						   INOUT_BUFFER_FIXED( length ) void *buffer,
						   IN_LENGTH const int length, 
						   IN_LENGTH const int outLength )
	{
	CK_ULONG resultLen = outLength;
	CK_RV status;

	assert( isReadPtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( pMechanism, sizeof( CK_MECHANISM ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length > 0 && length < MAX_INTLENGTH );
	REQUIRES( length == outLength );

	status = C_EncryptInit( pkcs11Info->hSession,
							( CK_MECHANISM_PTR ) pMechanism,
							contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Encrypt( pkcs11Info->hSession, buffer, length,
							buffer, &resultLen );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int genericDecrypt( const PKCS11_INFO *pkcs11Info, 
						   const CONTEXT_INFO *contextInfoPtr,
						   const CK_MECHANISM *pMechanism, 
						   INOUT_BUFFER_FIXED( length ) void *buffer,
						   IN_LENGTH const int length )
	{
	CK_ULONG resultLen = length;
	CK_RV status;

	assert( isReadPtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( pMechanism, sizeof( CK_MECHANISM ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	status = C_DecryptInit( pkcs11Info->hSession,
							( CK_MECHANISM_PTR ) pMechanism,
							contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Decrypt( pkcs11Info->hSession, buffer, length,
							buffer, &resultLen );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
	return( CRYPT_OK );
	}

/* Clean up the object associated with a context.  The CONTEXT_INFO * is
   actually a const in this case but we need to leave it non-const to make
   it type-compatible with the function pointer in the CONTEXT_INFO */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int genericEndFunction( /* const */ INOUT CONTEXT_INFO *contextInfoPtr )
	{
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	int cryptStatus;

	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Since the device object that corresponds to the cryptlib object is
	   created on-demand, it may not exist yet if the action that triggers
	   the on-demand creation hasn't been taken yet.  If no device object
	   exists, we're done */
	if( contextInfoPtr->deviceObject == CRYPT_ERROR )
		return( CRYPT_OK );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* If we're deleting an object that's in the middle of a multi-stage 
	   operation, record the fact that the operation has now ended.  We
	   have to perform this tracking explicitly since PKCS #11 only allows
	   one multi-stage operation per session */
	if( pkcs11Info->hActiveSignObject == contextInfoPtr->deviceObject )
		pkcs11Info->hActiveSignObject = CK_OBJECT_NONE;

	/* If this is a persistent object, we can't destroy it.  This is a bit
	   problematic since PKCS #11 doesn't differentiate between releasing
	   an object handle and destroying (deleting) it, which means that
	   repeatedly instantiating a persistent object (via getItemFunction())
	   and then destroying it leaks a PKCS #11 handle each time.  
	   Unfortunately there's nothing that we can do about this since the
	   problem lies at the PKCS #11 level */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		{
		krnlReleaseObject( iCryptDevice );
		return( CRYPT_OK );
		}

	/* Destroy the object */
	C_DestroyObject( pkcs11Info->hSession, contextInfoPtr->deviceObject );
	if( contextInfoPtr->altDeviceObject != CK_OBJECT_NONE )
		{
		C_DestroyObject( pkcs11Info->hSession, 
						 contextInfoPtr->altDeviceObject );
		}
	krnlReleaseObject( iCryptDevice );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Conventional Crypto/MAC Key Load Functions				*
*																			*
****************************************************************************/

/* Get a PKCS #11 mechanism corresponding to a cryptlib algorithm and 
   optional mode */

typedef enum { MECH_NONE, MECH_CONV, MECH_MAC, MECH_CONV_KEYGEN, 
			   MECH_MAC_KEYGEN, MECH_LAST } GETMECH_TYPE;

static CK_MECHANISM_TYPE getMechanism( const GETMECH_TYPE mechType,
									   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
									   IN_MODE const CRYPT_MODE_TYPE cryptMode );

/* Set up a key template and context information in preparation for creating 
   a device object */

STDC_NONNULL_ARG( ( 1 ) ) \
static void adjustKeyParity( INOUT_BUFFER_FIXED( length ) BYTE *key, 
							 IN_LENGTH_SHORT const int length )
	{
	int i;

	assert( isWritePtr( key, length ) );

	REQUIRES_V( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* Adjust a key to have odd parity, needed for DES keys */
	for( i = 0; i < length; i++ )
		{
		int ch = byteToInt( key[ i ] );
		
		ch = ( ch & 0x55 ) + ( ( ch >> 1 ) & 0x55 );
		ch = ( ch & 0x33 ) + ( ( ch >> 2 ) & 0x33 );
		if( !( ( ch + ( ch >> 4 ) ) & 0x01 ) )
			key[ i ] ^= 1;
		}
	}

/* Load a key into a device object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int initKey( INOUT CONTEXT_INFO *contextInfoPtr, 
					INOUT_ARRAY( templateCount ) \
						CK_ATTRIBUTE *keyTemplate, 
					IN_RANGE( 4, 10 ) const int templateCount, 
					IN_BUFFER( keyLength ) const void *key, 
					IN_LENGTH_SHORT const int keyLength )
	{
	CRYPT_DEVICE iCryptDevice;
	const CRYPT_ALGO_TYPE cryptAlgo = \
				contextInfoPtr->capabilityInfo->cryptAlgo;
	PKCS11_INFO *pkcs11Info;
	CK_OBJECT_HANDLE hObject;
	CK_RV status;
	BYTE *contextKeyPtr;
	int *contextKeyLenPtr;
	int keySize = \
		( cryptAlgo == CRYPT_ALGO_DES || cryptAlgo == CRYPT_ALGO_3DES ) ? \
		contextInfoPtr->capabilityInfo->keySize : keyLength;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( keyTemplate, \
						templateCount * sizeof( CK_ATTRIBUTE ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( templateCount >= 4 && templateCount <= 10 );
	REQUIRES( keyLength > 0 && keyLength < MAX_INTLENGTH_SHORT );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up pointers to the appropriate object sub-type data */
	if( contextInfoPtr->type == CONTEXT_CONV )
		{
		contextKeyPtr = contextInfoPtr->ctxConv->userKey;
		contextKeyLenPtr = &contextInfoPtr->ctxConv->userKeyLength;
		}
	else
		{
		REQUIRES( contextInfoPtr->type == CONTEXT_MAC );

		contextKeyPtr = contextInfoPtr->ctxMAC->userKey;
		contextKeyLenPtr = &contextInfoPtr->ctxMAC->userKeyLength;
		}

	/* Copy the key to internal storage */
	if( contextKeyPtr != key )
		memcpy( contextKeyPtr, key, keyLength );
	*contextKeyLenPtr = keyLength;

	/* Special-case handling for 2-key vs.3-key 3DES */
	if( cryptAlgo == CRYPT_ALGO_3DES )
		{
		/* If the supplied key contains only two DES keys, adjust the key to 
		   make it the equivalent of 3-key 3DES.  In addition since the 
		   nominal keysize is for 2-key 3DES, we have to make the actual 
		   size the maximum size, corresponding to 3-key 3DES */
		if( keyLength <= bitsToBytes( 64 * 2 ) )
			memcpy( contextKeyPtr + bitsToBytes( 64 * 2 ),
					contextKeyPtr, bitsToBytes( 64 ) );
		keySize = contextInfoPtr->capabilityInfo->maxKeySize;
		}

	/* If we're using DES we have to adjust the key parity because the spec 
	   says so, almost all implementations do this anyway but there's always 
	   the odd one out that we have to cater for */
	if( cryptAlgo == CRYPT_ALGO_DES || cryptAlgo == CRYPT_ALGO_3DES )
		adjustKeyParity( contextKeyPtr, keySize );

	/* Set up the key values.  Since the key passed in by the user may be 
	   smaller than the keysize required by algorithms that use fixed-size 
	   keys, we use the (optionally) zero-padded key of the correct length 
	   held in the context rather than the variable-length user-supplied 
	   one */
	REQUIRES( keyTemplate[ 7 ].type == CKA_VALUE );
	keyTemplate[ 7 ].pValue = contextKeyPtr;
	keyTemplate[ 7 ].ulValueLen = keySize;

	/* Load the key into the token */
	status = C_CreateObject( pkcs11Info->hSession, keyTemplate, 
							 templateCount, &hObject );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusOK( cryptStatus ) )
		{
		ENSURES( hObject != CK_OBJECT_NONE );

		contextInfoPtr->deviceObject = hObject;
		}
	else
		{
		zeroise( contextInfoPtr->ctxConv->userKey, keyLength );
		contextInfoPtr->ctxConv->userKeyLength = 0;
		}
	zeroise( keyTemplate, sizeof( CK_ATTRIBUTE ) * templateCount );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherInitKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						  IN_BUFFER( keyLength ) const void *key, 
						  IN_LENGTH_SHORT const int keyLength )
	{
	static const CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	static const CK_BBOOL bFalse = FALSE, bTrue = TRUE;
	const CK_KEY_TYPE type = contextInfoPtr->capabilityInfo->paramKeyType;
	CK_ATTRIBUTE keyTemplate[] = {
		/* General-purpose fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &class, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bFalse, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_ENCRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE, NULL_PTR, 0 },
		/* Persistent-object only fields */
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize }
		};
	const int templateCount = \
				( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ? 9 : 8;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( keyLength > 0 && keyLength < MAX_INTLENGTH_SHORT );

	/* If this is meant to be a persistent object, modify the template to 
	   make it a persistent token object and adjust the template entry count
	   to include the object label */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		keyTemplate[ 2 ].pValue = ( CK_VOID_PTR ) &bTrue;

	return( initKey( contextInfoPtr, keyTemplate, templateCount, 
					 key, keyLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int hmacInitKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						IN_BUFFER( keyLength ) const void *key, 
						IN_LENGTH_SHORT const int keyLength )
	{
	static const CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	static const CK_BBOOL bFalse = FALSE, bTrue = TRUE;
	const CK_KEY_TYPE type = contextInfoPtr->capabilityInfo->paramKeyType;
	CK_ATTRIBUTE keyTemplate[] = {
		/* General-purpose fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &class, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bFalse, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VERIFY, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE, NULL_PTR, 0 },
		/* Persistent-object only fields */
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize }
		};
	const int templateCount = \
				( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ? 9 : 8;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( keyLength > 0 && keyLength < MAX_INTLENGTH_SHORT );

	/* If this is meant to be a persistent object, modify the template to 
	   make it a persistent token object and adjust the template entry count
	   to include the object label */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		keyTemplate[ 2 ].pValue = ( CK_VOID_PTR ) &bTrue;

	return( initKey( contextInfoPtr, keyTemplate, templateCount, 
					 key, keyLength ) );
	}

/* Generate a key into a device object.  Normally we generate keys inside
   cryptlib and load them into the device object (so a keygen becomes a
   keygen inside cryptlib followed by a cipherInitKey()) in order to make 
   sure that the key data is accessible from the context.  If we didn't do 
   this, the user would have to be very careful to perform all key wrap/
   unwrap operations only via device objects.  This is particularly
   problematic with public-key operations since cryptlib always instantiates
   public-key objects as cryptlib native objects since they're so much 
   quicker in that form.  So for example importing a certificate and then
   using it to wrap a conventional encryption key that's been generated in
   a device is impossible because the key to wrap isn't accessible to the
   public-key context tied to the certificate.  Because of this, 
   USE_HW_KEYGEN should be used with great care */

#ifdef USE_HW_KEYGEN

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int generateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						INOUT_ARRAY( templateCount ) \
							CK_ATTRIBUTE *keyTemplate, 
						IN_RANGE( 4, 10 ) const int templateCount, 
						const BOOLEAN isMAC )
	{
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	CK_MECHANISM mechanism = { contextInfoPtr->capabilityInfo->paramKeyGen, 
							   NULL_PTR, 0 };
	CK_OBJECT_HANDLE hObject;
	CK_RV status;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( keyTemplate, \
						templateCount * sizeof( CK_ATTRIBUTE ) ) );

	REQUIRES( templateCount >= 4 && templateCount <= 10 );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Generate the key into the token */
	status = C_GenerateKey( pkcs11Info->hSession, &mechanism, 
							keyTemplate, templateCount, &hObject );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusOK( cryptStatus ) )
		{
		ENSURES( hObject != CK_OBJECT_NONE );

		contextInfoPtr->deviceObject = hObject;
		}
	zeroise( keyTemplate, sizeof( CK_ATTRIBUTE ) * templateCount );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int cipherGenerateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
							  IN_RANGE( bytesToBits( MIN_KEYSIZE ),
										bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
								const int keySizeBits )
	{
	static const CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	static const CK_BBOOL bFalse = FALSE, bTrue = TRUE;
	const CK_KEY_TYPE type = contextInfoPtr->capabilityInfo->paramKeyType;
	const CK_ULONG length = bitsToBytes( keySizeBits );
	CK_ATTRIBUTE keyTemplate[] = {
		/* General-purpose fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &class, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bFalse, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_ENCRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE_LEN, ( CK_VOID_PTR ) &length, sizeof( CK_ULONG ) },
		/* Persistent-object only fields */
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize }
		};
	const int templateCount = \
				( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ? 9 : 8;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( keySizeBits >= bytesToBits( MIN_KEYSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_KEYSIZE ) );

	/* If this is meant to be a persistent object, modify the template to 
	   make it a persistent token object and adjust the template entry count
	   to include the object label */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		keyTemplate[ 2 ].pValue = ( CK_VOID_PTR ) &bTrue;

	return( generateKey( contextInfoPtr, keyTemplate, templateCount, FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int hmacGenerateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
							IN_RANGE( bytesToBits( MIN_KEYSIZE ),
									  bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
								const int keySizeBits )
	{
	static const CK_OBJECT_CLASS class = CKO_SECRET_KEY;
	static const CK_BBOOL bFalse = FALSE, bTrue = TRUE;
	const CK_KEY_TYPE type = contextInfoPtr->capabilityInfo->paramKeyType;
	const CK_ULONG length = bitsToBytes( keySizeBits );
	CK_ATTRIBUTE keyTemplate[] = {
		/* General-purpose fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &class, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bFalse, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VERIFY, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE_LEN, ( CK_VOID_PTR ) &length, sizeof( CK_ULONG ) },
		/* Persistent-object only fields */
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize }
		};
	const int templateCount = \
				( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ? 9 : 8;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( keySizeBits >= bytesToBits( MIN_KEYSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_KEYSIZE ) );

	/* If this is meant to be a persistent object, modify the template to 
	   make it a persistent token object and adjust the template entry count
	   to include the object label */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		keyTemplate[ 2 ].pValue = ( CK_VOID_PTR ) &bTrue;

	return( generateKey( contextInfoPtr, keyTemplate, templateCount, TRUE ) );
	}
#else

#define cipherGenerateKey	NULL
#define hmacGenerateKey		NULL

#endif /* USE_HW_KEYGEN */

/****************************************************************************
*																			*
*						 Conventional Crypto Mapping Functions				*
*																			*
****************************************************************************/

/* Set up algorithm-specific encryption parameters */

CHECK_RETVAL_RANGE( 0, 64 ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initCryptParams( const CONTEXT_INFO *contextInfoPtr, 
							INOUT void *paramData )
	{
	const int ivSize = contextInfoPtr->capabilityInfo->blockSize;

	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( paramData, sizeof( int ) ) );
			/* Minimum writable size is integer */

	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RC2 )
		{
		if( contextInfoPtr->ctxConv->mode == CRYPT_MODE_ECB )
			{
			CK_RC2_PARAMS_PTR rc2params = ( CK_RC2_PARAMS_PTR ) paramData;

			*rc2params = 128;
			return( sizeof( CK_RC2_PARAMS ) );
			}
		else
			{
			CK_RC2_CBC_PARAMS_PTR rc2params = ( CK_RC2_CBC_PARAMS_PTR ) paramData;

			rc2params->ulEffectiveBits = 128;
			memcpy( rc2params->iv, contextInfoPtr->ctxConv->currentIV, ivSize );
			return( sizeof( CK_RC2_CBC_PARAMS ) );
			}
		}
	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RC5 )
		{
		if( contextInfoPtr->ctxConv->mode == CRYPT_MODE_ECB )
			{
			CK_RC5_PARAMS_PTR rc5params = ( CK_RC5_PARAMS_PTR ) paramData;

			rc5params->ulWordsize = 4;	/* Word size in bytes = blocksize/2 */
			rc5params->ulRounds = 12;
			return( sizeof( CK_RC5_PARAMS ) );
			}
		else
			{
			CK_RC5_CBC_PARAMS_PTR rc5params = ( CK_RC5_CBC_PARAMS_PTR ) paramData;

			rc5params->ulWordsize = 4;	/* Word size in bytes = blocksize/2 */
			rc5params->ulRounds = 12;
			rc5params->pIv = contextInfoPtr->ctxConv->currentIV;
			rc5params->ulIvLen = ivSize;
			return( sizeof( CK_RC5_CBC_PARAMS ) );
			}
		}

	return( 0 );
	}

/* Encrypt/decrypt data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherEncrypt( INOUT CONTEXT_INFO *contextInfoPtr, 
						  INOUT_BUFFER_FIXED( length ) void *buffer, 
						  IN_LENGTH const int length, 
						  const CK_MECHANISM_TYPE mechanismType )
	{
	CK_MECHANISM mechanism = { mechanismType, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE paramDataBuffer[ 64 + 8 ];
	const int ivSize = contextInfoPtr->capabilityInfo->blockSize;
	int paramSize, cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	/* Set up algorithm and mode-specific parameters */
	paramSize = initCryptParams( contextInfoPtr, &paramDataBuffer );
	if( paramSize > 0 )
		{
		mechanism.pParameter = paramDataBuffer;
		mechanism.ulParameterLen = paramSize;
		}
	else
		{
		/* Even if there are no algorithm-specific parameters, there may 
		   still be a mode-specific IV parameter */
		if( needsIV( contextInfoPtr->ctxConv->mode ) && \
			!isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
			{
			mechanism.pParameter = contextInfoPtr->ctxConv->currentIV;
			mechanism.ulParameterLen = ivSize;
			}
		}

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	cryptStatus = genericEncrypt( pkcs11Info, contextInfoPtr, &mechanism, buffer,
								  length, length );
	if( cryptStatusOK( cryptStatus ) )
		{
		if( needsIV( contextInfoPtr->ctxConv->mode ) && \
			!isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
			{
			/* Since PKCS #11 assumes that either all data is encrypted at 
			   once or that a given mechanism is devoted entirely to a single 
			   operation, we have to preserve the state (the IV) across 
			   calls */
			memcpy( contextInfoPtr->ctxConv->currentIV, \
					( BYTE * ) buffer + length - ivSize, ivSize );
			}
		}
	krnlReleaseObject( iCryptDevice );

	return( cryptStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherDecrypt( INOUT CONTEXT_INFO *contextInfoPtr, 
						  INOUT_BUFFER_FIXED( length ) void *buffer, 
						  IN_LENGTH const int length, 
						  const CK_MECHANISM_TYPE mechanismType )
	{
	CK_MECHANISM mechanism = { mechanismType, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE paramDataBuffer[ 64 + 8 ], ivBuffer[ CRYPT_MAX_IVSIZE + 8 ];
	const int ivSize = contextInfoPtr->capabilityInfo->blockSize;
	int paramSize, cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	/* Set up algorithm and mode-specific parameters */
	paramSize = initCryptParams( contextInfoPtr, &paramDataBuffer );
	if( paramSize > 0 )
		{
		mechanism.pParameter = paramDataBuffer;
		mechanism.ulParameterLen = paramSize;
		}
	else
		/* Even if there are no algorithm-specific parameters, there may 
		   still be a mode-specific IV parameter.  In addition we have to
		   save the end of the ciphertext as the IV for the next block if
		   this is required */
		if( needsIV( contextInfoPtr->ctxConv->mode ) && \
			!isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
			{
			mechanism.pParameter = contextInfoPtr->ctxConv->currentIV;
			mechanism.ulParameterLen = ivSize;
			}
	if( needsIV( contextInfoPtr->ctxConv->mode ) && \
		!isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
		memcpy( ivBuffer, ( BYTE * ) buffer + length - ivSize, ivSize );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	cryptStatus = genericDecrypt( pkcs11Info, contextInfoPtr, &mechanism, buffer,
								  length );
	if( cryptStatusOK( cryptStatus ) )
		{
		if( needsIV( contextInfoPtr->ctxConv->mode ) && \
			!isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
			{
			/* Since PKCS #11 assumes that either all data is encrypted at 
			   once or that a given mechanism is devoted entirely to a single 
			   operation, we have to preserve the state (the IV) across 
			   calls */
			memcpy( contextInfoPtr->ctxConv->currentIV, ivBuffer, ivSize );
			}
		}
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

/* Map a cryptlib algorithm and mode to a PKCS #11 mechanism type, with
   shortcuts for the most frequently-used algorithm(s) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherEncryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_3DES )
		return( cipherEncrypt( contextInfoPtr, buffer, length, CKM_DES3_ECB ) );
	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_AES )
		return( cipherEncrypt( contextInfoPtr, buffer, length, CKM_AES_ECB ) );
	return( cipherEncrypt( contextInfoPtr, buffer, length, 
				getMechanism( MECH_CONV, contextInfoPtr->capabilityInfo->cryptAlgo, 
							  CRYPT_MODE_ECB ) ) );
	}
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherEncryptCBC( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	return( cipherEncrypt( contextInfoPtr, buffer, length, 
						   contextInfoPtr->capabilityInfo->paramDefaultMech ) );
	}
#if defined( USE_RC4 ) 
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherEncryptOFB( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RC4 )
		return( cipherEncrypt( contextInfoPtr, buffer, length, CKM_RC4 ) );
	return( cipherEncrypt( contextInfoPtr, buffer, length, 
				getMechanism( MECH_CONV, contextInfoPtr->capabilityInfo->cryptAlgo, 
							  CRYPT_MODE_OFB ) ) );
	}
#endif /* USE_RC4 */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherDecryptECB( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_3DES )
		return( cipherDecrypt( contextInfoPtr, buffer, length, CKM_DES3_ECB ) );
	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_AES )
		return( cipherDecrypt( contextInfoPtr, buffer, length, CKM_AES_ECB ) );
	return( cipherDecrypt( contextInfoPtr, buffer, length, 
				getMechanism( MECH_CONV, contextInfoPtr->capabilityInfo->cryptAlgo, 
							  CRYPT_MODE_ECB ) ) );
	}
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherDecryptCBC( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	return( cipherDecrypt( contextInfoPtr, buffer, length, 
						   contextInfoPtr->capabilityInfo->paramDefaultMech ) );
	}
#if defined( USE_RC4 ) 
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int cipherDecryptOFB( INOUT CONTEXT_INFO *contextInfoPtr, 
							 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
							 IN_LENGTH int length )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RC4 )
		return( cipherDecrypt( contextInfoPtr, buffer, length, CKM_RC4 ) );
	return( cipherDecrypt( contextInfoPtr, buffer, length, 
				getMechanism( MECH_CONV, contextInfoPtr->capabilityInfo->cryptAlgo, 
							  CRYPT_MODE_OFB ) ) );
	}
#endif /* USE_RC4 */

/****************************************************************************
*																			*
*								MAC Mapping Functions						*
*																			*
****************************************************************************/

/* MAC data.  The PKCS #11 way of handling this is rather problematic since
   the HMAC operation is associated with a session and not with the the HMAC
   object.  This means that we can only have a single HMAC operation in 
   effect at any one time.  To protect against users starting a second 
   HMAC/sign operation, we record the device object handle of the currently 
   active signing object and don't allow any further signature operations to
   be initiated if there's an object currently in use */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int hmac( INOUT CONTEXT_INFO *contextInfoPtr, 
				 INOUT_BUFFER_FIXED( length ) BYTE *buffer, 
				 IN_LENGTH_Z int length )
	{
	CK_MECHANISM mechanism = { contextInfoPtr->capabilityInfo->paramDefaultMech, 
							   NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	CK_RV status;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( length == 0 ) || isWritePtr( buffer, length ) );

	REQUIRES( length >= 0 && length < MAX_INTLENGTH );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* If we're currently in the middle of a multi-stage sign operation we
	   can't start a new one.  We have to perform this tracking explicitly 
	   since PKCS #11 only allows one multi-stage operation per session */
	if( pkcs11Info->hActiveSignObject != CK_OBJECT_NONE && \
		pkcs11Info->hActiveSignObject != contextInfoPtr->deviceObject )
		{
		krnlReleaseObject( iCryptDevice );
		return( CRYPT_ERROR_INCOMPLETE );
		}

	/* If we haven't initialised the MAC operation yet, start it now */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
		{
		status = C_SignInit( pkcs11Info->hSession, &mechanism, 
							 contextInfoPtr->deviceObject );
		if( status != CKR_OK )
			return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );
		contextInfoPtr->flags |= CONTEXT_FLAG_HASH_INITED;
		pkcs11Info->hActiveSignObject = contextInfoPtr->deviceObject;
		}

	if( length > 0 )
		status = C_SignUpdate( pkcs11Info->hSession, buffer, length  );
	else
		{
		CK_ULONG dummy;

		status = C_SignFinal( pkcs11Info->hSession, 
							  contextInfoPtr->ctxMAC->mac, &dummy );
		pkcs11Info->hActiveSignObject = CK_OBJECT_NONE;
		}
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

/****************************************************************************
*																			*
*						 	Device Capability Routines						*
*																			*
****************************************************************************/

/* Conventional encryption and MAC mechanism information */

static const PKCS11_MECHANISM_INFO mechanismInfoConv[] = {
	/* Conventional encryption mechanisms */
	{ CKM_DES_ECB, CKM_DES_KEY_GEN, CKM_DES_CBC, CRYPT_ALGO_DES, CRYPT_MODE_ECB, CKK_DES,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptECB, cipherDecryptECB, NULL, NULL },
	{ CKM_DES_CBC, CKM_DES_KEY_GEN, CKM_DES_CBC, CRYPT_ALGO_DES, CRYPT_MODE_CBC, CKK_DES,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
	{ CKM_DES3_ECB, CKM_DES3_KEY_GEN, CKM_DES3_CBC, CRYPT_ALGO_3DES, CRYPT_MODE_ECB, CKK_DES3,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptECB, cipherDecryptECB, NULL, NULL },
	{ CKM_DES3_CBC, CKM_DES3_KEY_GEN, CKM_DES3_CBC, CRYPT_ALGO_3DES, CRYPT_MODE_CBC, CKK_DES3,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
#ifdef USE_RC2
	{ CKM_RC2_ECB, CKM_RC2_KEY_GEN, CKM_RC2_CBC, CRYPT_ALGO_RC2, CRYPT_MODE_ECB, CKK_RC2,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptECB, cipherDecryptECB, NULL, NULL },
	{ CKM_RC2_CBC, CKM_RC2_KEY_GEN, CKM_RC2_CBC, CRYPT_ALGO_RC2, CRYPT_MODE_CBC, CKK_RC2,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
#endif /* USE_RC2 */
#ifdef USE_RC4
	{ CKM_RC4, CKM_RC4_KEY_GEN, CKM_RC4, CRYPT_ALGO_RC4, CRYPT_MODE_OFB, CKK_RC4,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptOFB, cipherDecryptOFB, NULL, NULL },
#endif /* USE_RC4 */
#ifdef USE_RC5
	{ CKM_RC5_ECB, CKM_RC5_KEY_GEN, CKM_RC5_CBC, CRYPT_ALGO_RC5, CRYPT_MODE_ECB, CKK_RC5,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptECB, cipherDecryptECB, NULL, NULL },
	{ CKM_RC5_CBC, CKM_RC5_KEY_GEN, CKM_RC5_CBC, CRYPT_ALGO_RC5, CRYPT_MODE_CBC, CKK_RC5,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
#endif /* USE_RC5 */
	{ CKM_AES_ECB, CKM_AES_KEY_GEN, CKM_AES_CBC, CRYPT_ALGO_AES, CRYPT_MODE_ECB, CKK_AES,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptECB, cipherDecryptECB, NULL, NULL },
	{ CKM_AES_CBC, CKM_AES_KEY_GEN, CKM_AES_CBC, CRYPT_ALGO_AES, CRYPT_MODE_CBC, CKK_AES,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
#ifdef USE_BLOWFISH
	{ CKM_BLOWFISH_CBC, CKM_BLOWFISH_KEY_GEN, CKM_BLOWFISH_CBC, CRYPT_ALGO_BLOWFISH, CRYPT_MODE_CBC, CKK_BLOWFISH,
	  genericEndFunction, cipherInitKey, cipherGenerateKey, 
	  cipherEncryptCBC, cipherDecryptCBC, NULL, NULL },
#endif /* USE_BLOWFISH */

	/* MAC mechanisms.  The positioning of the encrypt/decrypt functions is 
	   a bit odd because cryptlib treats hashing as an encrypt/decrypt
	   operation while PKCS #11 treats it as a sign/verify operation, the
	   function names correspond to the PKCS #11 usage but the position is
	   for cryptlib usage.  In addition there aren't any HMAC key types so
	   it's necessary to use CKK_GENERIC_SECRET (there's actually a bug in
	   the standard around this because CKK_GENERIC_SECRET keys can't be 
	   used for en/decryption or sign/verify, at the moment some 
	   implementations allow them to be used with HMAC and some don't).  In
	   order to allow for implementations that define their own HMAC keygen
	   and key types, we use macros for CKM_x_HMAC_KEY_GEN and CKK_x_HMAC 
	   that expand either to the vendor-specific type or the generic CKM/CKK
	   types */
#ifdef USE_HMAC_MD5
	{ CKM_MD5_HMAC, CKM_MD5_HMAC_KEY_GEN, CKM_MD5_HMAC, CRYPT_ALGO_HMAC_MD5, CRYPT_MODE_NONE, CKK_MD5_HMAC,
	  genericEndFunction, hmacInitKey, hmacGenerateKey, 
	  hmac, hmac, NULL, NULL },
#endif /* USE_HMAC_MD5 */
	{ CKM_SHA_1_HMAC, CKM_SHA_1_HMAC_KEY_GEN, CKM_SHA_1_HMAC, CRYPT_ALGO_HMAC_SHA1, CRYPT_MODE_NONE, CKK_SHA_1_HMAC,
	  genericEndFunction, hmacInitKey, hmacGenerateKey, 
	  hmac, hmac, NULL, NULL },
#ifdef USE_HMAC_RIPEMD160
	{ CKM_RIPEMD160_HMAC, CKM_RIPEMD160_HMAC_KEY_GEN, CKM_RIPEMD160_HMAC, CRYPT_ALGO_HMAC_RIPEMD160, CRYPT_MODE_NONE, CKK_RIPEMD160_HMAC,
	  genericEndFunction, hmacInitKey, hmacGenerateKey, 
	  hmac, hmac, NULL, NULL },
#endif /* USE_HMAC_RIPEMD160 */
#ifdef USE_HMAC_SHA2
	{ CKM_SHA256_HMAC, CKM_SHA256_HMAC_KEY_GEN, CKM_SHA256_HMAC, CRYPT_ALGO_HMAC_SHA2, CRYPT_MODE_NONE, CKK_SHA256_HMAC,
	  genericEndFunction, hmacInitKey, hmacGenerateKey, 
	  hmac, hmac, NULL, NULL },
#endif /* USE_HMAC_SHA2 */

	{ CKM_NONE, CKM_NONE, CKM_NONE, CRYPT_ERROR, CRYPT_ERROR },
		{ CKM_NONE, CKM_NONE, CKM_NONE, CRYPT_ERROR, CRYPT_ERROR }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
const PKCS11_MECHANISM_INFO *getMechanismInfoConv( OUT_LENGTH_SHORT int *mechanismInfoSize )
	{
	assert( isWritePtr( mechanismInfoSize, sizeof( int ) ) );

	*mechanismInfoSize = FAILSAFE_ARRAYSIZE( mechanismInfoConv, \
											 PKCS11_MECHANISM_INFO );
	return( mechanismInfoConv );
	}

/* Map a cryptlib conventional-encryption algorithm and mode to a PKCS #11
   mechanism */

static CK_MECHANISM_TYPE getMechanism( const GETMECH_TYPE mechType,
									   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
									   IN_MODE const CRYPT_MODE_TYPE cryptMode )
	{
	int i;

	REQUIRES_EXT( ( ( mechType == MECH_CONV && \
					  isConvAlgo( cryptAlgo ) && \
					  cryptMode > CRYPT_MODE_NONE && \
					  cryptMode < CRYPT_MODE_LAST ) || 
					( mechType == MECH_CONV_KEYGEN && \
					  isConvAlgo( cryptAlgo ) && \
					  cryptMode == CRYPT_MODE_NONE ) || 
					( ( mechType == MECH_MAC || \
						mechType == MECH_MAC_KEYGEN ) && \
					  isMacAlgo( cryptAlgo ) && \
					  cryptMode == CRYPT_MODE_NONE ) ), CKM_NONE );

	/* Find a match for the algorithm type.  If it's a MAC algorithm or 
	   keygen mechanism, we're done */
	for( i = 0; mechanismInfoConv[ i ].cryptAlgo != cryptAlgo && \
				mechanismInfoConv[ i ].cryptAlgo != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( mechanismInfoConv, PKCS11_MECHANISM_INFO );
		 i++ );
	ENSURES_EXT( ( i < FAILSAFE_ARRAYSIZE( mechanismInfoConv, PKCS11_MECHANISM_INFO ) ),
				 CKM_NONE );
	ENSURES_EXT( ( i < sizeof( mechanismInfoConv ) / sizeof( PKCS11_MECHANISM_INFO ) && \
				   mechanismInfoConv[ i ].cryptAlgo != CRYPT_ERROR ), CKM_NONE );
	if( mechType == MECH_MAC )
		return( mechanismInfoConv[ i ].mechanism );
	if( mechType == MECH_CONV_KEYGEN || mechType == MECH_MAC_KEYGEN )
		return( mechanismInfoConv[ i ].keygenMechanism );

	/* It's a conventional-encryption mechanism, we have to match the 
	   encryption mode as well */
	ENSURES_EXT( mechType == MECH_CONV, CKM_NONE );
	while( mechanismInfoConv[ i ].cryptMode != cryptMode && \
		   mechanismInfoConv[ i ].cryptAlgo != CRYPT_ERROR && \
		   i < FAILSAFE_ARRAYSIZE( mechanismInfoConv, PKCS11_MECHANISM_INFO ) )
		i++;
	ENSURES_EXT( ( i < FAILSAFE_ARRAYSIZE( mechanismInfoConv, PKCS11_MECHANISM_INFO ) ),
				 CKM_NONE );
	ENSURES_EXT( ( i < sizeof( mechanismInfoConv ) / sizeof( PKCS11_MECHANISM_INFO ) && \
				 mechanismInfoConv[ i ].cryptAlgo != CRYPT_ERROR ), CKM_NONE );

	return( mechanismInfoConv[ i ].mechanism );
	}

/****************************************************************************
*																			*
*						 	Device Access Routines							*
*																			*
****************************************************************************/

/* Mechanisms supported by PKCS #11 devices.  These are actually cryptlib 
   native mechanisms (support of the various mechanisms in devices is too 
   patchy to rely on, see for example the comments about PKCS vs.raw RSA
   mechanisms elsewhere), but not the full set supported by the system 
   device since functions like private key export aren't available.  The 
   list is sorted in order of frequency of use in order to make lookups a 
   bit faster */

static const FAR_BSS MECHANISM_FUNCTION_INFO mechanismFunctions[] = {
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1, ( MECHANISM_FUNCTION ) importPKCS1 },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) signPKCS1 },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_PKCS1, ( MECHANISM_FUNCTION ) sigcheckPKCS1 },
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) exportPKCS1 },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_RAW, ( MECHANISM_FUNCTION ) importPKCS1 },
#ifdef USE_PGP
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) exportPKCS1PGP },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_PKCS1_PGP, ( MECHANISM_FUNCTION ) importPKCS1PGP },
#endif /* USE_PGP */
	{ MESSAGE_DEV_EXPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) exportCMS },
	{ MESSAGE_DEV_IMPORT, MECHANISM_ENC_CMS, ( MECHANISM_FUNCTION ) importCMS },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS5, ( MECHANISM_FUNCTION ) derivePKCS5 },
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PGP, ( MECHANISM_FUNCTION ) derivePGP },
#endif /* USE_PGP || USE_PGPKEYS */
#ifdef USE_SSL
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_TLS, ( MECHANISM_FUNCTION ) deriveSSL },
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_SSL, ( MECHANISM_FUNCTION ) deriveTLS },
	{ MESSAGE_DEV_SIGN, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) signSSL },
	{ MESSAGE_DEV_SIGCHECK, MECHANISM_SIG_SSL, ( MECHANISM_FUNCTION ) sigcheckSSL },
#endif /* USE_SSL */
#ifdef USE_CMP
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_CMP, ( MECHANISM_FUNCTION ) deriveCMP },
#endif /* USE_CMP */
#ifdef USE_PKCS12
	{ MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( MECHANISM_FUNCTION ) derivePKCS12 },
#endif /* USE_PKCS12 */
	{ MESSAGE_NONE, MECHANISM_NONE, NULL }, { MESSAGE_NONE, MECHANISM_NONE, NULL }
	};

/* Set up the function pointers to the device methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setDevicePKCS11( INOUT DEVICE_INFO *deviceInfo, 
					 IN_BUFFER( nameLength ) const char *name, 
					 IN_LENGTH_ATTRIBUTE const int nameLength )
	{
	int status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( name, nameLength ) );

	REQUIRES( nameLength > 0 && nameLength < MAX_ATTRIBUTE_SIZE );

	status = initPKCS11Init( deviceInfo, name, nameLength );
	if( cryptStatusError( status ) )
		return( status );
	deviceInfo->controlFunction = controlFunction;
	initPKCS11Read( deviceInfo );
	initPKCS11Write( deviceInfo );
	deviceInfo->mechanismFunctions = mechanismFunctions;
	deviceInfo->mechanismFunctionCount = \
		FAILSAFE_ARRAYSIZE( mechanismFunctions, MECHANISM_FUNCTION_INFO );

	return( CRYPT_OK );
	}
#endif /* USE_PKCS11 */
