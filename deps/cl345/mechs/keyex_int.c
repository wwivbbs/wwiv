/****************************************************************************
*																			*
*						Internal Key Exchange Routines						*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "mech.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "mechs/mech.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

#ifdef USE_INT_CMS

/****************************************************************************
*																			*
*							Low-level Key Export Functions					*
*																			*
****************************************************************************/

/* Export a conventionally encrypted session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int exportConventionalKey( OUT_BUFFER_OPT( encryptedKeyMaxLength, *encryptedKeyLength ) \
							void *encryptedKey, 
						   IN_DATALENGTH_Z const int encryptedKeyMaxLength,
						   OUT_DATALENGTH_Z int *encryptedKeyLength,
						   IN_HANDLE_OPT const CRYPT_CONTEXT iSessionKeyContext,
						   IN_HANDLE const CRYPT_CONTEXT iExportContext,
						   IN_ENUM( KEYEX ) const KEYEX_TYPE keyexType )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	const WRITEKEK_FUNCTION writeKeyexFunction = getWriteKekFunction( keyexType );
	BYTE buffer[ CRYPT_MAX_KEYSIZE + 16 + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int keySize, ivSize, status;

	assert( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			isWritePtrDynamic( encryptedKey, encryptedKeyMaxLength ) );
	assert( isWritePtr( encryptedKeyLength, sizeof( int ) ) );

	REQUIRES( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			  ( encryptedKey != NULL && \
				encryptedKeyMaxLength > MIN_CRYPT_OBJECTSIZE && \
				encryptedKeyMaxLength < MAX_BUFFER_SIZE ) );
	REQUIRES( ( keyexType == KEYEX_PGP && \
				iSessionKeyContext == CRYPT_UNUSED ) || \
			  ( keyexType != KEYEX_PGP && \
				isHandleRangeValid( iSessionKeyContext ) ) );
	REQUIRES( isHandleRangeValid( iExportContext ) );
	REQUIRES( isEnumRange( keyexType, KEYEX ) );

	/* Clear return value */
	*encryptedKeyLength = 0;

	/* Make sure that the requested key exchange format is available */
	if( writeKeyexFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

#ifdef USE_PGP
	/* PGP doesn't actually wrap up a key but derives the session key
	   directly from the password.  Because of this there isn't any key
	   wrapping to be done so we just write the key derivation parameters
	   and exit */
	if( keyexType == KEYEX_PGP )
		{
		STREAM stream;

		sMemOpenOpt( &stream, encryptedKey, encryptedKeyMaxLength );
		status = writeKeyexFunction( &stream, iExportContext, NULL, 0 );
		if( cryptStatusOK( status ) )
			*encryptedKeyLength = stell( &stream );
		sMemDisconnect( &stream );

		return( status );
		}
#endif /* USE_PGP */

	/* Get the export parameters */
	status = krnlSendMessage( iSessionKeyContext, IMESSAGE_GETATTRIBUTE,
							  &keySize, CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM1 : status );
	if( cryptStatusError( krnlSendMessage( iExportContext,
										   IMESSAGE_GETATTRIBUTE, &ivSize,
										   CRYPT_CTXINFO_IVSIZE ) ) )
		ivSize = 0;
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE" );

	/* Load an IV into the exporting context.  This is somewhat nasty in that
	   a side-effect of exporting a key is to load an IV, which isn't really 
	   part of the function's job description.  The alternative would be to 
	   require the user to explicitly load an IV before exporting the key, 
	   but this is equally nasty because they'll never remember.  The lesser 
	   of the two evils is to load the IV here and assume that anyone 
	   loading the IV themselves will read the docs, which warn about the 
	   side-effects of exporting a key.

	   Note that we always load a new IV when we export a key because the
	   caller may be using the context to exchange multiple keys.  Since each
	   exported key requires its own IV we perform an unconditional reload.
	   In addition because we don't want another thread coming along and
	   changing the IV while we're in the process of encrypting with it, we
	   lock the exporting key object until the encryption has completed and
	   the IV is written to the output */
	status = krnlSendMessage( iExportContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	if( ivSize > 0 )
		{
		status = krnlSendNotifier( iExportContext, IMESSAGE_CTX_GENIV );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "IMESSAGE_CTX_GENIV" );

	/* Encrypt the session key and write the result to the output stream */
	if( encryptedKey == NULL )
		{
		setMechanismWrapInfo( &mechanismInfo, NULL, 0, NULL, 0, 
							  iSessionKeyContext, iExportContext );
		}
	else
		{
		setMechanismWrapInfo( &mechanismInfo, buffer, CRYPT_MAX_KEYSIZE + 16, 
							  NULL, 0, iSessionKeyContext, iExportContext );
		}
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_ENC_CMS );
	if( cryptStatusOK( status ) )
		{
		STREAM stream;

		/* If we're perfoming a dummy export for a length check, set up a 
		   dummy value to write */
		if( encryptedKey == NULL )
			memset( buffer, 0x01, mechanismInfo.wrappedDataLength );

		INJECT_FAULT( MECH_CORRUPT_KEY, MECH_CORRUPT_KEY_1 );
		sMemOpenOpt( &stream, encryptedKey, encryptedKeyMaxLength );
		status = writeKeyexFunction( &stream, iExportContext,
									 ( encryptedKey != NULL ) ? \
										mechanismInfo.wrappedData : buffer,
									 mechanismInfo.wrappedDataLength );
		if( cryptStatusOK( status ) )
			*encryptedKeyLength = stell( &stream );
		sMemDisconnect( &stream );
		INJECT_FAULT( MECH_CORRUPT_SALT, MECH_CORRUPT_SALT_1 );
		INJECT_FAULT( MECH_CORRUPT_ITERATIONS, MECH_CORRUPT_ITERATIONS_1 );
		INJECT_FAULT( MECH_CORRUPT_PRFALGO, MECH_CORRUPT_PRFALGO_1 );
		CFI_CHECK_UPDATE( "writeKeyexFunction" );
		}
	( void ) krnlSendMessage( iExportContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	clearMechanismInfo( &mechanismInfo );
	zeroise( buffer, CRYPT_MAX_KEYSIZE + 16 );
	if( cryptStatusError( status ) )
		return( status );

	REQUIRES( CFI_CHECK_SEQUENCE_3( "IMESSAGE_GETATTRIBUTE", "IMESSAGE_CTX_GENIV",
									"writeKeyexFunction" ) );

	return( CRYPT_OK );
	}

/* Export a public-key encrypted session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int exportPublicKey( OUT_BUFFER_OPT( encryptedKeyMaxLength, *encryptedKeyLength ) \
						void *encryptedKey, 
					 IN_DATALENGTH_Z const int encryptedKeyMaxLength,
					 OUT_DATALENGTH_Z int *encryptedKeyLength,
					 IN_HANDLE const CRYPT_CONTEXT iSessionKeyContext,
					 IN_HANDLE const CRYPT_CONTEXT iExportContext,
					 IN_BUFFER_OPT( auxInfoLength ) const void *auxInfo, 
					 IN_LENGTH_SHORT_Z const int auxInfoLength,
					 IN_ENUM( KEYEX ) const KEYEX_TYPE keyexType )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	const WRITEKEYTRANS_FUNCTION writeKeytransFunction = getWriteKeytransFunction( keyexType );
	const BOOLEAN requiresSizeFixup = \
				( ( keyexType == KEYEX_CMS || keyexType == KEYEX_CRYPTLIB ) && \
				  ( encryptedKey != NULL ) ) ? TRUE : FALSE;
	BYTE buffer[ MAX_PKCENCRYPTED_SIZE + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int exportKeySize DUMMY_INIT, status;

	assert( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			isWritePtrDynamic( encryptedKey, encryptedKeyMaxLength ) );
	assert( isWritePtr( encryptedKeyLength, sizeof( int ) ) );
	assert( ( auxInfo == NULL && auxInfoLength == 0 ) || \
			isReadPtrDynamic( auxInfo, auxInfoLength ) );
	
	REQUIRES( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			  ( encryptedKey != NULL && \
				encryptedKeyMaxLength > MIN_CRYPT_OBJECTSIZE && \
				encryptedKeyMaxLength < MAX_BUFFER_SIZE ) );
	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );
	REQUIRES( isHandleRangeValid( iExportContext ) );
	REQUIRES( ( auxInfo == NULL && auxInfoLength == 0 ) || \
			  ( auxInfo != NULL && \
				isShortIntegerRangeNZ( auxInfoLength ) ) );
	REQUIRES( isEnumRange( keyexType, KEYEX ) );

	/* Clear return value */
	*encryptedKeyLength = 0;

	/* Make sure that the requested key exchange format is available */
	if( writeKeytransFunction  == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Get the export parameters */
	if( requiresSizeFixup )
		{
		status = krnlSendMessage( iExportContext, IMESSAGE_GETATTRIBUTE,
								  &exportKeySize, CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );
		if( exportKeySize > encryptedKeyMaxLength )
			return( CRYPT_ERROR_OVERFLOW );
		}
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE" );

	/* Encrypt the session key and write the result to the output stream */
	if( encryptedKey == NULL )
		{
		setMechanismWrapInfo( &mechanismInfo, NULL, 0, NULL, 0, 
							  iSessionKeyContext, iExportContext );
		}
	else
		{
		setMechanismWrapInfo( &mechanismInfo, buffer, MAX_PKCENCRYPTED_SIZE, 
							  NULL, 0, iSessionKeyContext, iExportContext );
		}
	if( keyexType == KEYEX_CMS_OAEP )
		{
		/* OAEP requires an additional parameter, the hash algorithm to use.
		   Actually it requires numerous additional parameters because in 
		   OAEP absolutely everything is parameterised, but at the moment the
		   only one that's really used is SHA-1, which is also the default
		   algorithm ID for the various parameters and so doesn't require 
		   extensive additional encoding.  Because of this we always use 
		   SHA-1 */
		mechanismInfo.auxInfo = CRYPT_ALGO_SHA1;
		}
	status = krnlSendMessage( iExportContext, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, 
							  ( keyexType == KEYEX_PGP ) ? \
								MECHANISM_ENC_PKCS1_PGP : \
							  ( keyexType == KEYEX_CMS_OAEP ) ? \
								MECHANISM_ENC_OAEP : MECHANISM_ENC_PKCS1 );
	if( cryptStatusOK( status ) )
		{
		STREAM stream;

		/* If we're perfoming a dummy export for a length check, set up a 
		   dummy value to write */
		if( encryptedKey == NULL )
			{
			memset( buffer, 0x01, mechanismInfo.wrappedDataLength );
			mechanismInfo.wrappedData = buffer;
			}

		/* If we're using the CMS data format then we need to fix up the 
		   size of the wrapped key data to match the exporting key size.  
		   This is necessary because the higher-level ASN.1 wrappers at the 
		   CMS envelope level won't reflect the fact that the size has 
		   changed at the CMS keyex level, so we need to adjust the data 
		   size to ensure that the amount of data we output matches what
		   was promised in the size check */
		if( requiresSizeFixup && mechanismInfo.wrappedDataLength < exportKeySize )
			{
			const int delta = exportKeySize - mechanismInfo.wrappedDataLength;

			REQUIRES( boundsCheck( delta, mechanismInfo.wrappedDataLength, 
								   MAX_PKCENCRYPTED_SIZE ) );
			memmove( ( BYTE * ) mechanismInfo.wrappedData + delta, 
					 mechanismInfo.wrappedData, 
					 mechanismInfo.wrappedDataLength );
			memset( mechanismInfo.wrappedData, 0, delta );
			mechanismInfo.wrappedDataLength = exportKeySize;
			}

		INJECT_FAULT( MECH_CORRUPT_KEY, MECH_CORRUPT_KEY_1 );
		sMemOpenOpt( &stream, encryptedKey, encryptedKeyMaxLength );
		status = writeKeytransFunction ( &stream, iExportContext, 
										 mechanismInfo.wrappedData,
										 mechanismInfo.wrappedDataLength,
										 auxInfo, auxInfoLength );
		if( cryptStatusOK( status ) )
			*encryptedKeyLength = stell( &stream );
		sMemDisconnect( &stream );
		CFI_CHECK_UPDATE( "writeKeytransFunction" );
		}
	clearMechanismInfo( &mechanismInfo );

	/* Clean up */
	zeroise( buffer, MAX_PKCENCRYPTED_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	REQUIRES( CFI_CHECK_SEQUENCE_2( "IMESSAGE_GETATTRIBUTE",
									"writeKeytransFunction" ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Low-level Key Import Functions					*
*																			*
****************************************************************************/

/* Import a conventionally encrypted session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int importConventionalKey( IN_BUFFER( encryptedKeyLength ) \
							const void *encryptedKey, 
						   IN_DATALENGTH const int encryptedKeyLength,
						   IN_HANDLE const CRYPT_CONTEXT iSessionKeyContext,
						   IN_HANDLE const CRYPT_CONTEXT iImportContext,
						   IN_ENUM( KEYEX ) const KEYEX_TYPE keyexType )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	const READKEK_FUNCTION readKeyexFunction = getReadKekFunction( keyexType );
	QUERY_INFO queryInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	int importAlgo, importMode DUMMY_INIT, status;	/* int vs.enum */

	assert( isReadPtrDynamic( encryptedKey, encryptedKeyLength ) );

	REQUIRES( encryptedKeyLength > MIN_CRYPT_OBJECTSIZE && \
			  encryptedKeyLength < MAX_BUFFER_SIZE );
	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );
	REQUIRES( isHandleRangeValid( iImportContext ) );
	REQUIRES( isEnumRange( keyexType, KEYEX ) );

	/* Make sure that the requested key exchange format is available */
	if( readKeyexFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Get the import parameters */
	status = krnlSendMessage( iImportContext, IMESSAGE_GETATTRIBUTE, 
							  &importAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iImportContext, IMESSAGE_GETATTRIBUTE,
								  &importMode, CRYPT_CTXINFO_MODE );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );

	/* Read and check the encrypted key record and make sure that we'll be 
	   using the correct type of encryption context to decrypt it */
	sMemConnect( &stream, encryptedKey, encryptedKeyLength );
	status = readKeyexFunction( &stream, &queryInfo );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) && \
		( importAlgo != queryInfo.cryptAlgo || \
		  importMode != queryInfo.cryptMode ) )
		status = CRYPT_ARGERROR_NUM1;
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Extract the encrypted key from the buffer and decrypt it.  Since we
	   don't want another thread changing the IV while we're using the import
	   context, we lock it for the duration */
	status = krnlSendMessage( iImportContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}
	if( needsIV( importMode ) && importAlgo != CRYPT_ALGO_RC4 )
		{
		setMessageData( &msgData, queryInfo.iv, queryInfo.ivLength );
		status = krnlSendMessage( iImportContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusError( status ) )
			{
			( void ) krnlSendMessage( iImportContext, IMESSAGE_SETATTRIBUTE, 
									  MESSAGE_VALUE_FALSE, 
									  CRYPT_IATTRIBUTE_LOCKED );
			zeroise( &queryInfo, sizeof( QUERY_INFO ) );
			return( status );
			}
		}
	ENSURES( boundsCheck( queryInfo.dataStart, queryInfo.dataLength,
						  encryptedKeyLength ) );
	setMechanismWrapInfo( &mechanismInfo,
						  ( BYTE * ) encryptedKey + queryInfo.dataStart, 
						  queryInfo.dataLength, NULL, 0, 
						  iSessionKeyContext, iImportContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_ENC_CMS );
	( void ) krnlSendMessage( iImportContext, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, 
							  CRYPT_IATTRIBUTE_LOCKED );
	clearMechanismInfo( &mechanismInfo );
	zeroise( &queryInfo, sizeof( QUERY_INFO ) );

	return( status );
	}

/* Import a public-key encrypted session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int importPublicKey( IN_BUFFER( encryptedKeyLength ) const void *encryptedKey, 
					 IN_DATALENGTH const int encryptedKeyLength,
					 IN_HANDLE_OPT const CRYPT_CONTEXT iSessionKeyContext,
					 IN_HANDLE const CRYPT_CONTEXT iImportContext,
					 OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iReturnedContext, 
					 IN_ENUM( KEYEX ) const KEYEX_TYPE keyexType )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	const READKEYTRANS_FUNCTION readKeytransFunction = getReadKeytransFunction( keyexType );
	QUERY_INFO queryInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	int compareType, status;

	assert( isReadPtrDynamic( encryptedKey, encryptedKeyLength ) );
	assert( ( keyexType == KEYEX_PGP && \
			  isWritePtr( iReturnedContext, sizeof( CRYPT_CONTEXT ) ) ) || \
			( keyexType != KEYEX_PGP && iReturnedContext == NULL ) );

	REQUIRES( encryptedKeyLength > MIN_CRYPT_OBJECTSIZE && \
			  encryptedKeyLength < MAX_BUFFER_SIZE );
	REQUIRES( ( keyexType == KEYEX_PGP && \
				iSessionKeyContext == CRYPT_UNUSED ) || \
			  ( keyexType != KEYEX_PGP && \
				isHandleRangeValid( iSessionKeyContext ) ) );
	REQUIRES( isHandleRangeValid( iImportContext ) );
	REQUIRES( ( keyexType == KEYEX_PGP && iReturnedContext != NULL ) || \
			  ( keyexType != KEYEX_PGP && iReturnedContext == NULL ) );
	REQUIRES( isEnumRange( keyexType, KEYEX ) );

	/* Clear return value */
	if( iReturnedContext != NULL )
		*iReturnedContext = CRYPT_ERROR;

	/* Make sure that the requested key exchange format is available */
	if( readKeytransFunction == NULL )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Read and check the encrypted key record */
	sMemConnect( &stream, encryptedKey, encryptedKeyLength );
	status = readKeytransFunction( &stream, &queryInfo );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Make sure that we've been given the correct key */
	setMessageData( &msgData, queryInfo.keyID, queryInfo.keyIDlength );
	switch( keyexType )
		{
		case KEYEX_CMS:
			setMessageData( &msgData, \
					( BYTE * ) encryptedKey + queryInfo.iAndSStart, \
					queryInfo.iAndSLength );
			compareType = MESSAGE_COMPARE_ISSUERANDSERIALNUMBER;
			break;

		case KEYEX_CRYPTLIB:
			compareType = MESSAGE_COMPARE_KEYID;
			break;

		case KEYEX_PGP:
			compareType = ( queryInfo.version == PGP_VERSION_2 ) ? \
						  MESSAGE_COMPARE_KEYID_PGP : \
						  MESSAGE_COMPARE_KEYID_OPENPGP;
			break;

		default:
			retIntError();
		}
	status = krnlSendMessage( iImportContext, IMESSAGE_COMPARE, &msgData, 
							  compareType );
	if( cryptStatusError( status ) && \
		compareType == MESSAGE_COMPARE_KEYID )
		{
		/* Checking for the keyID gets a bit complicated, in theory it's the 
		   subjectKeyIdentifier from a certificate but in practice this form 
		   is mostly used for certificateless public keys.  Because of this we 
		   check for the keyID first and if that fails fall back to the 
		   sKID */
		status = krnlSendMessage( iImportContext, IMESSAGE_COMPARE, 
								  &msgData, 
								  MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER );
		}
	if( cryptStatusError( status ) && \
		compareType == MESSAGE_COMPARE_KEYID_OPENPGP )
		{
		/* Some broken PGP implementations put PGP 2.x IDs in packets marked 
		   as OpenPGP packets so if we were doing a check for an OpenPGP ID 
		   and it failed, fall back to a PGP 2.x one */
		status = krnlSendMessage( iImportContext, IMESSAGE_COMPARE, 
								  &msgData, MESSAGE_COMPARE_KEYID_PGP );
		}
	if( cryptStatusError( status ) )
		{
		/* A failed comparison is reported as a generic CRYPT_ERROR, convert 
		   it into a wrong-key error */
		zeroise( &queryInfo, sizeof( QUERY_INFO ) );
		return( CRYPT_ERROR_WRONGKEY );
		}

	/* Decrypt the encrypted key and load it into the context */
	if( keyexType != KEYEX_PGP )
		{
		const BOOLEAN isOAEP = \
				( queryInfo.cryptAlgoEncoding == ALGOID_ENCODING_OAEP ) ? \
				TRUE : FALSE;

		setMechanismWrapInfo( &mechanismInfo,
							  ( BYTE * ) encryptedKey + queryInfo.dataStart, 
							  queryInfo.dataLength, NULL, 0, 
							  iSessionKeyContext, iImportContext );
		if( isOAEP )
			{
			/* See the comment in exportPublicKey() on the hardcoding of 
			   SHA-1 */
			mechanismInfo.auxInfo = CRYPT_ALGO_SHA1;
			}
		status = krnlSendMessage( iImportContext, IMESSAGE_DEV_IMPORT,
								  &mechanismInfo, isOAEP ? \
									MECHANISM_ENC_OAEP : \
									MECHANISM_ENC_PKCS1 );
		}
	else
		{
		/* PGP doesn't provide separate session key information with the
		   encrypted data but wraps it up alongside the encrypted key, so we
		   can't import the wrapped key into a context via the standard key
		   import functions but instead have to create the context as part
		   of the unwrap process */
		setMechanismWrapInfo( &mechanismInfo, 
							  ( BYTE * ) encryptedKey + queryInfo.dataStart,
							  queryInfo.dataLength, NULL, 0, 
							  CRYPT_UNUSED, iImportContext );
		status = krnlSendMessage( iImportContext, IMESSAGE_DEV_IMPORT,
								  &mechanismInfo, MECHANISM_ENC_PKCS1_PGP );
		if( cryptStatusOK( status ) )
			*iReturnedContext = mechanismInfo.keyContext;
		}
	clearMechanismInfo( &mechanismInfo );
	zeroise( &queryInfo, sizeof( QUERY_INFO ) );

	return( status );
	}
#endif /* USE_INT_CMS */
