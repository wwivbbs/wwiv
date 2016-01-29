/****************************************************************************
*																			*
*					cryptlib Enveloping Information Management				*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "envelope.h"
  #include "pgp.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "envelope/envelope.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

#ifdef USE_ENVELOPES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#ifdef USE_PGP

/* Check that an object being added is suitable for use with PGP data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkPgpUsage( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE envInfo )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( envInfo > CRYPT_ENVINFO_FIRST && envInfo < CRYPT_ENVINFO_LAST );

	/* The attribute being added isn't context-related, there's nothing 
	   PGP-specific to check */
	if( envInfo != CRYPT_ENVINFO_PUBLICKEY && \
		envInfo != CRYPT_ENVINFO_PRIVATEKEY && \
		envInfo != CRYPT_ENVINFO_KEY && \
		envInfo != CRYPT_ENVINFO_SESSIONKEY && \
		envInfo != CRYPT_ENVINFO_HASH && \
		envInfo != CRYPT_ENVINFO_SIGNATURE )
		return( CRYPT_OK );

	/* PGP doesn't support both PKC and conventional key exchange actions in 
	   the same envelope since the session key is encrypted for the PKC 
	   action but derived from the password for the conventional action */
	if( findAction( envelopeInfoPtr->preActionList,
					ACTION_KEYEXCHANGE ) != NULL )
		return( CRYPT_ERROR_INITED );

	/* PGP handles multiple signers by nesting signed data rather than 
	   attaching multiple signatures so we can only apply a single 
	   signature per envelope */
	if( envInfo == CRYPT_ENVINFO_SIGNATURE && \
		envelopeInfoPtr->postActionList != NULL )
		return( CRYPT_ERROR_INITED );

	/* PGP doesn't allow multiple hash algorithms to be used when signing 
	   data, a follow-on from the way that nested sigs are handled */
	if( envInfo == CRYPT_ENVINFO_HASH && \
		envelopeInfoPtr->actionList != NULL )
		{
		/* The one exception to this occurs when we're using a detached 
		   signature with an externally-provided hash value and the user 
		   has added a signing key before adding the hash, in which case 
		   a hash action will have been added automatically when the signing 
		   key was added.  In this case the presence of an automatically-
		   added hash action is OK since it'll be replaced with the hash
		   value that we're now adding */
		if( ( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) && \
			envelopeInfoPtr->actionList->action == ACTION_HASH && \
			( envelopeInfoPtr->actionList->flags & ACTION_ADDEDAUTOMATICALLY ) )
			return( CRYPT_OK );

		return( CRYPT_ERROR_INITED );
		}

	return( CRYPT_OK );
	}
#endif /* USE_PGP */

/* Clone a context so that we can add the clone to an action list */

static int cloneActionContext( OUT_HANDLE_OPT CRYPT_CONTEXT *iClonedContext,
							   IN_HANDLE const CRYPT_CONTEXT cryptContext,
							   IN_ALGO const CRYPT_ALGO_TYPE algorithm )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int status;

	assert( isWritePtr( iClonedContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( isHandleRangeValid( cryptContext ) );
	REQUIRES( algorithm > CRYPT_ALGO_NONE && algorithm < CRYPT_ALGO_LAST );

	/* Clear return value */
	*iClonedContext = CRYPT_ERROR;

	setMessageCreateObjectInfo( &createInfo, algorithm );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( cryptContext, IMESSAGE_CLONE, NULL,
							  createInfo.cryptHandle );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iClonedContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Misc.Enveloping Info Management Functions				*
*																			*
****************************************************************************/

/* Set up the encryption for an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initEnvelopeEncryption( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							IN_HANDLE const CRYPT_CONTEXT cryptContext,
							IN_ALGO_OPT const CRYPT_ALGO_TYPE algorithm, 
							IN_MODE_OPT const CRYPT_MODE_TYPE mode,
							IN_BUFFER_OPT( ivLength ) const BYTE *iv, 
							IN_LENGTH_IV_Z const int ivLength,
							const BOOLEAN copyContext )
	{
	CRYPT_CONTEXT iCryptContext = cryptContext;
	int contextAlgorithm DUMMY_INIT, contextMode DUMMY_INIT;
	int blockSize DUMMY_INIT, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( ( iv == NULL && ivLength == 0 ) || \
			isReadPtr( iv, ivLength ) );

	REQUIRES( isHandleRangeValid( cryptContext ) );
	REQUIRES( ( algorithm == CRYPT_ALGO_NONE && mode == CRYPT_MODE_NONE ) || \
			  ( isConvAlgo( algorithm ) ) );
	REQUIRES( ( algorithm == CRYPT_ALGO_NONE && mode == CRYPT_MODE_NONE ) || \
			  ( mode > CRYPT_MODE_NONE && mode < CRYPT_MODE_LAST ) );
	REQUIRES( ( iv == NULL && ivLength == 0 ) || \
			  ( iv != NULL && \
			    ivLength >= 8 && ivLength <= CRYPT_MAX_IVSIZE ) );

	/* Extract the information that we need to process data */
	status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE,
							  &contextAlgorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE,
								  &contextMode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE,
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the context is what's required */
	if( algorithm != CRYPT_ALGO_NONE && \
		( contextAlgorithm != algorithm || contextMode != mode ) )
		{
		/* This can only happen on de-enveloping if the data is corrupted or
		   if the user is asked for a KEK and tries to supply a session key
		   instead */
		return( CRYPT_ERROR_WRONGKEY );
		}
	if( ivLength != 0 && ivLength != blockSize ) 
		return( CRYPT_ERROR_BADDATA );

	/* If it's a user-supplied context take a copy for our own use.  This is
	   only done for non-idempotent user-supplied contexts, for everything
	   else we either use cryptlib's object management to handle things for
	   us or the context is a internal one created specifically for our own
	   use */
	if( copyContext )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		setMessageCreateObjectInfo( &createInfo, contextAlgorithm );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		status = krnlSendMessage( iCryptContext, IMESSAGE_CLONE, NULL,
								  createInfo.cryptHandle );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		iCryptContext = createInfo.cryptHandle;
		}

	/* Load the IV into the context and set up the encryption information for
	   the envelope */
	if( !isStreamCipher( contextAlgorithm ) )
		{
		if( iv != NULL )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, ( MESSAGE_CAST ) iv, ivLength );
			status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
									  &msgData, CRYPT_CTXINFO_IV );
			}
		else
			{
			/* There's no IV specified, generate a new one */
			status = krnlSendNotifier( iCryptContext, IMESSAGE_CTX_GENIV );
			}
		if( cryptStatusError( status ) )
			{
			if( copyContext )
				{
				/* Destroy the copy that we created earlier */
				krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
				}
			return( status );
			}
		}
	envelopeInfoPtr->iCryptContext = iCryptContext;
	envelopeInfoPtr->blockSize = blockSize;
	envelopeInfoPtr->blockSizeMask = ~( blockSize - 1 );

	return( CRYPT_OK );
	}

/* Check the consistency of enveloping resources before we begin enveloping,
   returning the ID of any missing attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkSignatureActionFunction( const ACTION_LIST *actionListPtr,
										 IN_INT_Z const int signingKeyPresent )
	{
	assert( isReadPtr( actionListPtr, sizeof( ACTION_LIST ) ) );

	/* If there are no signature-related auxiliary options present, there's
	   nothing to check */
	if( actionListPtr->iExtraData != CRYPT_ERROR || \
		actionListPtr->iTspSession != CRYPT_ERROR )
		return( CRYPT_OK );

	/* There must be a signing key present to handle the signature options */
	if( !signingKeyPresent || actionListPtr->iCryptHandle == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTINITED );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkMissingInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							 const BOOLEAN isFlush )
	{
	BOOLEAN signingKeyPresent = FALSE;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	/* Make sure that we have the minimum requirements for each usage type
	   present */
	switch( envelopeInfoPtr->usage )
		{
		case ACTION_COMPRESS:
			REQUIRES( envelopeInfoPtr->flags & ENVELOPE_ZSTREAMINITED );
			break;

		case ACTION_HASH:
			DEBUG_DIAG(( "Hashed (rather than MAC'd) enveloping isn't supported" ));
			assert( DEBUG_WARN );
			break;

		case ACTION_MAC:
			/* If it's a MAC envelope there must be at least one key exchange 
			   action present.  A few obscure operation sequences may 
			   however set the usage without setting a key exchange action.  
			   For example making the envelope a MAC envelope simply 
			   indicates that any future key exchange actions should be used 
			   for MACing rather than encryption but this is indicative of a 
			   logic error in the calling application so we report an error 
			   even if, strictly speaking, we could ignore it and continue */
			if( findAction( envelopeInfoPtr->preActionList, \
							ACTION_KEYEXCHANGE_PKC ) == NULL && \
				findAction( envelopeInfoPtr->preActionList, \
							ACTION_KEYEXCHANGE ) == NULL )
				{
				/* We return the most generic CRYPT_ENVINFO_KEY error code
				   since there are several possible missing attribute types 
				   that could be required */
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_KEY, 
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			break;

		case ACTION_CRYPT:
			/* If it's an encryption envelope there must be a key present at 
			   some level.  This situation doesn't normally occur since the 
			   higher-level code will only set the usage to encryption once 
			   a key exchange action has been added, but we check anyway 
			   just to be safe */
			if( findAction( envelopeInfoPtr->preActionList, \
							ACTION_KEYEXCHANGE_PKC ) == NULL && \
				findAction( envelopeInfoPtr->preActionList, \
							ACTION_KEYEXCHANGE ) == NULL && \
				findAction( envelopeInfoPtr->actionList, ACTION_CRYPT ) == NULL )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_KEY, 
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			break;

		case ACTION_SIGN:
			/* If it's a signing envelope there must be a signature key
			   present */
			if( findAction( envelopeInfoPtr->postActionList, \
							ACTION_SIGN ) == NULL )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_SIGNATURE, 
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			signingKeyPresent = TRUE;

			/* If it's a detached signature and we're performing a flush (in
			   other words we're wrapping up the enveloping with no data
			   pushed) then there has to be an externally-supplied hash
			   present */
			if( ( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) && \
				isFlush )
				{
				const ACTION_LIST *actionListPtr = \
							findAction( envelopeInfoPtr->actionList, \
										ACTION_HASH );
								
				if( actionListPtr == NULL || \
					( actionListPtr->flags & ACTION_ADDEDAUTOMATICALLY ) )
					{
					setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_HASH, 
								  CRYPT_ERRTYPE_ATTR_ABSENT );
					return( CRYPT_ERROR_NOTINITED );
					}
				}
		}

	REQUIRES( signingKeyPresent || \
			  !( ( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) || \
				 findAction( envelopeInfoPtr->actionList, ACTION_HASH ) ) );

	/* If there are signature-related options present (signature envelope,
	   detached-signature flag set, hash context present, or CMS attributes 
	   or a TSA session present) there must be a signing key also present */
	if( envelopeInfoPtr->postActionList != NULL )
		{
		int status;

		status = checkActionIndirect( envelopeInfoPtr->postActionList,
									  checkSignatureActionFunction, 
									  signingKeyPresent );
		if( cryptStatusError( status ) )
			{
			setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_SIGNATURE, 
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( status );
			}
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Add Enveloping Information 						*
*																			*
****************************************************************************/

/* Add keyset information (this function is also used by the de-enveloping 
   routines) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addKeysetInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
				   IN_RANGE( CRYPT_ENVINFO_KEYSET_SIGCHECK, \
							 CRYPT_ENVINFO_KEYSET_DECRYPT ) \
					const CRYPT_ATTRIBUTE_TYPE keysetFunction,
				   IN_HANDLE const CRYPT_KEYSET keyset )
	{
	CRYPT_KEYSET *iKeysetPtr;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( keysetFunction == CRYPT_ENVINFO_KEYSET_ENCRYPT || \
			  keysetFunction == CRYPT_ENVINFO_KEYSET_DECRYPT || \
			  keysetFunction == CRYPT_ENVINFO_KEYSET_SIGCHECK );
	REQUIRES( isHandleRangeValid( keyset ) );

	/* Figure out which keyset we want to set */
	switch( keysetFunction )
		{
		case CRYPT_ENVINFO_KEYSET_ENCRYPT:
			iKeysetPtr = &envelopeInfoPtr->iEncryptionKeyset;
			break;

		case CRYPT_ENVINFO_KEYSET_DECRYPT:
			iKeysetPtr = &envelopeInfoPtr->iDecryptionKeyset;
			break;

		case CRYPT_ENVINFO_KEYSET_SIGCHECK:
			iKeysetPtr = &envelopeInfoPtr->iSigCheckKeyset;
			break;

		default:
			retIntError();
		}

	/* Make sure that the keyset hasn't already been set */
	if( *iKeysetPtr != CRYPT_ERROR )
		{
		setErrorInfo( envelopeInfoPtr, keysetFunction,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( CRYPT_ERROR_INITED );
		}

	/* Remember the new keyset and increment its reference count */
	*iKeysetPtr = keyset;
	return( krnlSendNotifier( keyset, IMESSAGE_INCREFCOUNT ) );
	}

/* Add an encryption password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int addPasswordInfo( ENVELOPE_INFO *envelopeInfoPtr,
							IN_BUFFER( passwordLength ) const void *password, 
							IN_LENGTH_TEXT const int passwordLength )
	{
	CRYPT_ALGO_TYPE cryptAlgo = envelopeInfoPtr->defaultAlgo;
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	ACTION_RESULT actionResult;
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( passwordLength > 0 && passwordLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( envelopeInfoPtr->type != CRYPT_FORMAT_PGP );

	/* Make sure that we can still add another action */
	if( !moreActionsPossible( envelopeInfoPtr->preActionList ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* Create the appropriate encryption context.  We have to be careful to 
	   ensure that we use an algorithm which is compatible with the wrapping 
	   mechanism */
	if( isStreamCipher( cryptAlgo ) || \
		cryptStatusError( sizeofAlgoIDex( cryptAlgo, CRYPT_MODE_CBC, 0 ) ) )
		cryptAlgo = CRYPT_ALGO_3DES;
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT, 
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;

	/* Derive the key into the context */
	setMessageData( &msgData, ( MESSAGE_CAST ) password, passwordLength );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_CTXINFO_KEYING_VALUE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Make sure that this key exchange action isn't already present and 
	   insert it into the action list */
	actionResult = checkAction( envelopeInfoPtr->preActionList, 
								ACTION_KEYEXCHANGE, iCryptContext );
	if( actionResult == ACTION_RESULT_ERROR || \
		actionResult == ACTION_RESULT_INITED )
		{
		setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_PASSWORD,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		status = CRYPT_ERROR_INITED;
		}
	else
		{
		status = addAction( &envelopeInfoPtr->preActionList,
							envelopeInfoPtr->memPoolState, 
							ACTION_KEYEXCHANGE, iCryptContext );
		}
	if( cryptStatusError( status ) )
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	return( status );
	}

#ifdef USE_PGP

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int addPgpPasswordInfo( ENVELOPE_INFO *envelopeInfoPtr,
							   IN_BUFFER( passwordLength ) const void *password, 
							   IN_LENGTH_TEXT const int passwordLength )
	{
	CRYPT_ALGO_TYPE cryptAlgo = envelopeInfoPtr->defaultAlgo;
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE salt[ PGP_SALTSIZE + 8 ];
	static const int mode = CRYPT_MODE_CFB;	/* int vs.enum */
	int status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( passwordLength > 0 && passwordLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( envelopeInfoPtr->type == CRYPT_FORMAT_PGP );

	/* Make sure that we can still add another attribute */
	if( !moreActionsPossible( envelopeInfoPtr->preActionList ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* PGP doesn't support both PKC and conventional key exchange actions or 
	   multiple conventional key exchange actions in the same envelope since 
	   the session key is encrypted for the PKC action but derived from the 
	   password for the conventional action */
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
		( findAction( envelopeInfoPtr->preActionList,
					  ACTION_KEYEXCHANGE_PKC ) != NULL || \
		  envelopeInfoPtr->actionList != NULL ) )
		{
		setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_PUBLICKEY,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( CRYPT_ERROR_INITED );
		}

	/* Create the appropriate encryption context.  PGP wrapping always uses 
	   CFB mode (so there are no modes that need to be avoided) and the 
	   higher-level code has constrained the algorithm type to something 
	   that's encodable using the PGP data format so we don't need to 
	   perform any additional checking here */
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT, 
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;

	/* PGP uses CFB mode for everything so we change the mode from the 
	   default of CBC to CFB */
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, 
							  ( MESSAGE_CAST ) &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate a salt and derive the key into the context */
	setMessageData( &msgData, salt, PGP_SALTSIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusOK( status ) )
		{
		status = pgpPasswordToKey( iCryptContext, CRYPT_UNUSED, 
								   password, passwordLength,
								   envelopeInfoPtr->defaultHash,
								   salt, PGP_SALTSIZE, PGP_ITERATIONS );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Insert the context into the action list.  Since PGP doesn't perform a 
	   key exchange of a session key we insert the password-derived context 
	   directly into the main action list */
	status = addAction( &envelopeInfoPtr->actionList, 
						envelopeInfoPtr->memPoolState, ACTION_CRYPT, 
						iCryptContext );
	if( cryptStatusError( status ) )
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	return( status );
	}
#endif /* USE_PGP */

/* Add a context to an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int addContextInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
						   IN_HANDLE const CRYPT_HANDLE cryptHandle,
						   INOUT_PTR ACTION_LIST **actionListHeadPtrPtr,
						   IN_ENUM( ACTION ) const ACTION_TYPE actionType )
	{
	CRYPT_HANDLE iCryptHandle = cryptHandle;
	ACTION_LIST *actionListPtr, *hashActionPtr;
	ACTION_RESULT actionResult;
	int algorithm, mode = CRYPT_MODE_NONE, certHashAlgo, status;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isWritePtr( actionListHeadPtrPtr, sizeof( ACTION_LIST * ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( actionType > ACTION_NONE && actionType < ACTION_LAST );

	/* Make sure that we can still add another attribute */
	if( !moreActionsPossible( envelopeInfoPtr->preActionList ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* Make sure that the algorithm information is encodable using the 
	   selected envelope format.  This should already have been checked by
	   the calling function but we double-check here because this provides 
	   a convenient centralised location for it */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && isConvAlgo( algorithm ) )
		{
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE,
								  &mode, CRYPT_CTXINFO_MODE );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( !envelopeInfoPtr->checkAlgo( algorithm, mode ) )
		return( CRYPT_ARGERROR_NUM1 );

	/* If we're adding a hash action and this is a detached signature (so 
	   that the action contains the hash of the data to be signed by the 
	   detached signature) then the presence of an automatically-added 
	   existing hash action isn't a problem, but we need to replace it with 
	   the explicitly-added action */
	if( actionType == ACTION_HASH && \
		( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG ) )
		{
		/* Check whether there's an automatically-added hash action present 
		   that was created as a side-effect of adding a signature key.  If
		   there is, we just replace the existing action context with the
		   new one, leaving everything else unchanged */
		hashActionPtr = findAction( *actionListHeadPtrPtr, ACTION_HASH );
		if( hashActionPtr != NULL && \
			( hashActionPtr->flags & ACTION_ADDEDAUTOMATICALLY ) )
			{
			status = cloneActionContext( &iCryptHandle, iCryptHandle, 
										 algorithm );
			if( cryptStatusError( status ) )
				return( status );
			status = replaceAction( hashActionPtr, iCryptHandle );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
				return( status );
				}

			/* This is now an explicitly-added action rather than one that
			   was added automatically, as well as one for which we don't
			   need to explicitly complete the hashing */
			hashActionPtr->flags &= ~ACTION_ADDEDAUTOMATICALLY;
			hashActionPtr->flags |= ACTION_HASHCOMPLETE;

			return( CRYPT_OK );
			}
		}

	/* Find the insertion point for this action and make sure that it isn't 
	   already present.  The difference between ACTION_RESULT_INITED and 
	   ACTION_RESULT_PRESENT is that an inited response indicates that the 
	   user explicitly added the action and can't add it again while a 
	   present response indicates that the action was added automatically 
	   (and transparently) by cryptlib in response to the user adding some 
	   other action and so its presence shouldn't be reported as an error. 
	   This is because to the user it doesn't make any difference whether 
	   the same action was added automatically by cryptlib or explicitly by 
	   the user */
	actionResult = checkAction( *actionListHeadPtrPtr, actionType, 
								iCryptHandle );
	switch( actionResult )
		{
		case ACTION_RESULT_OK:
		case ACTION_RESULT_EMPTY:
			break;

		case ACTION_RESULT_INITED:
			return( CRYPT_ERROR_INITED );
	
		case ACTION_RESULT_PRESENT:
			return( CRYPT_OK );

		case ACTION_RESULT_ERROR:
			return( CRYPT_ARGERROR_NUM1 );

		default:
			retIntError();
		}

	/* Insert the action into the list.  If it's a non-idempotent context
	   (i.e. one whose state can change based on user actions) we clone it
	   for our own use, otherwise we just increment its reference count */
	if( actionType == ACTION_HASH || actionType == ACTION_CRYPT )
		{
		status = cloneActionContext( &iCryptHandle, iCryptHandle, 
									 algorithm );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		status = krnlSendNotifier( iCryptHandle, IMESSAGE_INCREFCOUNT );
		if( cryptStatusError( status ) )
			return( status );
		}
	status = addActionEx( &actionListPtr, actionListHeadPtrPtr,
						  envelopeInfoPtr->memPoolState, actionType, 
						  iCryptHandle );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	if( actionType == ACTION_HASH )
		{
		/* Remember that we need to hook the hash action up to a signature
		   action before we start enveloping data */
		actionListPtr->flags |= ACTION_NEEDSCONTROLLER;

		/* If this is a detached signature for which the hash value was 
		   added explicitly by the user, we don't need to complete the 
		   hashing ourselves since it's already been done by the user */
		if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
			actionListPtr->flags |= ACTION_HASHCOMPLETE;
		}

	/* If the newly-inserted action isn't a controlling action, we're done */
	if( actionType != ACTION_SIGN )
		return( status );

	/* Check whether the hash algorithm used in the certificate attached to 
	   the signing key is stronger than the one that's set for the envelope 
	   as a whole and if it is, upgrade the envelope hash algo.  This is 
	   based on the fact that anyone who's able to verify the certificate 
	   using a stronger hash algorithm must also be able to verify the 
	   envelope using the stronger algorithm.  This allows a transparent 
	   upgrade to stronger hash algorithms as they become available */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE,
							  &certHashAlgo, CRYPT_IATTRIBUTE_CERTHASHALGO );
	if( cryptStatusOK( status ) && \
		isStrongerHash( certHashAlgo, envelopeInfoPtr->defaultHash ) )
		envelopeInfoPtr->defaultHash = certHashAlgo;

	/* If there's no subject hash action available, create one so that we
	   can connect it to the signature action */
	if( envelopeInfoPtr->actionList == NULL )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		/* Create a default hash action */
		setMessageCreateObjectInfo( &createInfo, envelopeInfoPtr->defaultHash );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );

		/* Add the hash action to the list */
		status = addActionEx( &hashActionPtr, &envelopeInfoPtr->actionList,
							  envelopeInfoPtr->memPoolState, ACTION_HASH,
							  createInfo.cryptHandle );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( status );
			}

		/* Remember that the action was added invisibly to the caller so that
		   we don't return an error if they add it explicitly later on, and 
		   that it needs to be attached to a controlling action before it 
		   can be used */
		hashActionPtr->flags |= ACTION_ADDEDAUTOMATICALLY | \
								ACTION_NEEDSCONTROLLER;
		}
	else
		{
		/* Find the last hash action that was added */
		hashActionPtr = findLastAction( envelopeInfoPtr->actionList,
										ACTION_HASH );
		if( hashActionPtr == NULL )
			{
			setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_HASH,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTINITED );
			}
		}

	/* Connect the signature action to the last hash action that was added
	   and remember that this action now has a controlling action */
	actionListPtr->associatedAction = hashActionPtr;
	hashActionPtr->flags &= ~ACTION_NEEDSCONTROLLER;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Enveloping Information Management Functions				*
*																			*
****************************************************************************/

/* Add enveloping information to an envelope */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addEnvelopeInfo( INOUT ENVELOPE_INFO *envelopeInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE envInfo,
							IN_INT_Z const int value )
	{
	CRYPT_HANDLE cryptHandle = ( CRYPT_HANDLE ) value;
	ACTION_LIST *actionListPtr;

	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES( ( envInfo == CRYPT_IATTRIBUTE_INCLUDESIGCERT ) || \
			  ( envInfo == CRYPT_IATTRIBUTE_ATTRONLY ) || \
			  ( envInfo > CRYPT_ENVINFO_FIRST && \
				envInfo < CRYPT_ENVINFO_LAST ) );

	/* If it's a generic "add a context" action for a PGP envelope check 
	   that everything is valid.  This is necessary because the PGP format 
	   doesn't support the full range of enveloping capabilities */
#ifdef USE_PGP
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP && \
		envInfo > CRYPT_ENVINFO_FIRST && \
		envInfo < CRYPT_ENVINFO_LAST )
		{
		const int status = checkPgpUsage( envelopeInfoPtr, envInfo );
		if( cryptStatusError( status ) )
			{
			setErrorInfo( envelopeInfoPtr, envInfo,
						  CRYPT_ERRTYPE_ATTR_PRESENT );
			return( status );
			}
		}
#endif /* USE_PGP */

	/* If it's meta-information, remember the value */
	switch( envInfo )
		{
		case CRYPT_IATTRIBUTE_INCLUDESIGCERT:
			/* This is on by default so we should only be turning it off */
			REQUIRES( value == FALSE );

			envelopeInfoPtr->flags |= ENVELOPE_NOSIGNINGCERTS;
			return( CRYPT_OK );

		case CRYPT_IATTRIBUTE_ATTRONLY:
			/* This is off by default so we should only be turning it on */
			REQUIRES( value == TRUE );

			/* Detached-signature and attribute-only messages are mutually 
			   exclusive */
			if( envelopeInfoPtr->flags & ENVELOPE_DETACHED_SIG )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_DETACHEDSIGNATURE,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			envelopeInfoPtr->flags |= ENVELOPE_ATTRONLY;
			return( CRYPT_OK );

		case CRYPT_ENVINFO_DATASIZE:
			envelopeInfoPtr->payloadSize = value;
			return( CRYPT_OK );

		case CRYPT_ENVINFO_CONTENTTYPE:
			envelopeInfoPtr->contentType = value;
			return( CRYPT_OK );

		case CRYPT_ENVINFO_DETACHEDSIGNATURE:
			if( value )
				{
				/* Detached-signature and attribute-only messages are 
				   mutually exclusive.  Since the attribute-only message 
				   attribute is internal we can't set extended error 
				   information for this one */
				if( envelopeInfoPtr->flags & ENVELOPE_ATTRONLY )
					return( CRYPT_ERROR_INITED );
				envelopeInfoPtr->flags |= ENVELOPE_DETACHED_SIG;
				}
			else
				envelopeInfoPtr->flags &= ~ENVELOPE_DETACHED_SIG;
			return( CRYPT_OK );

		case CRYPT_ENVINFO_INTEGRITY:
			switch( value )
				{
				case CRYPT_INTEGRITY_NONE:
					return( CRYPT_OK );

				case CRYPT_INTEGRITY_MACONLY:
					if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
						{
						/* PGP doesn't have MAC-only integrity protection */
						return( CRYPT_ARGERROR_VALUE );
						}
					envelopeInfoPtr->usage = ACTION_MAC;
					return( CRYPT_OK );

				case CRYPT_INTEGRITY_FULL:
					/* If we're using authenticated encryption in the form
					   of crypt + MAC (rather than a combined auth-enc 
					   encryption mode like GCM, or PGP's semi-keyed hash
					   function that isn't really a MAC) then we can't use a 
					   raw session key because there are effectively two 
					   keys present, one for the encryption and one for the 
					   MACing.
					   
					   We generalise the check here to make sure that there 
					   are no main envelope actions set, in practice this 
					   can't happen because setting e.g. a hash action will 
					   set the envelope usage to USAGE_SIGN which precludes 
					   then setting CRYPT_ENVINFO_INTEGRITY, but the more 
					   general check here can't hurt */
					if( envelopeInfoPtr->actionList != NULL && \
						envelopeInfoPtr->type != CRYPT_FORMAT_PGP )
						{
						setErrorInfo( envelopeInfoPtr, 
									  CRYPT_ENVINFO_SESSIONKEY,
									  CRYPT_ERRTYPE_ATTR_PRESENT );
						return( CRYPT_ERROR_INITED );
						}

					envelopeInfoPtr->usage = ACTION_CRYPT;
					envelopeInfoPtr->flags |= ENVELOPE_AUTHENC;
					return( CRYPT_OK );
				}
			retIntError();

		case CRYPT_ENVINFO_KEYSET_SIGCHECK:
		case CRYPT_ENVINFO_KEYSET_ENCRYPT:
		case CRYPT_ENVINFO_KEYSET_DECRYPT:
			/* It's keyset information, just keep a record of it for later 
			   use */
			return( addKeysetInfo( envelopeInfoPtr, envInfo, cryptHandle ) );

		case CRYPT_ENVINFO_SIGNATURE_EXTRADATA:
		case CRYPT_ENVINFO_TIMESTAMP:
			{
			CRYPT_HANDLE *iCryptHandlePtr;

			/* Find the last signature action that was added and make sure
			   that it doesn't already have an action of this type attached 
			   to it */
			actionListPtr = findLastAction( envelopeInfoPtr->postActionList,
											ACTION_SIGN );
			if( actionListPtr == NULL )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_SIGNATURE,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ERROR_NOTINITED );
				}
			iCryptHandlePtr = ( envInfo == CRYPT_ENVINFO_SIGNATURE_EXTRADATA ) ? \
							  &actionListPtr->iExtraData : \
							  &actionListPtr->iTspSession;
			if( *iCryptHandlePtr != CRYPT_ERROR )
				{
				setErrorInfo( envelopeInfoPtr, envInfo,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			/* Increment its reference count and add it to the action */
			krnlSendNotifier( cryptHandle, IMESSAGE_INCREFCOUNT );
			*iCryptHandlePtr = cryptHandle;
			return( CRYPT_OK );
			}

		case CRYPT_ENVINFO_ORIGINATOR:
			/* Historic value only needed for Fortezza */
			return( CRYPT_ARGERROR_NUM1 );

		case CRYPT_ENVINFO_COMPRESSION:
#ifdef USE_COMPRESSION
			/* Make sure that we don't try and initialise the compression
			   multiple times */
			if( envelopeInfoPtr->flags & ENVELOPE_ZSTREAMINITED )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_COMPRESSION,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			/* Initialize the compression */
			if( deflateInit( &envelopeInfoPtr->zStream, \
							 Z_DEFAULT_COMPRESSION ) != Z_OK )
				return( CRYPT_ERROR_MEMORY );
			envelopeInfoPtr->flags |= ENVELOPE_ZSTREAMINITED;

			return( CRYPT_OK );
#else
			return( CRYPT_ARGERROR_NUM1 );
#endif /* USE_COMPRESSION */

		case CRYPT_ENVINFO_PUBLICKEY:
		case CRYPT_ENVINFO_PRIVATEKEY:
			return( addContextInfo( envelopeInfoPtr, cryptHandle,
									&envelopeInfoPtr->preActionList,
									ACTION_KEYEXCHANGE_PKC ) );

		case CRYPT_ENVINFO_KEY:
			/* PGP doesn't allow KEK-based encryption so if it's a PGP
			   envelope we drop through and treat it as a session key */
			if( envelopeInfoPtr->type != CRYPT_FORMAT_PGP )
				{
				return( addContextInfo( envelopeInfoPtr, cryptHandle, 
										&envelopeInfoPtr->preActionList,
										ACTION_KEYEXCHANGE ) );
				}
			/* Fall through */

		case CRYPT_ENVINFO_SESSIONKEY:
			/* We can't add more than one session key */
			if( envelopeInfoPtr->actionList != NULL )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_SESSIONKEY,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			/* If we're using authenticated encryption in the form of crypt + 
			   MAC (rather than a combined auth-enc encryption mode like GCM, 
			   or PGP's semi-keyed hash function that isn't really a MAC) 
			   then we can't use a raw session key because there are 
			   effectively two keys present, one for the encryption and one 
			   for the MACing */
			if( envelopeInfoPtr->flags & ENVELOPE_AUTHENC && \
				envelopeInfoPtr->type != CRYPT_FORMAT_PGP )
				{
				setErrorInfo( envelopeInfoPtr, CRYPT_ENVINFO_INTEGRITY,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			return( addContextInfo( envelopeInfoPtr, cryptHandle, 
									&envelopeInfoPtr->actionList,
									ACTION_CRYPT ) );

		case CRYPT_ENVINFO_HASH:
			return( addContextInfo( envelopeInfoPtr, cryptHandle, 
									&envelopeInfoPtr->actionList, 
									ACTION_HASH ) );

		case CRYPT_ENVINFO_SIGNATURE:
			return( addContextInfo( envelopeInfoPtr, cryptHandle, 
									&envelopeInfoPtr->postActionList, 
									ACTION_SIGN ) );
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int addEnvelopeInfoString( INOUT ENVELOPE_INFO *envelopeInfoPtr,
								  IN_RANGE( CRYPT_ENVINFO_PASSWORD, \
											CRYPT_ENVINFO_PASSWORD ) \
									const CRYPT_ATTRIBUTE_TYPE envInfo,
								  IN_BUFFER( valueLength ) const void *value, 
								  IN_LENGTH_TEXT const int valueLength )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );
	assert( isReadPtr( value, valueLength ) );

	REQUIRES( envInfo == CRYPT_ENVINFO_PASSWORD );
	REQUIRES( valueLength > 0 && valueLength <= CRYPT_MAX_TEXTSIZE );

#ifdef USE_PGP
	if( envelopeInfoPtr->type == CRYPT_FORMAT_PGP )
		return( addPgpPasswordInfo( envelopeInfoPtr, value, valueLength ) );
#endif /* USE_PGP */
	return( addPasswordInfo( envelopeInfoPtr, value, valueLength ) );
	}

/****************************************************************************
*																			*
*							Envelope Access Routines						*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initEnvResourceHandling( INOUT ENVELOPE_INFO *envelopeInfoPtr )
	{
	assert( isWritePtr( envelopeInfoPtr, sizeof( ENVELOPE_INFO ) ) );

	REQUIRES_V( !( envelopeInfoPtr->flags & ENVELOPE_ISDEENVELOPE ) );

	/* Set the access method pointers */
	envelopeInfoPtr->addInfo = addEnvelopeInfo;
	envelopeInfoPtr->addInfoString = addEnvelopeInfoString;
	envelopeInfoPtr->checkMissingInfo = checkMissingInfo;
	}
#endif /* USE_ENVELOPES */
