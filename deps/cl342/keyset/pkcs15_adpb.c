/****************************************************************************
*																			*
*					cryptlib PKCS #15 Public-key Add Interface				*
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

/* Define the following to use the corrected PKCS #15 v1.2 form for 
   ObjectValue.direct tagging rather than the original erroneous v1.1
   form.  Note that this will break backwards compatibility for cryptlib 
   versions before 3.4.0, however 3.4.0 also introduces AuthEncData so this 
   seems like a good time to make the changeover for the tagging as well */

#define USE_PKCS15V12_FORM

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Calculate the size of and if necessary allocate storage for public-key 
   and certificate data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int calculatePubkeyStorage( const PKCS15_INFO *pkcs15infoPtr,
								   OUT_PTR void **newPubKeyDataPtr, 
								   OUT_LENGTH_SHORT_Z int *newPubKeyDataSize, 
								   IN_LENGTH_SHORT const int pubKeySize,
								   IN_LENGTH_SHORT const int pubKeyAttributeSize,
								   IN_LENGTH_SHORT const int extraDataSize )
	{
	void *newPubKeyData;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( newPubKeyDataPtr, sizeof( void * ) ) );
	assert( isWritePtr( newPubKeyDataSize, sizeof( int ) ) ); 

	REQUIRES( pubKeySize > 0 && pubKeySize < MAX_INTLENGTH_SHORT );
	REQUIRES( pubKeyAttributeSize > 0 && \
			  pubKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( extraDataSize >= 0 && extraDataSize < MAX_INTLENGTH_SHORT );

	/* Calculate the new private-key data size */
	*newPubKeyDataSize = sizeofObject( \
							pubKeyAttributeSize + \
							sizeofObject( \
								sizeofObject( \
									sizeofObject( pubKeySize ) + \
									extraDataSize ) ) );
	ENSURES( *newPubKeyDataSize > 0 && *newPubKeyDataSize < MAX_INTLENGTH );

	/* If the new data will fit into the existing storage, we're done */
	if( *newPubKeyDataSize <= pkcs15infoPtr->pubKeyDataSize )
		return( CRYPT_OK );

	/* Allocate storage for the new data */
	newPubKeyData = clAlloc( "calculatePubkeyStorage", *newPubKeyDataSize );
	if( newPubKeyData == NULL )
		return( CRYPT_ERROR_MEMORY );
	*newPubKeyDataPtr = newPubKeyData;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int calculateCertStorage( const PKCS15_INFO *pkcs15infoPtr,
								 OUT_PTR void **newCertDataPtr, 
								 OUT_LENGTH_SHORT_Z int *newCertDataSize,
								 IN_LENGTH_SHORT const int certAttributeSize,
								 IN_LENGTH_SHORT const int certSize )
	{
	void *newCertData;

	assert( isReadPtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( newCertDataPtr, sizeof( void * ) ) );
	assert( isWritePtr( newCertDataSize, sizeof( int ) ) ); 

	REQUIRES( certAttributeSize > 0 && \
			  certAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( certSize > 0 && certSize < MAX_INTLENGTH_SHORT );

	/* Calculate the new certificate data size */
#ifdef USE_PKCS15V12_FORM
	*newCertDataSize = sizeofObject( certAttributeSize + \
									 sizeofObject( \
									   sizeofObject( \
										 sizeofObject( certSize ) ) ) );
#else
	*newCertDataSize = sizeofObject( certAttributeSize + \
									 sizeofObject( \
									   sizeofObject( certSize ) ) );
#endif /* USE_PKCS15V12_FORM */
	ENSURES( *newCertDataSize > 0 && *newCertDataSize < MAX_INTLENGTH );

	/* If the new data will fit into the existing storage, we're done */
	if( *newCertDataSize <= pkcs15infoPtr->certDataSize )
		return( CRYPT_OK );

	/* Allocate storage for the new data */
	newCertData = clAlloc( "calculateCertStorage", *newCertDataSize );
	if( newCertData == NULL )
		return( CRYPT_ERROR_MEMORY );
	*newCertDataPtr = newCertData;

	return( CRYPT_OK );
	}

/* Delete the public-key entry for a personality, used when we're replacing
   the pubkey with a certificate */

STDC_NONNULL_ARG( ( 1 ) ) \
static void deletePubKey( INOUT PKCS15_INFO *pkcs15infoPtr )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );

	zeroise( pkcs15infoPtr->pubKeyData, pkcs15infoPtr->pubKeyDataSize );
	clFree( "deletePubKey", pkcs15infoPtr->pubKeyData );
	pkcs15infoPtr->pubKeyData = NULL;
	pkcs15infoPtr->pubKeyDataSize = 0;
	}

/* Replace existing public-key or certificate data with updated 
   information */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void replacePubkeyData( INOUT PKCS15_INFO *pkcs15infoPtr, 
							   IN_BUFFER( newPubKeyDataSize ) \
								const void *newPubKeyData, 
							   IN_LENGTH_SHORT_MIN( 16 ) \
								const int newPubKeyDataSize,
							   IN_LENGTH_SHORT const int newPubKeyOffset )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( newPubKeyData, newPubKeyDataSize ) );

	REQUIRES_V( newPubKeyDataSize >= 16 && \
				newPubKeyDataSize < MAX_INTLENGTH_SHORT );
	REQUIRES_V( newPubKeyOffset > 0 && \
				newPubKeyOffset < newPubKeyDataSize && \
				newPubKeyOffset < MAX_INTLENGTH_SHORT );

	/* If we've allocated new storage for the data rather than directly 
	   replacing the existing entry, free the existing entry and replace it 
	   with the new one */
	if( newPubKeyData != pkcs15infoPtr->pubKeyData )
		{
		if( pkcs15infoPtr->pubKeyData != NULL )
			{
			zeroise( pkcs15infoPtr->pubKeyData, 
					 pkcs15infoPtr->pubKeyDataSize );
			clFree( "replacePubkeyData", pkcs15infoPtr->pubKeyData );
			}
		pkcs15infoPtr->pubKeyData = ( void * ) newPubKeyData;
		}

	/* Update the size information */
	pkcs15infoPtr->pubKeyDataSize = newPubKeyDataSize;
	pkcs15infoPtr->pubKeyOffset = newPubKeyOffset;
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void replaceCertData( INOUT PKCS15_INFO *pkcs15infoPtr, 
							 IN_BUFFER( newCertDataSize ) \
								const void *newCertData, 
							 IN_LENGTH_SHORT_MIN( 16 ) \
								const int newCertDataSize,
							 IN_LENGTH_SHORT const int newCertOffset )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( newCertData, newCertDataSize ) );

	REQUIRES_V( newCertDataSize >= 16 && \
				newCertDataSize < MAX_INTLENGTH_SHORT );
	REQUIRES_V( newCertOffset > 0 && \
				newCertOffset < newCertDataSize && \
				newCertOffset < MAX_INTLENGTH_SHORT );

	/* If we've allocated new storage for the data rather than directly 
	   replacing the existing entry, free the existing entry and replace it 
	   with the new one */
	if( newCertData != pkcs15infoPtr->certData )
		{
		if( pkcs15infoPtr->certData != NULL )
			{
			zeroise( pkcs15infoPtr->certData, pkcs15infoPtr->certDataSize );
			clFree( "replaceCertData", pkcs15infoPtr->certData );
			}
		pkcs15infoPtr->certData = ( void * ) newCertData;
		}

	/* Update the size information */
	pkcs15infoPtr->certDataSize = newCertDataSize;
	pkcs15infoPtr->certOffset = newCertOffset;
	}

/****************************************************************************
*																			*
*								Add a Certificate							*
*																			*
****************************************************************************/

/* Add a certificate to a PKCS #15 collection, updating affected public and
   private key attributes as required */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int pkcs15AddCert( INOUT PKCS15_INFO *pkcs15infoPtr, 
				   IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
				   IN_BUFFER_OPT( privKeyAttributeSize ) \
					const void *privKeyAttributes, 
				   IN_LENGTH_SHORT_Z const int privKeyAttributeSize,
				   IN_ENUM( CERTADD ) const CERTADD_TYPE certAddType, 
				   INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE certAttributes[ KEYATTR_BUFFER_SIZE + 8 ];
	void *newCertData = pkcs15infoPtr->certData;
	void *newPrivKeyData = pkcs15infoPtr->privKeyData;
	int newCertDataSize = DUMMY_INIT, certInfoSize = DUMMY_INIT;
	int newPrivKeyDataSize = DUMMY_INIT, privKeyInfoSize = DUMMY_INIT;
	int newCertOffset = DUMMY_INIT, certAttributeSize;
	int subType = PKCS15_SUBTYPE_NORMAL, keyTypeTag, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( ( certAddType == CERTADD_UPDATE_EXISTING && \
			  isReadPtr( privKeyAttributes, privKeyAttributeSize ) ) || \
			( ( certAddType == CERTADD_NORMAL || \
				certAddType == CERTADD_STANDALONE_CERT ) && \
			  privKeyAttributes == NULL && privKeyAttributeSize == 0 ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( ( certAddType == CERTADD_UPDATE_EXISTING && \
				privKeyAttributes != NULL && \
				privKeyAttributeSize > 0 && \
				privKeyAttributeSize < MAX_INTLENGTH_SHORT ) || \
			  ( ( certAddType == CERTADD_NORMAL || \
				  certAddType == CERTADD_STANDALONE_CERT ) && \
				privKeyAttributes == NULL && privKeyAttributeSize == 0 ) );
	REQUIRES( certAddType > CERTADD_NONE && certAddType < CERTADD_LAST );
	REQUIRES( errorInfo != NULL );

	/* Get the tag for encoding the key data */
	status = getKeyTypeTag( iCryptCert, CRYPT_ALGO_NONE, &keyTypeTag );
	if( cryptStatusError( status ) )
		return( status );

	/* If we've been passed a standalone certificate it has to be 
	   implicitly trusted in order to be added.  We don't perform this check 
	   if this is a storage object for a hardware device, which acts as a 
	   generic information store with no restrictions on what can be 
	   stored */
	if( certAddType == CERTADD_STANDALONE_CERT )
		{
		int value;

		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE,
								  &value, CRYPT_CERTINFO_TRUSTED_IMPLICIT );
		if( cryptStatusError( status ) || !value )
			{
			retExtArg( CRYPT_ARGERROR_NUM1, 
					   ( CRYPT_ARGERROR_NUM1, errorInfo, 
						 "Only a trusted certificate can be added as a "
						 "standalone certificate" ) );
			}

		/* Set the personality type to certificate-only */
		subType = PKCS15_SUBTYPE_CERT;
		}

	/* Write the certificate attributes */
	status = writeCertAttributes( certAttributes, KEYATTR_BUFFER_SIZE, 
								  &certAttributeSize, pkcs15infoPtr, 
								  iCryptCert );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write certificate attributes" ) );
		}

	/* Find out how big the PKCS #15 data will be and allocate room for it.
	   Since adding the certificate will affect the key attributes we need 
	   to rewrite the key information once we've added the certificate */
	if( certAddType == CERTADD_UPDATE_EXISTING )
		{
		/* Since we're re-using pre-encoded private key data the extra 
		   information is already present in encoded form so we set the 
		   extraDataSize parameter to zero */
		privKeyInfoSize = pkcs15infoPtr->privKeyDataSize - \
						  pkcs15infoPtr->privKeyOffset;
		status = calculatePrivkeyStorage( &newPrivKeyData, &newPrivKeyDataSize, 
										  pkcs15infoPtr->privKeyData,
										  pkcs15infoPtr->privKeyDataSize,
										  privKeyInfoSize,
										  privKeyAttributeSize, 0 );
		if( cryptStatusError( status ) )
			return( status );
		}
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( iCryptCert, IMESSAGE_CRT_EXPORT, &msgData,
							  CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		certInfoSize = msgData.length;
		status = calculateCertStorage( pkcs15infoPtr, &newCertData,
									   &newCertDataSize, certAttributeSize,
									   certInfoSize );
		}
	if( cryptStatusError( status ) )
		{
		if( newPrivKeyData != pkcs15infoPtr->privKeyData )
			clFree( "addCert", newPrivKeyData );
		return( status );
		}
	ANALYSER_HINT( newPrivKeyData != NULL );

	/* Write the PKCS #15 certificate data */
	sMemOpen( &stream, newCertData, newCertDataSize );
#ifdef USE_PKCS15V12_FORM
	writeSequence( &stream, certAttributeSize + \
							sizeofObject( \
							  sizeofObject( \
								sizeofObject( certInfoSize ) ) ) );
	swrite( &stream, certAttributes, certAttributeSize );
	writeConstructed( &stream, sizeofObject( \
								 sizeofObject( certInfoSize ) ), 
					  CTAG_OB_TYPEATTR );
	writeSequence( &stream, sizeofObject( certInfoSize ) );
	status = writeConstructed( &stream, certInfoSize, CTAG_OV_DIRECT );
#else
	writeSequence( &stream, certAttributeSize + \
							sizeofObject( sizeofObject( certInfoSize ) ) );
	swrite( &stream, certAttributes, certAttributeSize );
	writeConstructed( &stream, sizeofObject( certInfoSize ), 
					  CTAG_OB_TYPEATTR );
	status = writeSequence( &stream, certInfoSize );
#endif /* USE_PKCS15V12_FORM */
	if( cryptStatusOK( status ) )
		{
		newCertOffset = stell( &stream );
		status = exportCertToStream( &stream, iCryptCert, 
									 CRYPT_CERTFORMAT_CERTIFICATE );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		/* Undo what we've done so far without changing the existing PKCS #15
		   data */
		DEBUG_DIAG(( "Failed to set up/write certificate data" ));
		assert( DEBUG_WARN );
		if( newPrivKeyData != pkcs15infoPtr->privKeyData )
			clFree( "addCert", newPrivKeyData );
		if( newCertData != pkcs15infoPtr->certData && newCertData != NULL )
			clFree( "addCert", newCertData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write PKCS #15 certificate data" ) );
		}
	ENSURES( !cryptStatusError( checkObjectEncoding( newCertData, \
													 newCertDataSize ) ) );

	/* Replace the old certificate (if there is one) with the new one.  If 
	   it's a certificate associated with a private key we also have to 
	   update the private-key attributes, which can be affected by 
	   certificate information */
	pkcs15infoPtr->type = subType;
	replaceCertData( pkcs15infoPtr, newCertData, newCertDataSize, 
					 newCertOffset );
	if( certAddType == CERTADD_UPDATE_EXISTING )
		{
		updatePrivKeyAttributes( pkcs15infoPtr, 
								 newPrivKeyData, newPrivKeyDataSize, 
								 privKeyAttributes, privKeyAttributeSize, 
								 privKeyInfoSize, keyTypeTag );
		}

	/* The public-key data is redundant now that we've performed the update,
	   delete it */
	if( pkcs15infoPtr->pubKeyData != NULL )
		deletePubKey( pkcs15infoPtr );

	return( CRYPT_OK );
	}

/* Add a complete certificate chain to a PKCS #15 collection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int pkcs15AddCertChain( INOUT PKCS15_INFO *pkcs15info, 
						IN_LENGTH_SHORT const int noPkcs15objects,
						IN_HANDLE const CRYPT_CERTIFICATE iCryptCert, 
						INOUT ERROR_INFO *errorInfo )
	{
	BOOLEAN seenNonDuplicate = FALSE;
	int iterationCount = 0, status;

	assert( isWritePtr( pkcs15info, \
						sizeof( PKCS15_INFO ) * noPkcs15objects ) );

	REQUIRES( noPkcs15objects >= 1 && \
			  noPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( errorInfo != NULL );

	/* See if there are certificates in the chain beyond the first one, 
	   which we've already added.  Getting a data not found error is OK 
	   since it just means that there are no more certificates present */
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST,
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_CURSORNEXT,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( ( status == CRYPT_ERROR_NOTFOUND ) ? CRYPT_OK : status );

	/* Walk up the chain checking each certificate to see whether we need to 
	   add it */
	do
		{
		PKCS15_INFO *pkcs15infoPtr;
		BYTE iAndSID[ CRYPT_MAX_HASHSIZE + 8 ];
		int iAndSIDlength, index;

		/* Check whether this certificate is present */
		status = getCertID( iCryptCert, CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER,
							iAndSID, KEYID_SIZE, &iAndSIDlength );
		if( cryptStatusError( status ) )
			continue;
		if( findEntry( pkcs15info, noPkcs15objects, CRYPT_IKEYID_ISSUERID, 
					   iAndSID, iAndSIDlength, KEYMGMT_FLAG_NONE, FALSE ) != NULL )
			continue;

		/* We've found a certificate that isn't present yet, try and add 
		   it */
		pkcs15infoPtr = findFreeEntry( pkcs15info, noPkcs15objects, &index );
		if( pkcs15infoPtr == NULL )
			return( CRYPT_ERROR_OVERFLOW );
		status = pkcs15AddCert( pkcs15infoPtr, iCryptCert, NULL, 0, 
								CERTADD_NORMAL, errorInfo );
		if( cryptStatusOK( status ) )
			pkcs15infoPtr->index = index;

		/* A certificate being added may already be present, however we 
		   can't fail immediately because there may be further certificates 
		   in the chain that can be added so we keep track of whether we've 
		   successfully added at least one certificate and clear data 
		   duplicate errors */
		if( cryptStatusOK( status ) )
			seenNonDuplicate = TRUE;
		else
			{
			if( status == CRYPT_ERROR_DUPLICATE )
				status = CRYPT_OK;
			}
		}
	while( cryptStatusOK( status ) && \
		   krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
							MESSAGE_VALUE_CURSORNEXT,
							CRYPT_CERTINFO_CURRENT_CERTIFICATE ) == CRYPT_OK && \
		   iterationCount++ < FAILSAFE_ITERATIONS_MED );
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	if( cryptStatusOK( status ) && !seenNonDuplicate )
		{
		/* We reached the end of the chain without finding anything that we 
		   could add, return a data duplicate error */
		retExt( CRYPT_ERROR_DUPLICATE, 
				( CRYPT_ERROR_DUPLICATE, errorInfo, 
				  "Couldn't find any new certificates to add" ) );
		}
	return( status );
	}

/****************************************************************************
*																			*
*								Add a Public Key							*
*																			*
****************************************************************************/

/* Add a public key to a PKCS #15 collection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 8 ) ) \
int pkcs15AddPublicKey( INOUT PKCS15_INFO *pkcs15infoPtr, 
						IN_HANDLE const CRYPT_HANDLE iCryptContext, 
						IN_BUFFER( pubKeyAttributeSize ) \
							const void *pubKeyAttributes, 
						IN_LENGTH_SHORT const int pubKeyAttributeSize,
						IN_ALGO const CRYPT_ALGO_TYPE pkcCryptAlgo, 
						IN_LENGTH_PKC const int modulusSize, 
						const BOOLEAN isStorageObject, 
						INOUT ERROR_INFO *errorInfo )
	{
	const CRYPT_ATTRIBUTE_TYPE keyDataType = isStorageObject ? \
			CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL : CRYPT_IATTRIBUTE_KEY_SPKI;
	MESSAGE_DATA msgData;
	STREAM stream;
	void *newPubKeyData = pkcs15infoPtr->pubKeyData;
	int newPubKeyDataSize, newPubKeyOffset = DUMMY_INIT, pubKeySize;
	int extraDataSize = 0, keyTypeTag, status;

	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( pubKeyAttributes, pubKeyAttributeSize ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( pubKeyAttributeSize > 0 && \
			  pubKeyAttributeSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isPkcAlgo( pkcCryptAlgo ) );
	REQUIRES( ( isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE_ECC && \
				modulusSize <= CRYPT_MAX_PKCSIZE_ECC ) || \
			  ( !isEccAlgo( pkcCryptAlgo ) && \
				modulusSize >= MIN_PKCSIZE && \
				modulusSize <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( errorInfo != NULL );

	/* Get the tag for encoding the key data */
	status = getKeyTypeTag( CRYPT_UNUSED, pkcCryptAlgo, &keyTypeTag );
	if( cryptStatusError( status ) )
		return( status );

	/* Find out how big the PKCS #15 data will be and allocate room for it.
	   If it's a key metadata object then we have to read the information
	   using CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL since it's not necessarily
	   in the high state as required by CRYPT_IATTRIBUTE_KEY_SPKI because
	   the hardware may not be ready yet, but we can still fetch the stored
	   public-key data from it */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, keyDataType );
	if( cryptStatusError( status ) )
		return( status );
	pubKeySize = msgData.length;
	if( pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* RSA keys have an extra element for PKCS #11 compatibility */
		extraDataSize = sizeofShortInteger( modulusSize );
		}
	status = calculatePubkeyStorage( pkcs15infoPtr, &newPubKeyData, 
									 &newPubKeyDataSize, pubKeySize, 
									 pubKeyAttributeSize, extraDataSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the public key data */
	sMemOpen( &stream, newPubKeyData, newPubKeyDataSize );
	writeConstructed( &stream, pubKeyAttributeSize + \
							   sizeofObject( \
								 sizeofObject( \
								   sizeofObject( pubKeySize ) + \
								   extraDataSize ) ),
					  keyTypeTag );
	swrite( &stream, pubKeyAttributes, pubKeyAttributeSize );
	writeConstructed( &stream, sizeofObject( \
								sizeofObject( pubKeySize ) + \
								extraDataSize ),
					  CTAG_OB_TYPEATTR );
	writeSequence( &stream, sizeofObject( pubKeySize ) + extraDataSize );
	status = writeConstructed( &stream, pubKeySize, CTAG_OV_DIRECT );
	if( cryptStatusOK( status ) )
		{
		newPubKeyOffset = stell( &stream );
		status = exportAttributeToStream( &stream, iCryptContext,
										  keyDataType );
		}
	if( cryptStatusOK( status ) && pkcCryptAlgo == CRYPT_ALGO_RSA )
		{
		/* When using the SPKI option for storing key components the RSA
		   components require a [1] tag since the basic (non-SPKI) option is
		   also a SEQUENCE, so if it's an RSA key we modify the tag.  This is
		   easier than passing the tag requirement down through the kernel
		   call to the context.  In addition RSA keys have an extra element
		   for PKCS #11 compatibility */
		( ( BYTE * ) newPubKeyData )[ newPubKeyOffset ] = MAKE_CTAG( 1 );
		status = writeShortInteger( &stream, modulusSize, DEFAULT_TAG );
		}
	assert( stell( &stream ) == newPubKeyDataSize );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Failed to set up/write public key data" ));
		assert( DEBUG_WARN );
		if( newPubKeyData != pkcs15infoPtr->pubKeyData )
			clFree( "addPublicKey", newPubKeyData );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't write PKCS #15 public-key data" ) );
		}
	ENSURES( !cryptStatusError( checkObjectEncoding( newPubKeyData, \
													 newPubKeyDataSize ) ) );

	/* Replace the old data with the newly-written data */
	replacePubkeyData( pkcs15infoPtr, newPubKeyData, newPubKeyDataSize,
					   newPubKeyOffset );
	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
