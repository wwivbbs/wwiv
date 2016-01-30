/****************************************************************************
*																			*
*						 cryptlib Crypto Device Routines					*
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

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Process a crypto mechanism message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
static int processMechanismMessage( INOUT DEVICE_INFO *deviceInfoPtr,
									IN_MESSAGE const MESSAGE_TYPE action,
									IN_ENUM( MECHANISM ) \
										const MECHANISM_TYPE mechanism,
									INOUT void *mechanismInfo,
									INOUT MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	CRYPT_DEVICE localCryptDevice = deviceInfoPtr->objectHandle;
	MECHANISM_FUNCTION mechanismFunction = NULL;
	int refCount, i, status;

	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( messageExtInfo, sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );

	REQUIRES( isMechanismActionMessage( action ) );
	REQUIRES( mechanism > MECHANISM_NONE && mechanism < MECHANISM_LAST );
	REQUIRES( mechanismInfo != NULL );

	/* Find the function to handle this action and mechanism */
	if( deviceInfoPtr->mechanismFunctions != NULL )
		{
		for( i = 0;
			 i < deviceInfoPtr->mechanismFunctionCount && \
				deviceInfoPtr->mechanismFunctions[ i ].action != MESSAGE_NONE && \
				i < FAILSAFE_ITERATIONS_LARGE; 
			 i++ )
			{
			if( deviceInfoPtr->mechanismFunctions[ i ].action == action && \
				deviceInfoPtr->mechanismFunctions[ i ].mechanism == mechanism )
				{
				mechanismFunction = \
					deviceInfoPtr->mechanismFunctions[ i ].function;
				break;
				}
			}
		ENSURES( i < FAILSAFE_ITERATIONS_LARGE );
		ENSURES( i < deviceInfoPtr->mechanismFunctionCount );
		}
	if( mechanismFunction == NULL && \
		localCryptDevice != SYSTEM_OBJECT_HANDLE )
		{
		/* This isn't the system object, fall back to the system object and 
		   see if it can handle the mechanism.  We do it this way rather 
		   than sending the message through the kernel a second time because 
		   all the kernel checking of message parameters has already been 
		   done, this saves the overhead of a second, redundant kernel pass.  
		   This code was only ever used with Fortezza devices, with PKCS #11 
		   devices the support for various mechanisms is too patchy to allow 
		   us to rely on it so we always use system mechanisms which we know 
		   will get it right.
		   
		   Because it should never be used in normal use, we throw an 
		   exception if we get here inadvertently.  If this doesn't stop 
		   execution then the krnlAcquireObject() will since it will refuse 
		   to allocate the system object */
		assert( INTERNAL_ERROR );
		setMessageObjectUnlocked( messageExtInfo );
		status = krnlSuspendObject( deviceInfoPtr->objectHandle, &refCount );
		ENSURES( cryptStatusOK( status ) );
		localCryptDevice = SYSTEM_OBJECT_HANDLE;
		status = krnlAcquireObject( SYSTEM_OBJECT_HANDLE, /* Will always fail */
									OBJECT_TYPE_DEVICE,
									( void ** ) &deviceInfoPtr,
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusError( status ) )
			return( status );
		REQUIRES( deviceInfoPtr->mechanismFunctions != NULL );
		for( i = 0; 
			 i < deviceInfoPtr->mechanismFunctionCount && \
				deviceInfoPtr->mechanismFunctions[ i ].action != MESSAGE_NONE && \
				i < FAILSAFE_ITERATIONS_LARGE; 
			 i++ )
			{
			if( deviceInfoPtr->mechanismFunctions[ i ].action == action && \
				deviceInfoPtr->mechanismFunctions[ i ].mechanism == mechanism )
				{
				mechanismFunction = \
						deviceInfoPtr->mechanismFunctions[ i ].function;
				break;
				}
			}
		ENSURES( i < FAILSAFE_ITERATIONS_LARGE );
		ENSURES( i < deviceInfoPtr->mechanismFunctionCount );
		}
	if( mechanismFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* If the message has been sent to the system object, unlock it to allow 
	   it to be used by others and dispatch the message */
	if( localCryptDevice == SYSTEM_OBJECT_HANDLE )
		{
		setMessageObjectUnlocked( messageExtInfo );
		status = krnlSuspendObject( SYSTEM_OBJECT_HANDLE, &refCount );
		ENSURES( cryptStatusOK( status ) );
		assert( ( action == MESSAGE_DEV_DERIVE && 
				  mechanism >= MECHANISM_DERIVE_PKCS5 && \
				  mechanism <= MECHANISM_DERIVE_PGP && \
				  refCount <= 2 ) || \
				refCount == 1 );
				/* The system object can send itself a derive mechanism 
				   message during the self-test */
		return( mechanismFunction( NULL, mechanismInfo ) );
		}

	/* Send the message to the device */
	return( mechanismFunction( deviceInfoPtr, mechanismInfo ) );
	}

/****************************************************************************
*																			*
*								Device API Functions						*
*																			*
****************************************************************************/

/* Default object creation routines used when the device code doesn't set
   anything up */

static const CREATEOBJECT_FUNCTION_INFO defaultCreateFunctions[] = {
	{ OBJECT_TYPE_CONTEXT, createContext },
	{ OBJECT_TYPE_NONE, NULL }
	};

/* Handle a message sent to a device object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int deviceMessageFunction( INOUT TYPECAST( MESSAGE_FUNCTION_EXTINFO * ) \
									void *objectInfoPtr,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  void *messageDataPtr,
								  IN_INT_SHORT_Z const int messageValue )
	{
	MESSAGE_FUNCTION_EXTINFO *messageExtInfo = \
						( MESSAGE_FUNCTION_EXTINFO * ) objectInfoPtr;
	DEVICE_INFO *deviceInfoPtr = \
						( DEVICE_INFO * ) messageExtInfo->objectInfoPtr;
	int status;

	assert( isWritePtr( objectInfoPtr, sizeof( MESSAGE_FUNCTION_EXTINFO ) ) );
	assert( isWritePtr( deviceInfoPtr, sizeof( DEVICE_INFO ) ) );

	REQUIRES( message > MESSAGE_NONE && message < MESSAGE_LAST );
	REQUIRES( messageValue >= 0 && messageValue < MAX_INTLENGTH_SHORT );

	/* Process the destroy object message */
	if( message == MESSAGE_DESTROY )
		{
		/* Shut down the device if required */
		if( deviceInfoPtr->flags & DEVICE_ACTIVE && \
			deviceInfoPtr->shutdownFunction != NULL )
			deviceInfoPtr->shutdownFunction( deviceInfoPtr );

		return( CRYPT_OK );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		REQUIRES( message == MESSAGE_GETATTRIBUTE || \
				  message == MESSAGE_GETATTRIBUTE_S || \
				  message == MESSAGE_SETATTRIBUTE || \
				  message == MESSAGE_SETATTRIBUTE_S );
		REQUIRES( isAttribute( messageValue ) || \
				  isInternalAttribute( messageValue ) );

		if( message == MESSAGE_GETATTRIBUTE )
			return( getDeviceAttribute( deviceInfoPtr, 
										( int * ) messageDataPtr,
										messageValue, messageExtInfo ) );
		if( message == MESSAGE_GETATTRIBUTE_S )
			return( getDeviceAttributeS( deviceInfoPtr, 
										 ( MESSAGE_DATA * ) messageDataPtr,
										 messageValue, messageExtInfo ) );
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				return( CRYPT_OK );

			return( setDeviceAttribute( deviceInfoPtr, 
										 *( ( int * ) messageDataPtr ),
										 messageValue, messageExtInfo ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setDeviceAttributeS( deviceInfoPtr, msgData->data, 
										 msgData->length, messageValue,
										 messageExtInfo ) );
			}

		retIntError();
		}

	/* Process action messages */
	if( isMechanismActionMessage( message ) )
		{
		return( processMechanismMessage( deviceInfoPtr, message, 
										 messageValue, messageDataPtr, 
										 messageExtInfo ) );
		}

	/* Process messages that check a device */
	if( message == MESSAGE_CHECK )
		{
		/* The check for whether this device type can contain an object that
		   can perform the requested operation has already been performed by
		   the kernel so there's nothing further to do here */
		REQUIRES( ( messageValue == MESSAGE_CHECK_PKC_ENCRYPT_AVAIL || \
					messageValue == MESSAGE_CHECK_PKC_DECRYPT_AVAIL || \
					messageValue == MESSAGE_CHECK_PKC_SIGCHECK_AVAIL || \
					messageValue == MESSAGE_CHECK_PKC_SIGN_AVAIL ) && \
				  ( deviceInfoPtr->type == CRYPT_DEVICE_PKCS11 || \
					deviceInfoPtr->type == CRYPT_DEVICE_CRYPTOAPI || \
					deviceInfoPtr->type == CRYPT_DEVICE_HARDWARE ) );

		return( CRYPT_OK );
		}

	/* Process object-specific messages */
	if( message == MESSAGE_SELFTEST )
		{
		/* If the device doesn't have a self-test capability then there's 
		   not much that we can do */
		if( deviceInfoPtr->selftestFunction == NULL )
			return( CRYPT_OK );

		return( deviceInfoPtr->selftestFunction( deviceInfoPtr, 
												 messageExtInfo ) );
		}
	if( message == MESSAGE_KEY_GETKEY )
		{
		MESSAGE_KEYMGMT_INFO *getkeyInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( deviceInfoPtr->getItemFunction != NULL );

		/* Create a context via an object in the device */
		return( deviceInfoPtr->getItemFunction( deviceInfoPtr,
								&getkeyInfo->cryptHandle, messageValue,
								getkeyInfo->keyIDtype, getkeyInfo->keyID,
								getkeyInfo->keyIDlength, getkeyInfo->auxInfo,
								&getkeyInfo->auxInfoLength,
								getkeyInfo->flags ) );
		}
	if( message == MESSAGE_KEY_SETKEY )
		{
		MESSAGE_KEYMGMT_INFO *setkeyInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( deviceInfoPtr->setItemFunction != NULL );

		/* Update the device with the cert */
		return( deviceInfoPtr->setItemFunction( deviceInfoPtr,
												setkeyInfo->cryptHandle ) );
		}
	if( message == MESSAGE_KEY_DELETEKEY )
		{
		MESSAGE_KEYMGMT_INFO *deletekeyInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( deviceInfoPtr->deleteItemFunction != NULL );

		/* Delete an object in the device */
		return( deviceInfoPtr->deleteItemFunction( deviceInfoPtr,
						messageValue, deletekeyInfo->keyIDtype,
						deletekeyInfo->keyID, deletekeyInfo->keyIDlength ) );
		}
	if( message == MESSAGE_KEY_GETFIRSTCERT )
		{
		MESSAGE_KEYMGMT_INFO *getnextcertInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( deviceInfoPtr->getFirstItemFunction != NULL );
		REQUIRES( getnextcertInfo->auxInfoLength == sizeof( int ) );
		REQUIRES( messageValue == KEYMGMT_ITEM_PUBLICKEY );

		/* Fetch a cert in a cert chain from the device */
		return( deviceInfoPtr->getFirstItemFunction( deviceInfoPtr,
						&getnextcertInfo->cryptHandle, getnextcertInfo->auxInfo,
						getnextcertInfo->keyIDtype, getnextcertInfo->keyID,
						getnextcertInfo->keyIDlength, messageValue,
						getnextcertInfo->flags ) );
		}
	if( message == MESSAGE_KEY_GETNEXTCERT )
		{
		MESSAGE_KEYMGMT_INFO *getnextcertInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( deviceInfoPtr->getNextItemFunction != NULL );
		REQUIRES( getnextcertInfo->auxInfoLength == sizeof( int ) );

		/* Fetch a cert in a cert chain from the device */
		return( deviceInfoPtr->getNextItemFunction( deviceInfoPtr,
						&getnextcertInfo->cryptHandle, getnextcertInfo->auxInfo,
						getnextcertInfo->flags ) );
		}
	if( message == MESSAGE_DEV_QUERYCAPABILITY )
		{
		const void FAR_BSS *capabilityInfoPtr;
		CRYPT_QUERY_INFO *queryInfo = ( CRYPT_QUERY_INFO * ) messageDataPtr;

		/* Find the information for this algorithm and return the appropriate
		   information */
		capabilityInfoPtr = findCapabilityInfo( deviceInfoPtr->capabilityInfoList,
												messageValue );
		if( capabilityInfoPtr == NULL )
			return( CRYPT_ERROR_NOTAVAIL );
		getCapabilityInfo( queryInfo, capabilityInfoPtr );

		return( CRYPT_OK );
		}
	if( message == MESSAGE_DEV_CREATEOBJECT )
		{
		CRYPT_DEVICE iCryptDevice = deviceInfoPtr->objectHandle;
		CREATEOBJECT_FUNCTION createObjectFunction = NULL;
		const void *auxInfo = NULL;

		REQUIRES( messageValue > OBJECT_TYPE_NONE && \
				  messageValue < OBJECT_TYPE_LAST );

		/* If the device can't have objects created within it, complain */
		if( deviceInfoPtr->flags & DEVICE_READONLY )
			return( CRYPT_ERROR_PERMISSION );

		/* Find the function to handle this object */
		if( deviceInfoPtr->createObjectFunctions != NULL )
			{
			int i;
			
			for( i = 0;
				 i < deviceInfoPtr->createObjectFunctionCount && \
					deviceInfoPtr->createObjectFunctions[ i ].type != OBJECT_TYPE_NONE && \
					i < FAILSAFE_ITERATIONS_MED; i++ )
				{
				if( deviceInfoPtr->createObjectFunctions[ i ].type == messageValue )
					{
					createObjectFunction  = \
						deviceInfoPtr->createObjectFunctions[ i ].function;
					break;
					}
				}
			ENSURES( i < FAILSAFE_ITERATIONS_MED );
			ENSURES( i < deviceInfoPtr->createObjectFunctionCount );
			}
		if( createObjectFunction  == NULL )
			return( CRYPT_ERROR_NOTAVAIL );

		/* Get any auxiliary info that we may need to create the object */
		if( messageValue == OBJECT_TYPE_CONTEXT )
			auxInfo = deviceInfoPtr->capabilityInfoList;

		/* If the message has been sent to the system object, unlock it to
		   allow it to be used by others and dispatch the message.  This is
		   safe because the auxInfo for the system device is in a static,
		   read-only segment and persists even if the system device is
		   destroyed */
		if( deviceInfoPtr->objectHandle == SYSTEM_OBJECT_HANDLE )
			{
			int refCount;

			setMessageObjectUnlocked( messageExtInfo );
			status = krnlSuspendObject( SYSTEM_OBJECT_HANDLE, &refCount );
			ENSURES( cryptStatusOK( status ) );
			assert( refCount == 1 );
			status = createObjectFunction( messageDataPtr, auxInfo,
										   CREATEOBJECT_FLAG_NONE );
			}
		else
			{
			/* Create a dummy object, with all details handled by the device.
			   Unlike the system device we don't unlock the device info 
			   before we call the create object function because there may be
			   auxiliary info held in the device object that we need in order
			   to create the object.  This is OK since we're not tying up the
			   system device but only some auxiliary crypto device */
			status = createObjectFunction( messageDataPtr, auxInfo,
										   CREATEOBJECT_FLAG_DUMMY );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Make the newly-created object a dependent object of the device */
		return( krnlSendMessage( \
					( ( MESSAGE_CREATEOBJECT_INFO * ) messageDataPtr )->cryptHandle,
					IMESSAGE_SETDEPENDENT, ( MESSAGE_CAST ) &iCryptDevice,
					SETDEP_OPTION_INCREF ) );
		}
	if( message == MESSAGE_DEV_CREATEOBJECT_INDIRECT )
		{
		CRYPT_HANDLE iCryptHandle;
		const CRYPT_DEVICE iCryptDevice = deviceInfoPtr->objectHandle;
		int value, refCount;

		/* At the moment the only objects where can be created in this manner
		   are certificates */
		REQUIRES( messageValue == OBJECT_TYPE_CERTIFICATE );
		REQUIRES( deviceInfoPtr->objectHandle == SYSTEM_OBJECT_HANDLE );

		/* Unlock the system object to allow it to be used by others and
		   dispatch the message */
#ifdef USE_CERTIFICATES
		setMessageObjectUnlocked( messageExtInfo );
		status = krnlSuspendObject( SYSTEM_OBJECT_HANDLE, &refCount );
		ENSURES( cryptStatusOK( status ) );
		assert( refCount == 1 );
		status = createCertificateIndirect( messageDataPtr, NULL, 0 );
		if( cryptStatusError( status ) )
			return( status );
		iCryptHandle = \
			( ( MESSAGE_CREATEOBJECT_INFO * ) messageDataPtr )->cryptHandle;
#else
		return( CRYPT_ERROR_NOTAVAIL );
#endif /* USE_CERTIFICATES */

		/* Make the newly-created object a dependent object of the device.  
		   There's one special-case situation where we don't do this and 
		   that's when we're importing a certificate chain, which is a 
		   collection of individual certificate objects each of which have
		   already been made dependent on the device.  We could detect this
		   in one of two ways, either implicitly by reading the 
		   CRYPT_IATTRIBUTE_SUBTYPE attribute and assuming that if it's a
		   SUBTYPE_CERT_CERTCHAIN that the owner will have been set, or 
		   simply by reading the depending user object.  Explcitly checking
		   for ownership seems to be the best approach, although it may
		   pass through a case in which we've inadvertently set the owner
		   without intending do, in which case a backup check of the
		   subtype could be used to catch this */
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETDEPENDENT,
								  &value, OBJECT_TYPE_USER );
		if( cryptStatusOK( status ) )
			{
			/* The object is already owned, don't try and set an owner */
			return( CRYPT_OK );
			}
		return( krnlSendMessage( iCryptHandle, IMESSAGE_SETDEPENDENT, 
								 ( MESSAGE_CAST ) &iCryptDevice, 
								 SETDEP_OPTION_INCREF ) );
		}

	retIntError();
	}

/* Open a device.  This is a common function called to create both the
   internal system device object and general devices */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int openDevice( OUT_HANDLE_OPT CRYPT_DEVICE *iCryptDevice,
					   IN_HANDLE_OPT const CRYPT_USER iCryptOwner,
					   IN_ENUM_OPT( CRYPT_DEVICE ) \
							const CRYPT_DEVICE_TYPE deviceType,
					   IN_BUFFER_OPT( nameLength ) const char *name, 
					   IN_LENGTH_TEXT_Z const int nameLength,
					   OUT_PTR_OPT DEVICE_INFO **deviceInfoPtrPtr )
	{
	DEVICE_INFO *deviceInfoPtr;
	OBJECT_SUBTYPE subType;
	static const MAP_TABLE subtypeMapTbl[] = {
		{ CRYPT_DEVICE_NONE, SUBTYPE_DEV_SYSTEM },
		{ CRYPT_DEVICE_PKCS11, SUBTYPE_DEV_PKCS11 },
		{ CRYPT_DEVICE_CRYPTOAPI, SUBTYPE_DEV_CRYPTOAPI },
		{ CRYPT_DEVICE_HARDWARE, SUBTYPE_DEV_HARDWARE },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	int value, storageSize, status;

	assert( isWritePtr( iCryptDevice, sizeof( CRYPT_DEVICE ) ) );
	assert( ( name == NULL && nameLength == 0 ) || \
			( isReadPtr( name, nameLength ) ) );
	assert( isWritePtr( deviceInfoPtrPtr, sizeof( DEVICE_INFO * ) ) );

	REQUIRES( ( deviceType == CRYPT_DEVICE_NONE && \
				iCryptOwner == CRYPT_UNUSED ) || \
			  ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
				isHandleRangeValid( iCryptOwner ) );
	REQUIRES( deviceType >= CRYPT_DEVICE_NONE && \
			  deviceType < CRYPT_DEVICE_LAST );
	REQUIRES( ( name == NULL && nameLength == 0 ) || \
			  ( name != NULL && \
			    nameLength >= MIN_NAME_LENGTH && \
				nameLength <= CRYPT_MAX_TEXTSIZE ) );

	/* Clear return values */
	*iCryptDevice = CRYPT_ERROR;
	*deviceInfoPtrPtr = NULL;

	/* Fortezza support was removed as of cryptlib 3.4.0 so we add a special-
	   case check to make sure that we don't try and use it */
	if( deviceType == CRYPT_DEVICE_FORTEZZA )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Set up subtype-specific information */
	status = mapValue( deviceType, &value, subtypeMapTbl, 
					   FAILSAFE_ARRAYSIZE( subtypeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	subType = value;
	switch( deviceType )
		{
		case CRYPT_DEVICE_NONE:
			storageSize = sizeof( SYSTEMDEV_INFO );
			break;

		case CRYPT_DEVICE_PKCS11:
			storageSize = sizeof( PKCS11_INFO );
			break;

		case CRYPT_DEVICE_CRYPTOAPI:
			storageSize = sizeof( CRYPTOAPI_INFO );
			break;

		case CRYPT_DEVICE_HARDWARE:
			storageSize = sizeof( HARDWARE_INFO );
			break;

		default:
			retIntError();
		}

	/* Create the device object and connect it to the device */
	status = krnlCreateObject( iCryptDevice, ( void ** ) &deviceInfoPtr,
							   sizeof( DEVICE_INFO ) + storageSize,
							   OBJECT_TYPE_DEVICE, subType,
							   CREATEOBJECT_FLAG_NONE, iCryptOwner,
							   ACTION_PERM_NONE_ALL, deviceMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( deviceInfoPtr != NULL );
	*deviceInfoPtrPtr = deviceInfoPtr;
	deviceInfoPtr->objectHandle = *iCryptDevice;
	deviceInfoPtr->ownerHandle = iCryptOwner;
	deviceInfoPtr->type = deviceType;
	switch( deviceType )
		{
		case CRYPT_DEVICE_NONE:
			deviceInfoPtr->deviceSystem = \
							( SYSTEMDEV_INFO * ) deviceInfoPtr->storage;
			break;

		case CRYPT_DEVICE_PKCS11:
			deviceInfoPtr->devicePKCS11 = \
							( PKCS11_INFO * ) deviceInfoPtr->storage;
			break;

		case CRYPT_DEVICE_CRYPTOAPI:
			deviceInfoPtr->deviceCryptoAPI = \
							( CRYPTOAPI_INFO * ) deviceInfoPtr->storage;
			break;

		case CRYPT_DEVICE_HARDWARE:
			deviceInfoPtr->deviceHardware = \
							( HARDWARE_INFO * ) deviceInfoPtr->storage;
			break;

		default:
			retIntError();
		}
	deviceInfoPtr->storageSize = storageSize;

	/* Set up access information for the device */
	switch( deviceType )
		{
		case CRYPT_DEVICE_NONE:
			status = setDeviceSystem( deviceInfoPtr );
			break;

		case CRYPT_DEVICE_PKCS11:
			if( name == NULL )
				return( CRYPT_ARGERROR_STR1 );
			ENSURES( nameLength >= MIN_NAME_LENGTH && \
					 nameLength <= CRYPT_MAX_TEXTSIZE );
			status = setDevicePKCS11( deviceInfoPtr, name, nameLength );
			break;

		case CRYPT_DEVICE_CRYPTOAPI:
			status = setDeviceCryptoAPI( deviceInfoPtr );
			break;

		case CRYPT_DEVICE_HARDWARE:
			status = setDeviceHardware( deviceInfoPtr );
			break;

		default:
			retIntError();
		}
	if( cryptStatusOK( status ) && \
		deviceInfoPtr->createObjectFunctions == NULL )
		{
		/* The device-specific code hasn't set up anything, use the default
		   create-object functions, which just create encryption contexts
		   using the device capability information */
		deviceInfoPtr->createObjectFunctions = defaultCreateFunctions;
		deviceInfoPtr->createObjectFunctionCount = \
			FAILSAFE_ARRAYSIZE( defaultCreateFunctions, CREATEOBJECT_FUNCTION_INFO );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Connec to the device */
	return( deviceInfoPtr->initFunction( deviceInfoPtr, name, nameLength ) );
	}

/* Create a (non-system) device object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createDevice( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				  STDC_UNUSED const void *auxDataPtr, 
				  STDC_UNUSED const int auxValue )
	{
	CRYPT_DEVICE iCryptDevice;
	DEVICE_INFO *deviceInfoPtr = NULL;
	int initStatus, status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( createInfo->arg1 > CRYPT_DEVICE_NONE && \
			  createInfo->arg1 < CRYPT_DEVICE_LAST );
	REQUIRES( ( createInfo->arg1 != CRYPT_DEVICE_PKCS11 && \
				createInfo->arg1 != CRYPT_DEVICE_CRYPTOAPI ) || \
			  ( createInfo->strArgLen1 >= MIN_NAME_LENGTH && \
				createInfo->strArgLen1 <= CRYPT_MAX_TEXTSIZE ) );

	/* Wait for any async device driver binding to complete */
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) )
		{
		/* The kernel is shutting down, bail out */
		DEBUG_DIAG(( "Exiting due to kernel shutdown" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_PERMISSION );
		}

	/* Pass the call on to the lower-level open function */
	initStatus = openDevice( &iCryptDevice, createInfo->cryptOwner,
							 createInfo->arg1, createInfo->strArg1,
							 createInfo->strArgLen1, &deviceInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( deviceInfoPtr == NULL )
			return( initStatus );

		/* The init failed, make sure that the object gets destroyed when we
		   notify the kernel that the setup process is complete */
		krnlSendNotifier( iCryptDevice, IMESSAGE_DESTROY );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCryptDevice, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusOK( status ) && \
		createInfo->arg1 == CRYPT_DEVICE_CRYPTOAPI )
		{
		/* If it's a device that doesn't require an explicit login, move it
		   into the initialised state */
		status = krnlSendMessage( iCryptDevice, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_UNUSED,
								  CRYPT_IATTRIBUTE_INITIALISED );
		if( cryptStatusError( status ) )
			krnlSendNotifier( iCryptDevice, IMESSAGE_DESTROY );
		}
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );
	createInfo->cryptHandle = iCryptDevice;

	return( CRYPT_OK );
	}

/* Create the internal system device object.  This is somewhat special in
   that it can't be destroyed through a normal message (it can only be done
   from one place in the kernel) so if the open fails we don't use the normal
   signalling mechanism to destroy it but simply return an error code to the
   caller (the cryptlib init process).  This causes the init to fail and
   destroys the object when the kernel shuts down */

CHECK_RETVAL \
static int createSystemDeviceObject( void )
	{
	CRYPT_DEVICE iSystemObject;
	DEVICE_INFO *deviceInfoPtr;
	int initStatus, status;

	/* Pass the call on to the lower-level open function.  This device is
	   unique and has no owner or type.

	   Normally if an object init fails we tell the kernel to destroy it by 
	   sending it a destroy message, which is processed after the object's 
	   status has been set to normal.  However we don't have the privileges 
	   to do this for the system object (or the default user object) so we 
	   just pass the error code back to the caller, which causes the 
	   cryptlib init to fail.
	   
	   In addition the init can fail in one of two ways the object isn't
	   even created (deviceInfoPtr == NULL, nothing to clean up) in which 
	   case we bail out immediately, or the object is created but wasn't set 
	   up properly (deviceInfoPtr is allocated, but the object can't be 
	   used) in which case we bail out after we update its status.

	   A failure at this point is a bit problematic because it's not 
	   possible to inform the caller that the system object was successfully 
	   created but something went wrong after that.  That is, it's assumed
	   that create-object operations are atomic so that a failure status
	   indicates that all allocations and whatnot were rolled back, but 
	   since the system object can only be destroyed from within the kernel
	   once it's been created there's no way to roll back the creation of an
	   incomplete system object.  In fact it's not even clear what *could* 
	   cause a failure at this point apart from "circumstances beyond our 
	   control" (memory corruption, a coding error, or something similar).  
	   Because this is a can't-occur situation the best course of action for 
	   backing out of having a partially-initialised system object that's 
	   created at and exists at a level below what the standard system 
	   cleanup routines can handle is uncertain.
	   
	   At the moment we just exit and (potentialy) leak the memory, if this
	   situation ever occurs then presumably the calling application will 
	   exit and the OS will release the memory for us.  This is somewhat
	   ugly, but it's not really clear what other action we can take in the
	   error handler for a can-never-occur error */
	initStatus = openDevice( &iSystemObject, CRYPT_UNUSED, CRYPT_DEVICE_NONE,
							 NULL, 0, &deviceInfoPtr );
	if( deviceInfoPtr == NULL )
		return( initStatus );	/* Create object failed, return immediately */
	ENSURES( iSystemObject == SYSTEM_OBJECT_HANDLE );

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iSystemObject, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );

	/* The object has been initialised, move it into the initialised state */
	return( krnlSendMessage( iSystemObject, IMESSAGE_SETATTRIBUTE,
							 MESSAGE_VALUE_UNUSED,
							 CRYPT_IATTRIBUTE_INITIALISED ) );
	}

/* Generic management function for this class of object.  Unlike the usual
   multilevel init process which is followed for other objects, the devices
   have an OR rather than an AND relationship since the devices are
   logically independent so we set a flag for each device type that's
   successfully initialised rather than recording an init level */

typedef CHECK_RETVAL int ( *DEVICEINIT_FUNCTION )( void );
typedef void ( *DEVICEND_FUNCTION )( void );
typedef struct {
	DEVICEINIT_FUNCTION deviceInitFunction;
	DEVICEND_FUNCTION deviceEndFunction;
	const int initFlag;
	} DEVICEINIT_INFO;
		
#define DEV_NONE_INITED			0x00
#define DEV_PKCS11_INITED		0x01
#define DEV_CRYPTOAPI_INITED	0x02
#define DEV_HARDWARE_INITED		0x04

CHECK_RETVAL \
int deviceManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action )
	{
	DEVICEINIT_INFO deviceInitTbl[] = {
#ifdef USE_PKCS11
		{ deviceInitPKCS11, deviceEndPKCS11, DEV_PKCS11_INITED },
#endif /* USE_PKCS11 */
#ifdef USE_CRYPTOAPI
		{ deviceInitCryptoAPI, deviceEndCryptoAPI, DEV_CRYPTOAPI_INITED },
#endif /* USE_CRYPTOAPI */
#ifdef USE_HARDWARE
		{ deviceInitHardware, deviceEndHardware, DEV_HARDWARE_INITED },
#endif /* USE_HARDWARE */
		{ NULL, NULL, 0 }, { NULL, NULL, 0 }
		};
	static int initFlags = DEV_NONE_INITED;
	int i, status;

	REQUIRES( action == MANAGEMENT_ACTION_PRE_INIT || \
			  action == MANAGEMENT_ACTION_INIT || \
			  action == MANAGEMENT_ACTION_PRE_SHUTDOWN || \
			  action == MANAGEMENT_ACTION_SHUTDOWN );

	switch( action )
		{
		case MANAGEMENT_ACTION_PRE_INIT:
			status = createSystemDeviceObject();
			if( cryptStatusError( status ) )
				{
				DEBUG_DIAG(( "System object creation failed" ));
				}
			return( status );

		case MANAGEMENT_ACTION_INIT:
#ifndef CONFIG_FUZZ
			for( i = 0; deviceInitTbl[ i ].deviceInitFunction != NULL && \
						i < FAILSAFE_ARRAYSIZE( deviceInitTbl, \
												DEVICEINIT_INFO );
				 i++ )
				{
				if( krnlIsExiting() )
					{
					/* The kernel is shutting down, exit */
					return( CRYPT_ERROR_PERMISSION );
					}
				status = deviceInitTbl[ i ].deviceInitFunction();
				if( cryptStatusOK( status ) )
					initFlags |= deviceInitTbl[ i ].initFlag;
				}
			ENSURES( i < FAILSAFE_ARRAYSIZE( deviceInitTbl, \
											 DEVICEINIT_INFO ) );
#endif /* !CONFIG_FUZZ */
			return( CRYPT_OK );

		case MANAGEMENT_ACTION_PRE_SHUTDOWN:
			/* In theory we could signal the background entropy poll to 
			   start wrapping up at this point, however if the background 
			   polling is being performed in a thread or task then the 
			   shutdown is already signalled via the kernel shutdown flag.  
			   If it's performed by forking off a process, as it is on Unix 
			   systems, there's no easy way to communicate with this process 
			   so the shutdown function just kill()s it.  Because of this we 
			   don't try and do anything here, although this call is left in 
			   place as a no-op in case it's needed in the future */
			return( CRYPT_OK );

		case MANAGEMENT_ACTION_SHUTDOWN:
			for( i = 0; deviceInitTbl[ i ].deviceEndFunction != NULL && \
						i < FAILSAFE_ARRAYSIZE( deviceInitTbl, \
												DEVICEINIT_INFO );
				 i++ )
				{
				if( initFlags & deviceInitTbl[ i ].initFlag )
					deviceInitTbl[ i ].deviceEndFunction();
				}
			ENSURES( i < FAILSAFE_ARRAYSIZE( deviceInitTbl, \
											 DEVICEINIT_INFO ) );
			return( CRYPT_OK );
		}

	retIntError();
	}
