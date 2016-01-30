/****************************************************************************
*																			*
*							cryptlib User Routines							*
*						Copyright Peter Gutmann 1999-2007					*
*																			*
****************************************************************************/

#include <stdio.h>		/* For snprintf_s() */
#include "crypt.h"
#ifdef INC_ALL
  #include "asn1.h"
  #include "user.h"
#else
  #include "enc_dec/asn1.h"
  #include "misc/user.h"
#endif /* Compiler-specific includes */

/* The different types of userID that we can use to look up users in the 
   index */

typedef enum {
	USERID_NONE,		/* No userID type */
	USERID_USERID,		/* User's userID */
	USERID_CREATORID,	/* Creating SO's userID */
	USERID_NAME,		/* User's name */
	USERID_LAST			/* Last possible userID type */
	} USERID_TYPE;

/* cryptlib can work with multiple users (although it's extremely unlikely 
   that there'll ever be more than one or two), we allow a maximum of 
   MAX_USER_OBJECTS in order to discourage them from being used as a
   substitute for OS user management.  A setting of 32 objects consumes 
   ~4K of memory (32 x ~128), so we choose that as the limit */

#ifdef CONFIG_CONSERVE_MEMORY
  #define MAX_USER_OBJECTS		4
#else
  #define MAX_USER_OBJECTS		32
#endif /* CONFIG_CONSERVE_MEMORY */

/* The size of the default buffer used to read data from a keyset.  If
   the data is larger than this, the buffer is allocated dynamically */

#define USERDATA_BUFFERSIZE		1024

/* The maximum size of the encoded index data */

#define MAX_USERINDEX_SIZE	( ( 16 + ( KEYID_SIZE * 2 ) + \
							  CRYPT_MAX_TEXTSIZE + 8 ) * MAX_USER_OBJECTS )

/* Primary SO user info */

static const USER_FILE_INFO FAR_BSS primarySOInfo = {
	CRYPT_USER_SO,					/* SO user */
	USER_STATE_SOINITED,			/* SO initialised, not ready for use */
	"Security officer", 16,			/* Pre-set user name */
	"<<<PRIMARYSO_USER>>>", "<<<TETRAGRAMMATON>>>",
	-1			/* No user file when starting from zeroised state */
	};

/* The primary SO password after zeroisation */

#define PRIMARYSO_PASSWORD		"zeroised"
#define PRIMARYSO_ALTPASSWORD	"zeroized"
#define PRIMARYSO_PASSWORD_LENGTH 8

/* The structure that stores the user index in the default user object */

typedef struct {
	USER_FILE_INFO userIndex[ MAX_USER_OBJECTS ];	/* User index */
	int lastEntry;					/* Last entry in user index */
	} USER_INDEX_INFO;

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Open a user or index keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int openKeyset( OUT_HANDLE_OPT CRYPT_KEYSET *iKeyset, 
					   IN_BUFFER( fileNameLen ) const char *fileName, 
					   IN_LENGTH_SHORT const int fileNameLen, 
					   IN_ENUM_OPT( CRYPT_KEYOPT ) const int options )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	char userFilePath[ MAX_PATH_LENGTH + 8 ];
	int userFilePathLen, status;

	assert( isWritePtr( iKeyset, sizeof( CRYPT_KEYSET ) ) );
	assert( isReadPtr( fileName, fileNameLen ) );

	REQUIRES( fileNameLen > 0 && fileNameLen < MAX_INTLENGTH_SHORT );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Clear return value */
	*iKeyset = CRYPT_ERROR;

	/* Open the given keyset */
	status = fileBuildCryptlibPath( userFilePath, MAX_PATH_LENGTH, 
									&userFilePathLen, fileName, fileNameLen, 
									( options == CRYPT_KEYOPT_CREATE ) ? \
									BUILDPATH_CREATEPATH : BUILDPATH_GETPATH );
	if( cryptStatusError( status ) )
		{
		/* Map the lower-level filesystem-specific error into a more 
		   meaningful generic error */
		return( CRYPT_ERROR_OPEN );
		}
	setMessageCreateObjectInfo( &createInfo, CRYPT_KEYSET_FILE );
	createInfo.arg2 = options;
	createInfo.strArg1 = userFilePath;
	createInfo.strArgLen1 = userFilePathLen;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_KEYSET );
	if( cryptStatusOK( status ) )
		*iKeyset = createInfo.cryptHandle;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openUserKeyset( OUT_HANDLE_OPT CRYPT_KEYSET *iUserKeyset, 
						   IN_INT_SHORT_Z const int fileRef, 
						   IN_ENUM_OPT( CRYPT_KEYOPT ) const int options )
	{
	char userFileName[ 16 + 8 ];
	int userFileNameLen;

	assert( isWritePtr( iUserKeyset, sizeof( CRYPT_KEYSET ) ) );

	REQUIRES( fileRef >= 0 && fileRef < MAX_INTLENGTH_SHORT );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	userFileNameLen = sprintf_s( userFileName, 16, "u%06x", fileRef );
	return( openKeyset( iUserKeyset, userFileName, userFileNameLen, 
						options ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int openIndexKeyset( OUT CRYPT_KEYSET *iIndexKeyset, 
							IN_ENUM_OPT( CRYPT_KEYOPT ) const int options )
	{
	assert( isWritePtr( iIndexKeyset, sizeof( CRYPT_KEYSET ) ) );

	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	return( openKeyset( iIndexKeyset, "index", 5, options ) );
	}

/* Add a user key to the keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
static int addKey( IN_HANDLE const CRYPT_KEYSET iUserKeyset, 
				   IN_HANDLE const CRYPT_CONTEXT iCryptContext,
				   IN_BUFFER( userIdLength ) const void *userID, 
				   IN_LENGTH_SHORT const int userIdLength,
				   IN_BUFFER( passwordLength ) const char *password, 
				   IN_LENGTH_SHORT const int passwordLength,
				   const BOOLEAN isPrivateKey )
	{
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( userID, userIdLength ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( isHandleRangeValid( iUserKeyset ) );
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( userIdLength > 0 && userIdLength < MAX_INTLENGTH_SHORT );
	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH_SHORT );

	setMessageData( &msgData, ( MESSAGE_CAST ) userID, userIdLength );
	status = krnlSendMessage( iUserKeyset, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_USERID );
	if( cryptStatusError( status ) )
		return( status );

	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
						   ( MESSAGE_CAST ) password, passwordLength,
						   KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = iCryptContext;
	status = krnlSendMessage( iUserKeyset, IMESSAGE_KEY_SETKEY,
							  &setkeyInfo, isPrivateKey ? \
								KEYMGMT_ITEM_PRIVATEKEY : \
								KEYMGMT_ITEM_SECRETKEY );
	return( status );
	}

/****************************************************************************
*																			*
*								Manage User Index							*
*																			*
****************************************************************************/

/* Find a user in the user index.  Note that this search implements a flat
   namespace rather than allowing duplicate names created by different SOs
   because when we're looking up a user we don't know which SO they belong
   to until after we've looked them up */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static const USER_FILE_INFO *findUser( IN_ARRAY( noUserIndexEntries ) \
										const USER_FILE_INFO *userIndex,
									   IN_RANGE( 1, MAX_USER_OBJECTS ) \
										const int noUserIndexEntries, 
									   IN_ENUM( USERID ) const USERID_TYPE idType, 
									   IN_BUFFER( idLength ) const BYTE *id, 
									   IN_LENGTH_SHORT const int idLength )
	{
	int i, iterationCount;

	assert( isReadPtr( userIndex, \
					   sizeof( USER_FILE_INFO ) * noUserIndexEntries ) );
	assert( isReadPtr( id, idLength ) );

	REQUIRES_N( noUserIndexEntries > 0 && \
				noUserIndexEntries <= MAX_USER_OBJECTS );
	REQUIRES_N( idType > USERID_NONE && idType < USERID_LAST );
	REQUIRES_N( idLength > 0 && idLength < MAX_INTLENGTH_SHORT );

	for( i = 0, iterationCount = 0; 
		 i < noUserIndexEntries && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 i++, iterationCount++ )
		{
		const USER_FILE_INFO *userIndexPtr = &userIndex[ i ];

		switch( idType )
			{
			case USERID_USERID:
				if( idLength == KEYID_SIZE && \
					!memcmp( userIndexPtr->userID, id, idLength ) )
					return( userIndexPtr );
				break;

			case USERID_CREATORID:
				if( idLength == KEYID_SIZE && \
					!memcmp( userIndexPtr->creatorID, id, idLength ) )
					return( userIndexPtr );
				break;

			case USERID_NAME:
				if( idLength == userIndexPtr->userNameLength && \
					!memcmp( userIndexPtr->userName, id, idLength ) )
					return( userIndexPtr );
				break;

			default:
				retIntError_Null();
			}
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( NULL );
	}

/* Find a free user entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static USER_FILE_INFO *findFreeEntry( IN_ARRAY( noUserIndexEntries ) \
										USER_FILE_INFO *userIndex,
									  IN_RANGE( 1, MAX_USER_OBJECTS ) \
										const int noUserIndexEntries,
									  OUT_INT_SHORT_Z int *fileRef )
	{
	USER_FILE_INFO *userIndexPtr;
	int newFileRef, i, iterationCount;

	assert( isWritePtr( userIndex, \
						sizeof( USER_FILE_INFO ) * noUserIndexEntries ) );
	assert( isWritePtr( fileRef, sizeof( int ) ) );

	REQUIRES_N( noUserIndexEntries > 0 && \
				noUserIndexEntries <= MAX_USER_OBJECTS );

	/* Look for an available free entry */
	for( i = 0, iterationCount  = 0; 
		 i < noUserIndexEntries && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 i++, iterationCount++ )
		{
		if( userIndex[ i ].state == USER_STATE_NONE )
			break;
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( i >= noUserIndexEntries )
		{
		/* No more available entries */
		*fileRef = CRYPT_ERROR;
		return( NULL );
		}

	/* Remember where we found our match */
	userIndexPtr = &userIndex[ i ];

	/* We've found a free entry, now look for an unused fileRef.  There are 
	   two possible strategies for this, the first is to make it generational
	   and always allocate a new fileRef, the second is to use the smallest
	   available value, i.e. to re-use values.  The former has problems with
	   overflow (although it'd have to be a pretty funny situation to cause 
	   this), the latter has potential problems with user confusion when one
	   ref #3 user file is replaced by another ref #3 file that belongs to
	   a completely different user.  However, even the generational approach
	   has problems (unless we can make the last-used fileRef persistent)
	   because deleting the highest-numbered ref. and then creating a new one
	   will result in the fileRef being re-allocated to the newly-created
	   file.

	   Since this is all highly speculative (it's not certain under what 
	   conditions we could run into these problems because users aren't 
	   expected to be bypassing cryptlib to directly access the user files),
	   we take the simplest approach and use the lowest-value free fileRef.
	   This is somewhat ugly because it's potentially an O( n^2 ) operation,
	   but the actualy impact is insignificant because the number of users
	   is tiny and new user creation is extremely rare, so it's not worth
	   switching to the complexity of a more sophisticated algorithm */
	for( newFileRef = 0; newFileRef < MAX_USER_OBJECTS; newFileRef++ )
		{
		/* Check whether this fileRef is already in use.  If not, we're
		   done */
		for( i = 0; i < noUserIndexEntries; i++ )
			{
			if( userIndex[ i ].fileRef == newFileRef )
				break;
			}
		if( i >= MAX_USER_OBJECTS )
			break;
		}
	ENSURES_N( newFileRef < MAX_USER_OBJECTS );
	*fileRef = newFileRef;

	return( userIndexPtr );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int createUserEntry( OUT_OPT_PTR USER_FILE_INFO **userIndexPtrPtr,
							IN_ARRAY( noUserIndexEntries ) \
								USER_FILE_INFO *userIndex, 
							IN_RANGE( 1, MAX_USER_OBJECTS ) \
								const int noUserIndexEntries,
							INOUT USER_FILE_INFO *userFileInfo )
	{
	USER_FILE_INFO *userIndexPtr;
	int fileRef, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( userIndexPtrPtr, sizeof( USER_FILE_INFO * ) ) );
	assert( isWritePtr( userIndex, \
						sizeof( USER_FILE_INFO ) * noUserIndexEntries ) );
	assert( isWritePtr( userFileInfo, sizeof( USER_FILE_INFO ) ) );

	REQUIRES( noUserIndexEntries > 0 && \
			  noUserIndexEntries <= MAX_USER_OBJECTS );

	/* Clear return value */
	*userIndexPtrPtr = NULL;

	/* Check whether this user is already present in the index */
	if( findUser( userIndex, noUserIndexEntries, USERID_NAME, 
				  userFileInfo->userName, userFileInfo->userNameLength ) != NULL )
		return( CRYPT_ERROR_DUPLICATE );

	/* Make sure that the userID that we're using is unique.  This is a 
	   pretty straightforward operation, we just keep generating new random 
	   IDs until we get one that's not already present */
	for( iterationCount = 0; 
		 !cryptStatusError( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		if( findUser( userIndex, noUserIndexEntries, USERID_USERID, 
					  userFileInfo->userID, KEYID_SIZE ) != NULL )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, ( MESSAGE_CAST ) userFileInfo->userID, 
							KEYID_SIZE );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_GETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_RANDOM_NONCE );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Locate a new unused entry that we can use */
	userIndexPtr = findFreeEntry( userIndex, MAX_USER_OBJECTS, &fileRef );
	if( userIndexPtr == NULL )
		return( CRYPT_ERROR_OVERFLOW );
	userFileInfo->fileRef = fileRef;

	return( CRYPT_OK );
	}

/* Read the user index file:

	UserIndexEntry ::= SEQUENCE {
		iD					OCTET STRING SIZE(16),	-- User ID
		creatorID			OCTET STRING SIZE(16),	-- Creating SO's ID
		name				UTF8String,				-- User name
		fileReference		INTEGER					-- Reference to user file
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readIndexEntry( INOUT STREAM *stream, 
						   INOUT USER_FILE_INFO *userIndexPtr )
	{
	USER_FILE_INFO userIndexEntry;
	long value;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( userIndexPtr, sizeof( USER_FILE_INFO ) ) );

	/* Clear return value */
	memset( userIndexPtr, 0, sizeof( USER_FILE_INFO ) );

	/* Read the user index data */
	memset( &userIndexEntry, 0, sizeof( USER_FILE_INFO ) );
	readSequence( stream, NULL );
	readOctetString( stream, userIndexEntry.userID, &length, KEYID_SIZE, 
					 KEYID_SIZE );
	readOctetString( stream, userIndexEntry.creatorID, &length, KEYID_SIZE, 
					 KEYID_SIZE );
	readCharacterString( stream, userIndexEntry.userName, 
						 CRYPT_MAX_TEXTSIZE, &userIndexEntry.userNameLength, 
						 BER_STRING_UTF8 );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	userIndexEntry.fileRef = value;

	/* Return the result to the caller */
	memcpy( userIndexPtr, &userIndexEntry, sizeof( USER_FILE_INFO ) );
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int readIndex( IN_HANDLE const CRYPT_KEYSET iIndexKeyset, 
					  IN_ARRAY( maxUserObjects ) USER_FILE_INFO *userIndex, 
					  IN_RANGE( 1, MAX_USER_OBJECTS ) const int maxUserObjects )
	{
	STREAM stream;
	DYNBUF userIndexDB;
	int i, iterationCount, status;

	assert( isWritePtr( userIndex, \
						maxUserObjects * sizeof( USER_FILE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iIndexKeyset ) );
	REQUIRES( maxUserObjects > 0 && maxUserObjects <= MAX_USER_OBJECTS );

	/* Read the user index file into memory */
	status = dynCreate( &userIndexDB, iIndexKeyset, 
						CRYPT_IATTRIBUTE_USERINDEX );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, dynData( userIndexDB ), dynLength( userIndexDB ) );
	for( i = 0, iterationCount = 0; 
		 cryptStatusOK( status ) && \
			stell( &stream ) < dynLength( userIndexDB ) && \
			i < maxUserObjects && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 i++, iterationCount++ )
		{
		status = readIndexEntry( &stream, &userIndex[ i ] );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	sMemDisconnect( &stream );
	dynDestroy( &userIndexDB );
	if( cryptStatusError( status ) )
		return( status );
	if( i > maxUserObjects )
		return( CRYPT_ERROR_OVERFLOW );

	return( i );
	}

/* Write the user index file */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeUserIndexEntry( INOUT STREAM *stream, 
								const USER_FILE_INFO *userIndexPtr )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( userIndexPtr, sizeof( USER_FILE_INFO ) ) );

	writeSequence( stream, 2 * sizeofObject( KEYID_SIZE ) + \
				   sizeofObject( userIndexPtr->userNameLength ) + \
				   sizeofShortInteger( userIndexPtr->fileRef) );
	writeOctetString( stream, userIndexPtr->userID, KEYID_SIZE, DEFAULT_TAG );
	writeOctetString( stream, userIndexPtr->creatorID, KEYID_SIZE, DEFAULT_TAG );
	writeCharacterString( stream, userIndexPtr->userName,
						  userIndexPtr->userNameLength, BER_STRING_UTF8 );
	return( writeShortInteger( stream, userIndexPtr->fileRef, DEFAULT_TAG ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int writeUserIndex( IN_HANDLE const CRYPT_KEYSET iIndexKeyset,
						   IN_ARRAY( noUserIndexEntries ) \
							USER_FILE_INFO *userIndex, 
						   IN_RANGE( 1, MAX_USER_OBJECTS ) \
							const int noUserIndexEntries )
	{
	STREAM stream;
	MESSAGE_DATA msgData;
	BYTE userIndexData[ MAX_USERINDEX_SIZE + 8 ];
	int userIndexDataLength, i, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( userIndex, \
						noUserIndexEntries * sizeof( USER_FILE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iIndexKeyset ) );
	REQUIRES( noUserIndexEntries > 0 && \
			  noUserIndexEntries <= MAX_USER_OBJECTS );

	/* Write the user index data to a buffer so that we can send it to the 
	   index keyset */
	sMemOpen( &stream, userIndexData, MAX_USERINDEX_SIZE );
	for( i = 0, iterationCount = 0; 
		 i < noUserIndexEntries && cryptStatusOK( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 i++, iterationCount++ )
		{
		if( userIndex[ i ].state != USER_STATE_NONE )
			status = writeUserIndexEntry( &stream, &userIndex[ i ] );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	userIndexDataLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the user index data to the keyset */
	setMessageData( &msgData, userIndexData, userIndexDataLength );
	return( krnlSendMessage( iIndexKeyset, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_USERINDEX ) );
	}

/****************************************************************************
*																			*
*							Read/Write User Data							*
*																			*
****************************************************************************/

/* Read/write user data:

	UserInfo ::= SEQUENCE {
		role				ENUMERATED,				-- SO/user/CA
		iD					OCTET STRING SIZE(16),	-- User ID
		creatorID			OCTET STRING SIZE(16),	-- Creating SO's ID
		name				UTF8String,				-- User name
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readUserData( INOUT USER_FILE_INFO *userFileInfoPtr, 
						 IN_BUFFER( userDataLength ) const void *userData, 
						 IN_LENGTH_SHORT const int userDataLength )
	{
	STREAM stream;
	int enumValue, length, status;

	assert( isWritePtr( userFileInfoPtr, sizeof( USER_FILE_INFO ) ) );
	assert( isReadPtr( userData, userDataLength ) );

	REQUIRES( userDataLength > 0 && userDataLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( userFileInfoPtr, 0, sizeof( USER_FILE_INFO ) );

	/* Read the user info */
	sMemConnect( &stream, userData, userDataLength );
	readSequence( &stream, NULL );
	readEnumerated( &stream, &enumValue );
	userFileInfoPtr->type = enumValue;
	readOctetString( &stream, userFileInfoPtr->userID, &length, 
					 KEYID_SIZE, KEYID_SIZE );
	readOctetString( &stream, userFileInfoPtr->creatorID, &length, 
					 KEYID_SIZE, KEYID_SIZE );
	status = readCharacterString( &stream, userFileInfoPtr->userName,
								  CRYPT_MAX_TEXTSIZE, 
								  &userFileInfoPtr->userNameLength,
								  BER_STRING_UTF8 );
	sMemDisconnect( &stream );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int writeUserData( OUT_BUFFER( userDataMaxLength, \
									  *userDataLength ) void *userData, 
						  IN_LENGTH_SHORT const int userDataMaxLength,
						  OUT_LENGTH_SHORT_Z int *userDataLength, 
						  const USER_INFO *userInfoPtr )
	{
	const USER_FILE_INFO *userFileInfo = &userInfoPtr->userFileInfo;
	STREAM stream;
	int status;

	assert( isWritePtr( userData, userDataMaxLength ) );
	assert( isWritePtr( userDataLength, sizeof( int ) ) );
	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( userDataMaxLength > 0 && \
			  userDataMaxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( userData, 0, min( 16, userDataMaxLength ) );
	*userDataLength = 0;

	/* Write the user information to a memory buffer */
	sMemOpen( &stream, userData, userDataMaxLength );
	writeSequence( &stream, sizeofShortInteger( userFileInfo->type ) + \
				   2 * sizeofObject( KEYID_SIZE ) + \
				   sizeofObject( userFileInfo->userNameLength ) );
	writeEnumerated( &stream, userFileInfo->type, DEFAULT_TAG );
	writeOctetString( &stream, userFileInfo->userID, KEYID_SIZE, 
					  DEFAULT_TAG );
	writeOctetString( &stream, userFileInfo->creatorID, KEYID_SIZE, 
					  DEFAULT_TAG );
	status = writeCharacterString( &stream, userFileInfo->userName,
								   userFileInfo->userNameLength, 
								   BER_STRING_UTF8 );
	if( cryptStatusOK( status ) )
		*userDataLength = stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/* Send user data to a user keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int commitUserData( IN_HANDLE const CRYPT_KEYSET iUserKeyset, 
						   const USER_INFO *userInfoPtr, 
						   IN_BUFFER( userDataLength ) const void *userData, 
						   IN_LENGTH_SHORT const int userDataLength )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( userData, userDataLength ) );

	REQUIRES( isHandleRangeValid( iUserKeyset ) );
	REQUIRES( userDataLength > 0 && userDataLength < MAX_INTLENGTH_SHORT );

	/* Add the user ID and SO-signed user info to the keyset */
	setMessageData( &msgData, ( MESSAGE_CAST ) userData, userDataLength );
	status = krnlSendMessage( iUserKeyset, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_USERINFO );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, 
						( MESSAGE_CAST ) userInfoPtr->userFileInfo.userID,
						KEYID_SIZE );
		status = krnlSendMessage( iUserKeyset, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_USERID );
		}
	return( status );
	}

/* Read a user's info from a user keyset and verify it using the creating
   SO's key */

#if 0	/*!!!!!!!!!!!!!!! Needs a serious overhaul !!!!!!!!!!!!!!!!!!!!!*/
		/*!!!!!!!!!!!!! Should also do recursive walk !!!!!!!!!!!!!!!!!!*/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getCheckUserInfo( INOUT USER_FILE_INFO *userFileInfoPtr, 
							 IN_INT_SHORT_Z const int fileRef )
	{
	CRYPT_ALGO_TYPE hashAlgo;
	CRYPT_CONTEXT iHashContext;
	CRYPT_KEYSET iUserKeyset;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	STREAM stream;
	DYNBUF userDataDB;
	void *hashDataPtr, *signaturePtr;
	int soFileRef, hashDataLength, signatureLength, status;

	assert( isWritePtr( userFileInfoPtr, sizeof( USER_FILE_INFO ) ) );

	REQUIRES( fileRef >= 0 && fileRef < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( userFileInfoPtr, 0, sizeof( USER_FILE_INFO ) );

	/* Open the user keyset and read the user data from it */
	status = openUserKeyset( &iUserKeyset, fileRef, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		return( status );
	status = dynCreate( &userDataDB, iUserKeyset, 
						CRYPT_IATTRIBUTE_USERINFO );
	krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Burrow into the user info to get the information we need.  We do it
	   this way rather than using envelopes because we don't need the full
	   generality of the enveloping process (we know exactly what data to
	   expect) and to avoid the overhead of de-enveloping data every time a
	   user logs in */
	sMemConnect( &stream, dynData( userDataDB ), dynLength( userDataDB ) );
	readSequence( &stream, NULL );			/* Outer wrapper */
	readUniversal( &stream );				/* ContentType OID */
	readConstructed( &stream, NULL, 0 );	/* Content */
	readSequence( &stream, NULL );
	readUniversal( &stream );				/* Version */
	readSet( &stream, NULL );				/* DigestAlgorithms */
	readAlgoID( &stream, &hashAlgo );
	readSequence( &stream, NULL );			/* EncapContentInfo */
	readUniversal( &stream );				/* ContentType OID */
	readConstructed( &stream, NULL, 0 );	/* Content type wrapper */
	status = readGenericHole( &stream, &hashDataLength, 16, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		dynDestroy( &userDataDB );
		return( status );
		}
	hashDataPtr = sMemBufPtr( &stream );

	/* Read the user info */
	status = readUserData( userFileInfoPtr, hashDataPtr, hashDataLength );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		dynDestroy( &userDataDB );
		return( status );
		}

	/* Hash the signed data and verify the signature using the SO key */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		dynDestroy( &userDataDB );
		sMemDisconnect( &stream );
		return( status );
		}
	iHashContext = createInfo.cryptHandle;
	krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, hashDataPtr, 
					 hashDataLength );
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 );
	dynDestroy( &userDataDB );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* Read the signature */
	status = readSet( &stream, &signatureLength );
	signaturePtr = sMemBufPtr( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Open the SO keyset and read the SO public key from it */
	status = soFileRef = \
		findUserIndexEntry( USERID_USERID, userFileInfoPtr->creatorID, 
							KEYID_SIZE );
	if( cryptStatusOK( status ) )
		status = openUserKeyset( &iUserKeyset, soFileRef, 
								 CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_KEYID,
						   userFileInfoPtr->creatorID, KEYID_SIZE, NULL, 0,
						   KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( iUserKeyset, IMESSAGE_KEY_GETKEY,
							  &getkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
	krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Verify the signature using the SO key */
	status = iCryptCheckSignatureEx( signaturePtr, signatureLength,
									 CRYPT_FORMAT_CRYPTLIB,
									 getkeyInfo.cryptHandle, iHashContext, 
									 NULL );
	krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
	/* MAC (???) using password - needs PKCS #15 changes */
	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

	return( status );
	}
#endif /*!!!!!!!!!!!!!!! Needs a serious overhaul !!!!!!!!!!!!!!!!!!!!!*/

/****************************************************************************
*																			*
*							SO Management Functions							*
*																			*
****************************************************************************/

/* Return the primary SO user info.  This is used as a template to create 
   the primary SO user after a zeroise */

const USER_FILE_INFO *getPrimarySoUserInfo( void )
	{
	return( &primarySOInfo );
	}

/* Sign the user info and write it to the user keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int signUserData( IN_HANDLE const CRYPT_KEYSET iUserKeyset,
						 const USER_INFO *userInfoPtr,
						 IN_HANDLE const CRYPT_CONTEXT iSignContext )
	{
	ERROR_INFO errorInfo;
	BYTE userInfoBuffer[ USERDATA_BUFFERSIZE + 8 ];
	int userInfoLength, status;

	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( isHandleRangeValid( iUserKeyset ) );
	REQUIRES( isHandleRangeValid( iSignContext ) );

	/* Set dummy user data */
	memset( userInfoBuffer, '*', 16 );
	userInfoLength = 16;

	/* Sign the data via an envelope.  This is kind of heavyweight, but it's 
	   OK because we rarely create new users and it saves having to hand-
	   assemble the data like the PKCS #15 code does */
	status = envelopeSign( userInfoBuffer, userInfoLength, 
						   userInfoBuffer, USERDATA_BUFFERSIZE, 
						   &userInfoLength, CRYPT_CONTENT_DATA, 
						   iSignContext, CRYPT_UNUSED, &errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	return( CRYPT_ERROR_SIGNATURE );
	}

CHECK_RETVAL \
static int sigCheckUserData( void )
	{
	return( CRYPT_ERROR_SIGNATURE );
	}

/* Create an SO private key and write it to the user keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int createSOKey( IN_HANDLE const CRYPT_KEYSET iUserKeyset,
						INOUT USER_INFO *userInfoPtr, 
						IN_BUFFER( passwordLength ) const char *password, 
						IN_LENGTH_SHORT const int passwordLength )
	{
	const USER_FILE_INFO *userFileInfo = &userInfoPtr->userFileInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int actionPerms = MK_ACTION_PERM( MESSAGE_CTX_SIGN,
											ACTION_PERM_NONE_EXTERNAL ) | \
							MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK,
											ACTION_PERM_NONE_EXTERNAL );
	int status;

	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( isHandleRangeValid( iUserKeyset ) );
	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH_SHORT );

	/* Create the SO private key, making it internal and signature-only */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, ( MESSAGE_CAST ) userFileInfo->userName,
					min( userFileInfo->userNameLength, CRYPT_MAX_TEXTSIZE ) );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_CTXINFO_LABEL );
	status = krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE,
								  ( int * ) &actionPerms,
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add the SO private key to the keyset */
	status = addKey( iUserKeyset, createInfo.cryptHandle, 
					 userFileInfo->userID, KEYID_SIZE, password, 
					 passwordLength, TRUE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	userInfoPtr->iCryptContext = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

#if 0	/* Currently unused, for future use for CA users */

/* Create a CA secret key and write it to the user keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
static int createCAKey( IN_HANDLE const CRYPT_KEYSET iUserKeyset,
						INOUT USER_INFO *userInfoPtr, 
						IN_BUFFER( passwordLength ) const char *password, 
						IN_LENGTH_SHORT const int passwordLength )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	const int actionPerms = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT,
											ACTION_PERM_NONE_EXTERNAL ) | \
							MK_ACTION_PERM( MESSAGE_CTX_DECRYPT,
											ACTION_PERM_NONE_EXTERNAL );
	int status;

	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( isHandleRangeValid( iUserKeyset ) );
	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH_SHORT );

	/* Create the CA secret key, making it internal-only */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_3DES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, userInfoPtr->userID, KEYID_SIZE );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_CTXINFO_LABEL );
	status = krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE,
								  ( int * ) &actionPerms,
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add the CA secret key to the keyset */
	status = addKey( iUserKeyset, createInfo.cryptHandle, 
					 userInfoPtr->userID, KEYID_SIZE, password, 
					 passwordLength, FALSE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* 0 */

/* Create a primary SO user.  This can only occur when we're in the zeroised 
   state */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createPrimarySoUser( INOUT USER_INFO *userInfoPtr, 
								IN_BUFFER( passwordLength ) const char *password, 
								IN_LENGTH_SHORT const int passwordLength )
	{
	CRYPT_KEYSET iIndexKeyset, iUserKeyset;
	USER_FILE_INFO userIndex;
	BYTE userData[ USERDATA_BUFFERSIZE + 8 ];
	int userDataLength = DUMMY_INIT, status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH_SHORT );

	/* Create the user index file and user file for the primary SO user */
	status = openIndexKeyset( &iIndexKeyset, CRYPT_KEYOPT_CREATE );
	if( cryptStatusError( status ) )
		return( status );
	status = openUserKeyset( &iUserKeyset, 0, CRYPT_KEYOPT_CREATE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Update the index file */
	memcpy( &userIndex, &userInfoPtr->userFileInfo, 
			sizeof( USER_FILE_INFO ) );
	userIndex.fileRef = 0;
	status = writeUserIndex( iIndexKeyset, &userIndex, MAX_USER_OBJECTS );
	krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		/* We couldn't update the index file, delete the newly-created user
		   keyset. Since we haven't written anything to it, it's zero-length
		   so it's deleted automatically on close */
		krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	userInfoPtr->iKeyset = iUserKeyset;

	/* Create the SO key and the user keyset file */
	status = createSOKey( iUserKeyset, userInfoPtr, password, 
						  passwordLength );
	if( cryptStatusOK( status ) )
		status = writeUserData( userData, USERDATA_BUFFERSIZE, 
								&userDataLength, userInfoPtr );
	if( cryptStatusOK( status ) )
		status = commitUserData( iUserKeyset, userInfoPtr, userData, 
								 userDataLength );
	if( cryptStatusError( status ) )
		{
		/* The primary SO create failed, return to the zeroised state.  
		   Since we're already in an exception state here there's not
		   much that we can do if the zeroise fails */
		krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );
		( void ) zeroiseUsers( userInfoPtr );
		return( status );
		}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*status = createCAKey( iUserKeyset, userInfoPtr, password, passwordLength );*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

	return( status );
	}

/****************************************************************************
*																			*
*							User Management Functions						*
*																			*
****************************************************************************/

/* Check whether a supplied password is the zeroise password */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN isZeroisePassword( IN_BUFFER( passwordLen ) const char *password,
						   IN_LENGTH_SHORT const int passwordLen )
	{
	assert( isReadPtr( password, passwordLen ) );

	REQUIRES( passwordLen > 0 && passwordLen < MAX_INTLENGTH_SHORT );

	if( passwordLen != PRIMARYSO_PASSWORD_LENGTH )
		return( FALSE );
	return( !memcmp( password, PRIMARYSO_PASSWORD, 
					 PRIMARYSO_PASSWORD_LENGTH ) || \
			!memcmp( password, PRIMARYSO_ALTPASSWORD,
					 PRIMARYSO_PASSWORD_LENGTH ) ? \
			TRUE : FALSE );
	}

/* Perform a zeroise */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int zeroiseUsers( INOUT USER_INFO *userInfoPtr )
	{
	USER_INDEX_INFO *userIndexInfo = userInfoPtr->userIndexPtr;
	USER_FILE_INFO *userIndex = userIndexInfo->userIndex;
	char userFilePath[ MAX_PATH_LENGTH + 1 + 8 ];
	int userFilePathLen, i, iterationCount, status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	/* Read the user index and step through each entry clearing the user 
	   info for it */
	for( i = 0, iterationCount = 0; 
		 i < userIndexInfo->lastEntry && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 i++, iterationCount++ )
		{
		char userFileName[ 16 + 8 ];

		/* Erase the given user keyset */
		sprintf_s( userFileName, 16, "u%06x",  userIndex[ i ].fileRef );
		status = fileBuildCryptlibPath( userFilePath, MAX_PATH_LENGTH, 
										&userFilePathLen, userFileName, 
										strlen( userFileName ), 
										BUILDPATH_GETPATH );
		if( cryptStatusOK( status ) )
			{
			userFilePath[ userFilePathLen ] = '\0';
			fileErase( userFilePath );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* Erase the index file */
	status = fileBuildCryptlibPath( userFilePath, MAX_PATH_LENGTH, 
									&userFilePathLen, "index", 5, 
									BUILDPATH_GETPATH );
	if( cryptStatusOK( status ) )
		{
		userFilePath[ userFilePathLen ] = '\0';
		fileErase( userFilePath );
		}
	return( status );
	}

/* Create a user keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createUserKeyset( INOUT USER_INFO *defaultUserInfoPtr,
							 INOUT USER_INFO *newUserInfoPtr )
	{
	CRYPT_KEYSET iIndexKeyset, iUserKeyset;
	USER_FILE_INFO *userFileInfo = &newUserInfoPtr->userFileInfo;
	USER_FILE_INFO *userIndexPtr;
	int status;

	assert( isReadPtr( defaultUserInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( newUserInfoPtr, sizeof( USER_INFO ) ) );

	/* Try and open the index file */
	status = openIndexKeyset( &iIndexKeyset, CRYPT_IKEYOPT_EXCLUSIVEACCESS );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the index entry for the new user */
	status = createUserEntry( &userIndexPtr, 
							  defaultUserInfoPtr->userIndexPtr, 
							  MAX_USER_OBJECTS, userFileInfo );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Create the user keyset */
	status = openUserKeyset( &iUserKeyset, userFileInfo->fileRef, 
							 CRYPT_KEYOPT_CREATE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* We've got the user keyset and info created, update the in-memory 
	   index and index file */
	memcpy( userIndexPtr, userFileInfo, sizeof( USER_FILE_INFO ) );
	status = writeUserIndex( iIndexKeyset, defaultUserInfoPtr->userIndexPtr, 
							 MAX_USER_OBJECTS );
	krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		/* We couldn't update the index file, delete the newly-created user
		   keyset (since we haven't written anything to it, it's zero-length
		   so it's deleted automatically on close) */
		krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );
		}
	krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );

	/* Clean up */
	return( status );
	}

/* Set/change the password for a user object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setUserPassword( INOUT USER_INFO *userInfoPtr,
					 IN_BUFFER( passwordLength ) const char *password, 
					 IN_LENGTH_SHORT const int passwordLength )
	{
	CRYPT_KEYSET iUserKeyset;
	USER_FILE_INFO *userFileInfo = &userInfoPtr->userFileInfo;
	int status;

	assert( isReadPtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH_SHORT );

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!! Dummy references to keep the compiler happy !!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
{
USER_FILE_INFO dummyUserInfo = { 0 }, *userFileInfoPtr = &dummyUserInfo;
USER_INFO userInfo;

( void ) readUserData( userFileInfoPtr, "", 0 );
( void ) signUserData( 0, &userInfo, 0 );
( void ) sigCheckUserData();
( void ) createSOKey( 0, &userInfo, "", 0 );
( void ) createUserKeyset( &userInfo, &userInfo );
}
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

	/* No-one can ever directly set the default SO password */
	if( passwordLength == PRIMARYSO_PASSWORD_LENGTH && \
		( !memcmp( password, PRIMARYSO_PASSWORD,
				   PRIMARYSO_PASSWORD_LENGTH ) || \
		  !memcmp( password, PRIMARYSO_ALTPASSWORD,
				   PRIMARYSO_PASSWORD_LENGTH ) ) )
		return( CRYPT_ERROR_WRONGKEY );

	/* If we're setting the password for the primary SO in the zeroised
	   state, create a new user keyset and SO authentication key and write
	   the details to the keyset */
	if( userFileInfo->fileRef == -1 )
		{
		status = createPrimarySoUser( userInfoPtr, password, 
									  passwordLength );
		
		return( status );
		}

	/* Open an existing user keyset */
	status = openUserKeyset( &iUserKeyset, userFileInfo->fileRef, 
							 CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
	/* set state = USER_INITED */
	/* write MAC( ??? ) to user file - needs PKCS #15 changes */
	/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

	/* Close the keyset and commit the changes */
	krnlSendNotifier( iUserKeyset, IMESSAGE_DECREFCOUNT );

	/* The password has been set, we're now in the user inited state */
	userFileInfo->state = USER_STATE_USERINITED;
	return( CRYPT_OK );
	}

/* Initialise the user index in the default user object from the index file,
   and clean up after we're done with it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initUserIndex( OUT_OPT_PTR void **userIndexPtrPtr )
	{
	CRYPT_KEYSET iIndexKeyset;
	USER_INDEX_INFO *userIndexInfo;
	int noEntries, status;

	assert( isWritePtr( userIndexPtrPtr, sizeof( void * ) ) );

	/* Clear return value */
	*userIndexPtrPtr = NULL;

	/* Open the index file and read the index entries from it.  We open it
	   in exclusive mode since nothing else should be accessing it at this
	   point.

	   What to do if this fails is a bit tricky (see the comment in
	   createDefaultUserObject() for the config file read), however for the 
	   index keyset it's a bit more clear-cut, we shouldn't fail if the
	   access fails so we just skip it and continue, making it look as if
	   we're in the zeroised state */
	status = openIndexKeyset( &iIndexKeyset, CRYPT_IKEYOPT_EXCLUSIVEACCESS );
	if( cryptStatusError( status ) )
		{
		/* If there's no index file present, we're already in the zeroised
		   state */
		if( status == CRYPT_ERROR_NOTFOUND )
			return( CRYPT_OK );

		/* If keysets are disabled, we fall back to the hardwired 
		   configuration parameters */
		if( status == CRYPT_ERROR_NOTAVAIL )
			return( CRYPT_OK );

		/* Warn the user (in debug mode) that something went wrong */
		DEBUG_DIAG(( "Couldn't read user index, assuming zeroised state" ));
		assert( DEBUG_WARN );
		DEBUG_PRINT(( "User index read failed with status %d.\n", status ));

#if 0	/* Another problematic choice, should we potentially destroy a 
		   damaged index or leave it for the user to fix up?  Since this 
		   situation would never normally occur we leave it as a user-to-fix
		   for now */
		/* If there's something there but it's damaged, delete it so that we 
		   can start again */
		if( status == CRYPT_ERROR_BADDATA )
			{
			char userFilePath[ MAX_PATH_LENGTH + 1 + 8 ];
			int userFilePathLen;

			status = fileBuildCryptlibPath( userFilePath, MAX_PATH_LENGTH, 
											&userFilePathLen, "index", 5,
											BUILDPATH_GETPATH );
			if( cryptStatusOK( status ) )
				{
				userFilePath[ userFilePathLen ] = '\0';
				fileErase( userFilePath );
				}
			}
#endif /* 0 */

		return( CRYPT_OK );
		}

	/* Allocate room for the user index and read it into the default user 
	   object */
	if( ( userIndexInfo = clAlloc( "initUserIndex", \
								   sizeof( USER_INDEX_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( userIndexInfo, 0, sizeof( USER_INDEX_INFO ) );
	status = noEntries = readIndex( iIndexKeyset, userIndexInfo->userIndex,
									MAX_USER_OBJECTS );
	krnlSendNotifier( iIndexKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		clFree( "initUserIndex", userIndexInfo );
		return( status );
		}
	userIndexInfo->lastEntry = noEntries;
	*userIndexPtrPtr = userIndexInfo;

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endUserIndex( INOUT void *userIndexPtr )
	{
	USER_INDEX_INFO *userIndexInfo = userIndexPtr;

	assert( isWritePtr( userIndexInfo, sizeof( USER_INDEX_INFO ) ) );

	zeroise( userIndexInfo, sizeof( USER_INDEX_INFO ) );
	clFree( "endUserIndex", userIndexInfo );
	}
