/****************************************************************************
*																			*
*							cryptlib User Routines							*
*						Copyright Peter Gutmann 1999-2007					*
*																			*
****************************************************************************/

/* cryptlib's role-based access control mechanisms are present only for
   forwards-compatibility with future cryptlib versions that will include
   role-based access control if there's user demand for it.  The following
   code implements basic user management routines, but the full role-based
   access control functionality isn't present.  Some of the code related to
   this is therefore present only in template form */

#include <stdio.h>		/* For sprintf_s() */
#include "crypt.h"
#ifdef INC_ALL
  #include "trustmgr.h"
  #include "user.h"
#else
  #include "cert/trustmgr.h"
  #include "misc/user.h"
#endif /* Compiler-specific includes */

/* Default user info.  The default user is a special type that has both 
   normal user and SO privileges.  This is because in its usual usage mode 
   where cryptlib is functioning as a single-user system the user doesn't 
   know about the existence of user objects and just wants everything to 
   work the way that they expect.  Because of this the default user has to 
   be able to perform the full range of available operations, requiring that 
   they appear as both a normal user and an SO.

   For now the default user is marked as an SO user because the kernel checks
   don't allow dual-type objects and some operations require that the user be
   at least an SO user, once a distinction is made between SOs and users this
   will need to be fixed */

static const USER_FILE_INFO FAR_BSS defaultUserInfo = {
#if 0	/* 18/05/02 Disabled since ACL checks are messed up by the existence
		   of dual-user roles */
	CRYPT_USER_NONE,				/* Special-case SO+normal user */
#else
	CRYPT_USER_SO,					/* Special-case SO user */
#endif /* 0 */
	USER_STATE_USERINITED,			/* Initialised, ready for use */
	"Default cryptlib user", 21,	/* Pre-set user name */
	"<<<<DEFAULT_USER>>>>", "<<<<DEFAULT_USER>>>>",
	CRYPT_UNUSED					/* No corresponding user file */
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Process user-object-specific messages */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processUserManagement( INOUT USER_INFO *userInfoPtr, 
								  STDC_UNUSED void *userMgtInfo, 
								  IN_ENUM( MESSAGE_USERMGMT ) \
									const MESSAGE_USERMGMT_TYPE userMgtType )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	
	REQUIRES( userMgtType > MESSAGE_USERMGMT_NONE && \
			  userMgtType < MESSAGE_USERMGMT_LAST );

	switch( userMgtType )
		{
		case MESSAGE_USERMGMT_ZEROISE:
			userInfoPtr->flags |= USER_FLAG_ZEROISE;
			return( CRYPT_OK );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processTrustManagement( INOUT USER_INFO *userInfoPtr, 
								   INOUT_HANDLE CRYPT_HANDLE *iCertificate, 
								   IN_ENUM( MESSAGE_TRUSTMGMT ) \
									const MESSAGE_TRUSTMGMT_TYPE trustMgtType )
	{
	int status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_HANDLE ) ) );

	REQUIRES( trustMgtType > MESSAGE_TRUSTMGMT_NONE && \
			  trustMgtType < MESSAGE_TRUSTMGMT_LAST );

	switch( trustMgtType )
		{
		case MESSAGE_TRUSTMGMT_ADD:
			/* Add the cert to the trust info */
			status = addTrustEntry( userInfoPtr->trustInfoPtr, 
									*iCertificate, NULL, 0, TRUE );
			if( cryptStatusError( status ) )
				return( status );
			userInfoPtr->trustInfoChanged = TRUE;
			return( setOption( userInfoPtr->configOptions, 
							   userInfoPtr->configOptionsCount,
							   CRYPT_OPTION_CONFIGCHANGED, TRUE ) );

		case MESSAGE_TRUSTMGMT_DELETE:
			{
			void *entryToDelete;

			/* Find the entry to delete and remove it (fides, ut anima, unde 
			   abiit eo nunquam redit - Publius Syrus) */
			if( ( entryToDelete = findTrustEntry( userInfoPtr->trustInfoPtr,
												  *iCertificate, FALSE ) ) == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			deleteTrustEntry( userInfoPtr->trustInfoPtr, entryToDelete );
			userInfoPtr->trustInfoChanged = TRUE;
			return( setOption( userInfoPtr->configOptions,
							   userInfoPtr->configOptionsCount,
							   CRYPT_OPTION_CONFIGCHANGED, TRUE ) );
			}

		case MESSAGE_TRUSTMGMT_CHECK:
			/* Check whether the cert is present in the trusted certs
			   collection */
			return( ( findTrustEntry( userInfoPtr->trustInfoPtr, 
									  *iCertificate, FALSE ) != NULL ) ? \
					CRYPT_OK : CRYPT_ERROR_INVALID );

		case MESSAGE_TRUSTMGMT_GETISSUER:
			{
			void *trustedIssuerInfo;
			int trustedCert;

			/* Get the trusted issuer of this certificate */
			trustedIssuerInfo = findTrustEntry( userInfoPtr->trustInfoPtr,
												*iCertificate, TRUE );
			if( trustedIssuerInfo == NULL )
				return( CRYPT_ERROR_NOTFOUND );

			/* Get the issuer cert and return it to the caller */
			trustedCert = getTrustedCert( trustedIssuerInfo );
			if( cryptStatusError( trustedCert ) )
				return( trustedCert );
			ENSURES( trustedCert != *iCertificate );
			*iCertificate = trustedCert;

			return( CRYPT_OK );
			}
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							General User Object Functions					*
*																			*
****************************************************************************/

/* Handle a message sent to a user object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int userMessageFunction( INOUT TYPECAST( USER_INFO * ) \
									void *objectInfoPtr,
								IN_MESSAGE const MESSAGE_TYPE message,
								void *messageDataPtr, 
								IN_INT_Z const int messageValue )
	{
	USER_INFO *userInfoPtr = ( USER_INFO * ) objectInfoPtr;

	assert( isWritePtr( objectInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( message > MESSAGE_NONE && message < MESSAGE_LAST );
	REQUIRES( messageValue >= 0 && messageValue < MAX_INTLENGTH );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		/* Clean up any user-related crypto objects if necessary */
		if( userInfoPtr->iCryptContext != CRYPT_ERROR )
			krnlSendNotifier( userInfoPtr->iCryptContext,
							  IMESSAGE_DECREFCOUNT );
		if( userInfoPtr->iKeyset != CRYPT_ERROR )
			krnlSendNotifier( userInfoPtr->iKeyset, IMESSAGE_DECREFCOUNT );

		/* If we're doing a zeroise, clear any persistent user data.  It's a
		   bit unclear what to do in case of an error at this point since 
		   we're in the middle of a shutdown anyway.  We can't really cancel
		   the shutdown because the zeroise fails (what do you do if there's
		   an exception in the exception handler?) so we just have to 
		   continue and ignore the failure */
		if( userInfoPtr->flags & USER_FLAG_ZEROISE )
			( void ) zeroiseUsers( userInfoPtr );

		/* Clean up the trust info and config options */
		if( userInfoPtr->trustInfoPtr != NULL )
			endTrustInfo( userInfoPtr->trustInfoPtr );
		if( userInfoPtr->configOptions != NULL )
			endOptions( userInfoPtr->configOptions, 
						userInfoPtr->configOptionsCount );
		if( userInfoPtr->userIndexPtr != NULL )
			endUserIndex( userInfoPtr->userIndexPtr );

		return( CRYPT_OK );
		}

	/* If we're doing a zeroise, don't process any further messages except a 
	   destroy */
	if( userInfoPtr->flags & USER_FLAG_ZEROISE )
		return( CRYPT_ERROR_PERMISSION );

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		REQUIRES( message == MESSAGE_GETATTRIBUTE || \
				  message == MESSAGE_GETATTRIBUTE_S || \
				  message == MESSAGE_SETATTRIBUTE || \
				  message == MESSAGE_SETATTRIBUTE_S || \
				  message == MESSAGE_DELETEATTRIBUTE );
		REQUIRES( isAttribute( messageValue ) || \
				  isInternalAttribute( messageValue ) );

		if( message == MESSAGE_GETATTRIBUTE )
			return( getUserAttribute( userInfoPtr, 
									  ( int * ) messageDataPtr,
									  messageValue ) );
		if( message == MESSAGE_GETATTRIBUTE_S )
			return( getUserAttributeS( userInfoPtr, 
									   ( MESSAGE_DATA * ) messageDataPtr,
									   messageValue ) );
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				{
				REQUIRES( userInfoPtr->objectHandle == \
								DEFAULTUSER_OBJECT_HANDLE );
				return( CRYPT_OK );
				}

			return( setUserAttribute( userInfoPtr, 
									  *( ( int * ) messageDataPtr ),
									  messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setUserAttributeS( userInfoPtr, msgData->data, 
									   msgData->length, messageValue ) );
			}
		if( message == MESSAGE_DELETEATTRIBUTE )
			return( deleteUserAttribute( userInfoPtr, messageValue ) );

		retIntError();
		}

	/* Process object-specific messages */
	if( message == MESSAGE_USER_USERMGMT )
		return( processUserManagement( userInfoPtr, messageDataPtr,
									   messageValue ) );
	if( message == MESSAGE_USER_TRUSTMGMT )
		return( processTrustManagement( userInfoPtr, messageDataPtr,
									    messageValue ) );

	retIntError();
	}

/* Open a user object.  This is a low-level function encapsulated by
   createUser() and used to manage error exits */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int openUser( OUT_HANDLE_OPT CRYPT_USER *iCryptUser, 
					 IN_HANDLE const CRYPT_USER iCryptOwner,
					 const USER_FILE_INFO *userInfoTemplate,
					 OUT_OPT_PTR USER_INFO **userInfoPtrPtr )
	{
	USER_INFO *userInfoPtr;
	USER_FILE_INFO *userFileInfo;
	static const MAP_TABLE subtypeMapTbl[] = {
		{ CRYPT_USER_SO, SUBTYPE_USER_SO },
		{ CRYPT_USER_CA, SUBTYPE_USER_CA },
		{ CRYPT_USER_NORMAL, SUBTYPE_USER_NORMAL },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	OBJECT_SUBTYPE subType;
	int value, status;

	assert( isWritePtr( iCryptUser, sizeof( CRYPT_USER * ) ) );
	assert( isReadPtr( userInfoTemplate, sizeof( USER_FILE_INFO ) ) );
	assert( isWritePtr( userInfoPtrPtr, sizeof( USER_INFO * ) ) );

	REQUIRES( ( iCryptOwner == SYSTEM_OBJECT_HANDLE ) || \
			  ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );

	/* The default user is a special type that has both normal user and SO
	   privileges.  This is because in its usual usage mode where cryptlib 
	   is functioning as a single-user system the user doesn't know about 
	   the existence of user objects and just wants everything to work the 
	   way that they expect.  Because of this the default user has to be 
	   able to perform the full range of available operations, requiring 
	   that they appear as both a normal user and an SO */
#if 0	/* 18/05/02 Disabled since ACL checks are messed up by the existence
		   of dual-user roles */
	assert( userInfoTemplate->type == CRYPT_USER_NORMAL || \
			userInfoTemplate->type == CRYPT_USER_SO || \
			userInfoTemplate->type == CRYPT_USER_CA || \
			( userInfoTemplate->type == CRYPT_USER_NONE && \
			  userInfoTemplate->userNameLength == \
								defaultUserInfo.userNameLength && \
			  !memcmp( userInfoTemplate->userName, defaultUserInfo.userName,
					   defaultUserInfo.userNameLength ) ) );
#endif /* 0 */

	/* Clear return values */
	*iCryptUser = CRYPT_ERROR;
	*userInfoPtrPtr = NULL;

	/* Create the user object */
	status = mapValue( userInfoTemplate->type, &value, subtypeMapTbl, 
					   FAILSAFE_ARRAYSIZE( subtypeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	subType = value;
	status = krnlCreateObject( iCryptUser, ( void ** ) &userInfoPtr, 
							   sizeof( USER_INFO ), OBJECT_TYPE_USER, 
							   subType, CREATEOBJECT_FLAG_NONE, iCryptOwner,
							   ACTION_PERM_NONE_ALL, userMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( userInfoPtr != NULL );
	*userInfoPtrPtr = userInfoPtr;
	userInfoPtr->objectHandle = *iCryptUser;
	userFileInfo = &userInfoPtr->userFileInfo;
	userFileInfo->type = userInfoTemplate->type;
	userFileInfo->state = userInfoTemplate->state;
	userFileInfo->fileRef = userInfoTemplate->fileRef;
	memcpy( userFileInfo->userName, userInfoTemplate->userName,
			userInfoTemplate->userNameLength );
	userFileInfo->userNameLength = userInfoTemplate->userNameLength;
	memcpy( userFileInfo->userID, userInfoTemplate->userID, KEYID_SIZE );
	memcpy( userFileInfo->creatorID, userInfoTemplate->creatorID, KEYID_SIZE );

	/* Set up any internal objects to contain invalid handles */
	userInfoPtr->iKeyset = userInfoPtr->iCryptContext = CRYPT_ERROR;

	/* Initialise the config options and trust info */
	status = initTrustInfo( &userInfoPtr->trustInfoPtr );
	if( cryptStatusOK( status ) )
		status = initOptions( &userInfoPtr->configOptions,
							  &userInfoPtr->configOptionsCount );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createUser( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				STDC_UNUSED const void *auxDataPtr, 
				STDC_UNUSED const int auxValue )
	{
	CRYPT_USER iCryptUser;
	USER_INFO *userInfoPtr;
	char userFileName[ 16 + 8 ];
	int fileRef, initStatus, status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( createInfo->strArgLen1 >= MIN_NAME_LENGTH && \
			  createInfo->strArgLen1 <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( createInfo->strArgLen2 >= MIN_NAME_LENGTH && \
			  createInfo->strArgLen2 <= CRYPT_MAX_TEXTSIZE );

	/* We can't create another user object with the same name as the
	   cryptlib default user (actually we could and nothing bad would happen,
	   but we reserve the use of this name just in case) */
	if( createInfo->strArgLen1 == defaultUserInfo.userNameLength && \
		!strCompare( createInfo->strArg1, defaultUserInfo.userName,
					 defaultUserInfo.userNameLength ) )
		return( CRYPT_ERROR_INITED );

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/* Problem: Access to any user info is via the root user object, however */
/* we don't have access to it at this point.  Pass it in as auxDataPtr? */
/* Need to differentiate cryptCreateUser() vs. cryptLogin(), login uses */
/* the default user object as its target?  This is complex, we really */
/* need to target the message at the default user to get access to the user */
/* info index, but then it won't go through cryptdev's create-object- */
/* handling any more */
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
#if 0
	/* Find the user information for the given user */
	status = fileRef = findUserIndexEntry( USERID_NAME, createInfo->strArg1,
										   createInfo->strArgLen1 );
	if( cryptStatusError( status ) )
		{
		/* If we get a special-case OK status, we're in the zeroised state
		   with no user info present, make sure that the user is logging in
		   with the default SO password */
		if( status == OK_SPECIAL )
			status = ( isZeroisePassword( createInfo->strArg2, \
										  createInfo->strArgLen2 ) ) ? \
					 CRYPT_OK : CRYPT_ERROR_WRONGKEY;
		if( cryptStatusError( status ) )
			return( status );
		fileRef = -1;	/* No user file present yet for primary SO */

		/* We're logging in as the primary SO with the SO default password,
		   create the primary SO user object */
		assert( isZeroisePassword( createInfo->strArg2, \
								   createInfo->strArgLen2 ) );
		initStatus = openUser( &iCryptUser, createInfo->cryptOwner,
							   getPrimarySoUserInfo(), &userInfoPtr );
		}
	else
		{
		USER_FILE_INFO userFileInfo;

		/* We're in the non-zeroised state, no user can use the default SO
		   password */
		if( isZeroisePassword( createInfo->strArg2, createInfo->strArgLen2 ) )
			return( CRYPT_ERROR_WRONGKEY );

		/* Read the user info from the user file and perform access
		   verification */
		status = getCheckUserInfo( &userFileInfo, fileRef );
		if( cryptStatusError( status ) )
			return( status );

		/* Pass the call on to the lower-level open function */
		assert( createInfo->strArgLen1 == userFileInfo.userNameLength && \
				!memcmp( createInfo->strArg1, userFileInfo.userName,
						 userFileInfo.userNameLength ) );
		initStatus = openUser( &iCryptUser, createInfo->cryptOwner,
							   &userFileInfo, &userInfoPtr );
		zeroise( &userFileInfo, sizeof( USER_FILE_INFO ) );
		}
#endif
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
{	/* Get rid of compiler warnings */
userInfoPtr = NULL;
initStatus = CRYPT_ERROR_FAILED;
iCryptUser = CRYPT_UNUSED;
fileRef = 0;
}
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( userInfoPtr == NULL )
			return( initStatus );

		/* The init failed, make sure that the object gets destroyed when we
		   notify the kernel that the setup process is complete */
		krnlSendNotifier( iCryptUser, IMESSAGE_DESTROY );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCryptUser, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );

	/* If the user object has a corresponding user info file, read any
	   stored config options into the object.  We have to do this after
	   it's initialised because the config data, coming from an external
	   (and therefore untrusted) source has to go through the kernel's
	   ACL checking */
	if( fileRef >= 0 )
		{
		sprintf_s( userFileName, 16, "u%06x", fileRef );
		status = readConfig( iCryptUser, userFileName, 
							 userInfoPtr->trustInfoPtr );
		if( cryptStatusError( status ) )
			{
			/* The config data read failed, we can't create the user 
			   object that uses it */
			krnlSendNotifier( iCryptUser, IMESSAGE_DESTROY );
			return( status );
			}
		}
	createInfo->cryptHandle = iCryptUser;
	return( CRYPT_OK );
	}

/* Create the default user object */

CHECK_RETVAL \
static int createDefaultUserObject( void )
	{
	CRYPT_USER iUserObject;
	USER_INFO *userInfoPtr;
	int initStatus, status;

	/* Pass the call on to the lower-level open function.  This user is
	   unique and has no owner or type.

	   Normally if an object init fails we tell the kernel to destroy it by 
	   sending it a destroy message, which is processed after the object's 
	   status has been set to normal.  However we don't have the privileges 
	   to do this for the default user object (or the system object) so we 
	   just pass the error code back to the caller, which causes the 
	   cryptlib init to fail.

	   In addition the init can fail in one of two ways, either the object 
	   isn't even created (deviceInfoPtr == NULL, nothing to clean up), in 
	   which case we bail out immediately, or the object is created but wasn't 
	   set up properly (deviceInfoPtr is allocated, but the object can't be 
	   used) in which case we bail out after we update its status */
	initStatus = openUser( &iUserObject, SYSTEM_OBJECT_HANDLE, &defaultUserInfo,
						   &userInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( userInfoPtr == NULL )
			return( initStatus );
		}
	ENSURES( iUserObject == DEFAULTUSER_OBJECT_HANDLE );
	if( cryptStatusOK( initStatus ) )
		{
		/* Read the user index.  We make this part of the object init because
		   it's used for access control, unlike the config option read where
		   we can fall back to defaults if there's a problem this one is
		   critical enough that we abort the cryptlib init if it fails */
		initStatus = initUserIndex( &userInfoPtr->userIndexPtr );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iUserObject, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );

	/* Read any stored config options into the object.  We have to do this 
	   after it's initialised because the config data, coming from an 
	   external (and therefore untrusted) source has to go through the 
	   kernel's ACL checking.  
	   
	   What to do in case of an error reading the config file is a bit 
	   problematic, we don't want to cause whatever application is using
	   cryptlib to abort mysteriously just because a bit in some config file 
	   that most people don't even know exists got flipped, so we treat the 
	   read as an opportunistic read and fall back to built-in safe defaults 
	   if there's a problem, warning the user in debug mode */
	status = readConfig( iUserObject, "cryptlib", userInfoPtr->trustInfoPtr );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Couldn't read config data, using default config" ));
		assert( DEBUG_WARN );
		DEBUG_PRINT(( "Configuration file read failed with status %d.\n", 
					  status ));
		}

	/* The object has been initialised, move it into the initialised state */
	return( krnlSendMessage( iUserObject, IMESSAGE_SETATTRIBUTE, 
							 MESSAGE_VALUE_UNUSED, 
							 CRYPT_IATTRIBUTE_INITIALISED ) );
	}

/* Generic management function for this class of object */

CHECK_RETVAL \
int userManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action )
	{
	int status;

	REQUIRES( action == MANAGEMENT_ACTION_INIT );

	switch( action )
		{
		case MANAGEMENT_ACTION_INIT:
			status = createDefaultUserObject();
			if( cryptStatusError( status ) )
				{
				DEBUG_DIAG(( "User object creation failed" ));
				}
			return( status );
		}

	retIntError();
	}
