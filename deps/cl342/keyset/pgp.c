/****************************************************************************
*																			*
*						  cryptlib PGP Keyset Routines						*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "keyset.h"
  #include "pgp_key.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "keyset/keyset.h"
  #include "keyset/pgp_key.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

#ifdef USE_PGPKEYS

/* A PGP private keyset can contain multiple key objects so before we do 
   anything with the keyset we scan it and build an in-memory index of 
   what's present.  When we perform an update we just flush the in-memory 
   information to disk.

   Each keyset can contain information for multiple personalities (although 
   for private keys it's unlikely to contain more than a small number), we 
   allow a maximum of MAX_PGP_OBJECTS per keyset.  A setting of 16 objects 
   consumes ~4K of memory (16 x ~256) so we choose that as the limit */

#ifdef CONFIG_CONSERVE_MEMORY
  #define MAX_PGP_OBJECTS	4
#else
  #define MAX_PGP_OBJECTS	16
#endif /* CONFIG_CONSERVE_MEMORY */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Find a free PGP keyset entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static PGP_INFO *findFreeEntry( IN_ARRAY( noPgpObjects ) \
									const PGP_INFO *pgpInfo,
								IN_LENGTH_SHORT const int noPgpObjects )
	{
	int i;

	assert( isReadPtr( pgpInfo, \
					   sizeof( PGP_INFO ) * noPgpObjects ) );

	REQUIRES_N( noPgpObjects >= 1 && noPgpObjects < MAX_INTLENGTH_SHORT );

	for( i = 0; i < noPgpObjects && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		if( pgpInfo[ i ].keyData == NULL )
			break;
		}
	ENSURES_N( i < FAILSAFE_ITERATIONS_MED );
	if( i >= noPgpObjects )
		return( NULL );

	return( ( PGP_INFO * ) &pgpInfo[ i ] );
	}

/* Free object entries */

STDC_NONNULL_ARG( ( 1 ) ) \
void pgpFreeEntry( INOUT PGP_INFO *pgpInfo )
	{
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) ) );

	if( pgpInfo->keyData != NULL )
		{
		zeroise( pgpInfo->keyData, pgpInfo->keyDataLen );
		clFree( "pgpFreeEntry", pgpInfo->keyData );
		pgpInfo->keyData = NULL;
		pgpInfo->keyDataLen = 0;
		}
	zeroise( pgpInfo, sizeof( PGP_INFO  ) );
	}

/* Create a decryption context for the private key from a user-supplied 
   password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int createDecryptionContext( OUT_HANDLE_OPT CRYPT_CONTEXT *iSessionKey,
									const PGP_KEYINFO *keyInfo,
									IN_BUFFER( passwordLength ) \
										const void *password, 
									IN_LENGTH_NAME const int passwordLength )
	{
	CRYPT_CONTEXT iLocalContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	static const int mode = CRYPT_MODE_CFB;	/* int vs.enum */
	int ivSize, status;

	assert( isWritePtr( iSessionKey, sizeof( CRYPT_CONTEXT ) ) );
	assert( isReadPtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( passwordLength >= MIN_NAME_LENGTH && \
			  passwordLength < MAX_ATTRIBUTE_SIZE );

	/* Convert the user password into an encryption context */
	setMessageCreateObjectInfo( &createInfo, keyInfo->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT, 
							  &createInfo,  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iLocalContext = createInfo.cryptHandle;
	status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) )
		{
		status = pgpPasswordToKey( iLocalContext, 
								   ( keyInfo->cryptAlgo == CRYPT_ALGO_AES && \
								     keyInfo->aesKeySize > 0 ) ? \
									keyInfo->aesKeySize : CRYPT_UNUSED,
								   password, passwordLength, 
								   keyInfo->hashAlgo, 
								   ( keyInfo->saltSize > 0 ) ? \
									keyInfo->salt : NULL, keyInfo->saltSize,
								   keyInfo->keySetupIterations );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Load the IV into the context */
	status = krnlSendMessage( iLocalContext, IMESSAGE_GETATTRIBUTE, 
							  &ivSize, CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusOK( status ) && ivSize > keyInfo->ivSize )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) keyInfo->iv, 
						keyInfo->ivSize );
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_IV );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iSessionKey = iLocalContext;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Find a Key								*
*																			*
****************************************************************************/

/* Generate a cryptlib-style key ID for a PGP key and check it against the
   given key ID.  This will really suck with large public keyrings since it
   requires creating a context for each key that we check, but there's no 
   easy way around this and in any case it only occurs when using PGP keys 
   with non-PGP messages, which is fairly rare */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN matchKeyID( const PGP_KEYINFO *keyInfo, 
						   IN_BUFFER( requiredIDlength ) const BYTE *requiredID,
						   IN_LENGTH_KEYID const int requiredIDlength,
						   const BOOLEAN isPGPkeyID )
	{
	CRYPT_CONTEXT iLocalContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE keyID[ KEYID_SIZE + 8 ];
	int status;

	assert( isReadPtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isReadPtr( requiredID, requiredIDlength ) );

	ENSURES_B( requiredIDlength == PGP_KEYID_SIZE || \
			   requiredIDlength == KEYID_SIZE );

	/* If it's a PGP key ID we can check it directly against the two PGP
	   key IDs.  We don't distinguish between the two ID types externally
	   because it's a pain for external code to have to know that there are
	   two ID types that look the same and are often used interchangeably 
	   but of the two only the OpenPGP variant is valid for all keys (in 
	   fact there are some broken PGP variants that use PGP 2.x IDs marked 
	   as OpenPGP IDs, so checking both IDs is necessary for 
	   interoperability).  The mixing of ID types is safe because the 
	   chances of a collision are miniscule and the worst that can happen is 
	   that a signature check will fail (encryption keys are chosen by user 
	   ID and not key ID so accidentally using the wrong key to encrypt 
	   isn't an issue) */
	if( isPGPkeyID )
		{
		ENSURES_B( requiredIDlength == PGP_KEYID_SIZE );

		if( !memcmp( requiredID, keyInfo->openPGPkeyID, PGP_KEYID_SIZE ) )
			return( TRUE );
		return( ( keyInfo->pkcAlgo == CRYPT_ALGO_RSA ) && \
				!memcmp( requiredID, keyInfo->pgpKeyID, PGP_KEYID_SIZE ) );
		}
	ENSURES_B( requiredIDlength == KEYID_SIZE );

	/* Generate the key ID via a context.  We have to set the OpenPGP key ID
	   before the key load to mark it as a PGP key otherwise the key check 
	   will fail since it's not a full X9.42 key with DLP validation 
	   parameters */
	setMessageCreateObjectInfo( &createInfo, keyInfo->pkcAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Couldn't create PKC context to generate key ID" ));
		assert( DEBUG_WARN );
		return( FALSE );
		}
	iLocalContext = createInfo.cryptHandle;
	setMessageData( &msgData, ( MESSAGE_CAST ) keyInfo->openPGPkeyID, 
					PGP_KEYID_SIZE );
	status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyInfo->pubKeyData,
						keyInfo->pubKeyDataLen );
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_KEY_PGP );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyID, KEYID_SIZE );
		status = krnlSendMessage( iLocalContext, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		}
	krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Couldn't initialise PKC context to generate key ID" ));
		assert( DEBUG_WARN );
		return( FALSE );
		}

	/* Check if it's the same as the key ID that we're looking for */
	return( !memcmp( requiredID, keyID, requiredIDlength ) ? TRUE : FALSE );
	}

/* Check whether a key matches the required user ID */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN pgpCheckKeyMatch( const PGP_INFO *pgpInfo, 
						  const PGP_KEYINFO *keyInfo, 
						  const KEY_MATCH_INFO *keyMatchInfo )
	{
	int i;

	assert( isReadPtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( isReadPtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isReadPtr( keyMatchInfo, sizeof( KEY_MATCH_INFO ) ) );

	/* If there's an explicitly requested key usage type, make sure that the 
	   key is suitable */
	if( ( keyMatchInfo->flags & KEYMGMT_MASK_USAGEOPTIONS ) && \
		!( keyInfo->usageFlags & keyMatchInfo->flags ) )
		return( FALSE );

	/* If we're searching by key ID, check whether this is the packet that 
	   we want */
	if( keyMatchInfo->keyIDtype == CRYPT_IKEYID_KEYID || \
		keyMatchInfo->keyIDtype == CRYPT_IKEYID_PGPKEYID )
		{
		return( matchKeyID( keyInfo, keyMatchInfo->keyID, 
					keyMatchInfo->keyIDlength,
					( keyMatchInfo->keyIDtype == CRYPT_IKEYID_PGPKEYID ) ? \
						TRUE : FALSE ) );
		}

	REQUIRES_B( keyMatchInfo->keyIDtype == CRYPT_KEYID_NAME || \
				keyMatchInfo->keyIDtype == CRYPT_KEYID_URI );

	/* We're searching by user ID, walk down the list of userIDs checking
	   for a match */
	for( i = 0; i < pgpInfo->lastUserID && i < MAX_PGP_USERIDS; i++ )
		{
		/* Check if it's the one that we want.  If it's a key with subkeys 
		   and no usage type is explicitly specified this will always return 
		   the main key.  This is the best solution since the main key is 
		   always a signing key, which is more likely to be what the user 
		   wants.  Encryption keys will typically only be accessed via 
		   envelopes and the enveloping code can specify a preference of an 
		   encryption-capable key, while signing keys will be read directly 
		   and pushed into the envelope */
		if( strFindStr( pgpInfo->userID[ i ], pgpInfo->userIDlen[ i ],
						( char * ) keyMatchInfo->keyID, 
						keyMatchInfo->keyIDlength ) >= 0 )
			return( TRUE );
		}
	ENSURES_B( i < MAX_PGP_USERIDS );

	return( FALSE );
	}

/* Locate a key based on an ID.  This is complicated somewhat by the fact 
   that PGP groups multiple keys around the same textual ID so we have to 
   check both keys and subkeys for a possible match */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 4 ) ) \
static PGP_INFO *findEntry( const PGP_INFO *pgpInfo,
							IN_LENGTH_SHORT const int noPgpObjects,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							IN_FLAGS_Z( KEYMGMT ) const int requestedUsage, 
							OUT_OPT_PTR_OPT PGP_KEYINFO **keyInfo )
	{
	CONST_INIT_STRUCT_4( KEY_MATCH_INFO keyMatchInfo, \
						 keyIDtype, keyID, keyIDlength, requestedUsage );
	int i;

	CONST_SET_STRUCT( keyMatchInfo.keyIDtype = keyIDtype; \
					  keyMatchInfo.keyID = keyID; \
					  keyMatchInfo.keyIDlength = keyIDlength; \
					  keyMatchInfo.flags = requestedUsage );

	assert( isReadPtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( keyInfo == NULL || \
			isWritePtr( keyInfo, sizeof( PGP_KEYINFO * ) ) );

	REQUIRES_N( noPgpObjects >= 1 && noPgpObjects < MAX_INTLENGTH_SHORT );
	REQUIRES_N( keyIDtype == CRYPT_KEYID_NAME || \
				keyIDtype == CRYPT_KEYID_URI || \
				keyIDtype == CRYPT_IKEYID_KEYID || \
				keyIDtype == CRYPT_IKEYID_PGPKEYID );
	REQUIRES_N( keyIDlength >= MIN_NAME_LENGTH && \
				keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES_N( requestedUsage >= KEYMGMT_FLAG_NONE && \
				requestedUsage < KEYMGMT_FLAG_MAX );
	REQUIRES_N( ( requestedUsage & KEYMGMT_MASK_USAGEOPTIONS ) != \
				KEYMGMT_MASK_USAGEOPTIONS );

	/* Clear return value */
	if( keyInfo != NULL )
		*keyInfo = NULL;

	for( i = 0; i < noPgpObjects && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		if( pgpCheckKeyMatch( &pgpInfo[ i ], &pgpInfo[ i ].key,
							  &keyMatchInfo ) )
			{
			if( keyInfo != NULL )
				*keyInfo = ( PGP_KEYINFO * ) &pgpInfo[ i ].key;
			return( ( PGP_INFO * ) &pgpInfo[ i ] );
			}
		if( pgpCheckKeyMatch( &pgpInfo[ i ], &pgpInfo[ i ].subKey,
							  &keyMatchInfo ) )
			{
			if( keyInfo != NULL )
				*keyInfo = ( PGP_KEYINFO * ) &pgpInfo[ i ].subKey;
			return( ( PGP_INFO * ) &pgpInfo[ i ] );
			}
		}
	ENSURES_N( i < FAILSAFE_ITERATIONS_MED );

	return( NULL );
	}

/****************************************************************************
*																			*
*									Get a Key								*
*																			*
****************************************************************************/

/* Read key data from a PGP keyring */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int getItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							IN_OPT void *auxInfo, 
							INOUT_OPT int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	CRYPT_CONTEXT iDecryptionKey = DUMMY_INIT, iLocalContext;
	PGP_INFO *pgpInfo = ( PGP_INFO * ) keysetInfoPtr->keyData;
	PGP_KEYINFO *keyInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	const int auxInfoMaxLength = *auxInfoLength;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( ( auxInfo == NULL && auxInfoMaxLength == 0 ) || \
			isReadPtr( auxInfo, auxInfoMaxLength ) );
	
	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( auxInfo == NULL && *auxInfoLength == 0 ) || \
			  ( auxInfo != NULL && \
				*auxInfoLength > 0 && \
				*auxInfoLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Find the requested item.  This is complicated somewhat by the fact
	   that private keys are held in memory while public keys (which can
	   be arbitrarily numerous) are held on disk.  This means that the former
	   (and also public keys read from a private-key keyring) are found with 
	   a quick in-memory search while the latter require a scan of the 
	   keyring on disk */
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY || \
		keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE )
		{
		/* Try and locate the appropriate object in the PGP collection */
		pgpInfo = findEntry( keysetInfoPtr->keyData, MAX_PGP_OBJECTS, 
							 keyIDtype, keyID, keyIDlength, flags, 
							 &keyInfo );
		if( pgpInfo == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		}
	else
		{
		CONST_INIT_STRUCT_4( KEY_MATCH_INFO keyMatchInfo, \
							 keyIDtype, keyID, keyIDlength, flags );

		CONST_SET_STRUCT( keyMatchInfo.keyIDtype = keyIDtype; \
						  keyMatchInfo.keyID = keyID; \
						  keyMatchInfo.keyIDlength = keyIDlength; \
						  keyMatchInfo.flags = flags );

		/* Try and find the required key in the keyset */
		sseek( &keysetInfoPtr->keysetFile->stream, 0 );
		status = pgpReadKeyring( &keysetInfoPtr->keysetFile->stream,
								 pgpInfo, 1, &keyMatchInfo, &keyInfo, 
								 KEYSET_ERRINFO );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			return( status );
		}

	/* If it's just a check or label read, we're done */
	if( flags & ( KEYMGMT_FLAG_CHECK_ONLY | KEYMGMT_FLAG_LABEL_ONLY ) )
		{
		if( flags & KEYMGMT_FLAG_LABEL_ONLY )
			{
			const int userIDsize = min( pgpInfo->userIDlen[ 0 ],
										auxInfoMaxLength );

			REQUIRES( pgpInfo->userIDlen[ 0 ] > 0 && \
					  pgpInfo->userIDlen[ 0 ] < MAX_INTLENGTH_SHORT );
			
			*auxInfoLength = userIDsize;
			if( auxInfo != NULL )
				memcpy( auxInfo, pgpInfo->userID[ 0 ], userIDsize );
			}

		return( CRYPT_OK );
		}

	/* Set up the key to decrypt the private-key fields if necessary */
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY )
		{
		/* If no password is supplied let the caller know that they need a
		   password */
		if( auxInfo == NULL )
			{
			retExt( CRYPT_ERROR_WRONGKEY, 
					( CRYPT_ERROR_WRONGKEY, KEYSET_ERRINFO, 
					  "Need a password to decrypt the private key" ) );
			}

		/* If the key is stored as plaintext we can't do anything with it.  
		   This is just a safety check, we never get here anyway, see the 
		   comment in readSecretKeyDecryptionInfo() for details */
		if( keyInfo->cryptAlgo == CRYPT_ALGO_NONE )
			return( CRYPT_ERROR_WRONGKEY );

		/* Create a decryption context to decrypt the private key */
		status = createDecryptionContext( &iDecryptionKey, keyInfo, 
										  auxInfo, auxInfoMaxLength );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, KEYSET_ERRINFO, 
					  "Couldn't create decryption context for private key "
					  "from user password" ) );
			}
		}

	/* Load the key into the encryption context */
	setMessageCreateObjectInfo( &createInfo, keyInfo->pkcAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		if( itemType == KEYMGMT_ITEM_PRIVATEKEY )
			krnlSendNotifier( iDecryptionKey, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	iLocalContext = createInfo.cryptHandle;
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY )
		{
		REQUIRES( pgpInfo->userIDlen[ 0 ] > 0 && \
				  pgpInfo->userIDlen[ 0 ] < MAX_INTLENGTH_SHORT );

		setMessageData( &msgData, pgpInfo->userID[ 0 ],
						min( pgpInfo->userIDlen[ 0 ],
							 CRYPT_MAX_TEXTSIZE ) );
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_LABEL );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyInfo->openPGPkeyID, PGP_KEYID_SIZE );
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyInfo->pubKeyData,
						keyInfo->pubKeyDataLen );
		status = krnlSendMessage( iLocalContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData,
								  ( itemType == KEYMGMT_ITEM_PRIVATEKEY ) ? \
									CRYPT_IATTRIBUTE_KEY_PGP_PARTIAL : \
									CRYPT_IATTRIBUTE_KEY_PGP );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't recreate key from stored %s key data",
				  ( itemType == KEYMGMT_ITEM_PRIVATEKEY ) ? \
					"private" : "public" ) );
		}

	/* If it's a public key, we're done */
	if( itemType != KEYMGMT_ITEM_PRIVATEKEY )
		{
		*iCryptHandle = iLocalContext;

		return( CRYPT_OK );
		}

	/* Import the encrypted key into the PKC context */
	setMechanismWrapInfo( &mechanismInfo, keyInfo->privKeyData,
						  keyInfo->privKeyDataLen, NULL, 0, iLocalContext,
						  iDecryptionKey );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT, 
							  &mechanismInfo, pgpInfo->isOpenPGP ? \
								( keyInfo->hashedChecksum ?
								  MECHANISM_PRIVATEKEYWRAP_OPENPGP : \
								  MECHANISM_PRIVATEKEYWRAP_OPENPGP_OLD ) : \
								MECHANISM_PRIVATEKEYWRAP_PGP2 );
	clearMechanismInfo( &mechanismInfo );
	krnlSendNotifier( iDecryptionKey, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalContext, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't unwrap private key" ) );
		}
	*iCryptHandle = iLocalContext;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Add a Key								*
*																			*
****************************************************************************/

/* Add an item to the PGP keyring */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							IN_HANDLE const CRYPT_HANDLE cryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_BUFFER_OPT( passwordLength ) const char *password, 
							IN_LENGTH_NAME_Z const int passwordLength,
							IN_FLAGS( KEYMGMT ) const int flags )
	{
	PGP_INFO *pgpInfo = ( PGP_INFO * ) keysetInfoPtr->keyData, *pgpInfoPtr;
	MESSAGE_DATA msgData;
	BYTE iD[ CRYPT_MAX_HASHSIZE + 8 ];
	BOOLEAN privkeyPresent;
	char label[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
	int algorithm, iDsize = DUMMY_INIT, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( ( itemType == KEYMGMT_ITEM_PUBLICKEY && \
			  password == NULL && passwordLength == 0 ) || \
			( itemType == KEYMGMT_ITEM_PRIVATEKEY && \
			  isReadPtr( password, passwordLength ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );
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

	/* Check the object and extract ID information from it */
	status = krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
								  &algorithm, CRYPT_CTXINFO_ALGO );
		if( cryptStatusOK( status ) && algorithm != CRYPT_ALGO_RSA )
			{
			/* For now we can only store RSA keys because of the peculiar
			   properties of PGP DLP keys, which are actually two keys
			   with entirely different semantics and attributes but are
			   nevertheless occasionally treated as a single key by PGP */
			status = CRYPT_ARGERROR_NUM1;
			}
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, iD, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		if( cryptStatusOK( status ) )
			iDsize = msgData.length;
		}
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ARGERROR_OBJECT ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}
	privkeyPresent = cryptStatusOK( \
			krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
							 MESSAGE_CHECK_PKC_PRIVATE ) ) ? TRUE : FALSE;

	/* If we're adding a private key make sure that there's a context and a
	   password present.  Conversely if we're adding a public key make sure 
	   that there's no password present.  The password-check has already 
	   been performed by the kernel but we perform a second check here just 
	   to be safe.  The private-key check can't be performed by the kernel 
	   since it doesn't know the difference between public- and private-key 
	   contexts */
	switch( itemType )
		{
		case KEYMGMT_ITEM_PUBLICKEY:
			if( password != NULL )
				return( CRYPT_ARGERROR_STR1 );
			break;

		case KEYMGMT_ITEM_PRIVATEKEY:
			if( !privkeyPresent )
				{
				retExtArg( CRYPT_ARGERROR_NUM1, 
						   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
							 "Item being added doesn't contain a private "
							 "key" ) );
				}
			if( password == NULL )
				return( CRYPT_ARGERROR_STR1 );
			break;
		
		default:
			retIntError();
		}

	/* Find out where we can add data and what needs to be added.  At the 
	   moment we only allow atomic adds since the semantics of PGP's dual 
	   keys, with assorted optional attributes attached to one or both keys 
	   can't easily be handled using a straightforward add */
	pgpInfoPtr = findEntry( keysetInfoPtr->keyData, MAX_PGP_OBJECTS, 
							CRYPT_IKEYID_KEYID, iD,  iDsize, 
							KEYMGMT_FLAG_NONE, NULL );
	if( pgpInfoPtr != NULL )
		{
		retExt( CRYPT_ERROR_DUPLICATE, 
				( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
				  "Item is already present in keyset" ) );
		}

	/* Make sure that the label of what we're adding doesn't duplicate the 
	   label of an existing object */
	if( privkeyPresent )
		{
		setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusError( status ) )
			return( status );
		if( findEntry( keysetInfoPtr->keyData, MAX_PGP_OBJECTS, 
					   CRYPT_KEYID_NAME, msgData.data, msgData.length, 
					   KEYMGMT_FLAG_NONE, NULL ) != NULL )
			{
			retExt( CRYPT_ERROR_DUPLICATE, 
					( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
					  "Item with this label is already present" ) );
			}
		}

	/* Find out where we can add the new key data */
	pgpInfoPtr = findFreeEntry( pgpInfo, MAX_PGP_OBJECTS );
	if( pgpInfoPtr == NULL )
		{
		retExt( CRYPT_ERROR_OVERFLOW, 
				( CRYPT_ERROR_OVERFLOW, KEYSET_ERRINFO, 
				  "No more room in keyset to add this item" ) );
		}

	/* Not implemented yet */
	return( CRYPT_ERROR_NOTAVAIL );
	}

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

/* Shutdown functions */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int shutdownFunction( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	PGP_INFO *pgpInfo = ( PGP_INFO * ) keysetInfoPtr->keyData;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );

	/* If there's no PGP information data cached, we're done */
	if( pgpInfo == NULL )
		return( CRYPT_OK );

	/* Free the cached key information */
	if( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE )
		{
		int i;

		for( i = 0; i < MAX_PGP_OBJECTS; i++ )
			pgpFreeEntry( &pgpInfo[ i ] );
		}
	else
		pgpFreeEntry( pgpInfo );
	clFree( "shutdownFunction", pgpInfo );
	keysetInfoPtr->keyData = NULL;
	keysetInfoPtr->keyDataSize = 0;

	return( CRYPT_OK );
	}

/* PGP public keyrings can be arbitrarily large so we don't try to do any
   preprocessing, all we do at this point is allocate the key information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initPublicFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
							   STDC_UNUSED const char *name,
							   STDC_UNUSED const int nameLength,
							   IN_ENUM( CRYPT_KEYOPT ) \
								const CRYPT_KEYOPT_TYPE options )
	{
	PGP_INFO *pgpInfo;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC );
	REQUIRES( name == NULL && nameLength == 0 );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Allocate memory for the key information */
	if( ( pgpInfo = clAlloc( "initPublicFunction", \
							 sizeof( PGP_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( pgpInfo, 0, sizeof( PGP_INFO ) );
	if( ( pgpInfo->keyData = clAlloc( "initPublicFunction", \
									  KEYRING_BUFSIZE ) ) == NULL )
		{
		clFree( "initPublicFunction", pgpInfo );
		return( CRYPT_ERROR_MEMORY );
		}
	pgpInfo->keyDataLen = KEYRING_BUFSIZE;
	keysetInfoPtr->keyData = pgpInfo;
	keysetInfoPtr->keyDataSize = sizeof( PGP_INFO );

	return( CRYPT_OK );
	}

/* A PGP private keyring can contain multiple keys and whatnot so when we
   open it we scan it and record various pieces of information about it that 
   we can use later when we need to access it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initPrivateFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
								STDC_UNUSED const char *name,
								STDC_UNUSED const int nameLength,
								IN_ENUM( CRYPT_KEYOPT ) \
									const CRYPT_KEYOPT_TYPE options )
	{
	PGP_INFO *pgpInfo;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE );
	REQUIRES( name == NULL && nameLength == 0 );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Allocate the PGP object information */
	if( ( pgpInfo = clAlloc( "initPrivateFunction", \
							 sizeof( PGP_INFO ) * MAX_PGP_OBJECTS ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( pgpInfo, 0, sizeof( PGP_INFO ) * MAX_PGP_OBJECTS );
	keysetInfoPtr->keyData = pgpInfo;
	keysetInfoPtr->keyDataSize = sizeof( PGP_INFO ) * MAX_PGP_OBJECTS;

	/* If this is a newly-created keyset, there's nothing left to do */
	if( options == CRYPT_KEYOPT_CREATE )
		return( CRYPT_OK );

	/* Read all of the keys in the keyring */
	status = pgpReadKeyring( &keysetInfoPtr->keysetFile->stream, pgpInfo, 
							 MAX_PGP_OBJECTS, NULL, NULL, KEYSET_ERRINFO );
	if( status == OK_SPECIAL )
		{
		/* We couldn't process one or more packets, make the keyset read-
		   only to ensure that the incomplete key data isn't written to 
		   disk */
		keysetInfoPtr->options = CRYPT_KEYOPT_READONLY;
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		keysetInfoPtr->shutdownFunction( keysetInfoPtr );
	return( status );
	}

/****************************************************************************
*																			*
*							Keyset Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodPGPPublic( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );

	/* Set the access method pointers */
	keysetInfoPtr->initFunction = initPublicFunction;
	keysetInfoPtr->shutdownFunction = shutdownFunction;
	keysetInfoPtr->getItemFunction = getItemFunction;
	keysetInfoPtr->setItemFunction = setItemFunction;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodPGPPrivate( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );

	/* Set the access method pointers */
	keysetInfoPtr->initFunction = initPrivateFunction;
	keysetInfoPtr->shutdownFunction = shutdownFunction;
	keysetInfoPtr->getItemFunction = getItemFunction;
	keysetInfoPtr->setItemFunction = setItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_PGPKEYS */
