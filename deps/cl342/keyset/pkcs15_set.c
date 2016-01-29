/****************************************************************************
*																			*
*					   cryptlib PKCS #15 Set-item Routines					*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check whether we can add anything to a PKCS #15 personality */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 8, 9 ) ) \
static int checkAddInfo( const PKCS15_INFO *pkcs15infoPtr,
						 IN_HANDLE const CRYPT_HANDLE iCryptHandle,
						 const BOOLEAN isCertChain, 
						 const BOOLEAN privkeyPresent,
						 const BOOLEAN certPresent,
						 const BOOLEAN pkcs15keyPresent,
						 const BOOLEAN pkcs15certPresent,
						 OUT BOOLEAN *isCertUpdate, 
						 INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_DATA msgData;
	BOOLEAN unneededCert, unneededKey;
	int status;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( isCertUpdate, sizeof( BOOLEAN ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( errorInfo != NULL );

	/* Clear return value */
	*isCertUpdate = FALSE;

	/* Check what we can update (if anything) */
	unneededKey = privkeyPresent & pkcs15keyPresent;
	unneededCert = certPresent & pkcs15certPresent;
	if( ( ( unneededCert && !privkeyPresent ) || \
		  ( unneededKey && unneededCert ) ) && \
		pkcs15infoPtr->validTo > MIN_TIME_VALUE )
		{
		time_t validTo;

		/* The certificate would be a duplicate, see if it's more recent 
		   than the existing one.  We only perform this check if there's a 
		   validTo time stored for the certificate since without this 
		   restriction any certificate without a stored time could be 
		   overwritten */
		setMessageData( &msgData, &validTo, sizeof( time_t ) );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_VALIDTO );
		if( cryptStatusOK( status ) && validTo > pkcs15infoPtr->validTo )
			{
			time_t validFrom;

			/* It's a newer certificate, don't treat it as a duplicate.  
			   This check is effectively impossible to perform automatically 
			   since there are an infinite number of variations that have to 
			   be taken into account, for example a certificate for the same 
			   key issued by a different CA, same CA but it's changed the 
			   bits it sets in the keyUsage (digitalSignature vs.
			   nonRepudiation), slightly different issuer DN (Thawte 
			   certificates with a date encoded in the DN), and so on and so 
			   on.  Because this really requires manual processing by a 
			   human we don't even try and sort it all out but just allow a 
			   certificate for a given key (checked by the ID match) to be 
			   replaced by a newer certificate for the same key.  This is 
			   restrictive enough to prevent most obviously-wrong 
			   replacements while being permissive enough to allow most 
			   probably-OK replacements */
			unneededCert = FALSE;
			*isCertUpdate = TRUE;

			/* There's one special-case situation in which odd things can 
			   happen when updating certificates and that's when adding a 
			   future-dated certificate, which would result in the 
			   certificate being replaced with one that can't be used yet.  
			   There's no clean way to handle this because in order to know 
			   what to do we'd have to be able to guess the intent of the 
			   user, however for anything but signature certificates it's 
			   likely that the hit-and-miss certificate checking performed 
			   by most software won't even notice a future-dated 
			   certificate, and for signature certificates the semantics of 
			   signing data now using a certificate that isn't valid yet are 
			   somewhat uncertain.  Since in most cases no-one will even 
			   notice the problem, we throw an exception in the debug build 
			   but don't do anything in release builds.  This is probably 
			   less annoying to users than having the code reject an 
			   otherwise-valid future-dated certificate.  If anyone ever
			   complains about this then we can ask the users at that time
			   what sort of behaviour they're prefer */
			setMessageData( &msgData, &validFrom, sizeof( time_t ) );
			status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_CERTINFO_VALIDFROM );
			if( cryptStatusOK( status ) && \
				validFrom > getApproxTime() + 86400L )
				{
				assert( !"Attempt to replace certificate with future-dated certificate" );
				}
			}
		}

	/* Make sure that we can update at least one of the objects in the PKCS 
	   #15 personality */
	if( ( unneededKey && !certPresent ) ||		/* Key only, duplicate */
		( unneededCert && !privkeyPresent ) ||	/* Certificate only, duplicate */
		( unneededKey && unneededCert ) )		/* Key+certificate, duplicate */
		{
		/* If it's anything other than a certificate chain, we can't add 
		   anything */
		if( !isCertChain )
			{
			retExt( CRYPT_ERROR_DUPLICATE, 
					( CRYPT_ERROR_DUPLICATE, errorInfo, 
					  "No new data to add" ) );
			}

		/* Tell the caller that it's an opportunistic certificate-chain 
		   update */
		return( OK_SPECIAL );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Add a Key								*
*																			*
****************************************************************************/

/* Add an item to the PKCS #15 keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							IN_HANDLE const CRYPT_HANDLE cryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_BUFFER_OPT( passwordLength ) const char *password, 
							IN_LENGTH_NAME_Z const int passwordLength,
							IN_FLAGS( KEYMGMT ) const int flags )
	{
	CRYPT_CERTIFICATE iCryptCert DUMMY_INIT;
	PKCS15_INFO *pkcs15info = keysetInfoPtr->keyData, *pkcs15infoPtr;
	MESSAGE_DATA msgData;
	BYTE iD[ CRYPT_MAX_HASHSIZE + 8 ];
	const BOOLEAN isStorageObject = \
			( keysetInfoPtr->keysetFile->iHardwareDevice != CRYPT_UNUSED ) ? \
			TRUE : FALSE;
	BOOLEAN certPresent = FALSE, privkeyPresent = FALSE;
	BOOLEAN pkcs15certPresent = FALSE, pkcs15keyPresent = FALSE;
	BOOLEAN isCertChain = FALSE, isCertUpdate = FALSE;
	const int noPkcs15objects = keysetInfoPtr->keyDataNoObjects;
	int pkcs15index = CRYPT_ERROR, iDsize, value, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY || \
			  itemType == KEYMGMT_ITEM_SECRETKEY || \
			  itemType == KEYMGMT_ITEM_KEYMETADATA );
	REQUIRES( ( password == NULL && passwordLength == 0 ) || \
			  ( password != NULL && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( ( ( itemType == KEYMGMT_ITEM_PUBLICKEY || \
				  itemType == KEYMGMT_ITEM_KEYMETADATA ) && \
				password == NULL && passwordLength == 0 ) || \
			  ( ( itemType == KEYMGMT_ITEM_PRIVATEKEY || \
				  itemType == KEYMGMT_ITEM_SECRETKEY ) && \
				password != NULL && passwordLength != 0 ) );
	REQUIRES( ( isStorageObject && \
				( itemType == KEYMGMT_ITEM_PUBLICKEY || \
				  itemType == KEYMGMT_ITEM_KEYMETADATA ) ) || \
			   ( !isStorageObject && \
			     itemType != KEYMGMT_ITEM_KEYMETADATA ) );
	REQUIRES( flags == KEYMGMT_FLAG_NONE );

	/* If we're being sent a secret key, add it to the PKCS #15 keyset and 
	   exit */
	if( itemType == KEYMGMT_ITEM_SECRETKEY )
		return( addSecretKey( pkcs15info, noPkcs15objects, cryptHandle ) );

	/* Check the object, extract ID information from it, and determine
	   whether it's a standalone certificate (which produces a PKCS #15 
	   certificate object) or a private-key context (which produces a PKCS 
	   #15 private key object and either a PKCS #15 public-key object (if 
	   there's no certificate present) or a certificate object (if there's 
	   a certificate present)).  If it's a dummy context being used to
	   store key metadata then it won't necessarily be usable for encryption
	   operations so we skip the initial check in this case, the kernel will
	   already have performed the basic type check.

	   Note that we don't allow the addition of standalone public keys
	   (without corresponding private keys) since these keysets are private-
	   key keysets and not general-purpose public key exchange mechanisms.
	   Without this safeguard some users would use them as a general public-
	   key store in place of database keysets or (more rarely) as a type of 
	   unsigned certificate for exchanging public keys.
	   
	   In addition allowing the storage of standalone public keys is rather 
	   problematic since they need to have a label attached in order to be 
	   identified so performing a public-key add with a private-key context 
	   would work but performing one with a public-key context would fail.  
	   A certificate update on this public-key-only item would result in the 
	   presence a private-key-labelled certificate, which is even more 
	   strange for users to comprehend.  To keep things sensible we 
	   therefore disallow the addition of standalone public keys */
	if( itemType != KEYMGMT_ITEM_KEYMETADATA )
		{
		status = krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
								  MESSAGE_CHECK_PKC );
		if( cryptStatusError( status ) )
			return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM1 : status );
		if( cryptStatusOK( \
			krnlSendMessage( cryptHandle, IMESSAGE_CHECK, NULL,
							 MESSAGE_CHECK_PKC_PRIVATE ) ) )
			privkeyPresent = TRUE;
		}
	else
		{
		/* Private-key metadata implicitly has a private key present even
		   if it's not explicitly present in the dummy context */
		privkeyPresent = TRUE;
		}
	setMessageData( &msgData, iD, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusError( status ) )
		return( status );
	iDsize = msgData.length;

	/* If the object being added isn't a generic public key/certificate and 
	   is bound to crypto hardware, make sure that this keyset is PKCS #15 
	   object store */
	if( itemType != KEYMGMT_ITEM_PUBLICKEY )
		{
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, 
								  CRYPT_IATTRIBUTE_DEVICESTORAGEID );
		if( cryptStatusOK( status ) )
			{
			/* A hardware-bound object can't be added to a general-purpose 
			   keyset because there's no way to access the key components 
			   that are required to be stored */
			if( !isStorageObject )
				return( CRYPT_ERROR_PERMISSION );
			}
		else
			{
			if( isStorageObject )
				return( CRYPT_ERROR_PERMISSION );
			}
		}

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
		case KEYMGMT_ITEM_KEYMETADATA:
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

	/* If there's a certificate present make sure that it's something that 
	   can be stored.  We don't treat the wrong type as an error since we 
	   can still store the public/private key components even if we don't 
	   store the certificate */
	if( cryptStatusOK( \
		krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CERTINFO_CERTTYPE ) ) && \
		( value == CRYPT_CERTTYPE_CERTIFICATE || \
		  value == CRYPT_CERTTYPE_CERTCHAIN ) )
		{
		/* If it's a certificate chain, remember this for later since we may
		   need to store multiple certificates */
		if( value == CRYPT_CERTTYPE_CERTCHAIN )
			isCertChain = TRUE;

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
		
		/* Get a reference to the certificate that's associated with the
		   context.  We have to do this in order to lock it so that we can
		   store it since the lock operation is only valid for certificates 
		   and not contexts */
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETDEPENDENT, 
								  &iCryptCert, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		certPresent = TRUE;
		}

	/* Find out where we can add data and what needs to be added.  The 
	   strategy for adding items is:

										Existing
			New		|	None	| Priv+Pub	| Priv+Cert	|	Cert	|
		------------+-----------+-----------+-----------+-----------+
		Priv + Pub	|	Add		|	----	|	----	|	Add		|
					|			|			|			|			|
		Priv + Cert	|	Add		| Repl.pubk	| Add cert	| Add cert	|
					|			| with cert	| if newer	| if newer	|
		Cert		| If trusted|	Add		| Add cert	| Add cert	|
					|			|			| if newer	| if newer	|
		------------+-----------+-----------+-----------+-----------+

	   We don't check for the addition of a trusted certificate at this 
	   point since it could be buried in the middle of a certificate chain 
	   so we leave the checking to addCertChain() */
	pkcs15infoPtr = findEntry( pkcs15info, noPkcs15objects, CRYPT_KEYIDEX_ID,
							   iD, iDsize, KEYMGMT_FLAG_NONE, FALSE );
	if( pkcs15infoPtr != NULL )
		{
		/* Determine what actually needs to be added */
		if( pkcs15infoPtr->privKeyData != NULL )
			pkcs15keyPresent = TRUE;
		if( pkcs15infoPtr->certData != NULL )
			pkcs15certPresent = TRUE;

		/* See what we can add */
		status = checkAddInfo( pkcs15infoPtr, cryptHandle, isCertChain, 
							   privkeyPresent, certPresent, 
							   pkcs15keyPresent, pkcs15certPresent,
							   &isCertUpdate, KEYSET_ERRINFO );
		if( cryptStatusError( status ) )
			{
			/* If it's not an OK_SPECIAL status telling us that we can still 
			   try for an opportunistic certificate chain add, exit */
			if( status != OK_SPECIAL )
				return( status );
			
			/* In theory we can't add anything, however since we've been 
			   given a certificate chain there may be new certificates 
			   present that we can try and add opportunistically */
			status = krnlSendMessage( cryptHandle, IMESSAGE_SETATTRIBUTE,
									  MESSAGE_VALUE_TRUE,
									  CRYPT_IATTRIBUTE_LOCKED );
			if( cryptStatusError( status ) )
				return( status );
			status = pkcs15AddCertChain( pkcs15infoPtr, noPkcs15objects, 
										 cryptHandle, KEYSET_ERRINFO );
			( void ) krnlSendMessage( cryptHandle, IMESSAGE_SETATTRIBUTE,
									  MESSAGE_VALUE_FALSE, 
									  CRYPT_IATTRIBUTE_LOCKED );
			return( status );
			}
		}
	else
		{
		/* This key/certificate isn't already present, make sure that the 
		   label of what we're adding doesn't duplicate the label of an 
		   existing object */
		if( privkeyPresent )
			{
			char label[ CRYPT_MAX_TEXTSIZE + 8 ];
			int labelLength;

			setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE );
			status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_CTXINFO_LABEL );
			if( cryptStatusError( status ) )
				return( status );
			labelLength = msgData.length;
			if( findEntry( pkcs15info, noPkcs15objects, CRYPT_KEYID_NAME,
						   label, labelLength, KEYMGMT_FLAG_NONE, 
						   FALSE ) != NULL )
				{
				retExt( CRYPT_ERROR_DUPLICATE, 
						( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
						  "Item with label '%s' is already present",
						  sanitiseString( label, CRYPT_MAX_TEXTSIZE, 
										  labelLength ) ) );
				}
			}

		/* Find out where we can add the new key data */
		pkcs15infoPtr = findFreeEntry( pkcs15info, noPkcs15objects, 
									   &pkcs15index );
		if( pkcs15infoPtr == NULL )
			{
			retExt( CRYPT_ERROR_OVERFLOW, 
					( CRYPT_ERROR_OVERFLOW, KEYSET_ERRINFO, 
					  "No more room in keyset to add this item" ) );
			}
		}

	/* We're ready to go, lock the object for our exclusive use */
	if( certPresent )
		{
		status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_TRUE,
								  CRYPT_IATTRIBUTE_LOCKED );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Add the key data.  This will add the public/private key and any 
	   certificate data associated with the key as required */
	status = pkcs15AddKey( pkcs15infoPtr, cryptHandle, password, 
						   passwordLength, keysetInfoPtr->ownerHandle, 
						   privkeyPresent, certPresent, 
						   ( isCertUpdate || !pkcs15certPresent ) ? \
								TRUE : FALSE, 
						   pkcs15keyPresent, isStorageObject,
						   KEYSET_ERRINFO );
	if( cryptStatusError( status ) )
		{
		if( certPresent )
			{
			( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
									  MESSAGE_VALUE_FALSE, 
									  CRYPT_IATTRIBUTE_LOCKED );
			}
		return( status );
		}

	/* The update was successful, update the type and index information if 
	   this was a newly-created entry */
	if( pkcs15index != CRYPT_ERROR )
		{
		pkcs15infoPtr->type = PKCS15_SUBTYPE_NORMAL;
		pkcs15infoPtr->index = pkcs15index;
		}

	/* If we've been given a certificate chain, try and opportunistically 
	   add any further certificates that may be present in it.  Error 
	   handling once we get this far gets a bit tricky, we can still get an 
	   error at this point if the certificate chain update fails even if the 
	   main certificate add succeeded, however it's uncertain whether we 
	   should still report an error when the main intended update (of the 
	   private key and public key or certificate) succeeded.  Since the 
	   primary items to be added are the keys and a corresponding 
	   certificate (as handled in addKey()) we don't report an error if 
	   adding one of the coincidental certificates fails, since the primary 
	   items were added successfully */
	if( isCertChain )
		{
		( void ) pkcs15AddCertChain( pkcs15infoPtr, noPkcs15objects, 
									 cryptHandle, KEYSET_ERRINFO );
		}

	/* Clean up */
	if( certPresent )
		{
		( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		}
	return( status );
	}

/* Add special data to the PKCS #15 keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int setSpecialItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								   IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE dataType,
								   IN_BUFFER( dataLength ) const void *data, 
								   IN_LENGTH_SHORT const int dataLength )
	{
	PKCS15_INFO *pkcs15info = keysetInfoPtr->keyData;
	const int noPkcs15objects = keysetInfoPtr->keyDataNoObjects;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( dataType == CRYPT_IATTRIBUTE_CONFIGDATA || \
			  dataType == CRYPT_IATTRIBUTE_USERINDEX || \
			  dataType == CRYPT_IATTRIBUTE_USERID || \
			  dataType == CRYPT_IATTRIBUTE_USERINFO || \
			  dataType == CRYPT_IATTRIBUTE_HWSTORAGE );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT );

	/* Some hardware devices use PKCS #15 as their storage format for 
	   structured data, in which case this is a notification that some rules
	   about what can be added to a PKCS #15 keyset can be relaxed */
	if( dataType == CRYPT_IATTRIBUTE_HWSTORAGE )
		{
		keysetInfoPtr->keysetFile->iHardwareDevice = \
										*( ( CRYPT_HANDLE * ) data );
		return( CRYPT_OK );
		}

	return( addConfigData( pkcs15info, noPkcs15objects, dataType,
						   data, dataLength ) );
	}

/****************************************************************************
*																			*
*									Delete a Key							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int deleteItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							   IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							   IN_BUFFER( keyIDlength ) const void *keyID, 
							   IN_LENGTH_KEYID const int keyIDlength )
	{
	PKCS15_INFO *pkcs15infoPtr;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );
	REQUIRES( itemType > KEYMGMT_ITEM_NONE && \
			  itemType < KEYMGMT_ITEM_LAST );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Locate the appropriate object in the PKCS #15 collection.  Note that
	   we don't allow wildcard deletes, since this is a bit too risky */
	pkcs15infoPtr = findEntry( keysetInfoPtr->keyData, 
							   keysetInfoPtr->keyDataNoObjects, keyIDtype, 
							   keyID, keyIDlength, KEYMGMT_FLAG_NONE,
							   FALSE );
	if( pkcs15infoPtr == NULL )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No information present for this ID" ) );
		}

	/* Clear this entry */
	pkcs15freeEntry( pkcs15infoPtr );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Keyset Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS15set( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 );

	/* Set the access method pointers */
	keysetInfoPtr->setItemFunction = setItemFunction;
	keysetInfoPtr->setSpecialItemFunction = setSpecialItemFunction;
	keysetInfoPtr->deleteItemFunction = deleteItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
