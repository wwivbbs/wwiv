/****************************************************************************
*																			*
*					  cryptlib Generic Crypto HW Routines					*
*						Copyright Peter Gutmann 1998-2009					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "hardware.h"
  #include "dev_mech.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "device/hardware.h"
  #include "mechs/dev_mech.h"
#endif /* Compiler-specific includes */

#ifdef USE_HARDWARE

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Get access to the hardware device associated with a context */

static int getContextDeviceInfo( const CRYPT_HANDLE iCryptContext,
								 CRYPT_DEVICE *iCryptDevice, 
								 HARDWARE_INFO **hwInfoPtrPtr )
	{
	CRYPT_DEVICE iLocalDevice;
	DEVICE_INFO *deviceInfo;
	int status;

	assert( isWritePtr( iCryptDevice, sizeof( CRYPT_DEVICE ) ) );
	assert( isWritePtr( hwInfoPtrPtr, sizeof( HARDWARE_INFO * ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Clear return values */
	*iCryptDevice = CRYPT_ERROR;
	*hwInfoPtrPtr = NULL;

	/* Get the the device associated with this context */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETDEPENDENT, 
							  &iLocalDevice, OBJECT_TYPE_DEVICE );
	if( cryptStatusError( status ) )
		return( status );

	/* Get the hardware information from the device information */
	status = krnlAcquireObject( iLocalDevice, OBJECT_TYPE_DEVICE, 
								( void ** ) &deviceInfo, 
								CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( status ) )
		return( status );
	*iCryptDevice = iLocalDevice;
	*hwInfoPtrPtr = deviceInfo->deviceHardware;

	return( CRYPT_OK );
	}

/* Get a reference to the cryptographic hardware object that underlies a 
   cryptlib object.  This is used to connect a template object from a
   PKCS #15 storage object to the corresponding hardware object via the
   storageID that's recorded in the storage object */

static int getHardwareReference( const CRYPT_CONTEXT iCryptContext,
								 int *keyHandle )
	{
	MESSAGE_DATA msgData;
	BYTE storageID[ KEYID_SIZE + 8 ];
	int status;

	assert( isWritePtr( keyHandle, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	setMessageData( &msgData, storageID, KEYID_SIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_DEVICESTORAGEID );
	if( cryptStatusOK( status ) )
		status = hwLookupItem( storageID, KEYID_SIZE, keyHandle );
	if( cryptStatusError( status ) )
		{
		/* In theory this is an internal error but in practice we shouldn't
		   treat this as too fatal, what it really means is that the crypto
		   hardware (which we don't control and therefore can't do too much
		   about) is out of sync with the PKCS #15 storage object.  This can 
		   happen for example during the development process when the 
		   hardware is reinitialised but the storage object isn't, or from
		   any one of a number of other circumstances beyond our control.  
		   To deal with this we return a standard notfound error but also 
		   output a diagnostic message for developers to let them know that
		   they need to check hardware/storage object synchronisation */
		DEBUG_PRINT(( "Object held in PKCS #15 object store doesn't "
					  "correspond to anything known to the crypto "
					  "hardware HAL" ));
		return( CRYPT_ERROR_NOTFOUND );
		}

	return( CRYPT_OK );
	}

/* Open the PKCS #15 storage object associated with this device */

static int openStorageObject( CRYPT_KEYSET *iCryptKeyset,
							  const CRYPT_KEYOPT_TYPE options,
							  const CRYPT_DEVICE iCryptDevice )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	char storageFilePath[ MAX_PATH_LENGTH + 8 ];
	int storageFilePathLen, status;

	assert( isWritePtr( iCryptKeyset, sizeof( CRYPT_KEYSET ) ) );

	REQUIRES( options == CRYPT_KEYOPT_NONE || \
			  options == CRYPT_KEYOPT_CREATE );
	REQUIRES( isHandleRangeValid( iCryptDevice ) );

	/* Clear return value */
	*iCryptKeyset = CRYPT_ERROR;

	/* Try and open/create the PKCS #15 storage object */
	status = fileBuildCryptlibPath( storageFilePath, MAX_PATH_LENGTH, 
									&storageFilePathLen, "CLKEYS", 6, 
									BUILDPATH_GETPATH );
	if( cryptStatusError( status ) )
		return( status );
	setMessageCreateObjectInfo( &createInfo, CRYPT_KEYSET_FILE );
	if( options != CRYPT_KEYOPT_NONE )
		createInfo.arg2 = options;
	createInfo.strArg1 = storageFilePath;
	createInfo.strArgLen1 = storageFilePathLen;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_KEYSET );
	if( cryptStatusError( status ) )
		return( status );

	/* Now that we've got the storage object we have to perform a somewhat 
	   awkward double-linked-list update of the keyset to give it the handle 
	   of the owning device since we need to create any contexts for keys 
	   fetched from the storage object via the hardware device rather than 
	   the default system device.  In theory we could also do this via a new 
	   get-owning-object message but we still need to signal to the keyset 
	   that it's a storage object rather than a standard keyset so this 
	   action serves a second purpose anyway and we may as well use it to 
	   explicitly set the owning-device handle at the same time.

	   Note that we don't set the storage object as a dependent object of 
	   the device because it's not necessarily constant across device 
	   sessions.  In particular if we initialise or zeroise the device then 
	   the storage object will be reset, but there's no way to switch 
	   dependent objects without destroying and recreating the parent.  In
	   addition it's not certain whether the storage-object keyset should
	   really be a dependent object or not, in theory it's nice because it
	   allows keyset-specific messages/accesses to be sent to the device and
	   automatically routed to the keyset (standard accesses will still go 
	   to the device, so for example a getItem() will be handled as a 
	   device-get rather than a keyset-get) but such unmediated access to 
	   the underlying keyset probably isn't a good idea anyway */
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &iCryptDevice, 
							  CRYPT_IATTRIBUTE_HWSTORAGE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iCryptKeyset = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 		Init/Shutdown Routines						*
*																			*
****************************************************************************/

/* Initialise the capability information */

#define MAX_DEVICE_CAPABILITIES		32

static CAPABILITY_INFO_LIST capabilityInfoList[ MAX_DEVICE_CAPABILITIES ];

/* Initialise the cryptographic hardware and its crypto capability 
   interface */

int deviceInitHardware( void )
	{
	CAPABILITY_INFO *capabilityInfo;
	static BOOLEAN initCalled = FALSE;
	int noCapabilities, i, status;

	/* If we've previously tried to initialise the hardware, don't try it 
	   again */
	if( initCalled )
		return( CRYPT_OK );
	initCalled = TRUE;

	/* Get the hardware capability information */
	status = hwGetCapabilities( &capabilityInfo, &noCapabilities );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( noCapabilities > 0 && \
			 noCapabilities < MAX_DEVICE_CAPABILITIES );

	/* Build the list of available capabilities */
	memset( capabilityInfoList, 0, 
			sizeof( CAPABILITY_INFO_LIST ) * MAX_DEVICE_CAPABILITIES );
	for( i = 0; i < noCapabilities && \
				capabilityInfo[ i ].cryptAlgo != CRYPT_ALGO_NONE; i++ )
		{
		REQUIRES( sanityCheckCapability( &capabilityInfo[ i ], FALSE ) );
		
		capabilityInfoList[ i ].info = &capabilityInfo[ i ];
		capabilityInfoList[ i ].next = NULL;
		if( i > 0 )
			capabilityInfoList[ i - 1 ].next = &capabilityInfoList[ i ];
		}
	ENSURES( i < noCapabilities );

	return( CRYPT_OK );
	}

void deviceEndHardware( void )
	{
	}

/****************************************************************************
*																			*
*					Device Init/Shutdown/Device Control Routines			*
*																			*
****************************************************************************/

/* Close a previously-opened session with the device.  We have to have this
   before the initialisation function since it may be called by it if the 
   initialisation process fails */

static void shutdownFunction( DEVICE_INFO *deviceInfo )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	/* Shut down access to the storage object */
	if( hardwareInfo->iCryptKeyset != CRYPT_ERROR )
		{
		krnlSendNotifier( hardwareInfo->iCryptKeyset, IMESSAGE_DECREFCOUNT );
		hardwareInfo->iCryptKeyset = CRYPT_ERROR;
		}
	deviceInfo->flags &= ~( DEVICE_ACTIVE | DEVICE_LOGGEDIN );
	}

/* Open a session with the device */

static int initFunction( DEVICE_INFO *deviceInfo, const char *name,
						 const int nameLength )
	{
	CRYPT_KEYSET iCryptKeyset;
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	int status;

	UNUSED_ARG( name );

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	/* Set up any internal objects to contain invalid handles */
	hardwareInfo->iCryptKeyset = CRYPT_ERROR;

	/* Set up the device ID information */
	memcpy( hardwareInfo->label, "Cryptographic hardware device", 29 );
	hardwareInfo->labelLen = 29;
	deviceInfo->label = hardwareInfo->label;
	deviceInfo->labelLen = hardwareInfo->labelLen;

	/* Since this is a built-in hardware device it's always present and 
	   available */
	deviceInfo->flags |= DEVICE_ACTIVE | DEVICE_LOGGEDIN;

	/* Try and open the PKCS #15 storage object.  If we can't open it it 
	   means that either it doesn't exist (i.e. persistent key storage
	   isn't supported) or the device hasn't been initialised yet.  This 
	   isn't a fatal error, although it does mean that public-key 
	   operations will be severely restricted since these depend on storing 
	   key metadata in the storage object */
	status = openStorageObject( &iCryptKeyset, CRYPT_KEYOPT_NONE,
								deviceInfo->objectHandle );
	if( cryptStatusError( status ) )
		return( CRYPT_OK );	/* Storage object not available */
	hardwareInfo->iCryptKeyset = iCryptKeyset;

	return( CRYPT_OK );
	}

/* Handle device control functions */

static int controlFunction( DEVICE_INFO *deviceInfo,
							const CRYPT_ATTRIBUTE_TYPE type,
							const void *data, const int dataLength,
							MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	int status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isAttribute( type ) || isInternalAttribute( type ) );

	UNUSED_ARG( hardwareInfo );
	UNUSED_ARG( messageExtInfo );

	/* Handle user authorisation.  Since this is a built-in hardware device 
	   it's always available for use so these are just dummy routines, 
	   although they can be expanded to call down into the HAL for actual
	   authentication if any hardware with such a facility is ever used */
	if( type == CRYPT_DEVINFO_AUTHENT_USER || \
		type == CRYPT_DEVINFO_AUTHENT_SUPERVISOR )
		{
		/* Authenticate the user */
		/* ... */

		/* The device is now ready for use */
		deviceInfo->flags |= DEVICE_LOGGEDIN;		
		krnlSendMessage( deviceInfo->objectHandle, IMESSAGE_SETATTRIBUTE, 
						 MESSAGE_VALUE_UNUSED, CRYPT_IATTRIBUTE_INITIALISED );
		return( CRYPT_OK );
		}

	/* Handle authorisation value change */
	if( type == CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR )
		{
		/* Set SO PIN */
		/* ... */

		return( CRYPT_OK );
		}
	if( type == CRYPT_DEVINFO_SET_AUTHENT_USER )
		{
		/* Set user PIN */
		/* ... */

		return( CRYPT_OK );
		}

	/* Handle initialisation and zeroisation */
	if( type == CRYPT_DEVINFO_INITIALISE || \
		type == CRYPT_DEVINFO_ZEROISE )
		{
		CRYPT_KEYSET iCryptKeyset;

		/* Shut down any existing state if necessary in preparation for the 
		   zeroise/initialise.  Since this clears all state we manually 
		   reset the device-active flag since we're still active, just with
		   all information cleared */
		if( hardwareInfo->iCryptKeyset != CRYPT_ERROR )
			shutdownFunction( deviceInfo );
		hwInitialise();
		deviceInfo->flags |= DEVICE_ACTIVE;

		/* The only real difference between a zeroise and an initialise is
		   that the zeroise only clears existing state and exits while the 
		   initialise resets the state with the device ready to be used 
		   again */
		if( type == CRYPT_DEVINFO_ZEROISE )
			{
			char storageFilePath[ MAX_PATH_LENGTH + 1 + 8 ];
			int storageFilePathLen;

			/* Zeroise the device */
			status = fileBuildCryptlibPath( storageFilePath, MAX_PATH_LENGTH, 
											&storageFilePathLen, "CLKEYS", 6, 
											BUILDPATH_GETPATH );
			if( cryptStatusOK( status ) )
				{
				storageFilePath[ storageFilePathLen ] = '\0';
				fileErase( storageFilePath );
				}
			deviceInfo->flags &= ~DEVICE_LOGGEDIN;
			return( status );
			}

		/* Initialise the device.  In theory we're already in the logged-in
		   state but if the initialise was preceded by a zeroise then this
		   will have been cleared, so we explicitly re-set it */
		status = openStorageObject( &iCryptKeyset, CRYPT_KEYOPT_CREATE,
									deviceInfo->objectHandle );
		if( cryptStatusError( status ) )
			return( status );
		hardwareInfo->iCryptKeyset = iCryptKeyset;
		deviceInfo->flags |= DEVICE_LOGGEDIN;

		return( CRYPT_OK );
		}

	/* Handle high-reliability time */
	if( type == CRYPT_IATTRIBUTE_TIME )
		{
		time_t *timePtr = ( time_t * ) data;

		UNUSED_ARG( timePtr );

		return( CRYPT_ERROR_NOTAVAIL );
		}

	retIntError();
	}

/* Get random data from the device.  The messageExtInfo parameter is used
   to handle recursive messages sent to the system device during the 
   randomness-polling process and isn't used here */

static int getRandomFunction( DEVICE_INFO *deviceInfo, void *buffer,
							  const int length,
							  MESSAGE_FUNCTION_EXTINFO *messageExtInfo )
	{
	UNUSED_ARG( messageExtInfo );

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH );

	/* Fill the buffer with random data */
	return( hwGetRandom( buffer, length ) );
	}

/****************************************************************************
*																			*
*						Get/Set/Delete Item Routines						*
*																			*
****************************************************************************/

/* Instantiate an object in a device.  This works like the create context
   function but instantiates a cryptlib object using data already contained
   in the device (for example a stored private key or certificate).  If the
   value being read is a public key and there's a certificate attached then 
   the instantiated object is a native cryptlib object rather than a device
   object with a native certificate object attached because there doesn't 
   appear to be any good reason to create the public-key object in the 
   device, and the cryptlib native object will probably be faster anyway */

static int getItemFunction( DEVICE_INFO *deviceInfo,
							CRYPT_CONTEXT *iCryptContext,
							const KEYMGMT_ITEM_TYPE itemType,
							const CRYPT_KEYID_TYPE keyIDtype,
							const void *keyID, const int keyIDlength,
							void *auxInfo, int *auxInfoLength, 
							const int flags )
	{
#if 0
	const CRYPT_DEVICE cryptDevice = deviceInfo->objectHandle;
	CRYPT_CERTIFICATE iCryptCert = CRYPT_UNUSED;
	const CAPABILITY_INFO *capabilityInfoPtr;
	HW_KEYINFO keyInfo;
	MESSAGE_DATA msgData;
	const int extraFlags = ( itemType == KEYMGMT_ITEM_PRIVATEKEY ) ? \
						   KEYMGMT_FLAG_DATAONLY_CERT : 0;
	int keyHandle, p15status, status;
#endif
	CRYPT_CONTEXT iLocalContext;
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	int keyHandle, status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER );
	REQUIRES( auxInfo == NULL && *auxInfoLength == 0 );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

#if 1
	/* Redirect the fetch down to the PKCS #15 storage object, which will
	   create either a dummy context that we have to connect to the actual
	   hardware for a private-key object or a native public-key/certificate 
	   object if it's a non-private-key item */
	if( hardwareInfo->iCryptKeyset == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTINITED );
	setMessageKeymgmtInfo( &getkeyInfo, keyIDtype, keyID, keyIDlength,
						   NULL, 0, flags );
	status = krnlSendMessage( hardwareInfo->iCryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo,
							  itemType );
	if( cryptStatusError( status ) )
		return( status );
	iLocalContext = getkeyInfo.cryptHandle;

	/* If it's a public-key fetch, we've created a native object and we're
	   done */
	if( itemType != KEYMGMT_ITEM_PRIVATEKEY )
		{
		*iCryptContext = iLocalContext;
		return( CRYPT_OK );
		}

	/* It was a private-key fetch, we need to connect the dummy context that
	   was created with the underlying hardware.  When this final step
	   has been completed we can move the context to the initialised state */
	status = getHardwareReference( iLocalContext, &keyHandle );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE,
							  &keyHandle, CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE, 
								  MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
#else
	/* As a first step we redirect the fetch down to the PKCS #15 storage 
	   object to get any certificate that may be associated with the item.
	   We always do this because if we're fetching a public key then this
	   is all that we need and if we're fetching a private key then we'll
	   associate the certificate with it if it's present */
	if( hardwareInfo->iCryptKeyset == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTINITED );
	setMessageKeymgmtInfo( &getkeyInfo, keyIDtype, keyID, keyIDlength,
						   NULL, 0, flags | extraFlags );
	p15status = krnlSendMessage( hardwareInfo->iCryptKeyset,
								 IMESSAGE_KEY_GETKEY, &getkeyInfo,
								 KEYMGMT_ITEM_PUBLICKEY );
	if( cryptStatusOK( p15status ) )
		{
		iCryptCert = getkeyInfo.cryptHandle;

		/* If we were after a public key, we're done */
		if( itemType == KEYMGMT_ITEM_PUBLICKEY )
			{
			*iCryptContext = iCryptCert;
			return( CRYPT_OK );
			}

		/* If we were after a private key and the keyID is one that isn't
		   stored locally, extract it from the returned private key */
		// This won't work, there's no CRYPT_IKEYID_KEYID or 
		// CRYPT_IKEYID_PGPKEYID with the cert, only an 
		// CRYPT_IKEYID_ISSUERANDSERIALNUMBER
		}

	/* Look up the private key, which we may use directly or simply extract
	   the public-key components from.  If there's an error then we 
	   preferentially return the error code from the storage object, which 
	   is likely to be more informative than a generic CRYPT_ERROR_NOTFOUND 
	   from the hardware lookup */
	status = hwLookupItemInfo( keyIDtype, keyID, keyIDlength, &keyHandle, 
							   &keyInfo );
	if( cryptStatusError( status ) )
		return( cryptStatusError( p15status ) ? p15status : status );
	if( itemType == KEYMGMT_ITEM_PUBLICKEY )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		/* There's no certificate present but we do have private-key 
		   components from which we can extract the public-key portion to
		   create a native context instead of a device one.  This solves a 
		   variety of problems including the fact that performing public-key 
		   operations natively is often much faster than the time it takes
		   to marshall the data and get it to the crypto hardware, and that 
		   if we do it ourselves we can defend against a variety of RSA 
		   padding and timing attacks that have come up since the device 
		   firmware was done */
		setMessageCreateObjectInfo( &createInfo, keyInfo.cryptAlgo );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		iLocalContext = createInfo.cryptHandle;

		/* Send the keying information to the context.  We don't set the 
		   action flags because there are none recorded at the hardware 
		   level */
		status = setPublicComponents( createInfo.cryptHandle, 
									  keyInfo.cryptAlgo, 
									  &keyInfo.publicKeyInfo );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		*iCryptContext = iLocalContext;
		return( CRYPT_OK );
		}

	/* It's a private key, create a dummy context for the device object, 
	   remember the device that it's contained in, and record the handle for 
	   the device-internal key */
	capabilityInfoPtr = findCapabilityInfo( deviceInfo->capabilityInfoList, 
											keyInfo.cryptAlgo );
	if( capabilityInfoPtr == NULL )
		return( CRYPT_ERROR_NOTAVAIL );
	status = createContextFromCapability( &iLocalContext, cryptDevice, 
										  capabilityInfoPtr, 
										  CREATEOBJECT_FLAG_DUMMY | \
										  CREATEOBJECT_FLAG_PERSISTENT );
	if( cryptStatusError( status ) )
		{
		if( iCryptCert != CRYPT_UNUSED )
			krnlSendNotifier( iCryptCert, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	status = krnlSendMessage( iLocalContext, IMESSAGE_SETDEPENDENT,
							  ( MESSAGE_CAST ) &cryptDevice, 
							  SETDEP_OPTION_INCREF );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &keyHandle, 
								  CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		if( iCryptCert != CRYPT_UNUSED )
			krnlSendNotifier( iCryptCert, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Set the object's label, send the keying information to the context, 
	   and mark it as initialised (i.e. with a key loaded).  Setting the 
	   label requires special care because the label that we're setting 
	   matches that of an existing object, so trying to set it as a standard 
	   CRYPT_CTXINFO_LABEL will return a CRYPT_ERROR_DUPLICATE error when 
	   the context code checks for the existence of an existing label.  To 
	   handle this, we use the attribute CRYPT_IATTRIBUTE_EXISTINGLABEL to 
	   indicate that we're setting a label that matches an existing object 
	   in the device */
	if( keyInfo.labelLength <= 0 )
		{
		/* If there's no label present, use a dummy value */
		strlcpy_s( keyInfo.label, CRYPT_MAX_TEXTSIZE, "Label-less private key" );
		keyInfo.labelLength = 22;
		}
	setMessageData( &msgData, keyInfo.label, keyInfo.labelLength );
	status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_EXISTINGLABEL );
	if( cryptStatusOK( status ) )
		status = setPublicComponents( iLocalContext, keyInfo.cryptAlgo, 
									  &keyInfo.publicKeyInfo );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
	if( cryptStatusOK( status ) && ( iCryptCert != CRYPT_UNUSED ) )
		{
		/* If there's a certificate present, attach it to the context.  The 
		   certificate is an internal object used only by the context so we 
		   tell the kernel to mark it as owned by the context only */
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETDEPENDENT, 
								  ( MESSAGE_CAST ) &iCryptCert, 
								  SETDEP_OPTION_NOINCREF );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		if( iCryptCert != CRYPT_UNUSED )
			krnlSendNotifier( iCryptCert, IMESSAGE_DECREFCOUNT );
		}
#endif /* 1 */

	*iCryptContext = iLocalContext;
	return( CRYPT_OK );
	}

/* Add an object to a device.  This can only ever add a certificate
   (enforced by the kernel ACLs) so we don't have to perform any 
   special-case handling */

static int setItemFunction( DEVICE_INFO *deviceInfo, 
							const CRYPT_HANDLE iCryptHandle )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	MESSAGE_KEYMGMT_INFO setkeyInfo;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );

	/* Redirect the add down to the PKCS #15 storage object */
	if( hardwareInfo->iCryptKeyset == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTINITED );
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
						   NULL, 0, KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = iCryptHandle;
	return( krnlSendMessage( hardwareInfo->iCryptKeyset,
							 IMESSAGE_KEY_SETKEY, &setkeyInfo,
							 KEYMGMT_ITEM_PUBLICKEY ) );
	}

/* Delete an object in a device */

static int deleteItemFunction( DEVICE_INFO *deviceInfo,
							   const KEYMGMT_ITEM_TYPE itemType,
							   const CRYPT_KEYID_TYPE keyIDtype,
							   const void *keyID, const int keyIDlength )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	MESSAGE_KEYMGMT_INFO getkeyInfo, deletekeyInfo;
	int status;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME );
	REQUIRES( keyIDlength > 0 && keyIDlength <= CRYPT_MAX_TEXTSIZE );

	/* Perform the delete both from the PKCS #15 storage object and the
	   native storage.  This gets a bit complicated because in order to 
	   find the hardware object we have to extract the storageID, and in
	   order to get that we have to instantiate a dummy private-key object
	   to contain it.  In addition if the object that's stored isn't a
	   private-key object then there's no associated cryptographic 
	   hardware object.  To handle this we try and instantiate a dummy
	   private-key object in order to get the storageID.  If this succeeds,
	   we locate the underlying hardware object and delete it.  Finally, we
	   delete the PKCS #15 object, either a pure public-key/certificate 
	   object or the private-key metadata for the cryptographic hardware
	   object */
	if( hardwareInfo->iCryptKeyset == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTINITED );
	setMessageKeymgmtInfo( &getkeyInfo, keyIDtype, keyID, keyIDlength,
						   NULL, 0, KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( hardwareInfo->iCryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo,
							  KEYMGMT_ITEM_PRIVATEKEY );
	if( cryptStatusOK( status ) )
		{
		int keyHandle;

		/* It's a private-key object, get its hardware reference and delete 
		   it.  If this fails we continue anyway because we know that 
		   there's also a PKCS #15 object to delete */
		status = getHardwareReference( getkeyInfo.cryptHandle, &keyHandle );
		if( cryptStatusOK( status ) )
			( void ) hwDeleteItem( keyHandle );
		krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		}
	setMessageKeymgmtInfo( &deletekeyInfo, keyIDtype, keyID, keyIDlength,
						   NULL, 0, KEYMGMT_FLAG_NONE );
	return( krnlSendMessage( hardwareInfo->iCryptKeyset,
							 IMESSAGE_KEY_DELETEKEY, &deletekeyInfo,
							 itemType ) );
	}

/* Get the sequence of certificates in a chain from a device.  Since these 
   functions operate only on certificates we can redirect them straight down 
   to the underlying storage object */

static int getFirstItemFunction( DEVICE_INFO *deviceInfo, 
								 CRYPT_CERTIFICATE *iCertificate,
								 int *stateInfo, 
								 const CRYPT_KEYID_TYPE keyIDtype,
								 const void *keyID, const int keyIDlength,
								 const KEYMGMT_ITEM_TYPE itemType, 
								 const int options )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	MESSAGE_KEYMGMT_INFO getnextcertInfo;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );

	REQUIRES( keyIDtype == CRYPT_IKEYID_KEYID );
	REQUIRES( keyIDlength > 4 && keyIDlength < MAX_INTLENGTH_SHORT );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	*stateInfo = CRYPT_ERROR;

	/* Get the first certificate */
	setMessageKeymgmtInfo( &getnextcertInfo, keyIDtype, keyID, keyIDlength, 
						   stateInfo, sizeof( int ), options );
	return( krnlSendMessage( hardwareInfo->iCryptKeyset, 
							 IMESSAGE_KEY_GETFIRSTCERT, &getnextcertInfo, 
							 KEYMGMT_ITEM_PUBLICKEY ) );
	}

static int getNextItemFunction( DEVICE_INFO *deviceInfo, 
								CRYPT_CERTIFICATE *iCertificate,
								int *stateInfo, const int options )
	{
	HARDWARE_INFO *hardwareInfo = deviceInfo->deviceHardware;
	MESSAGE_KEYMGMT_INFO getnextcertInfo;

	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( *stateInfo ) || \
			  *stateInfo == CRYPT_ERROR );

	UNUSED_ARG( hardwareInfo );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* If the previous certificate was the last one, there's nothing left to 
	   fetch */
	if( *stateInfo == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

	/* Get the next certificate */
	setMessageKeymgmtInfo( &getnextcertInfo, CRYPT_KEYID_NONE, NULL, 0, 
						   stateInfo, sizeof( int ), options );
	return( krnlSendMessage( hardwareInfo->iCryptKeyset, 
							 IMESSAGE_KEY_GETNEXTCERT, &getnextcertInfo, 
							 KEYMGMT_ITEM_PUBLICKEY ) );
	}

/****************************************************************************
*																			*
*					Cryptographic HAL Assist Routines						*
*																			*
****************************************************************************/

/* The following helper routines provide common functionality needed by most 
   cryptographic HALs, which avoids having to reimplement the same code in 
   each HAL module.  These are called from the HAL to provide services such 
   as keygen assist and various context-management functions */

/* Set up a mapping from a context to its associated information in the
   underlying hardware.  This generates a storageID for the information and
   records it and the hardware handle in the context.  The caller has to 
   record the storageID alongside the crypto information held by the 
   hardware for later use to look up the information */

int setPersonalityMapping( CONTEXT_INFO *contextInfoPtr, const int keyHandle,
						   void *storageID, const int storageIDlength )
	{
	CRYPT_DEVICE iCryptDevice;
	HARDWARE_INFO *hardwareInfo;
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	MESSAGE_DATA msgData;
	BYTE buffer[ KEYID_SIZE + 8 ];
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( storageID, storageIDlength ) );

	REQUIRES( keyHandle >= 0 && keyHandle < INT_MAX );
	REQUIRES( storageIDlength >= 4 && storageIDlength <= KEYID_SIZE );

	/* Set up the mapping information in the context */
	status = hwGetRandom( buffer, KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, buffer, KEYID_SIZE );
	status = krnlSendMessage( contextInfoPtr->objectHandle, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData, 
							  CRYPT_IATTRIBUTE_DEVICESTORAGEID );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( contextInfoPtr->objectHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &keyHandle, 
								  CRYPT_IATTRIBUTE_DEVICEOBJECT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Copy the storageID back to the caller */
	memcpy( storageID, buffer, storageIDlength );

	/* If it's a non-PKC context then there's nothing further to do */
	if( contextInfoPtr->type != CONTEXT_PKC )
		return( CRYPT_OK );

	/* As a variation of the above, if it's a public-key context then we 
	   don't want to persist it to the storage object because public-key
	   contexts a bit of an anomaly, when generating our own keys we always 
	   have full private keys and when obtaining public keys from an 
	   external source they'll be in the form of certificates so there isn't 
	   really much need for persistent raw public keys.  At the moment the 
	   only time they're used is for the self-test, and potentially 
	   polluting the (typically quite limited) crypto hardware storage with 
	   unneeded public keys doesn't seem like a good idea */
	if( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY )
		return( CRYPT_OK );

	/* It's a PKC context, prepare to persist the key metadata to the 
	   underlying PKCS #15 object store */
	status = getContextDeviceInfo( contextInfoPtr->objectHandle, 
								   &iCryptDevice, &hardwareInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( hardwareInfo->iCryptKeyset == CRYPT_ERROR )
		{
		krnlReleaseObject( iCryptDevice );
		return( CRYPT_ERROR_NOTINITED );
		}

	/* Since this is a dummy context that contains no actual keying 
	   information (the key data is held in hardware) we set it as 
	   KEYMGMT_ITEM_KEYMETADATA */
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0, NULL, 0, 
						   KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = contextInfoPtr->objectHandle;
	status = krnlSendMessage( hardwareInfo->iCryptKeyset,
							  IMESSAGE_KEY_SETKEY, &setkeyInfo,
							  KEYMGMT_ITEM_KEYMETADATA );
	krnlReleaseObject( iCryptDevice );

	return( status );
	}

/* Many hardware devices have no native public/private key generation 
   support or can only generate something like the DLP x value, which is 
   just a raw string of random bits, but can't generate a full set of
   public/private key parameters since these require complex bignum
   operations not supported by the underlying cryptologic.  To handle this
   we provide supplementary software support routines that generate the
   key parameters using native contexts and bignum code and then pass them
   back to the crypto HAL to load into the cryptlogic */

static int generateKeyComponents( CONTEXT_INFO *staticContextInfo,
								  PKC_INFO *contextData, 
								  const CAPABILITY_INFO *capabilityInfoPtr,
								  const int keySizeBits )
	{
	int status;

	assert( isWritePtr( staticContextInfo, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( contextData, sizeof( PKC_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );

	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Initialise a static context to generate the key into */
	status = staticInitContext( staticContextInfo, CONTEXT_PKC, 
								capabilityInfoPtr, contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate a key into the static context */
	status = capabilityInfoPtr->generateKeyFunction( staticContextInfo,
													 keySizeBits );
	if( cryptStatusError( status ) )
		{
		staticDestroyContext( staticContextInfo );
		return( status );
		}

	return( CRYPT_OK );
	}

static int rsaGenerateComponents( CRYPT_PKCINFO_RSA *rsaKeyInfo,
								  const int keySizeBits )
	{
	CONTEXT_INFO staticContextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	int status;

	assert( isWritePtr( rsaKeyInfo, sizeof( CRYPT_PKCINFO_RSA ) ) );

	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Clear return value */
	cryptInitComponents( rsaKeyInfo, FALSE );

	/* Generate the key components */
	status = generateKeyComponents( &staticContextInfo, &contextData, 
									getRSACapability(), keySizeBits );
	if( cryptStatusError( status ) )
		return( status );

	/* Extract the newly-generated key components for the caller to use */
	BN_bn2bin( &pkcInfo->rsaParam_n, rsaKeyInfo->n );
	rsaKeyInfo->nLen = BN_num_bits( &pkcInfo->rsaParam_n );
	BN_bn2bin( &pkcInfo->rsaParam_e, rsaKeyInfo->e );
	rsaKeyInfo->eLen = BN_num_bits( &pkcInfo->rsaParam_e );
	BN_bn2bin( &pkcInfo->rsaParam_p, rsaKeyInfo->p );
	rsaKeyInfo->pLen = BN_num_bits( &pkcInfo->rsaParam_p );
	BN_bn2bin( &pkcInfo->rsaParam_q, rsaKeyInfo->q );
	rsaKeyInfo->qLen = BN_num_bits( &pkcInfo->rsaParam_q );
	BN_bn2bin( &pkcInfo->rsaParam_exponent1, rsaKeyInfo->e1 );
	rsaKeyInfo->e1Len = BN_num_bits( &pkcInfo->rsaParam_exponent1 );
	BN_bn2bin( &pkcInfo->rsaParam_exponent2, rsaKeyInfo->e2 );
	rsaKeyInfo->e2Len = BN_num_bits( &pkcInfo->rsaParam_exponent2 );
	BN_bn2bin( &pkcInfo->rsaParam_u, rsaKeyInfo->u );
	rsaKeyInfo->uLen = BN_num_bits( &pkcInfo->rsaParam_u );
	staticDestroyContext( &staticContextInfo );

	return( status );
	}

#if defined( USE_DH ) || defined( USE_DSA ) || defined( USE_ELGAMAL )

static int dlpGenerateComponents( CRYPT_PKCINFO_DLP *dlpKeyInfo,
								  const int keySizeBits,
								  const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CONTEXT_INFO staticContextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	int status;

	assert( isWritePtr( dlpKeyInfo, sizeof( CRYPT_PKCINFO_DLP ) ) );

	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );
	REQUIRES( cryptAlgo == CRYPT_ALGO_DH || \
			  cryptAlgo == CRYPT_ALGO_DSA || \
			  cryptAlgo == CRYPT_ALGO_ELGAMAL );

	/* Clear return value */
	cryptInitComponents( dlpKeyInfo, FALSE );

	/* Generate the key components */
	switch( cryptAlgo )
		{
#ifdef USE_DH
		case CRYPT_ALGO_DH:
			status = generateKeyComponents( &staticContextInfo, &contextData, 
											getDHCapability(), keySizeBits );
			break;
#endif /* USE_DH */

#ifdef USE_DSA
		case CRYPT_ALGO_DSA:
			status = generateKeyComponents( &staticContextInfo, &contextData, 
											getDSACapability(), keySizeBits );
			break;
#endif /* USE_DSA */

#ifdef USE_ELGAMAL
		case CRYPT_ALGO_ELGAMAL:
			status = generateKeyComponents( &staticContextInfo, &contextData, 
											getElgamalCapability(), keySizeBits );
			break;
#endif /* USE_ELGAMAL */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Extract the newly-generated key components for the caller to use */
	BN_bn2bin( &pkcInfo->dlpParam_p, dlpKeyInfo->p );
	dlpKeyInfo->pLen = BN_num_bits( &pkcInfo->dlpParam_p );
	BN_bn2bin( &pkcInfo->dlpParam_g, dlpKeyInfo->g );
	dlpKeyInfo->gLen = BN_num_bits( &pkcInfo->dlpParam_g );
	BN_bn2bin( &pkcInfo->dlpParam_q, dlpKeyInfo->q );
	dlpKeyInfo->qLen = BN_num_bits( &pkcInfo->dlpParam_q );
	BN_bn2bin( &pkcInfo->dlpParam_y, dlpKeyInfo->y );
	dlpKeyInfo->yLen = BN_num_bits( &pkcInfo->dlpParam_y );
	BN_bn2bin( &pkcInfo->dlpParam_x, dlpKeyInfo->x );
	dlpKeyInfo->xLen = BN_num_bits( &pkcInfo->dlpParam_x );
	staticDestroyContext( &staticContextInfo );

	return( status );
	}
#endif /* USE_DH || USE_DSA || USE_ELGAMAL */

int generatePKCcomponents( CONTEXT_INFO *contextInfoPtr, 
						   void *keyInfo, const int keySizeBits )
	{
	const CRYPT_ALGO_TYPE cryptAlgo = \
					contextInfoPtr->capabilityInfo->cryptAlgo;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keyInfo != NULL );
	REQUIRES( cryptAlgo == CRYPT_ALGO_DH || \
			  cryptAlgo == CRYPT_ALGO_RSA || \
			  cryptAlgo == CRYPT_ALGO_DSA || \
			  cryptAlgo == CRYPT_ALGO_ELGAMAL );
	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	switch( cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
			status = rsaGenerateComponents( keyInfo, keySizeBits );
			break;

#if defined( USE_DH ) || defined( USE_DSA ) || defined( USE_ELGAMAL )
		case CRYPT_ALGO_DH:
		case CRYPT_ALGO_DSA:
		case CRYPT_ALGO_ELGAMAL:
			status = dlpGenerateComponents( keyInfo, keySizeBits, cryptAlgo );
			break;
#endif /* USE_DH || USE_DSA || USE_ELGAMAL */

		default:
			return( CRYPT_ERROR_NOTAVAIL );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Send the public-key data (needed for certificates and the like) to 
	   the context */
	return( setPKCinfo( contextInfoPtr, cryptAlgo, keyInfo ) );
	}

/* Send public-key data to the context for use with certificate objects */

int setPKCinfo( CONTEXT_INFO *contextInfoPtr, 
				const CRYPT_ALGO_TYPE cryptAlgo, const void *keyInfo )
	{
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 4 ) + 8 ];
	MESSAGE_DATA msgData;
	int keyDataSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( cryptAlgo == CRYPT_ALGO_RSA && \
			  isReadPtr( keyInfo, sizeof( CRYPT_PKCINFO_RSA ) ) ) || \
			( cryptAlgo != CRYPT_ALGO_RSA && \
			  isReadPtr( keyInfo, sizeof( CRYPT_PKCINFO_DLP ) ) ) );

	REQUIRES( cryptAlgo == CRYPT_ALGO_DH || \
			  cryptAlgo == CRYPT_ALGO_RSA || \
			  cryptAlgo == CRYPT_ALGO_DSA || \
			  cryptAlgo == CRYPT_ALGO_ELGAMAL );

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all that we're doing here is sending in encoded public key data for 
	   use by objects such as certificates */
	switch( cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
			{
			const CRYPT_PKCINFO_RSA *rsaKeyInfo = \
						( CRYPT_PKCINFO_RSA * ) keyInfo;

			if( rsaKeyInfo->isPublicKey ) 
				contextInfoPtr->flags |= CONTEXT_FLAG_ISPUBLICKEY;
			else
				contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
			status = writeFlatPublicKey( keyDataBuffer, 
										 CRYPT_MAX_PKCSIZE * 4, 
										 &keyDataSize, CRYPT_ALGO_RSA, 
										 rsaKeyInfo->n, 
										 bitsToBytes( rsaKeyInfo->nLen ), 
										 rsaKeyInfo->e, 
										 bitsToBytes( rsaKeyInfo->eLen ), 
										 NULL, 0, NULL, 0 );
			break;
			}

		case CRYPT_ALGO_DH:
		case CRYPT_ALGO_DSA:
		case CRYPT_ALGO_ELGAMAL:
			{
			const CRYPT_PKCINFO_DLP *dlpKeyInfo = \
						( CRYPT_PKCINFO_DLP * ) keyInfo;

			if( dlpKeyInfo->isPublicKey ) 
				contextInfoPtr->flags |= CONTEXT_FLAG_ISPUBLICKEY;
			else
				contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
			status = writeFlatPublicKey( keyDataBuffer, 
										 CRYPT_MAX_PKCSIZE * 4, 
										 &keyDataSize, cryptAlgo, 
										 dlpKeyInfo->p, 
										 bitsToBytes( dlpKeyInfo->pLen ), 
										 dlpKeyInfo->q, 
										 bitsToBytes( dlpKeyInfo->qLen ), 
										 dlpKeyInfo->g, 
										 bitsToBytes( dlpKeyInfo->gLen ), 
										 dlpKeyInfo->y, 
										 bitsToBytes( dlpKeyInfo->yLen ) );
			break;
			}

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, keyDataBuffer, keyDataSize );
	return( krnlSendMessage( contextInfoPtr->objectHandle, 
							 IMESSAGE_SETATTRIBUTE_S, &msgData, 
							 CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL ) );
	}

/* Send encryption/MAC keying metadata to the context */

int setConvInfo( const CRYPT_CONTEXT iCryptContext, const int keySize )
	{
	assert( isHandleRangeValid( iCryptContext ) );

	REQUIRES( keySize >= MIN_KEYSIZE && keySize <= CRYPT_MAX_KEYSIZE );

	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, 
							 ( MESSAGE_CAST ) &keySize, 
							 CRYPT_IATTRIBUTE_KEYSIZE ) );
	}

/* The default cleanup function, which simply frees the context-related data 
   used by the cryptographic hardware if it's an ephemeral key */

int cleanupHardwareContext( const CONTEXT_INFO *contextInfoPtr )
	{
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* If this is non-ephemeral context data then we leave it intact when 
	   the corresponding context is destroyed, the only way to delete it is 
	   with an explicit deleteItem() */
	if( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT )
		return( CRYPT_OK );

	/* We have to be careful about deleting the context data since it may
	   not have been set up yet, for example if we're called due to a 
	   failure in the complete-creation/initialisation step of setting up a 
	   context */
	if( contextInfoPtr->deviceObject != CRYPT_ERROR )
		hwDeleteItem( contextInfoPtr->deviceObject );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						 	Device Access Routines							*
*																			*
****************************************************************************/

/* Mechanisms supported by the hardware.  These are actually cryptlib native 
   mechanisms, but not the full set supported by the system device since 
   functions like private key export aren't available.  The  list is sorted 
   in order of frequency of use in order to make lookups a bit faster */

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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setDeviceHardware( INOUT DEVICE_INFO *deviceInfo )
	{
	assert( isWritePtr( deviceInfo, sizeof( DEVICE_INFO ) ) );

	deviceInfo->initFunction = initFunction;
	deviceInfo->shutdownFunction = shutdownFunction;
	deviceInfo->controlFunction = controlFunction;
	deviceInfo->getItemFunction = getItemFunction;
	deviceInfo->setItemFunction = setItemFunction;
	deviceInfo->deleteItemFunction = deleteItemFunction;
	deviceInfo->getFirstItemFunction = getFirstItemFunction;
	deviceInfo->getNextItemFunction = getNextItemFunction;
	deviceInfo->getRandomFunction = getRandomFunction;
	deviceInfo->capabilityInfoList = capabilityInfoList;
	deviceInfo->mechanismFunctions = mechanismFunctions;
	deviceInfo->mechanismFunctionCount = \
		FAILSAFE_ARRAYSIZE( mechanismFunctions, MECHANISM_FUNCTION_INFO );

	return( CRYPT_OK );
	}
#endif /* USE_HARDWARE */
