/****************************************************************************
*																			*
*					 cryptlib Configuration Read/Write Routines				*
*						Copyright Peter Gutmann 1994-2008					*
*																			*
****************************************************************************/

#include "crypt.h"
#ifdef INC_ALL
  #include "trustmgr.h"
  #include "asn1.h"
  #include "user_int.h"
  #include "user.h"
#else
  #include "cert/trustmgr.h"
  #include "enc_dec/asn1.h"
  #include "misc/user_int.h"
  #include "misc/user.h"
#endif /* Compiler-specific includes */

#ifdef USE_KEYSETS

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Read an individual configuration option */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readConfigOption( INOUT STREAM *stream, 
							 IN_HANDLE CRYPT_USER iCryptUser )
	{
	CRYPT_ATTRIBUTE_TYPE attributeType;
	const BUILTIN_OPTION_INFO *builtinOptionInfoPtr;
	MESSAGE_DATA msgData;
	void *dataPtr DUMMY_INIT_PTR;
	long optionCode;
	int value, tag, length, status;

	/* Read the wrapper and option index and map it to the actual option.  
	   If we find an unknown index or one that shouldn't be writeable to 
	   persistent storage, we skip it and continue.  This is done to handle 
	   new options that may have been added after this version of cryptlib 
	   was built (for unknown indices) and because the stored configuration 
	   options are an untrusted source so we have to check for attempts to 
	   feed in bogus values (for non-writeable options) */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &optionCode );
	if( cryptStatusError( status ) )
		return( status );
	if( optionCode < 0 || optionCode > LAST_OPTION_INDEX )
		{
		/* Unknown option, ignore it */
		return( readUniversal( stream ) );
		}
	builtinOptionInfoPtr = getBuiltinOptionInfoByCode( optionCode );
	if( builtinOptionInfoPtr == NULL || \
		builtinOptionInfoPtr->index < 0 || \
		builtinOptionInfoPtr->index > LAST_OPTION_INDEX || \
		builtinOptionInfoPtr->index == CRYPT_UNUSED )
		{
		/* Unknown option, ignore it */
		return( readUniversal( stream ) );
		}
	attributeType = builtinOptionInfoPtr->option;

	/* Read the option value and set the option.  We don't treat a failure 
	   to set the option as a problem since the user probably doesn't want 
	   the entire system to fail because of a bad configuration option, and 
	   in any case we'll fall back to a safe default value */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( tag == BER_BOOLEAN || tag == BER_INTEGER )
		{
		/* It's a numeric value, read the appropriate type and try and set 
		   the option */
		if( tag == BER_BOOLEAN )
			status = readBoolean( stream, &value );
		else
			{
			long integer;

			status = readShortInteger( stream, &integer );
			if( cryptStatusOK( status ) )
				value = ( int ) integer;
			}
		if( cryptStatusError( status ) )
			return( status );
		( void ) krnlSendMessage( iCryptUser, IMESSAGE_SETATTRIBUTE, 
								  &value, attributeType );
		return( CRYPT_OK );
		}

	/* It's a string value, set the option straight from the encoded data */
	status = readGenericHole( stream, &length, 1, BER_STRING_UTF8 );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( stream, &dataPtr, length );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, length, SSKIP_MAX );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, dataPtr, length );
	( void ) krnlSendMessage( iCryptUser, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, attributeType );

	return( CRYPT_OK );
	}

/* Rumble through the configuration options to determine the total encoded 
   length of the ones that don't match the default setting.  We can't just 
   check the isDirty flag because if a value is reset to its default setting 
   the encoded size will be zero even though the isDirty flag is set */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int sizeofConfigData( IN_ARRAY( configOptionsCount ) \
								const OPTION_INFO *optionList, 
							 IN_INT_SHORT const int configOptionsCount,
							 OUT_DATALENGTH_Z int *length )
	{
	int dataLength = 0, i;

	assert( isReadPtr( optionList, 
					   sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( configOptionsCount > 0 && \
			  configOptionsCount < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*length = 0;

	/* Check each option to see whether it needs to be written to disk.  If 
	   it does, determine its length */
	for( i = 0; 
		 i < configOptionsCount && \
			optionList[ i ].builtinOptionInfo != NULL && \
			optionList[ i ].builtinOptionInfo->option <= LAST_STORED_OPTION; 
		 i++ )
		{
		const BUILTIN_OPTION_INFO *builtinOptionInfoPtr = \
									optionList[ i ].builtinOptionInfo;
		const OPTION_INFO *optionInfoPtr = &optionList[ i ];
		int lengthValue;

		/* If it's an option that can't be written to disk, skip it */
		if( builtinOptionInfoPtr->index == CRYPT_UNUSED )
			continue;

		if( builtinOptionInfoPtr->type == OPTION_STRING )
			{
			/* If the string value is the same as the default, there's
			   nothing to do */
			if( optionInfoPtr->strValue == NULL || \
				optionInfoPtr->strValue == builtinOptionInfoPtr->strDefault )
				continue;
			lengthValue = ( int ) \
					sizeofObject( \
						sizeofShortInteger( builtinOptionInfoPtr->index ) + \
						sizeofObject( optionInfoPtr->intValue ) );
			}
		else
			{
			/* If the integer/boolean value that's currently set isn't the
			   default setting, update it */
			if( optionInfoPtr->intValue == builtinOptionInfoPtr->intDefault )
				continue;
			lengthValue = ( int ) \
					sizeofObject( \
						sizeofShortInteger( builtinOptionInfoPtr->index ) + \
						( builtinOptionInfoPtr->type == OPTION_NUMERIC ? \
						  sizeofShortInteger( optionInfoPtr->intValue ) : \
						  sizeofBoolean() ) );
			}
		ENSURES( lengthValue > 0 && lengthValue < MAX_INTLENGTH_SHORT );
		dataLength += lengthValue;
		}
	ENSURES( i < configOptionsCount );
	ENSURES( dataLength >= 0 && dataLength < MAX_BUFFER_SIZE );

	*length = dataLength;
	return( CRYPT_OK );
	}

/* Write the configuration data to a stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeConfigData( INOUT STREAM *stream, 
							IN_ARRAY( configOptionsCount ) \
								const OPTION_INFO *optionList,
							IN_INT_SHORT const int configOptionsCount )
	{
	int i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( optionList, 
					   sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES( configOptionsCount > 0 && \
			  configOptionsCount < MAX_INTLENGTH_SHORT );

	/* Write each option that needs to be written to the stream */
	for( i = 0; 
		 i < configOptionsCount && \
			optionList[ i ].builtinOptionInfo != NULL && \
			optionList[ i ].builtinOptionInfo->option <= LAST_STORED_OPTION; 
		 i++ )
		{
		const BUILTIN_OPTION_INFO *builtinOptionInfoPtr = \
									optionList[ i ].builtinOptionInfo;
		const OPTION_INFO *optionInfoPtr = &optionList[ i ];

		/* If it's an option that can't be written to disk, skip it */
		if( builtinOptionInfoPtr->index == CRYPT_UNUSED )
			continue;

		if( builtinOptionInfoPtr->type == OPTION_STRING )
			{
			if( optionInfoPtr->strValue == NULL || \
				optionInfoPtr->strValue == builtinOptionInfoPtr->strDefault )
				continue;
			writeSequence( stream,
						   sizeofShortInteger( builtinOptionInfoPtr->index ) + \
						   sizeofObject( optionInfoPtr->intValue ) );
			writeShortInteger( stream, builtinOptionInfoPtr->index,
							   DEFAULT_TAG );
			status = writeCharacterString( stream, optionInfoPtr->strValue,
										   optionInfoPtr->intValue,
										   BER_STRING_UTF8 );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}

		if( optionInfoPtr->intValue == builtinOptionInfoPtr->intDefault )
			continue;
		if( builtinOptionInfoPtr->type == OPTION_NUMERIC )
			{
			writeSequence( stream,
						   sizeofShortInteger( builtinOptionInfoPtr->index ) + \
						   sizeofShortInteger( optionInfoPtr->intValue ) );
			writeShortInteger( stream, builtinOptionInfoPtr->index,
							   DEFAULT_TAG );
			status = writeShortInteger( stream, optionInfoPtr->intValue,
										DEFAULT_TAG );
			}
		else
			{
			writeSequence( stream,
						   sizeofShortInteger( builtinOptionInfoPtr->index ) + \
						   sizeofBoolean() );
			writeShortInteger( stream, builtinOptionInfoPtr->index,
							   DEFAULT_TAG );
			status = writeBoolean( stream, optionInfoPtr->intValue, 
								   DEFAULT_TAG );
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( i < configOptionsCount );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read Configuration Options 						*
*																			*
****************************************************************************/

/* Read any user-defined configuration options.  Since the configuration 
   file is an untrusted source we set the values in it via external messages 
   rather than manipulating the configuration info directly, which means 
   that everything read is subject to the usual ACL checks */

#ifdef USE_CERTIFICATES

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int readTrustedCerts( IN_HANDLE const CRYPT_KEYSET iCryptKeyset,
							 INOUT void *trustInfoPtr )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 1536 + 8 ];
	int iterationCount, status;

	assert( trustInfoPtr != NULL );

	REQUIRES( isHandleRangeValid( iCryptKeyset ) );

	/* Read each trusted certificate from the keyset */
	setMessageData( &msgData, buffer, CRYPT_MAX_PKCSIZE + 1536 );
	status = krnlSendMessage( iCryptKeyset, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_TRUSTEDCERT );
	for( iterationCount = 0;
		 cryptStatusOK( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		/* Add the certificate data as a trusted certificate item and look 
		   for the next one */
		status = addTrustEntry( trustInfoPtr, CRYPT_UNUSED, msgData.data,
								msgData.length, TRUE );
		if( cryptStatusOK( status ) )
			{
			setMessageData( &msgData, buffer, CRYPT_MAX_PKCSIZE + 1536 );
			status = krnlSendMessage( iCryptKeyset, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, 
									  CRYPT_IATTRIBUTE_TRUSTEDCERT_NEXT );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( ( status == CRYPT_ERROR_NOTFOUND ) ? CRYPT_OK : status );
	}
#endif /* USE_CERTIFICATES */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int readConfig( IN_HANDLE const CRYPT_USER iCryptUser, 
				IN_STRING const char *fileName, 
				INOUT_OPT void *trustInfoPtr )
	{
	CRYPT_KEYSET iCryptKeyset;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	DYNBUF configDB;
	char configFilePath[ MAX_PATH_LENGTH + 8 ];
	int configFilePathLen, iterationCount, status;

	assert( isReadPtr( fileName, 2 ) );

	ANALYSER_HINT_STRING( fileName );

	REQUIRES( iCryptUser == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptUser ) );

	/* Try and open the configuration file.  If we can't open it it merely 
	   means that the file doesn't exist, which isn't an error, we'll go 
	   with the built-in defaults */
	status = fileBuildCryptlibPath( configFilePath, MAX_PATH_LENGTH, 
									&configFilePathLen, fileName, 
									strlen( fileName ), BUILDPATH_GETPATH );
	if( cryptStatusError( status ) )
		return( CRYPT_OK );		/* Can't build configuration path */
	setMessageCreateObjectInfo( &createInfo, CRYPT_KEYSET_FILE );
	createInfo.arg2 = CRYPT_KEYOPT_READONLY;
	createInfo.strArg1 = configFilePath;
	createInfo.strArgLen1 = configFilePathLen;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_KEYSET );
	if( cryptStatusError( status ) )
		return( CRYPT_OK );		/* No configuration data present */
	iCryptKeyset = createInfo.cryptHandle;

	/* Get the configuration information from the keyset.  We fetch the 
	   overall configuration information into a dynbuf and then optionally
	   read the trust information (trusted certificates) if certificate use
	   is enabled, after which we close the keyset and read the 
	   configuration data from the dynbuf */
	status = dynCreate( &configDB, iCryptKeyset,
						CRYPT_IATTRIBUTE_CONFIGDATA );
	if( cryptStatusError( status ) )
		{
		/* If there were no configuration options present there may still be 
		   trusted certificates so we try and read those before exiting */
#ifdef USE_CERTIFICATES
		if( status == CRYPT_ERROR_NOTFOUND && trustInfoPtr != NULL )
			status = readTrustedCerts( iCryptKeyset, trustInfoPtr );
#endif /* USE_CERTIFICATES */
		krnlSendNotifier( iCryptKeyset, IMESSAGE_DECREFCOUNT );
		return( status );
		}
#ifdef USE_CERTIFICATES
	if( trustInfoPtr != NULL )
		status = readTrustedCerts( iCryptKeyset, trustInfoPtr );
#endif /* USE_CERTIFICATES */
	krnlSendNotifier( iCryptKeyset, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		dynDestroy( &configDB );
		return( status );
		}

	/* Read each configuration option */
	sMemConnect( &stream, dynData( configDB ), dynLength( configDB ) );
	for( iterationCount = 0;
		 cryptStatusOK( status ) && \
			stell( &stream ) < dynLength( configDB ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		status = readConfigOption( &stream, iCryptUser );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	sMemDisconnect( &stream );

	/* Clean up */
	dynDestroy( &configDB );
	return( status );
	}

/****************************************************************************
*																			*
*							Write Configuration Options 					*
*																			*
****************************************************************************/

/* Write any user-defined configuration options.  This is performed in two 
   phases, a first phase that encodes the configuration data and a second 
   phase that writes the data to disk.  The reason for the split is that the 
   second phase doesn't require the use of the user object data any more 
   and can be a somewhat lengthy process due to disk accesses and other bits 
   and pieces, because of this the caller is expected to unlock the user 
   object between the two phases to ensure that the second phase doesn't 
   stall all other operations that require it.
   
   The prepare phase is further subdivided into two sub-phases, necessitated 
   by the fact that we can have configuration data, trusted certificates, or 
   both, present to write.  First we determine what's present using 
   getConfigDisposition(), if there's configuration data present then we 
   prepare it using prepareConfigData(), and finally we write the 
   configuration data and/or trusted certificates using commitConfigData() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int getConfigDisposition( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
						  IN_INT_SHORT const int configOptionsCount, 	
						  const void *trustInfoPtr, 
						  OUT_ENUM( CONFIG_DISPOSITION ) \
							CONFIG_DISPOSITION_TYPE *disposition )
	{
#ifdef USE_CERTIFICATES
	const BOOLEAN hasTrustedCerts = trustedCertsPresent( trustInfoPtr );
#else
	const BOOLEAN hasTrustedCerts = FALSE;
#endif /* USE_CERTIFICATES */
	int length, status;

	assert( isReadPtr( configOptions, 
					   sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( trustInfoPtr != NULL );
	assert( isWritePtr( disposition, sizeof( CONFIG_DISPOSITION_TYPE ) ) );

	REQUIRES( configOptionsCount > 0 && \
			  configOptionsCount < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*disposition = CONFIG_DISPOSITION_NONE;

	/* If neither the configuration options nor any certificate trust 
	   settings have changed, there's nothing to do */
	if( !checkConfigChanged( configOptions, configOptionsCount ) && \
		!hasTrustedCerts )
		{
		*disposition = CONFIG_DISPOSITION_NO_CHANGE;

		return( CRYPT_OK );
		}

	/* Determine the total encoded length of the configuration options */
	status = sizeofConfigData( configOptions, configOptionsCount, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		{
		/* There's no configuration data present, if there are trusted 
		   certificates present then we need to write those, otherwise the
		   configuration file will be empty and the caller can delete it */
		*disposition = hasTrustedCerts ? \
							CONFIG_DISPOSITION_TRUSTED_CERTS_ONLY : \
							CONFIG_DISPOSITION_EMPTY_CONFIG_FILE;
		return( CRYPT_OK );
		}

	/* There's configuration data present, report whether there are trusted
	   certificates present as well */
	*disposition = hasTrustedCerts ? CONFIG_DISPOSITION_BOTH : \
									 CONFIG_DISPOSITION_DATA_ONLY;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int prepareConfigData( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
					   IN_INT_SHORT const int configOptionsCount, 	
					   OUT_BUFFER_ALLOC_OPT( *dataLength ) void **dataPtrPtr, 
					   OUT_DATALENGTH_Z int *dataLength )
	{
	STREAM stream;
	void *dataPtr;
	int length, status;

	assert( isReadPtr( configOptions, 
					   sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( configOptionsCount > 0 && \
			  configOptionsCount < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*dataPtrPtr = NULL;
	*dataLength = 0;

	/* Determine the total encoded length of the configuration options */
	status = sizeofConfigData( configOptions, configOptionsCount, &length );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( length > 0 && length < MAX_BUFFER_SIZE );

	/* Allocate a buffer to hold the encoded values */
	if( ( dataPtr = clAlloc( "prepareConfigData", length ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Write the configuration options */
	sMemOpen( &stream, dataPtr, length );
	status = writeConfigData( &stream, configOptions, configOptionsCount );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		clFree( "prepareConfigData", dataPtr );
		DEBUG_DIAG(( "Couldn't prepare config data for write" ));
		assert( DEBUG_WARN );
		return( status );
		}
	*dataPtrPtr = dataPtr;
	*dataLength = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int commitConfigData( IN_STRING const char *fileName,
					  IN_BUFFER_OPT( dataLength ) const void *data, 
					  IN_DATALENGTH_Z const int dataLength,
					  IN_HANDLE_OPT const CRYPT_USER iTrustedCertUserObject )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	char configFilePath[ MAX_PATH_LENGTH + 8 ];
	int configFilePathLen, status;

	assert( isReadPtr( fileName, 2 ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );

	ANALYSER_HINT_STRING( fileName );

	REQUIRES( iTrustedCertUserObject == CRYPT_UNUSED || \
			  ( iTrustedCertUserObject == DEFAULTUSER_OBJECT_HANDLE || \
				isHandleRangeValid( iTrustedCertUserObject ) ) );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( dataLength > 0 && dataLength < MAX_BUFFER_SIZE ) );

	/* Build the path to the configuration file and try and create it */
	status = fileBuildCryptlibPath( configFilePath, MAX_PATH_LENGTH, 
									&configFilePathLen, fileName, 
									strlen( fileName ), 
									BUILDPATH_CREATEPATH );
	if( cryptStatusError( status ) )
		{
		/* Map the lower-level filesystem-specific error into a more 
		   meaningful generic error */
		return( CRYPT_ERROR_OPEN );
		}
	setMessageCreateObjectInfo( &createInfo, CRYPT_KEYSET_FILE );
	createInfo.arg2 = CRYPT_KEYOPT_CREATE;
	createInfo.strArg1 = configFilePath;
	createInfo.strArgLen1 = configFilePathLen;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_KEYSET );
	if( cryptStatusError( status ) )
		{
		/* Map the lower-level keyset-specific error into a more meaningful
		   generic error */
		return( CRYPT_ERROR_OPEN );
		}

	/* Send the configuration data (if there is any) and any trusted 
	   certificates to the keyset.  { data, dataLength } can be empty if 
	   there are only trusted certificates to write */
	if( dataLength > 0 )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) data, dataLength );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_CONFIGDATA );
		}
	if( cryptStatusOK( status ) && \
		iTrustedCertUserObject != CRYPT_UNUSED )
		{
		status = krnlSendMessage( iTrustedCertUserObject, 
								  IMESSAGE_SETATTRIBUTE,
								  &createInfo.cryptHandle,
								  CRYPT_IATTRUBUTE_CERTKEYSET );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		fileErase( configFilePath );
		return( CRYPT_ERROR_WRITE );
		}
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Delete Configuration Options 					*
*																			*
****************************************************************************/

/* Delete the configuration file.  This always returns an OK status even if
   the delete fails since it's not certain what we should do in this case */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteConfig( IN_STRING const char *fileName )
	{
	char configFilePath[ MAX_PATH_LENGTH + 1 + 8 ];
	int configFilePathLen, status;

	assert( isReadPtr( fileName, 2 ) );

	ANALYSER_HINT_STRING( fileName );

	status = fileBuildCryptlibPath( configFilePath, MAX_PATH_LENGTH, 
									&configFilePathLen, fileName, 
									strlen( fileName ), BUILDPATH_GETPATH );
	if( cryptStatusOK( status ) )
		{
		configFilePath[ configFilePathLen ] = '\0';
		fileErase( configFilePath );
		}
	return( CRYPT_OK );
	}
#endif /* USE_KEYSETS */
