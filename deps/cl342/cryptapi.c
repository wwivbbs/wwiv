/****************************************************************************
*																			*
*						 cryptlib External API Interface					*
*						Copyright Peter Gutmann 1997-2013					*
*																			*
****************************************************************************/

/* NSA motto: In God we trust... all others we monitor.
														-- Stanley Miller */
#include "crypt.h"
#if defined( INC_ALL )
  #include "rpc.h"
#else
  #include "misc/rpc.h"
#endif /* Compiler-specific includes */

/* Handlers for the various commands */

#ifdef USE_CERTIFICATES

static int cmdCertCheck( COMMAND_INFO *cmd )
	{
	assert( cmd->type == COMMAND_CERTCHECK );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( !isHandleRangeValid( cmd->arg[ 1 ] ) && \
		( cmd->arg[ 1 ] != CRYPT_UNUSED ) )
		return( CRYPT_ARGERROR_NUM1 );

	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_CRT_SIGCHECK, NULL,
							 cmd->arg[ 1 ] ) );
	}

#ifdef USE_KEYSETS

static int cmdCertMgmt( COMMAND_INFO *cmd )
	{
	MESSAGE_CERTMGMT_INFO certMgmtInfo;
	int status;

	assert( cmd->type == COMMAND_CERTMGMT );
	assert( cmd->flags == COMMAND_FLAG_NONE || \
			cmd->flags == COMMAND_FLAG_RET_NONE );
	assert( cmd->noArgs == 4 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] < CRYPT_CERTACTION_FIRST_USER || \
		cmd->arg[ 1 ] > CRYPT_CERTACTION_LAST_USER )
		return( CRYPT_ARGERROR_VALUE );
	if( !isHandleRangeValid( cmd->arg[ 2 ] ) && \
		!( ( cmd->arg[ 1 ] == CRYPT_CERTACTION_EXPIRE_CERT || \
			 cmd->arg[ 1 ] == CRYPT_CERTACTION_CLEANUP ) && \
		   cmd->arg[ 2 ] == CRYPT_UNUSED ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( !isHandleRangeValid( cmd->arg[ 3 ] ) && \
		!( ( cmd->arg[ 1 ] == CRYPT_CERTACTION_ISSUE_CRL || \
			 cmd->arg[ 1 ] == CRYPT_CERTACTION_EXPIRE_CERT || \
			 cmd->arg[ 1 ] == CRYPT_CERTACTION_CLEANUP ) && \
		   cmd->arg[ 3 ] == CRYPT_UNUSED ) )
		return( CRYPT_ARGERROR_NUM2 );

	setMessageCertMgmtInfo( &certMgmtInfo, cmd->arg[ 2 ], cmd->arg[ 3 ] );
	if( cmd->flags == COMMAND_FLAG_RET_NONE )
		{
		/* If we aren't interested in the return value, set the crypt handle 
		   to CRYPT_UNUSED to indicate that there's no need to return the 
		   created certificate object */
		certMgmtInfo.cryptCert = CRYPT_UNUSED;
		}
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_KEY_CERTMGMT,
							  &certMgmtInfo, cmd->arg[ 1 ] );
	if( cryptStatusOK( status ) && cmd->flags != COMMAND_FLAG_RET_NONE )
		cmd->arg[ 0 ] = certMgmtInfo.cryptCert;
	return( status );
	}
#endif /* USE_KEYSETS */

static int cmdCertSign( COMMAND_INFO *cmd )
	{
	assert( cmd->type == COMMAND_CERTSIGN );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( !isHandleRangeValid( cmd->arg[ 1 ] ) )
		return( CRYPT_ARGERROR_NUM1 );

	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_CRT_SIGN, NULL,
							 cmd->arg[ 1 ] ) );
	}
#endif /* USE_CERTIFICATES */

static int cmdCreateObject( COMMAND_INFO *cmd )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BOOLEAN bindToOwner = FALSE, hasStrArg = FALSE;
	int owner DUMMY_INIT, status;

	assert( cmd->type == COMMAND_CREATEOBJECT );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs >= 2 && cmd->noArgs <= 4 );
	assert( cmd->noStrArgs >= 0 && cmd->noStrArgs <= 2 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) && \
		cmd->arg[ 0 ] != SYSTEM_OBJECT_HANDLE )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] <= OBJECT_TYPE_NONE || \
		cmd->arg[ 1 ] >= OBJECT_TYPE_LAST )
		return( CRYPT_ERROR_FAILED );	/* Internal error */
	switch( cmd->arg[ 1 ] )
		{
		case OBJECT_TYPE_CONTEXT:
			assert( cmd->noArgs == 3 );
			assert( cmd->noStrArgs == 0 );
			if( cmd->arg[ 2 ] <= CRYPT_ALGO_NONE || \
				cmd->arg[ 2 ] >= CRYPT_ALGO_LAST_EXTERNAL )
				return( CRYPT_ARGERROR_NUM1 );
			break;

		case OBJECT_TYPE_CERTIFICATE:
			assert( cmd->noArgs == 3 );
			assert( cmd->noStrArgs == 0 );
			if( cmd->arg[ 2 ] <= CRYPT_CERTTYPE_NONE || \
				cmd->arg[ 2 ] >= CRYPT_CERTTYPE_LAST_EXTERNAL )
				return( CRYPT_ARGERROR_NUM1 );
			break;

		case OBJECT_TYPE_DEVICE:
			assert( cmd->noArgs == 3 );
			assert( cmd->noStrArgs == 1 );
			if( cmd->arg[ 2 ] <= CRYPT_DEVICE_NONE || \
				cmd->arg[ 2 ] >= CRYPT_DEVICE_LAST )
				return( CRYPT_ARGERROR_NUM1 );
			if( cmd->arg[ 2 ] == CRYPT_DEVICE_PKCS11 || \
				cmd->arg[ 2 ] == CRYPT_DEVICE_CRYPTOAPI )
				{
				if( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
					cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE )
					return( CRYPT_ARGERROR_STR1 );
				hasStrArg = TRUE;
				}
			break;

		case OBJECT_TYPE_KEYSET:
			assert( cmd->noArgs == 4 );
			assert( cmd->noStrArgs >= 0 && cmd->noStrArgs <= 1 );
			if( cmd->arg[ 2 ] <= CRYPT_KEYSET_NONE || \
				cmd->arg[ 2 ] >= CRYPT_KEYSET_LAST )
				return( CRYPT_ARGERROR_NUM1 );
			if( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
				cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE )
				return( CRYPT_ARGERROR_STR1 );
			if( cmd->arg[ 3 ] < CRYPT_KEYOPT_NONE || \
				cmd->arg[ 3 ] >= CRYPT_KEYOPT_LAST_EXTERNAL )
				{
				/* CRYPT_KEYOPT_NONE is a valid setting for this parameter */
				return( CRYPT_ARGERROR_NUM2 );
				}
			hasStrArg = TRUE;
			break;

		case OBJECT_TYPE_ENVELOPE:
			assert( cmd->noArgs == 3 );
			assert( cmd->noStrArgs == 0 );
			if( cmd->arg[ 2 ] <= CRYPT_FORMAT_NONE || \
				cmd->arg[ 2 ] >= CRYPT_FORMAT_LAST_EXTERNAL )
				return( CRYPT_ARGERROR_NUM1 );
			break;

		case OBJECT_TYPE_SESSION:
			assert( cmd->noArgs == 3 );
			assert( cmd->noStrArgs == 0 );
			if( cmd->arg[ 2 ] <= CRYPT_SESSION_NONE || \
				cmd->arg[ 2 ] >= CRYPT_SESSION_LAST )
				return( CRYPT_ARGERROR_NUM1 );
			break;

		case OBJECT_TYPE_USER:
			assert( cmd->noArgs == 2 );
			assert( cmd->noStrArgs == 2 );
			if( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
				cmd->strArgLen[ 0 ] >= CRYPT_MAX_TEXTSIZE )
				return( CRYPT_ARGERROR_STR1 );
			if( cmd->strArgLen[ 1 ] < MIN_NAME_LENGTH || \
				cmd->strArgLen[ 1 ] >= CRYPT_MAX_TEXTSIZE )
				return( CRYPT_ARGERROR_STR2 );
			hasStrArg = TRUE;
			break;

		default:
			retIntError();
		}

	/* If we're creating the object via a device, we should set the new 
	   object owner to the device owner */
	if( cmd->arg[ 0 ] != SYSTEM_OBJECT_HANDLE )
		{
		bindToOwner = TRUE;
		owner = cmd->arg[ 0 ];
		}

	/* Create the object via the device.  Since we're usually doing this via 
	   the system object which is invisible to the user, we have to use an 
	   internal message for this one case */
	setMessageCreateObjectInfo( &createInfo, cmd->arg[ 2 ] );
	if( cmd->noArgs == 4 )
		createInfo.arg2 = cmd->arg[ 3 ];
	if( hasStrArg )
		{
		createInfo.strArg1 = cmd->strArg[ 0 ];
		createInfo.strArgLen1 = cmd->strArgLen[ 0 ];
		if( cmd->noStrArgs > 1 )
			{
			createInfo.strArg2 = cmd->strArg[ 1 ];
			createInfo.strArgLen2 = cmd->strArgLen[ 1 ];
			}
		}
	if( cmd->arg[ 0 ] == SYSTEM_OBJECT_HANDLE )
		{
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  cmd->arg[ 1 ] );
		}
	else
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_DEV_CREATEOBJECT,
								  &createInfo, cmd->arg[ 1 ] );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If the device used to create the object is bound to a thread, bind 
	   the created object to the thread as well.  If this fails, we don't 
	   return the object to the caller since it would be returned in a 
	   potentially unbound state */
	if( bindToOwner )
		{
		int ownerID;

		status = krnlSendMessage( owner, IMESSAGE_GETATTRIBUTE, &ownerID,
								  CRYPT_PROPERTY_OWNER );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( createInfo.cryptHandle,
									  IMESSAGE_SETATTRIBUTE, &ownerID,
									  CRYPT_PROPERTY_OWNER );
		if( cryptStatusError( status ) && status != CRYPT_ERROR_NOTINITED )
			{
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Make the newly-created object externally visible if necessary.  This
	   is only required when we're creating the object via the system
	   handle, which requires an internal message that leaves the object
	   internal */
	if( cmd->arg[ 0 ] == SYSTEM_OBJECT_HANDLE )
		{
		krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
						 MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_INTERNAL );
		}
	cmd->arg[ 0 ] = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

#ifdef USE_CERTIFICATES 

static int cmdCreateObjectIndirect( COMMAND_INFO *cmd )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( cmd->type == COMMAND_CREATEOBJECT_INDIRECT );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	if( cmd->arg[ 0 ] != SYSTEM_OBJECT_HANDLE )
		return( CRYPT_ERROR_FAILED );	/* Internal error */
	if( cmd->arg[ 1 ] != OBJECT_TYPE_CERTIFICATE )
		return( CRYPT_ERROR_FAILED );	/* Internal error */
	if( cmd->strArgLen[ 0 ] < MIN_CERTSIZE || \
		cmd->strArgLen[ 0 ] >= MAX_BUFFER_SIZE )
		return( CRYPT_ARGERROR_STR1 );

	/* Create the object via the device.  Since we're usually doing this via 
	   the system object which is invisible to the user, we have to use an 
	   internal message for this one case */
	setMessageCreateObjectIndirectInfo( &createInfo, cmd->strArg[ 0 ],
										cmd->strArgLen[ 0 ],
										CRYPT_CERTTYPE_NONE );
	if( cmd->arg[ 0 ] == SYSTEM_OBJECT_HANDLE )
		{
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		}
	else
		{
		status = krnlSendMessage( cmd->arg[ 0 ],
								  MESSAGE_DEV_CREATEOBJECT_INDIRECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Make the newly-created object externally visible */
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
					 MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_INTERNAL );
	cmd->arg[ 0 ] = createInfo.cryptHandle;
	return( status );
	}
#endif /* USE_CERTIFICATES */

static int cmdDecrypt( COMMAND_INFO *cmd )
	{
	int algorithm, mode = CRYPT_MODE_NONE, status;	/* int vs.enum */

	assert( cmd->type == COMMAND_DECRYPT );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( algorithm <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
								  &mode, CRYPT_CTXINFO_MODE );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		if( algorithm <= CRYPT_ALGO_LAST_PKC )
			{
			int blockSize;

			status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
									  &blockSize, CRYPT_CTXINFO_KEYSIZE );
			if( cryptStatusOK( status ) && cmd->strArgLen[ 0 ] != blockSize )
				status = CRYPT_ARGERROR_NUM1;
			if( cryptStatusError( status ) )
				return( status );
			}
		else
			{
			/* We shouldn't be invoking decrypt on a hash or MAC object */
			return( CRYPT_ARGERROR_OBJECT );
			}
		}
	if( cmd->strArgLen[ 0 ] <= 0 )
		return( CRYPT_ARGERROR_NUM1 );
	if( mode == CRYPT_MODE_ECB || mode == CRYPT_MODE_CBC )
		{
		int blockSize;

		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusOK( status ) && cmd->strArgLen[ 0 ] % blockSize )
			status = CRYPT_ARGERROR_NUM1;
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that the IV has been set */
	if( needsIV( mode ) && !isStreamCipher( algorithm ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusError( status ) )
			return( status );
		}

	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_CTX_DECRYPT,
							  cmd->strArgLen[ 0 ] ? cmd->strArg[ 0 ] : "",
							  cmd->strArgLen[ 0 ] );
	return( status );
	}

static int cmdDeleteAttribute( COMMAND_INFO *cmd )
	{
	assert( cmd->type == COMMAND_DELETEATTRIBUTE );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) && \
		cmd->arg[ 0 ] != DEFAULTUSER_OBJECT_HANDLE )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		if( cmd->arg[ 1 ] <= CRYPT_OPTION_FIRST || \
			cmd->arg[ 1 ] >= CRYPT_OPTION_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		if( cmd->arg[ 1 ] <= CRYPT_ATTRIBUTE_NONE || \
			cmd->arg[ 1 ] >= CRYPT_ATTRIBUTE_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}

	/* Delete the attribute.  Since we're usually doing this via the default
	   user object which is invisible to the user, we have to use an internal
	   message for this one case */
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		return( krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
								 IMESSAGE_DELETEATTRIBUTE, NULL,
								 cmd->arg[ 1 ] ) );
		}
	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_DELETEATTRIBUTE, NULL,
							 cmd->arg[ 1 ] ) );
	}

#ifdef USE_KEYSETS

static int cmdDeleteKey( COMMAND_INFO *cmd )
	{
	MESSAGE_KEYMGMT_INFO deletekeyInfo;
	int itemType = KEYMGMT_ITEM_PUBLICKEY;

	assert( cmd->type == COMMAND_DELETEKEY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs >= 2 && cmd->noArgs <= 3 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] <= CRYPT_KEYID_NONE || \
		cmd->arg[ 1 ] >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ARGERROR_NUM1 );
	if( cmd->arg[ 2 ] )
		{
		/* It's a special-case object being fetched from a CA store */
		if( cmd->arg[ 2 ] == CRYPT_CERTTYPE_REQUEST_CERT )
			itemType = KEYMGMT_ITEM_REQUEST;
		else
			{
			if( cmd->arg[ 2 ] == CRYPT_CERTTYPE_PKIUSER )
				itemType = KEYMGMT_ITEM_PKIUSER;
			else
				return( CRYPT_ARGERROR_NUM2 );
			}
		}
	if( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
		cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ARGERROR_STR1 );

	/* Delete the key from the keyset.  Unless the user has explicitly
	   specified a CA item to delete, we set the item type to delete to
	   public-key since private-key keysets will interpret this correctly
	   to mean they should also delete the associated private key */
	setMessageKeymgmtInfo( &deletekeyInfo, cmd->arg[ 1 ], cmd->strArg[ 0 ],
						   cmd->strArgLen[ 0 ], NULL, 0, KEYMGMT_FLAG_NONE );
	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_KEY_DELETEKEY,
							 &deletekeyInfo, itemType ) );
	}
#endif /* USE_KEYSETS */

static int cmdDestroyObject( COMMAND_INFO *cmd )
	{
	assert( cmd->type == COMMAND_DESTROYOBJECT );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );

	/* Decrement the object's reference count, which may or may not actually
	   destroy it (the kernel marks it as internal so it appears destroyed
	   to the caller) */
	return( krnlSendNotifier( cmd->arg[ 0 ], MESSAGE_DECREFCOUNT ) );
	}

static int cmdEncrypt( COMMAND_INFO *cmd )
	{
	int algorithm, mode = CRYPT_MODE_NONE, status;	/* int vs.enum */

	assert( cmd->type == COMMAND_ENCRYPT );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( algorithm <= CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
								  &mode, CRYPT_CTXINFO_MODE );
		if( cryptStatusError( status ) )
				return( status );
		}
	else
		{
		if( algorithm <= CRYPT_ALGO_LAST_PKC )
			{
			int blockSize;

			status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
									  &blockSize, CRYPT_CTXINFO_KEYSIZE );
			if( cryptStatusOK( status ) && cmd->strArgLen[ 0 ] != blockSize )
				status = CRYPT_ARGERROR_NUM1;
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	if( isHashAlgo( algorithm ) || isMacAlgo( algorithm ) )
		{
		/* For hash and MAC operations a length of zero is valid since this
		   is an indication to wrap up the hash operation */
		if( cmd->strArgLen[ 0 ] < 0 )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		if( cmd->strArgLen[ 0 ] <= 0 )
			return( CRYPT_ARGERROR_NUM1 );
		}
	if( mode == CRYPT_MODE_ECB || mode == CRYPT_MODE_CBC )
		{
		int blockSize;

		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusOK( status ) && cmd->strArgLen[ 0 ] % blockSize )
			status = CRYPT_ARGERROR_NUM1;
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's no IV set, generate one ourselves */
	if( needsIV( mode ) && !isStreamCipher( algorithm ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusError( status ) )
			{
			if( status == CRYPT_ERROR_NOTINITED )
				krnlSendNotifier( cmd->arg[ 0 ], MESSAGE_CTX_GENIV );
			else
				return( status );
			}
		}

	status = krnlSendMessage( cmd->arg[ 0 ],
							  ( isHashAlgo( algorithm ) || \
								isMacAlgo( algorithm ) ) ? \
								MESSAGE_CTX_HASH : MESSAGE_CTX_ENCRYPT,
							  cmd->strArgLen[ 0 ] ? cmd->strArg[ 0 ] : "",
							  cmd->strArgLen[ 0 ] );
	if( isHashAlgo( algorithm ) || isMacAlgo( algorithm ) )
		{
		/* There's no data to return since the hashing doesn't change it */
		cmd->strArgLen[ 0 ] = 0;
		}
	return( status );
	}

#ifdef USE_CERTIFICATES

static int cmdExportObject( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( cmd->type == COMMAND_EXPORTOBJECT );
	assert( cmd->flags == COMMAND_FLAG_NONE || \
			cmd->flags == COMMAND_FLAG_RET_LENGTH );
	assert( cmd->noArgs == 2 );
	assert( ( cmd->flags == COMMAND_FLAG_NONE && cmd->noStrArgs == 1 ) || \
			( cmd->flags == COMMAND_FLAG_RET_LENGTH && cmd->noStrArgs == 0 ) );
	assert( cmd->flags == COMMAND_FLAG_RET_LENGTH || \
			cmd->strArg[ 0 ] != NULL );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] <= CRYPT_CERTFORMAT_NONE || \
		cmd->arg[ 1 ] >= CRYPT_CERTFORMAT_LAST_EXTERNAL )
		{
		/* At the moment the only object that we can export is a certificate, 
		   so we make sure that the format type is valid for this */
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Export the certificate */
	if( cmd->flags == COMMAND_FLAG_RET_LENGTH )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_CRT_EXPORT,
								  &msgData, cmd->arg[ 1 ] );
		if( cryptStatusOK( status ) )
			cmd->arg[ 0 ] = msgData.length;
		}
	else
		{
		setMessageData( &msgData, cmd->strArg[ 0 ], cmd->strArgLen[ 0 ] );
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_CRT_EXPORT,
								  &msgData, cmd->arg[ 1 ] );
		if( cryptStatusOK( status ) )
			cmd->strArgLen[ 0 ] = msgData.length;
		}

	/* If we try and export using a disallowed format (e.g. a
	   CRYPT_CERTFORMAT_CERTCHAIN from a certificate request) we'll get an 
	   argument value error that we need to convert into something more 
	   sensible.  The error type to report is somewhat debatable since 
	   either the format type or the object can be regarded as being wrong, 
	   for example when exporting a certificate request as a certificate 
	   chain the format is wrong but when exporting a data-only object as 
	   anything the object is wrong.  To handle this, we report an argument 
	   value error as a numeric parameter error for cases where the format 
	   is incorrect for the object type, and a permission error for cases 
	   where the object can't be exported externally */
	if( status == CRYPT_ARGERROR_VALUE )
		{
		int type;

		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE, &type,
								  CRYPT_CERTINFO_CERTTYPE );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_OBJECT );
		status = ( type == CRYPT_CERTTYPE_CERTIFICATE || \
				   type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
				   type == CRYPT_CERTTYPE_CERTCHAIN || \
				   type == CRYPT_CERTTYPE_CERTREQUEST || \
				   type == CRYPT_CERTTYPE_CRL ) ? \
				 CRYPT_ARGERROR_NUM1 : CRYPT_ERROR_PERMISSION;
		}

	return( status );
	}
#endif /* USE_CERTIFICATES */

#if defined( USE_ENVELOPES ) || defined( USE_SESSIONS )

static int cmdFlushData( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( cmd->type == COMMAND_FLUSHDATA );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );

	/* Send the flush data command to the object */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_ENV_PUSHDATA,
							  &msgData, 0 );
	return( status );
	}
#endif /* USE_ENVELOPES || USE_SESSIONS */

static int cmdGenKey( COMMAND_INFO *cmd )
	{
	assert( cmd->type == COMMAND_GENKEY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 0 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );

	return( krnlSendNotifier( cmd->arg[ 0 ], MESSAGE_CTX_GENKEY ) );
	}

static int cmdGetAttribute( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( cmd->type == COMMAND_GETATTRIBUTE );
	assert( cmd->flags == COMMAND_FLAG_NONE || \
			cmd->flags == COMMAND_FLAG_RET_LENGTH );
	assert( cmd->noArgs >= 2 && cmd->noArgs <= 3 );
	assert( ( cmd->flags == COMMAND_FLAG_NONE && cmd->noStrArgs == 1 ) || \
			( ( cmd->noArgs == 2 || cmd->flags == COMMAND_FLAG_RET_LENGTH ) && \
				cmd->noStrArgs == 0 ) );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) && \
		cmd->arg[ 0 ] != DEFAULTUSER_OBJECT_HANDLE )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		if( cmd->arg[ 1 ] <= CRYPT_OPTION_FIRST || \
			cmd->arg[ 1 ] >= CRYPT_OPTION_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		if( cmd->arg[ 1 ] <= CRYPT_ATTRIBUTE_NONE || \
			cmd->arg[ 1 ] >= CRYPT_ATTRIBUTE_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}

	/* Get the attribute data from the object.  If it's a config option,
	   we're usually doing this via the default user object which is
	   invisible to the user, so we have to use an internal message for this
	   one case.

	   This is further complicated by the fact that the kernel checks that
	   the destination memory is writable and either returns an error (for
	   an external message) or throws an exception (for the internal message
	   required to access the user object) if it isn't.  Since the external
	   API doesn't allow the specification of the returned data length, it
	   uses a worst-case estimate which may be much larger than the actual
	   buffer size, which the kernel will refuse to write to.  To handle
	   this we first read the actual length and then ask for only that much
	   data, which the caller should have made available for the output */
	if( cmd->noArgs == 2 )
		{
		if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
			{
			return( krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
									 IMESSAGE_GETATTRIBUTE, &cmd->arg[ 0 ],
									 cmd->arg[ 1 ] ) );
			}
		return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE,
								 &cmd->arg[ 0 ], cmd->arg[ 1 ] ) );
		}
	setMessageData( &msgData, NULL, 0 );
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  cmd->arg[ 1 ] );
		}
	else
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE_S,
								  &msgData, cmd->arg[ 1 ] );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( cmd->flags == COMMAND_FLAG_RET_LENGTH )
		{
		cmd->arg[ 0 ] = msgData.length;
		return( CRYPT_OK );
		}
	msgData.data = cmd->strArg[ 0 ];
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  cmd->arg[ 1 ] );
		}
	else
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE_S,
								  &msgData, cmd->arg[ 1 ] );
		}
	if( cryptStatusOK( status ) )
		cmd->strArgLen[ 0 ] = msgData.length;
	return( status );
	}

#ifdef USE_KEYSETS

static int cmdGetKey( COMMAND_INFO *cmd )
	{
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	int messageType = ( cmd->arg[ 2 ] == CRYPT_KEYID_NONE ) ? \
				MESSAGE_KEY_GETNEXTCERT : MESSAGE_KEY_GETKEY;
	int owner, status;

	assert( cmd->type == COMMAND_GETKEY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 3 );
	assert( cmd->noStrArgs >= 1 && cmd->noStrArgs <= 2 );

	/* Perform basic server-side error checking.  Because of keyset queries
	   we have to accept CRYPT_KEYID_NONE as well as obviously valid key
	   IDs.  In addition if we find a missing ID we pass the request in as
	   a keyset query (this is matched to an implicit GetFirstCert performed
	   by setting the query attribute, this isn't really possible using the
	   external API) */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] <= KEYMGMT_ITEM_NONE || \
		cmd->arg[ 1 ] >= KEYMGMT_ITEM_REVOCATIONINFO )
		{
		/* Item can only be a public key, private key, secret key, CA 
		   request, or CA PKI user info */
		return( CRYPT_ARGERROR_NUM1 );
		}
	if( cmd->arg[ 2 ] < CRYPT_KEYID_NONE || \
		cmd->arg[ 2 ] >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ARGERROR_NUM2 );
	if( cmd->arg[ 2 ] == CRYPT_KEYID_NONE )
		{
		if( cmd->arg[ 1 ] != KEYMGMT_ITEM_PUBLICKEY )
			{
			/* If we're doing a keyset query, it has to be for a 
			   certificate */
			return( CRYPT_ARGERROR_NUM1 );
			}
		if( cmd->strArgLen[ 0 ] )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		if( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
			cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Read the key from the keyset */
	setMessageKeymgmtInfo( &getkeyInfo, cmd->arg[ 2 ],
						   cmd->strArgLen[ 0 ] ? cmd->strArg[ 0 ] : NULL,
							cmd->strArgLen[ 0 ],
						   cmd->strArgLen[ 1 ] ? cmd->strArg[ 1 ] : NULL,
							cmd->strArgLen[ 1 ], KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( cmd->arg[ 0 ], messageType, &getkeyInfo,
							  cmd->arg[ 1 ] );
	if( cryptStatusError( status ) )
		return( status );

	/* If the keyset is bound to a thread, bind the key read from it to the
	   thread as well.  If this fails, we don't return the imported key to
	   the caller since it would be returned in a potentially unbound state */
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_GETATTRIBUTE, &owner,
							  CRYPT_PROPERTY_OWNER );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( getkeyInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE, &owner,
								  CRYPT_PROPERTY_OWNER );
		}
	if( cryptStatusError( status ) && status != CRYPT_ERROR_NOTINITED )
		{
		krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	cmd->arg[ 0 ] = getkeyInfo.cryptHandle;
	return( CRYPT_OK );
	}
#endif /* USE_KEYSETS */

#if defined( USE_ENVELOPES ) || defined( USE_SESSIONS )

static int cmdPopData( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( cmd->type == COMMAND_POPDATA );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 2 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] < 1 )
		return( CRYPT_ARGERROR_NUM1 );

	/* Get the data from the object.  We always copy out the byte count value
	   because it's valid even if an error occurs */
	setMessageData( &msgData, cmd->strArg[ 0 ], cmd->arg[ 1 ] );
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_ENV_POPDATA,
							  &msgData, 0 );
	cmd->strArgLen[ 0 ] = msgData.length;
	return( status );
	}

static int cmdPushData( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( cmd->type == COMMAND_PUSHDATA );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 1 );
	assert( cmd->noStrArgs == 1 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->strArgLen[ 0 ] < 0 )
		return( CRYPT_ARGERROR_NUM1 );

	/* Send the data to the object.  We always copy out the byte count value
	   because it's valid even if an error occurs */
	setMessageData( &msgData, cmd->strArgLen[ 0 ] ? cmd->strArg[ 0 ] : NULL,
					cmd->strArgLen[ 0 ] );
	status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_ENV_PUSHDATA, &msgData,
							  0 );
	cmd->arg[ 0 ] = msgData.length;
	return( status );
	}
#endif /* USE_ENVELOPES || USE_SESSIONS */

static int cmdQueryCapability( COMMAND_INFO *cmd )
	{
	CRYPT_QUERY_INFO queryInfo;
	int status;

	assert( cmd->type == COMMAND_QUERYCAPABILITY );
	assert( cmd->flags == COMMAND_FLAG_NONE || \
			cmd->flags == COMMAND_FLAG_RET_LENGTH );
	assert( cmd->noArgs == 2 );
	assert( ( cmd->flags == COMMAND_FLAG_NONE && cmd->noStrArgs == 1 ) || \
			( cmd->flags == COMMAND_FLAG_RET_LENGTH && cmd->noStrArgs == 0 ) );
	assert( cmd->flags == COMMAND_FLAG_RET_LENGTH || \
			cmd->strArg[ 0 ] != NULL );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) && \
		cmd->arg[ 0 ] != SYSTEM_OBJECT_HANDLE )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 1 ] < CRYPT_ALGO_NONE || \
		cmd->arg[ 1 ] >= CRYPT_ALGO_LAST_EXTERNAL )
		return( CRYPT_ARGERROR_NUM1 );

	/* Query the device for information on the given algorithm and mode.
	   Since we're usually doing this via the system object which is
	   invisible to the user, we have to use an internal message for this
	   one case */
	if( cmd->arg[ 0 ] == SYSTEM_OBJECT_HANDLE )
		{
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_QUERYCAPABILITY, &queryInfo,
								  cmd->arg[ 1 ] );
		}
	else
		{
		status = krnlSendMessage( cmd->arg[ 0 ], MESSAGE_DEV_QUERYCAPABILITY,
								  &queryInfo, cmd->arg[ 1 ] );
		}
	if( cryptStatusOK( status ) )
		{
		/* Return either the length or the full capability into on what the
		   caller has asked for */
		if( cmd->flags == COMMAND_FLAG_RET_LENGTH )
			cmd->arg[ 0 ] = sizeof( CRYPT_QUERY_INFO );
		else
			{
			memcpy( cmd->strArg[ 0 ], &queryInfo,
					sizeof( CRYPT_QUERY_INFO ) );
			cmd->strArgLen[ 0 ] = sizeof( CRYPT_QUERY_INFO );
			}
		}

	return( status );
	}

#ifdef USE_RPCAPI

static int cmdServerQuery( COMMAND_INFO *cmd )
	{
	int value;

	assert( cmd->type == COMMAND_SERVERQUERY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs == 0 );
	assert( cmd->noStrArgs == 0 );

	/* Return information about the server */
	krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE,
					 &value, CRYPT_OPTION_INFO_MAJORVERSION );
	krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE,
					 &value, CRYPT_OPTION_INFO_MINORVERSION );

	return( CRYPT_OK );
	}
#endif /* USE_RPCAPI */

static int cmdSetAttribute( COMMAND_INFO *cmd )
	{
	MESSAGE_DATA msgData;

	assert( cmd->type == COMMAND_SETATTRIBUTE );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( ( cmd->noArgs == 3 && cmd->noStrArgs == 0 ) ||
			( cmd->noArgs == 2 && cmd->noStrArgs == 1 ) );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) && \
		cmd->arg[ 0 ] != DEFAULTUSER_OBJECT_HANDLE )
		return( CRYPT_ARGERROR_OBJECT );
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		if( cmd->arg[ 1 ] <= CRYPT_OPTION_FIRST || \
			cmd->arg[ 1 ] >= CRYPT_OPTION_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		if( cmd->arg[ 1 ] <= CRYPT_ATTRIBUTE_NONE || \
			cmd->arg[ 1 ] >= CRYPT_ATTRIBUTE_LAST )
			return( CRYPT_ARGERROR_NUM1 );
		}
	if( cmd->noStrArgs == 1 )
		{
		if( cmd->arg[ 1 ] == CRYPT_CTXINFO_KEY_COMPONENTS )
			{
			/* Public key components constitute a special case since the
			   composite structures used are quite large */
			if( cmd->strArgLen[ 0 ] != sizeof( CRYPT_PKCINFO_RSA ) && \
				cmd->strArgLen[ 0 ] != sizeof( CRYPT_PKCINFO_DLP ) && \
				cmd->strArgLen[ 0 ] != sizeof( CRYPT_PKCINFO_ECC ) )
				return( CRYPT_ARGERROR_NUM2 );
			}
		else
			{
			if( cmd->strArgLen[ 0 ] < 1 || \
				cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE )
				return( CRYPT_ARGERROR_NUM2 );
			}
		}

	/* Send the attribute data to the object, mapping the return code to the
	   correct value if necessary.  If it's a config option, we're usually
	   doing this via the default user object which is invisible to the user,
	   so we have to use an internal message for this one case */
	if( cmd->noStrArgs == 0 )
		{
		if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
			{
			return( krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
									 IMESSAGE_SETATTRIBUTE,
									 ( MESSAGE_CAST ) &cmd->arg[ 2 ],
									 cmd->arg[ 1 ] ) );
			}
		return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_SETATTRIBUTE,
								 ( MESSAGE_CAST ) &cmd->arg[ 2 ], cmd->arg[ 1 ] ) );
		}
	setMessageData( &msgData, cmd->strArg[ 0 ], cmd->strArgLen[ 0 ] );
	if( cmd->arg[ 0 ] == DEFAULTUSER_OBJECT_HANDLE )
		{
		return( krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
								 IMESSAGE_SETATTRIBUTE_S, &msgData,
								 cmd->arg[ 1 ] ) );
		}
	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_SETATTRIBUTE_S,
							 &msgData, cmd->arg[ 1 ] ) );
	}

#ifdef USE_KEYSETS

static int cmdSetKey( COMMAND_INFO *cmd )
	{
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	int itemType = ( cmd->noStrArgs == 1 ) ? \
				   KEYMGMT_ITEM_PRIVATEKEY : KEYMGMT_ITEM_PUBLICKEY;

	assert( cmd->type == COMMAND_SETKEY );
	assert( cmd->flags == COMMAND_FLAG_NONE );
	assert( cmd->noArgs >= 2 && cmd->noArgs <= 3 );
	assert( cmd->noStrArgs >= 0 && cmd->noStrArgs <= 1 );

	/* Perform basic server-side error checking */
	if( !isHandleRangeValid( cmd->arg[ 0 ] ) )
		return( CRYPT_ARGERROR_OBJECT );
	if( !isHandleRangeValid( cmd->arg[ 1 ] ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( cmd->noStrArgs == 1 && \
		( cmd->strArgLen[ 0 ] < MIN_NAME_LENGTH || \
		  cmd->strArgLen[ 0 ] >= MAX_ATTRIBUTE_SIZE ) )
		return( CRYPT_ARGERROR_STR1 );
	if( cmd->arg[ 2 ] )
		{
		int value;

		/* It's a certificate management item request being added to a CA 
		   store, usually this is a request but it may also be PKI user 
		   info */
		itemType = KEYMGMT_ITEM_REQUEST;
		if( cryptStatusOK( krnlSendMessage( cmd->arg[ 1 ],
									MESSAGE_GETATTRIBUTE, &value,
									CRYPT_CERTINFO_CERTTYPE ) ) && \
			value == CRYPT_CERTTYPE_PKIUSER )
			itemType = KEYMGMT_ITEM_PKIUSER;
		}
	else
		{
		int value;

		/* If we're adding a CRL, add it as revocation information rather
		   than as a generic public-key object */
		if( itemType != KEYMGMT_ITEM_PRIVATEKEY && \
			cryptStatusOK( krnlSendMessage( cmd->arg[ 1 ],
									MESSAGE_GETATTRIBUTE, &value,
									CRYPT_CERTINFO_CERTTYPE ) ) && \
			value == CRYPT_CERTTYPE_CRL )
			itemType = KEYMGMT_ITEM_REVOCATIONINFO;
		}

	/* Add the key */
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
						   ( cmd->noStrArgs == 1 ) ? cmd->strArg[ 0 ] : NULL,
						   cmd->strArgLen[ 0 ], KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = cmd->arg[ 1 ];
	return( krnlSendMessage( cmd->arg[ 0 ], MESSAGE_KEY_SETKEY,
							 &setkeyInfo, itemType ) );
	}
#endif /* USE_KEYSETS */

#ifdef USE_RPCAPI

/* Process a command from the client and send it to the appropriate handler */

static const COMMAND_HANDLER commandHandlers[] = {
	NULL, NULL, cmdServerQuery, cmdCreateObject, cmdCreateObjectIndirect,
	cmdExportObject, cmdDestroyObject, cmdQueryCapability, cmdGenKey,
	cmdEncrypt, cmdDecrypt, cmdGetAttribute, cmdSetAttribute,
	cmdDeleteAttribute, cmdGetKey, cmdSetKey, cmdDeleteKey, cmdPushData,
	cmdPopData, cmdFlushData, cmdCertSign, cmdCertCheck, cmdCertMgmt };

static void processCommand( BYTE *buffer )
	{
	COMMAND_INFO cmd = { 0 };
	BYTE header[ COMMAND_FIXED_DATA_SIZE ], *bufPtr;
	long totalLength;
	int i, status;

	/* Read the client's message header */
	memcpy( header, buffer, COMMAND_FIXED_DATA_SIZE );

	/* Process the fixed message header and make sure it's valid */
	getMessageType( header, cmd.type, cmd.flags, cmd.noArgs, cmd.noStrArgs );
	totalLength = getMessageLength( header + COMMAND_WORDSIZE );
	if( !checkCommandInfo( &cmd, totalLength ) || \
		cmd.type == COMMAND_RESULT )
		{
		assert( DEBUG_WARN );

		/* Return an invalid result message */
		putMessageType( buffer, COMMAND_RESULT, 0, 0, 0 );
		putMessageLength( buffer + COMMAND_WORDSIZE, 0 );
		return;
		}

	/* Read the rest of the clients message */
	bufPtr = buffer + COMMAND_FIXED_DATA_SIZE;
	for( i = 0; i < cmd.noArgs; i++ )
		{
		cmd.arg[ i ] = getMessageWord( bufPtr );
		bufPtr += COMMAND_WORDSIZE;
		}
	for( i = 0; i < cmd.noStrArgs; i++ )
		{
		cmd.strArgLen[ i ] = getMessageWord( bufPtr );
		cmd.strArg[ i ] = bufPtr + COMMAND_WORDSIZE;
		bufPtr += COMMAND_WORDSIZE + cmd.strArgLen[ i ];
		}
	if( !checkCommandConsistency( &cmd, totalLength ) )
		{
		assert( DEBUG_WARN );

		/* Return an invalid result message */
		putMessageType( buffer, COMMAND_RESULT, 0, 0, 0 );
		putMessageLength( buffer + COMMAND_WORDSIZE, 0 );
		return;
		}

	/* If it's a command that returns a string value, obtain the returned
	   data in the buffer.  Normally we limit the size to the maximum
	   attribute size, however encoded objects and data popped from
	   envelopes/sessions can be larger than this so we use the entire buffer
	   minus a safety margin */
	if( cmd.type == COMMAND_POPDATA || \
		( cmd.flags != COMMAND_FLAG_RET_LENGTH && \
		  ( cmd.type == COMMAND_EXPORTOBJECT || \
			cmd.type == COMMAND_QUERYCAPABILITY || \
			( cmd.type == COMMAND_GETATTRIBUTE && \
			  cmd.noArgs == 3 ) ) ) )
		{
		cmd.noStrArgs = 1;
		cmd.strArg[ 0 ] = bufPtr;
		if( cmd.type == COMMAND_EXPORTOBJECT || cmd.type == COMMAND_POPDATA )
			{
			cmd.strArgLen[ 0 ] = RPC_IO_BUFSIZE - 16 - ( bufPtr - buffer );
			assert( cmd.type != COMMAND_POPDATA || \
					cmd.strArgLen[ 0 ] >= MAX_FRAGMENT_SIZE );
			}
		else
			cmd.strArgLen[ 0 ] = MAX_ATTRIBUTE_SIZE;
		}

	/* Process the command and copy any return information back to the
	   caller */
	status = commandHandlers[ cmd.type ]( NULL, &cmd );
	bufPtr = buffer;
	if( cryptStatusError( status ) )
		{
		/* The push data command is a special case since an error can occur
		   after some data has been processed, so we still need to copy back
		   a result even if we get an error status */
		if( cmd.type == COMMAND_PUSHDATA )
			{
			putMessageType( bufPtr, COMMAND_RESULT, 0, 2, 0 );
			putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE * 2 );
			putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, status );
			putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, cmd.arg[ 0 ] );
			return;
			}

		/* The command failed, return a simple status value */
		putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 0 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, status );
		return;
		}
	if( cmd.type == COMMAND_CREATEOBJECT || \
		cmd.type == COMMAND_CREATEOBJECT_INDIRECT || \
		cmd.type == COMMAND_GETKEY || \
		cmd.type == COMMAND_PUSHDATA || \
		( ( cmd.type == COMMAND_EXPORTOBJECT || \
			cmd.type == COMMAND_QUERYCAPABILITY ) && \
		  cmd.flags == COMMAND_FLAG_RET_LENGTH ) || \
		( cmd.type == COMMAND_GETATTRIBUTE && \
		  ( cmd.noArgs == 2 || cmd.flags == COMMAND_FLAG_RET_LENGTH ) ) || \
		( cmd.type == COMMAND_CERTMGMT && cmd.flags != COMMAND_FLAG_RET_NONE ) )
		{
		/* Return object handle or numeric value or string length */
		putMessageType( bufPtr, COMMAND_RESULT, 0, 2, 0 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE * 2 );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
		putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, cmd.arg[ 0 ] );
		return;
		}
	if( cmd.type == COMMAND_ENCRYPT || \
		cmd.type == COMMAND_DECRYPT || \
		cmd.type == COMMAND_POPDATA || \
		cmd.type == COMMAND_EXPORTOBJECT || \
		cmd.type == COMMAND_QUERYCAPABILITY || \
		cmd.type == COMMAND_GETATTRIBUTE )
		{
		const long dataLength = cmd.strArgLen[ 0 ];

		/* Return capability info or attribute data and length */
		putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 1 );
		putMessageLength( bufPtr + COMMAND_WORDSIZE,
						  ( COMMAND_WORDSIZE * 2 ) + cmd.strArgLen[ 0 ] );
		putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
		putMessageWord( bufPtr + COMMAND_WORD2_OFFSET, dataLength );
		if( dataLength )
			memmove( bufPtr + COMMAND_WORD3_OFFSET, cmd.strArg[ 0 ],
					 dataLength );
		return;
		}
	putMessageType( bufPtr, COMMAND_RESULT, 0, 1, 0 );
	putMessageLength( bufPtr + COMMAND_WORDSIZE, COMMAND_WORDSIZE );
	putMessageWord( bufPtr + COMMAND_WORD1_OFFSET, CRYPT_OK );
	}

/* Dummy forwarding procedure to take the place of the comms channel between
   client and server */

static void serverTransact( void *clientBuffer )
	{
#ifdef CONFIG_CONSERVE_MEMORY
	static BYTE *serverBuffer = NULL;
#else
	BYTE serverBuffer[ RPC_IO_BUFSIZE ];
#endif /* Memory-starved systems */
	int length;

	/* On memory-starved systems (typically older ones limited to a maximum
	   of 64K of stack) we have to malloc() this on demand.  Since there's
	   no threading this is OK since there won't be any race conditions.
	   Note that there's no way to free this, but it'll be automatically
	   freed when the OS reclaims the application heap */
#ifdef CONFIG_CONSERVE_MEMORY
	if( serverBuffer == NULL && \
		( serverBuffer = clAlloc( "serverTransact", \
								  RPC_IO_BUFSIZE ) ) == NULL )
		{
		memset( clientBuffer, 0, 16 );
		return;
		}
#endif /* Memory-starved systems */

	/* Copy the command to the server buffer, process it, and copy the result
	   back to the client buffer to emulate the client <-> server
	   transmission */
	length = getMessageLength( ( BYTE * ) clientBuffer + COMMAND_WORDSIZE );
	memcpy( serverBuffer, clientBuffer, length + COMMAND_FIXED_DATA_SIZE );
	processCommand( serverBuffer );
	length = getMessageLength( ( BYTE * ) serverBuffer + COMMAND_WORDSIZE );
	memcpy( clientBuffer, serverBuffer, length + COMMAND_FIXED_DATA_SIZE );
	}

/* Dispatch a command to the server */

static int dispatchCommand( COMMAND_INFO *cmd )
	{
	COMMAND_INFO sentCmd = *cmd;
	BYTE buffer[ RPC_IO_BUFSIZE ], *bufPtr = buffer;
	BYTE header[ COMMAND_FIXED_DATA_SIZE ];
	BYTE *payloadStartPtr, *payloadPtr;
	const BOOLEAN isPushPop = \
		( cmd->type == COMMAND_PUSHDATA || cmd->type == COMMAND_POPDATA ) ? \
		TRUE : FALSE;
	const BOOLEAN isDataCommand = \
		( cmd->type == COMMAND_ENCRYPT || cmd->type == COMMAND_DECRYPT || \
		  isPushPop ) ? TRUE : FALSE;
	const long payloadLength = ( cmd->noArgs * COMMAND_WORDSIZE ) + \
							   ( cmd->noStrArgs * COMMAND_WORDSIZE ) + \
							   cmd->strArgLen[ 0 ] + cmd->strArgLen[ 1 ];
	long dataLength = ( cmd->type == COMMAND_POPDATA ) ? \
					  cmd->arg[ 1 ] : cmd->strArgLen[ 0 ], resultLength;
	int i;

	assert( checkCommandInfo( cmd, 0 ) );

	/* Clear the return value */
	memset( cmd, 0, sizeof( COMMAND_INFO ) );

	/* Make sure the data will fit into the buffer */
	if( !isDataCommand && \
		( COMMAND_FIXED_DATA_SIZE + payloadLength ) > RPC_IO_BUFSIZE )
		{
		long maxLength = dataLength;
		int maxPos = 0;

		/* Find the longest arg (the one that contributes most to the
		   problem) and report it as an error.  We report the problem
		   as being with the numeric rather than the string arg since
		   string args with implicit lengths (e.g.text strings) have
		   their length check in the API function, and all other string
		   args are given as (data, length) pairs  */
		for( i = 0; i < sentCmd.noStrArgs; i++ )
			{
			if( sentCmd.strArgLen[ i ] > maxLength )
				{
				maxLength = sentCmd.strArgLen[ i ];
				maxPos = i;
				}
			}
		return( CRYPT_ARGERROR_NUM1 - maxPos );
		}

	/* If it's a short-datasize command, process it and return immediately */
	if( !isDataCommand || ( dataLength < MAX_FRAGMENT_SIZE ) )
		{
		/* Write the header and message fields to the buffer */
		putMessageType( bufPtr, sentCmd.type, sentCmd.flags,
						sentCmd.noArgs, sentCmd.noStrArgs );
		putMessageLength( bufPtr + COMMAND_WORDSIZE, payloadLength );
		bufPtr += COMMAND_FIXED_DATA_SIZE;
		for( i = 0; i < sentCmd.noArgs; i++ )
			{
			putMessageWord( bufPtr, sentCmd.arg[ i ] );
			bufPtr += COMMAND_WORDSIZE;
			}
		for( i = 0; i < sentCmd.noStrArgs; i++ )
			{
			const int argLength = sentCmd.strArgLen[ i ];

			putMessageWord( bufPtr, argLength );
			if( argLength > 0 )
				memcpy( bufPtr + COMMAND_WORDSIZE, sentCmd.strArg[ i ],
						argLength );
			bufPtr += COMMAND_WORDSIZE + argLength;
			}

		/* Send the command to the server and read the servers message header */
		serverTransact( buffer );
		memcpy( header, buffer, COMMAND_FIXED_DATA_SIZE );

		/* Process the fixed message header and make sure that it's valid */
		getMessageType( header, cmd->type, cmd->flags,
						cmd->noArgs, cmd->noStrArgs );
		resultLength = getMessageLength( header + COMMAND_WORDSIZE );
		if( !checkCommandInfo( cmd, resultLength ) || \
			cmd->type != COMMAND_RESULT )
			{
			assert( DEBUG_WARN );
			return( CRYPT_ERROR );
			}

		/* Read the rest of the server's message */
		bufPtr = buffer + COMMAND_FIXED_DATA_SIZE;
		for( i = 0; i < cmd->noArgs; i++ )
			{
			cmd->arg[ i ] = getMessageWord( bufPtr );
			bufPtr += COMMAND_WORDSIZE;
			}
		for( i = 0; i < cmd->noStrArgs; i++ )
			{
			cmd->strArgLen[ i ] = getMessageWord( bufPtr );
			cmd->strArg[ i ] = bufPtr + COMMAND_WORDSIZE;
			bufPtr += COMMAND_WORDSIZE + cmd->strArgLen[ i ];
			}

		/* The first value returned is the status code, if it's nonzero return
		   it to the caller, otherwise move the other values down */
		if( cryptStatusError( cmd->arg[ 0 ] ) )
			{
			/* The push data command is a special case since it returns a
			   bytes copied value even if an error occurs */
			if( sentCmd.type == COMMAND_PUSHDATA )
				{
				const int status = cmd->arg[ 0 ];

				cmd->arg[ 0 ] = cmd->arg[ 1 ];
				cmd->arg[ 1 ] = 0;
				cmd->noArgs--;
				return( status );
				}
			return( cmd->arg[ 0 ] );
			}
		assert( cryptStatusOK( cmd->arg[ 0 ] ) );
		for( i = 1; i < cmd->noArgs; i++ )
			cmd->arg[ i - 1 ] = cmd->arg[ i ];
		cmd->arg[ i ] = 0;
		cmd->noArgs--;

		/* Copy any string arg data back to the caller */
		if( cmd->noStrArgs && cmd->strArgLen[ 0 ] )
			{
			memcpy( sentCmd.strArg[ 0 ], cmd->strArg[ 0 ],
					cmd->strArgLen[ 0 ] );
			cmd->strArg[ 0 ] = sentCmd.strArg[ 0 ];
			if( cmd->type == COMMAND_PUSHDATA )
				{
				/* A data push returns the actual number of copied bytes
				   (which may be less than the requested number of bytes) as
				   arg 0 */
				cmd->arg[ 0 ] = cmd->strArgLen[ 0 ];
				}
			}

		return( CRYPT_OK );
		}

	/* Remember where the variable-length payload starts in the buffer and
	   where it's to be copied to */
	payloadStartPtr = buffer + COMMAND_FIXED_DATA_SIZE + COMMAND_WORDSIZE;
	payloadPtr = sentCmd.strArg[ 0 ];

	/* It's a long-datasize command, handle fragmentation */
	do
		{
		COMMAND_INFO cmdResult = { 0 };
		const int fragmentLength = min( dataLength, MAX_FRAGMENT_SIZE );
		int status;

		/* Write the fixed and variable-length message fields to the buffer */
		putMessageType( buffer, sentCmd.type, 0, sentCmd.noArgs,
						sentCmd.noStrArgs );
		if( sentCmd.type == COMMAND_POPDATA )
			{
			putMessageLength( buffer + COMMAND_WORDSIZE,
							  ( COMMAND_WORDSIZE * 2 ) );
			}
		else
			{
			putMessageLength( buffer + COMMAND_WORDSIZE,
							  ( COMMAND_WORDSIZE * 2 ) + fragmentLength );
			}
		putMessageWord( buffer + COMMAND_FIXED_DATA_SIZE,
						sentCmd.arg[ 0 ] );
		putMessageWord( payloadStartPtr, fragmentLength );
		if( sentCmd.type != COMMAND_POPDATA )
			{
			memcpy( payloadStartPtr + COMMAND_WORDSIZE, payloadPtr,
					fragmentLength );
			}

		/* Process as much data as we can and read the server's message
		   header */
		serverTransact( buffer );
		memcpy( header, buffer, COMMAND_FIXED_DATA_SIZE );

		/* Process the fixed message header and make sure it's valid */
		getMessageType( header, cmdResult.type, cmdResult.flags,
						cmdResult.noArgs, cmdResult.noStrArgs );
		resultLength = getMessageLength( header + COMMAND_WORDSIZE );
		if( !checkCommandInfo( &cmdResult, resultLength ) || \
			cmdResult.type != COMMAND_RESULT || \
			cmdResult.flags != COMMAND_FLAG_NONE )
			{
			assert( DEBUG_WARN );
			return( CRYPT_ERROR );
			}
		if( cmdResult.noArgs != 1 || cmdResult.noStrArgs )
			{
			/* Make sure the parameters are valid for a non-error return */
			if( sentCmd.type == COMMAND_PUSHDATA )
				{
				if( cmdResult.noArgs != 2 || cmdResult.noStrArgs )
					{
					assert( DEBUG_WARN );
					return( CRYPT_ERROR );
					}
				}
			else
				if( cmdResult.noArgs != 1 || cmdResult.noStrArgs != 1 )
					{
					assert( DEBUG_WARN );
					return( CRYPT_ERROR );
					}
			}

		/* Process the fixed message header and make sure it's valid */
		bufPtr = buffer + COMMAND_FIXED_DATA_SIZE;
		status = getMessageWord( bufPtr );
		if( cryptStatusError( status ) )
			{
			/* The push data command is a special case since it returns a
			   bytes copied value even if an error occurs */
			if( sentCmd.type == COMMAND_PUSHDATA )
				{
				const long bytesCopied = \
							getMessageWord( bufPtr + COMMAND_WORDSIZE );

				if( bytesCopied < 0 )
					{
					assert( DEBUG_WARN );
					return( CRYPT_ERROR );
					}
				cmdResult.arg[ 0 ] = cmd->arg[ 0 ] + bytesCopied;
				cmdResult.noArgs = 1;
				}
			*cmd = cmdResult;
			return( status );
			}
		assert( cryptStatusOK( status ) );

		/* Read the rest of the server's message */
		resultLength = getMessageWord( bufPtr + COMMAND_WORDSIZE );
		if( isPushPop )
			{
			/* It's a variable-length transformation, we have to return a
			   non-negative number of bytes */
			if( resultLength <= 0 )
				{
				if( resultLength == 0 )
					{
					/* We've run out of data, return to the caller */
					break;
					}
				assert( DEBUG_WARN );
				return( CRYPT_ERROR );
				}
			if( cmd->type == COMMAND_PUSHDATA )
				cmd->arg[ 0 ] += resultLength;
			else
				cmd->strArgLen[ 0 ] += resultLength;
			if( sentCmd.type == COMMAND_POPDATA )
				{
				memcpy( payloadPtr, payloadStartPtr + COMMAND_WORDSIZE,
						resultLength );

				/* If we got less than what we asked for, there's nothing
				   more available, exit */
				if( resultLength < fragmentLength )
					dataLength = resultLength;
				}
			}
		else
			{
			/* It's an encrypt/decrypt, the result must be a 1:1
			   transformation unless it's a hash, in which case nothing is
			   returned */
			if( resultLength )
				{
				if( resultLength != fragmentLength )
					{
					assert( DEBUG_WARN );
					return( CRYPT_ERROR );
					}
				memcpy( payloadPtr, payloadStartPtr + COMMAND_WORDSIZE,
						resultLength );
				}
			else
				{
				/* If no data was returned, it was a hash, set a pseudo-
				   result length which is the same size as the amount of
				   data fed in */
				resultLength = fragmentLength;
				}
			}

		/* Move on to the next fragment */
		payloadPtr += resultLength;
		dataLength -= resultLength;
		}
	while( dataLength > 0 );

	return( CRYPT_OK );
	}
#endif /* USE_RPCAPI */

/****************************************************************************
*																			*
*						Client-side Translation Handling					*
*																			*
****************************************************************************/

/* When the cryptlib client is using a different character set, we need to
   map from the internal to the external character set.  The following
   function checks for attribute values that contain text strings.  In
   addition the functions cryptGetKey(), cryptGetPrivateKey(), 
   cryptAddPrivateKey(), and cryptLogin() use text strings that need to be 
   mapped to the internal character set */

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )

static BOOLEAN needsTranslation( const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	if( attribute < CRYPT_CTXINFO_LAST )
		{
		if( attribute < CRYPT_OPTION_LAST )
			{
			return( ( attribute == CRYPT_ATTRIBUTE_ERRORMESSAGE || \
					  attribute == CRYPT_OPTION_INFO_DESCRIPTION || \
					  attribute == CRYPT_OPTION_INFO_COPYRIGHT || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_FILTER || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_CACERTNAME || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_CERTNAME || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_CRLNAME || \
					  attribute == CRYPT_OPTION_KEYS_LDAP_EMAILNAME ) ? \
					TRUE : FALSE );
			}
		return( ( attribute == CRYPT_CTXINFO_NAME_ALGO || \
				  attribute == CRYPT_CTXINFO_NAME_MODE || \
				  attribute == CRYPT_CTXINFO_KEYING_VALUE || \
				  attribute == CRYPT_CTXINFO_LABEL ) ? \
				TRUE : FALSE );
		}
	if( attribute <= CRYPT_CERTINFO_LAST_NAME )
		{
		if( attribute < CRYPT_CERTINFO_FIRST_NAME )
			{
			return( ( attribute == CRYPT_CERTINFO_DN || \
					  attribute == CRYPT_CERTINFO_PKIUSER_ID || \
					  attribute == CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD || \
					  attribute == CRYPT_CERTINFO_PKIUSER_REVPASSWORD ) ? \
					TRUE : FALSE );
			}
		return( ( attribute == CRYPT_CERTINFO_COUNTRYNAME || \
				  attribute == CRYPT_CERTINFO_STATEORPROVINCENAME || \
				  attribute == CRYPT_CERTINFO_LOCALITYNAME || \
				  attribute == CRYPT_CERTINFO_ORGANIZATIONNAME || \
				  attribute == CRYPT_CERTINFO_ORGANIZATIONALUNITNAME || \
				  attribute == CRYPT_CERTINFO_COMMONNAME || \
				  attribute == CRYPT_CERTINFO_OTHERNAME_TYPEID || \
				  attribute == CRYPT_CERTINFO_RFC822NAME || \
				  attribute == CRYPT_CERTINFO_DNSNAME || \
				  attribute == CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER || \
				  attribute == CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME || \
				  attribute == CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER ) ? \
				TRUE : FALSE );
		}
	if( attribute <= CRYPT_CERTINFO_LAST_EXTENSION )
		{
		return( ( attribute == CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY || \
				  attribute == CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION || \
				  attribute == CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY || \
				  attribute == CRYPT_CERTINFO_SIGG_RESTRICTION || \
				  attribute == CRYPT_CERTINFO_SUBJECTDIR_TYPE || \
				  attribute == CRYPT_CERTINFO_CERTPOLICYID || \
				  attribute == CRYPT_CERTINFO_CERTPOLICY_CPSURI || \
				  attribute == CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION || \
				  attribute == CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT || \
				  attribute == CRYPT_CERTINFO_ISSUERDOMAINPOLICY || \
				  attribute == CRYPT_CERTINFO_SUBJECTDOMAINPOLICY || \
				  attribute == CRYPT_CERTINFO_SET_MERID || \
				  attribute == CRYPT_CERTINFO_SET_MERACQUIRERBIN || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTLANGUAGE || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTNAME || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTCITY || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE || \
				  attribute == CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME || \
				  attribute == CRYPT_CERTINFO_SET_MERCOUNTRY || \
				  attribute == CRYPT_CERTINFO_SET_MERAUTHFLAG ) ? \
				TRUE : FALSE );
		}
	if( attribute <= CRYPT_CERTINFO_LAST_CMS )
		{
		return( ( attribute == CRYPT_CERTINFO_CMS_SECLABEL_POLICY || \
				  attribute == CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK || \
				  attribute == CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE || \
				  attribute == CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE || \
				  attribute == CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION || \
				  attribute == CRYPT_CERTINFO_CMS_EQVLABEL_POLICY || \
				  attribute == CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK || \
				  attribute == CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE || \
				  attribute == CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE || \
				  attribute == CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES || \
				  attribute == CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME || \
				  attribute == CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL || \
				  attribute == CRYPT_CERTINFO_CMS_SPCAGENCYURL ) ? \
				TRUE : FALSE );
		}
	if( attribute < CRYPT_KEYINFO_LAST )
		return( ( attribute == CRYPT_KEYINFO_QUERY || \
				  attribute == CRYPT_KEYINFO_QUERY_REQUESTS ) ? \
				TRUE : FALSE );
	if( attribute < CRYPT_DEVINFO_LAST )
		{
		return( ( attribute == CRYPT_DEVINFO_INITIALISE || \
				  attribute == CRYPT_DEVINFO_AUTHENT_USER || \
				  attribute == CRYPT_DEVINFO_AUTHENT_SUPERVISOR || \
				  attribute == CRYPT_DEVINFO_SET_AUTHENT_USER || \
				  attribute == CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR || \
				  attribute == CRYPT_DEVINFO_ZEROISE || \
				  attribute == CRYPT_DEVINFO_LABEL ) ? \
				TRUE : FALSE );
		}
	if( attribute < CRYPT_ENVINFO_LAST )
		{
		return( ( attribute == CRYPT_ENVINFO_PASSWORD || \
				  attribute == CRYPT_ENVINFO_RECIPIENT || \
				  attribute == CRYPT_ENVINFO_PRIVATEKEY_LABEL ) ? \
				TRUE : FALSE );
		}
	if( attribute < CRYPT_SESSINFO_LAST )
		{
		return( ( attribute == CRYPT_SESSINFO_USERNAME || \
				  attribute == CRYPT_SESSINFO_PASSWORD || \
				  attribute == CRYPT_SESSINFO_SERVER_NAME || \
				  attribute == CRYPT_SESSINFO_CLIENT_NAME ) ? \
				TRUE : FALSE );
		}
	return( ( attribute == CRYPT_USERINFO_PASSWORD ) ? TRUE : FALSE );
	}

#ifdef EBCDIC_CHARS
  #define nativeStrlen( string )	strlen( string )
  #define nativeToCryptlibString( dest, destMaxLen, src, length ) \
		  ebcdicToAscii( dest, src, length )
  #define cryptlibToNativeString( dest, destMaxLen, src, length ) \
		  asciiToEbcdic( dest, src, length )
#else
  #define nativeStrlen( string )	wcslen( string )
  #define nativeToCryptlibString( dest, destMaxLen, src, length ) \
		  unicodeToAscii( dest, destMaxLen, src, length )
  #define cryptlibToNativeString( dest, destMaxLen, src, length ) \
		  asciiToUnicode( dest, destMaxLen, src, length )
#endif /* EBCDIC vs. Unicode translation */

#endif /* EBCDIC_CHARS || UNICODE_CHARS */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Some functions take null-terminated strings as parameters, since these
   can be ASCII or Unicode strings depending on the environment we need to
   define a situation-specific get-string-length function to handle them */

#ifdef nativeStrlen
  #define strParamLen	nativeStrlen
#else
  #define strParamLen	strlen
#endif /* System-specific string parameter length functions */

/* Internal parameter errors are reported in terms of the parameter type (e.g.
   invalid object, invalid attribute), but externally they're reported in
   terms of parameter numbers.  Before we return error values to the caller,
   we have to map them from the internal representation to the position they
   occur in in the function parameter list.  The following function takes a
   list of parameter types and maps the returned parameter type error to a
   parameter position error */

typedef enum {
	ARG_D,			/* Dummy placeholder */
	ARG_O,			/* Object */
	ARG_V,			/* Value (attribute) */
	ARG_N,			/* Numeric arg */
	ARG_S,			/* String arg */
	ARG_LAST
	} ERRORMAP;

static int mapError( const ERRORMAP *errorMap, const int errorMapSize, 
					 const int status )
	{
	ERRORMAP type;
	int count = 0, i;

	/* If it's not an internal parameter error, let it out */
	if( !cryptArgError( status ) )
		{
		assert( cryptStandardError( status ) );
		return( status );
		}

	/* Map the parameter error to a position error */
	switch( status )
		{
		case CRYPT_ARGERROR_OBJECT:
			type = ARG_O;
			break;
		case CRYPT_ARGERROR_VALUE:
			type = ARG_V;
			break;
		case CRYPT_ARGERROR_NUM1:
		case CRYPT_ARGERROR_NUM2:
			type = ARG_N;
			count = CRYPT_ARGERROR_NUM1 - status;
			break;
		case CRYPT_ARGERROR_STR1:
		case CRYPT_ARGERROR_STR2:
			type = ARG_S;
			count = CRYPT_ARGERROR_STR1 - status;
			break;
		default:
			retIntError();
		}
	for( i = 0; errorMap[ i ] != ARG_LAST && \
				i < errorMapSize; i++ )
		{
		if( errorMap[ i ] == type && !count-- )
			return( CRYPT_ERROR_PARAM1 - i );
		}
	if( i >= errorMapSize )
		retIntError();
	retIntError();
	}

/****************************************************************************
*																			*
*								Create/Destroy Objects						*
*																			*
****************************************************************************/

/* A flag to record whether the external API initialisation function has
   been called.  This merely reflects the current state of the cryptInit()/
   cryptEnd() calls at the external API level rather than the internal state
   of the kernel, and is used to try and catch problems with people who
   don't call cryptInit() */

static BOOLEAN initCalled = FALSE;

/* Initialise and shut down cryptlib.  These functions are a type of super-
   create/destroy in that they create/destroy an instantiation of cryptlib.
   Unlike the other functions in this module, these can't pass control to
   the kernel because it hasn't been instantiated yet, so they pass the call
   down to the internal init/shutdown functions */

C_CHECK_RETVAL \
C_RET cryptInit( void )
	{
	int status;

	status = initCryptlib();
	if( cryptStatusOK( status ) )
		initCalled = TRUE;
	return( status );
	}

C_RET cryptEnd( void )
	{
	initCalled = FALSE;
	return( endCryptlib() );
	}

/* Create an encryption context */

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptCreateContext( C_OUT CRYPT_CONTEXT C_PTR cryptContext,
						  C_IN CRYPT_USER cryptUser,
						  C_IN CRYPT_ALGO_TYPE cryptAlgo )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 0,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_CONTEXT } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isWritePtrConst( cryptContext, sizeof( CRYPT_CONTEXT ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*cryptContext = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( cryptAlgo <= CRYPT_ALGO_NONE || \
		cryptAlgo >= CRYPT_ALGO_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = cryptAlgo;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*cryptContext = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Create an encryption context via the device */

#ifdef USE_DEVICES

C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) \
C_RET cryptDeviceCreateContext( C_IN CRYPT_DEVICE device,
							    C_OUT CRYPT_CONTEXT C_PTR cryptContext,
							    C_IN CRYPT_ALGO_TYPE cryptAlgo )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 0,
		  { 0, OBJECT_TYPE_CONTEXT } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_D, ARG_N, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( device ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( cryptContext, sizeof( CRYPT_CONTEXT ) ) )
		return( CRYPT_ERROR_PARAM2 );
	*cryptContext = CRYPT_ERROR;
	if( cryptAlgo <= CRYPT_ALGO_NONE || \
		cryptAlgo >= CRYPT_ALGO_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = device;
	cmd.arg[ 2 ] = cryptAlgo;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*cryptContext = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_DEVICES */

/* Create a certificate */

#ifdef USE_CERTIFICATES

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptCreateCert( C_OUT CRYPT_CERTIFICATE C_PTR certificate,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_CERTTYPE_TYPE certType )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 0,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_CERTIFICATE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isWritePtrConst( certificate, sizeof( CRYPT_CERTIFICATE ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*certificate = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( certType <= CRYPT_CERTTYPE_NONE || \
		certType >= CRYPT_CERTTYPE_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = certType;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*certificate = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_CERTIFICATES */

/* Open a device */

#ifdef USE_DEVICES

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptDeviceOpen( C_OUT CRYPT_DEVICE C_PTR device,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_DEVICE_TYPE deviceType,
					   C_IN_OPT C_STR name )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 1,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_DEVICE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE nameBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *namePtr = NULL;
#else
	const char *namePtr = name;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int nameLen = 0, status;

	/* Perform basic error checking */
	if( !isReadPtrConst( device, sizeof( CRYPT_DEVICE ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*device = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( deviceType <= CRYPT_DEVICE_NONE || deviceType >= CRYPT_DEVICE_LAST )
		return( CRYPT_ERROR_PARAM3 );
	if( ( deviceType == CRYPT_DEVICE_PKCS11 || \
		  deviceType == CRYPT_DEVICE_CRYPTOAPI ) && \
		( !isReadPtrConst( name, MIN_NAME_LENGTH ) || \
		  strParamLen( name ) < MIN_NAME_LENGTH || \
		  strParamLen( name ) >= MAX_ATTRIBUTE_SIZE ) )
		return( CRYPT_ERROR_PARAM4 );
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	if( name != NULL )
		{
		status = nativeToCryptlibString( nameBuffer, MAX_ATTRIBUTE_SIZE,
										 name, nativeStrlen( name ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM4 );
		namePtr = nameBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	if( namePtr != NULL )
		{
		nameLen = strStripWhitespace( &namePtr, namePtr, strlen( namePtr ) );
		if( nameLen <= 0 )
			return( CRYPT_ERROR_PARAM4 );
		}

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = deviceType;
	cmd.strArg[ 0 ] = ( void * ) namePtr;
	cmd.strArgLen[ 0 ] = nameLen;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*device = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_DEVICES */

/* Create an envelope */

#ifdef USE_ENVELOPES

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptCreateEnvelope( C_OUT CRYPT_ENVELOPE C_PTR envelope,
						   C_IN CRYPT_USER cryptUser,
						   C_IN CRYPT_FORMAT_TYPE formatType )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 0,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_ENVELOPE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic error checking */
	if( !isWritePtrConst( envelope, sizeof( CRYPT_ENVELOPE ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*envelope = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( formatType <= CRYPT_FORMAT_NONE || \
		formatType >= CRYPT_FORMAT_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = formatType;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*envelope = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_ENVELOPES */

/* Open/create a keyset */

#ifdef USE_KEYSETS

C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 4 ) ) \
C_RET cryptKeysetOpen( C_OUT CRYPT_KEYSET C_PTR keyset,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_KEYSET_TYPE keysetType,
					   C_IN C_STR name, C_IN CRYPT_KEYOPT_TYPE options )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 4, 1,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_KEYSET } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_S, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE nameBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *namePtr = nameBuffer;
#else
	const char *namePtr = name;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int nameLen, status;

	/* Perform basic error checking */
	if( !isReadPtrConst( keyset, sizeof( CRYPT_KEYSET ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*keyset = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( keysetType <= CRYPT_KEYSET_NONE || keysetType >= CRYPT_KEYSET_LAST )
		return( CRYPT_ERROR_PARAM3 );
	if( !isReadPtrConst( name, MIN_NAME_LENGTH ) || \
		strParamLen( name ) < MIN_NAME_LENGTH || \
		strParamLen( name ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM4 );
	if( options < CRYPT_KEYOPT_NONE || \
		options >= CRYPT_KEYOPT_LAST_EXTERNAL )
		{
		/* CRYPT_KEYOPT_NONE is a valid setting for this parameter */
		return( CRYPT_ERROR_PARAM4 );
		}
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( nameBuffer, MAX_ATTRIBUTE_SIZE,
									 name, nativeStrlen( name ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM4 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	nameLen = strStripWhitespace( &namePtr, namePtr, strlen( namePtr ) );
	if( nameLen <= 0 )
		return( CRYPT_ERROR_PARAM4 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = keysetType;
	cmd.arg[ 3 ] = options;
	cmd.strArg[ 0 ] = ( void * ) namePtr;
	cmd.strArgLen[ 0 ] = nameLen;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*keyset = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_KEYSETS */

/* Create a session */

#ifdef USE_SESSIONS

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptCreateSession( C_OUT CRYPT_SESSION C_PTR session,
						  C_IN CRYPT_USER cryptUser,
						  C_IN CRYPT_SESSION_TYPE sessionType )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 3, 0,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_SESSION } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_O, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic error checking */
	if( !isWritePtrConst( session, sizeof( CRYPT_SESSION ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*session = CRYPT_ERROR;
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM2 );
	if( sessionType <= CRYPT_SESSION_NONE || \
		sessionType >= CRYPT_SESSION_LAST )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.arg[ 2 ] = sessionType;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*session = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_SESSIONS */

/* Log on/create a user object */

C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 2, 3 ) ) \
C_RET cryptLogin( C_OUT CRYPT_USER C_PTR user,
				  C_IN C_STR name, C_IN C_STR password )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT, COMMAND_FLAG_NONE, 2, 2,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_USER } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_S, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE nameBuffer[ CRYPT_MAX_TEXTSIZE + 1 ], *namePtr = nameBuffer;
	BYTE passwordBuffer[ CRYPT_MAX_TEXTSIZE + 1 ], *passwordPtr = passwordBuffer;
#else
	const char *namePtr = name, *passwordPtr = password;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int nameLen, passwordLen, status;

	/* Perform basic error checking */
	if( !isReadPtrConst( user, sizeof( CRYPT_USER ) ) )
		return( CRYPT_ERROR_PARAM1 );
	*user = CRYPT_ERROR;
	if( !isReadPtrConst( name, MIN_NAME_LENGTH ) || \
		strParamLen( name ) < MIN_NAME_LENGTH || \
		strParamLen( name ) > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtrConst( password, MIN_NAME_LENGTH ) || \
		strParamLen( password ) < MIN_NAME_LENGTH || \
		strParamLen( password ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM3 );
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( nameBuffer, MAX_ATTRIBUTE_SIZE,
									 name, nativeStrlen( name ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM2 );
	status = nativeToCryptlibString( passwordBuffer, MAX_ATTRIBUTE_SIZE,
									 password, nativeStrlen( password ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM3 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	nameLen = strStripWhitespace( &namePtr, namePtr, strlen( namePtr ) );
	if( nameLen <= 0 )
		return( CRYPT_ERROR_PARAM2 );
	passwordLen = strStripWhitespace( &passwordPtr, passwordPtr, 
									  strlen( passwordPtr ) );
	if( passwordLen <= 0 )
		return( CRYPT_ERROR_PARAM3 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.strArg[ 0 ] = ( void * ) namePtr;
	cmd.strArgLen[ 0 ] = nameLen;
	cmd.strArg[ 1 ] = ( void * ) passwordPtr;
	cmd.strArgLen[ 1 ] = passwordLen;
	status = DISPATCH_COMMAND( cmdCreateObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*user = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Destroy object functions */

C_RET cryptDestroyObject( C_IN CRYPT_HANDLE cryptHandle )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_DESTROYOBJECT, COMMAND_FLAG_NONE, 1, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) )
		return( CRYPT_ERROR_PARAM1 );

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = cryptHandle;
	status = DISPATCH_COMMAND( cmdDestroyObject, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_RET cryptDestroyCert( C_IN CRYPT_CERTIFICATE certificate )
	{
	return( cryptDestroyObject( certificate ) );
	}
C_RET cryptDestroyContext( C_IN CRYPT_CONTEXT cryptContext )
	{
	return( cryptDestroyObject( cryptContext ) );
	}
C_RET cryptDestroyEnvelope( C_IN CRYPT_ENVELOPE cryptEnvelope )
	{
	return( cryptDestroyObject( cryptEnvelope ) );
	}
C_RET cryptDeviceClose( C_IN CRYPT_DEVICE device )
	{
	return( cryptDestroyObject( device ) );
	}
C_RET cryptKeysetClose( C_IN CRYPT_KEYSET keyset )
	{
	return( cryptDestroyObject( keyset ) );
	}
C_RET cryptDestroySession( C_IN CRYPT_SESSION session )
	{
	return( cryptDestroyObject( session ) );
	}
C_RET cryptLogout( C_IN CRYPT_USER user )
	{
	return( cryptDestroyObject( user ) );
	}

/****************************************************************************
*																			*
*						Attribute Manipulation Functions					*
*																			*
****************************************************************************/

/* Get an attribute */

C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) \
C_RET cryptGetAttribute( C_IN CRYPT_HANDLE cryptHandle,
						 C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
						 C_OUT int C_PTR value )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETATTRIBUTE, COMMAND_FLAG_NONE, 2, 0,
		  { DEFAULTUSER_OBJECT_HANDLE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) && cryptHandle != CRYPT_UNUSED )
		return( CRYPT_ERROR_PARAM1 );
	if( attributeType <= CRYPT_ATTRIBUTE_NONE || attributeType >= CRYPT_ATTRIBUTE_LAST )
		return( CRYPT_ERROR_PARAM2 );
	if( !isWritePtrConst( value, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM3 );
	*value = CRYPT_ERROR;

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptHandle != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptHandle;
	cmd.arg[ 1 ] = attributeType;
	status = DISPATCH_COMMAND( cmdGetAttribute, cmd );
	if( cryptStatusOK( status ) )
		{
		*value = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 4 ) ) \
C_RET cryptGetAttributeString( C_IN CRYPT_HANDLE cryptHandle,
							   C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
							   C_OUT_OPT void C_PTR value,
							   C_OUT int C_PTR valueLength )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETATTRIBUTE, COMMAND_FLAG_NONE, 3, RETURN_VALUE( 1 ),
		  { DEFAULTUSER_OBJECT_HANDLE, 0, TRUE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_S, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) && cryptHandle != CRYPT_UNUSED )
		return( CRYPT_ERROR_PARAM1 );
	if( attributeType <= CRYPT_ATTRIBUTE_NONE || \
		attributeType >= CRYPT_ATTRIBUTE_LAST )
		return( CRYPT_ERROR_PARAM2 );
	if( !isWritePtrConst( valueLength, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM4 );
	*valueLength = CRYPT_ERROR;
	if( value != NULL )
		*( ( BYTE * ) value ) = '\0';

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( value == NULL )
		{
		cmd.flags = COMMAND_FLAG_RET_LENGTH;
		cmd.noStrArgs = 0;
		}
	if( cryptHandle != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptHandle;
	cmd.arg[ 1 ] = attributeType;	/* arg[ 2 ] = TRUE from template */
	cmd.strArg[ 0 ] = value;
	cmd.strArgLen[ 0 ] = RETURN_VALUE( MAX_ATTRIBUTE_SIZE );
	status = DISPATCH_COMMAND( cmdGetAttribute, cmd );
	if( cryptStatusOK( status ) )
		{
		*valueLength = ( value == NULL ) ? cmd.arg[ 0 ] : cmd.strArgLen[ 0 ];
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
		if( value != NULL && *valueLength > 0 && \
			needsTranslation( attributeType ) )
			{
			BYTE buffer[ MAX_ATTRIBUTE_SIZE ];

			if( *valueLength >= MAX_ATTRIBUTE_SIZE )
				return( CRYPT_ERROR_OVERFLOW );
			memcpy( buffer, value, *valueLength );
			cryptlibToNativeString( value, MAX_ATTRIBUTE_SIZE,
									buffer, *valueLength );
  #ifdef UNICODE_CHARS
			*valueLength *= sizeof( wchar_t );
  #endif /* UNICODE_CHARS */
			}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Set an attribute */

C_RET cryptSetAttribute( C_IN CRYPT_HANDLE cryptHandle,
						 C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
						 C_IN int value )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_SETATTRIBUTE, COMMAND_FLAG_NONE, 3, 0,
		  { DEFAULTUSER_OBJECT_HANDLE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) && cryptHandle != CRYPT_UNUSED )
		return( CRYPT_ERROR_PARAM1 );
	if( attributeType <= CRYPT_ATTRIBUTE_NONE || \
		attributeType >= CRYPT_ATTRIBUTE_LAST )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptHandle != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptHandle;
	cmd.arg[ 1 ] = attributeType;
	cmd.arg[ 2 ] = value;
	status = DISPATCH_COMMAND( cmdSetAttribute, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_RET cryptSetAttributeString( C_IN CRYPT_HANDLE cryptHandle,
							   C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
							   C_IN void C_PTR value, C_IN int valueLength )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_SETATTRIBUTE, COMMAND_FLAG_NONE, 2, 1,
		  { DEFAULTUSER_OBJECT_HANDLE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_S, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE valueBuffer[ MAX_ATTRIBUTE_SIZE ];
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int length = valueLength, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) && cryptHandle != CRYPT_UNUSED )
		return( CRYPT_ERROR_PARAM1 );
	if( attributeType <= CRYPT_ATTRIBUTE_NONE || \
		attributeType >= CRYPT_ATTRIBUTE_LAST )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtrConst( value, 1 ) )
		return( CRYPT_ERROR_PARAM3 );
	if( attributeType == CRYPT_CTXINFO_KEY_COMPONENTS )
		{
		/* Public key components constitute a special case since the
		   composite structures used are quite large */
		if( valueLength != sizeof( CRYPT_PKCINFO_RSA ) && \
			valueLength != sizeof( CRYPT_PKCINFO_DLP ) && \
			valueLength != sizeof( CRYPT_PKCINFO_ECC ) )
			return( CRYPT_ERROR_PARAM4 );
		}
	else
		{
		if( valueLength < 1 || valueLength >= MAX_ATTRIBUTE_SIZE )
			return( CRYPT_ERROR_PARAM4 );
		}
	if( !isReadPtr( value, valueLength ) )
		return( CRYPT_ERROR_PARAM3 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	if( needsTranslation( attributeType ) )
		{
		status = nativeToCryptlibString( valueBuffer, MAX_ATTRIBUTE_SIZE,
										 value, valueLength );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM3 );
		value = valueBuffer;
  #ifdef UNICODE_CHARS
		length = status;
  #endif /* UNICODE_CHARS */
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

#if defined( __WINDOWS__ ) && defined( __WIN32__ ) && !defined( __WIN64__ )
	/* In Vista/VS 2008 Microsoft switched the Win32 time_t from 32 to 64 
	   bits, a theoretically commendable move but one that makes it 
	   impossible to create a single DLL that works with code compiled with 
	   different versions of Visual Studio or with different languages (it's
	   not a problem with Win64 since the time_t there is always 64 bits).  
	   To get around the Win32 issue we try and detect a mismatch of time_t 
	   types and transparently convert them.  We first check that it's 
	   within the range where it could be a time attribute to avoid having 
	   to iterate through the whole list on every attribute-set operation, 
	   and only then check for an exact match */
	if( attributeType >= CRYPT_CERTINFO_VALIDFROM && \
		attributeType <= CRYPT_CERTINFO_CMS_MLEXP_TIME )
		{
		if( ( attributeType == CRYPT_CERTINFO_VALIDFROM || \
			  attributeType == CRYPT_CERTINFO_VALIDTO || \
			  attributeType == CRYPT_CERTINFO_THISUPDATE || \
			  attributeType == CRYPT_CERTINFO_NEXTUPDATE || \
			  attributeType == CRYPT_CERTINFO_REVOCATIONDATE || \
			  attributeType == CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF || \
			  attributeType == CRYPT_CERTINFO_SIGG_DATEOFCERTGEN || \
			  attributeType == CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE || \
			  attributeType == CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER || \
			  attributeType == CRYPT_CERTINFO_INVALIDITYDATE || \
			  attributeType == CRYPT_CERTINFO_CMS_SIGNINGTIME || \
			  attributeType == CRYPT_CERTINFO_CMS_MLEXP_TIME ) && \
			length != sizeof( time_t ) )
			{
			void *timeValuePtr;
			time_t time64, time32;

			/* It looks like we have a time_t size mismatch, try and correct
			   it */
			if( length != 4 && length != 8 )
				{
				/* A time_t can only be 32 or 64 bits */
				return( CRYPT_ERROR_PARAM4 );
				}
			if( length == 4 )
				{
				/* time_t is 64 bits and we've been given 32, convert to a
				   64-bit value */
				time64 = *( ( unsigned int * ) value );
				timeValuePtr = &time64;
				}
			else
				{
				/* time_t is 32 bits and we've been given 64, convert to a
				   32-bit value */
				const __int64 timeValue = *( ( __int64 * ) value );
				if( timeValue < MIN_TIME_VALUE || timeValue >= ULONG_MAX )
					return( CRYPT_ERROR_PARAM3 );
				time32 = *( ( time_t * ) value );
				timeValuePtr = &time32;
				}
			memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
			cmd.arg[ 0 ] = cryptHandle;
			cmd.arg[ 1 ] = attributeType;
			cmd.strArg[ 0 ] = ( void * ) timeValuePtr;
			cmd.strArgLen[ 0 ] = sizeof( time_t );
			status = DISPATCH_COMMAND( cmdSetAttribute, cmd );
			if( cryptStatusOK( status ) )
				return( CRYPT_OK );
			return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
							  status ) );
			}
		}
#endif /* Win32 */

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptHandle != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptHandle;
	cmd.arg[ 1 ] = attributeType;
	cmd.strArg[ 0 ] = ( void * ) value;
	cmd.strArgLen[ 0 ] = length;
	status = DISPATCH_COMMAND( cmdSetAttribute, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Delete an attribute */

C_RET cryptDeleteAttribute( C_IN CRYPT_HANDLE cryptHandle,
							C_IN CRYPT_ATTRIBUTE_TYPE attributeType )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_DELETEATTRIBUTE, COMMAND_FLAG_NONE, 2, 0,
		  { DEFAULTUSER_OBJECT_HANDLE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptHandle ) && cryptHandle != CRYPT_UNUSED )
		return( CRYPT_ERROR_PARAM1 );
	if( attributeType <= CRYPT_ATTRIBUTE_NONE || \
		attributeType >= CRYPT_ATTRIBUTE_LAST )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptHandle != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptHandle;
	cmd.arg[ 1 ] = attributeType;
	status = DISPATCH_COMMAND( cmdDeleteAttribute, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/****************************************************************************
*																			*
*								Encryption Functions						*
*																			*
****************************************************************************/

/* Generate a key into an encryption context */

C_CHECK_RETVAL \
C_RET cryptGenerateKey( C_IN CRYPT_CONTEXT cryptContext )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GENKEY, COMMAND_FLAG_NONE, 1, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptContext ) )
		return( CRYPT_ERROR_PARAM1 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = cryptContext;
	status = DISPATCH_COMMAND( cmdGenKey, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Encrypt/decrypt data */

C_RET cryptEncrypt( C_IN CRYPT_CONTEXT cryptContext,
					C_INOUT void C_PTR buffer,
					C_IN int length )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_ENCRYPT, COMMAND_FLAG_NONE, 1, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_S, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking.  In theory we should also
	   check for writability since the encryption does an in-place update,
	   however when we're hashing data it's valid for the data to be read-
	   only so we only check for readability.  In addition when hashing we
	   could be doing a hash-wrapup call so we allow a zero length and only
	   check the buffer if the length is nonzero */
	if( !isHandleRangeValid( cryptContext ) )
		return( CRYPT_ERROR_PARAM1 );
	if( length < 0 || length >= MAX_INTLENGTH )
		return( CRYPT_ERROR_PARAM3 );
	if( length > 0 && !isReadPtr( buffer, length ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = cryptContext;
	cmd.strArg[ 0 ] = ( length == 0 ) ? "" : buffer;
	cmd.strArgLen[ 0 ] = length;
	status = DISPATCH_COMMAND( cmdEncrypt, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_RET cryptDecrypt( C_IN CRYPT_CONTEXT cryptContext,
					C_INOUT void C_PTR buffer,
					C_IN int length )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_DECRYPT, COMMAND_FLAG_NONE, 1, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_S, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( cryptContext ) )
		return( CRYPT_ERROR_PARAM1 );
	if( length < 0 || length >= MAX_INTLENGTH )
		return( CRYPT_ERROR_PARAM3 );
	if( !isWritePtr( buffer, length ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = cryptContext;
	cmd.strArg[ 0 ] = buffer;
	cmd.strArgLen[ 0 ] = length;
	status = DISPATCH_COMMAND( cmdDecrypt, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/****************************************************************************
*																			*
*								Certificate Functions						*
*																			*
****************************************************************************/

#ifdef USE_CERTIFICATES

/* Sign/sig.check a certificate object.  The possibilities for signing are 
   as follows:

						Signer
	Type  |		Cert				Chain
	------+--------------------+---------------
	Cert  | Cert			   | Cert
		  |					   |
	Chain | Chain, length = 2  | Chain, length = n+1

   For sig.checking the certificate object is checked against an issuing 
   key/certificate or against a CRL, either as a raw CRL or a keyset contain 
   revocation information */

C_CHECK_RETVAL \
C_RET cryptSignCert( C_IN CRYPT_CERTIFICATE certificate,
					 C_IN CRYPT_CONTEXT signContext )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CERTSIGN, COMMAND_FLAG_NONE, 2, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isHandleRangeValid( signContext ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = certificate;
	cmd.arg[ 1 ] = signContext;
	status = DISPATCH_COMMAND( cmdCertSign, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL \
C_RET cryptCheckCert( C_IN CRYPT_HANDLE certificate,
					  C_IN CRYPT_HANDLE sigCheckKey )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CERTCHECK, COMMAND_FLAG_NONE, 2, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_V, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isHandleRangeValid( sigCheckKey ) && \
		( sigCheckKey != CRYPT_UNUSED ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = certificate;
	cmd.arg[ 1 ] = sigCheckKey;
	status = DISPATCH_COMMAND( cmdCertCheck, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Import/export a certificate, CRL, certification request, or certificate 
   chain.  In the export case this just copies the internal encoded object 
   to an external buffer.  For certificate/certificate chain export the 
   possibilities are as follows:

						Export
	Type  |		Cert				Chain
	------+--------------------+---------------
	Cert  | Cert			   | Cert as chain
		  |					   |
	Chain | Currently selected | Chain
		  | cert in chain	   | */

C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 4 ) ) \
C_RET cryptImportCert( C_IN void C_PTR certObject,
					   C_IN int certObjectLength,
					   C_IN CRYPT_USER cryptUser,
					   C_OUT CRYPT_CERTIFICATE C_PTR certificate )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CREATEOBJECT_INDIRECT, COMMAND_FLAG_NONE, 2, 1,
		  { SYSTEM_OBJECT_HANDLE, OBJECT_TYPE_CERTIFICATE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_S, ARG_N, ARG_N, ARG_O, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( certObjectLength < MIN_CERTSIZE || \
		certObjectLength >= MAX_BUFFER_SIZE )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtr( certObject, certObjectLength ) )
		return( CRYPT_ERROR_PARAM1 );
	if( cryptUser != CRYPT_UNUSED && !isHandleRangeValid( cryptUser ) )
		return( CRYPT_ERROR_PARAM3 );
	if( !isWritePtrConst( certificate, sizeof( CRYPT_CERTIFICATE ) ) )
		return( CRYPT_ERROR_PARAM4 );
	*certificate = CRYPT_ERROR;

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptUser != CRYPT_UNUSED )
		cmd.arg[ 0 ] = cryptUser;
	cmd.strArg[ 0 ] = ( void * ) certObject;
	cmd.strArgLen[ 0 ] = certObjectLength;
	status = DISPATCH_COMMAND( cmdCreateObjectIndirect, cmd );
	if( cryptStatusOK( status ) )
		{
		*certificate = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL \
C_RET cryptExportCert( C_OUT_OPT void C_PTR certObject,
					   C_IN int certObjectMaxLength,
					   C_OUT int C_PTR certObjectLength,
					   C_IN CRYPT_CERTFORMAT_TYPE certFormatType,
					   C_IN CRYPT_HANDLE certificate )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_EXPORTOBJECT, COMMAND_FLAG_NONE, 2, RETURN_VALUE( 1 ) };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_S, ARG_D, ARG_N, ARG_V, ARG_O, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( certObject != NULL )
		{
		if( certObjectMaxLength < MIN_CERTSIZE || \
			certObjectMaxLength >= MAX_BUFFER_SIZE )
			return( CRYPT_ERROR_PARAM2 );
		if( !isWritePtr( certObject, certObjectMaxLength ) )
			return( CRYPT_ERROR_PARAM1 );
		memset( certObject, 0, MIN_CERTSIZE );
		}
	if( !isWritePtrConst( certObjectLength, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM3 );
	*certObjectLength = CRYPT_ERROR;
	if( certFormatType <= CRYPT_CERTFORMAT_NONE || \
		certFormatType >= CRYPT_CERTFORMAT_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM4 );
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM5 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( certObject == NULL )
		{
		cmd.flags = COMMAND_FLAG_RET_LENGTH;
		cmd.noStrArgs = 0;
		}
	cmd.arg[ 0 ] = certificate;
	cmd.arg[ 1 ] = certFormatType;
	cmd.strArg[ 0 ] = certObject;
	cmd.strArgLen[ 0 ] = certObjectMaxLength;
	status = DISPATCH_COMMAND( cmdExportObject, cmd );
	if( cryptStatusOK( status ) )
		{
		*certObjectLength = ( certObject == NULL ) ? \
							cmd.arg[ 0 ] : cmd.strArgLen[ 0 ];
#if defined( EBCDIC_CHARS )
		if( ( certFormatType == CRYPT_CERTFORMAT_TEXT_CERTIFICATE || \
			  certFormatType == CRYPT_CERTFORMAT_TEXT_CERTCHAIN || \
			  certFormatType == CRYPT_CERTFORMAT_XML_CERTIFICATE || \
			  certFormatType == CRYPT_CERTFORMAT_XML_CERTCHAIN ) && \
			certObject != NULL )
			{
			/* It's text-encoded certificate data, convert it to the native 
			   text format before we return */
			cryptlibToNativeString( certObject, certObjectMaxLength,
									certObject, *certObjectLength );
			}
#endif /* EBCDIC_CHARS */
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

#ifdef USE_KEYSETS

/* CA management functions */

C_CHECK_RETVAL \
C_RET cryptCAAddItem( C_IN CRYPT_KEYSET keyset,
					  C_IN CRYPT_CERTIFICATE certificate )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_SETKEY, COMMAND_FLAG_NONE, 3, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = certificate;
	cmd.arg[ 2 ] = TRUE;
	status = DISPATCH_COMMAND( cmdSetKey, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) \
C_RET cryptCAGetItem( C_IN CRYPT_KEYSET keyset,
					  C_OUT CRYPT_CERTIFICATE C_PTR certificate,
					  C_IN CRYPT_CERTTYPE_TYPE certType,
					  C_IN CRYPT_KEYID_TYPE keyIDtype,
					  C_IN_OPT C_STR keyID )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETKEY, COMMAND_FLAG_NONE, 3, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_D, ARG_N, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = NULL;
#else
	const char *keyIDPtr = keyID;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	BOOLEAN isCert = FALSE;
	int keyIDlen = 0, status;

	/* Perform basic client-side error checking.  Because of keyset queries 
	   we have to accept CRYPT_KEYID_NONE and a null keyID as well as 
	   obviously valid key IDs */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( certificate, sizeof( CRYPT_HANDLE ) ) )
		return( CRYPT_ERROR_PARAM2 );
	*certificate = CRYPT_ERROR;
	if( certType == CRYPT_CERTTYPE_CERTIFICATE || \
		certType == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		certType == CRYPT_CERTTYPE_CERTCHAIN )
		isCert = TRUE;
	else
		{
		if( certType != CRYPT_CERTTYPE_CERTREQUEST && \
			certType != CRYPT_CERTTYPE_REQUEST_CERT && \
			certType != CRYPT_CERTTYPE_REQUEST_REVOCATION && \
			certType != CRYPT_CERTTYPE_PKIUSER )
			return( CRYPT_ERROR_PARAM3 );
		}
	if( keyIDtype < CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM4 );
	if( keyIDtype == CRYPT_KEYID_NONE )
		{
		if( keyID != NULL )
			return( CRYPT_ERROR_PARAM5 );
		}
	else
		{
		if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
			strParamLen( keyID ) < MIN_NAME_LENGTH || \
			strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
			return( CRYPT_ERROR_PARAM5 );
		}

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	if( keyID != NULL )
		{
		status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
										 keyID, nativeStrlen( keyID ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM4 );
		keyIDPtr = keyIDBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	if( keyIDPtr != NULL )
		{
		keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
		if( keyIDlen <= 0 )
			return( CRYPT_ERROR_PARAM5 );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	if( isCert )
		{
		/* If we're being asked for a standard certificate, the caller 
		   should really be using cryptGetPublicKey(), however for 
		   orthogonality we convert the request into a standard public-key 
		   read.  Note that this leads to some ambiguity since we can't 
		   explicitly specify what we want returned for a certificate read 
		   (for example we could get a chain if we ask for a single 
		   certificate or a single certificate if we ask for a chain, 
		   depending on what's there), but it's less confusing than refusing 
		   any request to read a certificate */
		cmd.arg[ 1 ] = KEYMGMT_ITEM_PUBLICKEY;
		}
	else
		{
		cmd.arg[ 1 ] = ( certType == CRYPT_CERTTYPE_PKIUSER ) ? \
					   KEYMGMT_ITEM_PKIUSER : KEYMGMT_ITEM_REQUEST;
		}
	cmd.arg[ 2 ] = keyIDtype;
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen ;
	status = DISPATCH_COMMAND( cmdGetKey, cmd );
	if( cryptStatusOK( status ) )
		{
		*certificate = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_RET cryptCADeleteItem( C_IN CRYPT_KEYSET keyset,
						 C_IN CRYPT_CERTTYPE_TYPE certType,
						 C_IN CRYPT_KEYID_TYPE keyIDtype,
						 C_IN C_STR keyID )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_DELETEKEY, COMMAND_FLAG_NONE, 3, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = keyIDBuffer;
#else
	const char *keyIDPtr = keyID;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int keyIDlen, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( certType != CRYPT_CERTTYPE_CERTIFICATE && \
		certType != CRYPT_CERTTYPE_CERTREQUEST && \
		certType != CRYPT_CERTTYPE_REQUEST_CERT && \
		certType != CRYPT_CERTTYPE_REQUEST_REVOCATION && \
		certType != CRYPT_CERTTYPE_PKIUSER )
		return( CRYPT_ERROR_PARAM2 );
	if( keyIDtype <= CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );
	if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
		strParamLen( keyID ) < MIN_NAME_LENGTH || \
		strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM4 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
									 keyID, nativeStrlen( keyID ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM4 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
	if( keyIDlen <= 0 )
		return( CRYPT_ERROR_PARAM4 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = keyIDtype;
	if( certType == CRYPT_CERTTYPE_CERTIFICATE )
		{
		/* Allow a delete of a certificate for the same reason as given above 
		   for cryptCAGetItem() */
		cmd.noArgs = 2;
		}
	else
		{
		cmd.arg[ 2 ] = ( certType == CRYPT_CERTTYPE_PKIUSER ) ? \
					   CRYPT_CERTTYPE_PKIUSER : CRYPT_CERTTYPE_REQUEST_CERT;
		}
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen;
	status = DISPATCH_COMMAND( cmdDeleteKey, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL \
C_RET cryptCACertManagement( C_OUT_OPT CRYPT_CERTIFICATE C_PTR certificate,
							 C_IN CRYPT_CERTACTION_TYPE action,
							 C_IN CRYPT_KEYSET keyset,
							 C_IN CRYPT_CONTEXT caKey,
							 C_IN CRYPT_CERTIFICATE certRequest )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_CERTMGMT, COMMAND_FLAG_NONE, 4, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_D, ARG_V, ARG_O, ARG_N, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( certificate != NULL )
		{
		if( !isWritePtrConst( certificate, sizeof( CRYPT_HANDLE ) ) )
			return( CRYPT_ERROR_PARAM1 );
		*certificate = CRYPT_ERROR;
		}
	if( action < CRYPT_CERTACTION_FIRST_USER || \
		action > CRYPT_CERTACTION_LAST_USER )
		return( CRYPT_ERROR_PARAM2 );
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM3 );
	if( !isHandleRangeValid( caKey ) && \
		!( ( action == CRYPT_CERTACTION_EXPIRE_CERT || \
			 action == CRYPT_CERTACTION_CLEANUP ) && caKey == CRYPT_UNUSED ) )
		return( CRYPT_ERROR_PARAM4 );
	if( !isHandleRangeValid( certRequest ) && \
		!( ( action == CRYPT_CERTACTION_ISSUE_CRL || \
			 action == CRYPT_CERTACTION_EXPIRE_CERT || \
			 action == CRYPT_CERTACTION_CLEANUP ) && \
		   certRequest == CRYPT_UNUSED ) )
		return( CRYPT_ERROR_PARAM5 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( certificate == NULL )
		cmd.flags = COMMAND_FLAG_RET_NONE;
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = action;
	cmd.arg[ 2 ] = caKey;
	cmd.arg[ 3 ] = certRequest;
	status = DISPATCH_COMMAND( cmdCertMgmt, cmd );
	if( cryptStatusOK( status ) && certificate != NULL )
		{
		*certificate = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_KEYSETS */
#endif /* USE_CERTIFICATES */

/****************************************************************************
*																			*
*							Envelope/Session Functions						*
*																			*
****************************************************************************/

#if defined( USE_ENVELOPES ) || defined( USE_SESSIONS )

/* Push data into an envelope/session object */

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) \
C_RET cryptPushData( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
					 C_IN int length, C_OUT int C_PTR bytesCopied )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_PUSHDATA, COMMAND_FLAG_NONE, 1, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_S, ARG_N, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int dummy, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( envelope ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isReadPtr( buffer, length ) )
		return( CRYPT_ERROR_PARAM2 );
	if( length <= 0 || length >= MAX_BUFFER_SIZE )
		return( CRYPT_ERROR_PARAM3 );
	if( bytesCopied == NULL )
		{
		/* If the user isn't interested in the bytes copied count, point at a
		   dummy location */
		bytesCopied = &dummy;
		}
	*bytesCopied = 0;

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = envelope;
	cmd.strArg[ 0 ] = ( void * ) buffer;
	cmd.strArgLen[ 0 ] = length;
	status = DISPATCH_COMMAND( cmdPushData, cmd );
	*bytesCopied = cmd.arg[ 0 ];
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Pop data from an envelope/session object */

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) \
C_RET cryptPopData( C_IN CRYPT_ENVELOPE envelope, C_OUT void C_PTR buffer,
					C_IN int length, C_OUT int C_PTR bytesCopied )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_POPDATA, COMMAND_FLAG_NONE, 2, RETURN_VALUE( 1 ) };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_S, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( envelope ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtr( buffer, length ) )
		return( CRYPT_ERROR_PARAM2 );
	if( length <= 0 || length >= MAX_BUFFER_SIZE )
		return( CRYPT_ERROR_PARAM3 );
	memset( buffer, 0, min( length, 16 ) );
	if( !isWritePtrConst( bytesCopied, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM4 );
	*bytesCopied = 0;

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = envelope;
	cmd.arg[ 1 ] = length;
	cmd.strArg[ 0 ] = ( void * ) buffer;
	cmd.strArgLen[ 0 ] = RETURN_VALUE( length );
	status = DISPATCH_COMMAND( cmdPopData, cmd );
	*bytesCopied = cmd.strArgLen[ 0 ];
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Flush data through an envelope/session object */

C_CHECK_RETVAL \
C_RET cryptFlushData( C_IN CRYPT_HANDLE envelope )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_FLUSHDATA, COMMAND_FLAG_NONE, 1, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( envelope ) )
		return( CRYPT_ERROR_PARAM1 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = envelope;
	status = DISPATCH_COMMAND( cmdFlushData, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_ENVELOPES || USE_SESSIONS */

/****************************************************************************
*																			*
*								Keyset Functions							*
*																			*
****************************************************************************/

#ifdef USE_KEYSETS

/* Retrieve a key from a keyset or equivalent object */

C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) \
C_RET cryptGetPublicKey( C_IN CRYPT_KEYSET keyset,
						 C_OUT CRYPT_HANDLE C_PTR cryptKey,
						 C_IN CRYPT_KEYID_TYPE keyIDtype,
						 C_IN_OPT C_STR keyID )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETKEY, COMMAND_FLAG_NONE, 3, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_D, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = NULL;
#else
	const char *keyIDPtr = keyID;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int keyIDlen = 0, status;

	/* Perform basic client-side error checking.  Because of keyset queries
	   we have to accept CRYPT_KEYID_NONE and a null keyID as well as
	   obviously valid key IDs */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( cryptKey, sizeof( CRYPT_HANDLE ) ) )
		return( CRYPT_ERROR_PARAM2 );
	*cryptKey = CRYPT_ERROR;
	if( keyIDtype < CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );
	if( keyIDtype == CRYPT_KEYID_NONE )
		{
		if( keyID != NULL )
			return( CRYPT_ERROR_PARAM4 );
		}
	else
		{
		if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
			strParamLen( keyID ) < MIN_NAME_LENGTH || \
			strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
			return( CRYPT_ERROR_PARAM4 );
		}

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	if( keyID != NULL )
		{
		status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
										 keyID, nativeStrlen( keyID ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM4 );
		keyIDPtr = keyIDBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	if( keyIDPtr != NULL )
		{
		keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
		if( keyIDlen <= 0 )
			return( CRYPT_ERROR_PARAM4 );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = KEYMGMT_ITEM_PUBLICKEY;
	cmd.arg[ 2 ] = keyIDtype;
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen;
	status = DISPATCH_COMMAND( cmdGetKey, cmd );
	if( cryptStatusOK( status ) )
		{
		*cryptKey = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) \
C_RET cryptGetPrivateKey( C_IN CRYPT_HANDLE keyset,
						  C_OUT CRYPT_CONTEXT C_PTR cryptContext,
						  C_IN CRYPT_KEYID_TYPE keyIDtype,
						  C_IN C_STR keyID, C_IN_OPT C_STR password )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETKEY, COMMAND_FLAG_NONE, 3, 2 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_D, ARG_N, ARG_S, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = keyIDBuffer;
	BYTE passwordBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *passwordPtr = NULL;
#else
	const char *keyIDPtr = keyID, *passwordPtr = password;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int keyIDlen, passwordLen = 0, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( cryptContext, sizeof( CRYPT_CONTEXT ) ) )
		return( CRYPT_ERROR_PARAM2 );
	*cryptContext = CRYPT_ERROR;
	if( keyIDtype <= CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );
	if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
		strParamLen( keyID ) < MIN_NAME_LENGTH || \
		strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM4 );
	if( password != NULL && \
		( !isReadPtrConst( password, MIN_NAME_LENGTH ) || \
		  strParamLen( password ) < MIN_NAME_LENGTH || \
		  strParamLen( password ) >= MAX_ATTRIBUTE_SIZE ) )
		return( CRYPT_ERROR_PARAM5 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
									 keyID, nativeStrlen( keyID ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM4 );
	if( password != NULL )
		{
		status = nativeToCryptlibString( passwordBuffer, MAX_ATTRIBUTE_SIZE,
										 password, nativeStrlen( password ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM5 );
		passwordPtr = passwordBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
	if( keyIDlen <= 0 )
		return( CRYPT_ERROR_PARAM4 );
	if( passwordPtr != NULL )
		{
		passwordLen = strStripWhitespace( &passwordPtr, passwordPtr, 
										  strlen( passwordPtr ) );
		if( passwordLen <= 0 )
			return( CRYPT_ERROR_PARAM5 );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = KEYMGMT_ITEM_PRIVATEKEY;
	cmd.arg[ 2 ] = keyIDtype;
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen;
	cmd.strArg[ 1 ] = ( void * ) passwordPtr;
	cmd.strArgLen[ 1 ] = passwordLen;
	status = DISPATCH_COMMAND( cmdGetKey, cmd );
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	zeroise( passwordBuffer, MAX_ATTRIBUTE_SIZE + 1 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	if( cryptStatusOK( status ) )
		{
		*cryptContext = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) \
C_RET cryptGetKey( C_IN CRYPT_HANDLE keyset,
				   C_OUT CRYPT_CONTEXT C_PTR cryptContext,
				   C_IN CRYPT_KEYID_TYPE keyIDtype, C_IN C_STR keyID, 
				   C_IN_OPT C_STR password )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_GETKEY, COMMAND_FLAG_NONE, 3, 2 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_D, ARG_N, ARG_S, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = keyIDBuffer;
	BYTE passwordBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *passwordPtr = NULL;
#else
	const char *keyIDPtr = keyID, *passwordPtr = password;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int keyIDlen, passwordLen = 0, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( cryptContext, sizeof( CRYPT_CONTEXT ) ) )
		return( CRYPT_ERROR_PARAM2 );
	*cryptContext = CRYPT_ERROR;
	if( keyIDtype <= CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM3 );
	if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
		strParamLen( keyID ) < MIN_NAME_LENGTH || \
		strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM4 );
	if( password != NULL && \
		( !isReadPtrConst( password, MIN_NAME_LENGTH ) || \
		  strParamLen( password ) < MIN_NAME_LENGTH || \
		  strParamLen( password ) >= MAX_ATTRIBUTE_SIZE ) )
		return( CRYPT_ERROR_PARAM5 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
									 keyID, nativeStrlen( keyID ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM4 );
	if( password != NULL )
		{
		status = nativeToCryptlibString( passwordBuffer, MAX_ATTRIBUTE_SIZE,
										 password, nativeStrlen( password ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM5 );
		passwordPtr = passwordBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
	if( keyIDlen <= 0 )
		return( CRYPT_ERROR_PARAM4 );
	if( passwordPtr != NULL )
		{
		passwordLen = strStripWhitespace( &passwordPtr, passwordPtr, 
										  strlen( passwordPtr ) );
		if( passwordLen <= 0 )
			return( CRYPT_ERROR_PARAM5 );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = KEYMGMT_ITEM_SECRETKEY;
	cmd.arg[ 2 ] = keyIDtype;
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen;
	cmd.strArg[ 1 ] = ( void * ) passwordPtr;
	cmd.strArgLen[ 1 ] = passwordLen;
	status = DISPATCH_COMMAND( cmdGetKey, cmd );
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	zeroise( passwordBuffer, MAX_ATTRIBUTE_SIZE + 1 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	if( cryptStatusOK( status ) )
		{
		*cryptContext = cmd.arg[ 0 ];
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Add a key from a keyset or equivalent object */

C_CHECK_RETVAL \
C_RET cryptAddPublicKey( C_IN CRYPT_KEYSET keyset,
						 C_IN CRYPT_CERTIFICATE certificate )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_SETKEY, COMMAND_FLAG_NONE, 2, 0 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = certificate;
	status = DISPATCH_COMMAND( cmdSetKey, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) \
C_RET cryptAddPrivateKey( C_IN CRYPT_KEYSET keyset,
						  C_IN CRYPT_HANDLE cryptKey,
						  C_IN C_STR password )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_SETKEY, COMMAND_FLAG_NONE, 2, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE passwordBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *passwordPtr = NULL;
#else
	const char *passwordPtr = password;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int passwordLen = 0, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isHandleRangeValid( cryptKey ) )
		return( CRYPT_ERROR_PARAM2 );
	if( password != NULL && \
		( !isReadPtrConst( password, MIN_NAME_LENGTH ) || \
		  isBadPassword( password ) || \
		  strParamLen( password ) < MIN_NAME_LENGTH || \
		  strParamLen( password ) >= MAX_ATTRIBUTE_SIZE ) )
		return( CRYPT_ERROR_PARAM3 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	if( password != NULL )
		{
		status = nativeToCryptlibString( passwordBuffer, MAX_ATTRIBUTE_SIZE,
										 password, nativeStrlen( password ) + 1 );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM3 );
		passwordPtr = passwordBuffer;
		}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	if( passwordPtr != NULL )
		{
		passwordLen = strStripWhitespace( &passwordPtr, passwordPtr, 
										  strlen( passwordPtr ) );
		if( passwordLen <= 0 )
			return( CRYPT_ERROR_PARAM3 );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = cryptKey;
	cmd.strArg[ 0 ] = ( void * ) passwordPtr;
	cmd.strArgLen[ 0 ] = passwordLen;
	status = DISPATCH_COMMAND( cmdSetKey, cmd );
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	zeroise( passwordBuffer, MAX_ATTRIBUTE_SIZE + 1 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

/* Delete a key from a keyset or equivalent object */

C_RET cryptDeleteKey( C_IN CRYPT_KEYSET keyset,
					  C_IN CRYPT_KEYID_TYPE keyIDtype,
					  C_IN C_STR keyID )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_DELETEKEY, COMMAND_FLAG_NONE, 2, 1 };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	BYTE keyIDBuffer[ MAX_ATTRIBUTE_SIZE + 1 ], *keyIDPtr = keyIDBuffer;
#else
	const char *keyIDPtr = keyID;
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
	int keyIDlen, status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( keyset ) )
		return( CRYPT_ERROR_PARAM1 );
	if( keyIDtype <= CRYPT_KEYID_NONE || \
		keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtrConst( keyID, MIN_NAME_LENGTH ) || \
		strParamLen( keyID ) < MIN_NAME_LENGTH || \
		strParamLen( keyID ) >= MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM3 );

#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
	status = nativeToCryptlibString( keyIDBuffer, MAX_ATTRIBUTE_SIZE,
									 keyID, nativeStrlen( keyID ) + 1 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM3 );
#endif /* EBCDIC_CHARS || UNICODE_CHARS */

	/* Check and clean up any string parameters if required */
	keyIDlen = strStripWhitespace( &keyIDPtr, keyIDPtr, strlen( keyIDPtr ) );
	if( keyIDlen <= 0 )
		return( CRYPT_ERROR_PARAM3 );

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	cmd.arg[ 0 ] = keyset;
	cmd.arg[ 1 ] = keyIDtype;
	cmd.strArg[ 0 ] = ( void * ) keyIDPtr;
	cmd.strArgLen[ 0 ] = keyIDlen;
	status = DISPATCH_COMMAND( cmdDeleteKey, cmd );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_KEYSETS */

/****************************************************************************
*																			*
*							User Management Functions						*
*																			*
****************************************************************************/

/****************************************************************************
*																			*
*									Misc Functions							*
*																			*
****************************************************************************/

/* cryptlib/object query functions */

C_CHECK_RETVAL \
C_RET cryptQueryCapability( C_IN CRYPT_ALGO_TYPE cryptAlgo,
							C_OUT_OPT CRYPT_QUERY_INFO C_PTR cryptQueryInfo )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_QUERYCAPABILITY, COMMAND_FLAG_NONE, 2, RETURN_VALUE( 1 ),
		  { SYSTEM_OBJECT_HANDLE } };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_N, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( cryptAlgo < CRYPT_ALGO_NONE || \
		cryptAlgo >= CRYPT_ALGO_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM1 );
	if( cryptQueryInfo != NULL )
		{
		if( !isWritePtrConst( cryptQueryInfo, sizeof( CRYPT_QUERY_INFO ) ) )
			return( CRYPT_ERROR_PARAM3 );
		memset( cryptQueryInfo, 0, sizeof( CRYPT_QUERY_INFO ) );
		}

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* Dispatch the command.  We need to map parameter errors down one
	   because the system object is invisible to the caller */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptQueryInfo == NULL )
		{
		cmd.flags = COMMAND_FLAG_RET_LENGTH;
		cmd.noStrArgs = 0;
		}
	cmd.arg[ 1 ] = cryptAlgo;
	cmd.strArg[ 0 ] = cryptQueryInfo;
	cmd.strArgLen[ 0 ] = RETURN_VALUE( MAX_ATTRIBUTE_SIZE );
	status = DISPATCH_COMMAND( cmdQueryCapability, cmd );
	if( cryptStatusOK( status ) )
		{
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
		if( cryptQueryInfo != NULL )
			{
			BYTE buffer[ MAX_ATTRIBUTE_SIZE ];
			const int algoNameLength = \
						strlen( ( char * ) cryptQueryInfo->algoName ) + 1;

			memcpy( buffer, cryptQueryInfo->algoName, algoNameLength );
			cryptlibToNativeString( cryptQueryInfo->algoName,
									CRYPT_MAX_TEXTSIZE, buffer, 
									algoNameLength );
			}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}

#ifdef USE_DEVICES

C_CHECK_RETVAL \
C_RET cryptDeviceQueryCapability( C_IN CRYPT_DEVICE device,
								  C_IN CRYPT_ALGO_TYPE cryptAlgo,
								  C_OUT_OPT CRYPT_QUERY_INFO C_PTR cryptQueryInfo )
	{
	static const COMMAND_INFO FAR_BSS cmdTemplate = \
		{ COMMAND_QUERYCAPABILITY, COMMAND_FLAG_NONE, 2, RETURN_VALUE( 1 ) };
	static const ERRORMAP FAR_BSS errorMap[] = \
		{ ARG_O, ARG_N, ARG_N, ARG_S, ARG_LAST, ARG_LAST };
	COMMAND_INFO cmd;
	int status;

	/* Perform basic client-side error checking */
	if( !isHandleRangeValid( device ) )
		return( CRYPT_ERROR_PARAM1 );
	if( cryptAlgo < CRYPT_ALGO_NONE || \
		cryptAlgo >= CRYPT_ALGO_LAST_EXTERNAL )
		return( CRYPT_ERROR_PARAM2 );
	if( cryptQueryInfo != NULL )
		{
		if( !isWritePtrConst( cryptQueryInfo, sizeof( CRYPT_QUERY_INFO ) ) )
			return( CRYPT_ERROR_PARAM4 );
		memset( cryptQueryInfo, 0, sizeof( CRYPT_QUERY_INFO ) );
		}

	/* Dispatch the command */
	memcpy( &cmd, &cmdTemplate, sizeof( COMMAND_INFO ) );
	if( cryptQueryInfo == NULL )
		{
		cmd.flags = COMMAND_FLAG_RET_LENGTH;
		cmd.noStrArgs = 0;
		}
	cmd.arg[ 0 ] = device;
	cmd.arg[ 1 ] = cryptAlgo;
	cmd.strArg[ 0 ] = cryptQueryInfo;
	cmd.strArgLen[ 0 ] = RETURN_VALUE( MAX_ATTRIBUTE_SIZE );
	status = DISPATCH_COMMAND( cmdQueryCapability, cmd );
	if( cryptStatusOK( status ) )
		{
#if defined( EBCDIC_CHARS ) || defined( UNICODE_CHARS )
		if( cryptQueryInfo != NULL )
			{
			BYTE buffer[ MAX_ATTRIBUTE_SIZE ];
			const int algoNameLength = \
						strlen( ( char * ) cryptQueryInfo->algoName ) + 1;

			memcpy( buffer, cryptQueryInfo->algoName, algoNameLength );
			cryptlibToNativeString( cryptQueryInfo->algoName,
									CRYPT_MAX_TEXTSIZE, buffer, 
									algoNameLength );
			}
#endif /* EBCDIC_CHARS || UNICODE_CHARS */
		return( CRYPT_OK );
		}
	return( mapError( errorMap, FAILSAFE_ARRAYSIZE( errorMap, ERRORMAP ), 
					  status ) );
	}
#endif /* USE_DEVICES */

/* Add random data to the random pool.  This should eventually be replaced
   by some sort of device control mechanism, the problem with doing this is
   that it's handled by the system device which isn't visible to the user */

C_RET cryptAddRandom( C_IN void C_PTR randomData, C_IN int randomDataLength )
	{
	/* Perform basic error checking */
	if( randomData == NULL )
		{
		if( randomDataLength != CRYPT_RANDOM_FASTPOLL && \
			randomDataLength != CRYPT_RANDOM_SLOWPOLL )
			return( CRYPT_ERROR_PARAM1 );
		}
	else
		{
		if( randomDataLength <= 0 || randomDataLength >= MAX_BUFFER_SIZE )
			return( CRYPT_ERROR_PARAM2 );
		if( !isReadPtr( randomData, randomDataLength ) )
			return( CRYPT_ERROR_PARAM1 );
		}

	/* Make sure that the user has remembered to initialise cryptlib */
	if( !initCalled )
		return( CRYPT_ERROR_NOTINITED );

	/* If we're adding data to the pool, add it now and exit.  Since the data
	   is of unknown provenance (and empirical evidence indicates that it
	   won't be very random) we give it a weight of zero for estimation
	   purposes */
	if( randomData != NULL )
		{
		MESSAGE_DATA msgData;

#if !defined( NDEBUG ) && ( defined( __WINDOWS__ ) || defined( CONFIG_FUZZ ) )
if( randomDataLength == 5 && !memcmp( randomData, "xyzzy", 5 ) )
{	/* For debugging tests only */
BYTE buffer[ 256 + 8 ];
int kludge = 100;
memset( buffer, '*', 256 );
setMessageData( &msgData, buffer, 256 );
krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE_S,
				 &msgData, CRYPT_IATTRIBUTE_ENTROPY );
krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
				 &kludge, CRYPT_IATTRIBUTE_ENTROPY_QUALITY );
return( CRYPT_OK );
}
#endif /* Windows debug */

		setMessageData( &msgData, ( MESSAGE_CAST ) randomData, 
						randomDataLength );
		return( krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								 IMESSAGE_SETATTRIBUTE_S, &msgData,
								 CRYPT_IATTRIBUTE_ENTROPY ) );
		}

	/* Perform either a fast or slow poll for random system data */
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_SETATTRIBUTE,
							 ( randomDataLength == CRYPT_RANDOM_SLOWPOLL ) ? \
								MESSAGE_VALUE_TRUE : MESSAGE_VALUE_FALSE,
							 CRYPT_IATTRIBUTE_RANDOM_POLL ) );
	}

/****************************************************************************
*																			*
*					Stubs/Replacements for Disabled Functions				*
*																			*
****************************************************************************/

/* If certain functionality is disabled then we have to provide stub 
   replacements for the functions */

#ifndef USE_CERTIFICATES

C_RET cryptCreateCert( C_OUT CRYPT_CERTIFICATE C_PTR certificate,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_CERTTYPE_TYPE certType )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptSignCert( C_IN CRYPT_CERTIFICATE certificate,
					 C_IN CRYPT_CONTEXT signContext )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptCheckCert( C_IN CRYPT_HANDLE certificate,
					  C_IN CRYPT_HANDLE sigCheckKey )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptImportCert( C_IN void C_PTR certObject,
					   C_IN int certObjectLength,
					   C_IN CRYPT_USER cryptUser,
					   C_OUT CRYPT_CERTIFICATE C_PTR certificate )
	{
	UNUSED_ARG( certObject );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptExportCert( C_OUT_OPT void C_PTR certObject,
					   C_IN int certObjectMaxLength,
					   C_OUT int C_PTR certObjectLength,
					   C_IN CRYPT_CERTFORMAT_TYPE certFormatType,
					   C_IN CRYPT_HANDLE certificate )
	{
	UNUSED_ARG( certObject );
	UNUSED_ARG( certObjectLength );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptGetCertExtension( C_IN CRYPT_CERTIFICATE certificate,
							 C_IN char C_PTR oid,
							 C_OUT int C_PTR criticalFlag,
							 C_OUT void C_PTR extension,
							 C_IN int extensionMaxLength,
							 C_OUT int C_PTR extensionLength )
	{
	UNUSED_ARG( oid );
	UNUSED_ARG( criticalFlag );
	UNUSED_ARG( extension );
	UNUSED_ARG( extensionLength );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptAddCertExtension( C_IN CRYPT_CERTIFICATE certificate,
							 C_IN char C_PTR oid, C_IN int criticalFlag,
							 C_IN void C_PTR extension,
							 C_IN int extensionLength )
	{
	UNUSED_ARG( oid );
	UNUSED_ARG( extension );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptDeleteCertExtension( C_IN CRYPT_CERTIFICATE certificate,
								C_IN char C_PTR oid )
	{
	UNUSED_ARG( oid );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_CERTIFICATES */

#if !defined( USE_CERTIFICATES ) || !defined( USE_KEYSETS )

C_RET cryptCAAddItem( C_IN CRYPT_KEYSET keyset,
					  C_IN CRYPT_CERTIFICATE certificate )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptCAGetItem( C_IN CRYPT_KEYSET keyset,
					  C_OUT CRYPT_CERTIFICATE C_PTR certificate,
					  C_IN CRYPT_CERTTYPE_TYPE certType,
					  C_IN CRYPT_KEYID_TYPE keyIDtype,
					  C_IN_OPT C_STR keyID )
	{
	UNUSED_ARG( keyID );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptCADeleteItem( C_IN CRYPT_KEYSET keyset,
						 C_IN CRYPT_CERTTYPE_TYPE certType,
						 C_IN CRYPT_KEYID_TYPE keyIDtype,
						 C_IN C_STR keyID )
	{
	UNUSED_ARG( keyID );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptCACertManagement( C_OUT_OPT CRYPT_CERTIFICATE C_PTR certificate,
							 C_IN CRYPT_CERTACTION_TYPE action,
							 C_IN CRYPT_KEYSET keyset,
							 C_IN CRYPT_CONTEXT caKey,
							 C_IN CRYPT_CERTIFICATE certRequest )
	{
	UNUSED_ARG( certificate );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_CERTIFICATES || !USE_KEYSETS */

#ifdef USE_PSEUDOCERTIFICATES 

C_RET cryptCreateAttachedCert( C_IN CRYPT_CONTEXT cryptContext,
							   C_IN void C_PTR certObject,
							   C_IN int certObjectLength )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	/* Perform basic error checking */
	if( !isHandleRangeValid( cryptContext ) )
		return( CRYPT_ERROR_PARAM1 );
	if( certObjectLength < MIN_CERTSIZE || \
		certObjectLength >= MAX_BUFFER_SIZE )
		return( CRYPT_ERROR_PARAM3 );
	if( !isReadPtr( certObject, certObjectLength ) )
		return( CRYPT_ERROR_PARAM2 );

	/* Create the pseudo-certificate object */
	setMessageCreateObjectIndirectInfo( &createInfo, certObject,
										certObjectLength, 
										CRYPT_CERTTYPE_NONE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );

	/* Attach the pseudo-certificate to the context */
	status = krnlSendMessage( cryptContext, MESSAGE_SETDEPENDENT,
							  &createInfo.cryptHandle, 
							  SETDEP_OPTION_NOINCREF );
	if( cryptStatusError( status ) )
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );

	return( status );
	}
#endif /* USE_PSEUDOCERTIFICATES */

#ifndef USE_DEVICES

C_RET cryptDeviceCreateContext( C_IN CRYPT_DEVICE device,
							    C_OUT CRYPT_CONTEXT C_PTR cryptContext,
							    C_IN CRYPT_ALGO_TYPE cryptAlgo )
	{
	UNUSED_ARG( cryptContext );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptDeviceOpen( C_OUT CRYPT_DEVICE C_PTR device,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_DEVICE_TYPE deviceType,
					   C_IN_OPT C_STR name )
	{
	UNUSED_ARG( device );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptDeviceQueryCapability( C_IN CRYPT_DEVICE device,
								  C_IN CRYPT_ALGO_TYPE cryptAlgo,
								  C_OUT_OPT CRYPT_QUERY_INFO C_PTR cryptQueryInfo )
	{
	UNUSED_ARG( cryptQueryInfo );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_DEVICES */

#ifndef USE_ENVELOPES

C_RET cryptCreateEnvelope( C_OUT CRYPT_ENVELOPE C_PTR envelope,
						   C_IN CRYPT_USER cryptUser,
						   C_IN CRYPT_FORMAT_TYPE formatType )
	{
	UNUSED_ARG( envelope );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_ENVELOPES */

#if !defined( USE_ENVELOPES ) && !defined( USE_SESSIONS )

C_RET cryptPushData( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
					 C_IN int length, C_OUT int C_PTR bytesCopied )
	{
	UNUSED_ARG( buffer );
	UNUSED_ARG( bytesCopied );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptPopData( C_IN CRYPT_ENVELOPE envelope, C_OUT void C_PTR buffer,
					C_IN int length, C_OUT int C_PTR bytesCopied )
	{
	UNUSED_ARG( buffer );
	UNUSED_ARG( bytesCopied );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptFlushData( C_IN CRYPT_HANDLE envelope )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_ENVELOPES && !USE_SESSIONS */

#ifndef USE_KEYSETS

C_RET cryptKeysetOpen( C_OUT CRYPT_KEYSET C_PTR keyset,
					   C_IN CRYPT_USER cryptUser,
					   C_IN CRYPT_KEYSET_TYPE keysetType,
					   C_IN C_STR name, C_IN CRYPT_KEYOPT_TYPE options )
	{
	UNUSED_ARG( keyset );
	UNUSED_ARG( name );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptGetPublicKey( C_IN CRYPT_KEYSET keyset,
						 C_OUT CRYPT_HANDLE C_PTR cryptKey,
						 C_IN CRYPT_KEYID_TYPE keyIDtype,
						 C_IN_OPT C_STR keyID )
	{
	UNUSED_ARG( cryptKey );
	UNUSED_ARG( keyID );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptGetPrivateKey( C_IN CRYPT_HANDLE keyset,
						  C_OUT CRYPT_CONTEXT C_PTR cryptContext,
						  C_IN CRYPT_KEYID_TYPE keyIDtype,
						  C_IN C_STR keyID, C_IN C_STR password )
	{
	UNUSED_ARG( cryptContext );
	UNUSED_ARG( keyID );
	UNUSED_ARG( password );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptGetKey( C_IN CRYPT_HANDLE keyset,
				   C_OUT CRYPT_CONTEXT C_PTR cryptContext,
				   C_IN CRYPT_KEYID_TYPE keyIDtype, C_IN C_STR keyID, 
				   C_IN C_STR password )
	{
	UNUSED_ARG( cryptContext );
	UNUSED_ARG( keyID );
	UNUSED_ARG( password );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptAddPrivateKey( C_IN CRYPT_KEYSET keyset,
						  C_IN CRYPT_HANDLE cryptKey,
						  C_IN C_STR password )
	{
	UNUSED_ARG( password );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptAddPublicKey( C_IN CRYPT_KEYSET keyset,
						 C_IN CRYPT_CERTIFICATE certificate )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptDeleteKey( C_IN CRYPT_KEYSET keyset,
					  C_IN CRYPT_KEYID_TYPE keyIDtype,
					  C_IN C_STR keyID )
	{
	UNUSED_ARG( keyID );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_KEYSETS */

#ifndef USE_SESSIONS

C_RET cryptCreateSession( C_OUT CRYPT_SESSION C_PTR session,
						  C_IN CRYPT_USER cryptUser,
						  C_IN CRYPT_SESSION_TYPE sessionType )
	{
	UNUSED_ARG( session );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* !USE_SESSIONS */

/****************************************************************************
*																			*
*							Custom Code Insertion Point						*
*																			*
****************************************************************************/

/* This section can be used to insert custom code for application-specific 
   purposes.  It's normally not used, and isn't present in the standard 
   distribution */

#if !defined( NDEBUG ) && 1

/* Insert custom code here */

/* Debug function to handle fault injection, which sets the global value 
   that controls the type of fault to simulate.  Note that this function is
   added unconditionally (provided that it's a debug build) since it's part 
   of the self-test code */

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with crypto fault injection enabled (debug build only)." )
#endif /* Warn about special features enabled */

C_RET cryptSetFaultType( C_IN int type )
	{
	faultType = type;

	return( CRYPT_OK );
	}

/* Debug function to set the memory-allocation invocation number at which
   we fail the malloc() */

#ifdef CONFIG_FAULT_MALLOC

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with memory fault injection enabled (debug build only)." )
#endif /* Warn about special features enabled */

C_RET cryptSetMemFaultCount( C_IN int number )
	{
	clFaultAllocSetCount( number );

	return( CRYPT_OK );
	}
#endif /* CONFIG_FAULT_MALLOC */

/* Debug function to handle fuzzing */

#ifdef CONFIG_FUZZ

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with fuzz data injection enabled (debug build only)." )
#endif /* Warn about special features enabled */

#include "io/stream_int.h"		/* For STREAM_MFLAG_PSEUDO_HTTP */
#include "session/session.h"

C_RET cryptFuzzInit( C_IN CRYPT_SESSION cryptSession,
					 C_IN CRYPT_CONTEXT cryptContext )
	{
	SESSION_INFO *sessionInfoPtr;
	int status;

	assert( isHandleRangeValid( cryptSession ) );
	assert( ( cryptContext == CRYPT_UNUSED ) || \
			isHandleRangeValid( cryptContext ) );

	/* Make sure that any network finalisation has been performed.  We have 
	   to do this explicitly since we're bypassing the normal session init
	   process */
	krnlWaitSemaphore( SEMAPHORE_DRIVERBIND );

	/* Get the session state information so that we can call directly into
	   internal functions */
	status = krnlAcquireObject( cryptSession, OBJECT_TYPE_SESSION, 
								( void ** ) &sessionInfoPtr,
								CRYPT_ARGERROR_OBJECT );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform any necessary final session initialistion */
	if( sessionInfoPtr->sendBuffer == NULL )
		{
		if( ( sessionInfoPtr->receiveBuffer = \
						clAlloc( "cryptSetFuzzData", \
								 sessionInfoPtr->receiveBufSize + 8 ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		if( ( sessionInfoPtr->sendBuffer = \
						clAlloc( "cryptSetFuzzData", \
								 sessionInfoPtr->receiveBufSize + 8 ) ) == NULL )
			{
			clFree( "cryptSetFuzzData", sessionInfoPtr->receiveBuffer );
			sessionInfoPtr->receiveBuffer = NULL;
			return( CRYPT_ERROR_MEMORY );
			}
		sessionInfoPtr->sendBufSize = sessionInfoPtr->receiveBufSize;
		}
	if( cryptContext != CRYPT_UNUSED )
		{
		sessionInfoPtr->privateKey = cryptContext;
		krnlSendNotifier( sessionInfoPtr->privateKey, IMESSAGE_INCREFCOUNT );
		}
	switch( sessionInfoPtr->type )
		{
		case CRYPT_SESSION_TSP:
			/* Set a dummy message imprint size */
			sessionInfoPtr->sessionTSP->imprintSize = 1;
			break;

		case CRYPT_SESSION_CMP:
			/* Set a dummy request object and auth key */
			sessionInfoPtr->iCertRequest = 0;
			sessionInfoPtr->iAuthInContext = 0;
			break;

		default:
			break;
		}

	krnlReleaseObject( cryptSession );

	return( CRYPT_OK );
	}

C_RET cryptSetFuzzData( C_IN CRYPT_SESSION cryptSession, 
						C_IN void *data, C_IN int dataLength )
	{
	SESSION_INFO *sessionInfoPtr;
	int status;

	assert( isHandleRangeValid( cryptSession ) );
	assert( isReadPtr( data, dataLength ) );

	/* Get the session state information so that we can call directly into
	   internal functions */
	status = krnlAcquireObject( cryptSession, OBJECT_TYPE_SESSION, 
								( void ** ) &sessionInfoPtr,
								CRYPT_ARGERROR_OBJECT );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up an emulated network I/O stream to read input from and process
	   the input as far as we can */
	sMemPseudoConnect( &sessionInfoPtr->stream, data, dataLength );
	if( sessionInfoPtr->type == CRYPT_SESSION_RTCS || \
		sessionInfoPtr->type == CRYPT_SESSION_OCSP || \
		sessionInfoPtr->type == CRYPT_SESSION_TSP || \
		sessionInfoPtr->type == CRYPT_SESSION_CMP || \
		sessionInfoPtr->type == CRYPT_SESSION_SCEP )
		{
		/* It's an HTTP pseudo-stream, mark it as such */
		sessionInfoPtr->stream.flags |= STREAM_MFLAG_PSEUDO_HTTP;
		}
	status = sessionInfoPtr->transactFunction( sessionInfoPtr );
	sMemDisconnect( &sessionInfoPtr->stream );

	krnlReleaseObject( cryptSession );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int bufferedTransportReadFunction( INOUT STREAM *stream, 
										  OUT_BUFFER( maxLength, *length ) \
											BYTE *buffer, 
										  IN_DATALENGTH const int maxLength, 
										  OUT_DATALENGTH_Z int *length, 
										  IN_FLAGS_Z( TRANSPORT ) \
											const int flags )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, maxLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( maxLength == 1 );

	/* Clear return value */
	*length = 0;

	if( stream->bufPos >= stream->bufEnd )
		return( CRYPT_ERROR_UNDERFLOW );

	*buffer = stream->buffer[ stream->bufPos++ ];
	*length = 1;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int bufferedTransportWriteFunction( INOUT STREAM *stream, 
										   IN_BUFFER( maxLength ) const BYTE *buffer, 
										   IN_DATALENGTH const int maxLength, 
										   OUT_DATALENGTH_Z int *length, 
										   IN_FLAGS_Z( TRANSPORT ) \
											const int flags )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( buffer, maxLength ) );

	*length = maxLength;
	return( CRYPT_OK );
	}

C_RET cryptFuzzSpecial( C_IN void *data, C_IN int dataLength, 
						C_IN int isServer )
	{
	HTTP_DATA_INFO httpDataInfo;
	ERROR_INFO errorInfo;
	NET_STREAM_INFO netStream;
	STREAM stream;
	BYTE buffer[ 8192 ];
	int status;

	assert( isReadPtr( data, dataLength ) );

	sMemPseudoConnect( &stream, data, dataLength );
	memset( &netStream, 0, sizeof( NET_STREAM_INFO ) );
	setStreamLayerHTTP( &netStream );
	netStream.bufferedTransportReadFunction = bufferedTransportReadFunction;
	netStream.bufferedTransportWriteFunction = bufferedTransportWriteFunction;
	stream.netStreamInfo = &netStream;
	stream.flags |= STREAM_MFLAG_PSEUDO_HTTP | STREAM_MFLAG_PSEUDO_RAW;
	if( isServer )
		netStream.nFlags |= STREAM_NFLAG_ISSERVER;
	initHttpDataInfo( &httpDataInfo, buffer, 8192 );
	status = sread( &stream, &httpDataInfo, sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &stream, &errorInfo );
		return( status );
		}
	sMemDisconnect( &stream );
	
	return( CRYPT_OK );
	}
#endif /* CONFIG_FUZZ */

#ifdef CONFIG_SUITEB_TESTS 

/* Special kludge function to enable nonstandard behaviour for Suite B 
   tests.  This just passes the call on down to the low-level internal
   Suite B configuration code */

int sslSuiteBTestConfig( const int magicValue );

C_RET cryptSuiteBTestConfig( C_IN int magicValue )
	{
	return( sslSuiteBTestConfig( magicValue ) );
	}
#endif /* CONFIG_SUITEB_TESTS  */

#endif /* Debug-mode only test code */
