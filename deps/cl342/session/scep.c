/****************************************************************************
*																			*
*						 cryptlib SCEP Session Management					*
*						Copyright Peter Gutmann 1999-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
  #include "scep.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
  #include "session/scep.h"
#endif /* Compiler-specific includes */

#ifdef USE_SCEP

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Initialise and clean up protocol information */

STDC_NONNULL_ARG( ( 1 ) ) \
void initSCEPprotocolInfo( OUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	memset( protocolInfo, 0, sizeof( SCEP_PROTOCOL_INFO ) );
	protocolInfo->iScepCert = CRYPT_ERROR;
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void destroySCEPprotocolInfo( INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	if( protocolInfo->iScepCert != CRYPT_ERROR )
		krnlSendNotifier( protocolInfo->iScepCert, IMESSAGE_DECREFCOUNT );

	zeroise( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) );
	}

/* Check a private key being added to a SCEP session to make sure that it's
   of the appropriate form */

CHECK_RETVAL_BOOL \
static BOOLEAN checkPrivateKey( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
								const BOOLEAN isCertKey )
	{
	int value, status;

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* If the private key requires an associated certificate, make sure that 
	   it's present and of the correct form */
	if( isCertKey )
		{
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, 
								  &value, CRYPT_CERTINFO_IMMUTABLE );
		if( cryptStatusError( status ) || !value )
			return( FALSE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, 
								  &value, CRYPT_CERTINFO_CERTTYPE );
		if( cryptStatusError( status ) || \
			( value != CRYPT_CERTTYPE_CERTIFICATE && \
			  value != CRYPT_CERTTYPE_CERTCHAIN ) )
			return( FALSE );
		
		return( TRUE );
		}

	/* The private key can't have an associated certificate, make sure 
	   there's none present */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, 
							  &value, CRYPT_CERTINFO_CERTTYPE );
	return( cryptStatusOK( status ) ? FALSE : TRUE );
	}

/* Check that the CA's certificate can also sign and encrypt data.  This is
   normally a really bad idea for CA certificates but is required by the 
   SCEP protocol.  We don't check for the object being a CA certificate 
   because we could be dealing with an RA, which isn't necessarily a CA */

CHECK_RETVAL_BOOL \
BOOLEAN checkSCEPCACert( IN_HANDLE const CRYPT_CERTIFICATE iCaCert,
 						 IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	REQUIRES_B( isHandleRangeValid( iCaCert ) );
	REQUIRES_B( options == KEYMGMT_FLAG_NONE || \
				options == KEYMGMT_FLAG_USAGE_CRYPT || \
				options == KEYMGMT_FLAG_USAGE_SIGN );

	/* Since this could be a certificate chain, we need to select the leaf
	   certificate in the chain */
	krnlSendMessage( iCaCert, IMESSAGE_SETATTRIBUTE,
					 MESSAGE_VALUE_CURSORFIRST,
					 CRYPT_CERTINFO_CURRENT_CERTIFICATE );

	/* Check whether the certificate has the required capabilities */
	if( options != KEYMGMT_FLAG_USAGE_SIGN )
		{
		if( cryptStatusError( \
				krnlSendMessage( iCaCert, IMESSAGE_CHECK, NULL,
								 MESSAGE_CHECK_PKC_ENCRYPT ) ) )
			return( FALSE );
		}
	if( options != KEYMGMT_FLAG_USAGE_CRYPT )
		{
		if( cryptStatusError( \
				krnlSendMessage( iCaCert, IMESSAGE_CHECK, NULL,
								 MESSAGE_CHECK_PKC_SIGCHECK ) ) )
			return( FALSE );
		}

	return( TRUE );
	}

/* Process a user name, either by checking that what we've been given by the 
   user is valid or by auto-generating one if the user has specified
   auto-detection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int processUserName( INOUT SESSION_INFO *sessionInfoPtr,
							IN_BUFFER( userNameLength ) const BYTE *userName,
							IN_LENGTH_SHORT const int userNameLength )
	{
	int index;

	/* Because users won't necessarily have a user name/transaction ID to 
	   use we allow them to specify the value as "[Autodetect]" and replace 
	   it with a base64'd nonce (to meet the encoding requirements below) */
	if( userNameLength == 12 && !memcmp( userName, "[Autodetect]", 12 ) )
		{
		MESSAGE_DATA msgData;
		BYTE nonce[ 16 + 8 ];
#ifndef USE_BASE64
		static const char FAR_BSS binToAscii[ 64 ] = \
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		int i;
#else
		int transIDlength;
#endif /* !USE_BASE64 */
		char transID[ CRYPT_MAX_TEXTSIZE + 8 ];
		int status;

		/* The caller has explicitly indicated that they want us to generate 
		   a transaction ID for them, generate it as a random text string */
		setMessageData( &msgData, nonce, 16 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
#ifdef USE_BASE64
		status = base64encode( transID, CRYPT_MAX_TEXTSIZE, 
							   &transIDlength, nonce, 16, 
							   CRYPT_CERTTYPE_NONE );
#else
		for( i = 0; i < 16; i++ )
			transID[ i ] = binToAscii[ nonce[ i ] & 0x3F ];
#endif /* USE_BASE64 */
		if( cryptStatusOK( status ) )
			{
			status = addSessionInfoEx( &sessionInfoPtr->attributeList,
									   CRYPT_SESSINFO_USERNAME, 
									   transID, 16, ATTR_FLAG_NONE );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Tell the caller that the attribute was processed internally */
		return( OK_SPECIAL );
		}

	/* For some bizarre reason SCEP encodes its transaction ID (which is our 
	   user name) as a PrintableString (even though the specification 
	   suggests that the value be created from a hash of the public key, 
	   which doesn't fit into a PrintableString) so we have to filter any 
	   supplied value to make sure that it can be encoded when we send it */
	for( index = 0; index < userNameLength; index++ )
		{
		static const char allowedChars[] = "'\"()+,-./:=? \x00\x00";
		const int ch = byteToInt( userName[ index ] );
		BOOLEAN foundMatch = FALSE;
		int i;

		if( isAlnum( ch ) )
			continue;
		for( i = 0; allowedChars[ i ] != '\0' && \
					i < FAILSAFE_ARRAYSIZE( allowedChars, char ); i++ )
			{
			if( allowedChars[ i ] == ch )
				{
				foundMatch = TRUE;
				break;
				}
			}
		ENSURES( i < FAILSAFE_ARRAYSIZE( allowedChars, char ) );
		if( !foundMatch )
			return( CRYPT_ARGERROR_STR1 );
		}
		
	return( CRYPT_OK );
	}

/* Generate/check the server certificate fingerprint */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int processKeyFingerprint( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *fingerprintPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 );
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* If the caller has supplied a certificate fingerprint, compare it to 
	   the received certificate's fingerprint to make sure that we're 
	   talking to the right system */
	if( fingerprintPtr != NULL )
		{
		setMessageData( &msgData, fingerprintPtr->value, 
						fingerprintPtr->valueLength );
		status = krnlSendMessage( sessionInfoPtr->iAuthInContext, 
								  IMESSAGE_COMPARE, &msgData, 
								  MESSAGE_COMPARE_FINGERPRINT_SHA1 );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
					  "Server certificate doesn't match key fingerprint" ) );
			}
		}
	else
		{
		BYTE certFingerprint[ CRYPT_MAX_HASHSIZE + 8 ];

		/* Remember the certificate fingerprint in case the caller wants to 
		   check it.  We don't worry if the add fails, it's a minor thing 
		   and not worth aborting the overall operation for */
		setMessageData( &msgData, certFingerprint, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( sessionInfoPtr->iAuthInContext, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		if( cryptStatusOK( status ) )
			{
			( void ) addSessionInfoS( &sessionInfoPtr->attributeList,
									  CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1,
									  certFingerprint, msgData.length );
			}
		}

	return( CRYPT_OK );
	}

/* Create SCEP signing attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int createScepAttributes( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT SCEP_PROTOCOL_INFO *protocolInfo,
						  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iScepAttributes,
						  IN_STRING const char *messageType,
						  IN_STATUS const int scepStatus )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );
	assert( isReadPtr( messageType, 2 ) );
	assert( isWritePtr( iScepAttributes, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( scepStatus >= MAX_ERROR && scepStatus <= CRYPT_OK );
	REQUIRES( userNamePtr != NULL );

	/* Clear return value */
	*iScepAttributes = CRYPT_ERROR;

	/* Create the signing attributes needed by SCEP and add the user name/
	   transaction ID and message type.  The transaction ID is a bit 
	   complicated, it's used to tie together all messages related to a 
	   certificate-issue transaction and acts more as a type of fixed nonce 
	   than any real identifier (the specification suggests using "an MD5 
	   hash on [sic] the public key", later extended to also encompass SHA1, 
	   SHA-256, and SHA-512).  This is complicated by the fact that the 
	   encoding is a PrintableString so the hash can't actually be used, but 
	   then another part of the spec says that it's encoded as a decimal 
	   value (!!) within the string.

	   The way we handle it is that the client sends it to the server as is 
	   (after a pre-check in checkAttributeFunction() for validity) and the 
	   server tries to decode it as a cryptlib-encoded PKI user ID that's 
	   used to look up the PKI user information.  If it can't be decoded by 
	   the server then it's not a request meant for a cryptlib server, so we 
	   can reject it immediately */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CMS_ATTRIBUTES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	iCmsAttributes = createInfo.cryptHandle;
	setMessageData( &msgData, userNamePtr->value, userNamePtr->valueLength );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_SCEP_TRANSACTIONID );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) messageType, 
						strlen( messageType ) );
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_SCEP_MESSAGETYPE );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add the message status if we're the server */
	if( isServer( sessionInfoPtr ) )
		{
		if( cryptStatusError( scepStatus ) )
			{
			/* SCEP provides an extremely limited set of error codes so 
			   there's not much that we can return in the way of additional 
			   failure information */
			setMessageData( &msgData, 
							( scepStatus == CRYPT_ERROR_SIGNATURE ) ? \
								MESSAGEFAILINFO_BADMESSAGECHECK : \
								MESSAGEFAILINFO_BADREQUEST,
							MESSAGEFAILINFO_SIZE );
			krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_CERTINFO_SCEP_FAILINFO );
			setMessageData( &msgData, MESSAGESTATUS_FAILURE,
							MESSAGESTATUS_SIZE );
			}
		else
			{
			setMessageData( &msgData, MESSAGESTATUS_SUCCESS, 
							MESSAGESTATUS_SIZE );
			}
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_SCEP_PKISTATUS );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	/* Add the nonce, identified as a sender nonce if we're the client and 
	   a recipient nonce if we're the server */
	if( isServer( sessionInfoPtr ) )
		{
		/* We're the server, use the client's nonce */
		setMessageData( &msgData, protocolInfo->nonce, 
						protocolInfo->nonceSize );
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, 
								  CRYPT_CERTINFO_SCEP_RECIPIENTNONCE );
		}
	else
		{
		/* If we're the client, generate a new nonce */
		setMessageData( &msgData, protocolInfo->nonce, SCEP_NONCE_SIZE );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
						 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		protocolInfo->nonceSize = SCEP_NONCE_SIZE;
		status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
								  &msgData, 
								  CRYPT_CERTINFO_SCEP_SENDERNONCE );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	*iScepAttributes = iCmsAttributes;

	return( CRYPT_OK );
	}

/* For some bizarre reason integer status values are encoded as strings,
   so we have to convert them to numeric values before we can do anything
   with them */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int getScepStatusValue( IN_HANDLE const CRYPT_CERTIFICATE iCmsAttributes,
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeType, 
						OUT int *value )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int numericValue, status;

	assert( isWritePtr( value, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iCmsAttributes ) );
	REQUIRES( attributeType == CRYPT_CERTINFO_SCEP_MESSAGETYPE || \
			  attributeType == CRYPT_CERTINFO_SCEP_PKISTATUS || \
			  attributeType == CRYPT_CERTINFO_SCEP_FAILINFO );

	/* Clear return value */
	*value = CRYPT_ERROR;

	/* Get the status string and decode it into an integer */
	setMessageData( &msgData, buffer, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, attributeType );
	if( cryptStatusError( status ) )
		return( status );
	status = strGetNumeric( buffer, msgData.length, &numericValue, 0, 20 );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );
	*value = numericValue;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Control Information Management Functions				*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 OUT void *data, 
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	CRYPT_CERTIFICATE *scepResponsePtr = ( CRYPT_CERTIFICATE * ) data;
	CRYPT_CERTIFICATE iCryptCert;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( data, sizeof( int ) ) );
	
	REQUIRES( type == CRYPT_SESSINFO_RESPONSE || \
			  type == CRYPT_SESSINFO_CACERTIFICATE || \
			  type == CRYPT_SESSINFO_CMP_REQUESTTYPE );

	/* If it's a general protocol-specific attribute read, return the
	   information and exit */
	if( type == CRYPT_SESSINFO_CMP_REQUESTTYPE )
		{
		if( scepInfo->requestType == CRYPT_REQUESTTYPE_NONE )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_CMP_REQUESTTYPE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_NOTFOUND );
			}
		*( ( int * ) data ) = scepInfo->requestType;
		return( CRYPT_OK );
		}

	/* If we didn't get a response or CA certificate then there's nothing to 
	   return */
	iCryptCert = ( type == CRYPT_SESSINFO_RESPONSE ) ? \
				   sessionInfoPtr->iCertResponse : \
				   sessionInfoPtr->iAuthInContext;
	if( iCryptCert == CRYPT_ERROR )
		return( CRYPT_ERROR_NOTFOUND );

	/* If the user is asking for the CA certificate and it wasn't fetched as 
	   part of the protocol run (in other words it was added by the user
	   when the session was set up) then they can't read it back as session
	   output */
	if( type == CRYPT_SESSINFO_CACERTIFICATE && \
		!( scepInfo->flags & SCEP_PFLAG_FETCHEDCACERT ) )
		return( CRYPT_ERROR_PERMISSION );

	/* Return the information to the caller */
	krnlSendNotifier( iCryptCert, IMESSAGE_INCREFCOUNT );
	*scepResponsePtr = iCryptCert;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	CRYPT_CERTIFICATE cryptCert = *( ( CRYPT_CERTIFICATE * ) data );
	int value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_REQUEST || \
			  type == CRYPT_SESSINFO_CACERTIFICATE || \
			  type == CRYPT_SESSINFO_CMP_REQUESTTYPE );

	/* If it's a request type, set it and exit */
	if( type == CRYPT_SESSINFO_CMP_REQUESTTYPE )
		{
		const int requestType = *( ( int * ) data );

		/* SCEP only allows a subset of the full range of request types */
		if( requestType != CRYPT_REQUESTTYPE_INITIALISATION && \
			requestType != CRYPT_REQUESTTYPE_CERTIFICATE && \
			requestType != CRYPT_REQUESTTYPE_KEYUPDATE )
			return( CRYPT_ARGERROR_NUM1 );

		/* For an initialisation request the private key must be a raw key
		   without an associated certificate */
		if( requestType == CRYPT_REQUESTTYPE_INITIALISATION )
			{
			if( sessionInfoPtr->privateKey != CRYPT_ERROR && \
				!checkPrivateKey( sessionInfoPtr->privateKey, FALSE ) )
				{
				setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
							  CRYPT_ERRTYPE_ATTR_VALUE );
				return( CRYPT_ARGERROR_NUM1 );
				}
			}
		else
			{
			/* For a renewal request the private key must have an associated
			   certificate, and there can't be a password present */
			if( sessionInfoPtr->privateKey != CRYPT_ERROR && \
				!checkPrivateKey( sessionInfoPtr->privateKey, TRUE ) )
				{
				setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
							  CRYPT_ERRTYPE_ATTR_VALUE );
				return( CRYPT_ARGERROR_NUM1 );
				}
			if( findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD ) != NULL )
				{
				setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ARGERROR_NUM1 );
				}
			}

		/* Remember the request type */
		scepInfo->requestType = requestType;

		return( CRYPT_OK );
		}

	/* Make sure that there aren't any conflicts with existing attributes */
	if( !checkAttributesConsistent( sessionInfoPtr, type ) )
		return( CRYPT_ERROR_INITED );

	/* Make sure that everything is set up ready to go */
	status = krnlSendMessage( cryptCert, IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_CERTINFO_IMMUTABLE );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( type == CRYPT_SESSINFO_CACERTIFICATE )
		{
		if( !value )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		/* If the user has indicated which request type they're using, make
		   sure that the PKCS #10 request is signed or unsigned as 
		   required */
		if( scepInfo->requestType != CRYPT_REQUESTTYPE_NONE )
			{
			/* If it's an initialisation request then the PKCS #10 has to be
			   unsigned so that we can add the challengePassword */
			if( scepInfo->requestType == CRYPT_REQUESTTYPE_INITIALISATION )
				{
				if( value )
					return( CRYPT_ARGERROR_NUM1 );
				}
			else
				{
				/* If it's a renewal request then the PKCS #10 has to be 
				   signed */
				if( !value )
					return( CRYPT_ARGERROR_NUM1 );
				}
			}
		}
	if( type == CRYPT_SESSINFO_CACERTIFICATE )
		{
		/* Make sure that the CA certificate has the unusual additional 
		   capabilities that are required to meet the SCEP protocol 
		   requirements.  Microsoft's SCEP server generates and uses 
		   CA certs that only have CRYPT_KEYUSAGE_KEYENCIPHERMENT set but 
		   not CRYPT_KEYUSAGE_DIGITALSIGNATURE (or occasionally
		   CRYPT_KEYUSAGE_DIGITALSIGNATURE but not 
		   CRYPT_KEYUSAGE_KEYENCIPHERMENT, there doesn't seem to be any 
		   discernable pattern to this), making them unusable for SCEP, so 
		   this check will reject anything coming from a Microsoft server 
		   unless the certificate compliance level is set to oblivious */
		if( !checkSCEPCACert( cryptCert, KEYMGMT_FLAG_NONE ) )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ARGERROR_NUM1 );
			}
		}

	/* Add the request and increment its usage count */
	krnlSendNotifier( cryptCert, IMESSAGE_INCREFCOUNT );
	if( type == CRYPT_SESSINFO_CACERTIFICATE )
		{
		sessionInfoPtr->iAuthInContext = cryptCert;
		status = processKeyFingerprint( sessionInfoPtr );
		}
	else
		sessionInfoPtr->iCertRequest = cryptCert;

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								   IN const void *data,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type > CRYPT_ATTRIBUTE_NONE && type < CRYPT_ATTRIBUTE_LAST );

	if( type != CRYPT_SESSINFO_USERNAME && \
		type != CRYPT_SESSINFO_PRIVATEKEY && \
		type != CRYPT_SESSINFO_REQUEST )
		return( CRYPT_OK );

	/* If it's a user name, used for the SCEP transaction ID, make sure that
	   it meets the SCEP requirements */
	if( type == CRYPT_SESSINFO_USERNAME )
		{
		status = processUserName( sessionInfoPtr, 
								  ( ( MESSAGE_DATA * ) data )->data,
								  ( ( MESSAGE_DATA * ) data )->length );
		if( cryptStatusError( status ) )
			{
			/* A return status of OK_SPECIAL is valid because it means that 
			   the name has been handled internally */
			if( status == OK_SPECIAL )
				return( OK_SPECIAL );

			return( CRYPT_ARGERROR_STR1 );
			}

		return( CRYPT_OK );
		}

	/* If it's a certificate request, check that appropriate attributes are/
	   aren't present */
	if( type == CRYPT_SESSINFO_REQUEST )
		{
		static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
		const CRYPT_CERTIFICATE cryptCertRequest = \
									*( ( CRYPT_CERTIFICATE * ) data );
		MESSAGE_DATA msgData;

		/* Make sure that the SCEP protocol attributes, which get stuffed 
		   into the request alongside the standard PKCS #10 data, haven't 
		   been set since we need to set them ourselves */
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cryptCertRequest, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_CHALLENGEPASSWORD );
		if( cryptStatusOK( status ) )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_CHALLENGEPASSWORD,
						  CRYPT_ERRTYPE_ATTR_PRESENT );
			return( CRYPT_ARGERROR_NUM1 );
			}

		/* Make sure that at least a CN and a public key are present.  
		   Normally this would be verified when the request is signed but 
		   since we have to use an unsigned/incomplete request for SCEP
		   purposes we have to explicitly perform a check here that would 
		   normally be performed by the certificate-processing code.

		   Unfortunately there isn't any obvius way to check for the 
		   presence of a public key in an unsigned certificate request,
		   since it hasn't been signed yet neither the CRYPT_IATTRIBUTE_SPKI 
		   nor the CRYPT_IATTRIBUTE_CERTKEYALGO are readable so we just have
		   to leave this to be caught by the certificate-request completion
		   code */
		status = krnlSendMessage( cryptCertRequest, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &nameValue, 
								  CRYPT_ATTRIBUTE_CURRENT );
		if( cryptStatusOK( status ) )
			{
			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( cryptCertRequest, 
									  IMESSAGE_GETATTRIBUTE_S, &msgData, 
									  CRYPT_CERTINFO_COMMONNAME );
			}
		if( cryptStatusError( status ) )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_COMMONNAME,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ARGERROR_NUM1 );
			}

		return( CRYPT_OK );
		}

	ENSURES( type == CRYPT_SESSINFO_PRIVATEKEY );

	/* Make sure that there aren't any conflicts with existing attributes */
	if( !checkAttributesConsistent( sessionInfoPtr, type ) )
		return( CRYPT_ERROR_INITED );

	/* If it's a client key, make sure that it meets the requirements, 
	   depending on whether we're doing an initialisation or renewal 
	   request.  If the request type hasn't been set yet then we default to  
	   an initialisation request, the most common type */
	if( !isServer( sessionInfoPtr ) )
		{
		const CRYPT_CONTEXT cryptContext = *( ( CRYPT_CONTEXT * ) data );

		if( !checkPrivateKey( cryptContext, 
				( scepInfo->requestType == CRYPT_REQUESTTYPE_CERTIFICATE || \
				  scepInfo->requestType == CRYPT_REQUESTTYPE_KEYUPDATE ) ? \
				  TRUE : FALSE ) )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ARGERROR_NUM1 );
			}
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodSCEP( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		TRUE,						/* Request-response protocol */
		SESSION_ISHTTPTRANSPORT,	/* Flags */
		80,							/* HTTP port */
		SESSION_NEEDS_USERID |		/* Client attributes */
			SESSION_NEEDS_KEYORPASSWORD | \
			SESSION_NEEDS_PRIVKEYSIGN | \
			SESSION_NEEDS_PRIVKEYCRYPT | \
			SESSION_NEEDS_REQUEST,
		SESSION_NEEDS_PRIVATEKEY |	/* Server attributes */
			SESSION_NEEDS_PRIVKEYSIGN | \
			SESSION_NEEDS_PRIVKEYCRYPT | \
			SESSION_NEEDS_PRIVKEYCERT | \
			SESSION_NEEDS_PRIVKEYCACERT | \
			SESSION_NEEDS_CERTSTORE,
		1, 1, 1						/* Version 1 */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers */
	sessionInfoPtr->protocolInfo = &protocolInfo;
	if( isServer( sessionInfoPtr ) )
		initSCEPserverProcessing( sessionInfoPtr );
	else
		initSCEPclientProcessing( sessionInfoPtr );
	sessionInfoPtr->getAttributeFunction = getAttributeFunction;
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;
	sessionInfoPtr->checkAttributeFunction = checkAttributeFunction;

	return( CRYPT_OK );
	}
#endif /* USE_SCEP */
