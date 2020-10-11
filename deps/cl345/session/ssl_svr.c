/****************************************************************************
*																			*
*							cryptlib TLS Server								*
*					   Copyright Peter Gutmann 1998-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "ssl.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/ssl.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSL

/* Determine whether the server needs to request client certificates/client
   authentication.  This is normally determined by whether an access-control
   keyset is available, but for the Suite B tests in which any test 
   certificate is regarded as being acceptable it can be overridden with a
   self-test flag */

#ifdef CONFIG_SUITEB_TESTS 
#define clientCertAuthRequired( sessionInfoPtr ) \
		( sessionInfoPtr->cryptKeyset != CRYPT_ERROR || suiteBTestClientCert )
#else
#define clientCertAuthRequired( sessionInfoPtr ) \
		( sessionInfoPtr->cryptKeyset != CRYPT_ERROR || \
		  TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_MANUAL_CERTCHECK ) )
#endif /* CONFIG_SUITEB_TESTS */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Read the client certificate chain and make sure that the certificate 
   being presented is valid for access */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readCheckClientCerts( INOUT SESSION_INFO *sessionInfoPtr, 
								 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
								 INOUT STREAM *stream )
	{
#ifdef CONFIG_SUITEB
	int length;
#endif /* CONFIG_SUITEB */
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Read the client certificate chain */
	status = readSSLCertChain( sessionInfoPtr, handshakeInfo, stream, 
							   &sessionInfoPtr->iKeyexAuthContext, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're doing keyset-based certificate verification, make sure that 
	   the client certificate is present in our certificate store.  Since 
	   we've already got a copy of the certificate, we only do a presence 
	   check rather than actually fetching the certificate.
	   
	   Checking whether the certificate is known to us at this point opens
	   us up to a theoretical account-enumeration attack in which an 
	   attacker who has obtained a copy of every certificate in the world
	   can throw them at us one after the other and then use timing 
	   measurements to see which ones get past this point.  OTOH deferring
	   the "is this certificate known to us" check until after we process
	   the keyex opens us up to a DoS attack since the attacker can force
	   us to perform a DH keyex rather than rejecting the handshake at this
	   point.  We could further flip things around and first read the
	   certificate, then read and cache the keyex data, then try and verify
	   the keyex data using the client signature, and only then actually
	   use it, but this greatly complicates the code and given the practical 
	   nonexistence of client certificates just adds a pile of needless
	   complexity for a mechanism that's virtually never used anyway.

	   Because of this we do a quick-reject check here and don't even go
	   into a keyex unless we recognise the certificate */
#ifndef CONFIG_SUITEB_TESTS 
	if( sessionInfoPtr->cryptKeyset != CRYPT_ERROR )
		{
		MESSAGE_KEYMGMT_INFO getkeyInfo;
		MESSAGE_DATA msgData;
		BYTE certID[ KEYID_SIZE + 8 ];

		setMessageData( &msgData, certID, KEYID_SIZE );
		status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		if( cryptStatusOK( status ) )
			{
			setMessageKeymgmtInfo( &getkeyInfo, CRYPT_IKEYID_CERTID, 
								   certID, KEYID_SIZE, NULL, 0, 
								   KEYMGMT_FLAG_CHECK_ONLY );
			status = krnlSendMessage( sessionInfoPtr->cryptKeyset, 
									  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
									  KEYMGMT_ITEM_PUBLICKEY );
			}
		if( cryptStatusError( status ) )
			{
#ifdef USE_ERRMSGS
			char certIDText[ CRYPT_MAX_TEXTSIZE + 8 ];
#endif /* USE_ERRMSGS */

			formatFingerprint( certIDText, CRYPT_MAX_TEXTSIZE, certID, 
							   KEYID_SIZE );
			retExt( CRYPT_ERROR_PERMISSION,
					( CRYPT_ERROR_PERMISSION, SESSION_ERRINFO, 
					  "Client certificate with ID '%s' is not trusted for "
					  "authentication purposes", certIDText ) );
			}
		}
#endif /* !CONFIG_SUITEB_TESTS */

	/* Make sure that the key is of the appropriate size for the Suite B 
	   security level.  At the 128-bit level both P256 and P384 are allowed, 
	   at the 256-bit level only P384 is allowed */
#ifdef CONFIG_SUITEB
	status = krnlSendMessage( sessionInfoPtr->iKeyexAuthContext, 
							  IMESSAGE_GETATTRIBUTE, &length,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		const int suiteBtype = \
						sessionInfoPtr->protocolFlags & SSL_PFLAG_SUITEB;

		if( suiteBtype == SSL_PFLAG_SUITEB_256 )
			{
			if( length != bitsToBytes( 384 ) )
				{
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
						  "Client Suite B certificate uses %d-bit key at "
						  "256-bit security level, should use 384-bit key", 
						  bytesToBits( length ) ) );
				}
			}
		else
			{
			if( length != bitsToBytes( 256 ) && \
				length != bitsToBytes( 384 ) )
				{
				retExt( CRYPT_ERROR_INVALID,
						( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
						  "Client Suite B certificate uses %d-bit key at "
						  "128-bit security level, should use 256- or "
						  "384-bit key", bytesToBits( length ) ) );
				}
			}
		}
#endif /* CONFIG_SUITEB */

	return( CRYPT_OK );
	}

/* Write the certificate request:

	  [	byte		ID = SSL_HAND_SERVER_CERTREQUEST ]
	  [	uint24		len				-- Written by caller ]
		byte		certTypeLen
		byte[]		certType = { RSA, DSA, ECDSA }
	  [	uint16	sigHashListLen		-- TLS 1.2 ]
	  [		byte	hashAlgoID		-- TLS 1.2 ]
	  [		byte	sigAlgoID		-- TLS 1.2 ]
		uint16		caNameListLen = 0
	  [		uint16	caNameLen		-- Omitted due to zero length ]
	  [		byte[]	caName			-- Omitted due to zero length ]

   This message is a real mess, it originally had a rather muddled 
   certificate-type indicator (which included things like "Ephemeral DH 
   signed with RSA") and an equally ambiguous CA list that many 
   implementations either left empty or filled with the name of every CA 
   that they'd ever heard of, see the special-case handling in 
   processCertRequest() in session/ssl_cli.c for the calisthenics required 
   to deal with this.  
   
   TLS 1.2 added a means of indicating which signature and hash algorithms 
   were acceptable, which is kind of essential because the explosion of hash 
   algorithms in 1.2 means that a server would have to run parallel hashes 
   of every handshake message for every possible hash algorithm until the 
   client sends their certificate-verify message (!!).  In other words 
   although it was planned as a means of indicating the server's 
   capabilities, it actually acts as a mechanism for keeping the client-auth 
   process manageable */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCertRequest( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT STREAM *stream )
	{
	const BOOLEAN rsaAvailable = algoAvailable( CRYPT_ALGO_RSA ) ? \
								 TRUE : FALSE;
	const BOOLEAN dsaAvailable = algoAvailable( CRYPT_ALGO_DSA ) ? \
								 TRUE : FALSE;
	const BOOLEAN ecdsaAvailable = algoAvailable( CRYPT_ALGO_ECDSA ) ? \
								   TRUE : FALSE;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );
	REQUIRES( rsaAvailable == TRUE || ecdsaAvailable == TRUE );

	/* Write the certificate type */
	status = sputc( stream, ( rsaAvailable ? 1 : 0 ) + \
							( dsaAvailable ? 1 : 0 ) + \
							( ecdsaAvailable ? 1 : 0 ) );
	if( rsaAvailable )
		status = sputc( stream, TLS_CERTTYPE_RSA );
	if( dsaAvailable )
		status = sputc( stream, TLS_CERTTYPE_DSA );
	if( ecdsaAvailable )
		status = sputc( stream, TLS_CERTTYPE_ECDSA );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the list of accepted signature and hash algorithms if required.  
	   In theory we could write the full list of algorithms, but thanks to 
	   SSL/TLS' braindamaged way of handling certificate-based 
	   authentication (see the comment above) this would make the 
	   certificate-authentication process unmanageable.  To get around this 
	   we only allow one single algorithm, the SHA-2 default for TLS 1.2+ */
	if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
		{
		writeUint16( stream, ( rsaAvailable ? 2 : 0 ) + \
							 ( dsaAvailable ? 2 : 0 ) + \
							 ( ecdsaAvailable ? 2 : 0 ) );
		if( rsaAvailable )
			{
			sputc( stream, TLS_HASHALGO_SHA2 );
			status = sputc( stream, TLS_SIGALGO_RSA );
			}
		if( dsaAvailable )
			{
			sputc( stream, TLS_HASHALGO_SHA2 );
			status = sputc( stream, TLS_SIGALGO_DSA );
			}
		if( ecdsaAvailable )
			{
			sputc( stream, TLS_HASHALGO_SHA2 );
			status = sputc( stream, TLS_SIGALGO_ECDSA );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the CA name list.  Since all we care about is whether a 
	   certificate is present in the authentication database, we don't try 
	   and write a list of every imaginable CA that the client will most 
	   likely ignore anyway */ 
	return( writeUint16( stream, 0 ) );
	}

/* Calculate an ID for use with the session scoreboard from the session ID 
   and the SNI.  This prevents an attacker from taking advantage of virtual 
   hosting with a shared session cache to redirect a connection from one 
   domain to another, which a purely session ID-based lookup would allow */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int convertSNISessionID( INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 OUT_BUFFER_FIXED( idBufferLength ) BYTE *idBuffer,
						 IN_LENGTH_FIXED( KEYID_SIZE ) const int idBufferLength )
	{
	STREAM sniStream;
	BYTE sniInfo[ UINT16_SIZE + MAX_SESSIONID_SIZE + \
				  UINT16_SIZE + KEYID_SIZE + 8 ];
	int sniInfoLength, status;

	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtrDynamic( idBuffer, idBufferLength ) );

	REQUIRES( idBufferLength == KEYID_SIZE );

	/* Clear return value */
	memset( idBuffer, 0, idBufferLength );

	/* Write the session ID and hashed SNI to a buffer for hashing */
	sMemOpen( &sniStream, sniInfo, UINT16_SIZE + MAX_SESSIONID_SIZE + \
								   UINT16_SIZE + KEYID_SIZE );
	writeUint16( &sniStream, handshakeInfo->sessionIDlength );
	swrite( &sniStream, handshakeInfo->sessionID, 
			handshakeInfo->sessionIDlength );
	writeUint16( &sniStream, KEYID_SIZE );
	status = swrite( &sniStream, handshakeInfo->hashedSNI, KEYID_SIZE );
	ENSURES( !cryptStatusError( status ) );
	sniInfoLength = stell( &sniStream );

	/* Generate the final ID from the combined session ID and SNI */
	hashData( idBuffer, idBufferLength, sniInfo, sniInfoLength );
	sMemClose( &sniStream );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Handle Client/Server Keyex						*
*																			*
****************************************************************************/

/* Process the client key exchange packet:

		byte		ID = SSL_HAND_CLIENT_KEYEXCHANGE
		uint24		len
	   DH:
		uint16		yLen
		byte[]		y
	   DH-PSK:
		uint16		userIDLen
		byte[]		userID
		uint16		yLen
		byte[]		y
	   ECDH:
		uint16		ecPointLen
		byte[]		ecPoint
	   PSK:
		uint16		userIDLen
		byte[]		userID 
	   RSA:
	  [ uint16		encKeyLen		-- TLS 1.x ]
		byte[]		rsaPKCS1( byte[2] { 0x03, 0x0n } || byte[46] random ) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processDHKeyex( INOUT SESSION_INFO *sessionInfoPtr, 
						   INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						   INOUT STREAM *stream, 
						   IN_OPT const ATTRIBUTE_LIST *passwordInfoPtr )
	{
	KEYAGREE_PARAMS keyAgreeParams;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( passwordInfoPtr == NULL || \
			isReadPtr( passwordInfoPtr, sizeof( ATTRIBUTE_LIST ) ) );

	/* Read the DH key agreement parameters */
	memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	status = readInteger16UChecked( stream, keyAgreeParams.publicValue,
									&keyAgreeParams.publicValueLen,
									MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
	if( cryptStatusError( status ) )
		{
		/* Some misconfigured clients may use very short keys, we perform a 
		   special-case check for these and return a more specific message 
		   than the generic bad-data error */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure DH key used in key exchange" ) );
			}

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid DH phase 2 key agreement data" ) );
		}

	/* If we're fuzzing the input then we don't need to go through any of 
	   the following crypto calisthenics.  In addition we can exit now 
	   because the remaining fuzzable code is common with the client and
	   has already been tested there */
	FUZZ_EXIT();

	/* Perform phase 2 of the DH key agreement */
	status = krnlSendMessage( handshakeInfo->dhContext, IMESSAGE_CTX_DECRYPT, 
							  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid DH phase 2 key agreement value" ) );
		}
	if( passwordInfoPtr != NULL )
		{
		status = createSharedPremasterSecret( \
							handshakeInfo->premasterSecret,
							CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE,
							&handshakeInfo->premasterSecretSize,
							passwordInfoPtr->value,
							passwordInfoPtr->valueLength, 
							keyAgreeParams.wrappedKey, 
							keyAgreeParams.wrappedKeyLen,
							TEST_FLAG( passwordInfoPtr->flags,
									   ATTR_FLAG_ENCODEDVALUE ) ? \
								TRUE : FALSE );
		if( cryptStatusError( status ) )
			{
			zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "Couldn't create master secret from shared "
					  "secret/password value" ) );
			}
		}
	else
		{
		/* Remember the premaster secret, the output of the DH operation */
		REQUIRES( rangeCheck( keyAgreeParams.wrappedKeyLen, 1,
							  CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE ) );
		memcpy( handshakeInfo->premasterSecret, keyAgreeParams.wrappedKey,
				keyAgreeParams.wrappedKeyLen );
		handshakeInfo->premasterSecretSize = keyAgreeParams.wrappedKeyLen;
		}
	zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processECDHKeyex( INOUT SESSION_INFO *sessionInfoPtr, 
							 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							 INOUT STREAM *stream )
	{
	KEYAGREE_PARAMS keyAgreeParams;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Read the ECDH key agreement parameters */
	memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	status = readEcdhValue( stream, keyAgreeParams.publicValue,
							CRYPT_MAX_PKCSIZE, 
							&keyAgreeParams.publicValueLen );
	if( cryptStatusError( status ) )
		{
		/* Some misconfigured clients may use very short keys, we perform a 
		   special-case check for these and return a more specific message 
		   than the generic bad-data error */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure ECDH key used in key exchange" ) );
			}

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid ECDH phase 2 key agreement data" ) );
		}

	/* If we're fuzzing the input then we don't need to go through any of 
	   the following crypto calisthenics.  In addition we can exit now 
	   because the remaining fuzzable code is common with the client and
	   has already been tested there */
	FUZZ_EXIT();

	/* Perform phase 2 of the ECDH key agreement */
	status = krnlSendMessage( handshakeInfo->dhContext, IMESSAGE_CTX_DECRYPT, 
							  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Invalid ECDH phase 2 key agreement value" ) );
		}

	/* The output of the ECDH operation is an ECC point, but for some 
	   unknown reason standard TLS only uses the x coordinate and not the 
	   full point.  To work around this we have to rewrite the point as a 
	   standalone x coordinate, which is relatively easy because we're  
	   using the uncompressed point format: 

		+---+---------------+---------------+
		|04	|		qx		|		qy		|
		+---+---------------+---------------+
			|<- fldSize --> |<- fldSize --> | */
	if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_TLS12LTS ) )
		{
		const int xCoordLen = ( keyAgreeParams.wrappedKeyLen - 1 ) / 2;

		REQUIRES( keyAgreeParams.wrappedKeyLen >= MIN_PKCSIZE_ECCPOINT && \
				  keyAgreeParams.wrappedKeyLen <= MAX_PKCSIZE_ECCPOINT && \
				  ( keyAgreeParams.wrappedKeyLen & 1 ) == 1 && \
				  keyAgreeParams.wrappedKey[ 0 ] == 0x04 );
		REQUIRES( boundsCheck( 1, xCoordLen, CRYPT_MAX_PKCSIZE ) );
		memmove( keyAgreeParams.wrappedKey, 
				 keyAgreeParams.wrappedKey + 1, xCoordLen );
		keyAgreeParams.wrappedKeyLen = xCoordLen;
		}

	/* Remember the premaster secret, the output of the ECDH operation */
	REQUIRES( rangeCheck( keyAgreeParams.wrappedKeyLen, 1,
						  CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE ) );
	memcpy( handshakeInfo->premasterSecret, keyAgreeParams.wrappedKey,
			keyAgreeParams.wrappedKeyLen );
	handshakeInfo->premasterSecretSize = keyAgreeParams.wrappedKeyLen;
	zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processPSKKeyex( INOUT SESSION_INFO *sessionInfoPtr, 
							INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							INOUT STREAM *stream )
	{
	const ATTRIBUTE_LIST *attributeListPtr, *attributeListCursor;
	const BOOLEAN isKeyex = \
				isKeyxAlgo( handshakeInfo->keyexAlgo ) ? TRUE : FALSE;
	BYTE userID[ CRYPT_MAX_TEXTSIZE + 8 ];
	int length, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( !isEccAlgo( handshakeInfo->keyexAlgo ) );

	/* Read the client user ID and make sure that it's a valid user.  
	   Handling non-valid users is somewhat problematic, we can either bail 
	   out immediately or invent a fake password for the (non-)user and 
	   continue with that.  The problem with this is that it doesn't really 
	   help hide whether the user is valid or not due to the fact that we're 
	   still vulnerable to a timing attack because it takes considerably 
	   longer to generate the random password than it does to read a fixed 
	   password string from memory, so an attacker can tell from the timing 
	   whether the username is valid or not.  In addition usability research 
	   on real-world users indicates that this actually reduces security 
	   while having little to no tangible benefit.  Because of this we don't 
	   try and fake out the valid/invalid user name indication but just exit 
	   immediately if an invalid name is found */
	length = readUint16( stream );
	if( length < 1 || length > CRYPT_MAX_TEXTSIZE || \
		cryptStatusError( sread( stream, userID, length ) ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid client user ID" ) );
		}
	attributeListPtr = findSessionInfoEx( sessionInfoPtr,
										  CRYPT_SESSINFO_USERNAME, 
										  userID, length );
	if( attributeListPtr == NULL )
		{
		retExt( CRYPT_ERROR_WRONGKEY,
				( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
				  "Unknown user name '%s'", 
				  sanitiseString( userID, CRYPT_MAX_TEXTSIZE, 
								  length ) ) );
		}

	/* Move on to the associated password */
	attributeListPtr = DATAPTR_GET( attributeListPtr->next );
	ENSURES( attributeListPtr != NULL && \
			 attributeListPtr->attributeID == CRYPT_SESSINFO_PASSWORD );

	/* Delete any other username/password pairs that may be present so that
	   the caller knows which set was used to authenticate.  These are 
	   present in the attribute list as:

		username - password - ... - username - password - ... 
	
	   so to delete them we repeatedly look for a username attribute and 
	   then delete that and the following attribute */
	LOOP_LARGE_INITCHECK( attributeListCursor = \
							findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME ), 
						  attributeListCursor != NULL )
		{
		ATTRIBUTE_LIST *userNamePtr = ( ATTRIBUTE_LIST * ) attributeListCursor;
		ATTRIBUTE_LIST *passwordPtr = DATAPTR_GET( userNamePtr->next );

		ENSURES( passwordPtr != NULL && \
				 passwordPtr->attributeID == CRYPT_SESSINFO_PASSWORD );
		REQUIRES( DATAPTR_ISVALID( passwordPtr->next ) );

		/* Try and find the next username/password pair after the current one */
		attributeListCursor = DATAPTR_GET( passwordPtr->next );
		if( attributeListCursor != NULL )
			{
			/* There are more attributes present after the current username/
			   password pair, look for another username/password */
			attributeListCursor = findSessionInfoNext( attributeListCursor, 
													   CRYPT_SESSINFO_USERNAME );
			}

		/* If this is a non-matching username/password pair, delete it */
		if( passwordPtr != attributeListPtr )
			{
			deleteSessionInfo( sessionInfoPtr, userNamePtr );
			deleteSessionInfo( sessionInfoPtr, passwordPtr );
			}
		}
	ENSURES( LOOP_BOUND_OK );

	/* If it's PSK with DH, perform the keyex with the PSK added */
	if( isKeyex )
		{
		return( processDHKeyex( sessionInfoPtr, handshakeInfo, stream, 
								attributeListPtr ) );
		}

	/* If we're fuzzing the input then we don't need to go through any of 
	   the following crypto calisthenics.  In addition we can exit now 
	   because the remaining fuzzable code is common with the client and
	   has already been tested there */
	FUZZ_EXIT();

	/* We're using straight PSK, the premaster secret is derived from the 
	   user password */
	status = createSharedPremasterSecret( \
						handshakeInfo->premasterSecret,
						CRYPT_MAX_PKCSIZE + CRYPT_MAX_TEXTSIZE,
						&handshakeInfo->premasterSecretSize, 
						attributeListPtr->value,
						attributeListPtr->valueLength, NULL, 0,
						TEST_FLAG( attributeListPtr->flags, 
								   ATTR_FLAG_ENCODEDVALUE ) ? \
							TRUE : FALSE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't create master secret from shared secret/password "
				  "value" ) );
		}

	return( CRYPT_OK );
	}

#ifdef USE_RSA_SUITES

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processRSAKeyex( INOUT SESSION_INFO *sessionInfoPtr, 
							INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							INOUT STREAM *stream
#ifdef USE_SSL3
							, IN_LENGTH_SHORT const int packetLength 
#endif /* USE_SSL3 */
							)
	{
	BYTE wrappedKey[ CRYPT_MAX_PKCSIZE + 8 ];
#ifdef USE_SSL3
	int length = packetLength, status = CRYPT_OK;
#else
	int length, status;
#endif /* USE_SSL3 */

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

#ifdef USE_SSL3
	REQUIRES( isShortIntegerRangeNZ( length ) );

	/* Read and unwrap the RSA-wrapped premaster secret */
	if( sessionInfoPtr->version <= SSL_MINOR_VERSION_SSL )
		{
		/* The original Netscape SSL implementation didn't provide a length 
		   for the encrypted key so that the packet length is the same as
		   the key length.  Everyone copied that so it became the de facto 
		   standard way to do it (sic faciunt omnes.  The spec itself is 
		   ambiguous on the topic).  
		   
		   This was fixed in TLS (although the spec is still ambigous) so 
		   the encoding differs slightly between SSL and TLS.  To work 
		   around this we have to duplicate a certain amount of the 
		   code in readInteger16UChecked() here */
		if( isShortPKCKey( length ) )
			status = CRYPT_ERROR_NOSECURE;
		else
			{
			if( length < MIN_PKCSIZE || length > CRYPT_MAX_PKCSIZE || \
				cryptStatusError( sread( stream, wrappedKey, length ) ) )
				status = CRYPT_ERROR_BADDATA;
			}
		}
	else
#endif /* USE_SSL3 */
	status = readInteger16UChecked( stream, wrappedKey, &length, 
									MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
	if( cryptStatusError( status ) )
		{
		/* Some misconfigured clients may use very short keys, we perform a 
		   special-case check for these and return a more specific message 
		   than the generic bad-data */
		if( status == CRYPT_ERROR_NOSECURE )
			{
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Insecure RSA key used in key exchange" ) );
			}

		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid RSA encrypted key data" ) );
		}

	/* If we're fuzzing the input then we don't need to go through any of 
	   the following crypto calisthenics.  In addition we can exit now 
	   because the remaining fuzzable code is common with the client and
	   has already been tested there */
	FUZZ_EXIT();

	/* Decrypt the pre-master secret */
	return( unwrapPremasterSecret( sessionInfoPtr, handshakeInfo, 
								   wrappedKey, length ) );
	}
#endif /* USE_RSA_SUITES */

/* Process the client's keyex.  This demultuplexing function sorts out which 
   type of keyex we're using and passes control to the appropriate keyex-
   specific function to handle it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int processKeyex( INOUT SESSION_INFO *sessionInfoPtr, 
						 INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
						 INOUT STREAM *stream
#ifdef USE_SSL3
						 , IN_LENGTH_SHORT const int length
#endif /* USE_SSL3 */
						 )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* If we're using any form of PSK (either raw PSK or PSK with DH) then 
	   the keyex is handled specially */
	if( handshakeInfo->authAlgo == CRYPT_ALGO_NONE )
		return( processPSKKeyex( sessionInfoPtr, handshakeInfo, stream ) );

	/* If we're using DH/ECDH, perform the necessary keyex operations */
	if( isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		if( isEccAlgo( handshakeInfo->keyexAlgo ) )
			{
			return( processECDHKeyex( sessionInfoPtr, handshakeInfo, 
									  stream ) );
			}
		else
			{
			return( processDHKeyex( sessionInfoPtr, handshakeInfo, 
									stream, NULL ) );
			}
		}

	/* It's a regular RSA keyex */
#ifdef USE_RSA_SUITES
  #ifdef USE_SSL3
	return( processRSAKeyex( sessionInfoPtr, handshakeInfo, stream, length ) );
  #else
	return( processRSAKeyex( sessionInfoPtr, handshakeInfo, stream ) );
  #endif /* USE_SSL3 */
#else
	retIntError();
#endif /* USE_RSA_SUITES */
	}

/* Build the server key exchange packet:

	  [	byte		ID = SSL_HAND_SERVER_KEYEXCHANGE ]
	  [	uint24		len				-- Written by caller ]
	   DH:
		uint16		dh_pLen
		byte[]		dh_p
	  [ uint16		dh_qLen
		byte[]		dh_q			-- TLS LTS ]
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	  [	byte		hashAlgoID		-- TLS 1.2 ]
	  [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature
	   DH-PSK:
		uint16		pskIdentityHintLen = 0
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
		uint16		dh_YsLen
		byte[]		dh_Ys
	   ECDH-PSK: (Not present)
	   ECDH:
		byte		curveType
		uint16		namedCurve
		uint8		ecPointLen		-- NB uint8 not uint16
		byte[]		ecPoint
	  [	byte		hashAlgoID		-- TLS 1.2 ]
	  [	byte		sigAlgoID		-- TLS 1.2 ]
		uint16		signatureLen
		byte[]		signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int createServerKeyex( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SSL_HANDSHAKE_INFO *handshakeInfo,
							  INOUT STREAM *stream )
	{
	const BOOLEAN isPSK = ( handshakeInfo->authAlgo == CRYPT_ALGO_NONE ) ? \
						  TRUE : FALSE;
	KEYAGREE_PARAMS keyAgreeParams;
	void *keyData;
	int keyDataOffset, keyDataLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Perform phase 1 of the DH/ECDH key agreement process */
	memset( &keyAgreeParams, 0, sizeof( KEYAGREE_PARAMS ) );
	status = krnlSendMessage( handshakeInfo->dhContext, IMESSAGE_CTX_ENCRYPT, 
							  &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
	if( cryptStatusError( status ) )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		return( status );
		}

	/* Write an empty PSK identity hint (whatever that's supposed to be) if
	   it's a PSK suite.  Perhaps we should always write "nan-demo 
	   kaimasen" */
	if( isPSK )
		{
		status = writeUint16( stream, 0 );
		if( cryptStatusError( status ) )
			{
			zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
			return( status );
			}
		}

	/* Write the DH/ECDH key parameters and public value */
	keyDataOffset = stell( stream );
	status = exportAttributeToStream( stream, handshakeInfo->dhContext,
								TEST_FLAG( sessionInfoPtr->protocolFlags, 
										   SSL_PFLAG_TLS12LTS ) ? \
								CRYPT_IATTRIBUTE_KEY_SSL_EXT : 
								CRYPT_IATTRIBUTE_KEY_SSL );
	if( cryptStatusOK( status ) )
		{
		if( isEccAlgo( handshakeInfo->keyexAlgo ) )
			{
			sputc( stream, keyAgreeParams.publicValueLen );
			status = swrite( stream, keyAgreeParams.publicValue,
							 keyAgreeParams.publicValueLen );
			}
		else
			{
			status = writeInteger16U( stream, keyAgreeParams.publicValue, 
									  keyAgreeParams.publicValueLen );
			}
		}
	if( cryptStatusError( status ) )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		return( status );
		}
	keyDataLength = stell( stream ) - keyDataOffset;

	/* If we're using a PSK suite then the exchange is authenticated via the 
	   PSK and we're done */
	if( isPSK )
		{
		zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );
		return( CRYPT_OK );
		}

	/* Non-PSK suites authenticate the exchange by signing it */
	status = sMemGetDataBlockAbs( stream, keyDataOffset, &keyData, 
								  keyDataLength );
	if( cryptStatusOK( status ) )
		{
		ANALYSER_HINT( keyData != NULL );

		INJECT_FAULT( BADSIG_DATA, SESSION_BADSIG_DATA_SSL_1 );
		status = createKeyexSignature( sessionInfoPtr, handshakeInfo, stream, 
									   keyData, keyDataLength );
		INJECT_FAULT( BADSIG_DATA, SESSION_BADSIG_DATA_SSL_2 );
		}
	zeroise( &keyAgreeParams, sizeof( KEYAGREE_PARAMS ) );

	return( status );
	}

/****************************************************************************
*																			*
*							Server-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int beginServerHandshake( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	SCOREBOARD_INFO scoreboardInfo DUMMY_INIT_STRUCT;
	MESSAGE_DATA msgData;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int packetOffset, clientHelloLength, serverHelloLength;
	int resumedSessionID = CRYPT_ERROR, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Read and process the client hello */
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, 
							  &clientHelloLength, SSL_MSG_FIRST_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, clientHelloLength );
	status = processHelloSSL( sessionInfoPtr, handshakeInfo, stream, TRUE );
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		{
		BYTE sessionIDbuffer[ KEYID_SIZE + 8 ];
		void *scoreboardInfoPtr = \
				DATAPTR_GET( sessionInfoPtr->sessionSSL->scoreboardInfoPtr );
		const BYTE *sessionIDptr = handshakeInfo->sessionID;
		int sessionIDlength = handshakeInfo->sessionIDlength;

		REQUIRES( scoreboardInfoPtr != NULL );

		if( status != OK_SPECIAL )
			return( status );

		/* If there's an SNI present, generate a pseudo-session ID that 
		   includes it.  This is used for the session cache lookup, but the 
		   original session ID is sent back to the client.  The same 
		   operation is performed when the session ID is added to the cache */
		if( handshakeInfo->hashedSNIpresent )
			{
			status = convertSNISessionID( handshakeInfo, sessionIDbuffer,
										  KEYID_SIZE );
			if( cryptStatusError( status ) )
				return( status );
			sessionIDptr = sessionIDbuffer;
			sessionIDlength = KEYID_SIZE;
			}

		/* The client has sent us a sessionID in an attempt to resume a 
		   previous session, see if it's in the session cache */
		resumedSessionID = \
				lookupScoreboardEntry( scoreboardInfoPtr, 
									   SCOREBOARD_KEY_SESSIONID_SVR, 
									   sessionIDptr, sessionIDlength, 
									   &scoreboardInfo );
#ifdef CONFIG_SUITEB_TESTS 
		resumedSessionID = CRYPT_ERROR;	/* Disable for Suite B tests */
#endif /* CONFIG_SUITEB_TESTS */
		}
	CFI_CHECK_UPDATE( "processHelloSSL" );

	/* If we're fuzzing the input then we can skip all data writes to 
	   minimise the overhead during fuzz testing */
	FUZZ_SKIP();

	/* Handle session resumption.  If it's a new session or the session data 
	   has expired from the cache, generate a new session ID */
	if( cryptStatusError( resumedSessionID ) )
		{
		setMessageData( &msgData, handshakeInfo->sessionID, SESSIONID_SIZE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData, 
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
		handshakeInfo->sessionIDlength = SESSIONID_SIZE;
		}
	else
		{
		/* We're resuming a previous session, if extended TLS facilities were
		   in use then make sure that the resumed session uses the same 
		   facilities.  This check preents downgrade attacks where additional
		   security features are disabled by a MITM */
		if( !TEST_FLAGS( sessionInfoPtr->protocolFlags, 
						 SSL_RESUMEDSESSION_FLAGS, 
						 scoreboardInfo.metaData ) )
			{
			retExt( CRYPT_ERROR_INVALID,
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "Session with options %X was resumed with options "
					  "%X.\n", scoreboardInfo.metaData,
					  GET_FLAGS( sessionInfoPtr->protocolFlags,
								 SSL_RESUMEDSESSION_FLAGS ) ) );
			}

		/* Remember the premaster secret for the resumed session */
		status = attributeCopyParams( handshakeInfo->premasterSecret, 
									  SSL_SECRET_SIZE,
									  &handshakeInfo->premasterSecretSize,
									  scoreboardInfo.data, 
									  scoreboardInfo.dataSize );
		ENSURES( cryptStatusOK( status ) );
		}
	CFI_CHECK_UPDATE( "resumedSession" );

	/* Get the nonce that's used to randomise all crypto operations and set 
	   up the server DH/ECDH context if necessary */
	setMessageData( &msgData, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusOK( status ) && isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		status = initDHcontextSSL( &handshakeInfo->dhContext, NULL, 0,
						( handshakeInfo->authAlgo != CRYPT_ALGO_NONE ) ? \
							sessionInfoPtr->privateKey : CRYPT_UNUSED,
						isEccAlgo( handshakeInfo->keyexAlgo ) ? \
							handshakeInfo->eccCurveID : CRYPT_ECCCURVE_NONE,
						TEST_FLAG( sessionInfoPtr->protocolFlags,
								   SSL_PFLAG_TLS12LTS ) ? TRUE : FALSE );
		}
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "initDHcontextSSL" );

	/* Build the server hello, certificate, optional certificate request, 
	   and done packets:

		byte		ID = SSL_HAND_SERVER_HELLO
		uint24		len
		byte[2]		version = { 0x03, 0x0n }
		byte[32]	nonce
		byte		sessIDlen
		byte[]		sessID
		uint16		suite
		byte		copr = 0
	  [	uint16	extListLen		-- RFC 3546/RFC 4366/RFC 6066
			byte	extType
			uint16	extLen
			byte[]	extData ] 
		...

	   We have to be careful how we handle extensions because the RFC makes 
	   the rather optimistic assumption that implementations can handle the 
	   presence of unexpected data at the end of the hello packet, to avoid 
	   problems with this we avoid sending extensions unless they're in 
	   response to extensions already sent by the client */
	status = openPacketStreamSSL( stream, sessionInfoPtr, CRYPT_USE_DEFAULT, 
								  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	status = continueHSPacketStream( stream, SSL_HAND_SERVER_HELLO, 
									 &packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	sputc( stream, SSL_MAJOR_VERSION );
	sputc( stream, sessionInfoPtr->version );
	swrite( stream, handshakeInfo->serverNonce, SSL_NONCE_SIZE );
	sputc( stream, handshakeInfo->sessionIDlength );
	swrite( stream, handshakeInfo->sessionID, 
			handshakeInfo->sessionIDlength );
	INJECT_FAULT( SESSION_CORRUPT_HANDSHAKE, SESSION_CORRUPT_HANDSHAKE_SSL_1 );
	writeUint16( stream, handshakeInfo->cipherSuite ); 
	status = sputc( stream, 0 );	/* No compression */
	if( cryptStatusOK( status ) && handshakeInfo->hasExtensions )
		status = writeServerExtensions( stream, handshakeInfo );
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	serverHelloLength = stell( stream ) - SSL_HEADER_SIZE;
	CFI_CHECK_UPDATE( "serverHello" );

	/* TLS 1.2 LTS implicitly enables various other crypto options, now that
	   we've got past the initial negotiations, enable those too */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_TLS12LTS ) )
		{
		SET_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_EMS );
		if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_GCM ) )
			{
			SET_FLAG( sessionInfoPtr->protocolFlags, 
					  SSL_PFLAG_ENCTHENMAC );
			}

		/* Hash the hello messages */
		getHashParameters( CRYPT_ALGO_SHA2, 0, &handshakeInfo->helloHashFunction, 
						   &handshakeInfo->helloHashSize );
		handshakeInfo->helloHashFunction( handshakeInfo->helloHashInfo, NULL, 0, 
										  sessionInfoPtr->receiveBuffer, 
										  clientHelloLength, HASH_STATE_START );
		handshakeInfo->helloHashFunction( handshakeInfo->helloHashInfo, NULL, 0,
										  sessionInfoPtr->sendBuffer + SSL_HEADER_SIZE, 
										  serverHelloLength, HASH_STATE_CONTINUE );
		}
	CFI_CHECK_UPDATE( "TLS12LTS" );

	/* If it's a resumed session then the server hello is followed 
	   immediately by the change cipherspec, which is sent by the shared 
	   handshake completion code */
	if( !cryptStatusError( resumedSessionID ) )
		{
		status = completePacketStreamSSL( stream, 0 );
		if( cryptStatusOK( status ) )
			status = hashHSPacketWrite( handshakeInfo, stream, 0 );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		CFI_CHECK_UPDATE( "resumedSession" );

		/* Tell the caller that it's a resumed session, leaving the stream
		   open in order to write the change cipherspec message that follows 
		   the server hello in a resumed session */
		DEBUG_PUTS(( "Resuming session with client based on sessionID =" ));
		DEBUG_DUMP_DATA( handshakeInfo->sessionID, 
						 handshakeInfo->sessionIDlength );

		REQUIRES( CFI_CHECK_SEQUENCE_6( "processHelloSSL", "resumedSession", 
										"initDHcontextSSL", "serverHello", 
										"TLS12LTS", "resumedSession" ) );
		return( OK_SPECIAL );
		}
	CFI_CHECK_UPDATE( "nonResumedSession" );

	/*	...	(optional server supplemental data)
		byte		ID = SSL_HAND_SUPPLEMENTAL_DATA
		uint24		len
		uint16		type
		uint16		len
		byte[]		value
		... */

	/*	...
		(optional server certificate chain)
		... */
	if( handshakeInfo->authAlgo != CRYPT_ALGO_NONE )
		{
		INJECT_FAULT( SESSION_WRONGCERT, SESSION_WRONGCERT_SSL_1 );
		status = writeSSLCertChain( sessionInfoPtr, stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		INJECT_FAULT( SESSION_WRONGCERT, SESSION_WRONGCERT_SSL_2 );
		}
	CFI_CHECK_UPDATE( "writeSSLCertChain" );

	/*	...			(optional server keyex) */
	if( isKeyxAlgo( handshakeInfo->keyexAlgo ) )
		{
		status = continueHSPacketStream( stream, SSL_HAND_SERVER_KEYEXCHANGE, 
										 &packetOffset );
		if( cryptStatusOK( status ) )
			{
			status = createServerKeyex( sessionInfoPtr, handshakeInfo, 
										stream );
			}
		if( cryptStatusOK( status ) )
			status = completeHSPacketStream( stream, packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
	CFI_CHECK_UPDATE( "createServerKeyex" );

	/*	...			(optional request for client certificate authentication)
		byte		ID = SSL_HAND_SERVER_CERTREQUEST
		uint24		len
		byte		certTypeLen
		byte[]		certType = { RSA, DSA, ECDSA }
	  [	uint16	sigHashListLen		-- TLS 1.2 ]
	  [		byte	hashAlgoID		-- TLS 1.2 ]
	  [		byte	sigAlgoID		-- TLS 1.2 ]
		uint16		caNameListLen = 4
			uint16	caNameLen = 2
			byte[]	caName = { 0x30, 0x00 }
		... */
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		status = continueHSPacketStream( stream, SSL_HAND_SERVER_CERTREQUEST, 
										 &packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		status = writeCertRequest( sessionInfoPtr, stream );
		if( cryptStatusOK( status ) )
			status = completeHSPacketStream( stream, packetOffset );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
	CFI_CHECK_UPDATE( "writeCertRequest" );

	/*	...
		byte		ID = SSL_HAND_SERVER_HELLODONE
		uint24		len = 0 */
	status = continueHSPacketStream( stream, SSL_HAND_SERVER_HELLODONE, 
									 &packetOffset );
	if( cryptStatusOK( status ) )
		status = completeHSPacketStream( stream, packetOffset );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "completeHSPacketStream" );

	/* Send the combined server packets to the client.  We perform the 
	   assorted hashing of the packets in between the network ops where 
	   it's effectively free */
	status = sendPacketSSL( sessionInfoPtr, stream, FALSE );
	INJECT_FAULT( SESSION_CORRUPT_HANDSHAKE, SESSION_CORRUPT_HANDSHAKE_SSL_2 );
	if( cryptStatusOK( status ) )
		status = hashHSPacketWrite( handshakeInfo, stream, 0 );
	sMemDisconnect( stream );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "sendPacketSSL" );

	REQUIRES( CFI_CHECK_SEQUENCE_12( "processHelloSSL", "resumedSession", 
									 "initDHcontextSSL", "serverHello", 
									 "TLS12LTS", "resumedSession",
									 "nonResumedSession", "writeSSLCertChain", 
									 "createServerKeyex", "writeCertRequest",
									 "completeHSPacketStream", "sendPacketSSL" ) );
	return( CRYPT_OK );
	}

/* Exchange keys with the client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int exchangeServerKeys( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( sanityCheckSessionSSL( sessionInfoPtr ) );

	/* Read the response from the client and, if we're expecting a client 
	   certificate, make sure that it's present */
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, &length,
							  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( stream, sessionInfoPtr->receiveBuffer, length );
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		/* Read the client certificate chain and make sure that the 
		   certificate being presented is valid for access */
		status = readCheckClientCerts( sessionInfoPtr, handshakeInfo, 
									   stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}

		/* Read the next packet(s) if necessary */
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "readCheckClientCerts" );

	/* Process the client key exchange packet */
	status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
								  SSL_HAND_CLIENT_KEYEXCHANGE, 
								  UINT16_SIZE + 1 );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
#ifdef USE_SSL3
	status = processKeyex( sessionInfoPtr, handshakeInfo, stream, length );
#else
	status = processKeyex( sessionInfoPtr, handshakeInfo, stream );
#endif /* USE_SSL3 */
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( stream );
		return( status );
		}
	CFI_CHECK_UPDATE( "processKeyex" );

	/* Create the session hash if required */
	if( TEST_FLAG( sessionInfoPtr->protocolFlags, SSL_PFLAG_EMS ) || \
		clientCertAuthRequired( sessionInfoPtr ) )
		{
		status = createSessionHash( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "createSessionHash" );

	/* If we're expecting a client certificate, process the client 
	   certificate verify */
	if( clientCertAuthRequired( sessionInfoPtr ) )
		{
		const BOOLEAN isECC = isEccAlgo( handshakeInfo->keyexAlgo ) ? \
							  TRUE : FALSE;

		/* Read the next packet(s) if necessary */
		status = refreshHSStream( sessionInfoPtr, handshakeInfo );
		if( cryptStatusError( status ) )
			return( status );

		/* Process the client certificate verify packet:

			byte		ID = SSL_HAND_CLIENT_CERTVERIFY
			uint24		len
			byte[]		signature */
		status = checkHSPacketHeader( sessionInfoPtr, stream, &length,
									  SSL_HAND_CLIENT_CERTVERIFY, 
									  isECC ? MIN_PKCSIZE_ECCPOINT : \
											  MIN_PKCSIZE );
		if( cryptStatusOK( status ) )
			{
			status = checkCertVerify( sessionInfoPtr, handshakeInfo, stream, 
									  length );
			}
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( stream );
			return( status );
			}
		}
	sMemDisconnect( stream );
	CFI_CHECK_UPDATE( "checkCertVerify" );

	REQUIRES( CFI_CHECK_SEQUENCE_4( "readCheckClientCerts", "processKeyex", 
									"createSessionHash", "checkCertVerify" ) );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSSLserverProcessing( SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	FNPTR_SET( handshakeInfo->beginHandshake, beginServerHandshake );
	FNPTR_SET( handshakeInfo->exchangeKeys, exchangeServerKeys );
	}
#endif /* USE_SSL */
