/****************************************************************************
*																			*
*				cryptlib Encryption Context Attribute Routines				*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#include "crypt.h"
#ifdef INC_ALL
  #include "context.h"
  #include "asn1.h"
#else
  #include "context/context.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Exit after setting extended error information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitError( INOUT CONTEXT_INFO *contextInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus,
					  IN_ENUM( CRYPT_ERRTYPE ) const CRYPT_ERRTYPE_TYPE errorType, 
					  IN_ERROR const int status )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );
	REQUIRES( errorType > CRYPT_ERRTYPE_NONE && \
			  errorType < CRYPT_ERRTYPE_LAST );
	REQUIRES( cryptStatusError( status ) );

	setErrorInfo( contextInfoPtr, errorLocus, errorType );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorInited( INOUT CONTEXT_INFO *contextInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( contextInfoPtr, errorLocus, 
					   CRYPT_ERRTYPE_ATTR_PRESENT, CRYPT_ERROR_INITED ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorNotInited( INOUT CONTEXT_INFO *contextInfoPtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( contextInfoPtr, errorLocus, 
					   CRYPT_ERRTYPE_ATTR_ABSENT, CRYPT_ERROR_NOTINITED ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorNotFound( INOUT CONTEXT_INFO *contextInfoPtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( contextInfoPtr, errorLocus, 
					   CRYPT_ERRTYPE_ATTR_ABSENT, CRYPT_ERROR_NOTFOUND ) );
	}

/****************************************************************************
*																			*
*								Get Attributes								*
*																			*
****************************************************************************/

/* Get a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
						 OUT_INT_Z int *valuePtr, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const CONTEXT_TYPE contextType = contextInfoPtr->type;
	int value;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( valuePtr, sizeof( int ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Clear return value */
	*valuePtr = 0;

	switch( attribute )
		{
		case CRYPT_ATTRIBUTE_ERRORTYPE:
			*valuePtr = contextInfoPtr->errorType;
			return( CRYPT_OK );

		case CRYPT_ATTRIBUTE_ERRORLOCUS:
			*valuePtr = contextInfoPtr->errorLocus;
			return( CRYPT_OK );

		case CRYPT_OPTION_MISC_SIDECHANNELPROTECTION:
			*valuePtr = ( contextInfoPtr->flags & \
						  CONTEXT_FLAG_SIDECHANNELPROTECTION ) ? 1 : 0;
						/* This is actually a protection level rather than a 
						   boolean value, although internally it's stored as
						   a boolean flag */
			return( CRYPT_OK );

		case CRYPT_CTXINFO_ALGO:
			*valuePtr = capabilityInfoPtr->cryptAlgo;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_MODE:
			REQUIRES( contextType == CONTEXT_CONV );

			*valuePtr = contextInfoPtr->ctxConv->mode;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYSIZE:
			switch( contextType )
				{
				case CONTEXT_CONV:
					value = contextInfoPtr->ctxConv->userKeyLength;
					break;

				case CONTEXT_PKC:
					value = bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits );
					break;

				case CONTEXT_MAC:
					value = contextInfoPtr->ctxMAC->userKeyLength;
					break;

				case CONTEXT_GENERIC:
					value = contextInfoPtr->ctxGeneric->genericSecretLength;
					break;

				default:
					retIntError();
				}
			if( value <= 0 )
				{
				/* If a key hasn't been loaded yet then we return the 
				   default key size */
				value = capabilityInfoPtr->keySize;
				}
			*valuePtr = value;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_BLOCKSIZE:
			if( contextType == CONTEXT_CONV && \
				( contextInfoPtr->ctxConv->mode == CRYPT_MODE_CFB || \
				  contextInfoPtr->ctxConv->mode == CRYPT_MODE_OFB ) )
				*valuePtr = 1;	/* Block cipher in stream mode */
			else
				*valuePtr = capabilityInfoPtr->blockSize;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_IVSIZE:
			REQUIRES( contextType == CONTEXT_CONV );

			if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
				isStreamCipher( capabilityInfoPtr->cryptAlgo ) )
				return( CRYPT_ERROR_NOTAVAIL );
			*valuePtr = capabilityInfoPtr->blockSize;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYING_ALGO:
		case CRYPT_OPTION_KEYING_ALGO:
			switch( contextType )
				{
				case CONTEXT_CONV:
					value = contextInfoPtr->ctxConv->keySetupAlgorithm;
					break;

				case CONTEXT_MAC:
					value = contextInfoPtr->ctxMAC->keySetupAlgorithm;
					break;

				default:
					retIntError();
				}
			if( value <= 0 )
				return( exitErrorNotInited( contextInfoPtr,
											CRYPT_CTXINFO_KEYING_ALGO ) );
			*valuePtr = value;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYING_ITERATIONS:
		case CRYPT_OPTION_KEYING_ITERATIONS:
			switch( contextType )
				{
				case CONTEXT_CONV:
					value = contextInfoPtr->ctxConv->keySetupIterations;
					break;

				case CONTEXT_MAC:
					value = contextInfoPtr->ctxMAC->keySetupIterations;
					break;

				default:
					retIntError();
				}
			if( value <= 0 )
				return( exitErrorNotInited( contextInfoPtr,
											CRYPT_CTXINFO_KEYING_ITERATIONS ) );
			*valuePtr = value;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_PERSISTENT:
			*valuePtr = ( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ? \
						TRUE : FALSE;
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_KEYFEATURES:
			REQUIRES( contextType == CONTEXT_PKC );

			*valuePtr = ( contextInfoPtr->flags & CONTEXT_FLAG_PBO ) ? 1 : 0;
#ifdef USE_DEVICES
			*valuePtr |= isHandleRangeValid( contextInfoPtr->deviceObject ) ? 2 : 0;
#endif /* USE_DEVICES */
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_DEVICEOBJECT:
#ifdef USE_DEVICES
			if( isHandleRangeValid( contextInfoPtr->deviceObject ) )
				{
				*valuePtr = contextInfoPtr->deviceObject;
				return( CRYPT_OK );
				}
#endif /* USE_DEVICES */
			return( CRYPT_ERROR_NOTFOUND );
		}

	retIntError();
	}

/* Get a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getContextAttributeS( INOUT CONTEXT_INFO *contextInfoPtr,
						  INOUT MESSAGE_DATA *msgData, 
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const CONTEXT_TYPE contextType = contextInfoPtr->type;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( msgData, sizeof( MESSAGE_DATA ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_CTXINFO_NAME_ALGO:
			return( attributeCopy( msgData, capabilityInfoPtr->algoName,
								   capabilityInfoPtr->algoNameLen ) );

		case CRYPT_CTXINFO_NAME_MODE:
			REQUIRES( contextType == CONTEXT_CONV );

			switch( contextInfoPtr->ctxConv->mode )
				{
				case CRYPT_MODE_ECB:
					return( attributeCopy( msgData, "ECB", 3 ) );
				case CRYPT_MODE_CBC:
					return( attributeCopy( msgData, "CBC", 3 ) );
				case CRYPT_MODE_CFB:
					return( attributeCopy( msgData, "CFB", 3 ) );
				case CRYPT_MODE_OFB:
					return( attributeCopy( msgData, "OFB", 3 ) );
				}
			retIntError();

		case CRYPT_CTXINFO_KEYING_SALT:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			if( contextType == CONTEXT_CONV )
				{
				if( contextInfoPtr->ctxConv->saltLength <= 0 )
					return( exitErrorInited( contextInfoPtr,
											 CRYPT_CTXINFO_KEYING_SALT ) );
				return( attributeCopy( msgData, contextInfoPtr->ctxConv->salt,
									   contextInfoPtr->ctxConv->saltLength ) );
				}
			if( contextInfoPtr->ctxMAC->saltLength <= 0 )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_KEYING_SALT ) );
			return( attributeCopy( msgData, contextInfoPtr->ctxMAC->salt,
								   contextInfoPtr->ctxMAC->saltLength ) );

		case CRYPT_CTXINFO_IV:
			REQUIRES( contextType == CONTEXT_CONV );

			if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
				isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
				return( CRYPT_ERROR_NOTAVAIL );
			if( !( contextInfoPtr->flags & CONTEXT_FLAG_IV_SET ) )
				return( exitErrorNotInited( contextInfoPtr, CRYPT_CTXINFO_IV ) );
			return( attributeCopy( msgData, contextInfoPtr->ctxConv->iv,
								   contextInfoPtr->ctxConv->ivLength ) );

		case CRYPT_CTXINFO_HASHVALUE:
			REQUIRES( contextType == CONTEXT_HASH || \
					  contextType == CONTEXT_MAC );

			if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_INITED ) )
				return( CRYPT_ERROR_NOTINITED );
			if( !( contextInfoPtr->flags & CONTEXT_FLAG_HASH_DONE ) )
				return( CRYPT_ERROR_INCOMPLETE );
			return( attributeCopy( msgData, ( contextType == CONTEXT_HASH ) ? \
										contextInfoPtr->ctxHash->hash : \
										contextInfoPtr->ctxMAC->mac,
								   capabilityInfoPtr->blockSize ) );

		case CRYPT_CTXINFO_LABEL:
			if( contextInfoPtr->labelSize <= 0 )
				return( exitErrorNotInited( contextInfoPtr,
											CRYPT_CTXINFO_LABEL ) );
			return( attributeCopy( msgData, contextInfoPtr->label,
								   contextInfoPtr->labelSize ) );

		case CRYPT_IATTRIBUTE_KEYID:
			REQUIRES( contextType == CONTEXT_PKC );
			REQUIRES( memcmp( contextInfoPtr->ctxPKC->keyID, 
							  "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) );

			return( attributeCopy( msgData, contextInfoPtr->ctxPKC->keyID,
								   KEYID_SIZE ) );

		case CRYPT_IATTRIBUTE_KEYID_PGP2:
			REQUIRES( contextType == CONTEXT_PKC );

			if( !( contextInfoPtr->flags & CONTEXT_FLAG_PGPKEYID_SET ) )
				return( CRYPT_ERROR_NOTFOUND );
			return( attributeCopy( msgData, contextInfoPtr->ctxPKC->pgp2KeyID,
								   PGP_KEYID_SIZE ) );

		case CRYPT_IATTRIBUTE_KEYID_OPENPGP:
			REQUIRES( contextType == CONTEXT_PKC );

			if( !( contextInfoPtr->flags & CONTEXT_FLAG_OPENPGPKEYID_SET ) )
				return( CRYPT_ERROR_NOTFOUND );
			return( attributeCopy( msgData, contextInfoPtr->ctxPKC->openPgpKeyID,
								   PGP_KEYID_SIZE ) );

		case CRYPT_IATTRIBUTE_KEY_SPKI:
		case CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL:
			/* CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL is used to read from dummy
			   contexts used as placeholders for external crypto hardware
			   functionality, these aren't necessarily in the high state as
			   required by a CRYPT_IATTRIBUTE_KEY_SPKI read when they're
			   accessed because the hardware may not be ready yet but we 
			   can still fetch the stored public-key data from them */
			REQUIRES( contextType == CONTEXT_PKC && \
					  !needsKey( contextInfoPtr ) );

			if( contextInfoPtr->ctxPKC->publicKeyInfo != NULL )
				{
				/* If the data is available in pre-encoded form, copy it
				   out */
				return( attributeCopy( msgData, contextInfoPtr->ctxPKC->publicKeyInfo,
									   contextInfoPtr->ctxPKC->publicKeyInfoSize ) );
				}
			ENSURES( attribute == CRYPT_IATTRIBUTE_KEY_SPKI );
			/* Drop through */

		case CRYPT_IATTRIBUTE_KEY_PGP:
		case CRYPT_IATTRIBUTE_KEY_SSH:
		case CRYPT_IATTRIBUTE_KEY_SSH1:
		case CRYPT_IATTRIBUTE_KEY_SSL:
			{
			STREAM stream;
			KEYFORMAT_TYPE formatType;

			REQUIRES( contextType == CONTEXT_PKC && \
					  !needsKey( contextInfoPtr ) );

			/* Write the appropriately-formatted key data from the context */
			status = attributeToFormatType( attribute, &formatType );
			if( cryptStatusError( status ) )
				return( status );
			sMemOpenOpt( &stream, msgData->data, msgData->length );
			status = contextInfoPtr->ctxPKC->writePublicKeyFunction( &stream,
												contextInfoPtr, formatType, 
												"public_key", 10 );
			if( cryptStatusOK( status ) )
				msgData->length = stell( &stream );
			sMemDisconnect( &stream );
			return( status );
			}

		case CRYPT_IATTRIBUTE_PGPVALIDITY:
			REQUIRES( contextType == CONTEXT_PKC );

			*( ( time_t * ) msgData->data ) = \
									contextInfoPtr->ctxPKC->pgpCreationTime;
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_DEVICESTORAGEID:
#ifdef USE_HARDWARE
			if( contextInfoPtr->deviceStorageIDset )
				return( attributeCopy( msgData, contextInfoPtr->deviceStorageID, 
									   KEYID_SIZE ) );
#endif /* USE_HARDWARE */
			return( CRYPT_ERROR_NOTFOUND );

		case CRYPT_IATTRIBUTE_ENCPARAMS:
			REQUIRES( contextType == CONTEXT_GENERIC );

			if( contextInfoPtr->ctxGeneric->encAlgoParamSize <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			return( attributeCopy( msgData, 
								   contextInfoPtr->ctxGeneric->encAlgoParams, 
								   contextInfoPtr->ctxGeneric->encAlgoParamSize ) );

		case CRYPT_IATTRIBUTE_MACPARAMS:
			REQUIRES( contextType == CONTEXT_GENERIC );

			if( contextInfoPtr->ctxGeneric->macAlgoParamSize <= 0 )
				return( CRYPT_ERROR_NOTFOUND );
			return( attributeCopy( msgData, 
								   contextInfoPtr->ctxGeneric->macAlgoParams, 
								   contextInfoPtr->ctxGeneric->macAlgoParamSize ) );

		case CRYPT_IATTRIBUTE_ICV:
			REQUIRES( contextType == CONTEXT_CONV );

			if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_GCM )
				return( CRYPT_ERROR_NOTAVAIL );
			return( capabilityInfoPtr->getInfoFunction( CAPABILITY_INFO_ICV, 
											contextInfoPtr, msgData->data,
											msgData->length ) );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*								Set Attributes								*
*																			*
****************************************************************************/

/* Set a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
						 IN_INT_Z const int value, 
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const CONTEXT_TYPE contextType = contextInfoPtr->type;
	int *valuePtr;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( ( value >= 0 && value < MAX_INTLENGTH ) || \
			  attribute == CRYPT_IATTRIBUTE_DEVICEOBJECT );
			  /* Some PKCS #11 device handles are most likely disguised
				 pointers so we have to allow for outrageous values for
				 these */
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_OPTION_MISC_SIDECHANNELPROTECTION:
			if( value > 0 )
				contextInfoPtr->flags |= CONTEXT_FLAG_SIDECHANNELPROTECTION;
			else
				contextInfoPtr->flags &= ~CONTEXT_FLAG_SIDECHANNELPROTECTION;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_MODE:
			REQUIRES( contextType == CONTEXT_CONV );

			/* If the mode for the context isn't set to the initial default 
			   value, it's already been explicitly set and we can't change 
			   it again.  This isn't quite as straightforward as it seems
			   because the definition of "initial default mode" is a bit
			   vague, for stream ciphers it's OFB, for block ciphers it's
			   usually CBC unless we're working with specific hardware 
			   crypto that only supports one mode and that mode isn't CBC.
			   For now this only seems to be ECB so we add a special-case
			   check for ECB-only operation */
			if( isStreamCipher( capabilityInfoPtr->cryptAlgo ) )
				{
				/* It's a stream cipher, the only possible mode is an
				   implicit OFB so any attempt to change it isn't valid */
				return( exitErrorInited( contextInfoPtr, 
										 CRYPT_CTXINFO_MODE ) );
				}
			else
				{
#if 1			/* So far no devices without CBC support have been 
				   encountered, so we always assume a default of CBC */
				if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_CBC )
					return( exitErrorInited( contextInfoPtr, 
											 CRYPT_CTXINFO_MODE ) );
#else
				if( capabilityInfoPtr->encryptCBCFunction != NULL )
					{
					if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_CBC )
						return( exitErrorInited( contextInfoPtr, 
												 CRYPT_CTXINFO_MODE ) );
					}
				else
					{
					/* This algorithm isn't available in CBC mode, the 
					   default will be ECB */
					if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_ECB )
						return( exitErrorInited( contextInfoPtr, 
												 CRYPT_CTXINFO_MODE ) );
					}
#endif
				}

			/* Set the en/decryption mode */
			return( capabilityInfoPtr->initParamsFunction( contextInfoPtr,
											KEYPARAM_MODE, NULL, value ) );

		case CRYPT_CTXINFO_KEYSIZE:
			/* Make sure that the requested size is within range */
			if( value < capabilityInfoPtr->minKeySize || \
				value > capabilityInfoPtr->maxKeySize )
				return( CRYPT_ARGERROR_NUM1 );

			/* Get the location to store the key size and make sure that 
			   it's not already set */
			switch( contextType )
				{
				case CONTEXT_CONV:
					valuePtr = &contextInfoPtr->ctxConv->userKeyLength;
					break;

				case CONTEXT_PKC:
					valuePtr = &contextInfoPtr->ctxPKC->keySizeBits;
					break;

				case CONTEXT_MAC:
					valuePtr = &contextInfoPtr->ctxMAC->userKeyLength;
					break;

				case CONTEXT_GENERIC:
					valuePtr = &contextInfoPtr->ctxGeneric->genericSecretLength;
					break;

				default:
					retIntError();
				}
			if( *valuePtr != 0 )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_KEYSIZE ) );

			/* Trim the user-supplied value to the correct shape, taking
			   into account various issues such as limitations with the
			   underlying crypto code/hardware and interop problems with 
			   algorithms that allow excessively long keys.  In virtute sunt 
			   multi ascensus.

			   If it's a PKC key then there's nothing further to do, since 
			   the range check above is all that we need.  ECC keys are a 
			   bit complicated because if we're using fixed curve parameters 
			   (which in practice we always do) there are only a small 
			   number of quantised key sizes that we can use, but since some 
			   of those correspond to very odd bit sizes like 521 bits that 
			   can't be specified as an integral byte count what we do in 
			   the ECC code is use the nearest fixed curve parameters equal 
			   to or above the given value, avoiding the need for the caller 
			   to play guessing games as to which byte-count value 
			   corresponds to which curve.
			   
			   For conventional/MAC keys we need to limit the maximum 
			   working key length to a sane size since the other side may 
			   not be able to handle stupidly large keys */
			if( contextType == CONTEXT_PKC )
				*valuePtr = bytesToBits( value );
			else
				*valuePtr = min( value, MAX_WORKING_KEYSIZE );
			return( CRYPT_OK );

		case CRYPT_CTXINFO_BLOCKSIZE:
			REQUIRES( contextType == CONTEXT_HASH || \
					  contextType == CONTEXT_MAC );

			/* Some hash (and corresponding MAC) algorithms have variable-
			   length outputs, in which case the blocksize is user-
			   definable */
			if( capabilityInfoPtr->initParamsFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			return( capabilityInfoPtr->initParamsFunction( contextInfoPtr,
										KEYPARAM_BLOCKSIZE, NULL, value ) );

		case CRYPT_CTXINFO_KEYING_ALGO:
		case CRYPT_OPTION_KEYING_ALGO:
			{
			CRYPT_ALGO_TYPE *algoValuePtr;

			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			/* The kernel only allows (potentially) valid values to be set,
			   but these may be disabled at the algorithm level so we have 
			   to perform an additional check here */
			if( !algoAvailable( value ) )
				{
				return( exitError( contextInfoPtr, attribute,
								   CRYPT_ERRTYPE_ATTR_VALUE, 
								   CRYPT_ERROR_NOTAVAIL ) );
				}

			algoValuePtr = ( contextType == CONTEXT_CONV ) ? \
						   &contextInfoPtr->ctxConv->keySetupAlgorithm : \
						   &contextInfoPtr->ctxMAC->keySetupAlgorithm;
			if( *algoValuePtr != CRYPT_ALGO_NONE )
				return( exitErrorInited( contextInfoPtr, attribute ) );
			*algoValuePtr = value;
			return( CRYPT_OK );
			}

		case CRYPT_CTXINFO_KEYING_ITERATIONS:
		case CRYPT_OPTION_KEYING_ITERATIONS:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			valuePtr = ( contextType == CONTEXT_CONV ) ? \
					   &contextInfoPtr->ctxConv->keySetupIterations : \
					   &contextInfoPtr->ctxMAC->keySetupIterations;
			if( *valuePtr )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_KEYING_ITERATIONS ) );
			*valuePtr = value;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_PERSISTENT:
			/* The is-object-persistent attribute functions as follows:

					 | Software	| Hardware
				-----+----------+-------------------
				PKC	 | R/O (1)	| R/O (2)
				-----+----------+-------------------
				Conv | R/O (1)	| R/W low state (3)
					 |			| R/O high state

			   (1) = Set if stored to or read from a keyset.
			   (2) = Always set.  Private-key objects are automatically
					 created as persistent objects, public-key objects
					 are (transparently) created as software objects since
					 the operation is much faster on the host system than
					 by going via the device.
			   (3) = Can be set in the low state, if set then the object
					 is created as a persistent object in the device.

			   Most of these checks are enforced by the kernel, the one 
			   thing that the ACL language can't specify is the requirement 
			   that a persistent conventional-encryption object be tied to
			   a device, which we do explicitly here */
			if( ( value != 0 ) && \
				!( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) )
				return( CRYPT_ERROR_PERMISSION );

			/* Set or clear the persistent flag as required */
			if( value != 0 )
				contextInfoPtr->flags |= CONTEXT_FLAG_PERSISTENT;
			else
				contextInfoPtr->flags &= ~CONTEXT_FLAG_PERSISTENT;
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_KEYSIZE:
			/* If it's a private key context or a persistent context we need 
			   to have a key label set before we can continue */
			if( ( ( contextInfoPtr->type == CONTEXT_PKC ) || \
				  ( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) ) && \
				contextInfoPtr->labelSize <= 0 )
				retIntError();

			/* If the key is held outside the context (e.g. in a device) we
			   may need to manually supply the key-related information 
			   needed by the context if it can't be obtained through other
			   means such as when the SubjectPublicKeyInfo is set */
			switch( contextType )
				{
				case CONTEXT_CONV:
					contextInfoPtr->ctxConv->userKeyLength = value;
					break;

				case CONTEXT_PKC:
					contextInfoPtr->ctxPKC->keySizeBits = bytesToBits( value );
					break;

				case CONTEXT_MAC:
					contextInfoPtr->ctxMAC->userKeyLength = value;
					break;

				case CONTEXT_GENERIC:
					contextInfoPtr->ctxGeneric->genericSecretLength = value;
					break;

				default:
					retIntError();
				}
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_DEVICEOBJECT:
#ifdef USE_DEVICES
			/* Setting the device object means that the crypto functionality 
			   for the context is enabled, which means that it's effectively 
			   in the key-loaded state, however for standard key-loaded
			   operations to be possible certain other preconditions need to 
			   be met, which we check for here */
			REQUIRES( ( contextType == CONTEXT_CONV && \
						contextInfoPtr->ctxConv->userKeyLength > 0 ) || \
					  ( contextType == CONTEXT_PKC && \
						contextInfoPtr->ctxPKC->keySizeBits > 0 && \
						contextInfoPtr->ctxPKC->publicKeyInfo != NULL ) || \
					  ( contextType == CONTEXT_MAC && \
						contextInfoPtr->ctxMAC->userKeyLength > 0 ) || \
					  ( contextType == CONTEXT_GENERIC && \
						contextInfoPtr->ctxGeneric->genericSecretLength > 0 ) || \
					  ( contextType == CONTEXT_HASH ) );

			/* Remember the reference to the associated crypto functionality
			   in the device */
			contextInfoPtr->deviceObject = value;
			contextInfoPtr->flags |= CONTEXT_FLAG_KEY_SET;
#endif /* USE_DEVICES */
			return( CRYPT_OK );
		}

	retIntError();
	}

/* Set a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setContextAttributeS( INOUT CONTEXT_INFO *contextInfoPtr,
						  IN_BUFFER( dataLength ) const void *data,
						  IN_LENGTH const int dataLength,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = contextInfoPtr->capabilityInfo;
	const CONTEXT_TYPE contextType = contextInfoPtr->type;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_CTXINFO_KEYING_SALT:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );
			REQUIRES( dataLength > 0 && dataLength <= CRYPT_MAX_HASHSIZE );

			if( contextType == CONTEXT_CONV )
				{
				if( contextInfoPtr->ctxConv->saltLength > 0 )
					return( exitErrorInited( contextInfoPtr,
											 CRYPT_CTXINFO_KEYING_SALT ) );
				memcpy( contextInfoPtr->ctxConv->salt, data, dataLength );
				contextInfoPtr->ctxConv->saltLength = dataLength;
				return( CRYPT_OK );
				}
			if( contextInfoPtr->ctxMAC->saltLength > 0 )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_KEYING_SALT ) );
			memcpy( contextInfoPtr->ctxMAC->salt, data, dataLength );
			contextInfoPtr->ctxMAC->saltLength = dataLength;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYING_VALUE:
			return( deriveKey( contextInfoPtr, data, dataLength ) );

		case CRYPT_CTXINFO_KEY:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC || \
					  contextType == CONTEXT_GENERIC );
			REQUIRES( needsKey( contextInfoPtr ) );

			/* If it's a persistent context we need to have a key label set 
			   before we can continue */
			if( ( contextInfoPtr->flags & CONTEXT_FLAG_PERSISTENT ) && \
				contextInfoPtr->labelSize <= 0 )
				return( exitErrorNotInited( contextInfoPtr, 
											CRYPT_CTXINFO_LABEL ) );

			/* The kernel performs a general check on the size of this
			   attribute but doesn't know about context subtype-specific
			   limits so we perform a context-specific check here */
			if( dataLength < capabilityInfoPtr->minKeySize || \
				dataLength > capabilityInfoPtr->maxKeySize )
				return( CRYPT_ARGERROR_NUM1 );

			/* Load the key into the context */
			status = contextInfoPtr->loadKeyFunction( contextInfoPtr,
													  data, dataLength );
			if( cryptStatusOK( status ) )
				contextInfoPtr->flags |= CONTEXT_FLAG_KEY_SET;
			return( status );

#ifndef USE_FIPS140
		case CRYPT_CTXINFO_KEY_COMPONENTS:
			return( setKeyComponents( contextInfoPtr, data, dataLength ) );
#endif /* !USE_FIPS140 */

		case CRYPT_CTXINFO_IV:
			REQUIRES( contextType == CONTEXT_CONV );

			/* If it's a mode that doesn't use an IV then the load IV 
			   operation is meaningless */
			if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
				isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
				return( CRYPT_ERROR_NOTAVAIL );

			/* Make sure that the data size is valid.  GCM is handled 
			   specially because the default IV size is somewhat smaller 
			   than the cipher block size */
			if( contextInfoPtr->ctxConv->mode == CRYPT_MODE_GCM )
				{
				if( dataLength < 8 || \
					dataLength > capabilityInfoPtr->blockSize )
					return( CRYPT_ARGERROR_NUM1 );
				}
			else
				{
				if( dataLength != capabilityInfoPtr->blockSize )
					return( CRYPT_ARGERROR_NUM1 );
				}

			/* Load the IV */
			return( capabilityInfoPtr->initParamsFunction( contextInfoPtr,
										KEYPARAM_IV, data, dataLength ) );

		case CRYPT_CTXINFO_LABEL:
			{
			CRYPT_HANDLE iCryptHandle;

			if( contextInfoPtr->labelSize > 0 )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_LABEL ) );

			/* Check any device object that the context is associated with 
			   to ensure that nothing with that label already exists in the
			   device.  For keysets the check for duplicates is performed 
			   when the context is explicitly added to the keyset but with 
			   devices the context will be implicitly created within the 
			   device at some future point (at context creation, on key 
			   load/generation, or at some other point) that depends on the 
			   device.  Because of this we perform a preemptive check for 
			   duplicates to avoid a potentially confusing error condition 
			   at some indeterminate point in the future.  
			   
			   Since objects are typed, we have to check for the three 
			   possible { label, type } combinations.  In theory we could 
			   require that labels are only unique per object type but this 
			   can lead to problems with underlying devices or keysets that 
			   only support a check by label and not by { label, type } 
			   combination.  In addition we can't send the message to the 
			   context because the kernel won't forward this message type 
			   (sending a get-key message to a context doesn't make sense) 
			   so we have to explicitly get the dependent device and then 
			   send the get-key directly to it */
			status = krnlSendMessage( contextInfoPtr->objectHandle,
									  IMESSAGE_GETDEPENDENT, &iCryptHandle, 
									  OBJECT_TYPE_DEVICE );
			if( cryptStatusOK( status ) && \
				( iCryptHandle != SYSTEM_OBJECT_HANDLE ) )
				{
				MESSAGE_KEYMGMT_INFO getkeyInfo;

				setMessageKeymgmtInfo( &getkeyInfo, CRYPT_KEYID_NAME, 
									   data, dataLength, NULL, 0, 
									   KEYMGMT_FLAG_CHECK_ONLY );
				status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_GETKEY, 
										  &getkeyInfo, KEYMGMT_ITEM_SECRETKEY );
				if( cryptStatusError( status ) )
					status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_GETKEY, 
											  &getkeyInfo,
											  KEYMGMT_ITEM_PUBLICKEY );
				if( cryptStatusError( status ) )
					status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_GETKEY, 
											  &getkeyInfo,
											  KEYMGMT_ITEM_PRIVATEKEY );
				if( cryptStatusOK( status ) )
					{
					/* We've found something with this label already 
					   present, we can't use it again */
					return( CRYPT_ERROR_DUPLICATE );
					}
				assert( !cryptArgError( status ) );
				}
			
			/* Fall through */
			}

		case CRYPT_IATTRIBUTE_EXISTINGLABEL:
			REQUIRES( dataLength > 0 && dataLength <= CRYPT_MAX_TEXTSIZE );

			/* The difference between CRYPT_CTXINFO_LABEL and 
			   CRYPT_IATTRIBUTE_EXISTINGLABEL is that the latter is used to 
			   set a label for a context that's being instantiated from a 
			   persistent object in a device.  We can't perform the 
			   duplicate-label check in this case because we'll always get a 
			   match for the device object's label */
			if( contextInfoPtr->labelSize > 0 )
				return( exitErrorInited( contextInfoPtr,
										 CRYPT_CTXINFO_LABEL ) );

			/* Set the label */
			memcpy( contextInfoPtr->label, data, dataLength );
			contextInfoPtr->labelSize = dataLength;
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_KEYID_OPENPGP:
			REQUIRES( contextType == CONTEXT_PKC );
			REQUIRES( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA || \
					  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA || \
					  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL );
			REQUIRES( dataLength == PGP_KEYID_SIZE );

			memcpy( contextInfoPtr->ctxPKC->openPgpKeyID, data, dataLength );
			contextInfoPtr->flags |= CONTEXT_FLAG_OPENPGPKEYID_SET;

			/* If it's a non-PGP 2.x key type, set the PGP 2.x keyID to the 
			   OpenPGP keyID.  This is necessary because non-PGP 2.x keys can
			   be used with PGP 2.x message formats which would imply the use 
			   of a PGP 2.x keyID except that it's not defined for this key 
			   type */
			if( contextInfoPtr->capabilityInfo->cryptAlgo != CRYPT_ALGO_RSA )
				{
				memcpy( contextInfoPtr->ctxPKC->pgp2KeyID, 
						contextInfoPtr->ctxPKC->openPgpKeyID, PGP_KEYID_SIZE );
				contextInfoPtr->flags |= CONTEXT_FLAG_PGPKEYID_SET;
				}
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_KEY_SPKI:
		case CRYPT_IATTRIBUTE_KEY_PGP:
		case CRYPT_IATTRIBUTE_KEY_SSH:
		case CRYPT_IATTRIBUTE_KEY_SSH1:
		case CRYPT_IATTRIBUTE_KEY_SSL:
		case CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL:
		case CRYPT_IATTRIBUTE_KEY_PGP_PARTIAL:
			REQUIRES( contextType == CONTEXT_PKC );

			return( setEncodedKey( contextInfoPtr, attribute, data, 
								   dataLength ) );

		case CRYPT_IATTRIBUTE_PGPVALIDITY:
			REQUIRES( contextType == CONTEXT_PKC );

			contextInfoPtr->ctxPKC->pgpCreationTime = *( ( time_t * ) data );
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_DEVICESTORAGEID:
#ifdef USE_HARDWARE
			REQUIRES( dataLength > 0 && dataLength <= KEYID_SIZE );
			memset( contextInfoPtr->deviceStorageID, 0, KEYID_SIZE );
			memcpy( contextInfoPtr->deviceStorageID, data, dataLength );
			contextInfoPtr->deviceStorageIDset = TRUE;
#endif /* USE_HARDWARE */
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_ENCPARAMS:
			REQUIRES( dataLength > 0 && dataLength <= CRYPT_MAX_TEXTSIZE );

			memcpy( contextInfoPtr->ctxGeneric->encAlgoParams, data, 
					dataLength );
			contextInfoPtr->ctxGeneric->encAlgoParamSize = dataLength;

			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_MACPARAMS:
			REQUIRES( dataLength > 0 && dataLength <= CRYPT_MAX_TEXTSIZE );

			memcpy( contextInfoPtr->ctxGeneric->macAlgoParams, data, 
					dataLength );
			contextInfoPtr->ctxGeneric->macAlgoParamSize = dataLength;

			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_AAD:
			REQUIRES( contextType == CONTEXT_CONV );

			if( contextInfoPtr->ctxConv->mode != CRYPT_MODE_GCM )
				return( CRYPT_ERROR_NOTAVAIL );

			/* Process the AAD */
			return( capabilityInfoPtr->initParamsFunction( contextInfoPtr,
											KEYPARAM_AAD , data, dataLength ) );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*								Delete Attributes							*
*																			*
****************************************************************************/

/* Delete an attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteContextAttribute( INOUT CONTEXT_INFO *contextInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const CONTEXT_TYPE contextType = contextInfoPtr->type;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_CTXINFO_KEYING_ALGO:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			if( contextType == CONTEXT_CONV )
				{
				if( contextInfoPtr->ctxConv->keySetupAlgorithm == CRYPT_ALGO_NONE )
					return( exitErrorNotFound( contextInfoPtr,
											   CRYPT_CTXINFO_KEYING_ALGO ) );
				contextInfoPtr->ctxConv->keySetupAlgorithm = CRYPT_ALGO_NONE;
				return( CRYPT_OK );
				}
			if( contextInfoPtr->ctxMAC->keySetupAlgorithm == CRYPT_ALGO_NONE )
				return( exitErrorNotFound( contextInfoPtr,
										   CRYPT_CTXINFO_KEYING_ALGO ) );
			contextInfoPtr->ctxMAC->keySetupAlgorithm = CRYPT_ALGO_NONE;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYING_ITERATIONS:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			if( contextType == CONTEXT_CONV )
				{
				if( contextInfoPtr->ctxConv->keySetupIterations == 0 )
					return( exitErrorNotFound( contextInfoPtr,
											   CRYPT_CTXINFO_KEYING_ITERATIONS ) );
				contextInfoPtr->ctxConv->keySetupIterations = 0;
				return( CRYPT_OK );
				}
			if( contextInfoPtr->ctxMAC->keySetupIterations == 0 )
				return( exitErrorNotFound( contextInfoPtr,
										   CRYPT_CTXINFO_KEYING_ITERATIONS ) );
			contextInfoPtr->ctxMAC->keySetupIterations = 0;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_KEYING_SALT:
			REQUIRES( contextType == CONTEXT_CONV || \
					  contextType == CONTEXT_MAC );

			if( contextType == CONTEXT_CONV )
				{
				if( contextInfoPtr->ctxConv->saltLength == 0 )
					return( exitErrorNotFound( contextInfoPtr,
											   CRYPT_CTXINFO_KEYING_SALT ) );
				zeroise( contextInfoPtr->ctxConv->salt, CRYPT_MAX_HASHSIZE );
				contextInfoPtr->ctxConv->saltLength = 0;
				return( CRYPT_OK );
				}
			if( contextInfoPtr->ctxMAC->saltLength == 0 )
				return( exitErrorNotFound( contextInfoPtr,
										   CRYPT_CTXINFO_KEYING_SALT ) );
			zeroise( contextInfoPtr->ctxMAC->salt, CRYPT_MAX_HASHSIZE );
			contextInfoPtr->ctxMAC->saltLength = 0;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_IV:
			REQUIRES( contextType == CONTEXT_CONV );

			if( !needsIV( contextInfoPtr->ctxConv->mode ) || \
				isStreamCipher( contextInfoPtr->capabilityInfo->cryptAlgo ) )
				return( exitErrorNotFound( contextInfoPtr,
										   CRYPT_CTXINFO_IV ) );
			contextInfoPtr->ctxConv->ivLength = \
					contextInfoPtr->ctxConv->ivCount = 0;
			contextInfoPtr->flags &= ~CONTEXT_FLAG_IV_SET;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_LABEL:
			if( contextInfoPtr->labelSize <= 0 )
				return( exitErrorNotFound( contextInfoPtr,
										   CRYPT_CTXINFO_LABEL ) );
			zeroise( contextInfoPtr->label, contextInfoPtr->labelSize );
			contextInfoPtr->labelSize = 0;
			return( CRYPT_OK );

		case CRYPT_CTXINFO_HASHVALUE:
			switch( contextType )
				{
				case CONTEXT_HASH:
					zeroise( contextInfoPtr->ctxHash->hash, CRYPT_MAX_HASHSIZE );
					break;

				case CONTEXT_MAC:
					zeroise( contextInfoPtr->ctxMAC->mac, CRYPT_MAX_HASHSIZE );
					break;

				default:
					retIntError();
				}
			contextInfoPtr->flags &= ~( CONTEXT_FLAG_HASH_INITED | \
										CONTEXT_FLAG_HASH_DONE );
			return( CRYPT_OK );
		}

	retIntError();
	}
