/****************************************************************************
*																			*
*						cryptlib Keyset Attribute Routines					*
*						Copyright Peter Gutmann 1995-2007					*
*																			*
****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "crypt.h"
#ifdef INC_ALL
  #include "keyset.h"
#else
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_KEYSETS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Exit after setting extended error information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitError( INOUT KEYSET_INFO *keysetInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus,
					  IN_ENUM( CRYPT_ERRTYPE ) const CRYPT_ERRTYPE_TYPE errorType, 
					  IN_ERROR const int status )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );
	REQUIRES( isEnumRange( errorType, CRYPT_ERRTYPE ) );
	REQUIRES( cryptStatusError( status ) );

	setErrorInfo( keysetInfoPtr, errorLocus, errorType );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorNotFound( INOUT KEYSET_INFO *keysetInfoPtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( keysetInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_ABSENT, 
					   CRYPT_ERROR_NOTFOUND ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorIncomplete( INOUT KEYSET_INFO *keysetInfoPtr,
								IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( keysetInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_PRESENT,
					   CRYPT_ERROR_INCOMPLETE ) );
	}

/****************************************************************************
*																			*
*								Get Attributes								*
*																			*
****************************************************************************/

/* Get a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getKeysetAttribute( INOUT KEYSET_INFO *keysetInfoPtr,
						OUT_INT_Z int *valuePtr, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( valuePtr, sizeof( int ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Clear return value */
	*valuePtr = 0;

	switch( attribute )
		{
		case CRYPT_ATTRIBUTE_ERRORTYPE:
			*valuePtr = keysetInfoPtr->errorType;
			return( CRYPT_OK );

		case CRYPT_ATTRIBUTE_ERRORLOCUS:
			*valuePtr = keysetInfoPtr->errorLocus;
			return( CRYPT_OK );
		}

	retIntError();
	}

/* Get a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getKeysetAttributeS( INOUT KEYSET_INFO *keysetInfoPtr,
						 INOUT MESSAGE_DATA *msgData, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( msgData, sizeof( MESSAGE_DATA ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_ATTRIBUTE_ERRORMESSAGE:
			{
#ifdef USE_ERRMSGS
			ERROR_INFO *errorInfo = &keysetInfoPtr->errorInfo;

			if( errorInfo->errorStringLength > 0 )
				{
				return( attributeCopy( msgData, errorInfo->errorString,
									   errorInfo->errorStringLength ) );
				}
#endif /* USE_ERRMSGS */
			return( exitErrorNotFound( keysetInfoPtr,
									   CRYPT_ATTRIBUTE_ERRORMESSAGE ) );
			}

		case CRYPT_IATTRIBUTE_CONFIGDATA:
		case CRYPT_IATTRIBUTE_USERINDEX:
		case CRYPT_IATTRIBUTE_USERINFO:
		case CRYPT_IATTRIBUTE_TRUSTEDCERT:
		case CRYPT_IATTRIBUTE_TRUSTEDCERT_NEXT:
			{
			const KEY_GETSPECIALITEM_FUNCTION getSpecialItemFunction = \
						( KEY_GETSPECIALITEM_FUNCTION ) \
						FNPTR_GET( keysetInfoPtr->getSpecialItemFunction );

			REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
					  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
			REQUIRES( getSpecialItemFunction != NULL );

			/* It's encoded cryptlib-specific data, fetch it from to the
			   keyset */
			return( getSpecialItemFunction( keysetInfoPtr, attribute, 
											msgData->data, msgData->length, 
											&msgData->length ) );
			}

		}

	retIntError();
	}

/****************************************************************************
*																			*
*								Set Attributes								*
*																			*
****************************************************************************/

/* Set a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setKeysetAttribute( INOUT KEYSET_INFO *keysetInfoPtr,
						IN_INT_Z const int value, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const KEY_SETSPECIALITEM_FUNCTION setSpecialItemFunction = \
				( KEY_SETSPECIALITEM_FUNCTION ) \
				FNPTR_GET( keysetInfoPtr->setSpecialItemFunction );

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( isIntegerRange( value ) );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );
	REQUIRES( setSpecialItemFunction != NULL );

	switch( attribute )
		{
		case CRYPT_IATTRIBUTE_HWSTORAGE:
			REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
					  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );

			return( setSpecialItemFunction( keysetInfoPtr, 
											CRYPT_IATTRIBUTE_HWSTORAGE, 
											&value, sizeof( int ) ) );
		}

	retIntError();
	}

/* Set a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setKeysetAttributeS( INOUT KEYSET_INFO *keysetInfoPtr,
						 IN_BUFFER( dataLength ) const void *data,
						 IN_LENGTH const int dataLength,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const KEY_SETSPECIALITEM_FUNCTION setSpecialItemFunction = \
				( KEY_SETSPECIALITEM_FUNCTION ) \
				FNPTR_GET( keysetInfoPtr->setSpecialItemFunction );
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtrDynamic( data, dataLength ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( isIntegerRangeNZ( dataLength ) );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );
	REQUIRES( setSpecialItemFunction != NULL );

	switch( attribute )
		{
		case CRYPT_KEYINFO_QUERY:
		case CRYPT_KEYINFO_QUERY_REQUESTS:
			{
			const KEY_ISBUSY_FUNCTION isBusyFunction = \
						( KEY_ISBUSY_FUNCTION ) \
						FNPTR_GET( keysetInfoPtr->isBusyFunction );

			REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
			REQUIRES( isBusyFunction != NULL );

			/* If we're in the middle of an existing query the user needs to
			   cancel it before starting another one */
			if( isBusyFunction( keysetInfoPtr ) && \
				( dataLength != 6 || strCompare( data, "cancel", 6 ) ) )
				return( exitErrorIncomplete( keysetInfoPtr, attribute ) );

			/* Send the query to the data source */
			return( setSpecialItemFunction( keysetInfoPtr, attribute, data, 
											dataLength ) );
			}

		case CRYPT_IATTRIBUTE_CONFIGDATA:
		case CRYPT_IATTRIBUTE_USERINDEX:
		case CRYPT_IATTRIBUTE_USERID:
		case CRYPT_IATTRIBUTE_USERINFO:
			REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
					  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );

			/* It's encoded cryptlib-specific data, pass it through to the
			   keyset */
			status = setSpecialItemFunction( keysetInfoPtr, attribute, 
											 data, dataLength );
			if( cryptStatusOK( status ) && \
				attribute != CRYPT_IATTRIBUTE_USERID )
				{
				/* The update succeeded, remember that the data in the keyset
				   has changed unless it's a userID that just modifies 
				   existing data */
				SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY );
				CLEAR_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_EMPTY );
				}
			return( status );
		}

	retIntError();
	}
#endif /* USE_KEYSETS */
