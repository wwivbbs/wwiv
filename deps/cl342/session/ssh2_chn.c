/****************************************************************************
*																			*
*						cryptlib SSHv2 Channel Management					*
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

/* Channel flags */

#define CHANNEL_FLAG_NONE		0x00	/* No channel flag */
#define CHANNEL_FLAG_ACTIVE		0x01	/* Channel is active */
#define CHANNEL_FLAG_WRITECLOSED 0x02	/* Write-side of ch.closed */

/* Per-channel information.  SSH channel IDs are 32-bit/4 byte data values
   and can be reused during sessions so we provide our own guaranteed-unique
   short int ID for users to identify a particular channel.  Since each
   channel can have its own distinct characteristics we have to record
   information like the window size and count and packet size information on 
   a per-channel basis.  In addition if the channel is tied to a forwarded 
   port we also record port-forwarding information in the generic channel-
   type and channel-type-argument strings */

typedef struct {
	/* General channel information.  The read and write channel numbers are 
	   the same for everything but Cisco software */
	int channelID;						/* cryptlib-level channel ID */
	long readChannelNo, writeChannelNo;	/* SSH-level channel ID */
	int flags;							/* Channel flags */

	/* External interface information */
	CRYPT_ATTRIBUTE_TYPE cursorPos;		/* Virtual cursor position */

	/* Channel parameters */
	int windowCount, windowSize;		/* Current window usage and tot.size */
	int maxPacketSize;					/* Max allowed packet size */

	/* Channel naming information */
	BUFFER( CRYPT_MAX_TEXTSIZE, typeLen ) \
	char type[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, arg1Len ) \
	char arg1[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, arg2Len ) \
	char arg2[ CRYPT_MAX_TEXTSIZE + 8 ];
	int typeLen, arg1Len, arg2Len;

	/* Channel extra data.  This contains encoded oddball protocol-specific
	   SSH packets to be sent or having been received */
	BUFFER_FIXED( UINT_SIZE + CRYPT_MAX_TEXTSIZE + ( UINT_SIZE * 4 ) ) \
	BYTE extraData[ ( UINT_SIZE + CRYPT_MAX_TEXTSIZE ) + \
					( UINT_SIZE * 4 ) + 8 ];
	} SSH_CHANNEL_INFO;

/* Check whether a channel corresponds to a null channel (a placeholder used
   when there's currently no channel active) and whether a channel is
   currently active */

#define isNullChannel( channelInfoPtr ) \
		( ( channelInfoPtr )->readChannelNo == UNUSED_CHANNEL_NO )
#define isActiveChannel( channelInfoPtr ) \
		( channelInfoPtr->flags & CHANNEL_FLAG_ACTIVE )

/* The maximum allowed number of channels */

#ifdef USE_SSH_EXTENDED
  #define SSH_MAX_CHANNELS		4
#else
  #define SSH_MAX_CHANNELS		1
#endif /* USE_SSH_EXTENDED */

#ifdef USE_SSH

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check whether there are any active channels still present.  Since a
   channel can be half-closed (we've closed it for write but the other
   side hasn't acknowledged the close yet) we allow the caller to specify
   an excluded channel ID that's treated as logically closed for active
   channel-check purposes even if a channel entry is still present for it.
   This can also be used when closing channels to check whether this is the 
   last channel open, since closing the last channel also shuts down the 
   entire session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isChannelActive( const SESSION_INFO *sessionInfoPtr,
								IN_INT_SHORT_Z const int excludedChannelID )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	int iterationCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_B( ( excludedChannelID == UNUSED_CHANNEL_ID ) || \
				( excludedChannelID > 0 && \
				  excludedChannelID < MAX_INTLENGTH_SHORT ) );

	for( attributeListPtr = sessionInfoPtr->attributeList, \
			iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		const SSH_CHANNEL_INFO *channelInfoPtr;

		/* If it's not an SSH channel, continue */
		if( attributeListPtr->attributeID != CRYPT_SESSINFO_SSH_CHANNEL )
			continue;

		/* It's an SSH channel, check whether it's the one that we're
		   after */
		ENSURES( attributeListPtr->valueLength == sizeof( SSH_CHANNEL_INFO ) );
		channelInfoPtr = attributeListPtr->value;
		if( isActiveChannel( channelInfoPtr ) && \
			channelInfoPtr->channelID != excludedChannelID )
			return( TRUE );
		}
	ENSURES_B( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( FALSE );
	}

/* Helper function used to access SSH-specific internal attributes within
   an attribute group (== single attribute-list item containing multiple
   sub-items).  Returns the attribute ID of the currently selected attribute
   when attrGetType == ATTR_CURRENT, otherwise a boolean indicating whether
   ATTR_PREV/ATTR_NEXT is still within the current subgroup */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int accessFunction( INOUT ATTRIBUTE_LIST *attributeListPtr,
						   IN_ENUM_OPT( ATTR ) const ATTR_TYPE attrGetType,
						   OUT_INT_Z int *value )
	{
	static const CRYPT_ATTRIBUTE_TYPE attributeOrderList[] = {
			CRYPT_SESSINFO_SSH_CHANNEL, CRYPT_SESSINFO_SSH_CHANNEL_TYPE,
			CRYPT_SESSINFO_SSH_CHANNEL_ARG1, CRYPT_SESSINFO_SSH_CHANNEL_ARG2,
			CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, CRYPT_ATTRIBUTE_NONE,
			CRYPT_ATTRIBUTE_NONE };
	SSH_CHANNEL_INFO *channelInfoPtr = attributeListPtr->value;
	CRYPT_ATTRIBUTE_TYPE attributeType = channelInfoPtr->cursorPos;
	BOOLEAN doContinue;
	int iterationCount;

	assert( isWritePtr( attributeListPtr, sizeof( ATTRIBUTE_LIST ) ) );
	assert( isWritePtr( value, sizeof( int ) ) );

	REQUIRES( attrGetType >= ATTR_NONE && attrGetType < ATTR_LAST );

	/* Clear return value */
	*value = 0;

	/* If we've just moved the cursor onto this attribute, reset the
	   position to the first internal attribute */
	if( attributeListPtr->flags & ATTR_FLAG_CURSORMOVED )
		{
		attributeType = channelInfoPtr->cursorPos = \
						CRYPT_SESSINFO_SSH_CHANNEL;
		attributeListPtr->flags &= ~ATTR_FLAG_CURSORMOVED;
		}

	/* If it's an information fetch, return the currently-selected 
	   attribute */
	if( attrGetType == ATTR_NONE )
		{
		*value = attributeType;
		return( CRYPT_OK );
		}

	for( doContinue = TRUE, iterationCount = 0; 
		 doContinue && iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		int orderIndex;

		/* Find the position of the current sub-attribute in the attribute
		   order list and use that to get its successor/predecessor sub-
		   attribute */
		for( orderIndex = 0; 
			 attributeOrderList[ orderIndex ] != attributeType && \
				attributeOrderList[ orderIndex ] != CRYPT_ATTRIBUTE_NONE && \
				orderIndex < FAILSAFE_ARRAYSIZE( attributeOrderList, \
												 CRYPT_ATTRIBUTE_TYPE ); 
			 orderIndex++ );
		ENSURES( orderIndex < FAILSAFE_ARRAYSIZE( attributeOrderList, \
										 CRYPT_ATTRIBUTE_TYPE ) );
		if( attributeOrderList[ orderIndex ] == CRYPT_ATTRIBUTE_NONE )
			{
			/* We've reached the first/last sub-attribute within the current
			   item/group, tell the caller that there are no more sub-
			   attributes present and they have to move on to the next item/
			   group */
			*value = FALSE;
			return( CRYPT_OK );
			}
		if( attrGetType == ATTR_PREV )
			{
			attributeType = ( orderIndex <= 0 ) ? \
								CRYPT_ATTRIBUTE_NONE : \
								attributeOrderList[ orderIndex - 1 ];
			}
		else
			attributeType = attributeOrderList[ orderIndex + 1 ];
		if( attributeType == CRYPT_ATTRIBUTE_NONE )
			{
			/* We've reached the first/last sub-attribute within the current
			   item/group, exit as before */
			*value = FALSE;
			return( CRYPT_OK );
			}

		/* Check whether the required sub-attribute is present.  If not, we
		   continue and try the next one */
		switch( attributeType )
			{
			case CRYPT_SESSINFO_SSH_CHANNEL:
			case CRYPT_SESSINFO_SSH_CHANNEL_TYPE:
			case CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE:
				doContinue = FALSE;	/* Always present */
				break;

			case CRYPT_SESSINFO_SSH_CHANNEL_ARG1:
				if( channelInfoPtr->arg1Len > 0 )
					doContinue = FALSE;
				break;

			case CRYPT_SESSINFO_SSH_CHANNEL_ARG2:
				if( channelInfoPtr->arg2Len > 0 )
					doContinue = FALSE;
				break;

			default:
				retIntError();
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	channelInfoPtr->cursorPos = attributeType;

	*value = TRUE;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Find Channel Information						*
*																			*
****************************************************************************/

/* Find the attribute entry for a channel */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static ATTRIBUTE_LIST *findChannelAttr( const SESSION_INFO *sessionInfoPtr,
										IN const long channelNo )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	int iterationCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_N( ( channelNo == CRYPT_USE_DEFAULT ) || \
				( channelNo >= 0 && channelNo <= LONG_MAX ) );

	for( attributeListPtr = sessionInfoPtr->attributeList, \
			iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		const SSH_CHANNEL_INFO *channelInfoPtr;

		/* If it's not an SSH channel, continue */
		if( attributeListPtr->attributeID != CRYPT_SESSINFO_SSH_CHANNEL )
			continue;

		/* It's an SSH channel, check whether it's the one that we're
		   after */
		ENSURES_N( attributeListPtr->valueLength == sizeof( SSH_CHANNEL_INFO ) );
		channelInfoPtr = attributeListPtr->value;
		if( channelNo == CRYPT_USE_DEFAULT )
			{
			/* We're looking for any open channel channel, return the first
			   match */
			if( channelInfoPtr->flags & CHANNEL_FLAG_WRITECLOSED )
				continue;
			return( attributeListPtr );
			}
		if( channelInfoPtr->readChannelNo == channelNo || \
			channelInfoPtr->writeChannelNo == channelNo )
			return( attributeListPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( NULL );
	}

/* Find the SSH channel information for a channel, matching by channel 
   number, channel ID, and channel host + port information */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static SSH_CHANNEL_INFO *findChannelByChannelNo( const SESSION_INFO *sessionInfoPtr,
												 IN const long channelNo )
	{
	const ATTRIBUTE_LIST *attributeListPtr = \
							findChannelAttr( sessionInfoPtr, channelNo );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_N( ( channelNo == CRYPT_USE_DEFAULT ) || \
				( channelNo >= 0 && channelNo <= LONG_MAX ) );

	return( ( attributeListPtr == NULL ) ? NULL : attributeListPtr->value );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static SSH_CHANNEL_INFO *findChannelByID( const SESSION_INFO *sessionInfoPtr,
											IN_INT_SHORT const int channelID )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	int iterationCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_N( channelID > 0 && channelID < MAX_INTLENGTH_SHORT );

	for( attributeListPtr = sessionInfoPtr->attributeList, \
			iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		const SSH_CHANNEL_INFO *channelInfoPtr;

		/* If it's not an SSH channel, continue */
		if( attributeListPtr->attributeID != CRYPT_SESSINFO_SSH_CHANNEL )
			continue;

		/* It's an SSH channel, check whether it's the that one we're
		   after */
		ENSURES_N( attributeListPtr->valueLength == sizeof( SSH_CHANNEL_INFO ) );
		channelInfoPtr = attributeListPtr->value;
		if( channelInfoPtr->channelID == channelID )
			return( ( SSH_CHANNEL_INFO * ) channelInfoPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( NULL );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static SSH_CHANNEL_INFO *findChannelByAddr( const SESSION_INFO *sessionInfoPtr,
											IN_BUFFER( addrInfoLen ) \
												const char *addrInfo,
											IN_LENGTH_SHORT \
												const int addrInfoLen )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	int iterationCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( addrInfo, addrInfoLen ) );

	REQUIRES_N( addrInfoLen > 0 && addrInfoLen < MAX_INTLENGTH_SHORT );

	for( attributeListPtr = sessionInfoPtr->attributeList, \
			iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		const SSH_CHANNEL_INFO *channelInfoPtr;

		/* If it's not an SSH channel, continue */
		if( attributeListPtr->attributeID != CRYPT_SESSINFO_SSH_CHANNEL )
			continue;

		/* It's an SSH channel, check whether it's the one that we're
		   after */
		ENSURES_N( attributeListPtr->valueLength == sizeof( SSH_CHANNEL_INFO ) );
		channelInfoPtr = attributeListPtr->value;
		if( channelInfoPtr->arg1Len == addrInfoLen && \
			!memcmp( channelInfoPtr->arg1, addrInfo, addrInfoLen ) )
			return( ( SSH_CHANNEL_INFO * ) channelInfoPtr );
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_MAX );

	return( NULL );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static const SSH_CHANNEL_INFO *getCurrentChannelInfo( const SESSION_INFO *sessionInfoPtr,
													  IN_ENUM( CHANNEL ) \
														const CHANNEL_TYPE channelType )
	{
	static const SSH_CHANNEL_INFO nullChannel = \
			{ UNUSED_CHANNEL_ID, UNUSED_CHANNEL_NO, UNUSED_CHANNEL_NO, 
			  CHANNEL_FLAG_NONE, CRYPT_ATTRIBUTE_NONE, 0 /*...*/ };
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	const SSH_CHANNEL_INFO *channelInfoPtr;
	const int channelID = ( channelType == CHANNEL_READ ) ? \
							sshInfo->currReadChannel : \
							sshInfo->currWriteChannel;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_N( channelType > CHANNEL_NONE && \
				channelType < CHANNEL_LAST );

	/* If there's no channel open yet, return the null channel */
	if( channelID == UNUSED_CHANNEL_ID )
		return( ( SSH_CHANNEL_INFO * ) &nullChannel );

	channelInfoPtr = findChannelByID( sessionInfoPtr,
									  ( channelType == CHANNEL_READ ) ? \
										sshInfo->currReadChannel : \
										sshInfo->currWriteChannel );
	return( ( channelInfoPtr == NULL ) ? \
			( SSH_CHANNEL_INFO * ) &nullChannel : channelInfoPtr );
	}

/****************************************************************************
*																			*
*							Get/Set Channel Information						*
*																			*
****************************************************************************/

/* Get the currently active channel */

CHECK_RETVAL_RANGE( 1, LONG_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
long getCurrentChannelNo( const SESSION_INFO *sessionInfoPtr,
						  IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr = \
				getCurrentChannelInfo( sessionInfoPtr, channelType );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( channelType == CHANNEL_READ || channelType == CHANNEL_WRITE );
	REQUIRES( channelInfoPtr != NULL );

	return( ( channelType == CHANNEL_READ ) ? \
			channelInfoPtr->readChannelNo : channelInfoPtr->writeChannelNo );
	}

/* Get/set an attribute or SSH-specific internal attribute from/for the 
   current channel */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelAttribute( const SESSION_INFO *sessionInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 OUT_INT_Z int *value )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr = \
				getCurrentChannelInfo( sessionInfoPtr, CHANNEL_READ );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( value, sizeof( int ) ) );

	REQUIRES( isAttribute( attribute ) );
	REQUIRES( channelInfoPtr != NULL );

	/* Clear return values */
	*value = 0;

	if( isNullChannel( channelInfoPtr ) )
		return( CRYPT_ERROR_NOTFOUND );

	switch( attribute )
		{
		case CRYPT_SESSINFO_SSH_CHANNEL:
			*value = channelInfoPtr->channelID;
			return( CRYPT_OK );

		case CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE:
			*value = isActiveChannel( channelInfoPtr ) ? TRUE : FALSE;
			return( CRYPT_OK );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelAttributeS( const SESSION_INFO *sessionInfoPtr,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  OUT_BUFFER_OPT( dataMaxLength, *dataLength ) \
								void *data, 
						  IN_LENGTH_SHORT_Z const int dataMaxLength, 
						  OUT_LENGTH_SHORT_Z int *dataLength )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr = \
				getCurrentChannelInfo( sessionInfoPtr, CHANNEL_READ );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( ( data == NULL && dataMaxLength == 0 ) || \
			isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( isAttribute( attribute ) );
	REQUIRES( ( data == NULL && dataMaxLength == 0 ) || \
			  ( data != NULL && \
				dataMaxLength > 0 && \
				dataMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( channelInfoPtr != NULL );

	/* Clear return values */
	if( data != NULL )
		memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	if( isNullChannel( channelInfoPtr ) )
		return( CRYPT_ERROR_NOTFOUND );

	switch( attribute )
		{
		case CRYPT_SESSINFO_SSH_CHANNEL_TYPE:
			return( attributeCopyParams( data, dataMaxLength, dataLength,
										 channelInfoPtr->type,
										 channelInfoPtr->typeLen ) );

		case CRYPT_SESSINFO_SSH_CHANNEL_ARG1:
			return( attributeCopyParams( data, dataMaxLength, dataLength,
										 channelInfoPtr->arg1,
										 channelInfoPtr->arg1Len ) );

		case CRYPT_SESSINFO_SSH_CHANNEL_ARG2:
			return( attributeCopyParams( data, dataMaxLength, dataLength,
										 channelInfoPtr->arg2,
										 channelInfoPtr->arg2Len ) );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getChannelExtAttribute( const SESSION_INFO *sessionInfoPtr,
							IN_ENUM( SSH_ATTRIBUTE ) \
								const SSH_ATTRIBUTE_TYPE attribute,
							OUT_INT_Z int *value )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr = \
				getCurrentChannelInfo( sessionInfoPtr, CHANNEL_READ );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( value, sizeof( int ) ) );

	REQUIRES( isAttribute( attribute ) );
	REQUIRES( channelInfoPtr != NULL );

	/* Clear return value */
	*value = 0;

	if( isNullChannel( channelInfoPtr ) )
		return( CRYPT_ERROR_NOTFOUND );

	switch( attribute )
		{
		case SSH_ATTRIBUTE_WINDOWCOUNT:
			*value = channelInfoPtr->windowCount;
			return( CRYPT_OK );

		case SSH_ATTRIBUTE_WINDOWSIZE:
			*value = channelInfoPtr->windowSize;
			return( CRYPT_OK );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setChannelAttribute( INOUT SESSION_INFO *sessionInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 IN_INT_SHORT const int value )
	{
	SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isAttribute( attribute ) );
	REQUIRES( value > 0 && value < MAX_INTLENGTH_SHORT );

	/* If we're setting the channel ID this doesn't change any channel
	   attribute but selects the one with the given ID */
	if( attribute == CRYPT_SESSINFO_SSH_CHANNEL )
		{
		channelInfoPtr = findChannelByID( sessionInfoPtr, value );
		if( channelInfoPtr == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		return( selectChannel( sessionInfoPtr, channelInfoPtr->writeChannelNo,
							   CHANNEL_WRITE ) );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int setChannelAttributeS( INOUT SESSION_INFO *sessionInfoPtr,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						  IN_BUFFER( dataLength ) const void *data, 
						  IN_LENGTH_TEXT const int dataLength )
	{
	SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( isAttribute( attribute ) );
	REQUIRES( dataLength > 0 && dataLength <= CRYPT_MAX_TEXTSIZE );

	/* Set the attribute for the currently-active channel */
	channelInfoPtr = ( SSH_CHANNEL_INFO * ) \
				getCurrentChannelInfo( sessionInfoPtr, CHANNEL_READ );
	REQUIRES( channelInfoPtr != NULL );
	if( isNullChannel( channelInfoPtr ) )
		return( CRYPT_ERROR_NOTFOUND );
	switch( attribute )
		{
		case CRYPT_SESSINFO_SSH_CHANNEL_TYPE:
			return( attributeCopyParams( channelInfoPtr->type, 
										 CRYPT_MAX_TEXTSIZE,
										 &channelInfoPtr->typeLen, 
										 data, dataLength ) );

		case CRYPT_SESSINFO_SSH_CHANNEL_ARG1:
			return( attributeCopyParams( channelInfoPtr->arg1, 
										 CRYPT_MAX_TEXTSIZE,
										 &channelInfoPtr->arg1Len, 
										 data, dataLength ) );

		case CRYPT_SESSINFO_SSH_CHANNEL_ARG2:
			return( attributeCopyParams( channelInfoPtr->arg2, 
										 CRYPT_MAX_TEXTSIZE,
										 &channelInfoPtr->arg2Len, 
										 data, dataLength ) );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setChannelExtAttribute( const SESSION_INFO *sessionInfoPtr,
							IN_ATTRIBUTE const SSH_ATTRIBUTE_TYPE attribute,
							IN_INT_Z const int value )
	{
	SSH_CHANNEL_INFO *channelInfoPtr = ( SSH_CHANNEL_INFO * ) \
				getCurrentChannelInfo( sessionInfoPtr, CHANNEL_READ );

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES( ( attribute == SSH_ATTRIBUTE_ACTIVE && value == TRUE ) || \
			  ( attribute != SSH_ATTRIBUTE_ACTIVE && \
				value >= 0 && value < INT_MAX ) );
	REQUIRES( channelInfoPtr != NULL );

	if( isNullChannel( channelInfoPtr ) )
		return( CRYPT_ERROR_NOTFOUND );

	switch( attribute )
		{
		case SSH_ATTRIBUTE_ACTIVE:
			channelInfoPtr->flags |= CHANNEL_FLAG_ACTIVE;
			return( CRYPT_OK );

		case SSH_ATTRIBUTE_WINDOWCOUNT:
			channelInfoPtr->windowCount = value;
			return( CRYPT_OK );

		case SSH_ATTRIBUTE_WINDOWSIZE:
			channelInfoPtr->windowSize = value;
			return( CRYPT_OK );

		case SSH_ATTRIBUTE_ALTCHANNELNO:
			channelInfoPtr->writeChannelNo = value;
			return( CRYPT_OK );
		}

	retIntError();
	}

/* Get the status of a channel: Not open, read-side open/write-side closed, 
   open */

CHECK_RETVAL_ENUM( CHANNEL ) STDC_NONNULL_ARG( ( 1 ) ) \
CHANNEL_TYPE getChannelStatusByChannelNo( const SESSION_INFO *sessionInfoPtr,
										  IN const long channelNo )
	{
	SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES_EXT( ( channelNo >= 0 && channelNo <= LONG_MAX ), \
				  CHANNEL_NONE );

	channelInfoPtr = findChannelByChannelNo( sessionInfoPtr, channelNo );
	return( ( channelInfoPtr == NULL ) ? CHANNEL_NONE : \
			( channelInfoPtr->flags & CHANNEL_FLAG_WRITECLOSED ) ? \
				CHANNEL_READ : CHANNEL_BOTH );
	}

CHECK_RETVAL_ENUM( CHANNEL ) STDC_NONNULL_ARG( ( 1 ) ) \
CHANNEL_TYPE getChannelStatusByAddr( const SESSION_INFO *sessionInfoPtr,
									 IN_BUFFER( addrInfoLen ) const char *addrInfo,
									 IN_LENGTH_SHORT const int addrInfoLen )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( addrInfo, addrInfoLen ) );

	REQUIRES_EXT( ( addrInfoLen > 0 && addrInfoLen < MAX_INTLENGTH_SHORT ), \
				  CHANNEL_NONE );

	channelInfoPtr = findChannelByAddr( sessionInfoPtr, addrInfo,
										addrInfoLen );
	return( ( channelInfoPtr == NULL ) ? CHANNEL_NONE : \
			( channelInfoPtr->flags & CHANNEL_FLAG_WRITECLOSED ) ? \
				CHANNEL_READ : CHANNEL_BOTH );
	}

/****************************************************************************
*																			*
*							Channel Management Functions					*
*																			*
****************************************************************************/

/* Select a channel */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM_OPT( CHANNEL ) const CHANNEL_TYPE channelType )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( ( channelNo == CRYPT_USE_DEFAULT ) || \
			  ( channelNo >= 0 && channelNo <= LONG_MAX ) );
			  /* CRYPT_USE_DEFAULT is used to select the first available 
			     channel, used when closing all channels in a loop */
	REQUIRES( ( channelType == CHANNEL_NONE ) || \
			  ( channelType > CHANNEL_NONE && channelType < CHANNEL_LAST ) );
			  /* We allow CHANNEL_NONE to allow selection of created-but-not-
			     yet-active channels */

	/* Locate the channel and update the current channel information.  We 
	   allow a special channel-type indicator of CHANNEL_NONE to specify the 
	   selection of not-yet-activated channels.  Since it's possible to have 
	   per-channel packet sizes we also update the overall packet size 
	   value */
	channelInfoPtr = findChannelByChannelNo( sessionInfoPtr, channelNo );
	if( channelInfoPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	if( !isActiveChannel( channelInfoPtr ) && channelType != CHANNEL_NONE )
		return( CRYPT_ERROR_NOTINITED );
	switch( channelType )
		{
		case CHANNEL_READ:
			sshInfo->currReadChannel = channelInfoPtr->channelID;
			break;

		case CHANNEL_WRITE:
			sshInfo->currWriteChannel = channelInfoPtr->channelID;
			break;

		case CHANNEL_BOTH:
		case CHANNEL_NONE:
			sshInfo->currReadChannel = \
				sshInfo->currWriteChannel = channelInfoPtr->channelID;
			break;

		default:
			retIntError();
		}
	sessionInfoPtr->maxPacketSize = channelInfoPtr->maxPacketSize;

	return( CRYPT_OK );
	}

/* Add/create/delete a channel */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int addChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				IN const long channelNo,
				IN_LENGTH_MIN( 1024 ) const int maxPacketSize, 
				IN_BUFFER( typeLen ) const void *type,
				IN_LENGTH_SHORT const int typeLen, 
				IN_BUFFER_OPT( arg1Len ) const void *arg1, 
				IN_LENGTH_SHORT const int arg1Len )
	{
	ATTRIBUTE_LIST *attributeListPtr;
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	SSH_CHANNEL_INFO channelInfo;
	int channelCount, iterationCount, status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( type, typeLen ) );
	assert( ( arg1 == NULL && arg1Len == 0 ) || 
			isReadPtr( arg1, arg1Len ) );

	REQUIRES( channelNo >= 0 && channelNo <= LONG_MAX );
	REQUIRES( maxPacketSize >= 1024 && maxPacketSize < MAX_INTLENGTH );
	REQUIRES( typeLen > 0 && typeLen < MAX_INTLENGTH_SHORT );
	REQUIRES( ( arg1 == NULL && arg1Len == 0 ) || \
			  ( arg1 != NULL && \
				arg1Len > 0 && arg1Len < MAX_INTLENGTH_SHORT ) );

	/* Make sure that this channel doesn't already exist */
	if( findChannelByChannelNo( sessionInfoPtr, channelNo ) != NULL )
		{
		retExt( CRYPT_ERROR_DUPLICATE,
				( CRYPT_ERROR_DUPLICATE, SESSION_ERRINFO, 
				  "Attempt to add duplicate channel %lX", channelNo ) );
		}

	/* SSH channels are allocated unique IDs for tracking by cryptlib,
	   since (at least in theory) the SSH-level channel IDs may repeat.
	   If the initial (not-yet-initialised) channel ID matches the
	   UNUSED_CHANNEL_ID magic value we initialise it to one past that
	   value */
	if( sshInfo->channelIndex <= UNUSED_CHANNEL_ID )
		sshInfo->channelIndex = UNUSED_CHANNEL_ID + 1;

	/* Make sure that we haven't exceeded the maximum number of channels */
	for( channelCount = 0, \
			attributeListPtr = sessionInfoPtr->attributeList, \
			iterationCount = 0;
		 attributeListPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 attributeListPtr = attributeListPtr->next, iterationCount++ )
		{
		if( attributeListPtr->attributeID == CRYPT_SESSINFO_SSH_CHANNEL )
			channelCount++;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );
	if( channelCount > SSH_MAX_CHANNELS )
		{
		retExt( CRYPT_ERROR_OVERFLOW,
				( CRYPT_ERROR_OVERFLOW, SESSION_ERRINFO, 
				  "Maximum number (%d) of SSH channels reached",
				  SSH_MAX_CHANNELS ) );
		}

	/* Initialise the information for the new channel and create it */
	memset( &channelInfo, 0, sizeof( SSH_CHANNEL_INFO ) );
	channelInfo.channelID = sshInfo->channelIndex++;
	channelInfo.readChannelNo = channelInfo.writeChannelNo = channelNo;
	channelInfo.maxPacketSize = maxPacketSize;
	status = attributeCopyParams( channelInfo.type, CRYPT_MAX_TEXTSIZE, 
								  &channelInfo.typeLen, type, typeLen );
	if( cryptStatusOK( status ) && arg1 != NULL )
		status = attributeCopyParams( channelInfo.arg1, CRYPT_MAX_TEXTSIZE, 
									  &channelInfo.arg1Len, arg1, arg1Len );
	if( cryptStatusOK( status ) )
		{
		status = addSessionInfoComposite( &sessionInfoPtr->attributeList,
										  CRYPT_SESSINFO_SSH_CHANNEL, 
										  accessFunction, &channelInfo, 
										  sizeof( SSH_CHANNEL_INFO ),
										  ATTR_FLAG_MULTIVALUED | \
										  ATTR_FLAG_COMPOSITE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Select the newly-created channel.  We have to select it using the
	   special-case indicator of CHANNEL_NONE since we can't normally
	   select an inactive channel */
	return( selectChannel( sessionInfoPtr, channelNo, CHANNEL_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createChannel( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	int iterationCount;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Find an unused channel number.  Since the peer can request the
	   creation of arbitrary-numbered channels we have to be careful to
	   ensure that we don't clash with any existing peer-requested channel
	   numbers when we create our own one */
	for( iterationCount = 0;
		 findChannelByChannelNo( sessionInfoPtr, \
								 sshInfo->nextChannelNo ) != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		/* This channel number is already in use, move on to the next one */
		sshInfo->nextChannelNo++;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* Create a channel with the new channel number */
	return( addChannel( sessionInfoPtr, sshInfo->nextChannelNo++,
						sessionInfoPtr->sendBufSize - EXTRA_PACKET_SIZE,
						"session", 7, NULL, 0 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteChannel( INOUT SESSION_INFO *sessionInfoPtr, 
				   IN const long channelNo,
				   IN_ENUM( CHANNEL ) const CHANNEL_TYPE channelType,
				   const BOOLEAN deleteLastChannel )
	{
	SSH_INFO *sshInfo = sessionInfoPtr->sessionSSH;
	SSH_CHANNEL_INFO *channelInfoPtr;
	ATTRIBUTE_LIST *attributeListPtr;
	int channelID;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( channelNo >= 0 && channelNo <= LONG_MAX );
	REQUIRES( channelType > CHANNEL_NONE && channelType < CHANNEL_LAST );

	/* Locate the channel information */
	attributeListPtr = findChannelAttr( sessionInfoPtr, channelNo );
	if( attributeListPtr == NULL )
		return( isChannelActive( sessionInfoPtr, UNUSED_CHANNEL_ID ) ? \
				CRYPT_ERROR_NOTFOUND : OK_SPECIAL );
	channelInfoPtr = attributeListPtr->value;
	channelID = channelInfoPtr->channelID;

	/* If we can't delete the last remaining channel (it has to be done
	   explicitly via a session close) and there are no active channels
	   left besides the current one then we can't do anything */
	if( !deleteLastChannel && \
		!isChannelActive( sessionInfoPtr, channelID ) )
		return( CRYPT_ERROR_PERMISSION );

	/* Delete the channel entry.  If we're only closing the write side we
	   mark the channel as closed for write but leave the overall channel
	   open */
	if( channelType == CHANNEL_WRITE )
		{
		REQUIRES( !( channelInfoPtr->flags & CHANNEL_FLAG_WRITECLOSED ) );
		channelInfoPtr->flags |= CHANNEL_FLAG_WRITECLOSED;
		if( channelID == sshInfo->currWriteChannel )
			sshInfo->currWriteChannel = UNUSED_CHANNEL_ID;
		return( isChannelActive( sessionInfoPtr, \
								 channelInfoPtr->channelID ) ? \
				CRYPT_OK : OK_SPECIAL );
		}
	deleteSessionInfo( &sessionInfoPtr->attributeList,
					   &sessionInfoPtr->attributeListCurrent,
					   attributeListPtr );

	/* If we've deleted the current channel, select a null channel until a
	   new one is created/selected */
	if( channelID == sshInfo->currReadChannel )
		sshInfo->currReadChannel = UNUSED_CHANNEL_ID;
	if( channelID == sshInfo->currWriteChannel )
		sshInfo->currWriteChannel = UNUSED_CHANNEL_ID;

	/* We've deleted an open channel, check if there are any channels left
	   and if not let the caller know */
	return( isChannelActive( sessionInfoPtr, UNUSED_CHANNEL_ID ) ? \
			CRYPT_OK : OK_SPECIAL );
	}

#if 0

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deleteChannelByAddr( INOUT SESSION_INFO *sessionInfoPtr, 
						 IN_BUFFER( addrInfoLen ) const char *addrInfo,
						 IN_LENGTH_SHORT const int addrInfoLen )
	{
	const SSH_CHANNEL_INFO *channelInfoPtr;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( addrInfo, addrInfoLen ) );

	REQUIRES( addrInfoLen > 0 && addrInfoLen < MAX_INTLENGTH_SHORT );

	channelInfoPtr = findChannelByAddr( sessionInfoPtr, addrInfo,
										addrInfoLen );
	if( channelInfoPtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* We've found the entry that it corresponds to, clear it.  This doesn't
	   actually delete the entire channel but merely deletes the forwarding.
	   See the note in ssh2_msg.c for why this is currently unused */
	memset( channelInfoPtr->arg1, 0, CRYPT_MAX_TEXTSIZE );
	channelInfoPtr->arg1Len = 0;
	return( CRYPT_OK );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*							Enqueue/Send Channel Messages					*
*																			*
****************************************************************************/

/* Enqueue a response to a request, to be sent at the next available
   opportunity.  This is required because we may be in the middle of
   assembling or sending a data packet when we need to send the response
   so the response has to be deferred until after the data packet has been
   completed and sent */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int enqueueResponse( INOUT SESSION_INFO *sessionInfoPtr, 
					 IN_RANGE( 1, 255 ) const int type,
					 IN_RANGE( 0, 4 ) const int noParams, 
					 IN const long channelNo,
					 const int param1, const int param2, const int param3 )
	{
	SSH_RESPONSE_INFO *respPtr = &sessionInfoPtr->sessionSSH->response;
	STREAM stream;
	int status = CRYPT_OK;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( type > 0 && type <= 0xFF );
	REQUIRES( noParams >= 0 && noParams <= 4 );
			  /* channelNo is the first parameter */
	REQUIRES( channelNo >= 0 && channelNo <= LONG_MAX );

	/* If there's already a response enqueued we can't enqueue another one
	   until it's been sent */
	REQUIRES( respPtr->type == 0 );

	respPtr->type = type;
	sMemOpen( &stream, respPtr->data, SSH_MAX_RESPONSESIZE );
	if( noParams > 0 )
		status = writeUint32( &stream, channelNo );
	if( noParams > 1 )
		status = writeUint32( &stream, param1 );
	if( noParams > 2 )
		status = writeUint32( &stream, param2 );
	if( noParams > 3 )
		status = writeUint32( &stream, param3 );
	ENSURES( cryptStatusOK( status ) );
	respPtr->dataLen = stell( &stream );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

/* Assemble a packet for and send a previously enqueued response.  This 
   potentially appends a control packet after the end of an existing payload
   packet so the buffer indicator used is sendBufSize rather than 
   maxPacketSize, since we may already have maxPacketSize bytes worth of
   payload data present.

   This can be called in one of two ways, if called as appendChannelData()
   then it's being piggybacked onto the end of existing data at a location
   specified by the 'offset' parameter and we assemble the packet at that 
   point but it'll be sent as part of the main packet send.  If called as 
   enqueueChannelData() or sendEnqueuedResponse() then we have the send 
   buffer to ourselves (offset == CRYPT_UNUSED) and can assemble and send it 
   immediately */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int encodeSendResponse( INOUT SESSION_INFO *sessionInfoPtr, 
							   IN_LENGTH_OPT const int offset,
							   OUT_OPT_LENGTH_Z int *responseSize )
	{
	SSH_RESPONSE_INFO *respPtr = &sessionInfoPtr->sessionSSH->response;
	STREAM stream;
	const BOOLEAN assembleOnly = ( offset != CRYPT_UNUSED ) ? TRUE : FALSE;
	BOOLEAN adjustedStartOffset = FALSE;
	int sendBufOffset = assembleOnly ? offset : sessionInfoPtr->sendBufPos;
	int encodedResponseSize = DUMMY_INIT, dummy, status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( responseSize == NULL || \
			isWritePtr( responseSize, sizeof( int ) ) );

	REQUIRES( ( offset == CRYPT_UNUSED && responseSize == NULL ) || \
			  ( offset >= 0 && offset < sessionInfoPtr->sendBufSize && \
			    responseSize != NULL ) );
	REQUIRES( sendBufOffset >= 0 && offset < sessionInfoPtr->sendBufSize );

	/* Make sure that there's room for at least two packets worth of 
	   wrappers plus a short control packet, needed to handle a control 
	   packet piggybacked onto the end of a data packet.  The reason for the
	   doubling of the IV count is because the minimum padding length 
	   requirement can result in an expansion by two encrypted blocks rather
	   than one */
	static_assert( EXTRA_PACKET_SIZE > \
						( 2 * ( SSH2_HEADER_SIZE + CRYPT_MAX_HASHSIZE + \
								( 2 * CRYPT_MAX_IVSIZE ) ) ) + 32,
				   "Buffer packet size" );

	/* Clear return value */
	if( responseSize != NULL )
		*responseSize = 0;

	/* If there's an incomplete packet in the process of being assembled in
	   the send buffer then we can't do anything.  If we're just assembling
	   a response to append to a completed packet then we know that the 
	   packet that's present is a complete one so we skip this check */
	if( assembleOnly && !sessionInfoPtr->partialWrite && \
		( sendBufOffset > sessionInfoPtr->sendBufStartOfs ) )
		return( CRYPT_OK );

	/* The send buffer is allocated to always allow the piggybacking of one
	   packet of control data */
	ENSURES( sendBufOffset + ( SSH2_HEADER_SIZE + 16 + respPtr->dataLen + \
							   CRYPT_MAX_HASHSIZE + CRYPT_MAX_IVSIZE ) <= \
			 sessionInfoPtr->sendBufSize );

	ENSURES( ( sendBufOffset <= sessionInfoPtr->sendBufStartOfs ) || \
			 ( sessionInfoPtr->partialWrite && \
			   sendBufOffset + ( SSH2_HEADER_SIZE + 16 + respPtr->dataLen + \
								 CRYPT_MAX_HASHSIZE + CRYPT_MAX_IVSIZE ) < \
			  sessionInfoPtr->sendBufSize ) );

	/* If we're in the data transfer phase and there's nothing in the send 
	   buffer, set the packet start offset to zero.  We have to do this 
	   because it's pre-adjusted to accomodate the header for a payload data 
	   packet, since we're assembling our own packet in the buffer there's 
	   no need for this additional header room */
	if( ( sessionInfoPtr->flags & SESSION_ISOPEN ) && \
		sendBufOffset == sessionInfoPtr->sendBufStartOfs )
		{
		sendBufOffset = 0;
		adjustedStartOffset = TRUE;
		}

	/* Assemble the response as a new packet at the end of any existing
	   data */
	REQUIRES( rangeCheckZ( sendBufOffset, 
						   sessionInfoPtr->sendBufSize - sendBufOffset,
						   sessionInfoPtr->sendBufSize ) );
	sMemOpen( &stream, sessionInfoPtr->sendBuffer + sendBufOffset,
			  sessionInfoPtr->sendBufSize - sendBufOffset );
	swrite( &stream, "\x00\x00\x00\x00\x00", SSH2_HEADER_SIZE );
	status = sputc( &stream, respPtr->type );
	if( respPtr->dataLen > 0 )
		{
		/* Some responses can consist purely of an ID byte */
		status = swrite( &stream, respPtr->data, respPtr->dataLen );
		}
	if( cryptStatusOK( status ) )
		status = wrapPacketSSH2( sessionInfoPtr, &stream, 0, FALSE, TRUE );
	if( cryptStatusOK( status ) )
		encodedResponseSize = stell( &stream );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* We've sent (or at least assembled) the response, clear the enqueued
	   data */
	memset( respPtr, 0, sizeof( SSH_RESPONSE_INFO ) );

	/* If we're only assembling the data and the caller is taking care of
	   sending the assembled packet, we're done */
	if( assembleOnly )
		{
		sMemDisconnect( &stream );
		*responseSize = encodedResponseSize;
		return( CRYPT_OK );
		}

	/* If we're still in the handshake phase (so this is part of the initial
	   session negotiation, for example a response to a global or channel
	   request) then we need to send the packet directly to avoid messing
	   up the send buffering */
	if( !( sessionInfoPtr->flags & SESSION_ISOPEN ) )
		{
		status = sendPacketSSH2( sessionInfoPtr, &stream, TRUE );
		sMemDisconnect( &stream );
		return( status );
		}

	/* Write the response, using the standard data-flush mechanism to try 
	   and get the data out.  We set the partial-write flag because what 
	   we've just added is pre-packaged data that doesn't have to go through 
	   the data-payload encoding process */
	sMemDisconnect( &stream );
	if( adjustedStartOffset )
		{
		/* We're in the data transfer phase, in which case the buffer 
		   position is offset into the send buffer to leave room for the 
		   packet header.  Since we're adding our own header we've started 
		   the packet at the start of the send buffer, i.e. with 
		   sendBufPos = 0 rather than sendBufPos = sendBufStartOffset, so we 
		   set the new position to the total size of the data written rather 
		   than adding it to the existing value */
		sessionInfoPtr->sendBufPos = encodedResponseSize;
		}
	else
		{
		assert( 0 );	/* When does this occur? */
		sessionInfoPtr->sendBufPos += encodedResponseSize;
		}
	sessionInfoPtr->partialWrite = TRUE;
	return( putSessionData( sessionInfoPtr, NULL, 0, &dummy ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sendEnqueuedResponse( INOUT SESSION_INFO *sessionInfoPtr )
	{
	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	return( encodeSendResponse( sessionInfoPtr, CRYPT_UNUSED, NULL ) );
	}

/* Enqueue channel control data ready to be sent, and try and send it if
   possible.  This is used to send window-adjust messages for the SSH
   performance handbrake by first enqueueing the window adjust and then,
   if the send buffer is available, sending it.  If it's not available
   then it'll be piggybacked onto the channel payload data later on when 
   it's being sent via appendChannelData() below */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int enqueueChannelData( INOUT SESSION_INFO *sessionInfoPtr, 
						IN_RANGE( 1, 255 ) const int type,
						IN const long channelNo, 
						const int param )
	{
	int status;

	assert( isReadPtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( type > 0 && type <= 0xFF );
	REQUIRES( channelNo >= 0 && channelNo <= LONG_MAX );

	status = enqueueResponse( sessionInfoPtr, type, 2, channelNo, param,
							  CRYPT_UNUSED, CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );
	return( encodeSendResponse( sessionInfoPtr, CRYPT_UNUSED, NULL ) );
	}

/* Append enqueued channel control data to existing channel payload data
   without trying to send it.  In this case the control data send is being 
   piggybacked on a payload data send and will be handled by the caller,
   with the control flow on the caller side being:

	preparepacketFunction:
		wrap channel data;
		if( enqueued control data present )
			appendChannelData();
	send wrapped channel data and wrapped control data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int appendChannelData( INOUT SESSION_INFO *sessionInfoPtr, 
					   IN_LENGTH_SHORT_Z const int offset )
	{
	int channelDataSize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( offset >= 0 && offset < sessionInfoPtr->sendBufSize );

	status = encodeSendResponse( sessionInfoPtr, offset, &channelDataSize );
	return( cryptStatusError( status ) ? status : channelDataSize );
	}
#endif /* USE_SSH */
