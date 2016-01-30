/****************************************************************************
*																			*
*								Write CMP Messages							*
*						Copyright Peter Gutmann 1999-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

/* Enabling the following define forces the use of full headers at all times.
   cryptlib always sends minimal headers once it detects that the other side 
   is using cryptlib, ommitting as much of the unnecessary junk as possible, 
   which significantly reduces the overall message size */

/* #define USE_FULL_HEADERS */

#ifdef USE_CMP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Write full certificate ID information.  This is written as an attribute 
   in the generalInfo field of the message header to allow unambiguous
   identification of the signing certificate, which the standard CMP format 
   can't do.  Although CMP uses a gratuitously incompatible definition of 
   the standard attribute type (calling it InfoTypeAndValue), it's possible 
   to shoehorn a standard attribute type in by taking the "ANY" in "ANY 
   DEFINED BY x" to mean "SET OF AttributeValue" (for once the use of 
   obsolete ASN.1 is a help, since it's so imprecise that we can shovel in 
   anything and it's still valid):

	SigningCertificate ::=  SEQUENCE {
		certs			SEQUENCE OF ESSCertID	-- Size (1)
		}

	ESSCertID ::= SEQUENCE {
		certID			OCTET STRING
		}

   All that we really need to identify certificates is the certificate ID, 
   so instead of writing a full ESSCertID (which also contains an optional 
   incompatible reinvention of the CMS IssuerAndSerialNumber) we write the 
   sole mandatory field, the certificate hash, which also keeps the overall 
   size down.
   
   This is further complicated though by the fact that certificate attributes
   are defined as SET OF while CMS attributes are defined as SEQUENCE (and of 
   course CMP has to gratuitously invent its own type which is neither of the 
   two).  This means that the read code would need to know whether a given 
   OID corresponds to a certificate or CMS attribute and read it 
   appropriately.  Because this is just too much of a mess, we pretend that 
   all attributes are certificate attributes (since this is a PKIX protocol)
   and encode them as a uniform SET OF */

CHECK_RETVAL \
static int sizeofCertID( IN_HANDLE const CRYPT_CONTEXT iCryptCert )
	{
	const int essCertIDSize = objSize( objSize( objSize( objSize( 20 ) ) ) );
					/* Infinitely-nested SHA-1 hash */

	REQUIRES( isHandleRangeValid( iCryptCert ) );

	return( objSize( sizeofOID( OID_ESS_CERTID ) + \
					 sizeofObject( essCertIDSize ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeCertID( INOUT STREAM *stream, 
						IN_HANDLE const CRYPT_CONTEXT iCryptCert )
	{
	MESSAGE_DATA msgData;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int essCertIDSize, payloadSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* Find out how big the payload will be */
	setMessageData( &msgData, certHash, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusError( status ) )
		return( status );
	essCertIDSize = ( int ) sizeofObject( msgData.length );
	payloadSize = objSize( objSize( objSize( essCertIDSize ) ) );

	/* Write the signing certificate ID information */
	writeSequence( stream, sizeofOID( OID_ESS_CERTID ) + \
						   ( int ) sizeofObject( payloadSize ) );
	writeOID( stream, OID_ESS_CERTID );
	writeSet( stream, payloadSize );
	writeSequence( stream, objSize( objSize( essCertIDSize ) ) );
	writeSequence( stream, objSize( essCertIDSize ) );
	writeSequence( stream, essCertIDSize );
	return( writeOctetString( stream, certHash, msgData.length, 
							  DEFAULT_TAG ) );
	}

/* Initialise the information needed to send client/server DNs in the PKI 
   header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
static int initDNInfo( INOUT SESSION_INFO *sessionInfoPtr,
					   OUT_HANDLE_OPT CRYPT_HANDLE *senderNameObject,
					   OUT_HANDLE_OPT CRYPT_HANDLE *recipNameObject,
					   OUT_LENGTH_SHORT_Z int *senderNameLength,
					   OUT_LENGTH_SHORT_Z int *recipNameLength,
					   const BOOLEAN isInitialClientMessage,
					   const BOOLEAN isClientCryptOnlyKey )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( senderNameObject, sizeof( CRYPT_HANDLE ) ) );
	assert( isWritePtr( recipNameObject, sizeof( CRYPT_HANDLE ) ) );
	assert( isWritePtr( senderNameLength, sizeof( int ) ) );
	assert( isWritePtr( recipNameLength, sizeof( int ) ) );

	/* Clear return values */
	*senderNameObject = *recipNameObject = CRYPT_ERROR;
	*senderNameLength = *recipNameLength = 0;

	/* Get the objects that we'll be using for our source of DN 
	   information */
	if( isServer( sessionInfoPtr ) )
		{
		*senderNameObject = sessionInfoPtr->privateKey;
		*recipNameObject = sessionInfoPtr->iCertResponse;
		}
	else
		{
		*senderNameObject = isClientCryptOnlyKey ? \
							sessionInfoPtr->iAuthOutContext : \
							sessionInfoPtr->iCertRequest;
		*recipNameObject = sessionInfoPtr->iAuthInContext;
		}

	/* Get the sender DN information */
	setMessageData( &msgData, NULL, 0 );
	status = krnlSendMessage( *senderNameObject, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_SUBJECT );
	if( status == CRYPT_ERROR_NOTFOUND && isInitialClientMessage )
		{
		/* If there's no subject DN present and it's the first message in a 
		   client's ir exchange, this isn't an error because the subject 
		   usually won't know their DN yet.  That's the theory anyway, 
		   some X.500-obsessive servers will reject a message with no 
		   sender name but there isn't really anything that we can do about 
		   this, particularly since we can't tell in advance what beaviour 
		   the server will exhibit */
		if( sessionInfoPtr->iCertResponse == CRYPT_ERROR )
			{
			*senderNameObject = CRYPT_ERROR;
			msgData.length = ( int ) sizeofObject( 0 );
			status = CRYPT_OK;
			}
		else
			{
			/* Try again with the response from the server, which contains 
			   our newly-allocated DN */
			*senderNameObject = sessionInfoPtr->iCertResponse;
			status = krnlSendMessage( *senderNameObject,
									  IMESSAGE_GETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_SUBJECT );
			}
		}
	if( cryptStatusError( status ) )
		return( status );
	*senderNameLength = msgData.length;

	/* Get the recipient DN information */
	setMessageData( &msgData, NULL, 0 );
	if( *recipNameObject != CRYPT_ERROR )
		{
		status = krnlSendMessage( *recipNameObject, IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_IATTRIBUTE_SUBJECT );
		}
	else
		{
		/* If we're sending an error response there may not be any recipient 
		   name information present yet if the error occurred before the 
		   recipient information could be established, and if this is a MAC-
		   authenticated PKIBoot we don't have the CA's certificate yet so 
		   we don't know its DN.  To work around this we send a zero-length 
		   DN (this is one of those places where an optional field is 
		   specified as being mandatory, to lend balance to the places where 
		   mandatory fields are specified as optional) */
		msgData.length = ( int ) sizeofObject( 0 );
		}
	if( cryptStatusError( status ) )
		return( status );
	*recipNameLength = msgData.length;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Write a PKI Header							*
*																			*
****************************************************************************/

/* Write a PKI header.  Fields marked with a * are unnecessary and are only 
   sent when we're not sending minimal headers.  Fields marked with a + are
   only sent in the first message or when not sending minimal headers:

	header				SEQUENCE {
		version			INTEGER (2),
	   *sender		[4]	EXPLICIT DirectoryName,	-- DN of initiator
	   *recipient	[4]	EXPLICIT DirectoryName,	-- DN of responder
		protAlgo	[1]	EXPLICIT AlgorithmIdentifier,
	   +protKeyID	[2] EXPLICIT OCTET STRING,
		transID		[4] EXPLICIT OCTET STRING SIZE (16),-- Random/copied from sender
	   *nonce		[5] EXPLICIT OCTET STRING SIZE (16),-- Random
	   *nonceX		[6] EXPLICIT OCTET STRING SIZE (n),	-- Copied from sender
		generalInfo	[8] EXPLICIT SEQUENCE OF Info OPT	-- cryptlib-specific info
		} 

   The handling can get a bit complex if we're writing a header in response
   to an error in reading the other side's header.  Since CMP includes such 
   a large amount of unnecessary or redundant information, it's not really 
   possible to detect in advance if we've got enough information to send a
   header or not, the best that we can do is to require that enough of the 
   header fields have been read (indicated by the 'headerRead' flag in the 
   protocol information) before we try and create our own header in 
   response */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writePkiHeader( INOUT STREAM *stream, 
						   INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_HANDLE senderNameObject = DUMMY_INIT, recipNameObject = DUMMY_INIT;
	STREAM nullStream;
	MESSAGE_DATA msgData;
#ifdef USE_FULL_HEADERS
	const BOOLEAN sendFullHeader = TRUE;
#else
	BOOLEAN sendFullHeader = FALSE;
#endif /* USE_FULL_HEADERS */
	BOOLEAN sendClibID = FALSE, sendCertID = FALSE, sendMacInfo = FALSE;
	BOOLEAN sendUserID = FALSE;
	int senderNameLength, recipNameLength, attributeLength = 0;
	int protInfoLength = DUMMY_INIT, totalLength, hashAlgo, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Determine which of the many unnecessary and inexplicable bits of the 
	   CMP header we actually have to send:

		sendCertID: Sent on the first message where it's required (which 
			isn't necessarily the first message in an exchange, for example 
			it's not used in an ir/ip), either to identify the CA's cert in 
			a CTL sent in a PKIBoot response or to identify the signing 
			certificate when we're using signature-based message 
			authentication.

		sendClibID: Sent on the first message to tell the other side that 
			this is a cryptlib client/server.

		sendFullHeader: Sent if the other side isn't running cryptlib, unless
			we're doing PKIBoot, for which we couldn't send full headers even 
			if we wanted to 
	
		sendMacInfo: Sent if we're using MAC integrity protection and the
			the other side isn't running cryptlib, or if this is the first 
			message.

		sendUserID: Sent on the first message or if we're sending full 
			headers, provided that it's actually available to send */
	if( !( sessionInfoPtr->protocolFlags & CMP_PFLAG_CERTIDSENT ) && \
		( ( isServer( sessionInfoPtr ) && \
			protocolInfo->operation == CTAG_PB_GENM ) || \
		  !protocolInfo->useMACsend ) )
		sendCertID = TRUE;
	if( !( sessionInfoPtr->protocolFlags & CMP_PFLAG_CLIBIDSENT ) )
		sendClibID = TRUE;
#ifndef USE_FULL_HEADERS
	if( !protocolInfo->isCryptlib && \
		protocolInfo->operation != CTAG_PB_GENM )
		sendFullHeader = TRUE;
#endif /* !USE_FULL_HEADERS */
	if( protocolInfo->useMACsend && \
		!( protocolInfo->isCryptlib && \
		   ( sessionInfoPtr->protocolFlags & CMP_PFLAG_MACINFOSENT ) ) )
		sendMacInfo = TRUE;
	if( ( sendFullHeader || \
		  !( sessionInfoPtr->protocolFlags & CMP_PFLAG_USERIDSENT ) ) && \
		( protocolInfo->userIDsize > 0 ) )
		sendUserID = TRUE;

	REQUIRES( !sendFullHeader || !protocolInfo->headerRead || \
			  ( protocolInfo->userIDsize > 0 && \
				protocolInfo->userIDsize < MAX_INTLENGTH_SHORT ) );
	REQUIRES( protocolInfo->transIDsize > 0 && \
			  protocolInfo->transIDsize < MAX_INTLENGTH_SHORT );

	/* Get any other state information that we may need */
	status = krnlSendMessage( sessionInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &hashAlgo, 
							  CRYPT_OPTION_ENCR_HASH );
	ENSURES( cryptStatusOK( status ) );
	protocolInfo->hashAlgo = hashAlgo;	/* int vs.enum */

	/* Determine how big the sender and recipient information will be.  We 
	   shouldn't need to send a recipient name for an ir because it won't
	   usually be known yet, but various implementations can't handle a 
	   zero-length GeneralName so we supply it if it's available even though 
	   it's redundant */
	if( sendFullHeader )
		{
		status = initDNInfo( sessionInfoPtr, &senderNameObject, 
							 &recipNameObject, &senderNameLength, 
							 &recipNameLength, 
							 ( protocolInfo->operation == CTAG_PB_IR ) ? \
								TRUE : FALSE,
							 protocolInfo->cryptOnlyKey );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* We're not using sender or recipient information since it doesn't 
		   serve any useful purpose, just set the fields to an empty 
		   SEQUENCE */
		senderNameLength = recipNameLength = sizeofObject( 0 );
		}

	/* Determine how big the remaining header data will be */
	sMemNullOpen( &nullStream );
	if( protocolInfo->useMACsend )
		{
		status = writeMacInfo( &nullStream, protocolInfo, sendMacInfo );
		}
	else
		{
		status = writeContextAlgoID( &nullStream, protocolInfo->authContext, 
									 protocolInfo->hashAlgo );
		}
	if( cryptStatusOK( status ) )
		protInfoLength = stell( &nullStream );
	sMemClose( &nullStream );
	if( cryptStatusError( status ) )
		return( status );
	if( sendClibID )
		{
		attributeLength += sizeofObject( \
								sizeofOID( OID_CRYPTLIB_PRESENCECHECK ) + \
								sizeofObject( 0 ) );
		}
	if( sendCertID )
		{
		const int certIDsize = sizeofCertID( protocolInfo->authContext );

		ENSURES( certIDsize > 0 && certIDsize < MAX_INTLENGTH_SHORT );

		attributeLength += certIDsize;
		}
	totalLength = sizeofShortInteger( CMP_VERSION ) + \
				  objSize( senderNameLength ) + objSize( recipNameLength ) + \
				  objSize( protInfoLength ) + \
				  objSize( sizeofObject( protocolInfo->transIDsize ) );
	if( sendUserID )
		totalLength += objSize( sizeofObject( protocolInfo->userIDsize ) );
	if( sendFullHeader )
		{
		if( protocolInfo->senderNonceSize > 0 )
			totalLength += objSize( \
								sizeofObject( protocolInfo->senderNonceSize ) );
		if( protocolInfo->recipNonceSize > 0 )
			totalLength += objSize( \
								sizeofObject( protocolInfo->recipNonceSize ) );
		}
	if( attributeLength > 0 )
		totalLength += objSize( objSize( attributeLength ) );

	/* Perform an early check for data-size problems before we go through 
	   all of the following code */
	if( sizeofObject( totalLength ) <= 0 || \
		sizeofObject( totalLength ) > sMemDataLeft( stream ) )
		return( CRYPT_ERROR_OVERFLOW );

	/* Write the PKI header wrapper, version information, and sender and 
	   recipient names if there's name information present */
	writeSequence( stream, totalLength );
	writeShortInteger( stream, CMP_VERSION, DEFAULT_TAG );
	if( sendFullHeader )
		{
		writeConstructed( stream, senderNameLength, 4 );
		if( senderNameObject != CRYPT_ERROR )
			{
			status = exportAttributeToStream( stream, senderNameObject, 
											  CRYPT_IATTRIBUTE_SUBJECT );
			if( cryptStatusError( status ) )
				return( status );
			}
		else
			writeSequence( stream, 0 );
		writeConstructed( stream, recipNameLength, 4 );
		if( recipNameObject != CRYPT_ERROR )
			{
			status = exportAttributeToStream( stream, recipNameObject, 
											  CRYPT_IATTRIBUTE_SUBJECT );
			}
		else
			status = writeSequence( stream, 0 );
		}
	else
		{
		/* This is one of the portions of CMP where an optional field is 
		   marked as mandatory, to balance out the mandatory fields that are 
		   marked as optional.  To work around this we write the names as 
		   zero-length DNs */
		writeConstructed( stream, senderNameLength, 4 );
		writeSequence( stream, 0 );
		writeConstructed( stream, recipNameLength, 4 );
		status = writeSequence( stream, 0 );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the protection information, assorted nonces and IDs, and extra
	   information that the other side may be able to make use of */
	writeConstructed( stream, protInfoLength, CTAG_PH_PROTECTIONALGO );
	if( protocolInfo->useMACsend )
		{
		status = writeMacInfo( stream, protocolInfo, sendMacInfo );
		sessionInfoPtr->protocolFlags |= CMP_PFLAG_MACINFOSENT;
		}
	else
		{
		status = writeContextAlgoID( stream, protocolInfo->authContext, 
									 protocolInfo->hashAlgo );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( sendUserID )
		{
		/* We're using full headers or we're the client sending our first
		   message, identify the sender key */
		writeConstructed( stream, objSize( protocolInfo->userIDsize ),
						  CTAG_PH_SENDERKID );
		writeOctetString( stream, protocolInfo->userID,
						  protocolInfo->userIDsize, DEFAULT_TAG );
		DEBUG_PRINT(( "%s: Writing userID.\n",
					  protocolInfo->isServer ? "SVR" : "CLI" ));
		DEBUG_DUMP_HEX( protocolInfo->isServer ? "SVR" : "CLI", 
						protocolInfo->userID, protocolInfo->userIDsize );
		sessionInfoPtr->protocolFlags |= CMP_PFLAG_USERIDSENT;
		}
	writeConstructed( stream, objSize( protocolInfo->transIDsize ),
					  CTAG_PH_TRANSACTIONID );
	status = writeOctetString( stream, protocolInfo->transID,
							   protocolInfo->transIDsize, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	if( sendFullHeader )
		{
		if( protocolInfo->senderNonceSize > 0 )
			{
			/* We're using nonces, generate a new sender nonce (the initial 
			   nonce will have been set when the protocol state was 
			   initialised) */
			setMessageData( &msgData, protocolInfo->senderNonce,
							protocolInfo->senderNonceSize );
			krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
			writeConstructed( stream, 
							  objSize( protocolInfo->senderNonceSize ),
							  CTAG_PH_SENDERNONCE );
			status = writeOctetString( stream, protocolInfo->senderNonce,
									   protocolInfo->senderNonceSize, 
									   DEFAULT_TAG );
			}
		if( protocolInfo->recipNonceSize > 0 )
			{
			writeConstructed( stream, 
							  objSize( protocolInfo->recipNonceSize ),
							  CTAG_PH_RECIPNONCE );
			status = writeOctetString( stream, protocolInfo->recipNonce,
									   protocolInfo->recipNonceSize, 
									   DEFAULT_TAG );
			}
		}
	if( cryptStatusError( status ) )
		return( status );
	if( attributeLength > 0 )
		{
		ENSURES( sendClibID || sendCertID );

		/* We haven't sent any messages yet, let the other side know that 
		   we're running cryptlib and identify our signing certificate as
		   required */
		writeConstructed( stream, objSize( attributeLength ),
						  CTAG_PH_GENERALINFO );
		writeSequence( stream, attributeLength );
		if( sendClibID )
			{
			sessionInfoPtr->protocolFlags |= CMP_PFLAG_CLIBIDSENT;
			writeSequence( stream, sizeofOID( OID_CRYPTLIB_PRESENCECHECK ) + \
								   sizeofObject( 0 ) );
			writeOID( stream, OID_CRYPTLIB_PRESENCECHECK );
			status = writeSet( stream, 0 );
			}
		if( sendCertID )
			{
			sessionInfoPtr->protocolFlags |= CMP_PFLAG_CERTIDSENT;
			status = writeCertID( stream, protocolInfo->authContext );
			}
		}
	return( status );
	}

/****************************************************************************
*																			*
*							Write a PKI Message								*
*																			*
****************************************************************************/

/* Write a PKI message:

	PkiMessage ::= SEQUENCE {
		header			PKIHeader,
		body			CHOICE { [0]... [24]... },
		protection	[0]	BIT STRING
		} */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writePkiMessage( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT CMP_PROTOCOL_INFO *protocolInfo,
					 IN_ENUM( CMPBODY ) const CMPBODY_TYPE bodyType )
	{
	WRITEMESSAGE_FUNCTION writeMessageFunction;
	BYTE protInfo[ 64 + MAX_PKCENCRYPTED_SIZE + 8 ], headerBuffer[ 8 + 8 ];
	STREAM stream;
	int headerSize = DUMMY_INIT, protInfoSize, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( bodyType > CMPBODY_NONE && bodyType < CMPBODY_LAST );

	DEBUG_PRINT(( "%s: Writing message body type %d.\n",
				  protocolInfo->isServer ? "SVR" : "CLI", bodyType ));

	/* Write the header and payload so that we can MAC/sign it */
	sMemOpen( &stream, sessionInfoPtr->receiveBuffer,
			  sessionInfoPtr->receiveBufSize );
	status = writePkiHeader( &stream, sessionInfoPtr, protocolInfo );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}

	/* Write the message data */
	writeMessageFunction = getMessageWriteFunction( bodyType, 
										isServer( sessionInfoPtr ) );
	ENSURES( writeMessageFunction != NULL );
	status = writeMessageFunction( &stream, sessionInfoPtr, protocolInfo );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}

	/* Generate the MAC or signature as appropriate */
	if( protocolInfo->useMACsend )
		{
		status = writeMacProtinfo( protocolInfo->iMacContext,
						sessionInfoPtr->receiveBuffer, stell( &stream ),
						protInfo, 64 + MAX_PKCENCRYPTED_SIZE, &protInfoSize );
		}
	else
		{
		status = writeSignedProtinfo( protocolInfo->authContext, 
						protocolInfo->hashAlgo, protocolInfo->hashParam,
						sessionInfoPtr->receiveBuffer, stell( &stream ),
						protInfo, 64 + MAX_PKCENCRYPTED_SIZE, &protInfoSize );
		}
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}

	/* Attach the MAC/signature to the payload */
	writeConstructed( &stream, protInfoSize, CTAG_PM_PROTECTION );
	status = swrite( &stream, protInfo, protInfoSize );
	if( cryptStatusOK( status ) )
		sessionInfoPtr->receiveBufEnd = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the wrapper and move it onto the front of the message:

		receiveBuffer								  receiveBufSize
			|												|
			v												v
			+-------+-----------------------+---------------+
			|		|						|				|
			+-------+-----------------------+---------------+
			|<--+-->|<--- receiveBufend --->|
				|
			headerSize

	   Unfortunately we can't just assume a fixed-length header because a 
	   few messages (conf and pkiConf) will be short, leading to a 1-byte 
	   length rather than the more typical 2-byte length */
	sMemOpen( &stream, headerBuffer, 8 );
	status = writeSequence( &stream, sessionInfoPtr->receiveBufEnd );
	if( cryptStatusOK( status ) )
		headerSize = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );
	REQUIRES( rangeCheck( headerSize, sessionInfoPtr->receiveBufEnd,
						  sessionInfoPtr->receiveBufSize ) );
	memmove( sessionInfoPtr->receiveBuffer + headerSize,
			 sessionInfoPtr->receiveBuffer,
			 sessionInfoPtr->receiveBufEnd );
	memcpy( sessionInfoPtr->receiveBuffer, headerBuffer, headerSize );
	sessionInfoPtr->receiveBufEnd += headerSize;

	return( CRYPT_OK );
	}
#endif /* USE_CMP */
