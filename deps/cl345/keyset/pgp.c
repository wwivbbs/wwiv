/****************************************************************************
*																			*
*						  cryptlib PGP Keyset Routines						*
*						Copyright Peter Gutmann 1992-2016					*
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

/* GPG 2.1 broke the standard keyring format, going from the RFC-standardised
   format to storing private keys in an undocumented, homebrew format in a 
   directory "private-keys.v1.d" and public keys in an equally undocumented
   homebrew "keybox" format "pubring.kbx", see
   https://www.gnupg.org/faq/whats-new-in-2.1.html#nosecring and 
   https://www.gnupg.org/faq/whats-new-in-2.1.html#keybox.  This means that
   GPG-sourced keys can only be used if they're first exported in the 
   standard RFC-compliant format */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Sanity-check the PGP information state */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckPGP( const PGP_INFO *pgpInfoPtr )
	{
	assert( isReadPtr( pgpInfoPtr, sizeof( PGP_INFO ) ) );

	/* Check that the basic fields are in order */
	if( !isShortIntegerRange( pgpInfoPtr->keyDataLen ) || \
		pgpInfoPtr->lastUserID < 0 || \
		pgpInfoPtr->lastUserID > MAX_PGP_USERIDS || \
		( pgpInfoPtr->isOpenPGP != FALSE && \
		  pgpInfoPtr->isOpenPGP != TRUE ) || \
		( pgpInfoPtr->isComplete != FALSE && \
		  pgpInfoPtr->isComplete != TRUE ) )
		{
		DEBUG_PUTS(( "sanityCheckPGP: General info" ));
		return( FALSE );
		}
	
	return( TRUE );
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
	assert( isReadPtrDynamic( password, passwordLength ) );

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
								     keyInfo->cryptAlgoParam > 0 ) ? \
									keyInfo->cryptAlgoParam : CRYPT_UNUSED,
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
	assert( isReadPtrDynamic( requiredID, requiredIDlength ) );

	REQUIRES_B( requiredIDlength == PGP_KEYID_SIZE || \
				requiredIDlength == KEYID_SIZE );
	REQUIRES_B( isPGPkeyID == TRUE || isPGPkeyID == FALSE );

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
				!memcmp( requiredID, keyInfo->pgp2KeyID, PGP_KEYID_SIZE ) );
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
	int i, LOOP_ITERATOR;

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

	/* If it's a wildcard match, return the first key */
	if( keyMatchInfo->keyID == NULL )
		{
		ENSURES( keyMatchInfo->keyIDlength == 0 );

		return( TRUE );
		}

	/* We're searching by user ID, walk down the list of userIDs checking
	   for a match */
	LOOP_EXT( i = 0, i < pgpInfo->lastUserID, i++, MAX_PGP_USERIDS + 1 )
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
	ENSURES( LOOP_BOUND_OK );

	return( FALSE );
	}

/* Locate a key based on an ID.  This is complicated somewhat by the fact 
   that PGP groups multiple keys around the same textual ID so we have to 
   check both keys and subkeys for a possible match */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static PGP_INFO *findEntry( const PGP_INFO *pgpInfo,
							IN_LENGTH_SHORT const int noPgpObjects,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID_Z const int keyIDlength,
							IN_FLAGS_Z( KEYMGMT ) const int requestedUsage, 
							OUT_OPT_PTR_COND PGP_KEYINFO **keyInfo )
	{
	CONST_INIT_STRUCT_4( KEY_MATCH_INFO keyMatchInfo, \
						 keyIDtype, keyID, keyIDlength, requestedUsage );
	int i, LOOP_ITERATOR;

	CONST_SET_STRUCT( keyMatchInfo.keyIDtype = keyIDtype; \
					  keyMatchInfo.keyID = keyID; \
					  keyMatchInfo.keyIDlength = keyIDlength; \
					  keyMatchInfo.flags = requestedUsage );

	assert( isReadPtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( ( keyID == NULL && keyIDlength == 0 ) || \
			isReadPtrDynamic( keyID, keyIDlength ) );
	assert( keyInfo == NULL || \
			isWritePtr( keyInfo, sizeof( PGP_KEYINFO * ) ) );

	REQUIRES_N( isShortIntegerRangeNZ( noPgpObjects ) );
	REQUIRES_N( keyIDtype == CRYPT_KEYID_NAME || \
				keyIDtype == CRYPT_KEYID_URI || \
				keyIDtype == CRYPT_IKEYID_KEYID || \
				keyIDtype == CRYPT_IKEYID_PGPKEYID );
	REQUIRES_N( ( keyID == NULL && keyIDlength == 0 ) || \
				( keyID != NULL && \
				  keyIDlength >= MIN_NAME_LENGTH && \
				  keyIDlength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES_N( isFlagRangeZ( requestedUsage, KEYMGMT ) );
	REQUIRES_N( ( requestedUsage & KEYMGMT_MASK_USAGEOPTIONS ) != \
				KEYMGMT_MASK_USAGEOPTIONS );

	/* Clear return value */
	if( keyInfo != NULL )
		*keyInfo = NULL;

	LOOP_MED( i = 0, i < noPgpObjects, i++ )
		{
		const PGP_INFO *pgpInfoPtr = &pgpInfo[ i ];

		/* If there's no entry at this position, continue */
		if( pgpInfoPtr->keyData == NULL )
			continue;

		ENSURES_N( sanityCheckPGP( pgpInfoPtr ) );

		if( pgpCheckKeyMatch( pgpInfoPtr, &pgpInfoPtr->key,
							  &keyMatchInfo ) )
			{
			if( keyInfo != NULL )
				*keyInfo = ( PGP_KEYINFO * ) &pgpInfoPtr->key;
			return( ( PGP_INFO * ) pgpInfoPtr );
			}
		if( pgpCheckKeyMatch( pgpInfoPtr, &pgpInfoPtr->subKey,
							  &keyMatchInfo ) )
			{
			if( keyInfo != NULL )
				*keyInfo = ( PGP_KEYINFO * ) &pgpInfoPtr->subKey;
			return( ( PGP_INFO * ) pgpInfoPtr );
			}
		}
	ENSURES_N( LOOP_BOUND_OK );

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
	CRYPT_CONTEXT iDecryptionKey DUMMY_INIT, iLocalContext;
	CRYPT_KEYID_TYPE localKeyIDtype = keyIDtype;
	PGP_INFO *pgpInfo = DATAPTR_GET( keysetInfoPtr->keyData ), *pgpInfoPtr;
	PGP_KEYINFO *keyInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	BYTE localKeyIDbuffer[ PGP_KEYID_SIZE + 8 ];
	const void *localKeyID = keyID;
	const int auxInfoMaxLength = *auxInfoLength;
	int localKeyIDlength = keyIDlength;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtrDynamic( keyID, keyIDlength ) );
	assert( ( auxInfo == NULL && auxInfoMaxLength == 0 ) || \
			isReadPtrDynamic( auxInfo, auxInfoMaxLength ) );
	
	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
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
				isShortIntegerRangeNZ( *auxInfoLength ) ) );
	REQUIRES( isFlagRangeZ( flags, KEYMGMT ) );
	REQUIRES( pgpInfo != NULL );

	/* Clear return value */
	*iCryptHandle = CRYPT_ERROR;

	/* PGP keys are also identified by hex key IDs alongside standard 
	   identifiers, of the form "0x[16 digits]".  To deal with these we try 
	   and recognise a CRYPT_KEYID_NAME that's actually a keyID and convert 
	   it to a CRYPT_IKEYID_PGPKEYID */
	if( ( keyIDtype == CRYPT_KEYID_NAME ) && \
		( keyIDlength == 2 + ( 2 * PGP_KEYID_SIZE ) ) && \
		!memcmp( keyID, "0x", 2 ) )
		{
		const BYTE *keyIDptr = ( ( BYTE * ) keyID ) + 2;
		int i, LOOP_ITERATOR;

		/* Read the PGP keyID a byte at a time */
		LOOP_EXT( i = 0, i < PGP_KEYID_SIZE, i++, PGP_KEYID_SIZE + 1 )
			{
			int ch;

			status = strGetHex( keyIDptr + ( i * 2 ), 2, &ch, 0x00, 0xFF );
			if( cryptStatusError( status ) )
				{
				retExt( CRYPT_ARGERROR_STR1, 
						( CRYPT_ARGERROR_STR1, KEYSET_ERRINFO, 
						  "Invalid OpenPGP key ID" ) );
				}
			localKeyIDbuffer[ i ] = intToByte( ch );
			}
		ENSURES( LOOP_BOUND_OK );

		/* Mark the updated keyID as a PGP keyID */
		localKeyIDtype = CRYPT_IKEYID_PGPKEYID;
		localKeyID = localKeyIDbuffer;
		localKeyIDlength = PGP_KEYID_SIZE;
		}

	/* If we're matching on the special-case key ID "[none]", perform a 
	   fetch of the first matching key */
	if( keyIDlength == 6 && !strCompare( keyID, "[none]", 6 ) )
		{
		localKeyIDtype = CRYPT_KEYID_NAME;
		localKeyID = NULL;
		localKeyIDlength = 0;
		}

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
		pgpInfoPtr = findEntry( pgpInfo, MAX_PGP_OBJECTS, localKeyIDtype, 
								localKeyID, localKeyIDlength, flags, 
								&keyInfo );
		if( pgpInfoPtr == NULL )
			return( CRYPT_ERROR_NOTFOUND );
		}
	else
		{
		CONST_INIT_STRUCT_4( KEY_MATCH_INFO keyMatchInfo, \
							 localKeyIDtype, localKeyID, localKeyIDlength, \
							 flags );

		CONST_SET_STRUCT( keyMatchInfo.keyIDtype = localKeyIDtype; \
						  keyMatchInfo.keyID = localKeyID; \
						  keyMatchInfo.keyIDlength = localKeyIDlength; \
						  keyMatchInfo.flags = flags );

		/* Try and find the required key in the keyset.  This function works 
		   in a bit of a strange way, unlike pgpReadPrivKeyring() it doesn't
		   select a specific private-key entry from data held in pgpInfo[] 
		   but instead scans through an arbitrary-length stream looking for 
		   a particular matching key, which it returns in pgpInfo[ 0 ].  In
		   addition since the matching key can be any of the collection of
		   physical keys associated with a single logical key, we return
		   information on the particular subkey in keyInfo */
		sseek( &keysetInfoPtr->keysetFile->stream, 0 );
		status = pgpScanPubKeyring( &keysetInfoPtr->keysetFile->stream,
									pgpInfo, &keyMatchInfo, &keyInfo, 
									KEYSET_ERRINFO );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			return( status );
		pgpInfoPtr = pgpInfo;	/* Key data is returned in pgpInfo[ 0 ] */
		}

	/* If it's just a check or label read, we're done */
	if( flags & ( KEYMGMT_FLAG_CHECK_ONLY | KEYMGMT_FLAG_LABEL_ONLY ) )
		{
		if( flags & KEYMGMT_FLAG_LABEL_ONLY )
			{
			const int userIDsize = min( pgpInfoPtr->userIDlen[ 0 ],
										auxInfoMaxLength );

			REQUIRES( isShortIntegerRangeNZ( pgpInfoPtr->userIDlen[ 0 ] ) );
			
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
		REQUIRES( isShortIntegerRangeNZ( pgpInfoPtr->userIDlen[ 0 ] ) );

		setMessageData( &msgData, pgpInfoPtr->userID[ 0 ],
						min( pgpInfoPtr->userIDlen[ 0 ],
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
							  &mechanismInfo, pgpInfoPtr->isOpenPGP ? \
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
	const PGP_INFO *pgpInfo = DATAPTR_GET( keysetInfoPtr->keyData );
	PGP_INFO *pgpInfoPtr;
	MESSAGE_DATA msgData;
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];
	BOOLEAN encryptionOnlyKey = FALSE, privkeyPresent;
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];
	int algorithm, keyIDsize DUMMY_INIT, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( ( itemType == KEYMGMT_ITEM_PUBLICKEY && \
			  password == NULL && passwordLength == 0 ) || \
			( itemType == KEYMGMT_ITEM_PRIVATEKEY && \
			  isReadPtrDynamic( password, passwordLength ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC );
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
	REQUIRES( pgpInfo != NULL );

	/* Check the object and extract ID information from it */
	status = krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_PKC );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, keyID, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID );
		if( cryptStatusOK( status ) )
			keyIDsize = msgData.length;
		}
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ARGERROR_OBJECT ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}
	if( findEntry( pgpInfo, 1, CRYPT_IKEYID_KEYID, keyID, keyIDsize, 
				   KEYMGMT_FLAG_NONE, NULL ) != NULL )
		{
		retExt( CRYPT_ERROR_DUPLICATE, 
				( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
				  "Item is already present in keyset" ) );
		}

	/* Find out what sort of key we're trying to store */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		switch( algorithm )
			{
			case CRYPT_ALGO_ELGAMAL:
			case CRYPT_ALGO_ECDH:
				/* If it's an encryption-only algorithm then we can only 
				   store the key data but can't sign metadata */
				encryptionOnlyKey = TRUE;
				break;

			case CRYPT_ALGO_RSA:
			case CRYPT_ALGO_DSA:
			case CRYPT_ALGO_ECDSA:
				break;

			default:
				status = CRYPT_ARGERROR_NUM1;
			}
		}
	if( cryptStatusError( status ) )
		return( status );
	privkeyPresent = checkContextCapability( cryptHandle, 
											 MESSAGE_CHECK_PKC_PRIVATE );

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

	/* Make sure that the label of what we're adding doesn't duplicate the 
	   label of an existing object */
	if( privkeyPresent )
		{
		int labelLength;

		setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusError( status ) )
			return( status );
		labelLength = msgData.length;
		if( findEntry( pgpInfo, 1, CRYPT_KEYID_NAME, label, labelLength, 
					   KEYMGMT_FLAG_NONE, NULL ) != NULL )
			{
			retExt( CRYPT_ERROR_DUPLICATE, 
					( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
					  "Item with label '%s' is already present",
					  sanitiseString( label, CRYPT_MAX_TEXTSIZE, 
									  labelLength ) ) );
			}
		}

	/* Storing PGP private keys is quite complicated and there's no good 
	   reason to use this format instead of PKCS #15, so for now we don't
	   implement it */
	if( itemType == KEYMGMT_ITEM_PRIVATEKEY )
		{
		retExt( CRYPT_ERROR_NOTAVAIL, 
				( CRYPT_ERROR_NOTAVAIL, KEYSET_ERRINFO, 
				  "Storing private keys in PGP format isn't supported" ) );
		}

	/* Find out where we can add data and what needs to be added.  This is 
	   quite problematic because of the way that PGP creates a single 
	   logical key out of multiple physical keys, unless two keys being 
	   added have the same label it's not possible to tell whether they're 
	   meant to be part of the same logical key or not.
	   
	   To deal with this we only allow a single logical key (comprising one 
	   or more physical keys) to be added, so instead of using findEntry() 
	   to locate a possible match we always choose the first entry */
#if 0
	pgpInfoPtr = findEntry( pgpInfo, 1, ... );
	if( pgpInfoPtr == NULL )
		{
		pgpInfoPtr = findFreeEntry( pgpInfo, 1 );
		if( pgpInfoPtr == NULL )
			{
			retExt( CRYPT_ERROR_OVERFLOW, 
					( CRYPT_ERROR_OVERFLOW, KEYSET_ERRINFO, 
					  "No more room in keyset to add this item" ) );
			}
		}
	else
		...
#endif /* 0 */
	pgpInfoPtr = DATAPTR_GET( keysetInfoPtr->keyData );	/* Pointer to first entry */
	ENSURES( pgpInfoPtr != NULL );
	if( pgpInfoPtr->isComplete )
		{
		retExt( CRYPT_ERROR_COMPLETE, 
				( CRYPT_ERROR_COMPLETE, KEYSET_ERRINFO, 
				  "No further keys can be added for this entry" ) );
		}
	if( encryptionOnlyKey && pgpInfoPtr->keyData != NULL )
		{
		retExt( CRYPT_ERROR_DUPLICATE, 
				( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
				  "This entry already contains an encryption key" ) );
		}

	/* If it's an encryption-only key then we need to save the key data away 
	    for later use, where it'll be signed using a signature key */
	if( encryptionOnlyKey )
		{
		void *keyData;
		int keyDataSize;

		/* Allocate storage for the key data, write the data to the storage, 
		   and remember it for later */
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEY_PGP );
		if( cryptStatusError( status ) )
			return( status );
		keyDataSize = msgData.length;
		REQUIRES( rangeCheck( keyDataSize, 1, MAX_INTLENGTH_SHORT ) );
		if( ( keyData = clAlloc( "setItemFunction", keyDataSize ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		setMessageData( &msgData, keyData, keyDataSize );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEY_PGP );
		if( cryptStatusError( status ) )
			{
			clFree( "setItemFunction", keyData ); 
			return( status );
			}
		pgpInfoPtr->keyData = keyData;
		pgpInfoPtr->keyDataLen = keyDataSize;

		return( CRYPT_OK );
		}

	/* In order to write the key data we need to be able to bind metadata to 
	   the main key using signatures, so we need to have been passed a 
	   private key in order to generate the signatures */
	if( !privkeyPresent )
		{
		retExt( CRYPT_ARGERROR_NUM1, 
				( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
				  "Key must be a private key in order to sign public "
				  "keyring data" ) );
		}

	/* Write the key data and associated metadata in PGP keyring format */
	status = pgpWritePubkey( pgpInfoPtr, cryptHandle );
	if( cryptStatusOK( status ) )
		pgpInfoPtr->isComplete = TRUE;

	return( status );
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
	PGP_INFO *pgpInfo;
	int status = CRYPT_OK;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || 
				keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) );

	/* If there's no PGP key information present, we're done */
	if( DATAPTR_ISNULL( keysetInfoPtr->keyData ) )
		return( CRYPT_OK );

	pgpInfo = DATAPTR_GET( keysetInfoPtr->keyData );
	ENSURES( pgpInfo != NULL );

	/* If the contents have been changed, commit the changes to disk */
	if( TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY ) )
		{
		STREAM *stream = &keysetInfoPtr->keysetFile->stream;
		BYTE buffer[ SAFEBUFFER_SIZE( STREAM_BUFSIZE ) + 8 ] STACK_ALIGN_DATA;

		sseek( stream, 0 );
		memset( buffer, 0, STREAM_BUFSIZE );
				/* Keep static analysers happy */
		safeBufferInit( SAFEBUFFER_PTR( buffer ), STREAM_BUFSIZE );
		sioctlSetString( stream, STREAM_IOCTL_IOBUFFER, 
						 SAFEBUFFER_PTR( buffer ), STREAM_BUFSIZE );
		status = swrite( stream, pgpInfo->keyData, pgpInfo->keyDataLen );
		if( cryptStatusOK( status ) )
			status = sflush( stream );
		sioctlSet( stream, STREAM_IOCTL_IOBUFFER, 0 );
		}

	/* Free the cached key information */
	if( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE )
		{
		int i, LOOP_ITERATOR;

		LOOP_EXT( i = 0, i < MAX_PGP_OBJECTS, i++, MAX_PGP_OBJECTS + 1 )
			pgpFreeEntry( &pgpInfo[ i ] );
		ENSURES( LOOP_BOUND_OK );
		}
	else
		pgpFreeEntry( pgpInfo );
	clFree( "shutdownFunction", pgpInfo );
	DATAPTR_SET( keysetInfoPtr->keyData, NULL );
	keysetInfoPtr->keyDataSize = 0;

	return( status );
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

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC );
	REQUIRES( name == NULL && nameLength == 0 );
	REQUIRES( options == CRYPT_KEYOPT_NONE || \
			  options == CRYPT_KEYOPT_CREATE );

	/* Allocate memory for the key information.  Since we're just scanning 
	   the keyring for a single matching key or writing a single key, we 
	   only need to allocate room for one entry.  If we're reading the 
	   keyring we also need to allocate a read buffer */
	if( ( pgpInfo = clAlloc( "initPublicFunction", \
							 sizeof( PGP_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( pgpInfo, 0, sizeof( PGP_INFO ) );
	if( options != CRYPT_KEYOPT_CREATE )
		{
		if( ( pgpInfo->keyData = clAlloc( "initPublicFunction", \
										  KEYRING_BUFSIZE ) ) == NULL )
			{
			clFree( "initPublicFunction", pgpInfo );
			return( CRYPT_ERROR_MEMORY );
			}
		pgpInfo->keyDataLen = KEYRING_BUFSIZE;
		}
	DATAPTR_SET( keysetInfoPtr->keyData, pgpInfo );
	keysetInfoPtr->keyDataSize = sizeof( PGP_INFO );

	ENSURES( sanityCheckKeyset( keysetInfoPtr ) );

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

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE );
	REQUIRES( name == NULL && nameLength == 0 );
	REQUIRES( options == CRYPT_KEYOPT_NONE || \
			  options == CRYPT_KEYOPT_CREATE );

	/* Allocate the PGP object information */
	if( ( pgpInfo = clAlloc( "initPrivateFunction", \
							 sizeof( PGP_INFO ) * MAX_PGP_OBJECTS ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( pgpInfo, 0, sizeof( PGP_INFO ) * MAX_PGP_OBJECTS );
	DATAPTR_SET( keysetInfoPtr->keyData, pgpInfo );
	keysetInfoPtr->keyDataSize = sizeof( PGP_INFO ) * MAX_PGP_OBJECTS;

	/* If this is a newly-created keyset, there's nothing left to do */
	if( options == CRYPT_KEYOPT_CREATE )
		return( CRYPT_OK );

	/* Read all of the keys in the keyring */
	status = pgpReadPrivKeyring( &keysetInfoPtr->keysetFile->stream, 
								 pgpInfo, MAX_PGP_OBJECTS, KEYSET_ERRINFO );
	if( status == OK_SPECIAL )
		{
		/* We couldn't process one or more packets, make the keyset read-
		   only to ensure that the incomplete key data isn't written to 
		   disk */
		SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_READONLY );
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		shutdownFunction( keysetInfoPtr );
		return( status );
		}

	ENSURES( sanityCheckKeyset( keysetInfoPtr ) );

	return( CRYPT_OK );
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
	FNPTR_SET( keysetInfoPtr->initFunction, initPublicFunction );
	FNPTR_SET( keysetInfoPtr->shutdownFunction, shutdownFunction );
	FNPTR_SET( keysetInfoPtr->getItemFunction, getItemFunction );
	FNPTR_SET( keysetInfoPtr->setItemFunction, setItemFunction );

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
	FNPTR_SET( keysetInfoPtr->initFunction, initPrivateFunction );
	FNPTR_SET( keysetInfoPtr->shutdownFunction, shutdownFunction );
	FNPTR_SET( keysetInfoPtr->getItemFunction, getItemFunction );
	FNPTR_SET( keysetInfoPtr->setItemFunction, setItemFunction );

	return( CRYPT_OK );
	}
#endif /* USE_PGPKEYS */
