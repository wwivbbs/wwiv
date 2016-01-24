/****************************************************************************
*																			*
*						cryptlib CMS Pre-enveloping Routines				*
*					    Copyright Peter Gutmann 1996-2010					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "envelope.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "envelope/envelope.h"
#endif /* Compiler-specific includes */

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*						Encrypted Content Pre-processing					*
*																			*
****************************************************************************/

/* Create a context for a particular envelope action type */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createActionContext( INOUT ENVELOPE_INFO *envelopeInfoPtr,
								IN_ENUM( ACTION ) const ACTION_TYPE actionType,
								IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
								IN_HANDLE_OPT \
									const CRYPT_CONTEXT iMasterKeyContext )
	{
	CRYPT_CONTEXT iActionContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( actionType == ACTION_CRYPT || actionType == ACTION_MAC || \
			  actionType == ACTION_xxx );
	REQUIRES( isConvAlgo( cryptAlgo ) || isMacAlgo( cryptAlgo ) || \
			  isSpecialAlgo( cryptAlgo ) );
	REQUIRES( iMasterKeyContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iMasterKeyContext ) );

	/* Make sure that we can still add another action */
	if( !moreActionsPossible( envelopeInfoPtr->actionList ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* Create a the appropriate context type and either generate a key for 
	   it if we're using standard encryption/authentication or derive a key
	   from the supplied generic-secret context if we're using authenticated 
	   encryption */
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iActionContext = createInfo.cryptHandle;
	if( iMasterKeyContext == CRYPT_UNUSED )
		{
		/* We're using standard encryption or authentication, generate a key
		   into the context */
		status = krnlSendNotifier( iActionContext, IMESSAGE_CTX_GENKEY );
		}
	else
		{
		MECHANISM_KDF_INFO mechanismInfo;

		/* We're using authenticated encryption, derive the key for the 
		   context from the generic-secret context */
		if( actionType == ACTION_CRYPT )
			{
			setMechanismKDFInfo( &mechanismInfo, iActionContext, 
								 iMasterKeyContext, CRYPT_ALGO_HMAC_SHA1, 
								 "encryption", 10 );
			}
		else
			{
			setMechanismKDFInfo( &mechanismInfo, iActionContext, 
								 iMasterKeyContext, CRYPT_ALGO_HMAC_SHA1, 
								 "authentication", 14 );
			}
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_KDF,
								  &mechanismInfo, MECHANISM_DERIVE_PKCS5 );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iActionContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add the context to the action list */
	status = addAction( &envelopeInfoPtr->actionList,
						envelopeInfoPtr->memPoolState, actionType, 
						iActionContext );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iActionContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Create the contexts needed for the enveloping process */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createEnvelopeContexts( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr;
	int status;

	REQUIRES( envelopeInfoPtr->actionList == NULL );

	switch( envelopeInfoPtr->usage )
		{
		case ACTION_CRYPT:
			/* If we're performing straight encryption, there's only one 
			   context to create */
			if( !( envelopeInfoPtr->flags & ENVELOPE_AUTHENC ) )
				{
				return( createActionContext( envelopeInfoPtr, ACTION_CRYPT,
											 envelopeInfoPtr->defaultAlgo,
											 CRYPT_UNUSED ) );
				}

			/* We're performing authenticated encryption, we need to create 
			   a generic-secret context for the master secret and separate 
			   encryption and MAC contexts to provide the protection */
			status = createActionContext( envelopeInfoPtr, ACTION_xxx,
										  CRYPT_IALGO_GENERIC_SECRET,
										  CRYPT_UNUSED );
			if( cryptStatusError( status ) )
				return( status );
			actionListPtr = findAction( envelopeInfoPtr->actionList, 
										ACTION_xxx ); 
			REQUIRES( actionListPtr != NULL );
			status = createActionContext( envelopeInfoPtr, ACTION_CRYPT,
										  envelopeInfoPtr->defaultAlgo,
										  actionListPtr->iCryptHandle );
			if( cryptStatusOK( status ) )
				status = createActionContext( envelopeInfoPtr, ACTION_MAC,
											  envelopeInfoPtr->defaultMAC,
											  actionListPtr->iCryptHandle );
			return( status );

		case ACTION_MAC:
			return( createActionContext( envelopeInfoPtr, ACTION_MAC,
										 envelopeInfoPtr->defaultMAC,
										 CRYPT_UNUSED ) );

		default:
			retIntError();
		}

	retIntError();
	}

/* Process an individual key exchange action for teh main envelope action */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processKeyexchangeAction( INOUT ENVELOPE_INFO *envelopeInfoPtr,
									 INOUT ACTION_LIST *preActionListPtr,
									 IN_HANDLE_OPT \
										const CRYPT_DEVICE iCryptDevice )
	{
	ACTION_LIST *actionListPtr = envelopeInfoPtr->actionList;
	int keyexAlgorithm = DUMMY_INIT, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( preActionListPtr, sizeof( ACTION_LIST ) ) );
	
	REQUIRES( preActionListPtr != NULL && \
			  ( preActionListPtr->action == ACTION_KEYEXCHANGE_PKC || \
				preActionListPtr->action == ACTION_KEYEXCHANGE ) );
	REQUIRES( iCryptDevice == CRYPT_UNUSED || \
			  isHandleRangeValid( iCryptDevice ) );
	REQUIRES( actionListPtr != NULL );

	/* If the session key/MAC/generic-secret context is tied to a device 
	   make sure that the key exchange object is in the same device */
	if( iCryptDevice != CRYPT_UNUSED )
		{
		CRYPT_DEVICE iKeyexDevice;

		status = krnlSendMessage( preActionListPtr->iCryptHandle,
								  MESSAGE_GETDEPENDENT, &iKeyexDevice,
								  OBJECT_TYPE_DEVICE );
		if( cryptStatusError( status ) || iCryptDevice != iKeyexDevice )
			{
			setErrorInfo( envelopeInfoPtr, 
						  ( envelopeInfoPtr->usage == ACTION_CRYPT ) ? \
							CRYPT_ENVINFO_SESSIONKEY : CRYPT_ENVINFO_INTEGRITY,
						  CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Remember that we now have a controlling action and connect the
	   controller to the subject */
	actionListPtr->flags &= ~ACTION_NEEDSCONTROLLER;
	preActionListPtr->associatedAction = actionListPtr;

	/* Evaluate the size of the exported action.  If it's a conventional key
	   exchange we force the use of the CMS format since there's no reason 
	   to use the cryptlib format.  Note that this assumes that the first 
	   action is the one that we'll be exporting the key for, which is 
	   required for authenticated encryption where there can be multiple 
	   actions (one for encryption and one for authentication) alongside the
	   generic-secret action present */
	status = iCryptExportKey( NULL, 0, &preActionListPtr->encodedSize, 
						( preActionListPtr->action == ACTION_KEYEXCHANGE ) ? \
							CRYPT_FORMAT_CMS : envelopeInfoPtr->type,
						actionListPtr->iCryptHandle, 
						preActionListPtr->iCryptHandle );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( preActionListPtr->iCryptHandle,
								  IMESSAGE_GETATTRIBUTE, &keyexAlgorithm,
								  CRYPT_CTXINFO_ALGO );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If there are any key exchange actions that will result in indefinite-
	   length encodings present we can't use a definite-length encoding for 
	   the key exchange actions */
	return( ( isDlpAlgo( keyexAlgorithm ) || \
			  isEccAlgo( keyexAlgorithm ) ) ? OK_SPECIAL : CRYPT_OK );
	}

/* Pre-process information for encrypted enveloping */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int cmsPreEnvelopeEncrypt( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	CRYPT_DEVICE iCryptDevice = CRYPT_UNUSED;
	ACTION_LIST *actionListPtr;
	BOOLEAN hasIndefSizeActions = FALSE;
	int totalSize, iterationCount, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envelopeInfoPtr->usage == ACTION_CRYPT || \
			  envelopeInfoPtr->usage == ACTION_MAC );

	/* If there are no key exchange actions present we're done */
	if( envelopeInfoPtr->preActionList == NULL )
		return( CRYPT_OK );

	/* Create the enveloping context(s) if necessary */
	if( envelopeInfoPtr->actionList == NULL )
		{
		status = createEnvelopeContexts( envelopeInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* If the encryption/MAC context is tied to a device get its handle 
		   so that we can check that all key exchange objects are also in the 
		   same device.

		   In theory if we're using a device for our crypto and performing
		   authenticated encryption then we'd need to check that all of the 
		   generic-secret, encryption and MAC contexts are contained in the 
		   same device, however since we don't allow these to be explicitly 
		   set by the user (the encryption and MAC keys are derived from the 
		   generic-secret context so it's not possible to set a key-loaded 
		   encryption/MAC context) this can never occur */
		REQUIRES( envelopeInfoPtr->actionList->next == NULL );
		status = krnlSendMessage( envelopeInfoPtr->actionList->iCryptHandle,
								  MESSAGE_GETDEPENDENT, &iCryptDevice,
								  OBJECT_TYPE_DEVICE );
		if( cryptStatusError( status ) )
			iCryptDevice = CRYPT_UNUSED;
		}
	REQUIRES( envelopeInfoPtr->actionList != NULL );

	/* If we're performing straight encryption or MACing, notify the kernel 
	   that the encryption/MAC context is attached to the envelope.  This is 
	   an internal object used only by the envelope so we tell the kernel 
	   not to increment its reference count when it attaches it.  If we're
	   performing authenticated encryption then we can't do this because 
	   we're going via an intermediate generic-secret object from which keys 
	   will be diversified into distinct encryption and MAC objects */
	if( !( envelopeInfoPtr->usage == ACTION_CRYPT && \
		   ( envelopeInfoPtr->flags & ENVELOPE_AUTHENC ) ) )
		{
		REQUIRES( envelopeInfoPtr->actionList->next == NULL );

		status = krnlSendMessage( envelopeInfoPtr->objectHandle, 
								  IMESSAGE_SETDEPENDENT,
								  &envelopeInfoPtr->actionList->iCryptHandle,
								  SETDEP_OPTION_NOINCREF );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Now walk down the list of key exchange actions evaluating their size
	   and connecting each one to the encryption/MAC/generic-secret action */
	totalSize = 0; 
	for( actionListPtr = envelopeInfoPtr->preActionList, iterationCount = 0;
		 actionListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED; 
		 actionListPtr = actionListPtr->next, iterationCount++ )
		{
		status = processKeyexchangeAction( envelopeInfoPtr, actionListPtr,
										   iCryptDevice );
		if( cryptStatusError( status ) )
			{
			/* An OK_SPECIAL state means that this keyex action will result 
			   in an indefinite-length encoding */
			if( status != OK_SPECIAL )
				return( status );
			hasIndefSizeActions = TRUE;
			}
		totalSize += actionListPtr->encodedSize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	envelopeInfoPtr->cryptActionSize = hasIndefSizeActions ? \
									   CRYPT_UNUSED : totalSize;
	ENSURES( ( envelopeInfoPtr->cryptActionSize == CRYPT_UNUSED ) || \
			 ( envelopeInfoPtr->cryptActionSize > 0 && \
			   envelopeInfoPtr->cryptActionSize < MAX_INTLENGTH ) );

	/* If we're MACing the data (either directly or because we're performing
	   authenticated encryption), hashing is now active.  The two actions 
	   have different flags because standalone MACing hashes plaintext while 
	   MACing as part of authenticated encryption hashes ciphertext */
	if( envelopeInfoPtr->usage == ACTION_MAC )
		envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;
	if( envelopeInfoPtr->usage == ACTION_CRYPT && \
		( envelopeInfoPtr->flags & ENVELOPE_AUTHENC ) )
		envelopeInfoPtr->dataFlags |= ENVDATA_AUTHENCACTIONSACTIVE;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Signed Content Pre-processing						*
*																			*
****************************************************************************/

/* Set up signature parameters such as signature attributes and timestamps 
   if necessary */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int cmsInitSigParams( const ACTION_LIST *actionListPtr,
					  IN_ENUM( CRYPT_FORMAT ) const CRYPT_FORMAT_TYPE formatType,
					  IN_HANDLE const CRYPT_USER iCryptOwner,
					  OUT SIGPARAMS *sigParams )
	{
	const CRYPT_CERTIFICATE signingAttributes = actionListPtr->iExtraData;
	int useDefaultAttributes, status;

	REQUIRES( formatType == CRYPT_FORMAT_CRYPTLIB || \
			  formatType == CRYPT_FORMAT_CMS || \
			  formatType == CRYPT_FORMAT_SMIME );
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );

	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );
	assert( isWritePtr( sigParams, sizeof( SIGPARAMS ) ) );

	initSigParams( sigParams );

	/* If it's a raw signature there are no additional signing parameters */
	if( formatType == CRYPT_FORMAT_CRYPTLIB )
		return( CRYPT_OK );

	/* Add the timestamping session if there's one present */
	if( actionListPtr->iTspSession != CRYPT_ERROR )
		sigParams->iTspSession = actionListPtr->iTspSession;

	/* If the caller has provided signing attributes, use those */
	if( signingAttributes != CRYPT_ERROR )
		{
		sigParams->iAuthAttr = signingAttributes;
		return( CRYPT_OK );
		}

	/* There are no siging attributes explicitly specified (which can only 
	   happen under circumstances controlled by the pre-envelope signing 
	   code) in which case we either get the signing code to add the default 
	   ones for us or use none at all if the use of default attributes is 
	   disabled */
	status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE,  
							  &useDefaultAttributes,
							  CRYPT_OPTION_CMS_DEFAULTATTRIBUTES );
	if( cryptStatusError( status ) )
		return( status );
	if( useDefaultAttributes )
		sigParams->useDefaultAuthAttr = TRUE;

	return( CRYPT_OK );
	}

/* Process signing certificates and match the content-type in the 
   authenticated attributes with the signed content type if it's anything 
   other than 'data' (the data content-type is added automatically) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processSigningCerts( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							    INOUT ACTION_LIST *actionListPtr )
	{
	int contentType, dummy, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	/* If we're including signing certificates and there are multiple 
	   signing certificates present add the currently-selected one to the 
	   overall certificate collection */
	if( !( envelopeInfoPtr->flags & ENVELOPE_NOSIGNINGCERTS ) && \
		envelopeInfoPtr->iExtraCertChain != CRYPT_ERROR )
		{
		status = krnlSendMessage( envelopeInfoPtr->iExtraCertChain,
								  IMESSAGE_SETATTRIBUTE,
								  &actionListPtr->iCryptHandle,
								  CRYPT_IATTRIBUTE_CERTCOLLECTION );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's no content-type present and the signed content type isn't 
	   'data' or it's an S/MIME envelope, create signing attributes to hold 
	   the content-type and smimeCapabilities */
	if( actionListPtr->iExtraData == CRYPT_ERROR && \
		( envelopeInfoPtr->contentType != CRYPT_CONTENT_DATA || \
		  envelopeInfoPtr->type == CRYPT_FORMAT_SMIME ) )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		setMessageCreateObjectInfo( &createInfo,
									CRYPT_CERTTYPE_CMS_ATTRIBUTES );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		actionListPtr->iExtraData = createInfo.cryptHandle;
		}

	/* If there are no signed attributes, we're done */
	if( actionListPtr->iExtraData == CRYPT_ERROR )
		return( CRYPT_OK );

	/* Make sure that the content-type in the attributes matches the actual 
	   content type by deleting any existing content-type if necessary and 
	   adding our one (quietly fixing things is easier than trying to report 
	   this error back to the caller - ex duobus malis minimum eligendum 
	   est) */
	if( krnlSendMessage( actionListPtr->iExtraData, 
						 IMESSAGE_GETATTRIBUTE, &dummy, 
						 CRYPT_CERTINFO_CMS_CONTENTTYPE ) != CRYPT_ERROR_NOTFOUND )
		{
		/* There's already a content-type present, delete it so that we can 
		   add our one.  We ignore the return status from the deletion since 
		   the status from the add that follows will be more meaningful to 
		   the caller */
		( void ) krnlSendMessage( actionListPtr->iExtraData, 
								  IMESSAGE_DELETEATTRIBUTE, NULL, 
								  CRYPT_CERTINFO_CMS_CONTENTTYPE );
		}
	contentType = envelopeInfoPtr->contentType;	/* int vs.enum */
	return( krnlSendMessage( actionListPtr->iExtraData, 
							 IMESSAGE_SETATTRIBUTE, &contentType, 
							 CRYPT_CERTINFO_CMS_CONTENTTYPE ) );
	}

/* Pre-process information for signed enveloping */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processSignatureAction( INOUT ENVELOPE_INFO *envelopeInfoPtr,
								   INOUT ACTION_LIST *actionListPtr )
	{
	SIGPARAMS sigParams;
	int signatureAlgo = DUMMY_INIT, signatureSize, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	REQUIRES( actionListPtr != NULL && \
			  actionListPtr->action == ACTION_SIGN && \
			  actionListPtr->associatedAction != NULL );

	/* Process signing certificates and fix up the content-type in the 
	   authenticated attributes if necessary */
	if( envelopeInfoPtr->type == CRYPT_FORMAT_CMS || \
		envelopeInfoPtr->type == CRYPT_FORMAT_SMIME )
		{
		status = processSigningCerts( envelopeInfoPtr, actionListPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Set up any necessary signature parameters such as signature 
	  attributes and timestamps if necessary */
	status = cmsInitSigParams( actionListPtr, envelopeInfoPtr->type, 
							   envelopeInfoPtr->ownerHandle, 
							   &sigParams );
	if( cryptStatusError( status ) )
		return( status );

	/* Evaluate the size of the exported action */
	status = iCryptCreateSignature( NULL, 0, &signatureSize, 
						envelopeInfoPtr->type, actionListPtr->iCryptHandle,
						actionListPtr->associatedAction->iCryptHandle,
						( envelopeInfoPtr->type == CRYPT_FORMAT_CRYPTLIB ) ? \
							NULL : &sigParams );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( actionListPtr->iCryptHandle,
								  IMESSAGE_GETATTRIBUTE, &signatureAlgo,
								  CRYPT_CTXINFO_ALGO );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( isDlpAlgo( signatureAlgo ) || isEccAlgo( signatureAlgo ) || \
		actionListPtr->iTspSession != CRYPT_ERROR )
		{
		/* If there are any signature actions that will result in indefinite-
		   length encodings present then we can't use a definite-length 
		   encoding for the signature */
		envelopeInfoPtr->dataFlags |= ENVDATA_HASINDEFTRAILER;
		actionListPtr->encodedSize = CRYPT_UNUSED;
		}
	else
		{
		actionListPtr->encodedSize = signatureSize;
		envelopeInfoPtr->signActionSize += signatureSize;
		}
	if( envelopeInfoPtr->dataFlags & ENVDATA_HASINDEFTRAILER )
		envelopeInfoPtr->signActionSize = CRYPT_UNUSED;
	ENSURES( ( envelopeInfoPtr->signActionSize == CRYPT_UNUSED ) || \
			 ( envelopeInfoPtr->signActionSize > 0 && \
			   envelopeInfoPtr->signActionSize < MAX_INTLENGTH ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int cmsPreEnvelopeSign( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	ACTION_LIST *actionListPtr = envelopeInfoPtr->postActionList;
	int iterationCount, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envelopeInfoPtr->usage == ACTION_SIGN );
	REQUIRES( actionListPtr != NULL && \
			  actionListPtr->associatedAction != NULL );

	/* If we're generating a detached signature the content is supplied
	   externally and has zero size */
	if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
		envelopeInfoPtr->payloadSize = 0;

	/* If it's an attributes-only message it must be zero-length CMS signed
	   data with signing attributes present */
	if( envelopeInfoPtr->flags & ENVELOPE_ATTRONLY )
		{
		if( envelopeInfoPtr->type != CRYPT_FORMAT_CMS || \
			actionListPtr->iExtraData == CRYPT_ERROR )
			{
			setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_SIGNATURE_EXTRADATA,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		if( envelopeInfoPtr->payloadSize > 0 )
			{
			setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_DATASIZE,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_INITED );
			}
		}

	/* If it's a CMS envelope we have to write the signing certificate chain
	   alongside the signatures as extra data unless it's explicitly 
	   excluded so we record how large the information will be for later */
	if( ( envelopeInfoPtr->type == CRYPT_FORMAT_CMS || \
		  envelopeInfoPtr->type == CRYPT_FORMAT_SMIME ) && \
		!( envelopeInfoPtr->flags & ENVELOPE_NOSIGNINGCERTS ) )
		{
		if( actionListPtr->next != NULL )
			{
			MESSAGE_CREATEOBJECT_INFO createInfo;

			/* There are multiple sets of signing certificates present, 
			   create a signing-certificate meta-object to hold the overall 
			   set of certificates */
			setMessageCreateObjectInfo( &createInfo,
										CRYPT_CERTTYPE_CERTCHAIN );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_DEV_CREATEOBJECT,
									  &createInfo, OBJECT_TYPE_CERTIFICATE );
			if( cryptStatusError( status ) )
				return( status );
			envelopeInfoPtr->iExtraCertChain = createInfo.cryptHandle;
			}
		else
			{
			MESSAGE_DATA msgData;

			/* There's a single signing certificate present, determine its 
			   size */
			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( actionListPtr->iCryptHandle,
									  IMESSAGE_CRT_EXPORT, &msgData,
									  CRYPT_ICERTFORMAT_CERTSET );
			if( cryptStatusError( status ) )
				return( status );
			envelopeInfoPtr->extraDataSize = msgData.length;
			}
		}

	/* Evaluate the size of each signature action */
	for( actionListPtr = envelopeInfoPtr->postActionList, iterationCount = 0; 
		 actionListPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED;
		 actionListPtr = actionListPtr->next, iterationCount++ )
		{
		status = processSignatureAction( envelopeInfoPtr, actionListPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* If we're writing the signing certificate chain and there are multiple 
	   signing certificates present, get the size of the overall certificate 
	   collection */
	if( envelopeInfoPtr->iExtraCertChain != CRYPT_ERROR )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( envelopeInfoPtr->iExtraCertChain,
								  IMESSAGE_CRT_EXPORT, &msgData,
								  CRYPT_ICERTFORMAT_CERTSET );
		if( cryptStatusError( status ) )
			return( status );
		envelopeInfoPtr->extraDataSize = msgData.length;
		}
	ENSURES( envelopeInfoPtr->extraDataSize >= 0 && \
			 envelopeInfoPtr->extraDataSize < MAX_INTLENGTH );

	/* Hashing is now active (you have no chance to survive make your 
	   time) */
	envelopeInfoPtr->dataFlags |= ENVDATA_HASHACTIONSACTIVE;

	return( CRYPT_OK );
	}
#endif /* USE_ENVELOPES */
