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

/* Check that the CA's certificate can also sign and encrypt data.  This is
   normally a really bad idea for CA certificates but is required by the 
   SCEP protocol.  We don't check for the object being a CA certificate 
   because we could be dealing with an RA, which isn't necessarily a CA */

CHECK_RETVAL_BOOL \
BOOLEAN checkSCEPCACert( IN_HANDLE const CRYPT_CERTIFICATE iCaCert,
 						 IN_FLAGS( KEYMGMT ) const int options )
	{
	REQUIRES_B( isHandleRangeValid( iCaCert ) );
	REQUIRES_B( options == KEYMGMT_FLAG_NONE || \
				options == KEYMGMT_FLAG_USAGE_CRYPT || \
				options == KEYMGMT_FLAG_USAGE_SIGN );

	krnlSendMessage( iCaCert, IMESSAGE_SETATTRIBUTE,
					 MESSAGE_VALUE_CURSORFIRST,
					 CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( options != KEYMGMT_FLAG_USAGE_SIGN && \
		cryptStatusError( \
			krnlSendMessage( iCaCert, IMESSAGE_CHECK, NULL,
							 MESSAGE_CHECK_PKC_ENCRYPT ) ) )
		return( FALSE );
	if( options != KEYMGMT_FLAG_USAGE_CRYPT && \
		cryptStatusError( \
			krnlSendMessage( iCaCert, IMESSAGE_CHECK, NULL,
							 MESSAGE_CHECK_PKC_SIGCHECK ) ) )
		return( FALSE );

	return( TRUE );
	}

/* Generate/check the server certificate fingerprint */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int processKeyFingerprint( INOUT SESSION_INFO *sessionInfoPtr )
	{
	const ATTRIBUTE_LIST *fingerprintPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT );
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
									  CRYPT_SESSINFO_SERVER_FINGERPRINT,
									  certFingerprint, msgData.length );
			}
		}

	return( CRYPT_OK );
	}

/* Create SCEP signing attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int createScepAttributes( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT SCEP_PROTOCOL_INFO *protocolInfo,
						  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iScepAttributes,
						  const BOOLEAN isInitiator, 
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
		const char *messageType = isInitiator ? \
			( ( sessionInfoPtr->sessionSCEP->flags & SCEP_PFLAG_PENDING ) ? \
				MESSAGETYPE_GETCERTINITIAL : MESSAGETYPE_PKCSREQ ) : \
			MESSAGETYPE_CERTREP;

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

	/* Add the message status if we're the responder */
	if( !isInitiator )
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

	/* Add the nonce, identified as a sender nonce if we're the initiator and 
	   a recipient nonce if we're the responder */
	if( isInitiator )
		{
		/* If we're the initiator, generate a new nonce */
		setMessageData( &msgData, protocolInfo->nonce, SCEP_NONCE_SIZE );
		krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
						 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		protocolInfo->nonceSize = SCEP_NONCE_SIZE;
		}
	else
		{
		/* We're the responder, use the initiator's nonce */
		setMessageData( &msgData, protocolInfo->nonce, 
						protocolInfo->nonceSize );
		}
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, isInitiator ? \
								CRYPT_CERTINFO_SCEP_SENDERNONCE : \
								CRYPT_CERTINFO_SCEP_RECIPIENTNONCE );
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
static int setAttributeFunction( INOUT SESSION_INFO *sessionInfoPtr,
								 IN const void *data,
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	CRYPT_CERTIFICATE cryptCert = *( ( CRYPT_CERTIFICATE * ) data );
	int value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( data, sizeof( int ) ) );

	REQUIRES( type == CRYPT_SESSINFO_REQUEST || \
			  type == CRYPT_SESSINFO_CACERTIFICATE );

	/* Make sure that there aren't any conflicts with existing attributes */
	if( !checkAttributesConsistent( sessionInfoPtr, type ) )
		return( CRYPT_ERROR_INITED );

	if( type == CRYPT_SESSINFO_CMP_PRIVKEYSET )
		{
		CRYPT_CERTIFICATE privKeyset = *( ( CRYPT_CERTIFICATE * ) data );

		/* Remember that we're using plug-and-play PKI functionality */
		sessionInfoPtr->sessionSCEP->flags |= SCEP_PFLAG_PNPPKI;

		krnlSendNotifier( privKeyset, IMESSAGE_INCREFCOUNT );
		sessionInfoPtr->privKeyset = privKeyset;
		return( CRYPT_OK );
		}

	/* Make sure that everything is set up ready to go */
	status = krnlSendMessage( cryptCert, IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_CERTINFO_IMMUTABLE );
	if( type == CRYPT_SESSINFO_CACERTIFICATE )
		{
		if( cryptStatusError( status ) || !value )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		/* The PKCS #10 request has to be unsigned so that we can add the 
		   challengePassword */
		if( cryptStatusError( status ) || value )
			return( CRYPT_ARGERROR_NUM1 );
		}
	if( type == CRYPT_SESSINFO_CACERTIFICATE )
		{
		/* Make sure that the CA certificate has the unusual additional 
		   capabilities that are required to meet the SCEP protocol 
		   requirements.  Microsoft's SCEP server generates and uses 
		   CA certs that only have CRYPT_KEYUSAGE_KEYENCIPHERMENT set but 
		   not CRYPT_KEYUSAGE_DIGITALSIGNATURE, making them unusable for 
		   SCEP, so this check will reject anything coming from a Microsoft 
		   server unless the certificate compliance level is set to 
		   oblivious */
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
		const BYTE *userName = ( ( MESSAGE_DATA * ) data )->data;
		const int userNameLength = ( ( MESSAGE_DATA * ) data )->length;
		int index;

		/* Because users won't necessarily have a user name/transaction ID 
		   to use we allow them to specify the value as "[Autodetect]" and 
		   replace it with a base64'd nonce (to meet the encoding 
		   requirements below) */
		if( userNameLength == 12 && !memcmp( userName, "[Autodetect]", 12 ) )
			{
			MESSAGE_DATA msgData;
			BYTE nonce[ 16 + 8 ];
			char transID[ CRYPT_MAX_TEXTSIZE + 8 ];
			int transIDlength;

			/* The caller has explicitly indicated that they want us to 
			   generate a transaction ID for them, generate it as a random
			   text string */
			setMessageData( &msgData, nonce, 16 );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
									  IMESSAGE_GETATTRIBUTE_S, &msgData, 
									  CRYPT_IATTRIBUTE_RANDOM_NONCE );
			if( cryptStatusOK( status ) )
				status = base64encode( transID, CRYPT_MAX_TEXTSIZE, 
									   &transIDlength, nonce, 16, 
									   CRYPT_CERTTYPE_NONE );
			if( cryptStatusOK( status ) )
				status = addSessionInfoEx( &sessionInfoPtr->attributeList,
										   CRYPT_SESSINFO_USERNAME, 
										   transID, 16, ATTR_FLAG_NONE );
			if( cryptStatusError( status ) )
				return( status );

			/* Tell the caller that the attribute was processed 
			   internally */
			return( OK_SPECIAL );
			}

		/* For some bizarre reason SCEP encodes it's transaction ID (which 
		   is our user name) as a PrintableString (even though the 
		   specification suggests that the value be created from a hash of 
		   the public key, which doesn't fit into a PrintableString) so we 
		   have to filter any supplied value to make sure that it can be 
		   encoded when we send it */
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

	/* If it's a certificate request, make sure that the SCEP protocol 
	   attributes, which get stuffed into the request alongside the standard 
	   PKCS #10 data, haven't been set since we need to set them ourselves */
	if( type == CRYPT_SESSINFO_REQUEST )
		{
		const CRYPT_CERTIFICATE cryptCertRequest = \
									*( ( CRYPT_CERTIFICATE * ) data );
		MESSAGE_DATA msgData;

		/* Make sure that this attribute isn't already present */
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( cryptCertRequest, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_CHALLENGEPASSWORD );
		if( cryptStatusOK( status ) )
			{
			setErrorInfo( sessionInfoPtr, CRYPT_CERTINFO_CHALLENGEPASSWORD,
						  CRYPT_ERRTYPE_ATTR_PRESENT );
			return( CRYPT_ARGERROR_NUM1 );
			}

		return( CRYPT_OK );
		}

	ENSURES( type == CRYPT_SESSINFO_PRIVATEKEY );

	/* Make sure that there aren't any conflicts with existing attributes */
	if( !checkAttributesConsistent( sessionInfoPtr, type ) )
		return( CRYPT_ERROR_INITED );

	/* If it's a client key, make sure that there's no certificate 
	   attached */
	if( !isServer( sessionInfoPtr ) )
		{
		const CRYPT_CONTEXT cryptContext = *( ( CRYPT_CONTEXT * ) data );
		int value;

		status = krnlSendMessage( cryptContext, IMESSAGE_GETATTRIBUTE, 
								  &value, CRYPT_CERTINFO_CERTTYPE );
		if( cryptStatusOK( status ) )
			return( CRYPT_ARGERROR_NUM1 );
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
			SESSION_NEEDS_PASSWORD | \
			SESSION_NEEDS_PRIVATEKEY | \
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
	sessionInfoPtr->setAttributeFunction = setAttributeFunction;
	sessionInfoPtr->checkAttributeFunction = checkAttributeFunction;

	return( CRYPT_OK );
	}
#endif /* USE_SCEP */
