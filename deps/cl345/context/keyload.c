/****************************************************************************
*																			*
*						  cryptlib Key Load Routines						*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

/* The default size of the salt for PKCS #5v2 key derivation, needed when we
   set the CRYPT_CTXINFO_KEYING_VALUE */

#define PKCS5_SALT_SIZE		8	/* 64 bits */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Convert a key attribute type into a key format type */

CHECK_RETVAL \
int attributeToFormatType( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute,
						   OUT_ENUM_OPT( KEYFORMAT ) KEYFORMAT_TYPE *keyformat )
	{
	static const MAP_TABLE attributeMapTbl[] = {
		{ CRYPT_IATTRIBUTE_KEY_SSH, KEYFORMAT_SSH },
		{ CRYPT_IATTRIBUTE_KEY_SSL, KEYFORMAT_SSL },
		{ CRYPT_IATTRIBUTE_KEY_SSL_EXT, KEYFORMAT_SSL_EXT },
		{ CRYPT_IATTRIBUTE_KEY_PGP, KEYFORMAT_PGP },
		{ CRYPT_IATTRIBUTE_KEY_PGP_PARTIAL, KEYFORMAT_PGP },
		{ CRYPT_IATTRIBUTE_KEY_SPKI, KEYFORMAT_CERT },
		{ CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL, KEYFORMAT_CERT },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	int value, status;

	assert( isWritePtr( keyformat, sizeof( KEYFORMAT_TYPE ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Clear return value */
	*keyformat = KEYFORMAT_NONE;

	status = mapValue( attribute, &value, attributeMapTbl, 
					   FAILSAFE_ARRAYSIZE( attributeMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	*keyformat = value;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Key Parameter Handling Functions					*
*																			*
****************************************************************************/

/* Initialise crypto parameters such as the IV and encryption mode, shared 
   by most capabilities.  This is never called directly, but is accessed
   through function pointers in the capability lists */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initGenericParams( INOUT CONTEXT_INFO *contextInfoPtr, 
					   IN_ENUM( KEYPARAM ) const KEYPARAM_TYPE paramType,
					   IN_OPT const void *data, 
					   IN_INT const int dataLength )
	{
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_CONV );
	REQUIRES( isEnumRange( paramType, KEYPARAM ) );
	REQUIRES( capabilityInfoPtr != NULL );

	/* Set the en/decryption mode if required */
	switch( paramType )
		{
		case KEYPARAM_MODE:
			REQUIRES( data == NULL );
			REQUIRES( isEnumRange( dataLength, CRYPT_MODE ) );

			switch( dataLength )
				{
				case CRYPT_MODE_ECB:
					FNPTR_SET( contextInfoPtr->encryptFunction,
							   capabilityInfoPtr->encryptFunction );
					FNPTR_SET( contextInfoPtr->decryptFunction, 
							   capabilityInfoPtr->decryptFunction );
					break;
				case CRYPT_MODE_CBC:
					FNPTR_SET( contextInfoPtr->encryptFunction,
							   capabilityInfoPtr->encryptCBCFunction );
					FNPTR_SET( contextInfoPtr->decryptFunction,
							   capabilityInfoPtr->decryptCBCFunction );
					break;
#ifdef USE_CFB
				case CRYPT_MODE_CFB:
					FNPTR_SET( contextInfoPtr->encryptFunction,
							   capabilityInfoPtr->encryptCFBFunction );
					FNPTR_SET( contextInfoPtr->decryptFunction,
							   capabilityInfoPtr->decryptCFBFunction );
					break;
#endif /* USE_CFB */
				case CRYPT_MODE_GCM:
					FNPTR_SET( contextInfoPtr->encryptFunction,
							   capabilityInfoPtr->encryptGCMFunction );
					FNPTR_SET( contextInfoPtr->decryptFunction,
							   capabilityInfoPtr->decryptGCMFunction );
					break;
				default:
					retIntError();
				}
			ENSURES( ( FNPTR_ISNULL( contextInfoPtr->encryptFunction ) && \
					   FNPTR_ISNULL( contextInfoPtr->decryptFunction ) ) || \
					 ( FNPTR_ISSET( contextInfoPtr->encryptFunction ) && \
					   FNPTR_ISSET( contextInfoPtr->decryptFunction ) ) );
			if( !FNPTR_ISSET( contextInfoPtr->encryptFunction ) || \
				!FNPTR_ISSET( contextInfoPtr->decryptFunction ) )
				{
				setErrorInfo( contextInfoPtr, CRYPT_CTXINFO_MODE, 
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_NOTAVAIL );
				}
			convInfo->mode = dataLength;

			return( CRYPT_OK );

		case KEYPARAM_IV:
			assert( isReadPtrDynamic( data, dataLength ) );

			REQUIRES( data != NULL && \
					  dataLength >= 8 && dataLength <= CRYPT_MAX_IVSIZE );

			/* Load an IV of the required length */
			REQUIRES( rangeCheck( dataLength, 1, CRYPT_MAX_IVSIZE ) );
			memcpy( convInfo->iv, data, dataLength );
			convInfo->ivLength = dataLength;
			convInfo->ivCount = 0;
			memcpy( convInfo->currentIV, convInfo->iv, dataLength );
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_IV_SET );

			return( CRYPT_OK );
			}

	retIntError();
	}

/* Check that user-supplied supplied PKC parameters make sense (algorithm-
   parameter-specific validity checks are performed at a lower level).  
   Although the checks are somewhat specific to particular PKC algorithm 
   classes we have to do them at this point in order to avoid duplicating 
   them in every plug-in PKC module, and because strictly speaking it's the 
   job of the higher-level code to ensure that the lower-level routines get 
   fed at least approximately valid input */

#ifndef USE_FIPS140 

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int checkPKCparams( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo, 
						   const void *keyInfo )
	{
	const CRYPT_PKCINFO_RSA *rsaKey = ( CRYPT_PKCINFO_RSA * ) keyInfo;

	REQUIRES( isPkcAlgo( cryptAlgo ) );
	REQUIRES( keyInfo != NULL );

	/* The ECC check is somewhat different to the others because ECC key
	   sizes work in different ways so we have to special-case this one */
	if( isEccAlgo( cryptAlgo ) )
		{
		const CRYPT_PKCINFO_ECC *eccKey = ( CRYPT_PKCINFO_ECC * ) keyInfo;

		assert( isReadPtr( keyInfo, sizeof( CRYPT_PKCINFO_ECC ) ) );

		/* Check the general info and make sure that all required values are
		   initialised */
		if( ( eccKey->isPublicKey != TRUE_ALT && \
			  eccKey->isPublicKey != FALSE ) )
			return( CRYPT_ARGERROR_STR1 );
#if 0	/* For now we always require the use of named curves, which means 
		   that the domain parameters can't be explicitly set */
		if( eccKey->curveType != CRYPT_ECCCURVE_NONE )
			{
			if( eccKey->pLen <= 0 || eccKey->aLen <= 0 || eccKey->bLen <= 0 || \
				eccKey->gxLen <= 0 || eccKey->gyLen <= 0 || eccKey->nLen <= 0 || \
				eccKey->qxLen <= 0 || eccKey->qyLen <= 0 || eccKey->dLen < 0 )
				return( CRYPT_ARGERROR_STR1 );
			}
		else
#endif /* 0 */
			{
			if( eccKey->pLen != 0 || eccKey->aLen != 0 || eccKey->bLen != 0 || \
				eccKey->gxLen != 0 || eccKey->gyLen != 0 || eccKey->nLen != 0 || \
				eccKey->hLen != 0 || eccKey->qxLen <= 0 || eccKey->qyLen <= 0 || \
				eccKey->dLen < 0 )
				return( CRYPT_ARGERROR_STR1 );
			if( !isEnumRange( eccKey->curveType, CRYPT_ECCCURVE ) )
				return( CRYPT_ARGERROR_STR1 );
			}

		/* Check the parameters and public components */
		if( isShortECCKey( eccKey->pLen ) )
			{
			/* Special-case handling for insecure-sized public keys */
			return( CRYPT_ERROR_NOSECURE );
			}
#if 0	/* Already checked above */
		if( eccKey->curveType != CRYPT_ECCCURVE_NONE )
			{
			if( eccKey->pLen < bytesToBits( ECCPARAM_MIN_P ) || \
				eccKey->pLen > bytesToBits( ECCPARAM_MAX_P ) || \
				eccKey->aLen < bytesToBits( ECCPARAM_MIN_A ) || \
				eccKey->aLen > bytesToBits( ECCPARAM_MAX_A ) || \
				eccKey->bLen < bytesToBits( ECCPARAM_MIN_B ) || \
				eccKey->bLen > bytesToBits( ECCPARAM_MAX_B ) || \
				eccKey->gxLen < bytesToBits( ECCPARAM_MIN_GX ) || \
				eccKey->gxLen > bytesToBits( ECCPARAM_MAX_GX ) || \
				eccKey->gyLen < bytesToBits( ECCPARAM_MIN_GY ) || \
				eccKey->gyLen > bytesToBits( ECCPARAM_MAX_GY ) || \
				eccKey->nLen < bytesToBits( ECCPARAM_MIN_N ) || \
				eccKey->nLen > bytesToBits( ECCPARAM_MAX_N ) || \
				eccKey->hLen < 1 || \
				eccKey->hLen > bytesToBits( ECCPARAM_MAX_N ) )
				return( CRYPT_ARGERROR_STR1 );
			}
#endif /* 0 */
		if( eccKey->qxLen < bytesToBits( ECCPARAM_MIN_QX ) || \
			eccKey->qxLen > bytesToBits( ECCPARAM_MAX_QX ) || \
			eccKey->qyLen < bytesToBits( ECCPARAM_MIN_QY ) || \
			eccKey->qyLen > bytesToBits( ECCPARAM_MAX_QY ) )
			return( CRYPT_ARGERROR_STR1 ); 
		if( eccKey->isPublicKey )
			return( CRYPT_OK );

		/* Check the private components */
		if( eccKey->dLen < bytesToBits( ECCPARAM_MIN_D ) || \
			eccKey->dLen > bytesToBits( ECCPARAM_MAX_D ) )
			return( CRYPT_ARGERROR_STR1 );
		return( CRYPT_OK );
		}

	/* For the non-ECC algorithms the DLP check is simpler than the RSA one 
	   because there are less odd parameter combinations possible so we get 
	   this one out of the way first.  Note that we don't get PKCS #3 DH 
	   keys at this level so we always require that q be present */
	if( isDlpAlgo( cryptAlgo ) )
		{
		const CRYPT_PKCINFO_DLP *dlpKey = ( CRYPT_PKCINFO_DLP * ) keyInfo;

		assert( isReadPtr( keyInfo, sizeof( CRYPT_PKCINFO_DLP ) ) );

		/* Check the general info and make sure that all required values are
		   initialised */
		if( ( dlpKey->isPublicKey != TRUE_ALT && \
			  dlpKey->isPublicKey != FALSE ) )
			return( CRYPT_ARGERROR_STR1 );
		if( dlpKey->pLen <= 0 || dlpKey->qLen <= 0 || dlpKey->gLen <= 0 || \
			dlpKey->yLen < 0 || dlpKey->xLen < 0 )
			return( CRYPT_ARGERROR_STR1 );

		/* Check the public components */
		if( isShortPKCKey( dlpKey->pLen ) )
			{
			/* Special-case handling for insecure-sized public keys */
			return( CRYPT_ERROR_NOSECURE );
			}
		if( dlpKey->pLen < bytesToBits( DLPPARAM_MIN_P ) || \
			dlpKey->pLen > bytesToBits( DLPPARAM_MAX_P ) || \
			dlpKey->qLen < bytesToBits( DLPPARAM_MIN_Q ) || \
			dlpKey->qLen > bytesToBits( DLPPARAM_MAX_Q ) || \
			dlpKey->gLen < bytesToBits( DLPPARAM_MIN_G ) || \
			dlpKey->gLen > bytesToBits( DLPPARAM_MAX_G ) || \
			dlpKey->yLen < bytesToBits( 0 ) || \
			dlpKey->yLen > bytesToBits( DLPPARAM_MAX_Y ) )
			/* y may be 0 if only x and the public parameters are available */
			{
			return( CRYPT_ARGERROR_STR1 );
			}
		if( !( dlpKey->p[ bitsToBytes( dlpKey->pLen ) - 1 ] & 0x01 ) || \
			!( dlpKey->q[ bitsToBytes( dlpKey->qLen ) - 1 ] & 0x01 ) )
			return( CRYPT_ARGERROR_STR1 );	/* Quick non-prime check */
		if( dlpKey->isPublicKey )
			return( CRYPT_OK );

		/* Check the private components */
		if( dlpKey->xLen < bytesToBits( DLPPARAM_MIN_X ) || \
			dlpKey->xLen > bytesToBits( DLPPARAM_MAX_X ) )
			return( CRYPT_ARGERROR_STR1 );
		return( CRYPT_OK );
		}

	assert( isReadPtr( keyInfo, sizeof( CRYPT_PKCINFO_RSA ) ) );

	/* Check the general info and make sure that all required values are
	   initialised */
	if( rsaKey->isPublicKey != TRUE_ALT && rsaKey->isPublicKey != FALSE )
		return( CRYPT_ARGERROR_STR1 );
	if( rsaKey->nLen <= 0 || rsaKey->eLen <= 0 || \
		rsaKey->dLen < 0 || rsaKey->pLen < 0 || rsaKey->qLen < 0 || \
		rsaKey->uLen < 0 || rsaKey->e1Len < 0 || rsaKey->e2Len < 0 )
		return( CRYPT_ARGERROR_STR1 );

	/* Check the public components */
	if( isShortPKCKey( rsaKey->nLen ) )
		{
		/* Special-case handling for insecure-sized public keys */
		return( CRYPT_ERROR_NOSECURE );
		}
	if( rsaKey->nLen < bytesToBits( RSAPARAM_MIN_N ) || \
		rsaKey->nLen > bytesToBits( RSAPARAM_MAX_N ) || \
		rsaKey->eLen < bytesToBits( RSAPARAM_MIN_E ) || \
		rsaKey->eLen > bytesToBits( RSAPARAM_MAX_E ) || \
		rsaKey->eLen > rsaKey->nLen )
		return( CRYPT_ARGERROR_STR1 );
	if( !( rsaKey->n[ bitsToBytes( rsaKey->nLen ) - 1 ] & 0x01 ) || \
		!( rsaKey->e[ bitsToBytes( rsaKey->eLen ) - 1 ] & 0x01 ) )
		return( CRYPT_ARGERROR_STR1 );	/* Quick non-prime check */
	if( rsaKey->isPublicKey )
		return( CRYPT_OK );

	/* Check the private components.  This can get somewhat complex, possible
	   combinations are:

		d, p, q
		d, p, q, u
		d, p, q, e1, e2, u
		   p, q, e1, e2, u

	   The reason for some of the odder combinations is that some 
	   implementations don't use all of the values (for example d isn't 
	   needed at all for the CRT shortcut) or recreate them when the key is 
	   loaded.  If only d, p, and q are present we recreate e1 and e2 from 
	   them, we also create u if necessary */
	if( rsaKey->pLen < bytesToBits( RSAPARAM_MIN_P ) || \
		rsaKey->pLen > bytesToBits( RSAPARAM_MAX_P ) || \
		rsaKey->pLen >= rsaKey->nLen || \
		rsaKey->qLen < bytesToBits( RSAPARAM_MIN_Q ) || \
		rsaKey->qLen > bytesToBits( RSAPARAM_MAX_Q ) || \
		rsaKey->qLen >= rsaKey->nLen )
		return( CRYPT_ARGERROR_STR1 );
	if( !( rsaKey->p[ bitsToBytes( rsaKey->pLen ) - 1 ] & 0x01 ) || \
		!( rsaKey->q[ bitsToBytes( rsaKey->qLen ) - 1 ] & 0x01 ) )
		return( CRYPT_ARGERROR_STR1 );	/* Quick non-prime check */
	if( rsaKey->dLen <= 0 && rsaKey->e1Len <= 0 )
		{
		/* Must have either d or e1 et al */
		return( CRYPT_ARGERROR_STR1 );
		}
	if( rsaKey->dLen > 0 && \
		( rsaKey->dLen < bytesToBits( RSAPARAM_MIN_D ) || \
		  rsaKey->dLen > bytesToBits( RSAPARAM_MAX_D ) ) )
		return( CRYPT_ARGERROR_STR1 );
	if( rsaKey->e1Len > 0 && \
		( rsaKey->e1Len < bytesToBits( RSAPARAM_MIN_EXP1 ) || \
		  rsaKey->e1Len > bytesToBits( RSAPARAM_MAX_EXP1 ) || \
		  rsaKey->e2Len < bytesToBits( RSAPARAM_MIN_EXP2 ) || \
		  rsaKey->e2Len > bytesToBits( RSAPARAM_MAX_EXP2 ) ) )
		return( CRYPT_ARGERROR_STR1 );
	if( rsaKey->uLen > 0 && \
		( rsaKey->uLen < bytesToBits( RSAPARAM_MIN_U ) || \
		  rsaKey->uLen > bytesToBits( RSAPARAM_MAX_U ) ) )
		return( CRYPT_ARGERROR_STR1 );
	return( CRYPT_OK );
	}
#endif /* !USE_FIPS140 */

/****************************************************************************
*																			*
*								Key Load Functions							*
*																			*
****************************************************************************/

/* Load a key into a CONTEXT_INFO structure.  These functions are called by 
   the various higher-level functions that move a key into a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int loadKeyConvFunction( INOUT CONTEXT_INFO *contextInfoPtr, 
								IN_BUFFER( keyLength ) const void *key, 
								IN_LENGTH_KEY const int keyLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	int keyDataSize, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_CONV );
	REQUIRES( keyLength >= MIN_KEYSIZE && keyLength <= CRYPT_MAX_KEYSIZE );
	REQUIRES( capabilityInfoPtr != NULL );

	/* If we don't need an IV, record it as being set */
	if( !needsIV( convInfo->mode ) || \
		isStreamCipher( capabilityInfoPtr->cryptAlgo ) )
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_IV_SET );

	/* Perform the key setup */
	status = capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_STATESIZE, 
												 NULL, &keyDataSize, 0 );
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfoPtr->initKeyFunction( contextInfoPtr, key, 
													 keyLength );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Checksum the keying information for side-channel attack protection
	   purposes */
	if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
		{
		convInfo->keyDataSize = keyDataSize;
		convInfo->keyDataChecksum = checksumData( convInfo->key, 
												  convInfo->keyDataSize );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int loadKeyPKCFunction( INOUT CONTEXT_INFO *contextInfoPtr, 
							   IN_BUFFER_OPT( keyLength ) const void *key, 
							   IN_LENGTH_SHORT_Z const int keyLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( key == NULL ) || isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES( ( key == NULL && keyLength == 0 ) || \
			  ( key != NULL && \
			    keyLength > 16 && keyLength < MAX_INTLENGTH_SHORT ) );
			  /* The key data for this function may be NULL if the values
			     have been read from encoded X.509/SSH/SSL/PGP data straight
				 into the context bignums */
	REQUIRES( capabilityInfoPtr != NULL );

#ifndef USE_FIPS140 
	/* Make sure that the parameters make sense */
	if( key != NULL )
		{
		status = checkPKCparams( capabilityInfoPtr->cryptAlgo, key );
		if( cryptStatusError( status ) )
			return( status );
		SET_FLAG( contextInfoPtr->flags, 0x08 );
				  /* Tell lib_kg to check params too */
		}
#endif /* !USE_FIPS140 */

	/* Load the keying information.  The checksumming of the key data is 
	   performed by the key-init code since for the DLP algorithms it can 
	   get complicated in the presence of (EC)DH contexts that aren't quite
	   public or private keys */
	status = capabilityInfoPtr->initKeyFunction( contextInfoPtr, key, 
												 keyLength );
	if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
		clearTempBignums( contextInfoPtr->ctxPKC );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int loadKeyMacFunction( INOUT CONTEXT_INFO *contextInfoPtr, 
							   IN_BUFFER( keyLength ) const void *key, 
							   IN_LENGTH_KEY const int keyLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_MAC );
	REQUIRES( keyLength >= MIN_KEYSIZE && keyLength <= CRYPT_MAX_KEYSIZE );
	REQUIRES( capabilityInfoPtr != NULL );

	return( capabilityInfoPtr->initKeyFunction( contextInfoPtr, key, keyLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int loadKeyGenericFunction( INOUT CONTEXT_INFO *contextInfoPtr, 
								   IN_BUFFER( keyLength ) const void *key, 
								   IN_LENGTH_KEY const int keyLength )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_GENERIC );
	REQUIRES( keyLength >= bitsToBytes( 128 ) && \
			  keyLength <= CRYPT_MAX_KEYSIZE );
	REQUIRES( capabilityInfoPtr != NULL );

	return( capabilityInfoPtr->initKeyFunction( contextInfoPtr, key, keyLength ) );
	}

/****************************************************************************
*																			*
*							Key Component Load Functions					*
*																			*
****************************************************************************/

/* Load an encoded X.509/SSH/SSL/PGP key into a context.  This is used for 
   two purposes, to load public key components into native contexts and to 
   save encoded X.509 public-key data for use in certificates associated 
   with non-native contexts held in a device.  The latter is required 
   because there's no key data stored with the context itself that we can 
   use to create the SubjectPublicKeyInfo, however it's necessary to have 
   SubjectPublicKeyInfo available for certificate requests/certificates.  

   Normally this is sufficient because cryptlib always generates native 
   contexts for public keys/certificates and for private keys the data is 
   generated in the device with the encoded public components attached to 
   the context as described above.  However for DH keys this gets a bit more 
   complex because although the private key is generated in the device, in 
   the case of the DH responder this is only the DH x value, with the 
   parameters (p and g) being supplied externally by the initiator.  This 
   means that it's necessary to decode at least some of the public key data 
   in order to create the y value after the x value has been generated in 
   the device.  The only situation where this functionality is currently 
   needed is for the SSHv2 code, which at the moment always uses native DH 
   contexts.  For this reason we leave off resolving this issue until it's 
   actually required */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int setEncodedKey( INOUT CONTEXT_INFO *contextInfoPtr, 
				   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE keyType, 
				   IN_BUFFER( keyDataLen ) const void *keyData, 
				   IN_LENGTH_SHORT const int keyDataLen )
	{
	const PKC_CALCULATEKEYID_FUNCTION calculateKeyIDFunction = \
				( PKC_CALCULATEKEYID_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->ctxPKC->calculateKeyIDFunction );
	const PKC_READKEY_FUNCTION readPublicKeyFunction = \
					( PKC_READKEY_FUNCTION ) \
					FNPTR_GET( contextInfoPtr->ctxPKC->readPublicKeyFunction );
	STREAM stream;
	KEYFORMAT_TYPE formatType;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( keyData, keyDataLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES( needsKey( contextInfoPtr ) || \
			  TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) );
	REQUIRES( keyType == CRYPT_IATTRIBUTE_KEY_SPKI || \
			  keyType == CRYPT_IATTRIBUTE_KEY_PGP || \
			  keyType == CRYPT_IATTRIBUTE_KEY_SSH || \
			  keyType == CRYPT_IATTRIBUTE_KEY_SSL || \
			  keyType == CRYPT_IATTRIBUTE_KEY_SSL_EXT || \
			  keyType == CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL || \
			  keyType == CRYPT_IATTRIBUTE_KEY_PGP_PARTIAL );
	REQUIRES( keyDataLen >= 2 && keyDataLen < MAX_INTLENGTH_SHORT );
			  /* Can be very short in the case of ECC curve IDs */
	REQUIRES( calculateKeyIDFunction != NULL );
	REQUIRES( readPublicKeyFunction != NULL );

	/* If the keys are held externally (e.g. in a crypto device), copy the 
	   SubjectPublicKeyInfo data in and set up any other information that we 
	   may need from it.  This information is used when loading a context 
	   from a key contained in a device, for which the actual key components 
	   aren't directly available in the context but may be needed in the 
	   future for things like certificate requests and certificates */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
		{
		REQUIRES( keyType == CRYPT_IATTRIBUTE_KEY_SPKI || \
				  keyType == CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL );

		REQUIRES( rangeCheck( keyDataLen, 1, MAX_INTLENGTH_SHORT ) );
		if( ( contextInfoPtr->ctxPKC->publicKeyInfo = \
					clAlloc( "setEncodedKey", keyDataLen ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		memcpy( contextInfoPtr->ctxPKC->publicKeyInfo, keyData, keyDataLen );
		contextInfoPtr->ctxPKC->publicKeyInfoSize = keyDataLen;
		return( calculateKeyIDFunction( contextInfoPtr ) );
		}

	/* Read the appropriately-formatted key data into the context, applying 
	   a lowest-common-denominator set of usage flags to the loaded key */
	status = attributeToFormatType( keyType, &formatType );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, keyData, keyDataLen );
	status = readPublicKeyFunction( &stream, contextInfoPtr, formatType, 0 );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a partial load of the initial public portions of a private 
	   key with further key component operations to follow, there's nothing 
	   more to do at this point and we're done */
	if( keyType == CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL || \
		keyType == CRYPT_IATTRIBUTE_KEY_PGP_PARTIAL )
		return( calculateKeyIDFunction( contextInfoPtr ) );

	/* Complete the key load using the internally-stored key components */
	return( completeKeyLoad( contextInfoPtr, 
				( keyType == CRYPT_IATTRIBUTE_KEY_PGP ) ? TRUE : FALSE ) );
	}

/* Complete the load process for a key that was loaded with setEncodedKey() 
   or by setting DLP/ECDLP domain parameters */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int completeKeyLoad( INOUT CONTEXT_INFO *contextInfoPtr, 
					 const BOOLEAN isPGPkey )
	{
	static const int actionFlags = \
		MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_NONE_EXTERNAL );
	static const int actionFlagsDH = ACTION_PERM_NONE_EXTERNAL_ALL;
	static const int actionFlagsPGP = \
		MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL );
	const CAPABILITY_INFO *capabilityInfoPtr = \
				DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const PKC_CALCULATEKEYID_FUNCTION calculateKeyIDFunction = \
				( PKC_CALCULATEKEYID_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->ctxPKC->calculateKeyIDFunction );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
				( CTX_LOADKEY_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->loadKeyFunction );
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isPGPkey == TRUE || isPGPkey == FALSE );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( calculateKeyIDFunction != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* Perform an internal load that uses the key component values that 
	   we've just read into the context */
	SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
	status = loadKeyFunction( contextInfoPtr, NULL, 0 );
	if( cryptStatusError( status ) )
		{
		/* Map the status to a more appropriate code if necessary */
		return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
		}
	SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET );

	/* Restrict the key usage to public-key-only actions if necessary.  For 
	   PGP key loads (which, apart from the restrictions specified with the 
	   stored key data aren't constrained by the presence of ACLs in the 
	   form of certificates) we allow external usage, for DH (whose keys can be 
	   both public and private keys even though technically it's a public 
	   key) we allow both encryption and decryption usage, and for public 
	   keys read from certificates we  allow internal usage only */
	status = krnlSendMessage( contextInfoPtr->objectHandle,
						IMESSAGE_SETATTRIBUTE, 
						isPGPkey ? ( MESSAGE_CAST ) &actionFlagsPGP : \
						( isKeyxAlgo( capabilityInfoPtr->cryptAlgo ) ) ? \
							( MESSAGE_CAST ) &actionFlagsDH : \
							( MESSAGE_CAST ) &actionFlags,
						CRYPT_IATTRIBUTE_ACTIONPERMS );
	if( cryptStatusError( status ) )
		return( status );
	return( calculateKeyIDFunction( contextInfoPtr ) );
	}

/* Load the components of a composite PKC key into a context */

#ifndef USE_FIPS140

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setKeyComponents( INOUT CONTEXT_INFO *contextInfoPtr, 
					  IN_BUFFER( keyDataLen ) const void *keyData, 
					  IN_LENGTH_SHORT_MIN( 32 ) const int keyDataLen )
	{
	static const int actionFlags = \
		MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL );
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const PKC_CALCULATEKEYID_FUNCTION calculateKeyIDFunction = \
				( PKC_CALCULATEKEYID_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->ctxPKC->calculateKeyIDFunction );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
				( CTX_LOADKEY_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->loadKeyFunction );
	BOOLEAN isPublicKey;
	int externalBoolean, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( keyData, keyDataLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  needsKey( contextInfoPtr ) );
	REQUIRES( keyDataLen == sizeof( CRYPT_PKCINFO_RSA ) || \
			  keyDataLen == sizeof( CRYPT_PKCINFO_DLP ) || \
			  keyDataLen == sizeof( CRYPT_PKCINFO_ECC ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( calculateKeyIDFunction != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* If it's a private key we need to have a key label set before we can 
	   continue.  The checking for this is a bit complex because at this
	   point all that the context knows is that it's a generic PKC context,
	   but it won't know whether it's a public- or private-key context until
	   the key is actually loaded.  To determine what it'll become we look
	   into the key data to see what's being loaded.
	   
	   In addition to this the key data probably came from an external 
	   source (internal loads only come from marshalled data), so we have
	   to convert the boolean representation from the external to the 
	   internal form */
	if( isEccAlgo( capabilityInfoPtr->cryptAlgo ) )
		externalBoolean = ( ( CRYPT_PKCINFO_ECC * ) keyData )->isPublicKey;
	else
		{
		if( isDlpAlgo( capabilityInfoPtr->cryptAlgo ) )
			externalBoolean = ( ( CRYPT_PKCINFO_DLP * ) keyData )->isPublicKey;
		else
			externalBoolean = ( ( CRYPT_PKCINFO_RSA * ) keyData )->isPublicKey;
		}
	isPublicKey = externalBoolean ? TRUE : FALSE;
	if( !isPublicKey && contextInfoPtr->labelSize <= 0 )
		return( CRYPT_ERROR_NOTINITED );

	/* If it's a dummy object with keys held externally (e.g. in a crypto 
	   device) we need a key label set in order to access the object at a 
	   later date */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) && \
		contextInfoPtr->labelSize <= 0 )
		return( CRYPT_ERROR_NOTINITED );

	/* Load the key components into the context */
	status = loadKeyFunction( contextInfoPtr, keyData, keyDataLen );
	if( cryptStatusError( status ) )
		return( status );
	SET_FLAG( contextInfoPtr->flags, 
			  CONTEXT_FLAG_KEY_SET | CONTEXT_FLAG_PBO );

	/* Restrict the key usage to public-key-only actions if it's a public 
	   key.  DH keys act as both public and private keys so we don't 
	   restrict their usage */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY ) && \
		( capabilityInfoPtr->cryptAlgo != CRYPT_ALGO_DH ) )
		{
		status = krnlSendMessage( contextInfoPtr->objectHandle,
								  IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &actionFlags,
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( calculateKeyIDFunction( contextInfoPtr ) );
	}
#endif /* !USE_FIPS140 */

/****************************************************************************
*																			*
*							Key Generation Functions						*
*																			*
****************************************************************************/

/* Generate a key into a CONTEXT_INFO structure */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKeyConvFunction( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
							DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
							( CTX_LOADKEY_FUNCTION ) \
							FNPTR_GET( contextInfoPtr->loadKeyFunction );
	CONV_INFO *convInfo = contextInfoPtr->ctxConv;
	MESSAGE_DATA msgData;
	int keyLength = convInfo->userKeyLength, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_CONV );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* If there's no key size specified, use the default length */
	if( keyLength <= 0 )
		keyLength = capabilityInfoPtr->keySize;

	/* If the context is implemented in a crypto device it may have the
	   capability to generate the key itself so if there's a keygen function
	   present we call this to generate the key directly into the context
	   rather than generating it ourselves and loading it in.  Note that to
	   export this key we'll need to use an exporting context which is also
	   located in the device, since we can't access it externally */
	if( capabilityInfoPtr->generateKeyFunction != NULL )
		{
		return( capabilityInfoPtr->generateKeyFunction( contextInfoPtr,
												bytesToBits( keyLength ) ) );
		}

	/* Generate a random session key into the context.  We load the random 
	   data directly into the pagelocked encryption context and pass that in 
	   as the key buffer, loadKey() won't copy the data if src == dest */
	setMessageData( &msgData, convInfo->userKey, keyLength );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	convInfo->userKeyLength = keyLength;
	return( loadKeyFunction( contextInfoPtr, convInfo->userKey, 
							 convInfo->userKeyLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKeyPKCFunction( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const PKC_CALCULATEKEYID_FUNCTION calculateKeyIDFunction = \
				( PKC_CALCULATEKEYID_FUNCTION ) \
				FNPTR_GET( contextInfoPtr->ctxPKC->calculateKeyIDFunction );
	int keyLength = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( calculateKeyIDFunction != NULL );

	/* Writing a key in PGP format requires that it have a creation time 
	   associated with it, however keys from non-PGP sources don't have 
	   this.  In theory if we wanted to write out the key in PGP format 
	   then we'd have to set one in order to have something to write out.  
	   However, the creation time is hashed into the OpenPGP key ID when the 
	   ID is generated, which means that moving a key via a non-PGP format 
	   changes its key ID.  

	   Fortunately the standard never actually explains what the creation 
	   time field is for, so it probably doesn't matter what we set it to.
       Because of this we leave it at its default value of zero */
#if defined( USE_PGP ) && 0
	contextInfoPtr->ctxPKC->pgpCreationTime = getApproxTime();
#endif /* USE_PGP */

	/* If there's no key size specified, use the default length.  In theory 
	   we could also read the CRYPT_OPTION_PKC_KEYSIZE at this point, 
	   however this only applies to the algorithm selected by 
	   CRYPT_OPTION_PKC_ALGO, or perhaps the algorithm class of 
	   CRYPT_OPTION_PKC_ALGO (for example { RSA, DSA, DH } vs. { ECDSA, 
	   ECDH }), however this then creates a confusing gotcha where all 
	   algorithms except CRYPT_OPTION_PKC_ALGO use the default key size if 
	   none is explicitly specified using CRYPT_CTXINFO_KEYSIZE while
	   CRYPT_OPTION_PKC_ALGO takes its key size from 
	   CRYPT_OPTION_PKC_KEYSIZE.  A quick user poll indicated that no-one 
	   specifically wanted this behaviour, so we don't use 
	   CRYPT_OPTION_PKC_KEYSIZE */
	if( keyLength <= 0 )
		keyLength = capabilityInfoPtr->keySize;

	/* Generate the key into the context */
	status = capabilityInfoPtr->generateKeyFunction( contextInfoPtr,
												bytesToBits( keyLength ) );
	if( !TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_DUMMY ) )
		clearTempBignums( contextInfoPtr->ctxPKC );
	if( cryptStatusError( status ) )
		return( status );
	return( calculateKeyIDFunction( contextInfoPtr ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKeyMacFunction( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
							DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
							( CTX_LOADKEY_FUNCTION ) \
							FNPTR_GET( contextInfoPtr->loadKeyFunction );
	MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
	MESSAGE_DATA msgData;
	int keyLength = macInfo->userKeyLength, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_MAC );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* If there's no key size specified, use the default length */
	if( keyLength <= 0 )
		keyLength = capabilityInfoPtr->keySize;

	/* If the context is implemented in a crypto device it may have the
	   capability to generate the key itself so if there's a keygen function
	   present we call this to generate the key directly into the context
	   rather than generating it ourselves and loading it in.  Note that to
	   export this key we'll need to use an exporting context which is also
	   located in the device, since we can't access it externally */
	if( capabilityInfoPtr->generateKeyFunction != NULL )
		{
		return( capabilityInfoPtr->generateKeyFunction( contextInfoPtr,
												bytesToBits( keyLength ) ) );
		}

	/* Generate a random session key into the context.  We load the random 
	   data directly into the pagelocked encryption context and pass that in 
	   as the key buffer, loadKey() won't copy the data if src == dest */
	setMessageData( &msgData, macInfo->userKey, keyLength );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	macInfo->userKeyLength = keyLength;
	return( loadKeyFunction( contextInfoPtr, macInfo->userKey, 
							 macInfo->userKeyLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKeyGenericFunction( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
							DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
							( CTX_LOADKEY_FUNCTION ) \
							FNPTR_GET( contextInfoPtr->loadKeyFunction );
	GENERIC_INFO *genericInfo = contextInfoPtr->ctxGeneric;
	MESSAGE_DATA msgData;
	int keyLength = genericInfo->genericSecretLength, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_GENERIC );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* If there's no key size specified, use the default length */
	if( keyLength <= 0 )
		keyLength = capabilityInfoPtr->keySize;

	/* If the context is implemented in a crypto device it may have the
	   capability to generate the key itself so if there's a keygen function
	   present we call this to generate the key directly into the context
	   rather than generating it ourselves and loading it in.  Note that to
	   export this key we'll need to use an exporting context which is also
	   located in the device, since we can't access it externally */
	if( capabilityInfoPtr->generateKeyFunction != NULL )
		{
		return( capabilityInfoPtr->generateKeyFunction( contextInfoPtr,
												bytesToBits( keyLength ) ) );
		}

	/* Generate a random session key into the context.  We load the random 
	   data directly into the pagelocked encryption context and pass that in 
	   as the key buffer, loadKey() won't copy the data if src == dest */
	setMessageData( &msgData, genericInfo->genericSecret, keyLength );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	genericInfo->genericSecretLength = keyLength;
	return( loadKeyFunction( contextInfoPtr, genericInfo->genericSecret, 
							 genericInfo->genericSecretLength ) );
	}

/****************************************************************************
*																			*
*							Key Derivation Functions						*
*																			*
****************************************************************************/

/* Derive a key into a context from a user-supplied keying value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deriveKey( INOUT CONTEXT_INFO *contextInfoPtr, 
			   IN_BUFFER( keyValueLen ) const void *keyValue, 
			   IN_LENGTH_SHORT const int keyValueLen )
	{
	static const MAP_TABLE mapTbl[] = {
		{ CRYPT_ALGO_SHA1, CRYPT_ALGO_HMAC_SHA1 },
		{ CRYPT_ALGO_SHA2, CRYPT_ALGO_HMAC_SHA2 },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	const CAPABILITY_INFO *capabilityInfoPtr = \
						DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const CTX_LOADKEY_FUNCTION loadKeyFunction = \
						( CTX_LOADKEY_FUNCTION ) \
						FNPTR_GET( contextInfoPtr->loadKeyFunction );
	MECHANISM_DERIVE_INFO mechanismInfo;
	int hmacAlgo = ( contextInfoPtr->type == CONTEXT_CONV ) ? \
					 contextInfoPtr->ctxConv->keySetupAlgorithm : \
					 contextInfoPtr->ctxMAC->keySetupAlgorithm;
	int value DUMMY_INIT, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( keyValue, keyValueLen ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( contextInfoPtr->type == CONTEXT_CONV || \
			  contextInfoPtr->type == CONTEXT_MAC );
	REQUIRES( needsKey( contextInfoPtr ) );
	REQUIRES( isShortIntegerRangeNZ( keyValueLen ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( loadKeyFunction != NULL );

	/* If it's a persistent context we need to have a key label set before 
	   we can continue */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_PERSISTENT ) && \
		contextInfoPtr->labelSize <= 0 )
		return( CRYPT_ERROR_NOTINITED );

	/* Set up various derivation parameters if they're not already set */
	if( hmacAlgo == CRYPT_ALGO_NONE )
		{
		status = krnlSendMessage( contextInfoPtr->ownerHandle, 
								  IMESSAGE_GETATTRIBUTE, &hmacAlgo, 
								  CRYPT_OPTION_ENCR_HASH );
		if( cryptStatusOK( status ) )
			{
			status = mapValue( hmacAlgo, &value, mapTbl, 
							   FAILSAFE_ARRAYSIZE( mapTbl, MAP_TABLE ) );
			}
		if( cryptStatusError( status ) )
			return( status );
		hmacAlgo = value;
		}
	if( contextInfoPtr->type == CONTEXT_CONV )
		{
		CONV_INFO *convInfo = contextInfoPtr->ctxConv;
		int keySize;

		keySize = ( convInfo->userKeyLength > 0 ) ? \
				  convInfo->userKeyLength : capabilityInfoPtr->keySize;
		if( convInfo->saltLength <= 0 )
			{
			MESSAGE_DATA nonceMsgData;

			setMessageData( &nonceMsgData, convInfo->salt, PKCS5_SALT_SIZE );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_GETATTRIBUTE_S, &nonceMsgData,
									  CRYPT_IATTRIBUTE_RANDOM_NONCE );
			if( cryptStatusError( status ) )
				return( status );
			convInfo->saltLength = PKCS5_SALT_SIZE;
			}
		convInfo->keySetupAlgorithm = hmacAlgo;
		setMechanismDeriveInfo( &mechanismInfo, convInfo->userKey, keySize,
								keyValue, keyValueLen, 
								convInfo->keySetupAlgorithm, 
								convInfo->salt, convInfo->saltLength, 
								convInfo->keySetupIterations );
		if( mechanismInfo.iterations <= 0 )
			{
			status = krnlSendMessage( contextInfoPtr->ownerHandle, 
									  IMESSAGE_GETATTRIBUTE, 
									  &mechanismInfo.iterations, 
									  CRYPT_OPTION_KEYING_ITERATIONS );
			if( cryptStatusError( status ) )
				return( status );
			convInfo->keySetupIterations = mechanismInfo.iterations;
			}
		}
	else
		{
		MAC_INFO *macInfo = contextInfoPtr->ctxMAC;
		int keySize;

		keySize = ( macInfo->userKeyLength > 0 ) ? \
				  macInfo->userKeyLength : capabilityInfoPtr->keySize;
		if( macInfo->saltLength <= 0 )
			{
			MESSAGE_DATA nonceMsgData;

			setMessageData( &nonceMsgData, macInfo->salt, PKCS5_SALT_SIZE );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_GETATTRIBUTE_S, &nonceMsgData,
									  CRYPT_IATTRIBUTE_RANDOM_NONCE );
			if( cryptStatusError( status ) )
				return( status );
			macInfo->saltLength = PKCS5_SALT_SIZE;
			}
		macInfo->keySetupAlgorithm = hmacAlgo;
		setMechanismDeriveInfo( &mechanismInfo, macInfo->userKey, keySize,
								keyValue, keyValueLen, 
								macInfo->keySetupAlgorithm, 
								macInfo->salt, macInfo->saltLength,
								macInfo->keySetupIterations );
		if( mechanismInfo.iterations <= 0 )
			{
			status = krnlSendMessage( contextInfoPtr->ownerHandle, 
									  IMESSAGE_GETATTRIBUTE, 
									  &mechanismInfo.iterations, 
									  CRYPT_OPTION_KEYING_ITERATIONS );
			if( cryptStatusError( status ) )
				return( status );
			macInfo->keySetupIterations = mechanismInfo.iterations;
			}
		}

	/* Turn the user key into an encryption context key and load the key 
	   into the context */
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE, 
							  &mechanismInfo, MECHANISM_DERIVE_PBKDF2 );
	if( cryptStatusOK( status ) )
		{
		if( contextInfoPtr->type == CONTEXT_CONV )
			contextInfoPtr->ctxConv->userKeyLength = mechanismInfo.dataOutLength;
		else
			contextInfoPtr->ctxMAC->userKeyLength = mechanismInfo.dataOutLength;
		status = loadKeyFunction( contextInfoPtr, mechanismInfo.dataOut,
								  mechanismInfo.dataOutLength );
		}
	if( cryptStatusOK( status ) )
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_KEY_SET );
	zeroise( &mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) );

	return( status );
	}

/****************************************************************************
*																			*
*							Context Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyHandling( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_V( sanityCheckContext( contextInfoPtr ) );

	/* Set the access method pointers */
	switch( contextInfoPtr->type )
		{
		case CONTEXT_CONV:
			FNPTR_SET( contextInfoPtr->loadKeyFunction, loadKeyConvFunction );
			FNPTR_SET( contextInfoPtr->generateKeyFunction, generateKeyConvFunction );
			break;

		case CONTEXT_PKC:
			FNPTR_SET( contextInfoPtr->loadKeyFunction, loadKeyPKCFunction );
			FNPTR_SET( contextInfoPtr->generateKeyFunction, generateKeyPKCFunction );
			break;

		case CONTEXT_MAC:
			FNPTR_SET( contextInfoPtr->loadKeyFunction, loadKeyMacFunction );
			FNPTR_SET( contextInfoPtr->generateKeyFunction, generateKeyMacFunction );
			break;

		case CONTEXT_GENERIC:
			FNPTR_SET( contextInfoPtr->loadKeyFunction, loadKeyGenericFunction );
			FNPTR_SET( contextInfoPtr->generateKeyFunction, generateKeyGenericFunction );
			break;

		default:
			retIntError_Void();
		}
	}
