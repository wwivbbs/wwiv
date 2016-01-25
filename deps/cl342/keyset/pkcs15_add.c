/****************************************************************************
*																			*
*						cryptlib PKCS #15 Key Add Interface					*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Determine the tag to use when encoding a given key type.  There isn't any
   tag for Elgamal but the keys are the same as X9.42 DH keys and cryptlib
   uses the OID rather than the tag to determine the key type so the
   following sleight-of-hand works */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getKeyTypeTag( IN_HANDLE_OPT const CRYPT_CONTEXT cryptContext,
				   IN_ALGO_OPT const CRYPT_ALGO_TYPE cryptAlgo,
				   OUT int *tag )
	{
	static const MAP_TABLE tagMapTbl[] = {
		{ CRYPT_ALGO_RSA, 100 },
		{ CRYPT_ALGO_DH, CTAG_PK_DH },
		{ CRYPT_ALGO_ELGAMAL, CTAG_PK_DH },
		{ CRYPT_ALGO_DSA, CTAG_PK_DSA },
		{ CRYPT_ALGO_ECDSA, CTAG_PK_ECC },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	int keyCryptAlgo, value, status;

	REQUIRES( ( isHandleRangeValid( cryptContext ) && \
				cryptAlgo == CRYPT_ALGO_NONE ) || \
			  ( cryptContext == CRYPT_UNUSED && \
				isPkcAlgo( cryptAlgo ) ) );

	/* Clear return value */
	*tag = 0;

	/* If the caller hasn't already supplied the algorithm details, get them
	   from the context */
	if( cryptAlgo != CRYPT_ALGO_NONE )
		keyCryptAlgo = cryptAlgo;
	else
		{
		status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE,
								  &keyCryptAlgo, CRYPT_CTXINFO_ALGO );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Map the algorithm to the corresponding tag.  We have to be a bit 
	   careful with the tags because the out-of-band special-case value 
	   DEFAULT_TAG looks like an error value, so we supply a dummy value
	   of '100' for this tag and map it back to DEFAULT_TAG when we return
	   it to the caller */
	status = mapValue( keyCryptAlgo, &value, tagMapTbl, 
					   FAILSAFE_ARRAYSIZE( tagMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	*tag = ( value == 100 ) ? DEFAULT_TAG : value;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Add Miscellaneous Items							*
*																			*
****************************************************************************/

/* Add configuration data to a PKCS #15 collection.  The different data
   types are:

	IATTRIBUTE_USERID: ID for objects in user keysets.  All items in the 
		keyset (which will be the user object's private key and their 
		configuration information) are given this value as their ID.
	
	IATTRIBUTE_CONFIGDATA: ASN.1-encoded cryptlib configuration options.

	IATTRIBUTE_USERINDEX: ASN.1-encoded table mapping userIDs and names to 
		a unique index value that's used to locate the file or storage 
		location for that user's configuration data.

	IATTRIBUTE_USERINFO: ASN.1-encoded user information containing their 
		role, ID, name information, and any additional required information.

   The lookup process for a given user's information is to read the 
   IATTRIBUTE_USERINDEX from the user index keyset (typically index.p15) to 
   find the user's index value, and then use that to read the 
   IATTRIBUTE_USERINFO from the user keyset (typically u<index>.p15).  The 
   cryptlib-wide IATTRIBUTE_CONFIGDATA is stored in the cryptlib default 
   initialisation keyset, typically cryptlib.p15.

   If we're being sent empty data (corresponding to an empty SEQUENCE, so 
   dataLength < 8), it means that the caller wants to clear this entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int addConfigData( IN_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
				   IN_LENGTH_SHORT const int noPkcs15objects, 
				   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE dataType,
				   IN_BUFFER( dataLength ) const char *data, 
				   IN_LENGTH_SHORT const int dataLength )
	{
	PKCS15_INFO *pkcs15infoPtr = NULL;
	const BOOLEAN isDataClear = ( dataLength < 8 ) ? TRUE : FALSE;
	void *newData;
	int i;

	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( noPkcs15objects >= 1 && \
			  noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( dataType == CRYPT_IATTRIBUTE_CONFIGDATA || \
			  dataType == CRYPT_IATTRIBUTE_USERINDEX || \
			  dataType == CRYPT_IATTRIBUTE_USERID || \
			  dataType == CRYPT_IATTRIBUTE_USERINFO );
	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH_SHORT );

	/* If it's a user ID, set all object IDs to this value.  This is needed
	   for user keysets where there usually isn't any key ID present (there
	   is one for SO keysets that have public/private keys attached to them 
	   but they're not identified by key ID so it's not much use).  In this 
	   case the caller has to explicitly set an ID, which is the user ID */
	if( dataType == CRYPT_IATTRIBUTE_USERID )
		{
		const int length = min( dataLength, CRYPT_MAX_HASHSIZE );

		REQUIRES( dataLength == KEYID_SIZE );

		for( i = 0; i < noPkcs15objects && i < FAILSAFE_ITERATIONS_MED; i++ )
			{
			memcpy( pkcs15info[ i ].iD, data, length );
  			pkcs15info[ i ].iDlength = length;
			}
		ENSURES( i < FAILSAFE_ITERATIONS_MED );
		return( CRYPT_OK );
		}

	/* Find an entry that contains data identical to what we're adding now 
	   (which we'll replace with the new data) or failing that, the first 
	   free entry */
	for( i = 0; i < noPkcs15objects && i < FAILSAFE_ITERATIONS_MED; i++ )
		{
		if( pkcs15info[ i ].type == PKCS15_SUBTYPE_DATA && \
			pkcs15info[ i ].dataType == dataType )
			{
			pkcs15infoPtr = &pkcs15info[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MED );
	if( pkcs15infoPtr == NULL )
		{
		/* If we're trying to delete an existing entry then not finding what
		   we want to delete is an error */
		ENSURES( !isDataClear );

		/* We couldn't find an existing entry to update, add a new entry */
		pkcs15infoPtr = findFreeEntry( pkcs15info, noPkcs15objects, NULL );
		}
	if( pkcs15infoPtr == NULL )
		{
		/* The appropriate error value to return here is a 
		   CRYPT_ERROR_OVERFLOW because we always try to add a new entry if
		   we can't find an existing one, so the final error status is 
		   always an overflow */
		return( CRYPT_ERROR_OVERFLOW );
		}

	/* If we're clearing an existing entry, we're done */
	if( isDataClear )
		{
		pkcs15freeEntry( pkcs15infoPtr );
		return( CRYPT_OK );
		}

	/* If we're adding new data and there's no existing storage available, 
	   allocate storage for it */
	if( pkcs15infoPtr->dataData == NULL || \
		dataLength > pkcs15infoPtr->dataDataSize )
		{
		newData = clAlloc( "addConfigData", dataLength );
		if( newData == NULL )
			return( CRYPT_ERROR_MEMORY );

		/* If there's existing data present, clear and free it */
		if( pkcs15infoPtr->dataData != NULL )
			{
			zeroise( pkcs15infoPtr->dataData, pkcs15infoPtr->dataDataSize );
			clFree( "addConfigData", pkcs15infoPtr->dataData );
			}
		}
	else
		{
		/* There's existing data present and the new data will fit into its
		   storage, re-use the existing storage */
		newData = pkcs15infoPtr->dataData;
		}

	/* Remember the pre-encoded configuration data */
	pkcs15infoPtr->dataData = newData;
	memcpy( pkcs15infoPtr->dataData, data, dataLength );
	pkcs15infoPtr->dataDataSize = dataLength;

	/* Set the type information for the data */
	pkcs15infoPtr->type = PKCS15_SUBTYPE_DATA;
	pkcs15infoPtr->dataType = dataType;

	return( CRYPT_OK );
	}

/* Add a secret key to a PKCS #15 collection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addSecretKey( IN_ARRAY( noPkcs15objects ) PKCS15_INFO *pkcs15info, 
				  IN_LENGTH_SHORT const int noPkcs15objects,
				  IN_HANDLE const CRYPT_HANDLE iCryptContext )
	{
	PKCS15_INFO *pkcs15infoPtr = NULL;
	MESSAGE_DATA msgData;
	char label[ CRYPT_MAX_TEXTSIZE + 8 ];
	int status;

	assert( isWritePtr( pkcs15infoPtr, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );

	REQUIRES( noPkcs15objects >= 1 && \
			  noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Check the object and make sure that the label of what we're adding
	   doesn't duplicate the label of an existing object */
	status = krnlSendMessage( iCryptContext, IMESSAGE_CHECK, NULL,
							  MESSAGE_CHECK_CRYPT );
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ARGERROR_OBJECT ) ? \
				CRYPT_ARGERROR_NUM1 : status );
		}
	setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		return( status );
	if( findEntry( pkcs15info, noPkcs15objects, CRYPT_KEYID_NAME, 
				   msgData.data, msgData.length, 
				   KEYMGMT_FLAG_NONE, FALSE ) != NULL )
		return( CRYPT_ERROR_DUPLICATE );

	/* Find out where we can add the new key data */
	pkcs15infoPtr = findFreeEntry( pkcs15info, noPkcs15objects, NULL );
	if( pkcs15infoPtr == NULL )
		return( CRYPT_ERROR_OVERFLOW );

	pkcs15infoPtr->type = PKCS15_SUBTYPE_SECRETKEY;

	/* This functionality is currently unused */
	retIntError();
	}

/****************************************************************************
*																			*
*							External Add-a-Key Interface					*
*																			*
****************************************************************************/

/* Add a key to a PKCS #15 collection.  The strategy for adding items is:

										Existing
		New		|	None	| Priv+Pub	| Priv+Cert	|	Cert	|
	------------+-----------+-----------+-----------+-----------+
	Priv + Pub	|	Add		|	----	|	----	|	Add		|
				|			|			|			|			|
	Priv + Cert	|	Add		| Repl.pubk	| Add cert	| Add cert	|
				|			| with cert	| if newer	| if newer	|
	Cert		| If trusted|	Add		| Add cert	| Add cert	|
				|			|			| if newer	| if newer	|
	------------+-----------+-----------+-----------+-----------+ */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 11 ) ) \
int pkcs15AddKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
				  IN_HANDLE const CRYPT_HANDLE iCryptHandle,
				  IN_BUFFER_OPT( passwordLength ) const void *password, 
				  IN_LENGTH_NAME_Z const int passwordLength,
				  IN_HANDLE const CRYPT_USER iOwnerHandle, 
				  const BOOLEAN privkeyPresent, const BOOLEAN certPresent, 
				  const BOOLEAN doAddCert, const BOOLEAN pkcs15keyPresent,
				  const BOOLEAN isStorageObject, 
				  INOUT ERROR_INFO *errorInfo )
	{
	BYTE pubKeyAttributes[ KEYATTR_BUFFER_SIZE + 8 ];
	BYTE privKeyAttributes[ KEYATTR_BUFFER_SIZE + 8 ];
	int pubKeyAttributeSize = 0, privKeyAttributeSize = 0;
	int modulusSize = DUMMY_INIT, pkcCryptAlgo, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( ( privkeyPresent && isReadPtr( password, passwordLength ) ) || \
			( ( !privkeyPresent || isStorageObject ) && \
			  password == NULL && passwordLength == 0 ) );

	REQUIRES( ( privkeyPresent && password != NULL && \
				passwordLength >= MIN_NAME_LENGTH && \
				passwordLength < MAX_ATTRIBUTE_SIZE ) || \
			  ( ( !privkeyPresent || isStorageObject ) && \
				password == NULL && passwordLength == 0 ) );
	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( iOwnerHandle == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iOwnerHandle ) );
	REQUIRES( errorInfo != NULL );

	/* Get information from the context */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE, 
							  &pkcCryptAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE, 
								  &modulusSize, CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the attribute information.  We have to rewrite the key
	   information when we add a non-standalone certificate even if we don't 
	   change the key because adding a certificate can affect key 
	   attributes */
	if( ( certPresent && pkcs15keyPresent ) ||		/* Updating existing */
		( privkeyPresent && !pkcs15keyPresent ) )	/* Adding new */
		{
		status = writeKeyAttributes( privKeyAttributes, KEYATTR_BUFFER_SIZE,
									 &privKeyAttributeSize,
									 pubKeyAttributes, KEYATTR_BUFFER_SIZE,
									 &pubKeyAttributeSize, pkcs15infoPtr,
									 iCryptHandle );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't write PKCS #15 key attributes" ) );
			}
		}

	/* Write the certificate if necessary.  We do this one first because 
	   it's the easiest to back out of */
	if( certPresent && doAddCert )
		{
		/* Select the leaf certificate in case it's a certificate chain */
		status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_CURSORFIRST,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );

		/* Write the certificate information.  There may be further 
		   certificates in the chain but we don't try and do anything with 
		   these at this level, the addition of supplemental certificates is 
		   handled by the caller */
		if( pkcs15keyPresent )
			{
			status = pkcs15AddCert( pkcs15infoPtr, iCryptHandle, 
									privKeyAttributes, privKeyAttributeSize, 
									CERTADD_UPDATE_EXISTING, errorInfo );
			}
		else
			{
			status = pkcs15AddCert( pkcs15infoPtr, iCryptHandle, NULL, 0,
									( privkeyPresent || isStorageObject ) ? \
										CERTADD_NORMAL : \
										CERTADD_STANDALONE_CERT, errorInfo );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* If there's no public/private-key context to add, exit */
		if( !privkeyPresent || pkcs15keyPresent )
			return( CRYPT_OK );
		}

	/* Add the public key information if the information hasn't already been 
	   added via a certificate */
	if( !certPresent )
		{
		ENSURES( privkeyPresent && !pkcs15keyPresent );

		status = pkcs15AddPublicKey( pkcs15infoPtr, iCryptHandle, 
									 pubKeyAttributes, pubKeyAttributeSize, 
									 pkcCryptAlgo, modulusSize, isStorageObject,
									 errorInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Add the private key information */
	return( pkcs15AddPrivateKey( pkcs15infoPtr, iCryptHandle, iOwnerHandle,
								 password, passwordLength, privKeyAttributes,
								 privKeyAttributeSize, pkcCryptAlgo,
								 modulusSize, isStorageObject, errorInfo ) );
	}
#endif /* USE_PKCS15 */
