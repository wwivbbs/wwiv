/****************************************************************************
*																			*
*								Read CMP Messages							*
*						Copyright Peter Gutmann 1999-2009					*
*																			*
****************************************************************************/

#include <stdio.h>
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

#ifdef USE_CMP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Read the kitchen-sink field in the PKI header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readGeneralInfoAttribute( INOUT STREAM *stream, 
									 INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	BYTE oid[ MAX_OID_SIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Read the attribute.  Since there are only two attribute types that we 
	   use, we hardcode the read in here rather than performing a general-
	   purpose attribute read */
	readSequence( stream, NULL );
	status = readEncodedOID( stream, oid, MAX_OID_SIZE, &length, 
							 BER_OBJECT_IDENTIFIER );
	if( cryptStatusError( status ) )
		return( status );

	/* Process the cryptlib presence-check value */
	if( length == sizeofOID( OID_CRYPTLIB_PRESENCECHECK ) && \
		!memcmp( oid, OID_CRYPTLIB_PRESENCECHECK, length ) )
		{
		/* The other side is running cryptlib, we can make some common-sense 
		   assumptions about its behaviour */
		protocolInfo->isCryptlib = TRUE;
		return( readUniversal( stream ) );			/* Attribute */
		}

	/* Check for the ESSCertID, which fixes CMP's broken certificate 
	   identification mechanism */
	if( length == sizeofOID( OID_ESS_CERTID ) && \
		!memcmp( oid, OID_ESS_CERTID, length ) )
		{
		BYTE certID[ CRYPT_MAX_HASHSIZE + 8 ];
		int certIDsize, endPos;

		/* Extract the certificate hash from the ESSCertID */
		readSet( stream, NULL );					/* Attribute */
		readSequence( stream, NULL );				/* SigningCerts */
		readSequence( stream, NULL );				/* Certs */
		status = readSequence( stream, &length );	/* ESSCertID */
		if( cryptStatusError( status ) )
			return( status );
		endPos = stell( stream ) + length;
		status = readOctetString( stream, certID, &certIDsize, 
								  KEYID_SIZE, KEYID_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		if( protocolInfo->certIDsize != KEYID_SIZE || \
			memcmp( certID, protocolInfo->certID, KEYID_SIZE ) )
			{
			/* The certificate used for authentication purposes has changed,
			   remember the new certID */
			memcpy( protocolInfo->certID, certID, KEYID_SIZE );
			protocolInfo->certIDsize = KEYID_SIZE;
			protocolInfo->certIDchanged = TRUE;
			}
		if( stell( stream ) < endPos )
			{
			/* Skip the issuerSerial if there's one present.  We can't 
			   really do much with it in this form without rewriting it into 
			   the standard issuerAndSerialNumber, but in any case we don't 
			   need it because we've already got the certificate ID */
			status = readUniversal( stream );
			}
		return( status );
		}

	/* It's something that we don't recognise, skip it */
	return( readUniversal( stream ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readGeneralInfo( INOUT STREAM *stream, 
							INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	long endPos;
	int length, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Go through the various attributes looking for anything that we can
	   use */
	readConstructed( stream, NULL, CTAG_PH_GENERALINFO );
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	for( iterationCount = 0; 
		 stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_MED; iterationCount++ )
		{
		status = readGeneralInfoAttribute( stream, protocolInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( status );
	}

/* Read the user ID in the PKI header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readUserID( INOUT STREAM *stream, 
					   INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	BYTE userID[ CRYPT_MAX_HASHSIZE + 8 ];
	int userIDsize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Read the PKI user ID that we'll need to handle the integrity 
	   protection on the message */
	readConstructed( stream, NULL, CTAG_PH_SENDERKID );
	status = readOctetString( stream, userID, &userIDsize, 8, 
							  CRYPT_MAX_HASHSIZE );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( userIDsize >= 8 && userIDsize <= CRYPT_MAX_HASHSIZE );

	/* If there's already been a previous transaction (which means that we 
	   have PKI user information present) and the current transaction 
	   matches what was used in the previous one, we don't have to update 
	   the user information */
	if( protocolInfo->userIDsize == userIDsize && \
		!memcmp( protocolInfo->userID, userID, userIDsize ) )
		{
		DEBUG_PRINT(( "%s: Skipped repeated userID.\n",
					  protocolInfo->isServer ? "SVR" : "CLI" ));
		DEBUG_DUMP_HEX( protocolInfo->isServer ? "SVR" : "CLI", 
						protocolInfo->userID, protocolInfo->userIDsize );
		return( CRYPT_OK );
		}

	/* Record the new or changed the PKI user information and delete the 
	   MAC context associated with the previous user if necessary */
	memcpy( protocolInfo->userID, userID, userIDsize );
	protocolInfo->userIDsize = userIDsize;
	protocolInfo->userIDchanged = TRUE;
	if( protocolInfo->iMacContext != CRYPT_ERROR )
		{
		krnlSendNotifier( protocolInfo->iMacContext,
						  IMESSAGE_DECREFCOUNT );
		protocolInfo->iMacContext = CRYPT_ERROR;
		}
	DEBUG_PRINT(( "%s: Read new userID.\n",
				  protocolInfo->isServer ? "SVR" : "CLI" ));
	DEBUG_DUMP_HEX( protocolInfo->isServer ? "SVR" : "CLI", 
					protocolInfo->userID, protocolInfo->userIDsize );

	return( CRYPT_OK );
	}

/* Read the transaction ID (effectively the nonce) in the PKI header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readTransactionID( INOUT STREAM *stream, 
							  INOUT CMP_PROTOCOL_INFO *protocolInfo,
							  const BOOLEAN isServerInitialMessage )
	{
	BYTE buffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* If this is the first message and we're the server, record the 
	   transaction ID for later */
	if( isServerInitialMessage )
		{
		status = readOctetString( stream, protocolInfo->transID,
								  &protocolInfo->transIDsize,
								  4, CRYPT_MAX_HASHSIZE );
		if( cryptStatusError( status ) )
			return( status );
		DEBUG_PRINT(( "%s: Read initial transID.\n",
					  protocolInfo->isServer ? "SVR" : "CLI" ));
		DEBUG_DUMP_HEX( protocolInfo->isServer ? "SVR" : "CLI", 
						protocolInfo->transID, protocolInfo->transIDsize );
		return( CRYPT_OK );
		}

	/* Make sure that the transaction ID for this message matches the 
	   recorded value (the bad signature error code is the best that we can 
	   provide here) */
	status = readOctetString( stream, buffer, &length, 4, 
							  CRYPT_MAX_HASHSIZE );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length >= 4 && length <= CRYPT_MAX_HASHSIZE );
	DEBUG_PRINT(( "%s: Read transID.\n",
				  protocolInfo->isServer ? "SVR" : "CLI" ));
	DEBUG_DUMP_HEX( protocolInfo->isServer ? "SVR" : "CLI", 
					protocolInfo->transID, protocolInfo->transIDsize );
	if( protocolInfo->transIDsize != length || \
		memcmp( protocolInfo->transID, buffer, length ) )
		return( CRYPT_ERROR_SIGNATURE );

	return( CRYPT_OK );
	}

/* Read the integrity protection algorithm information in the PKI header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readProtectionAlgo( INOUT STREAM *stream, 
							   INOUT CMP_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_ALGO_TYPE cryptAlgo, hashAlgo;
	int hashParam, streamPos, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* Read the wrapper.  If there's a problem we exit immediately since an 
	   error status from the readAlgoIDex() that follows is interpreted to 
	   indicate the presence of the weird Entrust MAC rather than a real 
	   error */
	status = readConstructed( stream, NULL, CTAG_PH_PROTECTIONALGO );
	if( cryptStatusError( status ) )
		return( status );
	streamPos = stell( stream );
	status = readAlgoIDex( stream, &cryptAlgo, &hashAlgo, &hashParam,
						   ALGOID_CLASS_PKCSIG );
	if( cryptStatusOK( status ) )
		{
		/* Make sure that it's a recognised signature algorithm to avoid
		   false positives if the other side sends some bizarre algorithm 
		   ID */
		if( !isSigAlgo( cryptAlgo ) )
			return( CRYPT_ERROR_NOTAVAIL );

		/* It's a recognised signature algorithm, use the CA certificate to 
		   verify it rather than the MAC */
		protocolInfo->useMACreceive = FALSE;
		protocolInfo->hashAlgo = hashAlgo;
		protocolInfo->hashParam = hashParam;

		return( CRYPT_OK );
		}
	ENSURES( cryptStatusError( status ) );

	/* It's nothing normal, it must be the Entrust MAC algorithm information, 
	   remember where it starts so that we can process it later */
	sClearError( stream );
	protocolInfo->macInfoPos = streamPos;
	status = readUniversal( stream );
	protocolInfo->useMACreceive = TRUE;

	return( status );
	}

/* Update the session's user ID and certificate ID information from the 
   newly-read protocol information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int updateUserID( INOUT SESSION_INFO *sessionInfoPtr,
				  INOUT CMP_PROTOCOL_INFO *protocolInfo,
				  const BOOLEAN isServerInitialMessage,
				  const BOOLEAN useMAC )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	/* We've got a new PKI user ID, if it looks like a cryptlib encoded ID 
	   save it in encoded form, otherwise save it as is.  Again, CMP's
	   totally ambiguous protocol fields complicate things for us because 
	   although in theory we could reject any message containing a 
	   non-cryptlib user ID on the basis that it couldn't have been assigned 
	   to the user by a cryptlib server, the fact that an arbitrary client 
	   could be sending us who knows what sort of data in the user ID field, 
	   expecting the key to be identified through other means, means that we 
	   can't perform this simple check.  We can at least reject a 
	   non-cryptlib ID for the ir, which must be MAC'd */
	if( isServer( sessionInfoPtr ) && protocolInfo->userIDsize == 9 )
		{
		BYTE encodedUserID[ CRYPT_MAX_TEXTSIZE + 8 ];
		int encodedUserIDLength;

		status = encodePKIUserValue( encodedUserID, CRYPT_MAX_TEXTSIZE,
									 &encodedUserIDLength, 
									 protocolInfo->userID, 
									 protocolInfo->userIDsize, 3 );
		if( cryptStatusError( status ) )
			return( status );
		status = updateSessionInfo( &sessionInfoPtr->attributeList,
									CRYPT_SESSINFO_USERNAME, encodedUserID,
									encodedUserIDLength, CRYPT_MAX_TEXTSIZE,
									ATTR_FLAG_ENCODEDVALUE );
		}
	else
		{
		/* If we're processing an ir then that at least must have a valid 
		   cryptlib user ID */
		if( isServerInitialMessage && useMAC )
			{
			retExt( CRYPT_ERROR_WRONGKEY,
					( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO,
					  "User ID provided by client isn't a cryptlib user "
					  "ID" ) );
			}

		/* It's not a valid cryptlib PKI user ID, save it anyway since
		   it'll be used for diagnostic/error-reporting purposes */
		status = updateSessionInfo( &sessionInfoPtr->attributeList,
									CRYPT_SESSINFO_USERNAME,
									protocolInfo->userID,
									protocolInfo->userIDsize,
									CRYPT_MAX_TEXTSIZE, ATTR_FLAG_NONE );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If this is the first message to the server and we're using MAC-based
	   authentication, set up the server's MAC context based on the 
	   information supplied by the client */
	if( isServerInitialMessage && useMAC )
		return( initServerAuthentMAC( sessionInfoPtr, protocolInfo ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int updateCertID( INOUT SESSION_INFO *sessionInfoPtr,
				  INOUT CMP_PROTOCOL_INFO *protocolInfo,
				  const BOOLEAN isServerInitialMessage )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	status = addSessionInfoS( &sessionInfoPtr->attributeList,
							  CRYPT_SESSINFO_SERVER_FINGERPRINT,
							  protocolInfo->certID,
							  protocolInfo->certIDsize );
	if( cryptStatusError( status ) )
		return( status );

	/* If this is the first message to the server, set up the server's 
	   public-key context for the client's key based on the information
	   supplied by the client */
	if( isServerInitialMessage )
		return( initServerAuthentSign( sessionInfoPtr, protocolInfo ) );

	return( CRYPT_OK );
	}

/* In another piece of brilliant design, CMP provides the information 
   required to set up MAC processing in reverse order, so we don't know what 
   to do with any MAC information that may be present in the header until 
   we've read the start of the message body.  To handle this we have to 
   record the position of the MAC information in the header and then go back 
   and process it once we've read the necessary additional data from the 
   message body, which is handled by the following function */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int updateMacInfo( INOUT SESSION_INFO *sessionInfoPtr,
						  INOUT CMP_PROTOCOL_INFO *protocolInfo,
						  INOUT STREAM *stream,
						  const BOOLEAN isRevocation )
	{
	const ATTRIBUTE_LIST *passwordPtr = \
					findSessionInfo( sessionInfoPtr->attributeList,
									 CRYPT_SESSINFO_PASSWORD );
	BYTE macKey[ 64 + 8 ];
	BOOLEAN decodedMacKey = FALSE;
	const void *macKeyPtr;
	const int streamPos = stell( stream );
	int macKeyLength, status;

	REQUIRES( passwordPtr != NULL );

	sseek( stream, protocolInfo->macInfoPos );
	if( isRevocation && protocolInfo->altMacKeySize > 0 )
		{
		/* If it's a revocation and we're using a distinct revocation
		   password (which we've already decoded into a MAC key), use
		   that */
		macKeyPtr = protocolInfo->altMacKey;
		macKeyLength = protocolInfo->altMacKeySize;
		}
	else
		{
		/* It's a standard issue (or we're using the same password/key
		   for the issue and revocation), use that */
		if( passwordPtr->flags & ATTR_FLAG_ENCODEDVALUE )
			{
			/* It's an encoded value, get the decoded form */
			macKeyPtr = macKey;
			status = decodePKIUserValue( macKey, 64, &macKeyLength, 
										 passwordPtr->value, 
										 passwordPtr->valueLength );
			ENSURES( cryptStatusOK( status ) );
			decodedMacKey = TRUE;
			}
		else
			{
			macKeyPtr = passwordPtr->value;
			macKeyLength = passwordPtr->valueLength;
			}
		}
	status = readMacInfo( stream, protocolInfo, macKeyPtr,
						  macKeyLength, SESSION_ERRINFO );
	if( decodedMacKey )
		zeroise( macKey, 64 );
	if( cryptStatusError( status ) )
		return( status );
	sseek( stream, streamPos );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Read a PKI Header							*
*																			*
****************************************************************************/

/* Read a PKI header and make sure that it matches the header that we sent
   (for EE or non-initial CA/RA messages) or set up the EE information in
   response to an initial message (for an initial CA/RA message).  We ignore
   all of the redundant fields in the header that don't directly affect the
   protocol, based on the results of CMP interop testing this appears to be
   standard practice among implementers.  This also helps get around 
   problems with implementations that get the fields wrong, since most of 
   the fields aren't useful it doesn't affect the processing while making 
   the code more tolerant of implementation errors:

	header				SEQUENCE {
		version			INTEGER (2),
		senderDN	[4]	EXPLICIT DirectoryName,		-- Copied if non-clib
		dummy		[4]	EXPLICIT DirectoryName,		-- Ignored
		dummy		[0] EXPLICIT GeneralisedTime OPT,-- Ignored
		protAlgo	[1]	EXPLICIT AlgorithmIdentifier,
		protKeyID	[2] EXPLICIT OCTET STRING,		-- Copied if changed
		dummy		[3] EXPLICIT OCTET STRING OPT,	-- Ignored
		transID		[4] EXPLICIT OCTET STRING,
		nonce		[5] EXPLICIT OCTET STRING OPT,	-- Copied if non-clib
		dummy		[6] EXPLICIT OCTET STRING OPT,	-- Ignored
		dummy		[7] SEQUENCE OF UTF8String OPT,	-- Ignored
		generalInfo	[8] EXPLICIT SEQUENCE OF Info OPT -- cryptlib-specific info
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readPkiHeader( INOUT STREAM *stream, 
						  INOUT CMP_PROTOCOL_INFO *protocolInfo,
						  INOUT ERROR_INFO *errorInfo,
						  const BOOLEAN isServerInitialMessage )
	{
	int length, endPos, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	/* Clear per-message state information */
	protocolInfo->userIDchanged = protocolInfo->certIDchanged = \
		protocolInfo->useMACreceive = FALSE;
	protocolInfo->macInfoPos = CRYPT_ERROR;
	protocolInfo->senderDNPtr = NULL;
	protocolInfo->senderDNlength = 0;
	protocolInfo->headerRead = FALSE;

	/* Read the wrapper and skip the static information, which matches what 
	   we sent and is protected by the MAC so there's little point in 
	   looking at it */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	readShortInteger( stream, NULL );		/* Version */
	if( !protocolInfo->isCryptlib )
		{
		/* The ID of the key used for integrity protection (or in general
		   the identity of the sender) can be specified either as the sender
		   DN or the senderKID or both, or in some cases even indirectly via
		   the transaction ID.  With no real guidance as to which one to 
		   use, implementors are using any of these options to identify the 
		   key.  Since we need to check that the integrity-protection key 
		   that we're using is correct so that we can report a more 
		   appropriate error than bad signature or bad data, we need to 
		   remember the sender DN for later in case this is the only form of 
		   key identification provided.  Unfortunately since the sender DN 
		   can't uniquely identify a certificate, if this is the only 
		   identifier that we're given then the caller can still get a bad 
		   signature error, yet another one of CMPs many wonderful features */
		status = readConstructed( stream, &protocolInfo->senderDNlength, 4 );
		if( cryptStatusOK( status ) && protocolInfo->senderDNlength > 0 )
			{
			status = sMemGetDataBlock( stream, &protocolInfo->senderDNPtr, 
									   protocolInfo->senderDNlength );
			if( cryptStatusOK( status ) )
				status = readUniversal( stream );
			}
		}
	else
		{
		/* cryptlib includes a proper certID so the whole signer
		   identification mess is avoided and we can ignore the sender DN */
		status = readUniversal( stream );	/* Sender DN */
		}
	if( cryptStatusOK( status ) )
		status = readUniversal( stream );	/* Recipient DN */
	if( peekTag( stream ) == MAKE_CTAG( CTAG_PH_MESSAGETIME ) )
		status = readUniversal( stream );	/* Message time */
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid DN information in PKI header" ) );
		}
	if( peekTag( stream ) != MAKE_CTAG( CTAG_PH_PROTECTIONALGO ) )
		{
		/* The message was sent without integrity protection, report it as
		   a signature error rather than the generic bad data error that
		   we'd get from the following read */
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, errorInfo, 
				  "Message was sent without integrity protection" ) );
		}
	status = readProtectionAlgo( stream, protocolInfo );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo, 
				  "Invalid integrity protection information in PKI "
				  "header" ) );
		}
	if( peekTag( stream ) == MAKE_CTAG( CTAG_PH_SENDERKID ) )
		{								/* Sender protection keyID */
		status = readUserID( stream, protocolInfo );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, errorInfo, 
					  "Invalid PKI user ID in PKI header" ) );
			}
		}
	else
		{
		/* If we're the server, the client must provide a PKI user ID in the
		   first message unless we got one in an earlier transaction */
		if( isServerInitialMessage && protocolInfo->userIDsize <= 0 )
			{
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Missing PKI user ID in PKI header" ) );
			}
		}
	if( peekTag( stream ) == MAKE_CTAG( CTAG_PH_RECIPKID ) )
		readUniversal( stream );			/* Recipient protection keyID */

	/* Record the transaction ID (which is effectively the nonce) or make 
	   sure that it matches the one that we sent.  There's no real need to 
	   do an explicit duplicate check since a replay attempt will be 
	   rejected as a duplicate by the certificate store and the locking 
	   performed at that level makes it a much better place to catch 
	   duplicates, but we do it anyway because it doesn't cost anything and
	   we can catch at least some problems a bit earlier */
	status = readConstructed( stream, NULL, CTAG_PH_TRANSACTIONID );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Missing transaction ID in PKI header" ) );
		}
	status = readTransactionID( stream, protocolInfo, 
								isServerInitialMessage );
	if( cryptStatusError( status ) )
		{
		protocolInfo->pkiFailInfo = CMPFAILINFO_BADRECIPIENTNONCE;
		retExt( status, 
				( status, errorInfo, 
				  ( status == CRYPT_ERROR_SIGNATURE ) ? \
				  "Returned message transaction ID doesn't match our "
						"transaction ID" : \
				  "Invalid transaction ID in PKI header" ) );
		}

	/* Read the sender nonce, which becomes the new recipient nonce, and skip
	   the recipient nonce if there's one present.  These values may be
	   absent, either because the other side doesn't implement them or
	   because they're not available, for example because it's sending a
	   response to an error that occurred before it could read the nonce from
	   a request.  In any case we don't bother checking the nonce values
	   since the transaction ID serves the same purpose */
	if( stell( stream ) < endPos && \
		peekTag( stream ) == MAKE_CTAG( CTAG_PH_SENDERNONCE ) )
		{
		readConstructed( stream, NULL, CTAG_PH_SENDERNONCE );
		status = readOctetString( stream, protocolInfo->recipNonce,
								  &protocolInfo->recipNonceSize,
								  4, CRYPT_MAX_HASHSIZE );
		if( cryptStatusError( status ) )
			{
			protocolInfo->pkiFailInfo = CMPFAILINFO_BADSENDERNONCE;
			retExt( status,
					( status, errorInfo, 
					  "Invalid sender nonce in PKI header" ) );
			}
		}
	if( stell( stream ) < endPos && \
		peekTag( stream ) == MAKE_CTAG( CTAG_PH_RECIPNONCE ) )
		{
		readConstructed( stream, NULL, CTAG_PH_RECIPNONCE );
		status = readUniversal( stream );
		if( cryptStatusError( status ) )
			{
			protocolInfo->pkiFailInfo = CMPFAILINFO_BADRECIPIENTNONCE;
			retExt( status,
					( status, errorInfo, 
					  "Invalid recipient nonce in PKI header" ) );
			}
		}

	/* Remember that we've successfully read enough of the header 
	   information to generate a response */
	protocolInfo->headerRead = TRUE;

	/* Skip any further junk and process the general information if there is 
	   any */
	if( stell( stream ) < endPos && \
		peekTag( stream ) == MAKE_CTAG( CTAG_PH_FREETEXT ) )
		{
		status = readUniversal( stream );	/* Junk */
		if( cryptStatusError( status ) )
			return( status );
		}
	if( stell( stream ) < endPos && \
		peekTag( stream ) == MAKE_CTAG( CTAG_PH_GENERALINFO ) )
		{
		status = readGeneralInfo( stream, protocolInfo );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, errorInfo, 
					  "Invalid generalInfo information in PKI header" ) );
			}
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Read a PKI Message								*
*																			*
****************************************************************************/

/* Read a PKI message:

	PkiMessage ::= SEQUENCE {
		header			PKIHeader,
		body			CHOICE { [0]... [24]... },
		protection	[0]	BIT STRING
		}

   Note that readPkiDatagram() has already performed an initial valid-ASN.1
   check before we get here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readPkiMessage( INOUT SESSION_INFO *sessionInfoPtr,
					INOUT CMP_PROTOCOL_INFO *protocolInfo,
					IN_ENUM_OPT( CTAG_PB ) CMP_MESSAGE_TYPE messageType )
	{
#ifdef USE_ERRMSGS
	ERROR_INFO *errorInfo = &sessionInfoPtr->errorInfo;
#endif /* USE_ERRMSGS */
	READMESSAGE_FUNCTION readMessageFunction;
	STREAM stream;
	const BOOLEAN isServerInitialMessage = \
					( messageType == CTAG_PB_READ_ANY ) ? TRUE : FALSE;
	void *integrityInfoPtr = DUMMY_INIT_PTR;
	int protPartStart = DUMMY_INIT, protPartSize, bodyStart = DUMMY_INIT;
	int length, integrityInfoLength, tag, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( CMP_PROTOCOL_INFO ) ) );

	REQUIRES( ( messageType >= CTAG_PB_IR && \
				messageType < CTAG_PB_LAST ) || \
			  ( messageType == CTAG_PB_READ_ANY ) );
			  /* CTAG_PB_IR == 0 so this is the same as _NONE */

	DEBUG_PRINT(( "%s: Reading message type %d.\n",
				  protocolInfo->isServer ? "SVR" : "CLI", messageType ));

	/* Strip off the header and PKIStatus wrapper */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer,
				 sessionInfoPtr->receiveBufEnd );
	status = readSequence( &stream, NULL );		/* Outer wrapper */
	if( cryptStatusOK( status ) )
		{
		protPartStart = stell( &stream );
		status = readPkiHeader( &stream, protocolInfo, SESSION_ERRINFO,
								isServerInitialMessage );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	ENSURES( protocolInfo->transIDsize > 0 && \
			 protocolInfo->transIDsize <= CRYPT_MAX_HASHSIZE );

	/* Set up session state information based on the header that we've just 
	   read */
	if( protocolInfo->isCryptlib )
		sessionInfoPtr->flags |= SESSION_ISCRYPTLIB;

	/* In order to fix CMP's inability to properly identify keys via 
	   certificates, we use the certID field in the generalInfo.  If there's
	   no PKI user ID present but no certID either then we can't identify 
	   the key that's needed in order to continue.  This also retroactively 
	   invalidates the headerRead flag, since we don't know which key to use
	   to authenticate our response.

	   In theory we shouldn't ever get into this state because we require 
	   a PKI user ID for the client's initial message and the server will
	   always send a certID for its signing certificate, but due to the
	   confusing combination of values that can affect the protocol state
	   (see the start of writePkiHeader() in cmp_wr.c for an example) we
	   do the following as a safety check to catch potential problems 
	   early.
	   
	   This also leads to a special-case exception, if we're the client then
	   the server may identify its signing key purely through the 
	   dysfunctional sender DN mechanism (see the comment in 
	   readPkiHeader()) so we allow this option as well.  The DN can't 
	   usefully tell us whether it's the correct key or not (see the
	   comment in checkMessageSignature()) but there's not much else that
	   we can do */
	if( protocolInfo->useMACreceive )
		{
		/* There is one special-case situation in which we can have no user 
		   ID present and that's when we're doing a PnP PKI transaction with 
		   an initial PKIBoot that creates the required MAC context followed 
		   by an ir that doesn't need to send any ID information since it's 
		   reusing the MAC context that was created by the PKIBoot */
		if( protocolInfo->userIDsize <= 0 && \
			!( protocolInfo->isCryptlib && \
			   protocolInfo->iMacContext != CRYPT_ERROR ) )
			{
			sMemDisconnect( &stream );
			protocolInfo->headerRead = FALSE;
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Missing PKI user ID for MAC authentication of PKI "
					  "messages" ) );
			}
		}
	else
		{
		/* As with the case for MAC contexts, there's a special-case 
		   situation in which there's no certID present and that's during a 
		   PnP PKI transaction preceded by a PKIBoot that communicates the 
		   CA's certificate, where the PKIBoot creates the required 
		   sig-check context as part of the initialisation process.  In
		   addition we have to allow for DN-only identification from 
		   servers, see the comment above for details */
		if( protocolInfo->certIDsize <= 0 && \
			( !isServer ( sessionInfoPtr ) && 
			  protocolInfo->senderDNlength <= 0 ) && \
			!( protocolInfo->isCryptlib && \
			   sessionInfoPtr->iAuthInContext != CRYPT_ERROR ) )
			{
			sMemDisconnect( &stream );
			protocolInfo->headerRead = FALSE;
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Missing certificate ID for signature authentication "
					  "of PKI messages" ) );
			}
		}

	/* If this is the first message from the client and we've been sent a 
	   new user ID or certificate ID (via the ESSCertID in the header's
	   kitchen-sink field, used to identify the signing certificate when
	   signature-based authentication is used), process the user/
	   authentication information */
	if( protocolInfo->userIDchanged )
		{
		status = updateUserID( sessionInfoPtr, protocolInfo,
							   isServerInitialMessage, 
							   protocolInfo->useMACreceive );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}
	if( protocolInfo->certIDchanged )
		{
		status = updateCertID( sessionInfoPtr, protocolInfo,
							   isServerInitialMessage );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}

	/* Determine the message body type.  An error response can occur at any
	   point in an exchange so we process this immediately.  We don't do an
	   integrity verification for this one since it's not certain what we
	   should report if the check fails (what if the error response is to
	   report that no key is available to authenticate the user, for 
	   example?), and an unauthenticated error message is better than an 
	   authenticated paketewhainau */
	tag = peekTag( &stream );
	if( cryptStatusError( tag ) )
		return( tag );
	tag = EXTRACT_CTAG( tag );
	if( tag == CTAG_PB_ERROR )
		{
		readMessageFunction = getMessageReadFunction( CTAG_PB_ERROR );
		ENSURES( readMessageFunction != NULL );
		readConstructed( &stream, NULL, CTAG_PB_ERROR );
		status = readSequence( &stream, &length );
		if( cryptStatusOK( status ) )
			status = readMessageFunction( &stream, sessionInfoPtr, 
										  protocolInfo, CTAG_PB_ERROR, 
										  length );
		sMemDisconnect( &stream );
		return( status );
		}

	/* If this is an initial message then we don't know what to expect yet 
	   so we set the type to whatever we find, as long as it's a valid 
	   message to send to a CA */
	if( messageType == CTAG_PB_READ_ANY )
		{
		if( tag == CTAG_PB_IR || tag == CTAG_PB_CR || \
			tag == CTAG_PB_P10CR || tag == CTAG_PB_KUR || \
			tag == CTAG_PB_RR || \
			( messageType == CTAG_PB_READ_ANY && tag == CTAG_PB_GENM ) )
			protocolInfo->operation = messageType = tag;
		else
			{
			sMemDisconnect( &stream );
			protocolInfo->pkiFailInfo = CMPFAILINFO_BADREQUEST;
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Client sent invalid initial message type %d", 
					  tag ) );
			}
		}
	else
		{
		/* Make sure that this is what we're after */
		if( tag != messageType )
			{
			sMemDisconnect( &stream );
			protocolInfo->pkiFailInfo = CMPFAILINFO_BADREQUEST;
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid message type, expected %d, got %d", 
					  messageType, tag ) );
			}
		}

	/* If we're using a MAC for authentication, we can finally set up the
	   MAC information using the appropriate password.  We couldn't do this 
	   when we read the header because the order of the information used to 
	   set this up is backwards, so we have to go back and re-process it 
	   now */
	if( protocolInfo->useMACreceive )
		{
		status = updateMacInfo( sessionInfoPtr, protocolInfo, &stream,
								( messageType == CTAG_PB_RR ) ? TRUE : FALSE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}

	/* Remember where the message body starts and skip it (it'll be 
	   processed after we verify its integrity) */
	status = readConstructed( &stream, &length, messageType );
	if( cryptStatusOK ( status ) )
		{
		bodyStart = stell( &stream );
		status = sSkip( &stream, length );
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		protocolInfo->pkiFailInfo = CMPFAILINFO_BADDATAFORMAT;
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid message body start" ) );
		}

	/* Read the start of the message integrity information */
	protPartSize = stell( &stream ) - protPartStart;
	ENSURES( protPartSize > 0 && protPartSize < MAX_INTLENGTH_SHORT );
	status = readConstructed( &stream, &integrityInfoLength,
							  CTAG_PM_PROTECTION );
	if( cryptStatusOK( status ) )
		{
		status = sMemGetDataBlock( &stream, &integrityInfoPtr, 
								   integrityInfoLength );
		}
	if( cryptStatusError( status ) )
		{
		/* If the integrity protection is missing report it as a wrong-
		   integrity-information problem, the closest that we can get to the 
		   real error */
		protocolInfo->pkiFailInfo = CMPFAILINFO_WRONGINTEGRITY;
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, errorInfo, 
				  "Signature/MAC data is missing or truncated" ) );
		}
	if( tag == CTAG_PB_IR && !protocolInfo->useMACreceive )
		{
		/* An ir has to be MAC'd, in theory this doesn't really matter but
		   the spec requires that we only allow a MAC.  If it's not MAC'd it
		   has to be a cr, which is exactly the same only different */
		sMemDisconnect( &stream );
		protocolInfo->pkiFailInfo = CMPFAILINFO_WRONGINTEGRITY;
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, errorInfo, 
				  "Received signed ir, should be MAC'd" ) );
		}
#if 0	/* 10/10/11 No longer necessary since we now allow MAC'd rr's */
	if( tag == CTAG_PB_RR && protocolInfo->useMACreceive )
		{
		/* An rr can't be MAC'd because the trail from the original PKI
		   user to the certificate being revoked can become arbitrarily 
		   blurred, with the certificate being revoked having a different 
		   DN, issuerAndSerialNumber, and public key after various updates,
		   replacements, and reissues.  cryptlib does actually track the
		   resulting directed graph via the certificate store log and allows
		   fetching the original authorising issuer of a certificate using 
		   the KEYMGMT_FLAG_GETISSUER option, however this requires that the
		   client also be running cryptlib, or specifically that it submit
		   a certificate ID in the request, this being the only identifier 
		   that reliably identifies the certificate being revoked.  Since 
		   it's somewhat unsound to assume this, we don't currently handle 
		   MAC'ed rr's, however everything is in place to allow them to be 
		   implemented if they're really needed */
		sMemDisconnect( &stream );
		protocolInfo->pkiFailInfo = CMPFAILINFO_WRONGINTEGRITY;
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, errorInfo, 
				  "Received MAC'd rr, should be signed" ) );
		}
#endif /* 0 */
	ANALYSER_HINT( integrityInfoPtr != NULL );

	/* Verify the message integrity.  We convert any error that we encounter 
	   during this check to a CRYPT_ERROR_SIGNATURE, this is somewhat 
	   overreaching since it could have been something like a formatting 
	   error but overall the problem is in the signature-check so we make 
	   this explicit rather than returning a somewhat vague underflow/
	   overflow/bad-data/whatever */
	if( protocolInfo->useMACreceive )
		{
		status = checkMessageMAC( &stream, protocolInfo, 
						sessionInfoPtr->receiveBuffer + protPartStart,
						protPartSize  );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			retExt( CRYPT_ERROR_SIGNATURE,
					( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
					  "Bad message MAC" ) );
			}
		}
	else
		{
		status = checkMessageSignature( protocolInfo,
						sessionInfoPtr->receiveBuffer + protPartStart,
						protPartSize, integrityInfoPtr, integrityInfoLength, 
						sessionInfoPtr->iAuthInContext );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			if( status == CRYPT_ERROR_WRONGKEY )
				{
				/* Provide a more specific error message for the wrong-key 
				   error */
				retExt( CRYPT_ERROR_WRONGKEY,
						( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						  "Message signature key doesn't match our "
						  "signature check key, signature can't be "
						  "checked" ) );
				}
			retExt( CRYPT_ERROR_SIGNATURE,
					( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
					   "Bad message signature" ) );
			}
		}
	sseek( &stream, bodyStart );

	/* In the usual CMP tradition there's a nonstandard way used to encode
	   one of the message types, which we have to handle specially here */
	if( messageType == CTAG_PB_PKICONF )
		{
		status = readNull( &stream );
		sMemDisconnect( &stream );
		return( status );
		}

	/* Read the message body wrapper */
	status = readSequence( &stream, &length );
	if( cryptStatusError( status ) )
		return( status );

	/* Process the message body */
	readMessageFunction = getMessageReadFunction( messageType );
	if( readMessageFunction == NULL )
		{
		DEBUG_DIAG(( "No message-read function available for message "
					 "type %d", messageType ));
		assert( DEBUG_WARN );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Unexpected message type %d", messageType ) );
		}
	status = readMessageFunction( &stream, sessionInfoPtr, protocolInfo,
								  messageType, length );
	sMemDisconnect( &stream );
	return( status );
	}
#endif /* USE_CMP */
