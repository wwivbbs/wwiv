/****************************************************************************
*																			*
*						 cryptlib PGP Key Write Routines					*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "pgp_rw.h"
  #include "keyset.h"
  #include "pgp_key.h"
#else
  #include "crypt.h"
  #include "enc_dec/pgp_rw.h"
  #include "keyset/keyset.h"
  #include "keyset/pgp_key.h"
#endif /* Compiler-specific includes */

#ifdef USE_PGPKEYS

/* Sizes of various buffers used to hold intermediate values before they're
   encoded into PGP keyring format */

#define KEYDATA_BUFFER_SIZE		( ( CRYPT_MAX_PKCSIZE * 3 ) + 128 )
								/* DSA p + q + g + metadata */
#define SIGDATA_BUFFER_SIZE		( CRYPT_MAX_PKCSIZE + 256 )	
								/* RSA sig + metadata */
#define USERID_BUFFER_SIZE		128

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Hash a key or userID in canonicalised form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int hashKeyData( IN_HANDLE const CRYPT_CONTEXT iHashContext,
						IN_BUFFER( keyDataLength ) const BYTE *keyData,
						IN_LENGTH_SHORT_MIN( 16 ) const int keyDataLength )
	{
	STREAM stream;
	BYTE headerBuffer[ 8 + 8 ];
	int headerLength DUMMY_INIT, status;

	assert( isReadPtr( keyData, keyDataLength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( keyDataLength >= 16 && keyDataLength < MAX_INTLENGTH_SHORT );

	/* Write the dummy header used to hash the key data.  We can't use 
	   pgpWritePacketHeader() for this because we have to create a fixed-
	   format header rather than an optimally-encoded one */
	sMemOpen( &stream, headerBuffer, 8 );
	sputc( &stream, 0x99 );		/* Pubkey CTB, 16-bit length */
	status = writeUint16( &stream, keyDataLength );
	if( cryptStatusOK( status ) )
		headerLength = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	/* Hash the dummy header and the key data payload */
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
							  headerBuffer, headerLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) keyData, keyDataLength );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int hashUserID( IN_HANDLE const CRYPT_CONTEXT iHashContext,
					   IN_BUFFER( userIDlength ) const BYTE *userID,
					   IN_LENGTH_SHORT const int userIDlength )
	{
	STREAM stream;
	BYTE headerBuffer[ 8 + 8 ];
	int headerLength DUMMY_INIT, status;

	assert( isReadPtr( userID, userIDlength ) );

	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( userIDlength > 0 && userIDlength < MAX_INTLENGTH_SHORT );

	/* Write the dummy header used to hash the attributes and userID data.  
	   We can't use pgpWritePacketHeader() for this because we have to 
	   create a fixed-format header rather than an optimally-encoded one */
	sMemOpen( &stream, headerBuffer, 8 );
	sputc( &stream, 0xB4 );		/* UserID CTB, 32-bit length */
	status = writeUint32( &stream, userIDlength );
	if( cryptStatusOK( status ) )
		headerLength = stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	/* Hash the dummy header and the attributes and userID packet payload */
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
							  headerBuffer, headerLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH,
								  ( MESSAGE_CAST ) userID, userIDlength );
	return( status );
	}

/* Build a PGP-style userID from the identifiers associated with a key.  
   Ideally we use the more comprehensive information present in a
   certificate, but if there's no certificate present then we have to fall 
   back to the key label */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) \
static int getUserID( IN_HANDLE const CRYPT_CONTEXT cryptHandle,
					  OUT_BUFFER( userIDmaxLength, \
								  *userIDlength ) void *userID,
					  IN_LENGTH_SHORT_MIN( 16 ) const int userIDmaxLength,
					  OUT_LENGTH_BOUNDED_Z( userIDmaxLength ) \
							int *userIDlength )
	{
	CRYPT_CERTIFICATE iCryptCert;
	MESSAGE_DATA msgData DUMMY_INIT_STRUCT;
	STREAM stream;
	BYTE nameBuffer[ USERID_BUFFER_SIZE + 8 ];
	static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
	static const int altNameValue = CRYPT_CERTINFO_SUBJECTALTNAME;
	int nameLength, value, status;

	assert( isWritePtr( userID, userIDmaxLength ) );
	assert( isWritePtr( userIDlength, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( userIDmaxLength >= 16 && \
			  userIDmaxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( userID, 0, min( 16, userIDmaxLength ) );
	*userIDlength = 0;

	/* Check whether we're dealing with a certificate */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) || \
		( value != CRYPT_CERTTYPE_CERTIFICATE && \
		  value != CRYPT_CERTTYPE_CERTCHAIN ) )
		{
		/* It's a straight context, the best that we can do in terms of a 
		   userID is the label for the context */
		setMessageData( &msgData, userID, userIDmaxLength );
		status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusError( status ) )
			return( status );
		*userIDlength = msgData.length;

		return( CRYPT_OK );
		}

	/* Get the associated certificate object handle and lock it so that we 
	   can mess around with internal cursors */
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETDEPENDENT, 
							  &iCryptCert, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_TRUE,
								  CRYPT_IATTRIBUTE_LOCKED );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Fetch the certificate holder name, usually the common name but 
	   possibly an alternative if there's no CN present */
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &nameValue, 
							  CRYPT_ATTRIBUTE_CURRENT );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, nameBuffer, 
						min( userIDmaxLength, USERID_BUFFER_SIZE ) );
		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_HOLDERNAME );
		}
	if( cryptStatusError( status ) )
		{
		( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( status );
		}
	nameLength = msgData.length;
	ENSURES( nameLength > 0 && nameLength <= userIDmaxLength );

	/* Set the user ID to the holder name */
	memcpy( userID, nameBuffer, nameLength );
	*userIDlength = nameLength;

	/* Check whether the holder name is an email address, or at least of the 
	   form "...@...".  If it is then we're done */
	if( strFindCh( nameBuffer, nameLength, '@' ) > 0 )
		{
		( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( CRYPT_OK );
		}

	/* Fetch the email address if there's one present */
	setMessageData( &msgData, nameBuffer, USERID_BUFFER_SIZE );
	krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &altNameValue, 
					 CRYPT_ATTRIBUTE_CURRENT );
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_RFC822NAME );
	( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	
	/* If there isn't room to append the email address to the name, in the
	   form "name <email>", then we're done */
	if( nameLength + 2 + msgData.length + 1 > userIDmaxLength )
		return( CRYPT_OK );

	/* Construct the userID as name + " <" + email + ">" */
	sMemOpen( &stream, ( BYTE * ) userID + nameLength, 
			  userIDmaxLength - nameLength );
	swrite( &stream, " <", 2 );
	swrite( &stream, nameBuffer, msgData.length );
	status = swrite( &stream, ">", 1 );
	if( cryptStatusOK( status ) )
		*userIDlength += stell( &stream );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Subpacket Signature Routines					*
*																			*
****************************************************************************/

/* Create signed attributes.  In addition to the default information 
   (signing time and signing key ID), PGP keys can have all manner of 
   (mostly) noise attributes added to them, but it's not clear what purpose 
   any of them really serve apart from bloating up the public key data.  The 
   sender will choose its preferred default algorithms (e.g. AES, SHA-256) 
   anyway out of any list that we send so we don't need to specify them, and 
   in some cases the choices are tautologies, e.g. the ECC keys for which 
   only AES is allowed so there's no point in specifying anything.

   The possible packets that we can add are:

	- Preferred symmetric algorithms.
	- Preferred hash algorithms.
	- Preferred compression algorithms.
	- Primary userID flag.
	- Features: MDC.

   None of these really seem to serve any purpose if they're not set, but 
   since it seems to be a required fashion statement to have them present we
   generate the relevant attributes and include them in the signed data*/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int createSignedAttributes( OUT_BUFFER( attributeMaxLength, \
											   *attributeLength ) void *attributes,
								   IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
										const int attributeMaxLength,
								   OUT_LENGTH_BOUNDED_Z( attributeMaxLength ) \
										int *attributeLength )
	{
	STREAM stream;
	BYTE buffer[ 16 + 8 ];
	int bufPos, status;

	assert( isWritePtr( attributes, attributeMaxLength ) );
	assert( isWritePtr( attributeLength, sizeof( int ) ) );

	REQUIRES( attributeMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  attributeMaxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( attributes, 0, min( 16, attributeMaxLength ) );
	*attributeLength = 0;

	sMemOpen( &stream, attributes, attributeMaxLength );

	/* Preferred symmetric algorithms:

		byte	length
		byte	type = PGP_SUBPACKET_PREF_SYM
		byte[]	algorithms.
		
	   We don't advertise obsolete algorithms like IDEA and CAST even if 
	   they're available because we don't want to encourage their use */
	bufPos = 0;
	if( algoAvailable( CRYPT_ALGO_AES ) )
		{
		buffer[ bufPos++ ] = PGP_ALGO_AES_128;
		buffer[ bufPos++ ] = PGP_ALGO_AES_256;
		}
	if( algoAvailable( CRYPT_ALGO_3DES ) )
		buffer[ bufPos++ ] = PGP_ALGO_3DES;
	if( bufPos > 0 )
		{
		sputc( &stream, 1 + bufPos );
		sputc( &stream, PGP_SUBPACKET_PREF_SYM );
		swrite( &stream, buffer, bufPos );
		}

	/* Preferred hash algorithms:

		byte	length
		byte	type = PGP_SUBPACKET_PREF_HASH
		byte[]	algorithms */
	bufPos = 0;
	if( algoAvailable( CRYPT_ALGO_SHAng ) )
		buffer[ bufPos++ ] = PGP_ALGO_SHAng;
	if( algoAvailable( CRYPT_ALGO_SHA2 ) )
		buffer[ bufPos++ ] = PGP_ALGO_SHA2_256;
	if( algoAvailable( CRYPT_ALGO_SHA1 ) )
		buffer[ bufPos++ ] = PGP_ALGO_SHA;
	if( bufPos > 0 )
		{
		sputc( &stream, 1 + bufPos );
		sputc( &stream, PGP_SUBPACKET_PREF_HASH );
		swrite( &stream, buffer, bufPos );
		}

	/* Preferred compression algorithms:

		byte	length
		byte	type = PGP_SUBPACKET_PREF_COPR
		byte[]	algorithms */
#ifdef USE_COMPRESSION
	sputc( &stream, 1 + 2 );
	sputc( &stream, PGP_SUBPACKET_PREF_COPR );
	sputc( &stream, PGP_ALGO_ZLIB );
	sputc( &stream, PGP_ALGO_ZIP );
#else
	sputc( &stream, 1 + 1 );
	sputc( &stream, PGP_SUBPACKET_PREF_COPR );
	sputs( &stream, PGP_ALGO_NONE );
#endif /* USE_COMPRESSION */

	/* Primary userID:
		
		byte	length = 1 + 1
		byte	type = PGP_SUBPACKET_PRIMARY_USERID
		byte	boolean = 1

	   This is somewhat redundant since there's only one userID anyway, but 
	   we include it just in case something relies on it */
	sputc( &stream, 1 + 1 );
	sputc( &stream, PGP_SUBPACKET_PRIMARY_USERID );
	sputc( &stream, 1 );

	/* Features:
		
		byte	length = 1 + 1
		byte	type = PGP_SUBPACKET_FEATURES
		byte	flags = 0x1, MDC supported */
	sputc( &stream, 1 + 1 );
	sputc( &stream, PGP_SUBPACKET_FEATURES );
	status = sputc( &stream, 1 );

	if( cryptStatusOK( status ) )
		*attributeLength = stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/* Create a subpacket signature to bind the subpacket to the main key 
   packet.  This simply signs the userID or key data with the primary key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 6 ) ) \
static int createSubpacketSignature( OUT_BUFFER( sigMaxLength, \
												 *signatureLength ) void *signature,
									 IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
										const int sigMaxLength,
									 OUT_LENGTH_BOUNDED_Z( sigMaxLength ) \
										int *signatureLength,
									 IN_HANDLE const CRYPT_CONTEXT iSigContext,
									 IN_HANDLE const CRYPT_CONTEXT iHashContext,
									 IN_BUFFER( subPacketDataLength ) \
										const void *subPacketData,
									 IN_LENGTH_SHORT const int subPacketDataLength,
									 IN_BUFFER_OPT( sigAttributeLength ) \
										const void *sigAttributes,
									 IN_LENGTH_SHORT_Z const int sigAttributeLength,
									 IN_RANGE( PGP_SIG_NONE + 1, PGP_SIG_LAST - 1 ) \
										const int sigType )
	{
	CRYPT_CONTEXT iLocalHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	SIGPARAMS sigParams;
	int status;

	assert( isWritePtr( signature, sigMaxLength ) );
	assert( isWritePtr( signatureLength, sizeof( int ) ) );
	assert( isReadPtr( subPacketData, subPacketDataLength ) );
	assert( ( sigAttributes == NULL && sigAttributeLength == 0 ) || \
			isReadPtr( sigAttributes, sigAttributeLength ) );

	REQUIRES( sigMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  sigMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iSigContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );
	REQUIRES( subPacketDataLength > 0 && \
			  subPacketDataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( ( sigAttributes == NULL && sigAttributeLength == 0 ) || \
			  ( sigAttributes != NULL && sigAttributeLength > 0 && \
				sigAttributeLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( sigType == PGP_SIG_CERT0 || \
			  sigType == PGP_SIG_SUBKEY );

	/* Clear return values */
	memset( signature, 0, min( 16, sigMaxLength ) );
	*signatureLength = 0;

	/* Clone the primary key hash context so that we can continue hashing
	   the subpacket data */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iLocalHashContext = createInfo.cryptHandle;
	status = krnlSendMessage( iHashContext, IMESSAGE_CLONE, NULL,
							  iLocalHashContext );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Hash the subpacket data */
	if( sigType == PGP_SIG_CERT0 )
		{
		status = hashUserID( iLocalHashContext, subPacketData, 
							 subPacketDataLength );
		}
	else
		{
		status = hashKeyData( iLocalHashContext, subPacketData, 
							  subPacketDataLength );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalHashContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Finally, sign the hashed data */
	initSigParamsPGP( &sigParams, sigType, sigAttributes, 
					  sigAttributeLength );
	status = iCryptCreateSignature( signature, sigMaxLength,
									signatureLength, CRYPT_FORMAT_PGP,
									iSigContext, iLocalHashContext,
									&sigParams );

	/* Clean up */
	krnlSendNotifier( iLocalHashContext, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/****************************************************************************
*																			*
*								PGP Key Export Routines						*
*																			*
****************************************************************************/

/* Export a PGP key.  There are two types of export that we can perform, an
   RSA key, which exports a single key and is self-signed, and an 
   Elgamal/ECDH + (EC)DSA key, which exports both keys and uses the (EC)DSA 
   key to sign the Elgamal/ECDH key.
   
   The OpenPGP key format is a complex mess, for RSA keys we use:

	RSA key
		user ID, RSA binding signature

   For Elgamal/ECDH + (EC)DSA keys we use:

	(EC)DSA key										-- Primary key
		user ID, (EC)DSA binding signature
		Elgamal/ECDH key, (EC)DSA binding signature	-- Secondary key 

   The following code assumes that various checks and the storing of the
   Elgamal/ECDH key data have already been performed by higher-level code,
   and concerns itself purely with the generation of signatures and encoding
   of key data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWritePubkey( INOUT PGP_INFO *pgpInfoPtr,
					IN_HANDLE const CRYPT_HANDLE cryptHandle )
	{
	CRYPT_CONTEXT iKeyHash;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE primaryKeyBuffer[ KEYDATA_BUFFER_SIZE + 8 ];
	BYTE secondaryKeySigBuffer[ SIGDATA_BUFFER_SIZE + 8 ];
	BYTE userIDsigBuffer[ SIGDATA_BUFFER_SIZE + 8 ];
	BYTE userIDbuffer[ USERID_BUFFER_SIZE + 8 ];
	BYTE sigAttributes[ 64 + 8 ];
	void *keyData;
	int primaryKeyDataLen, secondaryKeySigLen = 0;
	int userIDlen, userIDsigLen DUMMY_INIT, sigAttributeLen;
	int keyDataLength, altKeyDataLength = 0, status;

	assert( isWritePtr( pgpInfoPtr, sizeof( PGP_INFO ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );

	/* Get the primary key and userID for the key */
	setMessageData( &msgData, primaryKeyBuffer, KEYDATA_BUFFER_SIZE );
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_KEY_PGP );
	if( cryptStatusError( status ) )
		return( status );
	primaryKeyDataLen = msgData.length;
	status = getUserID( cryptHandle, userIDbuffer, USERID_BUFFER_SIZE, 
						&userIDlen );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the signed attributes for the user ID */
	status = createSignedAttributes( sigAttributes, 64, &sigAttributeLen );
	if( cryptStatusError( status ) )
		return( status );

	/* Hash the primary key data in preparation for creating any subkey
	   signatures */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_SHA1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iKeyHash = createInfo.cryptHandle;
	status = hashKeyData( iKeyHash, primaryKeyBuffer, primaryKeyDataLen );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iKeyHash, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Sign the user ID */
	status = createSubpacketSignature( userIDsigBuffer, SIGDATA_BUFFER_SIZE, 
									   &userIDsigLen, cryptHandle, iKeyHash, 
									   userIDbuffer, userIDlen, sigAttributes, 
									   sigAttributeLen, PGP_SIG_CERT0 );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iKeyHash, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Sign the secondary (encryption) key if there's one present */
	if( pgpInfoPtr->keyData != NULL )
		{
		status = createSubpacketSignature( secondaryKeySigBuffer, SIGDATA_BUFFER_SIZE, 
										   &secondaryKeySigLen, cryptHandle, 
										   iKeyHash, pgpInfoPtr->keyData, 
										   pgpInfoPtr->keyDataLen, NULL, 0, 
										   PGP_SIG_SUBKEY );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iKeyHash, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		altKeyDataLength = 16 + pgpInfoPtr->keyDataLen + secondaryKeySigLen;
		}
	krnlSendNotifier( iKeyHash, IMESSAGE_DECREFCOUNT );

	/* Allocate storage to write the keyring data */
	keyDataLength = 16 + primaryKeyDataLen + 16 + userIDlen + \
					userIDsigLen + altKeyDataLength;
	keyData = clAlloc( "pgpWritePubkey", keyDataLength );
	if( keyData == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Write the key(s), user ID, and signature data into the buffer:

		pubkey
			userID, binding signature
		  [	secondary pubkey, binding signature ] */
	sMemOpen( &stream, keyData, keyDataLength );
	status = pgpWritePacketHeader( &stream, PGP_PACKET_PUBKEY, 
								   primaryKeyDataLen );
	if( cryptStatusOK( status ) )
		{
		swrite( &stream, primaryKeyBuffer, primaryKeyDataLen );
		status = pgpWritePacketHeader( &stream, PGP_PACKET_USERID, 
									   userIDlen );
		}
	if( cryptStatusOK( status ) )
		{
		swrite( &stream, userIDbuffer, userIDlen );
		status = swrite( &stream, userIDsigBuffer, userIDsigLen );
		}
	if( cryptStatusOK( status ) && pgpInfoPtr->keyData > 0 )
		{
		status = pgpWritePacketHeader( &stream, PGP_PACKET_PUBKEY_SUB, 
									   pgpInfoPtr->keyDataLen );
		if( cryptStatusOK( status ) )
			{
			swrite( &stream, pgpInfoPtr->keyData, pgpInfoPtr->keyDataLen );
			status = swrite( &stream, secondaryKeySigBuffer, 
							 secondaryKeySigLen );
			}
		}
	if( cryptStatusOK( status ) )
		keyDataLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Finally, rearrange the buffers containing key data as required */
	if( pgpInfoPtr->keyData != NULL )
		clFree( "pgpWritePublicKey", pgpInfoPtr->keyData );
	pgpInfoPtr->keyData = keyData;
	pgpInfoPtr->keyDataLen = keyDataLength;

	return( CRYPT_OK );
	}
#endif /* USE_PGPKEYS */
