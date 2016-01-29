/****************************************************************************
*																			*
*						cryptlib PKCS #12 Write Routines					*
*						Copyright Peter Gutmann 1997-2002					*
*																			*
****************************************************************************/

/* This code is based on breakms.c, which breaks the encryption of several of
   MS's extremely broken PKCS #12 implementations.  Because of the security
   problems associated with key files produced by MS software and the fact
   that this format is commonly used to spray private keys around without any
   regard to their sensitivity, cryptlib doesn't support it as a writeable
   format.  As one vendor who shall remain anonymous put it, "We don't want 
   to put our keys anywhere where MS software can get to them" */

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs12.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs12.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS12

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Write the garbled PKCS #12 version of a CMS wrapper */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeNonCMSheader( INOUT STREAM *stream, 
							  IN_BUFFER( oidLength ) const BYTE *oid, 
							  IN_RANGE( 1, MAX_OID_SIZE ) const int oidLength, 
							  IN_LENGTH_SHORT const int length, 
							  IN_LENGTH_SHORT const int attrDataLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oid, oidLength ) );

	REQUIRES( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );
	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES( attrDataLength > 0 && attrDataLength < MAX_INTLENGTH_SHORT );

	writeSequence( stream, ( int ) \
				   ( sizeofOID( oid ) + \
				     sizeofObject( sizeofObject( length ) ) + \
					 sizeofObject( attrDataLength ) ) );
	swrite( stream, oid, oidLength );
	writeConstructed( stream, ( int ) sizeofObject( length ), 0 );
	return( writeSequence( stream, length ) );
	}

/* Write the MAC data at the end of the keyset */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
static int sizeofMacData( const PKCS12_INFO *pkcs12info ) 
	{
	assert( isReadPtr( pkcs12info, sizeof( PKCS12_INFO ) ) );

	return( sizeofObject( \
				sizeofAlgoID( CRYPT_ALGO_SHA1 ) + \
				sizeofObject( 20 ) ) + \
			 sizeofObject( pkcs12info->macSaltSize ) + \
			 sizeofShortInteger( pkcs12info->macIterations ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeMacData( INOUT STREAM *stream,
						 const PKCS12_INFO *pkcs12info )
	{
	MESSAGE_DATA msgData;
	BYTE macBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	const int macDataSize = sizeofMacData( pkcs12info );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( pkcs12info, sizeof( PKCS12_INFO ) ) );

	REQUIRES( macDataSize >= 32 && macDataSize < MAX_INTLENGTH_SHORT );

	/* Wrap up the MACing and get the MAC value */
	status = krnlSendMessage( pkcs12info->iMacContext, 
							  IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, macBuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( pkcs12info->iMacContext, 
							  IMESSAGE_GETATTRIBUTE_S, &msgData, 
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the MAC data.  Despite the fact that the algorithm being used 
	   is HMAC, the OID that we have to write is the one for plain SHA-1 */
	writeSequence( stream, macDataSize );
	writeSequence( stream, sizeofAlgoID( CRYPT_ALGO_SHA1 ) + \
						   sizeofObject( 20 ) );
	writeAlgoID( stream, CRYPT_ALGO_SHA1 );
	writeOctetString( stream, macBuffer, msgData.length, DEFAULT_TAG );
	writeOctetString( stream, pkcs12info->macSalt, pkcs12info->macSaltSize,
					  DEFAULT_TAG );
	return( writeShortInteger( stream, pkcs12info->macIterations, 
							   DEFAULT_TAG ) );
	}

/****************************************************************************
*																			*
*							Write a Key/Certiifcate							*
*																			*
****************************************************************************/

/* Write an encrypted private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writePrivateKey( INOUT PKCS12_OBJECT_INFO *keyObjectInfo,
							IN_HANDLE const CRYPT_HANDLE iPrivKeyContext,
							IN_HANDLE const CRYPT_HANDLE iKeyWrapContext )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	STREAM stream;
	void *privKeyData;
	int privKeyDataSize DUMMY_INIT, pbeInfoDataSize, headerSize, status;

	assert( isWritePtr( keyObjectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( isHandleRangeValid( iKeyWrapContext ) );

	/* Calculate the eventual encrypted key size and allocate storage for it */
	setMechanismWrapInfo( &mechanismInfo, NULL, 0, NULL, 0, iPrivKeyContext,
						  iKeyWrapContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT, 
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP_PKCS8 );
	if( cryptStatusOK( status ) )
		privKeyDataSize = mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( ( keyObjectInfo->data = clAlloc( "setItemFunction", \
										 64 + privKeyDataSize ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	keyObjectInfo->dataSize = 64 + privKeyDataSize;

	/* Calculate the size of the key-derivation information */
	pbeInfoDataSize = ( int ) sizeofObject( keyObjectInfo->saltSize ) + \
							  sizeofShortInteger( keyObjectInfo->iterations );

	/* Write the key-derivation information */
	sMemOpen( &stream, ( void * ) keyObjectInfo->data, 
			  keyObjectInfo->dataSize );
	writeSequence( &stream,
				   sizeofOID( OID_PKCS12_PBEWITHSHAAND3KEYTRIPLEDESCBC ) + \
				   ( int ) sizeofObject( pbeInfoDataSize ) );
	writeOID( &stream, OID_PKCS12_PBEWITHSHAAND3KEYTRIPLEDESCBC );
	writeSequence( &stream, pbeInfoDataSize );
	writeOctetString( &stream, keyObjectInfo->salt, keyObjectInfo->saltSize, 
					  DEFAULT_TAG );
	writeShortInteger( &stream, keyObjectInfo->iterations, DEFAULT_TAG );
	status = writeOctetStringHole( &stream, privKeyDataSize, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	headerSize = stell( &stream );
	ENSURES( rangeCheck( headerSize, privKeyDataSize, 
						 keyObjectInfo->dataSize ) );

	/* Insert the wrapped key into the stream buffer */
	status = sMemGetDataBlockRemaining( &stream, &privKeyData, 
										&privKeyDataSize );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	setMechanismWrapInfo( &mechanismInfo, privKeyData, privKeyDataSize,
						  NULL, 0, iPrivKeyContext, iKeyWrapContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT, 
							  &mechanismInfo, MECHANISM_PRIVATEKEYWRAP_PKCS8 );
	if( cryptStatusOK( status ) )
		{
		keyObjectInfo->dataSize = headerSize + \
								  mechanismInfo.wrappedDataLength;
		keyObjectInfo->payloadOffset = headerSize;
		keyObjectInfo->payloadSize = mechanismInfo.wrappedDataLength;
		}
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	sMemDisconnect( &stream );
	ENSURES( !cryptStatusError( \
					checkObjectEncoding( keyObjectInfo->data, \
										 keyObjectInfo->dataSize ) ) );

	return( CRYPT_OK );
	}

/* Write a certificate.  PKCS #12 generally only stores the individual 
   certificate that's associated with the private key rather than a complete 
   certificate chain, so we only add the leaf certificate rather than all of 
   the certificates in the chain (that is, in theory it's possible to stuff 
   all of the certificates in the chain into the keyset, but since PKCS #12 
   has no way of identifying or indexing them there's not much that anything 
   can do with them even if they're present) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeCertificate( INOUT PKCS12_OBJECT_INFO *certObjectInfo,
							 IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( certObjectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );

	/* Select the leaf certificate */
	status = krnlSendMessage( cryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST,
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );

	/* Allocate storage for the encoded certificate data */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( cryptHandle, IMESSAGE_CRT_EXPORT, &msgData, 
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	if( ( certObjectInfo->data = clAlloc( "setItemFunction", \
										  msgData.length ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Export the certificate */
	msgData.data = ( void * ) certObjectInfo->data;
	status = krnlSendMessage( cryptHandle, IMESSAGE_CRT_EXPORT, &msgData, 
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		clFree( "setItemFunction", ( void * ) certObjectInfo->data );
		certObjectInfo->data = NULL;

		return( status );
		}
	certObjectInfo->dataSize = msgData.length;

	/* Since we've just created the data object, the payload is identical to
	   the data */
	certObjectInfo->payloadSize = certObjectInfo->dataSize;
	certObjectInfo->payloadOffset = 0;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Write a Keyset								*
*																			*
****************************************************************************/

/* Write a PKCS #12 item ("safeBag").  We can't write this directly to the
   output but have to buffer it via an intermediate stream so that we can 
   MAC it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeMacItem( INOUT STREAM *stream, 
						 const PKCS12_INFO *pkcs12info,
						 const BOOLEAN isPrivateKey, 
						 const BOOLEAN macData )
	{
	const PKCS12_OBJECT_INFO *pkcs12objectInfo = \
			isPrivateKey ? &pkcs12info->keyInfo : &pkcs12info->certInfo;
	STREAM memStream;
	BYTE objectHeaderBuffer[ 256 + 8 ], idBuffer[ 256 + 8 ];
	const int idDataSize = ( int ) \
						( sizeofOID( OID_PKCS9_LOCALKEYID ) + \
						  sizeofObject( \
							sizeofObject( 1 ) ) );
	const int labelDataSize = ( int ) \
						( sizeofOID( OID_PKCS9_FRIENDLYNAME ) + \
						  sizeofObject( \
							sizeofObject( pkcs12info->labelLength * 2 ) ) );
	const int attrDataSize = ( int ) \
						( sizeofObject( idDataSize ) + \
						  sizeofObject( labelDataSize ) );
	int objectHeaderSize DUMMY_INIT, idSize DUMMY_INIT, i, j, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( pkcs12info, sizeof( PKCS12_INFO ) ) );

	/* Write the object header to a buffer where it can be MACed */
	sMemOpen( &memStream, objectHeaderBuffer, 256 );
	if( isPrivateKey )
		{
		status = writeNonCMSheader( &memStream, OID_PKCS12_SHROUDEDKEYBAG,
									sizeofOID( OID_PKCS12_SHROUDEDKEYBAG ),
									pkcs12objectInfo->dataSize, attrDataSize );
		}
	else
		{
		status = writeNonCMSheader( &memStream, OID_PKCS12_CERTBAG,
							sizeofOID( OID_PKCS12_CERTBAG ), ( int ) \
							( sizeofOID( OID_PKCS9_X509CERTIFICATE ) + \
							  sizeofObject( \
								sizeofObject( pkcs12objectInfo->dataSize ) ) ),
							  attrDataSize );
		if( cryptStatusOK( status ) )
			{
			writeOID( &memStream, OID_PKCS9_X509CERTIFICATE );
			writeConstructed( &memStream, ( int ) \
							  sizeofObject( pkcs12objectInfo->dataSize ), 0 );
			status = writeOctetStringHole( &memStream, 
								pkcs12objectInfo->dataSize, DEFAULT_TAG );
			}
		}
	if( cryptStatusOK( status ) )
		objectHeaderSize = stell( &memStream );
	sMemDisconnect( &memStream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the object header and data to the keyset */
	swrite( stream, objectHeaderBuffer, objectHeaderSize );
	status = swrite( stream, pkcs12objectInfo->data, 
					 pkcs12objectInfo->dataSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Mac the payload data if necessary (we don't have to perform the 
	   MACing if we're performing a dummy write to a null stream) */
	if( macData )
		{
		status = krnlSendMessage( pkcs12info->iMacContext, IMESSAGE_CTX_HASH,
								  objectHeaderBuffer, objectHeaderSize );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( pkcs12info->iMacContext, 
									  IMESSAGE_CTX_HASH, 
									  ( MESSAGE_CAST ) pkcs12objectInfo->data, 
									  pkcs12objectInfo->dataSize );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the item's ID and label.  These are supposedly optional, but
	   some apps will break if they're not present.  We have to keep the ID
	   short (rather than using, say, a keyID) because some apps assume that 
	   it's a 32-bit int or something similar.  In addition apps seem to 
	   change this value at random (Windows changes it to a GUID, Mozilla/
	   NSS changes it to a SHA-1 hash) so it's pretty much meaningless 
	   anyway */
	sMemOpen( &memStream, idBuffer, 256 );
	writeSet( &memStream, attrDataSize );
	writeSequence( &memStream, idDataSize );
	writeOID( &memStream, OID_PKCS9_LOCALKEYID );
	writeSet( &memStream, sizeofObject( 1 ) );
	writeOctetStringHole( &memStream, 1, DEFAULT_TAG );
	sputc( &memStream, 1 );		/* ID, fixed at 0x01 for a single key */
	writeSequence( &memStream, labelDataSize );
	writeOID( &memStream, OID_PKCS9_FRIENDLYNAME );
	writeSet( &memStream, ( int ) sizeofObject( pkcs12info->labelLength * 2 ) );
	status = writeGenericHole( &memStream, pkcs12info->labelLength * 2,
							   BER_STRING_BMP );
	for( i = 0, j = 0; i < pkcs12info->labelLength && \
					   i < CRYPT_MAX_TEXTSIZE; i++ )
		{
		/* Convert the ASCII string into a BMP string */
		sputc( &memStream, 0 );
		status = sputc( &memStream, byteToInt( pkcs12info->label[ i ] ) );
		}
	ENSURES( i < CRYPT_MAX_TEXTSIZE );
	if( cryptStatusOK( status ) )
		idSize = stell( &memStream );
	sMemDisconnect( &memStream );
	if( cryptStatusError( status ) )
		return( status );
	status = swrite( stream, idBuffer, idSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Mac the attribute data if necessary (we don't have to perform the 
	   MACing if we're performing a dummy write to a null stream) */
	if( macData )
		{
		status = krnlSendMessage( pkcs12info->iMacContext, 
								  IMESSAGE_CTX_HASH, idBuffer, idSize );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

/* Flush a PKCS #12 collection to a stream */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pkcs12Flush( INOUT STREAM *stream, 
				 IN_ARRAY( noPkcs12objects ) const PKCS12_INFO *pkcs12info, 
				 IN_LENGTH_SHORT const int noPkcs12objects )
	{
	STREAM memStream;
	BYTE objectHeaderBuffer[ 32 + 8 ];
	BOOLEAN privateKeyPresent = FALSE;
	const int macDataSize = sizeofMacData( pkcs12info );
	int safeDataSize DUMMY_INIT, authSafeDataSize;
	int objectHeaderSize DUMMY_INIT, i, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( pkcs12info, \
					   sizeof( PKCS12_INFO ) * noPkcs12objects ) );

	REQUIRES( noPkcs12objects >= 1 && \
			  noPkcs12objects < MAX_INTLENGTH_SHORT );

	/* Determine the overall size of the collection of objects */
	sMemNullOpen( &memStream );
	for( i = 0; i < noPkcs12objects && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		if( pkcs12info[ i ].keyInfo.dataSize > 0 )
			{
			privateKeyPresent = TRUE;
			status = writeMacItem( &memStream, pkcs12info, TRUE, FALSE );
			if( cryptStatusError( status ) )
				break;
			}
		if( pkcs12info[ i ].certInfo.dataSize > 0 )
			{
			status = writeMacItem( &memStream, pkcs12info, FALSE, FALSE );
			if( cryptStatusError( status ) )
				break;
			}
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MED );
	if( cryptStatusOK( status ) )
		safeDataSize = stell( &memStream );
	sMemClose( &memStream );
	if( cryptStatusError( status ) )
		return( status );
	if( !privateKeyPresent )
		{
		/* If there's no data present, let the caller know that the keyset
		   is empty */
		return( OK_SPECIAL );
		}
	authSafeDataSize = ( int ) \
					sizeofObject( \
						sizeofObject( \
							sizeofOID( OID_CMS_DATA ) + \
							sizeofObject( \
								sizeofObject( sizeofObject( safeDataSize ) ) ) ) );

	/* Write the outermost (authSafe) layer of cruft */
	writeSequence( stream, ( int ) \
				   sizeofShortInteger( 3 ) + \
				   sizeofObject( \
						sizeofOID( OID_CMS_DATA ) + \
						sizeofObject( \
							sizeofObject( authSafeDataSize ) ) ) + \
				   sizeofObject( macDataSize ) );
	writeShortInteger( stream, 3, DEFAULT_TAG );
	status = writeCMSheader( stream, OID_CMS_DATA, sizeofOID( OID_CMS_DATA ),
							 authSafeDataSize, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Write and MAC the next layer (safe) of cruft.  We have to do this via
	   an intermediate memory stream so that we can MAC the data before we 
	   write it to the keyset */
	sMemOpen( &memStream, objectHeaderBuffer, 32 );
	writeSequence( &memStream, ( int ) \
				   sizeofObject( \
						sizeofOID( OID_CMS_DATA ) + \
						sizeofObject( \
							sizeofObject( sizeofObject( safeDataSize ) ) ) ) );
	status = writeCMSheader( &memStream, OID_CMS_DATA, 
							 sizeofOID( OID_CMS_DATA ),
							 sizeofObject( safeDataSize ), TRUE );
	if( cryptStatusOK( status ) )
		status = writeSequence( &memStream, safeDataSize );
	if( cryptStatusOK( status ) )
		objectHeaderSize = stell( &memStream );
	sMemDisconnect( &memStream );
	if( cryptStatusError( status ) )
		return( status );
	swrite( stream, objectHeaderBuffer, objectHeaderSize );
	status = krnlSendMessage( pkcs12info->iMacContext, 
							  IMESSAGE_CTX_HASH, objectHeaderBuffer, 
							  objectHeaderSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the individual objects */
	for( i = 0; i < noPkcs12objects; i++ )
		{
		if( pkcs12info[ i ].keyInfo.dataSize > 0 )
			{
			status = writeMacItem( stream, pkcs12info, TRUE, TRUE );
			if( cryptStatusError( status ) )
				return( status );
			}
		if( pkcs12info[ i ].certInfo.dataSize > 0 )
			{
			status = writeMacItem( stream, pkcs12info, FALSE, TRUE );
			if( cryptStatusError( status ) )
				return( status );
			}
		}

	/* Write the MAC data at the end of the keyset */
	status = writeMacData( stream, pkcs12info );
	if( cryptStatusError( status ) )
		return( status );

	return( sflush( stream ) );
	}

/****************************************************************************
*																			*
*									Add a Key								*
*																			*
****************************************************************************/

/* Add an item to the PKCS #12 keyset.  PKCS #12's braindamaged format 
   severly restricts what we can allow in terms of keyset updates.  Since 
   there's only a single KEK used for both the private-key wrap and the
   overall MAC of the data, we can't store more than one private key.  In
   addition because of the lack of any useful indexing information there's
   no way to match anything to anything else, so we have to assume that
   a certificate being added belongs with the already-present private key.
   To handle all of these constraints we only store a single private key and
   only allow a certificate to be added either alongside or after the 
   private key has been added */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							IN_HANDLE const CRYPT_HANDLE cryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_BUFFER_OPT( passwordLength ) const char *password, 
							IN_LENGTH_NAME_Z const int passwordLength,
							IN_FLAGS( KEYMGMT ) const int flags )
	{
	CRYPT_CONTEXT iKeyWrapContext;
	PKCS12_INFO *pkcs12info = keysetInfoPtr->keyData;
	PKCS12_OBJECT_INFO *keyObjectInfo = &pkcs12info->keyInfo;
	PKCS12_OBJECT_INFO *certObjectInfo = &pkcs12info->certInfo;
	BOOLEAN certPresent = FALSE, contextPresent;
	BOOLEAN pkcs12keyPresent = ( keyObjectInfo->dataSize > 0 ) ? TRUE : FALSE;
	BOOLEAN wrapContextInitialised = FALSE;
	int value, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( pkcs12info, \
						sizeof( PKCS12_INFO ) * \
							keysetInfoPtr->keyDataNoObjects ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 );
	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( ( password == NULL && passwordLength == 0 ) || \
			  ( password != NULL && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( ( itemType == KEYMGMT_ITEM_PUBLICKEY && \
				password == NULL && passwordLength == 0 ) || \
			  ( itemType == KEYMGMT_ITEM_PRIVATEKEY && \
				password != NULL && passwordLength != 0 ) );
	REQUIRES( flags == KEYMGMT_FLAG_NONE );

	/* If there's already a key and certificate present then we can't add 
	   anything else.  This check also catches the (invalid) case of a 
	   certificate being present without a corresponding private key */
	if( certObjectInfo->dataSize > 0 )
		{
		retExt( CRYPT_ERROR_OVERFLOW, 
				( CRYPT_ERROR_OVERFLOW, KEYSET_ERRINFO, 
				  "No more room in keyset to add this item" ) );
		}

	/* Check the object and extract any information that we may need from 
	   it */
	status = krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC );
	if( cryptStatusOK( status ) )
		{
		int algorithm;	/* int vs.enum */

		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
								  &algorithm, CRYPT_CTXINFO_ALGO );
		if( cryptStatusOK( status ) && algorithm != CRYPT_ALGO_RSA )
			{
			retExtArg( CRYPT_ARGERROR_NUM1, 
					   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
						 "PKCS #12 keysets can only store RSA private keys "
						 "and certificates" ) );
			}
		}
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ARGERROR_OBJECT ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}
	contextPresent = cryptStatusOK( \
						krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
										 MESSAGE_CHECK_PKC_PRIVATE ) ) ? \
					 TRUE : FALSE;

	/* If there's a certificate present, make sure that it's something that 
	   can be stored.  We don't treat the wrong type as an error since we 
	   can still store the public/private key components even if we don't 
	   store the certificate */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusOK( status ) && \
		( value == CRYPT_CERTTYPE_CERTIFICATE || \
		  value == CRYPT_CERTTYPE_CERTCHAIN ) )
		{
		certPresent = TRUE;

		/* If the certificate isn't signed then we can't store it in this 
		   state */
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
								  &value, CRYPT_CERTINFO_IMMUTABLE );
		if( cryptStatusError( status ) || !value )
			{
			retExt( CRYPT_ERROR_NOTINITED, 
					( CRYPT_ERROR_NOTINITED, KEYSET_ERRINFO, 
					  "Certificate being added is incomplete (unsigned)" ) );
			}

		/* We can't add a certificate unless there's already a key present.  
		   Since PKCS #12 doesn't store any indexing information we have no 
		   idea whether the two actually belong together, so we just have to 
		   hope for the best */
		if( !pkcs12keyPresent )
			{
			retExt( CRYPT_ERROR_NOTINITED, 
					( CRYPT_ERROR_NOTINITED, KEYSET_ERRINFO, 
					  "No key present that corresponds to the certificate "
					  "being added" ) );
			}
		}
	else
		{
		/* If we're trying to add a standalone key and there's already one
		   present then we can't add another one */
		if( pkcs12keyPresent )
			{
			retExt( CRYPT_ERROR_INITED, 
					( CRYPT_ERROR_INITED, KEYSET_ERRINFO, 
					  "No more room in keyset to add this item" ) );
			}
		}

	/* PKCS #12 keysets can't store public keys, only private keys and 
	   certifiates */
	if( itemType == KEYMGMT_ITEM_PUBLICKEY && !certPresent )
		{
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
					 "PKCS #12 keysets can only store private keys and "
					 "certificates, not public keys" ) );
		}

	/* At this point we're either storing a certificate or a private key 
	   (with an optional certificate attached) */
	ENSURES( ( itemType == KEYMGMT_ITEM_PUBLICKEY && certPresent ) || \
			 ( itemType == KEYMGMT_ITEM_PRIVATEKEY && contextPresent ) );

	/* If we're adding a private key, make sure that there's a password 
	   present.  Conversely, if there's a password present make sure that 
	   we're adding a private key */
	if( pkcs12keyPresent )
		{
		/* We're adding a certificate, there can't be a password present.  
		   Some PKCS #12 implementations encrypt public certificates for no
		   adequately explained reason, we always store them as plaintext
		   since they are, after all, *public* certificates */
		if( password != NULL )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		/* We're adding a private key, there must be a password present */
		if( password == NULL )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Get what little index information PKCS #12 stores with a key */
	if( !pkcs12keyPresent && contextPresent )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, pkcs12info->label, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusError( status ) )
			return( status );
		pkcs12info->labelLength = msgData.length;
		}

	/* Write the certificate if necessary.  We do this one first because 
	   it's the easiest to back out of */
	if( certPresent )
		{
		/* We're ready to go, lock the object for our exclusive use */
		status = krnlSendMessage( cryptHandle, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_TRUE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		if( cryptStatusError( status ) )
			return( status );

		status = writeCertificate( certObjectInfo, cryptHandle );
		( void ) krnlSendMessage( cryptHandle, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, KEYSET_ERRINFO, 
					  "Couldn't extract certificate data from "
					  "certificate" ) );
			}
		pkcs12info->flags |= PKCS12_FLAG_CERT;

		/* If there's already a key present, return now */
		if( pkcs12keyPresent )
			return( CRYPT_OK );
		}

	/* Create the key wrap context and the MAC context (if necessary) from 
	   the password */
	status = createPkcs12KeyWrapContext( keyObjectInfo, 
										 keysetInfoPtr->ownerHandle, 
										 password, passwordLength,
										 &iKeyWrapContext, TRUE );
	if( cryptStatusOK( status ) )
		{
		wrapContextInitialised = TRUE;
		if( !pkcs12info->macInitialised )
			{
			status = createPkcs12MacContext( pkcs12info, 
											 keysetInfoPtr->ownerHandle, 
											 password, passwordLength, 
											 &pkcs12info->iMacContext, TRUE );
			}
		}
	if( cryptStatusError( status ) )
		{
		pkcs12freeEntry( pkcs12info );
		if( wrapContextInitialised )
			krnlSendNotifier( iKeyWrapContext, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't create session/MAC key to secure private "
				  "key" ) );
		}
	pkcs12info->macInitialised = TRUE;

	/* Write the encrypted and MACed private key */
	status = writePrivateKey( keyObjectInfo, cryptHandle, 
							  iKeyWrapContext );
	krnlSendNotifier( iKeyWrapContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		pkcs12freeEntry( pkcs12info );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't write wrapped private key data" ) );
		}
	pkcs12info->flags |= PKCS12_FLAG_PRIVKEY;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Keyset Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS12set( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 );

	/* Set the access method pointers */
	keysetInfoPtr->setItemFunction = setItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_PKCS12 */
