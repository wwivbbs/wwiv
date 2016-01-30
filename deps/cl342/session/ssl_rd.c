/****************************************************************************
*																			*
*					cryptlib SSL v3/TLS Session Read Routines				*
*					   Copyright Peter Gutmann 1998-2010					*
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

/****************************************************************************
*																			*
*								Legacy SSLv2 Functions						*
*																			*
****************************************************************************/

#ifdef ALLOW_SSLV2_HELLO	/* See warning in ssl.h */

/* Handle a legacy SSLv2 client hello:

	uint16	length code = { 0x80, len }
	byte	type = SSL_HAND_CLIENT_HELLO
	byte[2]	vers = { 0x03, 0x0n } */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int handleSSLv2Header( SESSION_INFO *sessionInfoPtr, 
							  SSL_HANDSHAKE_INFO *handshakeInfo, 
							  const BYTE *bufPtr )
	{
	STREAM stream;
	int length, value, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	REQUIRES( bufPtr[ 0 ] == SSL_MSG_V2HANDSHAKE );

	/* Make sure that the length is in order.  Beyond the header we need at 
	   least the three 16-bit field lengths, one 24-bit cipher suite, and at 
	   least 16 bytes of nonce */
	bufPtr++;			/* Skip SSLv2 length ID, already checked by caller */
	length = *bufPtr++;
	if( length < ID_SIZE + VERSIONINFO_SIZE + \
				 ( UINT16_SIZE * 3 ) + 3 + 16 || \
		length > sessionInfoPtr->receiveBufSize )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid legacy SSLv2 hello packet length %d", length ) );
		}

	/* Due to the different ordering of header fields in SSLv2, the type and 
	   version is regarded as part of the payload that needs to be 
	   hashed, rather than the header as for SSLv3 */
	sMemConnect( &stream, bufPtr, ID_SIZE + VERSIONINFO_SIZE );
	status = hashHSPacketRead( handshakeInfo, &stream );
	ENSURES( cryptStatusOK( status ) );
	value = sgetc( &stream );
	if( value != SSL_HAND_CLIENT_HELLO )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Unexpected legacy SSLv2 packet type %d, should be %d", 
				  value, SSL_HAND_CLIENT_HELLO ) );
		}
	status = processVersionInfo( sessionInfoPtr, &stream, 
								 &handshakeInfo->clientOfferedVersion );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	length -= stell( &stream );
	sMemDisconnect( &stream );

	/* Read the packet payload */
	status = sread( &sessionInfoPtr->stream, sessionInfoPtr->receiveBuffer, 
					length );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	if( status != length )
		{
		/* If we timed out during the handshake phase, treat it as a hard 
		   timeout error */
		retExt( CRYPT_ERROR_TIMEOUT,
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timeout during legacy SSLv2 hello packet read, only got "
				  "%d of %d bytes", status, length ) );
		}
	sessionInfoPtr->receiveBufPos = 0;
	sessionInfoPtr->receiveBufEnd = length;
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
	status = hashHSPacketRead( handshakeInfo, &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	/* SSLv2 puts the version information in the header so we set the SSLv2 
	   flag in the handshake information to ensure that it doesn't get 
	   confused with a normal SSL packet type */
	handshakeInfo->isSSLv2 = TRUE;

	return( length );
	}
#endif /* ALLOW_SSLV2_HELLO */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Get a string description of overall and handshake packet types, used for 
   diagnostic error messages */

CHECK_RETVAL_PTR \
const char *getSSLPacketName( IN_RANGE( 0, 255 ) const int packetType )
	{
	typedef struct {
		const int packetType;
		const char *packetName;
		} PACKET_NAME_INFO;
	static const PACKET_NAME_INFO packetNameInfo[] = {
		{ SSL_MSG_CHANGE_CIPHER_SPEC, "change_cipher_spec" },
		{ SSL_MSG_ALERT, "alert" },
		{ SSL_MSG_HANDSHAKE, "handshake" },
		{ SSL_MSG_APPLICATION_DATA, "application_data" },
		{ CRYPT_ERROR, "<Unknown type>" },
			{ CRYPT_ERROR, "<Unknown type>" }
		};
	int i;

	REQUIRES_EXT( ( packetType >= 0 && packetType <= 0xFF ),
				  "<Internal error>" );

	for( i = 0; packetNameInfo[ i ].packetType != packetType && \
				packetNameInfo[ i ].packetType != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( packetNameInfo, PACKET_NAME_INFO ); 
		 i++ );
	REQUIRES_EXT( ( i < FAILSAFE_ARRAYSIZE( packetNameInfo, \
											PACKET_NAME_INFO ) ),
				  "<Internal error>" );

	return( packetNameInfo[ i ].packetName );
	}

CHECK_RETVAL_PTR \
const char *getSSLHSPacketName( IN_RANGE( 0, 255 ) const int packetType )
	{
	typedef struct {
		const int packetType;
		const char *packetName;
		} PACKET_NAME_INFO;
	static const PACKET_NAME_INFO packetNameInfo[] = {
		{ SSL_HAND_CLIENT_HELLO, "client_hello" },
		{ SSL_HAND_SERVER_HELLO, "server_hello" },
		{ SSL_HAND_CERTIFICATE, "certificate" },
		{ SSL_HAND_SERVER_KEYEXCHANGE, "server_key_exchange" },
		{ SSL_HAND_SERVER_CERTREQUEST, "certificate_request" },
		{ SSL_HAND_SERVER_HELLODONE, "server_hello_done" },
		{ SSL_HAND_CLIENT_CERTVERIFY, "certificate_verify" },
		{ SSL_HAND_CLIENT_KEYEXCHANGE, "client_key_exchange" },
		{ SSL_HAND_FINISHED, "finished" },
		{ SSL_HAND_SUPPLEMENTAL_DATA, "supplemental_data" },
		{ CRYPT_ERROR, "<Unknown type>" },
			{ CRYPT_ERROR, "<Unknown type>" }
		};
	int i;

	REQUIRES_EXT( ( packetType >= 0 && packetType <= 0xFF ),
				  "<Internal error>" );

	for( i = 0; packetNameInfo[ i ].packetType != packetType && \
				packetNameInfo[ i ].packetType != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( packetNameInfo, PACKET_NAME_INFO ); 
		 i++ );
	REQUIRES_EXT( ( i < FAILSAFE_ARRAYSIZE( packetNameInfo, \
											PACKET_NAME_INFO ) ),
				  "<Internal error>" );

	return( packetNameInfo[ i ].packetName );
	}

/* Process version information.  This is always called with sufficient data 
   in the input stream so we don't have to worry about special-casing error
   reporting for stream read errors */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processVersionInfo( INOUT SESSION_INFO *sessionInfoPtr, 
						INOUT STREAM *stream, 
						OUT_OPT_INT_Z int *clientVersion )
	{
	int version;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( clientVersion == NULL || \
			isWritePtr( clientVersion, sizeof( int ) ) );

	/* Clear return value */
	if( clientVersion != NULL )
		*clientVersion = CRYPT_ERROR;

	/* Check the major version number */
	version = sgetc( stream );
	if( version != SSL_MAJOR_VERSION )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid major version number %d, should be 3", version ) );
		}

	/* Check the minor version number.  If we've already got the version
	   established, make sure that it matches the existing one, otherwise
	   determine which version we'll be using */
	version = sgetc( stream );
	if( clientVersion == NULL )
		{
		if( version != sessionInfoPtr->version )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid version number 3.%d, should be 3.%d", 
					  version, sessionInfoPtr->version ) );
			}
		return( CRYPT_OK );
		}
	switch( version )
		{
		case SSL_MINOR_VERSION_SSL:
			/* If the other side can't do TLS, fall back to SSL */
			if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS )
				sessionInfoPtr->version = SSL_MINOR_VERSION_SSL;
			break;

		case SSL_MINOR_VERSION_TLS:
			/* If the other side can't do TLS 1.1, fall back to TLS 1.0 */
			if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS11 )
				sessionInfoPtr->version = SSL_MINOR_VERSION_TLS;
			break;

		case SSL_MINOR_VERSION_TLS11:
			/* If the other side can't do TLS 1.2, fall back to TLS 1.1 */
			if( sessionInfoPtr->version >= SSL_MINOR_VERSION_TLS12 )
				sessionInfoPtr->version = SSL_MINOR_VERSION_TLS11;
			break;

		case SSL_MINOR_VERSION_TLS12:
			/* If the other side can't do post-TLS 1.2, fall back to 
			   TLS 1.2 */
			if( sessionInfoPtr->version > SSL_MINOR_VERSION_TLS12 )
				sessionInfoPtr->version = SSL_MINOR_VERSION_TLS12;
			break;

		default:
			/* If we're the server and the client has offered a vaguely 
			   sensible version, fall back to the highest version that we
			   support */
			if( isServer( sessionInfoPtr ) && \
				version <= SSL_MINOR_VERSION_TLS12 + 2 )
				{
				sessionInfoPtr->version = SSL_MINOR_VERSION_TLS12;
				break;
				}

			/* It's nothing that we can handle */
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid protocol version 3.%d", version ) );
		}

	/* If there's a requirement for a minimum version, make sure that it's
	   been met */
	if( sessionInfoPtr->sessionSSL->minVersion > 0 && \
		version < sessionInfoPtr->sessionSSL->minVersion )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid version number 3.%d, should be at least 3.%d", 
				  version, sessionInfoPtr->sessionSSL->minVersion ) );
		}

	*clientVersion = version;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Check a Packet								*
*																			*
****************************************************************************/

/* Check that the header of an SSL packet is in order:

	byte	type
	byte[2]	vers = { 0x03, 0x0n }
	uint16	length
  [ byte[]	iv	- TLS 1.1+ ]

  This is always called with sufficient data in the input stream that we 
  don't have to worry about special-casing error reporting for stream read
  errors.

  If this is the initial hello packet then we request a dummy version 
  information read since the peer's version isn't known yet this point.  The 
  actual version information is taken from the hello packet data, not from 
  the SSL wrapper */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int checkPacketHeader( INOUT SESSION_INFO *sessionInfoPtr, 
							  INOUT STREAM *stream,
							  OUT_LENGTH_Z int *packetLength, 
							  IN_RANGE( SSL_MSG_FIRST, \
										SSL_MSG_LAST_SPECIAL ) \
									const int packetType, 
							  IN_LENGTH_Z const int minLength, 
							  IN_LENGTH const int maxLength )
	{
	SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;
	const int expectedPacketType = \
					( packetType == SSL_MSG_FIRST_HANDSHAKE ) ? \
					SSL_MSG_HANDSHAKE : packetType;
	int value, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetLength, sizeof( int ) ) );

	REQUIRES( ( packetType >= SSL_MSG_FIRST && \
				packetType <= SSL_MSG_LAST ) || \
			  ( packetType == SSL_MSG_FIRST_HANDSHAKE ) );
	REQUIRES( ( packetType == SSL_MSG_APPLICATION_DATA && \
				minLength == 0 ) || \
			  ( minLength > 0 && minLength < MAX_INTLENGTH ) );
	REQUIRES( maxLength >= minLength && maxLength < MAX_INTLENGTH );

	/* Clear return value */
	*packetLength = 0;

	/* Check the packet type */
	value = sgetc( stream );
	if( value != expectedPacketType )
		{
		/* There is one special case in which a mismatch is allowed and 
		   that's when we're expecting a data packet and we instead get a
		   handshake packet, which may be a rehandshake request from the 
		   server.  Unfortunately we can't tell that at this point because 
		   the packet is encrypted, so all that we can do is set a flag 
		   indicating that when we process the actual payload we need to 
		   check for a re-handshake */
		if( expectedPacketType == SSL_MSG_APPLICATION_DATA && \
			value == SSL_MSG_HANDSHAKE && \
			!isServer( sessionInfoPtr ) )
			{
			/* Tell the body-read code to check for a rehandshake (via a
			   hello_request) in the decrypted packet payload */
			sessionInfoPtr->protocolFlags |= SSL_PFLAG_CHECKREHANDSHAKE;
			}
		else
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Unexpected %s (%d) packet, expected %s (%d)", 
					  getSSLPacketName( value ), value, 
					  getSSLPacketName( expectedPacketType ), 
					  expectedPacketType ) );
			}
		}
	status = processVersionInfo( sessionInfoPtr, stream, 
				( packetType == SSL_MSG_FIRST_HANDSHAKE ) ? &value : NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Check the packet length */
	length = readUint16( stream );
	if( sessionInfoPtr->flags & SESSION_ISSECURE_READ )
		{
		if( length < sslInfo->ivSize + minLength + \
					 sessionInfoPtr->authBlocksize || \
			length > sslInfo->ivSize + MAX_PACKET_SIZE + \
					 sessionInfoPtr->authBlocksize + 256 || \
			length > maxLength )
			status = CRYPT_ERROR_BADDATA;
		}
	else
		{
		if( length < minLength || length > MAX_PACKET_SIZE || \
			length > maxLength )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid packet length %d for %s (%d) packet", 
				  length, getSSLPacketName( packetType ), packetType ) );
		}

	/* Load the TLS 1.1+ explicit IV if necessary */
	if( ( sessionInfoPtr->flags & SESSION_ISSECURE_READ ) && \
		sslInfo->ivSize > 0 )
		{
		int ivLength;

		status = loadExplicitIV( sessionInfoPtr, stream, &ivLength );
		if( cryptStatusError( status ) )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Error loading TLS explicit IV" ) );
			}
		length -= ivLength;
		ENSURES( length >= minLength + sessionInfoPtr->authBlocksize && \
				 length <= maxLength );
		}
	*packetLength = length;

	return( CRYPT_OK );
	}

/* Check that the header of an SSL packet and SSL handshake packet is in 
   order.  This is always called with sufficient data in the input stream so 
   we don't have to worry about special-casing error reporting for stream 
   read errors, however for the handshake packet read we do need to check 
   the length because several of these can be encapsulated within a single 
   SSL packet so we can't check the exact space requirements in advance */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkPacketHeaderSSL( INOUT SESSION_INFO *sessionInfoPtr, 
						  INOUT STREAM *stream, 
						  OUT_LENGTH_Z int *packetLength )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetLength, sizeof( int ) ) );

	return( checkPacketHeader( sessionInfoPtr, stream, packetLength,
							   SSL_MSG_APPLICATION_DATA, 0, 
							   sessionInfoPtr->receiveBufSize ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int checkHSPacketHeader( INOUT SESSION_INFO *sessionInfoPtr, 
						 INOUT STREAM *stream, 
						 OUT_LENGTH_Z int *packetLength, 
						 IN_RANGE( SSL_HAND_FIRST, \
								   SSL_HAND_LAST ) const int packetType, 
						 IN_LENGTH_SHORT_Z const int minSize )
	{
	int type, length;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetLength, sizeof( int ) ) );
	
	REQUIRES( packetType >= SSL_HAND_FIRST && packetType <= SSL_HAND_LAST );
	REQUIRES( minSize >= 0 && minSize < MAX_INTLENGTH_SHORT );
			  /* May be zero for change cipherspec */

	/* Clear return value */
	*packetLength = 0;

	/* Make sure that there's enough data left in the stream to safely read
	   the header from it.  This will be caught anyway by the checks below, 
	   but performing the check here makes the error reporting a bit more 
	   precise */
	if( sMemDataLeft( stream ) < 1 + LENGTH_SIZE )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid handshake packet header" ) );
		}

	/*	byte		ID = type
		uint24		length */
	type = sgetc( stream );
	if( type != packetType )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid handshake packet %s (%d), expected %s (%d)", 
				  getSSLHSPacketName( type ), type, 
				  getSSLHSPacketName( packetType ), packetType ) );
		}
	length = readUint24( stream );
	if( length < minSize || length > MAX_PACKET_SIZE || \
		length > sMemDataLeft( stream ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid length %d for %s (%d) handshake packet", 
				  length, getSSLHSPacketName( type ), type ) );
		}
	*packetLength = length;
	DEBUG_PRINT(( "Read %s (%d) handshake packet, length %ld.\n", 
				  getSSLHSPacketName( type ), type, length ));
	DEBUG_DUMP_DATA( sessionInfoPtr->receiveBuffer + stell( stream ), 
					 length );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Read/Unwrap a Packet						*
*																			*
****************************************************************************/

/* Unwrap an SSL data packet:

			  data
				|------------			  MAC'd
				v======================== Encrypted
	+-----+-----+-----------+-----+-----+
	| hdr |(IV)	|	data	| MAC | pad |
	+-----+-----+-----------+-----+-----+
				|<---- dataMaxLen ----->|
				|<- dLen -->|

			  data
				|
				v================		AuthEnc'd
	+-----+-----+---------------+-----+
	| hdr |(IV)	|	data		| ICV |
	+-----+-----+---------------+-----+
				|<--- dataMaxLen ---->|
				|<--- dLen ---->|

   This decrypts the data, removes the padding if necessary, checks and 
   removes the MAC/ICV, and returns the payload length.  Processing of the 
   header and IV have already been performed during the packet header 
   read */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int unwrapPacketSSL( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT_BUFFER( dataMaxLength, \
								   *dataLength ) void *data, 
					 IN_LENGTH const int dataMaxLength, 
					 OUT_LENGTH_Z int *dataLength,
					 IN_RANGE( SSL_HAND_FIRST, \
							   SSL_HAND_LAST ) const int packetType )
	{
	BYTE dummyDataBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	BOOLEAN badDecrypt = FALSE;
	int length = dataMaxLength, payloadLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) && \
			sessionInfoPtr->flags & SESSION_ISSECURE_READ );
	assert( isWritePtr( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( dataMaxLength >= sessionInfoPtr->authBlocksize && \
			  dataMaxLength <= MAX_PACKET_SIZE + \
							   sessionInfoPtr->authBlocksize + 256 );
	REQUIRES( packetType >= SSL_HAND_FIRST && packetType <= SSL_HAND_LAST );

	/* Clear return value */
	*dataLength = 0;

	/* Make sure that the length is a multiple of the block cipher size */
	if( sessionInfoPtr->cryptBlocksize > 1 && \
		( dataMaxLength % sessionInfoPtr->cryptBlocksize ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid encrypted packet length %d relative to cipher "
				  "block size %d for %s (%d) packet", dataMaxLength, 
				  sessionInfoPtr->cryptBlocksize, 
				  getSSLPacketName( packetType ), packetType ) );
		}

	/* If we're using GCM then the ICV isn't encrypted, and we have to 
	   process the packet metadata that's normally MACed as GCM AAD */
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		SSL_INFO *sslInfo = sessionInfoPtr->sessionSSL;

		/* Shorten the packet by the size of the ICV */
		length -= sessionInfoPtr->authBlocksize;
		if( length < 0 || length > MAX_PACKET_SIZE )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid payload length %d for %s (%d) packet", 
					  length, getSSLPacketName( packetType ), 
					  packetType ) );
			}

		/* Process the packet metadata as GCM AAD */
		status = macDataTLSGCM( sessionInfoPtr->iCryptInContext, 
								sslInfo->readSeqNo, sessionInfoPtr->version, 
								length, packetType );
		if( cryptStatusError( status ) )
			return( status );
		sslInfo->readSeqNo++;
		}

	/* Decrypt the packet in the buffer.  We allow zero-length blocks (once
	   the padding is stripped) because some versions of OpenSSL send these 
	   as a kludge to work around pre-TLS 1.1 chosen-IV attacks */
	status = decryptData( sessionInfoPtr, data, length, &length );
	if( cryptStatusError( status ) )
		{
		if( status != CRYPT_ERROR_BADDATA )
			return( status );

		/* There's been a padding error, don't exit immediately but record 
		   that there was a problem for after we've done the MACing.  
		   Delaying the error reporting until then helps prevent timing 
		   attacks of the kind described by Brice Canvel, Alain Hiltgen,
		   Serge Vaudenay, and Martin Vuagnoux in "Password Interception 
		   in a SSL/TLS Channel", Crypto'03, LNCS No.2729, p.583.  These 
		   are close to impossible in most cases because we delay sending 
		   the close notify over a much longer period than the MAC vs.non-
		   MAC time difference and because it requires repeatedly connecting
		   with a fixed-format secret such as a password at the same 
		   location in the packet (which MS Outlook does however manage to 
		   do), but we take this step anyway just to be safe */
		badDecrypt = TRUE;
		length = min( dataMaxLength, 
					  MAX_PACKET_SIZE + sessionInfoPtr->authBlocksize );
		}
	if( sessionInfoPtr->protocolFlags & SSL_PFLAG_GCM )
		{
		/* If we're using GCM then the ICV check has already been 
		   performed as part of the decryption and we're done */
		if( cryptStatusError( status ) )
			return( status );

		*dataLength = length;
		return( CRYPT_OK );
		}
	payloadLength = length - sessionInfoPtr->authBlocksize;
	if( payloadLength < 0 || payloadLength > MAX_PACKET_SIZE )
		{
		/* This is a bit of an odd situation and can really only occur if 
		   we've been sent a malformed packet for which removing the padding
		   reduces the remaining data size to less than the minimum required
		   to store a MAC.  In order to avoid being used as a timing oracle
		   we create a minimum-length dummy MAC value and use that as the 
		   MAC for a zero-length packet, with the same error suppression as 
		   for a bad decrypt */
		data = dummyDataBuffer;
		payloadLength = 0;
		length = sessionInfoPtr->authBlocksize;
		memset( data, 0, length );
		badDecrypt = TRUE;
		}

	/* MAC the decrypted data.  The badDecrypt flag suppresses the reporting
	   of a MAC error due to an earlier bad decrypt, which has already been
	   reported by decryptData() */
	if( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL )
		status = checkMacSSL( sessionInfoPtr, data, length, payloadLength, 
							  packetType, badDecrypt );
	else
		status = checkMacTLS( sessionInfoPtr, data, length, payloadLength, 
							  packetType, badDecrypt );
	if( badDecrypt )
		{
		/* Report the delayed decrypt error, held to this point to make 
		   timing attacks more difficult */
		return( CRYPT_ERROR_BADDATA );
		}
	if( cryptStatusError( status ) )
		return( status );

	*dataLength = payloadLength;
	return( CRYPT_OK );
	}

/* Read an SSL handshake packet.  Since the data transfer phase has its own 
   read/write code we can perform some special-case handling based on this */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readHSPacketSSL( INOUT SESSION_INFO *sessionInfoPtr,
					 INOUT_OPT SSL_HANDSHAKE_INFO *handshakeInfo, 
					 OUT_LENGTH_Z int *packetLength, 
					 IN_RANGE( SSL_HAND_FIRST, \
							   SSL_MSG_LAST_SPECIAL ) const int packetType )
	{
	STREAM stream;
	BYTE headerBuffer[ SSL_HEADER_SIZE + CRYPT_MAX_IVSIZE + 8 ];
	const int localPacketType = \
					( packetType == SSL_MSG_FIRST_ENCRHANDSHAKE ) ? \
					SSL_MSG_HANDSHAKE : packetType;
	int firstByte, bytesToRead, length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( ( handshakeInfo == NULL ) || \
			isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );
	assert( isWritePtr( packetLength, sizeof( int ) ) );

	REQUIRES( ( packetType >= SSL_MSG_FIRST && \
				packetType <= SSL_MSG_LAST ) || \
			  ( packetType == SSL_MSG_FIRST_HANDSHAKE || \
				packetType == SSL_MSG_FIRST_ENCRHANDSHAKE ) );
	REQUIRES( sessionInfoPtr->receiveBufStartOfs >= SSL_HEADER_SIZE && \
			  sessionInfoPtr->receiveBufStartOfs < \
							SSL_HEADER_SIZE + CRYPT_MAX_IVSIZE );

	/* Clear return value */
	*packetLength = 0;

	/* Read and process the header */
	status = readFixedHeaderAtomic( sessionInfoPtr, headerBuffer,
									sessionInfoPtr->receiveBufStartOfs );
	if( cryptStatusError( status ) )
		{
		/* This is the first packet for which encryption has been turned on.  
		   Some implementations handle crypto failures badly, simply closing 
		   the connection rather than returning an alert message as they're 
		   supposed to.  In particular IIS, due to the separation of 
		   protocol and transport layers, has the HTTP server layer close 
		   the connect before any error-handling at the SSL protocol layer 
		   can take effect.  To deal with this problem, we assume that a
		   closed connection in this situation is due to a crypto problem
		   rather than a networking problem */
		if( status == CRYPT_ERROR_READ && \
			packetType == SSL_MSG_FIRST_ENCRHANDSHAKE )
			{
			retExtErr( CRYPT_ERROR_WRONGKEY,
					   ( CRYPT_ERROR_WRONGKEY, SESSION_ERRINFO, 
						 SESSION_ERRINFO, 
						 "Other side unexpectedly closed the connection, "
						 "probably due to incorrect encryption keys being "
						 "negotiated during the handshake: " ) );
			}
		return( status );
		}
	firstByte = byteToInt( headerBuffer[ 0 ] );

	/* Check for an SSL alert message */
	if( firstByte == SSL_MSG_ALERT )
		{
		return( processAlert( sessionInfoPtr, headerBuffer, 
							  sessionInfoPtr->receiveBufStartOfs ) );
		}

	/* Decode and process the SSL packet header.  If this is the first 
	   packet sent by the other side we check for various special-case 
	   conditions and handle them specially.  Since the first byte is 
	   supposed to be an SSL_MSG_HANDSHAKE (value 22 or 0x16) which in 
	   ASCII would be a SYN we can quickly weed out use of non-encrypted
	   protocols like SMTP, POP3, IMAP, and FTP that are often used with 
	   SSL, and the obsolete SSLv2 protocol */
	if( packetType == SSL_MSG_FIRST_HANDSHAKE && \
		firstByte != SSL_MSG_HANDSHAKE )
		{
		/* Try and detect whether the other side is talking something that 
		   isn't SSL, typically SMTP, POP3, IMAP, or FTP, which are used
		   with SSL but require explicit negotiation to switch to xxx-with-
		   SSL.  In theory we could check for more explicit indications of
		   one of these protocols ("220" for SMTP and FTP, "+OK" for POP3
		   and "OK" for IMAP) but there's a danger of mis-identifying the
		   other protocol and returning a message that's worse than a
		   generic bad-data, so for now we just report the text that was 
		   sent and let the user figure it out */
		if( strIsPrintable( headerBuffer, SSL_HEADER_SIZE ) )
			{
			char textBuffer[ CRYPT_MAX_TEXTSIZE + 8 ];

			memcpy( textBuffer, headerBuffer, SSL_HEADER_SIZE );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "%s sent ASCII text string beginning '%s...', is "
					  "this the correct address/port?",
					  isServer( sessionInfoPtr ) ? "Server" : "Client",
					  sanitiseString( textBuffer, CRYPT_MAX_TEXTSIZE, 
									  SSL_HEADER_SIZE ) ) );
			}

		/* Detect whether the other side is using the obsolete SSLv2 
		   protocol */
		if( firstByte == SSL_MSG_V2HANDSHAKE )
			{
#ifdef ALLOW_SSLV2_HELLO	/* See warning in ssl.h */
			/* It's an SSLv2 handshake, handle it specially */
			status = length = handleSSLv2Header( sessionInfoPtr, handshakeInfo, 
												 headerBuffer );
			if( cryptStatusError( status ) )
				return( status );
			*packetLength = length;

			return( CRYPT_OK );
#else
			retExt( CRYPT_ERROR_NOSECURE,
					( CRYPT_ERROR_NOSECURE, SESSION_ERRINFO, 
					  "Client sent obsolete handshake for the insecure SSLv2 "
					  "protocol" ) );
#endif /* ALLOW_SSLV2_HELLO */
			}
		}
	sMemConnect( &stream, headerBuffer, sessionInfoPtr->receiveBufStartOfs );
	status = checkPacketHeader( sessionInfoPtr, &stream, &bytesToRead, 
						localPacketType, 
						( localPacketType == SSL_MSG_CHANGE_CIPHER_SPEC ) ? \
							1 : MIN_PACKET_SIZE,
						sessionInfoPtr->receiveBufSize ); 
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the payload packet(s) */
	status = length = \
		sread( &sessionInfoPtr->stream, sessionInfoPtr->receiveBuffer, 
			   bytesToRead );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	if( length != bytesToRead )
		{
		/* If we timed out during the handshake phase, treat it as a hard 
		   timeout error */
		retExt( CRYPT_ERROR_TIMEOUT,
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timed out reading packet data for %s (%d) packet, only "
				  "got %d of %d bytes", getSSLPacketName( localPacketType ), 
				  localPacketType, length, bytesToRead ) );
		}
	sessionInfoPtr->receiveBufPos = 0;
	sessionInfoPtr->receiveBufEnd = length;
	if( handshakeInfo != NULL )
		{
		sMemConnect( &stream, sessionInfoPtr->receiveBuffer, length );
		status = hashHSPacketRead( handshakeInfo, &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		}
	*packetLength = length;

	return( CRYPT_OK );
	}

/* Read the next handshake stream packet */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int refreshHSStream( INOUT SESSION_INFO *sessionInfoPtr, 
					 INOUT SSL_HANDSHAKE_INFO *handshakeInfo )
	{
	STREAM *stream = &handshakeInfo->stream;
	int length, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( handshakeInfo, sizeof( SSL_HANDSHAKE_INFO ) ) );

	/* If there's still data present in the stream, there's nothing left
	   to do */
	length = sMemDataLeft( stream );
	if( length > 0 )
		{
		/* We need enough data to contain at least a handshake packet header 
		   in order to continue */
		if( length < 1 + LENGTH_SIZE || length > MAX_INTLENGTH )
			{
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid handshake packet data length %d", length ) );
			}

		return( CRYPT_OK );
		}

	/* Refill the stream */
	sMemDisconnect( stream );
	status = readHSPacketSSL( sessionInfoPtr, handshakeInfo, &length,
							  SSL_MSG_HANDSHAKE );
	if( cryptStatusError( status ) )
		return( status );
	return( sMemConnect( stream, sessionInfoPtr->receiveBuffer, length ) );
	}		

/****************************************************************************
*																			*
*								Receive SSL Alerts							*
*																			*
****************************************************************************/

/* SSL alert information */

typedef struct {
	const int type;				/* SSL alert type */
	const char *message;		/* Description string */
	const int messageLength;
	const int cryptlibError;	/* Equivalent cryptlib error status */
	} ALERT_INFO;

static const ALERT_INFO alertInfo[] = {
	{ SSL_ALERT_CLOSE_NOTIFY, "Close notify", 12, CRYPT_ERROR_COMPLETE },
	{ SSL_ALERT_UNEXPECTED_MESSAGE, "Unexpected message", 18, CRYPT_ERROR_FAILED },
	{ SSL_ALERT_BAD_RECORD_MAC, "Bad record MAC", 14, CRYPT_ERROR_SIGNATURE },
	{ TLS_ALERT_DECRYPTION_FAILED, "Decryption failed", 17, CRYPT_ERROR_WRONGKEY },
	{ TLS_ALERT_RECORD_OVERFLOW, "Record overflow", 15, CRYPT_ERROR_OVERFLOW },
	{ SSL_ALERT_DECOMPRESSION_FAILURE, "Decompression failure", 21, CRYPT_ERROR_FAILED },
	{ SSL_ALERT_HANDSHAKE_FAILURE, "Handshake failure", 17, CRYPT_ERROR_FAILED },
	{ SSL_ALERT_NO_CERTIFICATE, "No certificate", 14, CRYPT_ERROR_PERMISSION },
	{ SSL_ALERT_BAD_CERTIFICATE, "Bad certificate", 15, CRYPT_ERROR_INVALID },
	{ SSL_ALERT_UNSUPPORTED_CERTIFICATE, "Unsupported certificate", 23, CRYPT_ERROR_INVALID },
	{ SSL_ALERT_CERTIFICATE_REVOKED, "Certificate revoked", 19, CRYPT_ERROR_INVALID },
	{ SSL_ALERT_CERTIFICATE_EXPIRED, "Certificate expired", 19, CRYPT_ERROR_INVALID },
	{ SSL_ALERT_CERTIFICATE_UNKNOWN, "Certificate unknown", 19, CRYPT_ERROR_INVALID },
	{ TLS_ALERT_ILLEGAL_PARAMETER, "Illegal parameter", 17, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_UNKNOWN_CA, "Unknown CA", 10, CRYPT_ERROR_INVALID },
	{ TLS_ALERT_ACCESS_DENIED, "Access denied", 13, CRYPT_ERROR_PERMISSION },
	{ TLS_ALERT_DECODE_ERROR, "Decode error", 12, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_DECRYPT_ERROR, "Decrypt error", 13, CRYPT_ERROR_WRONGKEY },
	{ TLS_ALERT_EXPORT_RESTRICTION, "Export restriction", 18, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_PROTOCOL_VERSION, "Protocol version", 16, CRYPT_ERROR_NOTAVAIL },
	{ TLS_ALERT_INSUFFICIENT_SECURITY, "Insufficient security", 21, CRYPT_ERROR_NOSECURE },
	{ TLS_ALERT_INTERNAL_ERROR, "Internal error", 14, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_USER_CANCELLED, "User cancelled", 14, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_NO_RENEGOTIATION, "No renegotiation", 16, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_UNSUPPORTED_EXTENSION, "Unsupported extension", 21, CRYPT_ERROR_NOTAVAIL },
	{ TLS_ALERT_CERTIFICATE_UNOBTAINABLE, "Certificate unobtainable", 24, CRYPT_ERROR_NOTFOUND },
	{ TLS_ALERT_UNRECOGNIZED_NAME, "Unrecognized name", 17, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_BAD_CERTIFICATE_STATUS_RESPONSE, "Bad certificate status response", 31, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_BAD_CERTIFICATE_HASH_VALUE, "Bad certificate hash value", 26, CRYPT_ERROR_FAILED },
	{ TLS_ALERT_UNKNOWN_PSK_IDENTITY, "Unknown PSK identity", 20, CRYPT_ERROR_NOTFOUND },
	{ CRYPT_ERROR, NULL }, { CRYPT_ERROR, NULL }
	};

/* Process an alert packet.  IIS often just drops the connection rather than 
   sending an alert when it encounters a problem, although we try and work
   around some of the known problems, e.g. by sending a canary in the client
   hello to force IIS to at least send back something rather than just 
   dropping the connection, see ssl_hs.c.  In addition when communicating 
   with IIS the only error indication that we sometimes get will be a 
   "Connection closed by remote host" rather than an SSL-level error message,
   see the comment in readHSPacketSSL() for the reason for this.  Also, when 
   it encounters an unknown certificate MSIE will complete the handshake and 
   then close the connection (via a proper close alert in this case rather 
   than just closing the connection), wait while the user clicks OK several 
   times, and then restart the connection via an SSL resume.  Netscape in 
   contrast just hopes that the session won't time out while waiting for the 
   user to click OK.  As a result cryptlib sees a closed connection and 
   aborts the session setup process when talking to MSIE, requiring a second 
   call to the session setup to continue with the resumed session */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int processAlert( INOUT SESSION_INFO *sessionInfoPtr, 
				  IN_BUFFER( headerLength ) const void *header, 
				  IN_LENGTH const int headerLength )
	{
	STREAM stream;
	BYTE buffer[ 256 + 8 ];
	int length, type, i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( header, headerLength ) );

	REQUIRES( headerLength > 0 && headerLength < MAX_INTLENGTH );

	/* Process the alert packet header */
	sMemConnect( &stream, header, headerLength );
	status = checkPacketHeader( sessionInfoPtr, &stream, &length, 
								SSL_MSG_ALERT, ALERTINFO_SIZE,
								sessionInfoPtr->receiveBufSize );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	if( sessionInfoPtr->flags & SESSION_ISSECURE_READ )
		{
		if( length < ALERTINFO_SIZE || length > 256 )
			status = CRYPT_ERROR_BADDATA;
		}
	else
		{
		if( length != ALERTINFO_SIZE )
			status = CRYPT_ERROR_BADDATA;
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid alert message length %d", length ) );
		}

	/* Read and process the alert packet */
	status = sread( &sessionInfoPtr->stream, buffer, length );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	if( status != length )
		{
		/* If we timed out before we could get all of the alert data, bail
		   out without trying to perform any further processing.  We're 
		   about to shut down the session anyway so there's no point in 
		   potentially stalling for ages trying to find a lost byte */
		sendCloseAlert( sessionInfoPtr, TRUE );
		sessionInfoPtr->flags |= SESSION_SENDCLOSED;
		retExt( CRYPT_ERROR_TIMEOUT, 
				( CRYPT_ERROR_TIMEOUT, SESSION_ERRINFO, 
				  "Timed out reading alert message, only got %d of %d "
				  "bytes", status, length ) );
		}
	if( ( sessionInfoPtr->flags & SESSION_ISSECURE_READ ) && \
		( length > ALERTINFO_SIZE || \
		  isStreamCipher( sessionInfoPtr->cryptAlgo ) ) )
		{
		/* We only try and decrypt if the alert information is big enough to 
		   be encrypted, i.e. it contains the fixed-size data + padding.  
		   This situation can occur if there's an error moving from the non-
		   secure to the secure state.  However, if it's a stream cipher the 
		   ciphertext and plaintext are the same size so we always have to 
		   try the decryption.

		   Before calling unwrapPacketSSL() we set the receive-buffer end-
		   position indicator, which isn't otherwise explicitly used but is
		   required for a sanity check in the unwrap code */
		sessionInfoPtr->receiveBufEnd = length;
		status = unwrapPacketSSL( sessionInfoPtr, buffer, length, &length, 
								  SSL_MSG_ALERT );
		if( cryptStatusError( status ) )
			{
			sendCloseAlert( sessionInfoPtr, TRUE );
			sessionInfoPtr->flags |= SESSION_SENDCLOSED;
			return( status );
			}
		}

	/* Tell the other side that we're going away */
	sendCloseAlert( sessionInfoPtr, TRUE );
	sessionInfoPtr->flags |= SESSION_SENDCLOSED;

	/* Process the alert information.  In theory we should also make the 
	   session non-resumable if the other side goes away without sending a 
	   close alert, but this leads to too many problems with non-resumable 
	   sessions if we do so.  For example many protocols do their own 
	   end-of-data indication (e.g. "Connection: close" in HTTP and BYE in 
	   SMTP) and so don't bother with a close alert.  In other cases 
	   implementations just drop the connection without sending a close 
	   alert, carried over from many early Unix protocols that used a 
	   connection close to signify end-of-data, which has caused problems 
	   ever since for newer protocols that want to keep the connection open.  
	   This behaviour is nearly universal for some protocols tunneled over 
	   SSL, for example vsftpd by default disables close-alert handling
	   because the author was unable to find FTP client anywhere that uses
	   it (see the manpage entry for "strict_ssl_write_shutdown").  Other 
	   implementations still send their alert but then immediately close the 
	   connection.  Because of this haphazard approach to closing 
	   connections many implementations allow a session to be resumed even 
	   if no close alert is sent.  In order to be compatible with this 
	   behaviour, we do the same (thus perpetuating the problem).  If 
	   necessary this can be fixed by calling deleteSessionCacheEntry() if 
	   the connection is closed without a close alert having been sent */
	if( buffer[ 0 ] != SSL_ALERTLEVEL_WARNING && \
		buffer[ 0 ] != SSL_ALERTLEVEL_FATAL )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid alert message level %d", buffer[ 0 ] ) );
		}
	type = buffer[ 1 ];
	for( i = 0; alertInfo[ i ].type != CRYPT_ERROR && \
				alertInfo[ i ].type != type && \
				i < FAILSAFE_ARRAYSIZE( alertInfo, ALERT_INFO ); i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( alertInfo, ALERT_INFO ) );
	if( alertInfo[ i ].type == CRYPT_ERROR )
		{
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Unknown alert message type %d at alert level %d", 
				  type, buffer[ 0 ] ) );
		}
	retExtStr( alertInfo[ i ].cryptlibError,
			   ( alertInfo[ i ].cryptlibError, SESSION_ERRINFO, 
				 alertInfo[ i ].message, alertInfo[ i ].messageLength,
				 ( sessionInfoPtr->version == SSL_MINOR_VERSION_SSL ) ? \
					"Received SSL alert message: " : \
					"Received TLS alert message: " ) );
	}
#endif /* USE_SSL */
