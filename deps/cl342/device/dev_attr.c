/****************************************************************************
*																			*
*					cryptlib Crypto Device Attribute Routines				*
*						Copyright Peter Gutmann 1997-2007					*
*																			*
****************************************************************************/

#include "crypt.h"
#ifdef INC_ALL
  #include "capabil.h"
  #include "device.h"
#else
  #include "device/capabil.h"
  #include "device/device.h"
#endif /* Compiler-specific includes */

/* When we get random data from a device we run the (practical) FIPS 140 
   tests over the output to make sure that it's really random, at least as 
   far as the tests can tell us.  If the data fails the test we get more and 
   try again.  The following value defines how many times we retry before 
   giving up.  In test runs, a count of 2 failures is reached every ~50,000 
   iterations, 5 is never reached (in fact with 1M tests, 3 is never 
   reached) */

#define NO_ENTROPY_FAILURES	5

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Exit after setting extended error information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitError( INOUT DEVICE_INFO *deviceInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus,
					  IN_ENUM( CRYPT_ERRTYPE ) const CRYPT_ERRTYPE_TYPE errorType, 
					  IN_ERROR const int status )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );
	REQUIRES( errorType > CRYPT_ERRTYPE_NONE && \
			  errorType < CRYPT_ERRTYPE_LAST );
	REQUIRES( cryptStatusError( status ) );

	setErrorInfo( deviceInfoPtr, errorLocus, errorType );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorInited( INOUT DEVICE_INFO *deviceInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( deviceInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_PRESENT,
					   CRYPT_ERROR_INITED ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorNotFound( INOUT DEVICE_INFO *deviceInfoPtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( deviceInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_ABSENT,
					   CRYPT_ERROR_NOTFOUND ) );
	}

/****************************************************************************
*																			*
*							Read a Random-data Attribute					*
*																			*
****************************************************************************/

/* Get a random data block with FIPS 140 checking */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getRandomChecked( INOUT DEVICE_INFO *deviceInfoPtr, 
							 OUT_BUFFER_FIXED( length ) void *data,
							 IN_LENGTH_SHORT const int length,
							 INOUT_OPT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	int i;

	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( data, length ) );
	assert( messageExtInfo == NULL || \
			( isWritePtr( messageExtInfo, \
						  sizeof( MESSAGE_FUNCTION_EXTINFO ) ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* Get random data from the device and check it using the FIPS 140 
	   tests.  If it's less than 64 bits then we let it pass since the 
	   sample size is too small to be useful, samples this small are only 
	   ever drawn from the generator for use as padding with crypto keys 
	   that are always >= 64 bits which will themselves have been checked
	   when they were read, so a problem with the generator will be 
	   detected even if we don't check small samples */
	for( i = 0; i < NO_ENTROPY_FAILURES; i++ )
		{
		int status;

		status = deviceInfoPtr->getRandomFunction( deviceInfoPtr, 
										data, length, messageExtInfo );
		if( cryptStatusOK( status ) && \
			( length < 8 || checkEntropy( data, length ) ) )
			return( CRYPT_OK );
		}

	/* We couldn't get anything that passed the FIPS 140 tests, we can't
	   go any further */
	zeroise( data, length );
	DEBUG_DIAG(( "Random data didn't pass FIPS 140 tests after %d "
				 "iterations", NO_ENTROPY_FAILURES ));
	assert( DEBUG_WARN );
	return( CRYPT_ERROR_RANDOM );
	}

/* Get a random data block with no zero bytes, for PKCS #1 padding */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getRandomNonzero( INOUT DEVICE_INFO *deviceInfoPtr, 
							 OUT_BUFFER_FIXED( length ) void *data,
							 IN_LENGTH_SHORT const int length )
	{
	BYTE randomBuffer[ 128 + 8 ], *outBuffer = data;
	int count, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( data, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* The extraction of data is a little complex because we don't know how 
	   much data we'll need (as a rule of thumb it'll be size + ( size / 256 ) 
	   bytes, but in a worst-case situation we could need to draw out 
	   megabytes of data), so we copy out 128 bytes worth at a time (a 
	   typical value for a 1K bit key) and keep going until we've filled the 
	   output requirements */
	for( count = 0, iterationCount = 0;
		 count < length && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		int i;

		/* Copy as much as we can from the randomness pool.  We don't allow 
		   the device to be unlocked during this process since we may need
		   it again for further reads */
		status = getRandomChecked( deviceInfoPtr, randomBuffer, 128, NULL );
		if( cryptStatusError( status ) )
			{
			zeroise( data, length );
			return( status );
			}
		for( i = 0; count < length && i < 128; i++ )
			{
			if( randomBuffer[ i ] != 0 )
				outBuffer[ count++ ] = randomBuffer[ i ];
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	FORALL( i, 0, length, \
			outBuffer[ i ] != 0 );
	zeroise( randomBuffer, 128 );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Get Attributes								*
*																			*
****************************************************************************/

/* Get a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getDeviceAttribute( INOUT DEVICE_INFO *deviceInfoPtr,
						OUT_INT_Z int *valuePtr, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						INOUT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( valuePtr, sizeof( int ) ) );
	assert( isWritePtr( messageExtInfo, \
			sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_ATTRIBUTE_ERRORTYPE:
			*valuePtr = deviceInfoPtr->errorType;
			return( CRYPT_OK );

		case CRYPT_ATTRIBUTE_ERRORLOCUS:
			*valuePtr = deviceInfoPtr->errorLocus;
			return( CRYPT_OK );

		case CRYPT_DEVINFO_LOGGEDIN:
			if( deviceInfoPtr->flags & DEVICE_REMOVABLE )
				{
				int status;

				/* If it's a removable device the user could implicitly log 
				   out by removing it so we have to perform an explicit 
				   check to see whether it's still there */
				status = deviceInfoPtr->controlFunction( deviceInfoPtr,
									CRYPT_DEVINFO_LOGGEDIN, NULL, 0, NULL );
				if( cryptStatusError( status ) )
					return( status );
				assert( !isMessageObjectUnlocked( messageExtInfo ) );
				}
			*valuePtr = ( deviceInfoPtr->flags & DEVICE_LOGGEDIN ) ? \
						TRUE : FALSE;
			return( CRYPT_OK );
		}

	retIntError();
	}

/* Get a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int getDeviceAttributeS( INOUT DEVICE_INFO *deviceInfoPtr,
						 INOUT MESSAGE_DATA *msgData, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( msgData, sizeof( MESSAGE_DATA ) ) );
	assert( isWritePtr( messageExtInfo, \
			sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_ATTRIBUTE_ERRORMESSAGE:
			{
#ifdef USE_ERRMSGS
			ERROR_INFO *errorInfo;

			switch( deviceInfoPtr->type )
				{
				case CRYPT_DEVICE_PKCS11:
					errorInfo = &deviceInfoPtr->devicePKCS11->errorInfo;
					break;

				case CRYPT_DEVICE_CRYPTOAPI:
					errorInfo = &deviceInfoPtr->deviceCryptoAPI->errorInfo;
					break;

				default:
					return( exitErrorNotFound( deviceInfoPtr,
										CRYPT_ATTRIBUTE_ERRORMESSAGE ) );
				}

			if( errorInfo->errorStringLength > 0 )
				{
				return( attributeCopy( msgData, errorInfo->errorString,
									   errorInfo->errorStringLength ) );
				}
#endif /* USE_ERRMSGS */
			return( exitErrorNotFound( deviceInfoPtr,
									   CRYPT_ATTRIBUTE_ERRORMESSAGE ) );
			}

		case CRYPT_DEVINFO_LABEL:
			if( deviceInfoPtr->label == NULL )
				return( exitErrorNotFound( deviceInfoPtr,
										   CRYPT_DEVINFO_LABEL ) );
			return( attributeCopy( msgData, deviceInfoPtr->label,
								   strlen( deviceInfoPtr->label ) ) );

		case CRYPT_IATTRIBUTE_RANDOM:
			if( deviceInfoPtr->getRandomFunction == NULL )
				return( CRYPT_ERROR_RANDOM );
			return( getRandomChecked( deviceInfoPtr, msgData->data,
									  msgData->length, messageExtInfo ) );

		case CRYPT_IATTRIBUTE_RANDOM_NZ:
			if( deviceInfoPtr->getRandomFunction == NULL )
				return( CRYPT_ERROR_RANDOM );
			return( getRandomNonzero( deviceInfoPtr, msgData->data,
									  msgData->length ) );

		case CRYPT_IATTRIBUTE_RANDOM_NONCE:
			if( deviceInfoPtr->getRandomFunction == NULL )
				return( CRYPT_ERROR_RANDOM );

			return( deviceInfoPtr->controlFunction( deviceInfoPtr,
													CRYPT_IATTRIBUTE_RANDOM_NONCE,
													msgData->data,
													msgData->length,
													messageExtInfo ) );

		case CRYPT_IATTRIBUTE_TIME:
			{
			time_t *timePtr = msgData->data;
			int status;

			/* If the device doesn't contain a time source we can't provide 
			   time information */
			if( !( deviceInfoPtr->flags & DEVICE_TIME ) )
				return( CRYPT_ERROR_NOTAVAIL );

			/* Get the time from the device */
			status = deviceInfoPtr->controlFunction( deviceInfoPtr,
													 CRYPT_IATTRIBUTE_TIME,
													 msgData->data,
													 msgData->length, NULL );
			if( cryptStatusError( status ) )
				return( status );

			/* Perform a sanity check on the returned value, if it's too far 
			   out then we don't trust it */
			if( *timePtr <= MIN_TIME_VALUE )
				{
				*timePtr = 0;
				return( CRYPT_ERROR_NOTAVAIL );
				}

			return( CRYPT_OK );
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int setDeviceAttribute( INOUT DEVICE_INFO *deviceInfoPtr,
						IN_INT_Z const int value, 
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( messageExtInfo, \
			sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	REQUIRES( value >= 0 && value < MAX_INTLENGTH );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Send the control information to the device */
	return( deviceInfoPtr->controlFunction( deviceInfoPtr, attribute, NULL,
											value, messageExtInfo ) );
	}

/* Set a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int setDeviceAttributeS( INOUT DEVICE_INFO *deviceInfoPtr,
						 IN_BUFFER( dataLength ) const void *data,
						 IN_LENGTH const int dataLength,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						 MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	int status;

	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( messageExtInfo, \
			sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* If the user is logging on to the device we have to perform a bit of 
	   extra processing */
	if( attribute == CRYPT_DEVINFO_AUTHENT_USER || \
		attribute == CRYPT_DEVINFO_AUTHENT_SUPERVISOR )
		{
		/* Make sure that a login is actually required for the device */
		if( !( deviceInfoPtr->flags & DEVICE_NEEDSLOGIN ) )
			return( exitErrorInited( deviceInfoPtr, attribute ) );

		/* Send the logon information to the device */
		status = deviceInfoPtr->controlFunction( deviceInfoPtr, attribute,
												 ( void * ) data, dataLength,
												 messageExtInfo );
		if( cryptStatusError( status ) )
			return( status );
		assert( !isMessageObjectUnlocked( messageExtInfo ) );

		/* The user has logged in, if the token has a hardware RNG grab 256 
		   bits of entropy and send it to the system device.  Since we have 
		   no idea how good this entropy is (it could be just a DES-based 
		   PRNG using a static key or even an LFSR, which some smart cards 
		   use) we don't set any entropy quality indication */
		if( deviceInfoPtr->getRandomFunction != NULL )
			{
			BYTE buffer[ 32 + 8 ];

			status = deviceInfoPtr->getRandomFunction( deviceInfoPtr,
													   buffer, 32, NULL );
			if( cryptStatusOK( status ) )
				{
				MESSAGE_DATA msgData2;

				setMessageData( &msgData2, buffer, 32 );
				( void ) krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
										  IMESSAGE_SETATTRIBUTE_S, &msgData2, 
										  CRYPT_IATTRIBUTE_ENTROPY );
				}
			zeroise( buffer, 32 );
			}

		return( CRYPT_OK );
		}

	/* Send the control information to the device */
	return( deviceInfoPtr->controlFunction( deviceInfoPtr, attribute,
											( void * ) data, dataLength, 
											messageExtInfo ) );
	}
