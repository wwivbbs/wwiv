/****************************************************************************
*																			*
*						cryptlib Plug-and-play PKI Routines					*
*						 Copyright Peter Gutmann 1999-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

#if defined( USE_CMP ) || defined( USE_SCEP )

/* When we generate a new key there are a variety of different key types
   (meaning key usages) that we can generate it for, constrained to some
   extent by what the underlying certificate management protocol supports.  
   The following values identify the key type that we need to generate */

typedef enum {
	KEY_TYPE_NONE,			/* No key type */
	KEY_TYPE_ENCRYPTION,	/* Encryption key */
	KEY_TYPE_SIGNATURE,		/* Signature key */
	KEY_TYPE_BOTH,			/* Dual encryption/signature key */
	KEY_TYPE_LAST			/* Last possible key type */
	} KEY_TYPE;

/* A structure to store key type-related information, indexed by the KEY_TYPE 
   value */

static const struct {
	const char *label;		/* Label for private key */
	const int labelLength;
	const int actionPerms;	/* Context action perms */
	const int keyUsage;		/* Certificate key usage */
	} keyInfo[] = {
	{ NULL, 0,				/* KEY_TYPE_NONE */
		0, CRYPT_KEYUSAGE_NONE },
	{ "Encryption key", 14,	/* KEY_TYPE_ENCRYPTION */
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_NONE_EXTERNAL ),
		CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ "Signature key", 13,	/* KEY_TYPE_SIGNATURE */
		MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_NONE_EXTERNAL ),
		CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ "Private key", 11,	/* KEY_TYPE_BOTH */
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_NONE_EXTERNAL ),
		CRYPT_KEYUSAGE_KEYENCIPHERMENT | CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ NULL, 0, 0, CRYPT_KEYUSAGE_NONE }, { NULL, 0, 0, CRYPT_KEYUSAGE_NONE }
	};

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Clean up an object if the PnP operation fails.  This is required when 
   working with devices since we need to explicitly delete anything that
   was created in the device as well as just deleting the cryptlib object */

static void cleanupObject( IN_HANDLE const CRYPT_CONTEXT iPrivateKey, 
						   IN_ENUM( KEY_TYPE ) const KEY_TYPE keyType )
	{
	CRYPT_DEVICE iCryptDevice;
	MESSAGE_KEYMGMT_INFO deletekeyInfo;
	int status;

	REQUIRES_V( isHandleRangeValid( iPrivateKey ) );
	REQUIRES_V( keyType > KEY_TYPE_NONE && keyType < KEY_TYPE_LAST );

	/* Delete the cryptlib object.  If it's a native object, we're done */
	status = krnlSendMessage( iPrivateKey, IMESSAGE_GETDEPENDENT,
							  &iCryptDevice, OBJECT_TYPE_DEVICE );
	krnlSendNotifier( iPrivateKey, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return;

	/* Delete the key from the device.  We set the item type to delete to
	   public key since the device object will interpret this correctly
	   to mean that it should also delete the associated private key.  We
	   don't bother checking the return code since there's not much that
	   we can do to recover if this fails */
	setMessageKeymgmtInfo( &deletekeyInfo, CRYPT_KEYID_NAME, 
						   keyInfo[ keyType ].label,
						   keyInfo[ keyType ].labelLength, NULL, 0, 
						   KEYMGMT_FLAG_NONE );
	( void ) krnlSendMessage( iCryptDevice, IMESSAGE_KEY_DELETEKEY,
							  &deletekeyInfo, KEYMGMT_ITEM_PUBLICKEY );
	}

/* Check whether a network connection is still open, used when performing
   multiple transactions in a single session */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isConnectionOpen( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int streamState, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	status = sioctlGet( &sessionInfoPtr->stream, STREAM_IOCTL_CONNSTATE, 
						&streamState, sizeof( int ) );
	return( cryptStatusError( status ) ? FALSE : streamState );
	}

/* Check for the presence of a named object in a keyset/device */

CHECK_RETVAL_BOOL \
static BOOLEAN isNamedObjectPresent( IN_HANDLE const CRYPT_HANDLE iCryptHandle,
									 IN_ENUM( KEY_TYPE ) const KEY_TYPE keyType )
	{
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	const char *keyLabel = keyInfo[ keyType ].label;
	const int keyLabelLength = keyInfo[ keyType ].labelLength;
	int status;

	REQUIRES_B( isHandleRangeValid( iCryptHandle ) );
	REQUIRES_B( keyType > KEY_TYPE_NONE && keyType < KEY_TYPE_LAST );

	setMessageKeymgmtInfo( &getkeyInfo, CRYPT_KEYID_NAME, keyLabel, 
						   keyLabelLength, NULL, 0, 
						   KEYMGMT_FLAG_CHECK_ONLY );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_GETKEY, 
							  &getkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
	if( cryptStatusError( status ) )
		{
		setMessageKeymgmtInfo( &getkeyInfo, CRYPT_KEYID_NAME, keyLabel, 
							   keyLabelLength, NULL, 0,
							   KEYMGMT_FLAG_CHECK_ONLY );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_GETKEY, 
								  &getkeyInfo, KEYMGMT_ITEM_PRIVATEKEY );
		}
	return( cryptStatusOK( status ) ? TRUE : FALSE );
	}

/* Get the identified CA/RA certificate from a CTL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getCACert( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iNewCert, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCTL, 
					  IN_BUFFER_OPT( certIDlength ) const void *certID, 
					  IN_LENGTH_KEYID_Z const int certIDlength )
	{
	int status;

	assert( isWritePtr( iNewCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( ( certID == NULL && certIDlength == 0 ) || \
			( isReadPtr( certID,  certIDlength ) ) );

	REQUIRES( isHandleRangeValid( iCTL ) );
	REQUIRES( ( certID == NULL && certIDlength == 0 ) || \
			  ( certID != NULL && certIDlength == KEYID_SIZE ) );

	/* Clear return value */
	*iNewCert = CRYPT_ERROR;

	/* Step through the certificate trust list checking each certificate in 
	   turn to see if it's the identified CA/RA certificate.  Some CAs may 
	   only send a single certificate in the CTL and not explicitly identify 
	   it so if there's no certificate ID present we just use the first 
	   one.  Note that the limit is given as FAILSAFE_ITERATIONS_MED since 
	   we're using it as a fallback check on the maximum chain length allowed
	   by the certificate-import code.  In other words anything over the
	   certificate-handling code's maximum chain length is handled as a 
	   normal error and it's only if we exceed this that we have an internal 
	   error */
	status = krnlSendMessage( iCTL, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST,
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	if( certIDlength > 0 )
		{
		MESSAGE_DATA msgData;
		int iterationCount;

		setMessageData( &msgData, ( MESSAGE_CAST ) certID, KEYID_SIZE );
		for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MED; 
			 iterationCount++ )
			{
			status = krnlSendMessage( iCTL, IMESSAGE_COMPARE, &msgData, 
									  MESSAGE_COMPARE_FINGERPRINT_SHA1 );
			if( cryptStatusOK( status ) )
				{
				/* We've found the identified certificate, we're done */
				break;
				}
			status = krnlSendMessage( iCTL, IMESSAGE_SETATTRIBUTE,
									  MESSAGE_VALUE_CURSORNEXT,
									  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
			if( cryptStatusError( status ) )
				{
				/* We've run out of certificates without finding a match, 
				   exit */
				return( CRYPT_ERROR_NOTFOUND );
				}
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
		}

	/* We've found the identified certificate, convert it from the data-only 
	   form in the CTL to a full certificate that can be used to verify 
	   returned data.  This is easier than trying to disconnect and re-
	   connect certificate and context objects directly (ex duobus malis 
	   minimum eligendum est) */
	return( krnlSendMessage( iCTL, IMESSAGE_GETATTRIBUTE, iNewCert, 
							 CRYPT_IATTRIBUTE_CERTCOPY ) );
	}

/****************************************************************************
*																			*
*					Certificate Creation/Update Routines					*
*																			*
****************************************************************************/

/* Generate a new key of the appropriate type.  For CMP we typically 
   generate first a signature key and then an encryption key (unless it's
   a CA key, for which we only generate a certificate-signing key), for
   SCEP we generate a dual encryption/signature key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKey( OUT_HANDLE_OPT CRYPT_CONTEXT *iPrivateKey,
						IN_HANDLE const CRYPT_USER iCryptUser,
						IN_HANDLE const CRYPT_DEVICE iCryptDevice,
						IN_ENUM( KEY_TYPE ) const KEY_TYPE keyType )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BOOLEAN substitutedAlgorithm = FALSE;
	int pkcAlgo, keySize, status;

	assert( isWritePtr( iPrivateKey, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( iCryptUser == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptUser ) );
	REQUIRES( iCryptDevice == SYSTEM_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptDevice ) );
	REQUIRES( keyType > KEY_TYPE_NONE && keyType < KEY_TYPE_LAST );

	/* Clear return value */
	*iPrivateKey = CRYPT_ERROR;

	/* Get the algorithm to use for the key.  We try and use the given 
	   default PKC algorithm, however some devices don't support all 
	   algorithm types so if this isn't available we try and fall back to 
	   other choices */
	status = krnlSendMessage( iCryptUser, IMESSAGE_GETATTRIBUTE, &pkcAlgo, 
							  CRYPT_OPTION_PKC_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( !algoAvailable( pkcAlgo ) )
		{
		/* The default algorithm type isn't available for this device, try 
		   and fall back to an alternative */
		switch( pkcAlgo )
			{
			case CRYPT_ALGO_RSA:
				pkcAlgo = CRYPT_ALGO_DSA;
				break;

			case CRYPT_ALGO_DSA:
				pkcAlgo = CRYPT_ALGO_RSA;
				break;

			default:
				return( CRYPT_ERROR_NOTAVAIL );
			}
		if( !algoAvailable( pkcAlgo ) )
			return( CRYPT_ERROR_NOTAVAIL );

		/* Remember that we've switched to a fallback algorithm */
		substitutedAlgorithm = TRUE;
		}

	/* Make sure that the chosen algorithm is compatible with what we're 
	   trying to do */
	switch( keyType )
		{
		case KEY_TYPE_ENCRYPTION:
			/* If we're being asked for an encryption key, which in normal 
			   operation implies that we've already successfully completed 
			   the process of acquiring a signature key, and only a non-
			   encryption algorithm is available, we return OK_SPECIAL to 
			   tell the caller that the failure is non-fatal.  However if 
			   we've substituted an algorithm (for example DSA when RSA was 
			   requested) then we genuinely can't go any further and exit
			   with an error */
			if( !isCryptAlgo( pkcAlgo ) )
				return( substitutedAlgorithm ? \
						CRYPT_ERROR_NOTAVAIL : OK_SPECIAL );
			break;

		case KEY_TYPE_SIGNATURE:
			if( !isSigAlgo( pkcAlgo ) )
				return( CRYPT_ERROR_NOTAVAIL );
			break;

		case KEY_TYPE_BOTH:
			if( !isCryptAlgo( pkcAlgo ) || !isSigAlgo( pkcAlgo ) )
				return( CRYPT_ERROR_NOTAVAIL );
			break;

		default:
			retIntError();
		}

	/* Create a new key using the given PKC algorithm and of the default 
	   size */
	setMessageCreateObjectInfo( &createInfo, pkcAlgo );
	status = krnlSendMessage( iCryptDevice, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iCryptUser, IMESSAGE_GETATTRIBUTE, &keySize, 
							  CRYPT_OPTION_PKC_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, &keySize, 
								  CRYPT_CTXINFO_KEYSIZE );
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) keyInfo[ keyType ].label, 
						keyInfo[ keyType ].labelLength );
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CTXINFO_LABEL );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Generate the key */
	status = krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE,
								  ( int * ) &keyInfo[ keyType ].actionPerms,
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iPrivateKey = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Create a certificate request for a key.  If a certificate with a subject 
   DN template is provided we copy this into the request, otherwise we 
   create a minimal key-only request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createCertRequest( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertReq, 
							  IN_HANDLE const CRYPT_CONTEXT iPrivateKey,
							  IN_HANDLE_OPT const CRYPT_CERTIFICATE iSubjDNCert,
							  IN_ENUM( KEY_TYPE ) const KEY_TYPE keyType )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	const BOOLEAN isPKCS10 = ( keyType == KEY_TYPE_BOTH ) ? TRUE : FALSE;
	int status;

	assert( isWritePtr( iCertReq, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( isHandleRangeValid( iPrivateKey ) );
	REQUIRES( iSubjDNCert == CRYPT_UNUSED || \
			  isHandleRangeValid( iSubjDNCert ) );
	REQUIRES( keyType > KEY_TYPE_NONE && keyType < KEY_TYPE_LAST );

	/* Clear return value */
	*iCertReq = CRYPT_ERROR;

	/* Create the signing key certificate request */
	setMessageCreateObjectInfo( &createInfo, isPKCS10 ? \
								CRYPT_CERTTYPE_CERTREQUEST : \
								CRYPT_CERTTYPE_REQUEST_CERT );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
							  OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the key information to the request and sign it if it's a CMP
	   request.  We can't sign PKCS #10 requests (for SCEP) because the 
	   client session has to add further information required by the server 
	   to the request before it submits it */
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  ( int * ) &iPrivateKey,
							  CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
								  ( int * ) &keyInfo[ keyType ].keyUsage, 
								  CRYPT_CERTINFO_KEYUSAGE );
	if( cryptStatusOK( status ) && iSubjDNCert != CRYPT_UNUSED )
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
								  ( int * ) &iSubjDNCert,
								  CRYPT_CERTINFO_CERTIFICATE );
	if( cryptStatusOK( status ) && !isPKCS10 )
		status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CRT_SIGN,
								  NULL, iPrivateKey );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iCertReq = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Update a keyset/device with a newly-created key and certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
static int updateKeys( IN_HANDLE const CRYPT_HANDLE iCryptHandle,
					   IN_HANDLE const CRYPT_CONTEXT iPrivateKey,
					   IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
					   IN_BUFFER( passwordLength ) const char *password, 
					   IN_LENGTH_NAME const int passwordLength )
	{
	MESSAGE_KEYMGMT_INFO setkeyInfo;
	int objectType, status;

	assert( isReadPtr( password, passwordLength ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( isHandleRangeValid( iPrivateKey ) );
	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( passwordLength >= MIN_NAME_LENGTH && \
			  passwordLength < MAX_ATTRIBUTE_SIZE );

	/* Find out whether the storage object is a keyset or a device.  If it's
	   a device there's no need to add the private key since it'll have been
	   created inside the device */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE, 
							  &objectType, CRYPT_IATTRIBUTE_TYPE );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the private key and certificate to the keyset/device */
	if( objectType == OBJECT_TYPE_KEYSET )
		{
		setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
							   ( MESSAGE_CAST ) password, passwordLength,
							   KEYMGMT_FLAG_NONE );
		setkeyInfo.cryptHandle = iPrivateKey;
		status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_SETKEY,
								  &setkeyInfo, KEYMGMT_ITEM_PRIVATEKEY );
		if( cryptStatusError( status ) )
			return( status );
		}
	setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
						   NULL, 0, KEYMGMT_FLAG_NONE );
	setkeyInfo.cryptHandle = iCryptCert;
	return( krnlSendMessage( iCryptHandle, IMESSAGE_KEY_SETKEY,
							 &setkeyInfo, KEYMGMT_ITEM_PUBLICKEY ) );
	}

/* Update the keyset/device with any required trusted certificates up to the 
   root.  This ensures that we can still build a full certificate chain even 
   if the PKIBoot trusted certificates aren't preserved */

static int updateTrustedCerts( IN_HANDLE const CRYPT_HANDLE iCryptHandle,
							   IN_HANDLE const CRYPT_HANDLE iLeafCert )
	{
	CRYPT_CERTIFICATE iCertCursor = iLeafCert;
	int iterationCount, status;

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( isHandleRangeValid( iLeafCert ) );

	for( status = CRYPT_OK, iterationCount = 0; 
		 cryptStatusOK( status ) && iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		MESSAGE_KEYMGMT_INFO setkeyInfo;

		/* Get the trusted issuer certificate for the current certificate 
		   and send it to the keyset/device */
		status = krnlSendMessage( iCertCursor, IMESSAGE_USER_TRUSTMGMT, 
								  &iCertCursor, MESSAGE_TRUSTMGMT_GETISSUER );
		if( cryptStatusError( status ) )
			break;
		setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
							   NULL, 0, KEYMGMT_FLAG_NONE );
		setkeyInfo.cryptHandle = iCertCursor;
		status = krnlSendMessage( iCryptHandle, IMESSAGE_KEY_SETKEY, 
								  &setkeyInfo, KEYMGMT_ITEM_PUBLICKEY );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							PnP PKI Session Management						*
*																			*
****************************************************************************/

/* Run a plug-and-play PKI session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pnpPkiSession( INOUT SESSION_INFO *sessionInfoPtr )
	{
	CRYPT_DEVICE iCryptDevice = SYSTEM_OBJECT_HANDLE;
	CRYPT_CONTEXT iPrivateKey1, iPrivateKey2 ;
	CRYPT_CERTIFICATE iCertReq, iCACert DUMMY_INIT;
	const ATTRIBUTE_LIST *attributeListPtr;
	const ATTRIBUTE_LIST *passwordPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD );
	const KEY_TYPE keyType = ( sessionInfoPtr->type == CRYPT_SESSION_CMP ) ? \
							 KEY_TYPE_SIGNATURE : KEY_TYPE_BOTH;
	const char *storageObjectName = "keyset";
	BOOLEAN isCAcert;
	int objectType, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( passwordPtr != NULL );

	/* If we've been passed a device as the private-key storage location,
	   create the key in the device instead of as a local object */
	status = krnlSendMessage( sessionInfoPtr->privKeyset,
							  IMESSAGE_GETATTRIBUTE, &objectType,
							  CRYPT_IATTRIBUTE_TYPE );
	if( cryptStatusError( status ) )
		return( status );
	if( objectType == OBJECT_TYPE_DEVICE )
		{
		iCryptDevice = sessionInfoPtr->privKeyset;
		storageObjectName = "device";
		}

	/* Make sure that the named objects that are about to be created aren't 
	   already present in the keyset/device */
	if( isNamedObjectPresent( sessionInfoPtr->privKeyset, keyType ) )
		{
		retExt( CRYPT_ERROR_DUPLICATE,
				( CRYPT_ERROR_DUPLICATE, SESSION_ERRINFO, 
				  "%s is already present in %s",
				  ( keyType == KEY_TYPE_SIGNATURE ) ? \
					"Signature key" : "Key", storageObjectName ) );
		}
	if( sessionInfoPtr->type == CRYPT_SESSION_CMP )
		{
		if( isNamedObjectPresent( sessionInfoPtr->privKeyset, 
								  KEY_TYPE_ENCRYPTION ) )
			{
			retExt( CRYPT_ERROR_DUPLICATE,
					( CRYPT_ERROR_DUPLICATE, SESSION_ERRINFO, 
					  "Encryption key is already present in %s",
					  storageObjectName ) );
			}
		}

	/* Perform the PKIBoot exchange (done as part of an ir) to get the 
	   initial trusted certificate set.  We also set the retain-connection 
	   flag since we're going to follow this with another transaction */
	if( sessionInfoPtr->type == CRYPT_SESSION_CMP )
		{
		sessionInfoPtr->sessionCMP->requestType = CRYPT_REQUESTTYPE_PKIBOOT;
		sessionInfoPtr->protocolFlags |= CMP_PFLAG_RETAINCONNECTION;
		}
	status = sessionInfoPtr->transactFunction( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	if( !isConnectionOpen( sessionInfoPtr ) )
		{
		/* If the connection was shut down by the other side, signal an 
		   error.  This is possibly a bit excessive since we could always 
		   try reactivating the session, but there's no good reason for the 
		   other side to simply close the connection and requiring it to 
		   remain open simplifies the implementation */
		krnlSendNotifier( sessionInfoPtr->iCertResponse, 
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCertResponse = CRYPT_ERROR;
		retExt( CRYPT_ERROR_READ,
				( CRYPT_ERROR_READ, SESSION_ERRINFO, 
				  "Server closed connection after PKIBoot phase before any "
				  "certificates could be issued" ) );
		}

	/* Get the CA/RA certificate from the returned CTL and set it as the 
	   certificate to use for authenticating server responses */
	attributeListPtr = findSessionInfo( sessionInfoPtr->attributeList,
										CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 );
	if( attributeListPtr != NULL )
		{
		status = getCACert( &iCACert, sessionInfoPtr->iCertResponse, 
							attributeListPtr->value, 
							attributeListPtr->valueLength );
		}
	else
		{
		status = getCACert( &iCACert, sessionInfoPtr->iCertResponse, 
							NULL, 0 );
		}
	krnlSendNotifier( sessionInfoPtr->iCertResponse, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertResponse = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't read CA/RA certificate from returned "
				  "certificate trust list" ) );
		}
	sessionInfoPtr->iAuthInContext = iCACert;

	/* Create a private key and a certificate request for it */
	status = generateKey( &iPrivateKey1, sessionInfoPtr->ownerHandle,
						  iCryptDevice, keyType );
	if( cryptStatusError( status ) )
		{
		ENSURES( status != OK_SPECIAL );
		retExt( status, 
				( status, SESSION_ERRINFO, "Couldn't create %s key",
				  ( keyType == KEY_TYPE_SIGNATURE ) ? \
					"signature" : "private" ) );
		}
	status = createCertRequest( &iCertReq, iPrivateKey1, CRYPT_UNUSED, 
								keyType );
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, keyType );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create %skey certificate request",
				  ( keyType == KEY_TYPE_SIGNATURE ) ? \
					"signature " : "" ) );
		}

	/* Set up the request information and activate the session */
	if( sessionInfoPtr->type == CRYPT_SESSION_CMP )
		{
		/* If it's CMP, start with an ir.  The second certificate will be 
		   fetched with a cr */
		sessionInfoPtr->sessionCMP->requestType = \
										CRYPT_REQUESTTYPE_INITIALISATION;
		}
	sessionInfoPtr->iCertRequest = iCertReq;
	status = sessionInfoPtr->transactFunction( sessionInfoPtr );
	krnlSendNotifier( sessionInfoPtr->iCertRequest, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertRequest = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, keyType );
		return( status );
		}

	/* Check whether we've been issued a standalone CA certificate rather 
	   than a standard signature certificate to be followed by an encryption 
	   certificate or a standard signature + encryption certificate */
	status = krnlSendMessage( sessionInfoPtr->iCertResponse, 
							  IMESSAGE_GETATTRIBUTE, &isCAcert,
							  CRYPT_CERTINFO_CA );
	if( cryptStatusError( status ) )
		isCAcert = FALSE;

	/* If the connection was shut down by the other side and we're 
	   performing a multi-part operation that requires it to remain open, 
	   signal an error.  This is possibly a bit excessive since we could 
	   always try reactivating the session, but there's no good reason for 
	   the other side to simply close the connection and requiring it to 
	   remain open simplifies the implementation */
	if( sessionInfoPtr->type == CRYPT_SESSION_CMP && \
		!isConnectionOpen( sessionInfoPtr ) && !isCAcert )
		{
		cleanupObject( iPrivateKey1, keyType );
		krnlSendNotifier( sessionInfoPtr->iCertResponse, 
						  IMESSAGE_DECREFCOUNT );
		sessionInfoPtr->iCertResponse = CRYPT_ERROR;
		retExt( CRYPT_ERROR_READ,
				( CRYPT_ERROR_READ, SESSION_ERRINFO, 
				  "Server closed connection before second (encryption) "
				  "certificate could be issued" ) );
		}

	/* We've got the first certificate, update the keyset/device */
	status = updateKeys( sessionInfoPtr->privKeyset, iPrivateKey1,
						 sessionInfoPtr->iCertResponse, 
						 passwordPtr->value, passwordPtr->valueLength );
	if( cryptStatusOK( status ) )
		{
		CRYPT_CERTIFICATE iNewCert;

		/* Recreate the certificate as a data-only certificate and attach it 
		   to the signing key so that we can use it to authenticate a 
		   request for an encryption key.  We need to recreate the 
		   certificate because we're about to attach it to the private-key 
		   context for further operations, and attaching a certificate with 
		   a public-key context already attached isn't possible.  Even if 
		   we're not getting a second certificate, we still need the current 
		   certificate attached so that we can use it as the base 
		   certificate for the trusted certificate update that we perform 
		   before we exit */
		status = krnlSendMessage( sessionInfoPtr->iCertResponse, 
								  IMESSAGE_GETATTRIBUTE, &iNewCert, 
								  CRYPT_IATTRIBUTE_CERTCOPY_DATAONLY );
		if( cryptStatusOK( status ) )
			krnlSendMessage( iPrivateKey1, IMESSAGE_SETDEPENDENT, &iNewCert, 
							 SETDEP_OPTION_NOINCREF );
		}
	krnlSendNotifier( sessionInfoPtr->iCertResponse, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertResponse = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, keyType );
		retExt( ( status == CRYPT_ARGERROR_NUM1 ) ? \
				CRYPT_ERROR_INVALID : status,
				( ( status == CRYPT_ARGERROR_NUM1 ) ? \
				  CRYPT_ERROR_INVALID : status, SESSION_ERRINFO, 
				  "Couldn't update %s with %skey/certificate", 
				  storageObjectName, isCAcert ? "CA " : \
				  ( keyType == KEY_TYPE_SIGNATURE ) ? "signature " : "" ) );
		}

	/* If it's a combined encryption/signature key or a standalone CA key, 
	   we're done.  See the comment at the end of this function for the 
	   details of the trusted-certificates update process */
	if( keyType == KEY_TYPE_BOTH || isCAcert )
		{
		updateTrustedCerts( sessionInfoPtr->privKeyset, iPrivateKey1 );
		krnlSendNotifier( iPrivateKey1, IMESSAGE_DECREFCOUNT );
		return( CRYPT_OK );
		}

	/* We're running a CMP session from this point on */
	ENSURES( sessionInfoPtr->type == CRYPT_SESSION_CMP );

	/* Create the second, encryption private key and a certificate request 
	   for it */
	status = generateKey( &iPrivateKey2, sessionInfoPtr->ownerHandle,
						  iCryptDevice, KEY_TYPE_ENCRYPTION );
	if( status == OK_SPECIAL )
		{
		/* Encryption isn't available via this device, exit without going
		   through the second phase of the exchange, leaving only the
		   signature key and certificates set up */
		updateTrustedCerts( sessionInfoPtr->privKeyset, iPrivateKey1 );
		krnlSendNotifier( iPrivateKey1, IMESSAGE_DECREFCOUNT );
		return( CRYPT_OK );
		}
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, KEY_TYPE_SIGNATURE );
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't create encryption key" ) );
		}
	status = createCertRequest( &iCertReq, iPrivateKey2, iPrivateKey1,
								KEY_TYPE_ENCRYPTION );
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, KEY_TYPE_SIGNATURE );
		cleanupObject( iPrivateKey2, KEY_TYPE_ENCRYPTION );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create encryption key certificate request" ) );
		}

	/* Set up the request information and activate the session.  This 
	   request is slightly different to the previous one since we now have a 
	   signature certificate that we can use to authenticate the request (in 
	   fact we have to use this since we can't authenticate the message with 
	   an encryption-only key).  In addition since this is the last 
	   transaction we turn off the retain-connection flag */
	sessionInfoPtr->protocolFlags &= ~CMP_PFLAG_RETAINCONNECTION;
	sessionInfoPtr->sessionCMP->requestType = CRYPT_REQUESTTYPE_CERTIFICATE;
	sessionInfoPtr->iCertRequest = iCertReq;
	sessionInfoPtr->privateKey = iPrivateKey2;
	sessionInfoPtr->iAuthOutContext = iPrivateKey1;
	status = sessionInfoPtr->transactFunction( sessionInfoPtr );
	sessionInfoPtr->privateKey = CRYPT_ERROR;
	sessionInfoPtr->iAuthOutContext = CRYPT_ERROR;
	krnlSendNotifier( sessionInfoPtr->iCertRequest, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertRequest = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, KEY_TYPE_SIGNATURE );
		cleanupObject( iPrivateKey2, KEY_TYPE_ENCRYPTION );
		return( status );
		}

	/* We've got the second certificate, update the keyset/device */
	status = updateKeys( sessionInfoPtr->privKeyset, iPrivateKey2,
						 sessionInfoPtr->iCertResponse, 
						 passwordPtr->value, passwordPtr->valueLength );
	krnlSendNotifier( sessionInfoPtr->iCertResponse, IMESSAGE_DECREFCOUNT );
	sessionInfoPtr->iCertResponse = CRYPT_ERROR;
	if( cryptStatusError( status ) )
		{
		cleanupObject( iPrivateKey1, KEY_TYPE_SIGNATURE );
		cleanupObject( iPrivateKey2, KEY_TYPE_ENCRYPTION );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't update %s with encryption key/certificate",
				  storageObjectName ) );
		}

	/* Finally, update the keyset/device with any required trusted 
	   certificates up to the root.  This ensures that we can still build a 
	   full certificate chain even if the PKIBoot trusted certificates 
	   aren't preserved.  We don't check for errors from this function since 
	   it's not worth aborting the process for some minor CA certificate 
	   update problem, the user keys and certificates will still function 
	   without them */
	updateTrustedCerts( sessionInfoPtr->privKeyset, iPrivateKey1 );

	/* Both keys were certified and the keys and certificates sent to the 
	   keyset/device, we're done */
	krnlSendNotifier( iPrivateKey1, IMESSAGE_DECREFCOUNT );
	krnlSendNotifier( iPrivateKey2, IMESSAGE_DECREFCOUNT );
	return( CRYPT_OK );
	}
#endif /* USE_CMP || USE_SCEP */
