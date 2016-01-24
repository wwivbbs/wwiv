/****************************************************************************
*																			*
*						cryptlib PKCS #11 PKC Routines						*
*						Copyright Peter Gutmann 1998-2011					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Tell context.h that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "device.h"
  #include "pkcs11_api.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "device/device.h"
  #include "device/pkcs11_api.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS11

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Read an attribute value, used to read public-key components.  The odd 
   two-phase read is necessary for buggy implementations that fail if the 
   given size isn't exactly the same as the data size */

static int readAttributeValue( PKCS11_INFO *pkcs11Info,
							   const CK_OBJECT_HANDLE hObject,
							   const CK_ATTRIBUTE_TYPE attrType, 
							   void *buffer, const int bufMaxLen,
							   int *length )
	{
	CK_ATTRIBUTE attrTemplate = { attrType, NULL_PTR, bufMaxLen };
	CK_RV status;
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( buffer, bufMaxLen ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	/* Clear return value */
	memset( buffer, 0, min( 16, bufMaxLen ) );
	*length = CRYPT_ERROR;

	status = C_GetAttributeValue( pkcs11Info->hSession, hObject, 
								  &attrTemplate, 1 );
	if( status == CKR_OK )
		{
		attrTemplate.pValue = buffer;
		status = C_GetAttributeValue( pkcs11Info->hSession, hObject, 
									  &attrTemplate, 1 );
		}
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusOK( status ) )
		*length = attrTemplate.ulValueLen;
	return( cryptStatus );
	}

/* When we've generated or loaded a key, the underlying device may impose
   additional usage restrictions on it that go beyond what we've requested 
   at object-creation time.  In order to deal with this we read back the 
   attributes that are set for the newly-created device object and update 
   the object's action flags to reflect this */

static int updateActionFlags( INOUT PKCS11_INFO *pkcs11Info,
							  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							  const CK_OBJECT_HANDLE hObject,
							  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
							  const BOOLEAN isPrivateKey )
	{
	int actionFlags, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( isPkcAlgo( cryptAlgo ) );

	cryptStatus = actionFlags = \
			getActionFlags( pkcs11Info, hObject, 
							isPrivateKey ? KEYMGMT_ITEM_PRIVATEKEY : \
										   KEYMGMT_ITEM_PUBLICKEY, 
							cryptAlgo );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, 
							 ( MESSAGE_CAST ) &actionFlags, 
							 CRYPT_IATTRIBUTE_ACTIONPERMS ) );
	}

/****************************************************************************
*																			*
*						 	Capability Interface Routines					*
*																			*
****************************************************************************/

/* Sign data, check a signature.  We use Sign and Verify rather than the
   xxxRecover variants because there's no need to use Recover, and because
   many implementations don't do Recover */

static int genericSign( PKCS11_INFO *pkcs11Info, 
						CONTEXT_INFO *contextInfoPtr,
						const CK_MECHANISM *pMechanism, 
						const void *inBuffer, const int inLength, 
						void *outBuffer, const int outLength )
	{
	CK_ULONG resultLen = outLength;
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( inBuffer, inLength ) );
	assert( isWritePtr( outBuffer, outLength ) );

	REQUIRES( inLength > 0 && inLength < MAX_INTLENGTH_SHORT );
	REQUIRES( outLength > 0 && outLength < MAX_INTLENGTH_SHORT );

	/* If we're currently in the middle of a multi-stage sign operation we
	   can't start a new one.  We have to perform this tracking explicitly 
	   since PKCS #11 only allows one multi-stage operation per session */
	if( pkcs11Info->hActiveSignObject != CK_OBJECT_NONE )
		return( CRYPT_ERROR_INCOMPLETE );

	status = C_SignInit( pkcs11Info->hSession,
						 ( CK_MECHANISM_PTR ) pMechanism, 
						 contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Sign( pkcs11Info->hSession, ( CK_BYTE_PTR ) inBuffer, 
						 inLength, outBuffer, &resultLen );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

	return( CRYPT_OK );
	}

static int genericVerify( PKCS11_INFO *pkcs11Info, 
						  CONTEXT_INFO *contextInfoPtr,
						  const CK_MECHANISM *pMechanism, 
						  const void *inBuffer, const int inLength, 
						  void *outBuffer, const int outLength )
	{
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( pMechanism, sizeof( CK_MECHANISM ) ) );
	assert( isReadPtr( inBuffer, inLength ) );
	assert( isWritePtr( outBuffer, outLength ) );

	REQUIRES( inLength > 0 && inLength < MAX_INTLENGTH_SHORT );
	REQUIRES( outLength > 0 && outLength < MAX_INTLENGTH_SHORT );

	/* If we're currently in the middle of a multi-stage sign operation we
	   can't start a new one.  We have to perform this tracking explicitly 
	   since PKCS #11 only allows one multi-stage operation per session */
	if( pkcs11Info->hActiveSignObject != CK_OBJECT_NONE )
		return( CRYPT_ERROR_INCOMPLETE );

	status = C_VerifyInit( pkcs11Info->hSession,
						   ( CK_MECHANISM_PTR ) pMechanism,
						   contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Verify( pkcs11Info->hSession, ( CK_BYTE_PTR ) inBuffer, 
						   inLength, outBuffer, outLength );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

	return( CRYPT_OK );
	}

/* Encrypt, decrypt */

static int genericEncrypt( PKCS11_INFO *pkcs11Info, 
						   CONTEXT_INFO *contextInfoPtr,
						   const CK_MECHANISM *pMechanism, void *buffer,
						   const int length, const int outLength )
	{
	CK_ULONG resultLen = outLength;
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( pMechanism, sizeof( CK_MECHANISM ) ) );
	assert( isWritePtr( buffer, length ) );
	assert( isWritePtr( buffer, outLength ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES( outLength > 0 && outLength < MAX_INTLENGTH_SHORT );

	status = C_EncryptInit( pkcs11Info->hSession,
							( CK_MECHANISM_PTR ) pMechanism,
							contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Encrypt( pkcs11Info->hSession, buffer, length,
							buffer, &resultLen );
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

	/* When performing RSA operations some buggy implementations perform 
	   leading-zero trunction, so we restore leading zeroes if necessary */
	if( ( pMechanism->mechanism == CKM_RSA_X_509 || \
		  pMechanism->mechanism == CKM_RSA_PKCS ) && \
		( int ) resultLen < length )
		{
		const int delta = length - resultLen;

		REQUIRES( rangeCheck( delta, resultLen, length ) );
		memmove( ( BYTE * ) buffer + delta, buffer, resultLen );
		memset( buffer, 0, delta );
		}

	return( CRYPT_OK );
	}

static int genericDecrypt( PKCS11_INFO *pkcs11Info, 
						   CONTEXT_INFO *contextInfoPtr,
						   const CK_MECHANISM *pMechanism, void *buffer,
						   const int length, int *resultLength )
	{
	CK_ULONG resultLen = length;
	CK_RV status;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( pMechanism, sizeof( CK_MECHANISM ) ) );
	assert( isWritePtr( buffer, length ) );
	assert( isWritePtr( resultLength, sizeof( int ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );

	status = C_DecryptInit( pkcs11Info->hSession,
							( CK_MECHANISM_PTR ) pMechanism,
							contextInfoPtr->deviceObject );
	if( status == CKR_OK )
		status = C_Decrypt( pkcs11Info->hSession, buffer, length,
							buffer, &resultLen );
	if( status == CKR_KEY_FUNCTION_NOT_PERMITTED )
		{
		static const CK_OBJECT_CLASS secretKeyClass = CKO_SECRET_KEY;
		static const CK_KEY_TYPE secretKeyType = CKK_GENERIC_SECRET;
		static const CK_BBOOL bTrue = TRUE;
		CK_ATTRIBUTE symTemplate[] = { 
			{ CKA_CLASS, ( CK_VOID_PTR ) &secretKeyClass, sizeof( CK_OBJECT_CLASS ) },
			{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &secretKeyType, sizeof( CK_KEY_TYPE ) },
			{ CKA_EXTRACTABLE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
			{ CKA_VALUE_LEN, &resultLen, sizeof( CK_ULONG ) } 
			};
		CK_OBJECT_HANDLE hSymKey;

		/* If a straight decrypt isn't allowed, try an unwrap instead and 
		   then export the key.  This works because we're using the same
		   mechanism as for decrypt and converting the entire "unwrapped key"
		   into a generic secret key that we then extract, which is the
		   same as doing a straight decrypt of the data (this sort of thing
		   should require a note from your mother before you're allowed to do
		   it).  The reason why it's done in this roundabout manner is that 
		   this is what Netscape tries first, so people doing a minimal 
		   implementation do this first and don't bother with anything else.  
		   Note that doing it this way is rather slower than a straight 
		   decrypt, which is why we try for decrypt first */
		status = C_UnwrapKey( pkcs11Info->hSession,
							  ( CK_MECHANISM_PTR ) pMechanism,
							  contextInfoPtr->deviceObject, buffer, length,
							  symTemplate, 4, &hSymKey );
		if( status == CKR_OK )
			{
			CK_ATTRIBUTE valueTemplate[] = { CKA_VALUE, buffer, length };

			status = C_GetAttributeValue( pkcs11Info->hSession, 
										  hSymKey, valueTemplate, 1 );
			if( status == CKR_OK )
				resultLen = valueTemplate[ 0 ].ulValueLen;
			C_DestroyObject( pkcs11Info->hSession, hSymKey );
			}
		}
	if( status != CKR_OK )
		return( pkcs11MapError( status, CRYPT_ERROR_FAILED ) );

	/* When performing raw RSA operations some buggy implementations perform 
	   leading-zero trunction, so we restore leading zeroes if necessary.  We
	   can't do the restore with the PKCS mechanism since it always returns a 
	   result length shorter than the input length */
	if( pMechanism->mechanism == CKM_RSA_X_509 && \
		( int ) resultLen < length )
		{
		const int delta = length - resultLen;

		REQUIRES( rangeCheck( delta, resultLen, length ) );
		memmove( ( BYTE * ) buffer + delta, buffer, resultLen );
		memset( buffer, 0, delta );
		resultLen = length;
		}

	/* Some mechanisms change the data length, in which case we need to tell
	   the caller how much was actually returned */
	if( resultLength != NULL )
		*resultLength = ( int ) resultLen;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							DH Mapping Functions							*
*																			*
****************************************************************************/

/* DH algorithm-specific mapping functions.  These work somewhat differently
   from the other PKC functions because DH objects are ephemeral, the only 
   fixed values being p and g.  In addition there's no real concept of 
   public and private keys, only an object where the CKA_VALUE attribute
   contains y (nominally the public key) and one where it contains x
   (nominally the private key).  The use of DH objects then is as follows:

	load/genkey: genkey with supplied p and g to produce x and y values;
				 save "public key" (y) as altObjectHandle;

	DH phase 1:  return public key CKA_VALUE (= y);

	DH phase 2:  derive using private key, y' = mechanism parameters */

int dhSetPublicComponents( PKCS11_INFO *pkcs11Info,
						   const CRYPT_CONTEXT iCryptContext,
						   const CK_OBJECT_HANDLE hDhKey,
						   const void *q, const int qLen )
	{
	BYTE p[ CRYPT_MAX_PKCSIZE + 8 ], g[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE y[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 3 ) + 8 ];
	MESSAGE_DATA msgData;
	int pLen, gLen = DUMMY_INIT, yLen = DUMMY_INIT, keyDataSize, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( q, qLen ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( qLen > 0 && qLen <= CRYPT_MAX_PKCSIZE );

	/* Get the public key components from the device */
	cryptStatus = readAttributeValue( pkcs11Info, hDhKey, CKA_PRIME, 
									  p, CRYPT_MAX_PKCSIZE, &pLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hDhKey, CKA_BASE, 
										  g, CRYPT_MAX_PKCSIZE, &gLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hDhKey, CKA_VALUE, 
										  y, CRYPT_MAX_PKCSIZE, &yLen );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all that we're doing here is sending in encoded public key data for 
	   use by objects such as certificates */
	cryptStatus = writeFlatPublicKey( keyDataBuffer, CRYPT_MAX_PKCSIZE * 3,
									  &keyDataSize, CRYPT_ALGO_DH, 
									  p, pLen, g, gLen, q, qLen, y, yLen );
	if( cryptStatusOK( cryptStatus ) )
		{
		setMessageData( &msgData, keyDataBuffer, keyDataSize );
		cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
									   &msgData, 
										CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL );
		}
	return( cryptStatus );
	}

static int dhInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					  const int keyLength )
	{
	static const CK_MECHANISM mechanism = { CKM_DH_PKCS_KEY_PAIR_GEN, NULL_PTR, 0 };
	static const CK_BBOOL bTrue = TRUE;
	CK_ATTRIBUTE privateKeyTemplate[] = {
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_DERIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		};
	CK_ATTRIBUTE publicKeyTemplate[] = {
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_DERIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		};
	CK_OBJECT_HANDLE hPublicKey, hPrivateKey;
	CK_RV status;
	const CRYPT_PKCINFO_DLP *dhKey = ( CRYPT_PKCINFO_DLP * ) key;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( keyLength == sizeof( CRYPT_PKCINFO_DLP ) );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Generate the keys.  We can't set CKA_SENSITIVE for the private key 
	   because although this is appropriate for the key (we don't want people
	   extracting the x value), some implementations carry it over to the 
	   derived key in phase 2 and make that non-extractable as well */
	publicKeyTemplate[ 2 ].pValue = ( CK_VOID_PTR ) dhKey->p;
	publicKeyTemplate[ 2 ].ulValueLen = bitsToBytes( dhKey->pLen );
	publicKeyTemplate[ 3 ].pValue = ( CK_VOID_PTR ) dhKey->g;
	publicKeyTemplate[ 3 ].ulValueLen = bitsToBytes( dhKey->gLen );
	status = C_GenerateKeyPair( pkcs11Info->hSession,
								( CK_MECHANISM_PTR ) &mechanism,
								( CK_ATTRIBUTE_PTR ) publicKeyTemplate, 4,
								( CK_ATTRIBUTE_PTR ) privateKeyTemplate, 3,
								&hPublicKey, &hPrivateKey );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Send the keying information to the context */
	cryptStatus = dhSetPublicComponents( pkcs11Info, 
										 contextInfoPtr->objectHandle,
										 hPublicKey, dhKey->q, 
										 bitsToBytes( dhKey->qLen ) );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Remember what we've set up.  Unlike conventional PKC algorithms for
	   which we only store the private-key object handle, for DH key 
	   agreement we need to store the handles for both objects */
	cryptStatus = krnlSendMessage( contextInfoPtr->objectHandle, 
								   IMESSAGE_SETATTRIBUTE,
								   ( MESSAGE_CAST ) &hPrivateKey, 
								   CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusOK( cryptStatus ) )
		{
		contextInfoPtr->altDeviceObject = hPublicKey;
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
		}
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int dhGenerateKey( CONTEXT_INFO *contextInfoPtr, const int keysizeBits )
	{
	CRYPT_PKCINFO_DLP dhKey;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE pubkeyBuffer[ ( CRYPT_MAX_PKCSIZE * 3 ) + 8 ], label[ 8 + 8 ];
	STREAM stream;
	int length, cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( keysizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keysizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* CKM_DH_KEY_PAIR_GEN is really a Clayton's key generation mechanism 
	   since it doesn't actually generate the p, g values.  Because of this 
	   we have to generate half the key ourselves in a native context, then 
	   copy portions from the native context over in flat form and complete 
	   the keygen via the device.  The easiest way to do this is to create a 
	   native DH context, generate a key, grab the public portions, and 
	   destroy the context again.  Since the keygen can take awhile and 
	   doesn't require the device, we do it before we grab the device */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_DH );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								   OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	setMessageData( &msgData, label, 8 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
					 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_CTXINFO_LABEL );
	cryptStatus = krnlSendNotifier( createInfo.cryptHandle, 
									IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( cryptStatus ) )
		{
		setMessageData( &msgData, pubkeyBuffer, CRYPT_MAX_PKCSIZE * 3 );
		cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
									   IMESSAGE_GETATTRIBUTE_S, &msgData, 
									   CRYPT_IATTRIBUTE_KEY_SPKI );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the public key information by extracting the flat values from 
	   the SubjectPublicKeyInfo.  Note that the data used is represented in
	   DER-canonical form, there may be PKCS #11 implementations that can't 
	   handle this (for example they may require p to be zero-padded to make 
	   it exactly n bytes rather than (say) n - 1 bytes if the high byte is 
	   zero) */
	cryptInitComponents( &dhKey, CRYPT_KEYTYPE_PUBLIC );
	sMemConnect( &stream, pubkeyBuffer, msgData.length );
	readSequence( &stream, NULL );					/* SEQUENCE */
	readSequence( &stream, NULL );						/* SEQUENCE */
	readUniversal( &stream );								/* OID */
	readSequence( &stream, NULL );							/* SEQUENCE */
	readGenericHole( &stream, &length, 16, BER_INTEGER  );		/* p */
	cryptStatus = sread( &stream, dhKey.p, length );
	if( cryptStatusOK( cryptStatus ) )
		{
		dhKey.pLen = bytesToBits( length );
		readGenericHole( &stream, &length, 16, BER_INTEGER  );	/* q */
		cryptStatus = sread( &stream, dhKey.q, length );
		}
	if( cryptStatusOK( cryptStatus ) )
		{
		dhKey.qLen = bytesToBits( length );
		readGenericHole( &stream, &length, 16, BER_INTEGER  );	/* g */
		cryptStatus = sread( &stream, dhKey.g, length );
		}
	if( cryptStatusOK( cryptStatus ) )
		dhKey.gLen = bytesToBits( length );
	sMemDisconnect( &stream );
	REQUIRES( cryptStatusOK( cryptStatus ) );

	/* From here on it's a standard DH key load */
	return( dhInitKey( contextInfoPtr, &dhKey, sizeof( CRYPT_PKCINFO_DLP  ) ) );
	}

static int dhEncrypt( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	CK_ATTRIBUTE yValueTemplate = { CKA_VALUE, NULL, CRYPT_MAX_PKCSIZE };
	CK_RV status;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	KEYAGREE_PARAMS *keyAgreeParams = ( KEYAGREE_PARAMS * ) buffer;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == sizeof( KEYAGREE_PARAMS ) );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Get the y value from phase 1 of the DH key agreement (generated when 
	   the key was loaded/generated) from the device.  The odd two-phase y 
	   value read is necessary for buggy implementations that fail if the 
	   given size isn't exactly the same as the data size */
	status = C_GetAttributeValue( pkcs11Info->hSession, 
								  contextInfoPtr->altDeviceObject,
								  &yValueTemplate, 1 );
	if( status == CKR_OK )
		{
		yValueTemplate.pValue = keyAgreeParams->publicValue;
		status = C_GetAttributeValue( pkcs11Info->hSession, 
									  contextInfoPtr->altDeviceObject,
									  &yValueTemplate, 1 );
		}
	if( status == CKR_OK )
		keyAgreeParams->publicValueLen = yValueTemplate.ulValueLen;
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int dhDecrypt( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_OBJECT_CLASS secretKeyClass = CKO_SECRET_KEY;
	static const CK_KEY_TYPE secretKeyType = CKK_GENERIC_SECRET;
	static const CK_BBOOL bTrue = TRUE;
	CK_MECHANISM mechanism = { CKM_DH_PKCS_DERIVE, NULL_PTR, 0 };
	CK_ULONG valueLen;
	CK_ATTRIBUTE symTemplate[] = { 
		{ CKA_CLASS, ( CK_VOID_PTR ) &secretKeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &secretKeyType, sizeof( CK_KEY_TYPE ) },
		{ CKA_EXTRACTABLE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VALUE_LEN, &valueLen, sizeof( CK_ULONG ) } 
		};
	CK_OBJECT_HANDLE hSymKey;
	CK_RV status;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	KEYAGREE_PARAMS *keyAgreeParams = ( KEYAGREE_PARAMS * ) buffer;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length == sizeof( KEYAGREE_PARAMS ) );
	REQUIRES( keyAgreeParams->publicValue != NULL && \
			  keyAgreeParams->publicValueLen >= MIN_PKCSIZE && \
			  keyAgreeParams->publicValueLen < MAX_INTLENGTH_SHORT );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Use the supplied y value to perform phase 2 of the DH key agreement.  
	   Since PKCS #11 mechanisms don't allow the resulting data to be 
	   returned directly, we move it into a generic secret-key object and
	   then read it from that */
	valueLen = keyAgreeParams->publicValueLen;	/* symTemplate[4].pValue */
	mechanism.pParameter = keyAgreeParams->publicValue;
	mechanism.ulParameterLen = keyAgreeParams->publicValueLen;
	status = C_DeriveKey( pkcs11Info->hSession, &mechanism,
						  contextInfoPtr->deviceObject, 
						  symTemplate, 4, &hSymKey );
	if( status == CKR_OK )
		{
		CK_ATTRIBUTE valueTemplate[] = { CKA_VALUE, keyAgreeParams->wrappedKey, 
										 valueLen };

		status = C_GetAttributeValue( pkcs11Info->hSession, 
									  hSymKey, valueTemplate, 1 );
		if( status == CKR_OK )
			keyAgreeParams->wrappedKeyLen = valueTemplate[ 0 ].ulValueLen;
		C_DestroyObject( pkcs11Info->hSession, hSymKey );
		}
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

/****************************************************************************
*																			*
*							RSA Mapping Functions							*
*																			*
****************************************************************************/

/* RSA algorithm-specific mapping functions.  Externally we always appear to 
   use the X.509 (raw) mechanism for the encrypt/decrypt/sign/verify 
   functions since cryptlib does its own padding (with workarounds for 
   various bugs and peculiarities).  Internally however we have to use the
   PKCS mechanism since some implementations don't support the X.509
   mechanism, and add/remove the padding to fake out the presence of a raw
   RSA mechanism */

int rsaSetPublicComponents( PKCS11_INFO *pkcs11Info,
							const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hRsaKey,
							const BOOLEAN nativeContext )
	{
	BYTE n[ CRYPT_MAX_PKCSIZE + 8 ], e[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 2 ) + 8 ];
	MESSAGE_DATA msgData;
	int nLen, eLen = DUMMY_INIT, keyDataSize, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Get the public key components from the device */
	cryptStatus = readAttributeValue( pkcs11Info, hRsaKey, CKA_MODULUS, 
									  n, CRYPT_MAX_PKCSIZE, &nLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hRsaKey, CKA_PUBLIC_EXPONENT, 
										  e, CRYPT_MAX_PKCSIZE, &eLen );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all that we're doing here is sending in encoded public key data for 
	   use by objects such as certificates */
	cryptStatus = writeFlatPublicKey( keyDataBuffer, CRYPT_MAX_PKCSIZE * 2,
									  &keyDataSize, CRYPT_ALGO_RSA, 
									  n, nLen, e, eLen, NULL, 0, NULL, 0 );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	setMessageData( &msgData, keyDataBuffer, keyDataSize );
	if( nativeContext )
		{
		return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
								 &msgData, CRYPT_IATTRIBUTE_KEY_SPKI ) );
		}
	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							 &msgData, CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL ) );
	}

static int rsaSetKeyInfo( PKCS11_INFO *pkcs11Info,
						  CONTEXT_INFO *contextInfoPtr, 
						  const CK_OBJECT_HANDLE hPrivateKey,
						  const CK_OBJECT_HANDLE hPublicKey )
	{
	MESSAGE_DATA msgData;
	BYTE idBuffer[ KEYID_SIZE + 8 ];
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Remember what we've set up.  Note that PKCS #11 tokens create 
	   distinct public- and private-key objects but we're only interested
	   in the private-key one, so we store the private-key object handle
	   in the context */
	cryptStatus = krnlSendMessage( contextInfoPtr->objectHandle, 
								   IMESSAGE_SETATTRIBUTE,
								   ( MESSAGE_CAST ) &hPrivateKey, 
								   CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;

	/* Get the key ID from the context and use it as the object ID.  Since 
	   some objects won't allow after-the-event ID updates, we don't treat a 
	   failure to update as an error.  We do however assert on it in debug 
	   mode since if we later want to update the key with a certificate then 
	   we need the ID set in order to locate the object that the certificate 
	   is associated with */
	setMessageData( &msgData, idBuffer, KEYID_SIZE );
	cryptStatus = krnlSendMessage( contextInfoPtr->objectHandle, 
								   IMESSAGE_GETATTRIBUTE_S, &msgData, 
								   CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusOK( cryptStatus ) )
		{
		CK_ATTRIBUTE idTemplate = { CKA_ID, msgData.data, msgData.length };
		CK_RV status;

		if( hPublicKey != CK_OBJECT_NONE )
			{
			status = C_SetAttributeValue( pkcs11Info->hSession, hPublicKey, 
										  &idTemplate, 1 );
			assert( status == CKR_OK );
			}
		status = C_SetAttributeValue( pkcs11Info->hSession, hPrivateKey, 
									  &idTemplate, 1 );
		assert( status == CKR_OK );
		}
	
	return( cryptStatus );
	}

static int rsaInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					   const int keyLength )
	{
	static const CK_OBJECT_CLASS privKeyClass = CKO_PRIVATE_KEY;
	static const CK_OBJECT_CLASS pubKeyClass = CKO_PUBLIC_KEY;
	static const CK_KEY_TYPE type = CKK_RSA;
	static const CK_BBOOL bTrue = TRUE;
	CK_ATTRIBUTE rsaKeyTemplate[] = {
		/* Shared fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &privKeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_MODULUS, NULL, 0 },
		{ CKA_PUBLIC_EXPONENT, NULL, 0 },
		/* Private-key only fields */
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE_EXPONENT, NULL, 0 },
		{ CKA_PRIME_1, NULL, 0 },
		{ CKA_PRIME_2, NULL, 0 },
		{ CKA_EXPONENT_1, NULL, 0 },
		{ CKA_EXPONENT_2, NULL, 0 },
		{ CKA_COEFFICIENT, NULL, 0 },
		};
	const CRYPT_PKCINFO_RSA *rsaKey = ( CRYPT_PKCINFO_RSA * ) key;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	CK_OBJECT_HANDLE hRsaKey;
	CK_RV status;
	const int templateCount = rsaKey->isPublicKey ? 8 : 15;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	
	REQUIRES( keyLength == sizeof( CRYPT_PKCINFO_RSA ) );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the key values */
	rsaKeyTemplate[ 6 ].pValue = ( CK_VOID_PTR ) rsaKey->n;
	rsaKeyTemplate[ 6 ].ulValueLen = bitsToBytes( rsaKey->nLen );
	rsaKeyTemplate[ 7 ].pValue = ( CK_VOID_PTR ) rsaKey->e;
	rsaKeyTemplate[ 7 ].ulValueLen = bitsToBytes( rsaKey->eLen );
	if( !rsaKey->isPublicKey )
		{
		rsaKeyTemplate[ 9 ].pValue = ( CK_VOID_PTR ) rsaKey->d;
		rsaKeyTemplate[ 9 ].ulValueLen = bitsToBytes( rsaKey->dLen );
		rsaKeyTemplate[ 10 ].pValue = ( CK_VOID_PTR ) rsaKey->p;
		rsaKeyTemplate[ 10 ].ulValueLen = bitsToBytes( rsaKey->pLen );
		rsaKeyTemplate[ 11 ].pValue = ( CK_VOID_PTR ) rsaKey->q;
		rsaKeyTemplate[ 11 ].ulValueLen = bitsToBytes( rsaKey->qLen );
		rsaKeyTemplate[ 12 ].pValue = ( CK_VOID_PTR ) rsaKey->e1;
		rsaKeyTemplate[ 12 ].ulValueLen = bitsToBytes( rsaKey->e1Len );
		rsaKeyTemplate[ 13 ].pValue = ( CK_VOID_PTR ) rsaKey->e2;
		rsaKeyTemplate[ 13 ].ulValueLen = bitsToBytes( rsaKey->e2Len );
		rsaKeyTemplate[ 14 ].pValue = ( CK_VOID_PTR ) rsaKey->u;
		rsaKeyTemplate[ 14 ].ulValueLen = bitsToBytes( rsaKey->uLen );
		}
	else
		{
		/* If it's a public key then we need to change the type and 
		   indication of the operations that it's allowed to perform */
		rsaKeyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &pubKeyClass;
		rsaKeyTemplate[ 3 ].type = CKA_VERIFY;
		rsaKeyTemplate[ 4 ].type = CKA_ENCRYPT;
		}

	/* Load the key into the token */
	status = C_CreateObject( pkcs11Info->hSession, rsaKeyTemplate, 
							 templateCount, &hRsaKey );
	zeroise( rsaKeyTemplate, sizeof( CK_ATTRIBUTE ) * templateCount );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		/* If we're trying to set a public key and this is one of those
		   tinkertoy tokens that only does private-key ops, return a more
		   appropriate error code */
		if( rsaKey->isPublicKey && \
			contextInfoPtr->capabilityInfo->encryptFunction == NULL &&
			contextInfoPtr->capabilityInfo->sigCheckFunction == NULL )
			cryptStatus = CRYPT_ERROR_NOTAVAIL;

		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Send the keying information to the context and set up the key ID 
	   information */
	cryptStatus = rsaSetPublicComponents( pkcs11Info, 
										  contextInfoPtr->objectHandle, hRsaKey,
										  FALSE );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = rsaSetKeyInfo( pkcs11Info, contextInfoPtr, 
									 hRsaKey, CK_OBJECT_NONE );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = updateActionFlags( pkcs11Info, 
										 contextInfoPtr->objectHandle,
										 hRsaKey, CRYPT_ALGO_RSA,
										 !rsaKey->isPublicKey );
		}
	if( cryptStatusError( cryptStatus ) )
		C_DestroyObject( pkcs11Info->hSession, hRsaKey );
	else
		{
		/* Remember that this object is backed by a crypto device */
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
		}

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int rsaGenerateKey( CONTEXT_INFO *contextInfoPtr, const int keysizeBits )
	{
	static const CK_MECHANISM mechanism = { CKM_RSA_PKCS_KEY_PAIR_GEN, NULL_PTR, 0 };
	static const CK_BBOOL bTrue = TRUE;
	static const BYTE exponent[] = { 0x01, 0x00, 0x01 };
	const CK_ULONG modulusBits = keysizeBits;
	CK_ATTRIBUTE privateKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_DECRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		};
	CK_ATTRIBUTE publicKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_ENCRYPT, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_VERIFY, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PUBLIC_EXPONENT, ( CK_VOID_PTR ) exponent, sizeof( exponent ) },
		{ CKA_MODULUS_BITS, ( CK_VOID_PTR ) &modulusBits, sizeof( CK_ULONG ) }
		};
	CK_OBJECT_HANDLE hPublicKey, hPrivateKey;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	CK_RV status;
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keysizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keysizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Generate the keys */
	status = C_GenerateKeyPair( pkcs11Info->hSession,
								( CK_MECHANISM_PTR ) &mechanism,
								publicKeyTemplate, 6, privateKeyTemplate, 6,
								&hPublicKey, &hPrivateKey );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Send the keying information to the context and set up the key ID 
	   information */
	cryptStatus = rsaSetPublicComponents( pkcs11Info, 
										  contextInfoPtr->objectHandle, 
										  hPublicKey, FALSE );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = rsaSetKeyInfo( pkcs11Info, contextInfoPtr, hPrivateKey, 
									 hPublicKey );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = updateActionFlags( pkcs11Info, 
										 contextInfoPtr->objectHandle,
										 hPrivateKey, CRYPT_ALGO_RSA, TRUE );
		}
	if( cryptStatusError( cryptStatus ) )
		{
		C_DestroyObject( pkcs11Info->hSession, hPublicKey );
		C_DestroyObject( pkcs11Info->hSession, hPrivateKey );
		}
	else
		{
		/* Remember that this object is backed by a crypto device */
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
		}

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int rsaSign( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_MECHANISM mechanism = { CKM_RSA_PKCS, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE *bufPtr = buffer;
	const int keySize = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
	int cryptStatus, i;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == keySize );

	/* Undo the PKCS #1 padding to make CKM_RSA_PKCS look like 
	   CKM_RSA_X_509 */
	REQUIRES( bufPtr[ 0 ] == 0 && bufPtr[ 1 ] == 1 && bufPtr[ 2 ] == 0xFF );
	for( i = 2; i < keySize; i++ )
		{
		if( bufPtr[ i ] == 0 )
			break;
		}
	i++;	/* Skip final 0 byte */

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericSign( pkcs11Info, contextInfoPtr, &mechanism, 
							   bufPtr + i, keySize - i, buffer, keySize );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int rsaVerify( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_MECHANISM mechanism = { CKM_RSA_X_509, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE data[ CRYPT_MAX_PKCSIZE + 8 ];
	const int keySize = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
	int cryptStatus;

	/* This function is present but isn't used as part of any normal 
	   operation because cryptlib does the same thing much faster in 
	   software and because some tokens don't support public-key 
	   operations */
	DEBUG_PRINT(( "Warning: rsaVerify() called for device object, should "
				  "be handled via native object.\n" ));

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == keySize );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericVerify( pkcs11Info, contextInfoPtr, &mechanism, 
								 data, keySize, buffer, keySize );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int rsaEncrypt( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_MECHANISM mechanism = { CKM_RSA_PKCS, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE *bufPtr = buffer;
	const int keySize = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
	int cryptStatus, i;

	/* This function is present but isn't used as part of any normal 
	   operation because cryptlib does the same thing much faster in 
	   software and because some tokens don't support public-key 
	   operations.  The only way that it can be invoked is by calling
	   cryptEncrypt() directly on a device context */

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == keySize );

	/* Undo the PKCS #1 padding to make CKM_RSA_PKCS look like 
	   CKM_RSA_X_509 */
	assert( bufPtr[ 0 ] == 0 && bufPtr[ 1 ] == 2 );
	for( i = 2; i < keySize; i++ )
		{
		if( bufPtr[ i ] == 0 )
			break;
		}
	i++;	/* Skip final 0 byte */
	memmove( bufPtr, bufPtr + i, keySize - i );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericEncrypt( pkcs11Info, contextInfoPtr, &mechanism, 
								  bufPtr, keySize - i, keySize );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int rsaDecrypt( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_MECHANISM mechanism = { CKM_RSA_PKCS, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	MESSAGE_DATA msgData;
	BYTE *bufPtr = buffer;
	const int keySize = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
	int cryptStatus, i, resultLen;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == keySize );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericDecrypt( pkcs11Info, contextInfoPtr, &mechanism, 
								  buffer, keySize, &resultLen );
	krnlReleaseObject( iCryptDevice );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Redo the PKCS #1 padding to CKM_RSA_PKCS look like CKM_RSA_X_509.  
	   Note that this doesn't have to be cryptographically strong since
	   it gets stripped as soon as we return to the caller, it just has
	   to be random:

	  bufPtr							 keySize
		|									|
		+---+---+------------+---+----------+
		| 0 | 1 |   random   | 0 |   key    |
		+---+---+------------+---+----------+
				|			 |	 |			|
				<------------>	 <---------->
				 keySize -		   resultLen
				 resultLen - 3

	   This gets a bit ugly because the random padding has to be nonzero,
	   which would require using the non-nonce RNG.  To work around this,
	   we look for any zeroes in the data and fill them with some other
	   value */
	REQUIRES( rangeCheck( keySize - resultLen, resultLen, length ) );
	memmove( bufPtr + keySize - resultLen, bufPtr, resultLen );
	bufPtr[ 0 ] = 0;
	bufPtr[ 1 ] = 2;
	setMessageData( &msgData, bufPtr + 2, keySize - resultLen - 3 );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_GETATTRIBUTE_S, &msgData, 
								   CRYPT_IATTRIBUTE_RANDOM_NONCE );
	for( i = 2; i < keySize - resultLen - 1; i++ )
		{
		if( bufPtr[ i ] == 0 )
			{
			/* Create some sort of non-constant non-zero value to replace 
			   the zero byte with, since PKCS #1 can't have zero bytes.  
			   Note again that this doesn't have to be a strong random 
			   value, it just has to vary a bit */
			const int pad = 0xAA ^ ( i & 0xFF );
			bufPtr[ i ] = pad ? pad : 0x21;
			}
		}
	bufPtr[ keySize - resultLen - 1 ] = 0;
	ENSURES( 2 + ( keySize - resultLen - 3 ) + 1 + resultLen == keySize );

	return( cryptStatus );
	}

/****************************************************************************
*																			*
*							DSA Mapping Functions							*
*																			*
****************************************************************************/

/* DSA algorithm-specific mapping functions */

static int dsaSetKeyInfo( PKCS11_INFO *pkcs11Info, 
						  const CRYPT_CONTEXT iCryptContext,
						  const CK_OBJECT_HANDLE hPrivateKey,
						  const CK_OBJECT_HANDLE hPublicKey,
						  const void *p, const int pLen,
						  const void *q, const int qLen,
						  const void *g, const int gLen,
						  const void *y, const int yLen,
						  const BOOLEAN nativeContext )
	{
	MESSAGE_DATA msgData;
	BYTE keyDataBuffer[ ( CRYPT_MAX_PKCSIZE * 4 ) + 8 ];
	BYTE idBuffer[ KEYID_SIZE + 8 ];
	int keyDataSize, cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );
	assert( isReadPtr( p, pLen ) );
	assert( isReadPtr( q, qLen ) );
	assert( isReadPtr( g, gLen ) );
	assert( isReadPtr( y, yLen ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Send the public key data to the context.  We send the keying 
	   information as CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL rather than 
	   CRYPT_IATTRIBUTE_KEY_SPKI since the latter transitions the context 
	   into the high state.  We don't want to do this because we're already 
	   in the middle of processing a message that does this on completion, 
	   all that we're doing here is sending in encoded public key data for 
	   use by objects such as certificates */
	cryptStatus = writeFlatPublicKey( keyDataBuffer, CRYPT_MAX_PKCSIZE * 3,
									  &keyDataSize, CRYPT_ALGO_DSA, 
									  p, pLen, q, qLen, g, gLen, y, yLen );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	setMessageData( &msgData, keyDataBuffer, keyDataSize );
	if( nativeContext )
		{
		/* If we're just setting public key components for a native context, 
		   we're done */
		return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
								 &msgData, CRYPT_IATTRIBUTE_KEY_SPKI ) );
		}
	cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
								   &msgData, CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Remember what we've set up.  Note that PKCS #11 tokens create 
	   distinct public- and private-key objects but we're only interested
	   in the private-key one, so we store the private-key object handle
	   in the context */
	cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
								   ( MESSAGE_CAST ) &hPrivateKey, 
								   CRYPT_IATTRIBUTE_DEVICEOBJECT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Get the key ID from the context and use it as the object ID.  Since 
	   some objects won't allow after-the-even ID updates, we don't treat a
	   failure to update as an error */
	setMessageData( &msgData, idBuffer, KEYID_SIZE );
	cryptStatus = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S, 
								   &msgData, CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusOK( cryptStatus ) )
		{
		CK_ATTRIBUTE idTemplate = { CKA_ID, msgData.data, msgData.length };

		if( hPublicKey != CRYPT_UNUSED )
			C_SetAttributeValue( pkcs11Info->hSession, hPublicKey, 
								 &idTemplate, 1 );
		C_SetAttributeValue( pkcs11Info->hSession, hPrivateKey, 
							 &idTemplate, 1 );
		}
	
	return( cryptStatus );
	}

int dsaSetPublicComponents( PKCS11_INFO *pkcs11Info,
							const CRYPT_CONTEXT iCryptContext,
							const CK_OBJECT_HANDLE hDsaKey )
	{
	BYTE p[ CRYPT_MAX_PKCSIZE + 8 ], q[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE g[ CRYPT_MAX_PKCSIZE + 8 ], y[ CRYPT_MAX_PKCSIZE + 8 ];
	int pLen, qLen = DUMMY_INIT, gLen = DUMMY_INIT, yLen = DUMMY_INIT;
	int cryptStatus;

	assert( isWritePtr( pkcs11Info, sizeof( PKCS11_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Get the public key components from the device */
	cryptStatus = readAttributeValue( pkcs11Info, hDsaKey, CKA_PRIME, 
									  p, CRYPT_MAX_PKCSIZE, &pLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hDsaKey, CKA_SUBPRIME, 
										  q, CRYPT_MAX_PKCSIZE, &qLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hDsaKey, CKA_BASE, 
										  g, CRYPT_MAX_PKCSIZE, &gLen );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = readAttributeValue( pkcs11Info, hDsaKey, CKA_VALUE, 
										  y, CRYPT_MAX_PKCSIZE, &yLen );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	return( dsaSetKeyInfo( pkcs11Info, iCryptContext, CK_OBJECT_NONE, hDsaKey, 
						   p, pLen, q, qLen, g, gLen, y, yLen, TRUE ) );
	}

static int dsaInitKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					   const int keyLength )
	{
	static const CK_OBJECT_CLASS privKeyClass = CKO_PRIVATE_KEY;
	static const CK_OBJECT_CLASS pubKeyClass = CKO_PUBLIC_KEY;
	static const CK_KEY_TYPE type = CKK_DSA;
	static const CK_BBOOL bTrue = TRUE;
	CK_ATTRIBUTE dsaKeyTemplate[] = {
		/* Shared fields */
		{ CKA_CLASS, ( CK_VOID_PTR ) &privKeyClass, sizeof( CK_OBJECT_CLASS ) },
		{ CKA_KEY_TYPE, ( CK_VOID_PTR ) &type, sizeof( CK_KEY_TYPE ) },
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		{ CKA_VALUE, NULL, 0 },
		/* Private-key only fields */
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		};
	const CRYPT_PKCINFO_DLP *dsaKey = ( CRYPT_PKCINFO_DLP * ) key;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	CK_OBJECT_HANDLE hDsaKey;
	CK_RV status;
	BYTE yValue[ CRYPT_MAX_PKCSIZE + 8 ];
	const void *yValuePtr;
	const int templateCount = dsaKey->isPublicKey ? 9 : 10;
	int yValueLength, cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( keyLength == sizeof( CRYPT_PKCINFO_DLP ) );

	/* Creating a private-key object is somewhat problematic since the 
	   PKCS #11 interpretation of DSA reuses CKA_VALUE for x in the private
	   key and y in the public key, so it's not possible to determine y from
	   a private key because the x value is sensitive and can't be extracted.
	   Because of this we have to create a native private-key context (which 
	   will generate the y value from x), read out the y value, and destroy
	   it again (see the comments in the DSA generate key section for more on
	   this problem).  Since this doesn't require the device, we do it before 
	   we grab the device */
	if( !dsaKey->isPublicKey )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;
		MESSAGE_DATA msgData;
		STREAM stream;
		BYTE pubkeyBuffer[ ( CRYPT_MAX_PKCSIZE * 3 ) + 8 ], label[ 8 + 8 ];
		void *yValueDataPtr = DUMMY_INIT_PTR;
		int yValueDataSize;

		/* Create a native private-key DSA context, which generates the y 
		   value internally */
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_DSA );
		cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									   IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
									   OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		setMessageData( &msgData, label, 8 );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
						 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S, 
						 &msgData, CRYPT_CTXINFO_LABEL );
		setMessageData( &msgData, ( MESSAGE_CAST ) dsaKey, 
						sizeof( CRYPT_PKCINFO_DLP ) );
		cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
									   IMESSAGE_SETATTRIBUTE_S, &msgData, 
									   CRYPT_CTXINFO_KEY_COMPONENTS );
		if( cryptStatusError( cryptStatus ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( cryptStatus );
			}

		/* Get the public key data and extract the y value from it.  Note 
		   that the data used is represented in DER-canonical form, there may 
		   be PKCS #11 implementations that can't handle this (for example 
		   they may require y to be zero-padded to make it exactly 64 bytes 
		   rather than (say) 63 bytes if the high byte is zero) */
		setMessageData( &msgData, pubkeyBuffer, CRYPT_MAX_PKCSIZE * 3 );
		cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
									   IMESSAGE_GETATTRIBUTE_S, &msgData, 
									   CRYPT_IATTRIBUTE_KEY_SPKI );
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		if( cryptStatusError( cryptStatus ) )
			return( cryptStatus );
		sMemConnect( &stream, msgData.data, msgData.length );
		readSequence( &stream, NULL );		/* SEQUENCE { */
		readUniversal( &stream );				/* AlgoID */
		readBitStringHole( &stream, NULL, 16, DEFAULT_TAG );/* BIT STRING */
		status = readGenericHole( &stream, &yValueDataSize, 16, 
								  BER_INTEGER  );/* INTEGER */
		if( cryptStatusOK( status ) )
			status = sMemGetDataBlock( &stream, &yValueDataPtr, 
									   yValueDataSize );
		ENSURES( cryptStatusOK( status ) );
		ENSURES( yValueDataSize >= 16 && \
				 yValueDataSize <= CRYPT_MAX_PKCSIZE );
		memcpy( yValue, yValueDataPtr, yValueDataSize );
		sMemDisconnect( &stream );

		/* The y value is the recovered value from the key data */
		yValuePtr = yValue;
		yValueLength = yValueDataSize;
		}
	else
		{
		/* It's a public key, use the pre-generated y value */
		yValuePtr = dsaKey->y,
		yValueLength = bitsToBytes( dsaKey->yLen );
		}

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the key values */
	dsaKeyTemplate[ 5 ].pValue = ( CK_VOID_PTR ) dsaKey->p;
	dsaKeyTemplate[ 5 ].ulValueLen = bitsToBytes( dsaKey->pLen );
	dsaKeyTemplate[ 6 ].pValue = ( CK_VOID_PTR ) dsaKey->q;
	dsaKeyTemplate[ 6 ].ulValueLen = bitsToBytes( dsaKey->qLen );
	dsaKeyTemplate[ 7 ].pValue = ( CK_VOID_PTR ) dsaKey->g;
	dsaKeyTemplate[ 7 ].ulValueLen = bitsToBytes( dsaKey->gLen );
	if( !dsaKey->isPublicKey )
		{
		dsaKeyTemplate[ 8 ].pValue = ( CK_VOID_PTR ) dsaKey->x;
		dsaKeyTemplate[ 8 ].ulValueLen = bitsToBytes( dsaKey->xLen );
		}
	else
		{
		dsaKeyTemplate[ 8 ].pValue = ( CK_VOID_PTR ) dsaKey->y;
		dsaKeyTemplate[ 8 ].ulValueLen = bitsToBytes( dsaKey->yLen );

		/* If it's a public key, we need to change the type and the 
		   indication of the operations that it's allowed to perform */
		dsaKeyTemplate[ 0 ].pValue = ( CK_VOID_PTR ) &pubKeyClass;
		dsaKeyTemplate[ 3 ].type = CKA_VERIFY;
		}

	/* Load the key into the token */
	status = C_CreateObject( pkcs11Info->hSession, dsaKeyTemplate, 
							 templateCount, &hDsaKey );
	zeroise( dsaKeyTemplate, sizeof( CK_ATTRIBUTE ) * templateCount );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		/* If we're trying to set a public key and this is one of those
		   tinkertoy tokens that only does private-key ops, return a more
		   appropriate error code */
		if( dsaKey->isPublicKey && \
			contextInfoPtr->capabilityInfo->sigCheckFunction == NULL )
			cryptStatus = CRYPT_ERROR_NOTAVAIL;

		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Send the keying information to the context and set up the key ID 
	   information */
	cryptStatus = dsaSetKeyInfo( pkcs11Info, contextInfoPtr->objectHandle, 
								 hDsaKey, CK_OBJECT_NONE,
								 dsaKey->p, bitsToBytes( dsaKey->pLen ), 
								 dsaKey->q, bitsToBytes( dsaKey->qLen ),
								 dsaKey->g, bitsToBytes( dsaKey->gLen ),
								 yValuePtr, yValueLength, FALSE );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = updateActionFlags( pkcs11Info, 
										 contextInfoPtr->objectHandle,
										 hDsaKey, CRYPT_ALGO_DSA,
										 !dsaKey->isPublicKey );
		}
	if( cryptStatusError( cryptStatus ) )
		C_DestroyObject( pkcs11Info->hSession, hDsaKey );
	else
		{
		/* Remember that this object is backed by a crypto device */
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
		}

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int dsaGenerateKey( CONTEXT_INFO *contextInfoPtr, const int keysizeBits )
	{
	static const CK_MECHANISM mechanism = { CKM_DSA_KEY_PAIR_GEN, NULL_PTR, 0 };
	static const CK_BBOOL bTrue = TRUE;
	CK_ATTRIBUTE privateKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIVATE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_SENSITIVE, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_SIGN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		};
	CK_ATTRIBUTE publicKeyTemplate[] = {
		{ CKA_TOKEN, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_LABEL, contextInfoPtr->label, contextInfoPtr->labelSize },
		{ CKA_VERIFY, ( CK_VOID_PTR ) &bTrue, sizeof( CK_BBOOL ) },
		{ CKA_PRIME, NULL, 0 },
		{ CKA_SUBPRIME, NULL, 0 },
		{ CKA_BASE, NULL, 0 },
		};
	CK_ATTRIBUTE yValueTemplate = { CKA_VALUE, NULL, CRYPT_MAX_PKCSIZE * 2 };
	CK_OBJECT_HANDLE hPublicKey, hPrivateKey;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	BYTE pubkeyBuffer[ ( CRYPT_MAX_PKCSIZE * 3 ) + 8 ], label[ 8 + 8 ];
	CK_RV status;
	STREAM stream;
	void *dataPtr = DUMMY_INIT_PTR;
	int length, cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keysizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keysizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	/* CKM_DSA_KEY_PAIR_GEN is really a Clayton's key generation mechanism 
	   since it doesn't actually generate the p, q, or g values (presumably 
	   it dates back to the original FIPS 186 shared domain parameters idea).
	   Because of this we'd have to generate half the key ourselves in a 
	   native context, then copy portions from the native context over in 
	   flat form and complete the keygen via the device.  The easiest way to
	   do this is to create a native DSA context, generate a key, grab the
	   public portions, and destroy the context again (i.e. generate a full
	   key on a superscalar 2GHz RISC CPU, then throw half of it away, and 
	   regenerate it on a 5MHz 8-bit tinkertoy).  Since the keygen can take 
	   awhile and doesn't require the device, we do it before we grab the 
	   device */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_DSA );
	cryptStatus = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								   IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								   OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	setMessageData( &msgData, label, 8 );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
					 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE_S,
					 &msgData, CRYPT_CTXINFO_LABEL );
	cryptStatus = krnlSendNotifier( createInfo.cryptHandle, 
									IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( cryptStatus ) )
		{
		setMessageData( &msgData, pubkeyBuffer, CRYPT_MAX_PKCSIZE * 3 );
		cryptStatus = krnlSendMessage( createInfo.cryptHandle, 
									   IMESSAGE_GETATTRIBUTE_S, &msgData, 
									   CRYPT_IATTRIBUTE_KEY_SPKI );
		}
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Set up the public key information by extracting the flat values from 
	   the SubjectPublicKeyInfo.  Note that the data used is represented in
	   DER-canonical form, there may be PKCS #11 implementations that
	   can't handle this (for example they may require q to be zero-padded
	   to make it exactly 20 bytes rather than (say) 19 bytes if the high
	   byte is zero) */
	sMemConnect( &stream, pubkeyBuffer, msgData.length );
	readSequence( &stream, NULL );				/* SEQUENCE */
	readSequence( &stream, NULL );					/* SEQUENCE */
	readUniversal( &stream );							/* OID */
	readSequence( &stream, NULL );						/* SEQUENCE */
	cryptStatus = readGenericHole( &stream, &length, 16,	/* p */
								   BER_INTEGER  );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = sMemGetDataBlock( &stream, &dataPtr, length );
	if( cryptStatusError( cryptStatus ) )
		retIntError();
	publicKeyTemplate[ 3 ].pValue = dataPtr;
	publicKeyTemplate[ 3 ].ulValueLen = length;
	sSkip( &stream, length );
	cryptStatus = readGenericHole( &stream, &length, 16, 	/* q */
								   BER_INTEGER  );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = sMemGetDataBlock( &stream, &dataPtr, length );
	if( cryptStatusError( cryptStatus ) )
		retIntError();
	publicKeyTemplate[ 4 ].pValue = dataPtr;
	publicKeyTemplate[ 4 ].ulValueLen = length;
	sSkip( &stream, length );
	cryptStatus = readGenericHole( &stream, &length, 16, 	/* g */
								   BER_INTEGER  );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = sMemGetDataBlock( &stream, &dataPtr, length );
	if( cryptStatusError( cryptStatus ) )
		retIntError();
	publicKeyTemplate[ 5 ].pValue = dataPtr;
	publicKeyTemplate[ 5 ].ulValueLen = length;
	sMemDisconnect( &stream );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Generate the keys */
	status = C_GenerateKeyPair( pkcs11Info->hSession,
								( CK_MECHANISM_PTR ) &mechanism,
								( CK_ATTRIBUTE_PTR ) publicKeyTemplate, 6,
								( CK_ATTRIBUTE_PTR ) privateKeyTemplate, 5,
								&hPublicKey, &hPrivateKey );
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusError( cryptStatus ) )
		{
		krnlReleaseObject( iCryptDevice );
		return( cryptStatus );
		}

	/* Read back the generated y value, send the public key information to 
	   the context, and set up the key ID information.  The odd two-phase y 
	   value read is necessary for buggy implementations that fail if the 
	   given size isn't exactly the same as the data size */
	status = C_GetAttributeValue( pkcs11Info->hSession, hPublicKey,
								  &yValueTemplate, 1 );
	if( status == CKR_OK )
		{
		yValueTemplate.pValue = pubkeyBuffer;
		status = C_GetAttributeValue( pkcs11Info->hSession, hPublicKey, 
									  &yValueTemplate, 1 );
		}
	cryptStatus = pkcs11MapError( status, CRYPT_ERROR_FAILED );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = dsaSetKeyInfo( pkcs11Info, contextInfoPtr->objectHandle, 
			hPrivateKey, hPublicKey,
			publicKeyTemplate[ 3 ].pValue, publicKeyTemplate[ 3 ].ulValueLen, 
			publicKeyTemplate[ 4 ].pValue, publicKeyTemplate[ 4 ].ulValueLen, 
			publicKeyTemplate[ 5 ].pValue, publicKeyTemplate[ 5 ].ulValueLen,
			yValueTemplate.pValue, yValueTemplate.ulValueLen, FALSE );
		}
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = updateActionFlags( pkcs11Info, 
										 contextInfoPtr->objectHandle,
										 hPrivateKey, CRYPT_ALGO_DSA, TRUE );
		}
	if( cryptStatusError( cryptStatus ) )
		{
		C_DestroyObject( pkcs11Info->hSession, hPublicKey );
		C_DestroyObject( pkcs11Info->hSession, hPrivateKey );
		}
	else
		/* Remember that this object is backed by a crypto device */
		contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;

	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
	}

static int dsaSign( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
	static const CK_MECHANISM mechanism = { CKM_DSA, NULL_PTR, 0 };
	CRYPT_DEVICE iCryptDevice;
	PKCS11_INFO *pkcs11Info;
	DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
	PKC_INFO *dsaKey = contextInfoPtr->ctxPKC;
	BIGNUM *r, *s;
	BYTE signature[ 40 + 8 ];
	int cryptStatus;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );

	REQUIRES( length == sizeof( DLP_PARAMS ) );
	REQUIRES( dlpParams->inParam1 != NULL && \
			  dlpParams->inLen1 == 20 );
	REQUIRES( dlpParams->inParam2 == NULL && dlpParams->inLen2 == 0 );
	REQUIRES( dlpParams->outParam != NULL && \
			  dlpParams->outLen >= ( 2 + 20 ) * 2 && \
			  dlpParams->outLen < MAX_INTLENGTH_SHORT );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericSign( pkcs11Info, contextInfoPtr, &mechanism, 
							   dlpParams->inParam1, dlpParams->inLen1, 
							   signature, 40 );
	krnlReleaseObject( iCryptDevice );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Encode the result as a DL data block.  We have to do this via bignums, 
	   but this isn't a big deal since DSA signing via tokens is almost never 
	   used */
	r = BN_new();
	if( r == NULL )
		return( CRYPT_ERROR_MEMORY );
	s = BN_new();
	if( s == NULL )
		{
		BN_free( r );
		return( CRYPT_ERROR_MEMORY );
		}
	cryptStatus = importBignum( r, signature, 20, 
							    bitsToBytes( 160 - 32 ), 20, NULL, 
								KEYSIZE_CHECK_NONE );
	if( cryptStatusOK( cryptStatus ) )
		cryptStatus = importBignum( r, signature + 20, 20,
								    bitsToBytes( 160 - 32 ), 20, NULL, 
									KEYSIZE_CHECK_NONE );
	if( cryptStatusOK( cryptStatus ) )
		{
		cryptStatus = \
			dsaKey->encodeDLValuesFunction( dlpParams->outParam, 
											dlpParams->outLen, &dlpParams->outLen,
											r, s, dlpParams->formatType );
		}
	BN_clear_free( s );
	BN_clear_free( r );
	return( cryptStatus );
	}

static int dsaVerify( CONTEXT_INFO *contextInfoPtr, BYTE *buffer, int length )
	{
/*	static const CK_MECHANISM mechanism = { CKM_DSA, NULL_PTR, 0 }; */
/*	CRYPT_DEVICE iCryptDevice; */
	const DLP_PARAMS *dlpParams = ( DLP_PARAMS * ) buffer;
/*	PKC_INFO *dsaKey = contextInfoPtr->ctxPKC; */
/*	BIGNUM r, s; */
/*	BYTE signature[ 40 + 8 ]; */
/*	int cryptStatus; */

	/* This function is present but isn't used as part of any normal 
	   operation because cryptlib does the same thing much faster in 
	   software and because some tokens don't support public-key 
	   operations */
	DEBUG_PRINT(( "Warning: dsaVerify() called for device object, should "
				  "be handled via native object.\n" ));

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( buffer, length ) );
	
	REQUIRES( length == sizeof( DLP_PARAMS ) );
	REQUIRES( dlpParams->inParam1 != NULL && dlpParams->inLen1 == 20 );
	REQUIRES( dlpParams->inParam2 != NULL && \
			  ( ( dlpParams->formatType == CRYPT_FORMAT_CRYPTLIB && \
				  dlpParams->inLen2 >= 46 ) || \
				( dlpParams->formatType == CRYPT_FORMAT_PGP && \
				  dlpParams->inLen2 == 44 ) || \
				( dlpParams->formatType == CRYPT_IFORMAT_SSH && \
				  dlpParams->inLen2 == 40 ) ) );
	REQUIRES( dlpParams->outParam == NULL && dlpParams->outLen == 0 );

	/* This code can never be called since DSA public-key contexts are 
	   always native contexts */
	retIntError();

#if 0
	/* Decode the values from a DL data block and make sure r and s are
	   valid */
	BN_init( &r );
	BN_init( &s );
	cryptStatus = \
		dsaKey->decodeDLValuesFunction( dlpParams->inParam2, dlpParams->inLen2, 
										&r, &s, NULL, dlpParams->formatType );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );

	/* Get the information for the device associated with this context */
	cryptStatus = getContextDeviceInfo( contextInfoPtr->objectHandle, 
										&iCryptDevice, &pkcs11Info );
	if( cryptStatusError( cryptStatus ) )
		return( cryptStatus );
	cryptStatus = genericVerify( pkcs11Info, contextInfoPtr, &mechanism, 
								 buffer, 20, signature, 40 );
	krnlReleaseObject( iCryptDevice );
	return( cryptStatus );
#endif /* 0 */
	}

/****************************************************************************
*																			*
*						 	Device Capability Routines						*
*																			*
****************************************************************************/

/* PKC mechanism information */

static const PKCS11_MECHANISM_INFO mechanismInfoPKC[] = {
	/* The handling of the RSA mechanism is a bit odd.  Almost everyone 
	   supports CKM_RSA_X_509 even though what's reported as being supported 
	   is CKM_RSA_PKCS, however the PKCS mechanism is often implemented in a 
	   buggy manner with all sorts of problems with handling the padding.  
	   The safest option would be to use the raw RSA one and do the padding 
	   ourselves, which means that it'll always be done right.  Since some 
	   implementations report raw RSA as being unavailable even though it's 
	   present, we detect it by checking for the PKCS mechanism but using 
	   raw RSA.  However, some implementations genuinely don't do raw RSA, so
	   the code fakes it by removing/adding dummy PKCS padding as required 
	   so that the caller sees raw RSA and the device sees PKCS.  This is a
	   compromise: We can handle the real (rather than faked) PKCS padding
	   ourselves and work around bugs in the output from other 
	   implementations, but we can't implement any new mechanisms other than
	   PKCS without support in the device.  The only implementation where 
	   even this causes problems is some versions of GemSAFE, which don't do 
	   raw RSA and also get the PKCS mechanism wrong */
#ifdef USE_DH
	{ CKM_DH_PKCS_DERIVE, CKM_DH_PKCS_KEY_PAIR_GEN, CKM_NONE, CRYPT_ALGO_DH, CRYPT_MODE_NONE, CKK_DH,
	  NULL, dhInitKey, dhGenerateKey, 
	  dhEncrypt, dhDecrypt, NULL, NULL },
#endif /* USE_DH */
	{ CKM_RSA_PKCS, CKM_RSA_PKCS_KEY_PAIR_GEN, CKM_NONE, CRYPT_ALGO_RSA, CRYPT_MODE_NONE, CKK_RSA,
	  NULL, rsaInitKey, rsaGenerateKey, 
	  rsaEncrypt, rsaDecrypt, rsaSign, rsaVerify },
#ifdef USE_DSA
	{ CKM_DSA, CKM_DSA_KEY_PAIR_GEN, CKM_NONE, CRYPT_ALGO_DSA, CRYPT_MODE_NONE, CKK_DSA,
	  NULL, dsaInitKey, dsaGenerateKey, 
	  NULL, NULL, dsaSign, dsaVerify },
#endif /* USE_DSA */
	{ CKM_NONE, CKM_NONE, CKM_NONE, CRYPT_ERROR, CRYPT_ERROR, },
		{ CKM_NONE, CKM_NONE, CKM_NONE, CRYPT_ERROR, CRYPT_ERROR, }
	};

const PKCS11_MECHANISM_INFO *getMechanismInfoPKC( int *mechanismInfoSize )
	{
	assert( isWritePtr( mechanismInfoSize, sizeof( int ) ) );

	*mechanismInfoSize = FAILSAFE_ARRAYSIZE( mechanismInfoPKC, \
											 PKCS11_MECHANISM_INFO );
	return( mechanismInfoPKC );
	}
#endif /* USE_PKCS11 */
