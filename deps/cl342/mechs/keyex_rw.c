/****************************************************************************
*																			*
*						Key Exchange Read/Write Routines					*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "pgp_rw.h"
  #include "mech.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "enc_dec/pgp_rw.h"
  #include "mechs/mech.h"
#endif /* Compiler-specific includes */

/* Context-specific tags for the KEK record */

enum { CTAG_KK_DA };

/* Context-specific tags for the KeyTrans record */

enum { CTAG_KT_SKI };

/****************************************************************************
*																			*
*					Conventionally-Encrypted Key Routines					*
*																			*
****************************************************************************/

/* The OID for the PKCS #5 v2.0 key derivation function and the parameterised
   PWRI key wrap algorithm */

#define OID_PBKDF2	MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x05\x0C" )
#define OID_PWRIKEK	MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x09" )

/* Read/write a PBKDF2 key derivation record:

	SEQUENCE {
		algorithm					AlgorithmIdentifier (pkcs-5 12),
		params SEQUENCE {
			salt					OCTET STRING,
			iterationCount			INTEGER (1..MAX),
			keyLength				INTEGER OPTIONAL,
			prf						AlgorithmIdentifier DEFAULT hmacWithSHA1
			},
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readKeyDerivationInfo( INOUT STREAM *stream, 
								  OUT QUERY_INFO *queryInfo )
	{
	long endPos, value;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the outer wrapper and key derivation algorithm OID */
	readConstructed( stream, NULL, CTAG_KK_DA );
	status = readFixedOID( stream, OID_PBKDF2, sizeofOID( OID_PBKDF2 ) );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the PBKDF2 parameters, limiting the salt and iteration count to
	   sane values */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	readOctetString( stream, queryInfo->salt, &queryInfo->saltLength, 
					 2, CRYPT_MAX_HASHSIZE );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value < 1 || value > MAX_KEYSETUP_ITERATIONS )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->keySetupIterations = ( int ) value;
	queryInfo->keySetupAlgo = CRYPT_ALGO_HMAC_SHA1;
	if( stell( stream ) < endPos && \
		sPeek( stream ) == BER_INTEGER )
		{
		/* There's an optional key length that may override the default 
		   key size present, read it.  Note that we compare the upper
		   bound to MAX_WORKING_KEYSIZE rather than CRYPT_MAX_KEYSIZE,
		   since this is a key used directly with an encryption algorithm
		   rather than a generic keying value that may be hashed down to 
		   size */
		status = readShortInteger( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		if( value < MIN_KEYSIZE || value > MAX_WORKING_KEYSIZE )
			return( CRYPT_ERROR_BADDATA );
		queryInfo->keySize = ( int ) value;
		}
	if( stell( stream ) < endPos )
		{
		CRYPT_ALGO_TYPE prfAlgo;
	
		/* There's a non-default hash algorithm ID present, read it */
		status = readAlgoID( stream, &prfAlgo, ALGOID_CLASS_HASH );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->keySetupAlgo = prfAlgo;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeKeyDerivationInfo( INOUT STREAM *stream,
								   IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	MESSAGE_DATA msgData;
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];
	int saltLength, keySetupIterations, prfAlgo = DUMMY_INIT;
	int derivationInfoSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Get the key derivation information */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &keySetupIterations,
							  CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &prfAlgo, CRYPT_CTXINFO_KEYING_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, salt, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_KEYING_SALT );
	if( cryptStatusError( status ) )
		return( status );
	saltLength = msgData.length;
	derivationInfoSize = ( int ) sizeofObject( saltLength ) + \
						 sizeofShortInteger( keySetupIterations );
	if( prfAlgo != CRYPT_ALGO_HMAC_SHA1 )
		derivationInfoSize += sizeofAlgoID( prfAlgo );

	/* Write the PBKDF2 information */
	writeConstructed( stream, sizeofOID( OID_PBKDF2 ) +
					  ( int ) sizeofObject( derivationInfoSize ), CTAG_KK_DA );
	writeOID( stream, OID_PBKDF2 );
	writeSequence( stream, derivationInfoSize );
	writeOctetString( stream, salt, saltLength, DEFAULT_TAG );
	status = writeShortInteger( stream, keySetupIterations, DEFAULT_TAG );
	if( prfAlgo != CRYPT_ALGO_HMAC_SHA1 )
		status = writeAlgoID( stream, prfAlgo );
	zeroise( salt, CRYPT_MAX_HASHSIZE );
	return( status );
	}

/* Read/write CMS KEK data.  This is the weird Spyrus key wrap that was 
   slipped into CMS, nothing seems to support this so we don't either */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCmsKek( INOUT STREAM *stream, 
					   OUT QUERY_INFO *queryInfo )
	{
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the header */
	readConstructed( stream, NULL, CTAG_RI_KEKRI );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != KEK_VERSION )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_ERROR_NOTAVAIL );
	}

#if 0	/* 21/4/06 Disabled since it was never used */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeCmsKek( INOUT STREAM *stream, 
						IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						IN_BUFFER( encryptedKeyLength ) const BYTE *encryptedKey, 
						IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) \
							const int encryptedKeyLength )
	{
	STREAM localStream;
	MESSAGE_DATA msgData;
	BYTE kekInfo[ 128 + 8 ], label[ CRYPT_MAX_TEXTSIZE + 8 ];
	const int algoIdInfoSize = \
				sizeofContextAlgoID( iCryptContext, CRYPT_ALGO_NONE, 
									 ALGOID_FLAG_NONE );
	int kekInfoSize, labelSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKeyLength >= MIN_KEYSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );

	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );

	/* Get the label */
	setMessageData( &msgData, label, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		return( status );
	labelSize = msgData.length;

	/* Determine the size of the KEK info.  To save evaluating it twice in a
	   row and because it's short, we just write it to local buffers */
	sMemOpen( &localStream, kekInfo, 128 );
	writeSequence( &localStream, sizeofOID( OID_PWRIKEK ) + algoIdInfoSize );
	writeOID( &localStream, OID_PWRIKEK );
	status = writeContextCryptAlgoID( &localStream, iCryptContext );
	if( cryptStatusOK( status ) )
		kekInfoSize = stell( &localStream );
	sMemDisconnect( &localStream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the algorithm identifiers and encrypted key */
	writeConstructed( stream, ( int ) sizeofShortInteger( KEK_VERSION ) + \
					  sizeofObject( sizeofObject( labelSize ) ) + \
					  kekInfoSize + sizeofObject( encryptedKeyLength ),
					  CTAG_RI_KEKRI );
	writeShortInteger( stream, KEK_VERSION, DEFAULT_TAG );
	writeSequence( stream, sizeofObject( labelSize ) );
	writeOctetString( stream, label, labelSize, DEFAULT_TAG );
	swrite( stream, kekInfo, kekInfoSize );
	return( writeOctetString( stream, encryptedKey, encryptedKeyLength,
							  DEFAULT_TAG ) );
	}
#endif /* 0 */

/* Read/write cryptlib KEK data:

	[3] SEQUENCE {
		version						INTEGER (0),
		keyDerivationAlgorithm	[0]	AlgorithmIdentifier OPTIONAL,
		keyEncryptionAlgorithm		AlgorithmIdentifier,
		encryptedKey				OCTET STRING
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCryptlibKek( INOUT STREAM *stream, 
							OUT QUERY_INFO *queryInfo )
	{
	QUERY_INFO keyDerivationQueryInfo = DUMMY_INIT_STRUCT;
	const int startPos = stell( stream );
	BOOLEAN hasDerivationInfo = FALSE;
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_INTLENGTH );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* If it's a CMS KEK, read it as such */
	if( peekTag( stream ) == CTAG_RI_KEKRI )
		return( readCmsKek( stream, queryInfo ) );

	/* Read the header */
	readConstructed( stream, NULL, CTAG_RI_PWRI );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != PWRI_VERSION )
		return( CRYPT_ERROR_BADDATA );

	/* Read the optional KEK derivation info and KEK algorithm info */
	if( peekTag( stream ) == MAKE_CTAG( CTAG_KK_DA ) )
		{
		status = readKeyDerivationInfo( stream, &keyDerivationQueryInfo );
		if( cryptStatusError( status ) )
			return( status );
		hasDerivationInfo = TRUE;
		}
	readSequence( stream, NULL );
	readFixedOID( stream, OID_PWRIKEK, sizeofOID( OID_PWRIKEK ) );
	status = readContextAlgoID( stream, NULL, queryInfo, DEFAULT_TAG,
								ALGOID_CLASS_CRYPT );
	if( cryptStatusError( status ) )
		return( status );

	/* If there's key-derivation information available, copy it across to 
	   the overall query information */
	if( hasDerivationInfo )
		{
		memcpy( queryInfo->salt, keyDerivationQueryInfo.salt,
				keyDerivationQueryInfo.saltLength );
		queryInfo->saltLength = keyDerivationQueryInfo.saltLength;
		queryInfo->keySetupIterations = \
						keyDerivationQueryInfo.keySetupIterations;
		queryInfo->keySetupAlgo = keyDerivationQueryInfo.keySetupAlgo;
		if( keyDerivationQueryInfo.keySize > 0 )
			{
			/* How to handle the optional keysize value from the key-
			   derivation information is a bit unclear, for example what 
			   should we do if the encryption algorithm is AES-256 but the 
			   keysize is 128 bits?  At the moment this problem is resolved
			   by the fact that nothing seems to use the keysize value */
			queryInfo->keySize = keyDerivationQueryInfo.keySize;
			}
		}

	/* Finally, read the start of the encrypted key */
	status = readOctetStringHole( stream, &queryInfo->dataLength, 
								  MIN_KEYSIZE, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining key data is present */
	return( sSkip( stream, queryInfo->dataLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeCryptlibKek( STREAM *stream, 
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							 IN_BUFFER( encryptedKeyLength ) \
								const BYTE *encryptedKey, 
							 IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) \
								const int encryptedKeyLength )
	{
	STREAM localStream;
	BYTE derivationInfo[ CRYPT_MAX_HASHSIZE + 32 + 8 ], kekInfo[ 128 + 8 ];
	BOOLEAN hasKeyDerivationInfo = TRUE;
	const int algoIdInfoSize = sizeofCryptContextAlgoID( iCryptContext );
	int derivationInfoSize = 0, kekInfoSize = DUMMY_INIT, value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKeyLength >= MIN_KEYSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );

	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );

	/* If it's a non-password-derived key and there's a label attached,
	   write it as a KEKRI with a PWRI algorithm identifier as the key
	   encryption algorithm */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( status == CRYPT_ERROR_NOTINITED )
		{
		hasKeyDerivationInfo = FALSE;

#if 0	/* 21/4/06 Disabled since it was never used */
		MESSAGE_DATA msgData;

		/* There's no password-derivation information present, see if there's
		   a label present */
		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_LABEL );
		if( cryptStatusOK( status ) )
			{
			/* There's a label present, write it as a PWRI within a KEKRI */
			return( writeCmsKek( stream, iCryptContext, encryptedKey,
								 encryptedKeyLength ) );
			}
#endif /* 0 */
		}

	/* Determine the size of the derivation info and KEK info.  To save
	   evaluating it twice in a row and because it's short, we just write
	   it to local buffers */
	if( hasKeyDerivationInfo )
		{
		sMemOpen( &localStream, derivationInfo, CRYPT_MAX_HASHSIZE + 32 );
		status = writeKeyDerivationInfo( &localStream, iCryptContext );
		if( cryptStatusOK( status ) )
			derivationInfoSize = stell( &localStream );
		sMemDisconnect( &localStream );
		if( cryptStatusError( status ) )
			return( status );
		}
	sMemOpen( &localStream, kekInfo, 128 );
	writeSequence( &localStream, sizeofOID( OID_PWRIKEK ) + algoIdInfoSize );
	writeOID( &localStream, OID_PWRIKEK );
	status = writeCryptContextAlgoID( &localStream, iCryptContext );
	if( cryptStatusOK( status ) )
		kekInfoSize = stell( &localStream );
	sMemDisconnect( &localStream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the algorithm identifiers and encrypted key */
	writeConstructed( stream, sizeofShortInteger( PWRI_VERSION ) +
					  derivationInfoSize + kekInfoSize +
					  ( int ) sizeofObject( encryptedKeyLength ),
					  CTAG_RI_PWRI );
	writeShortInteger( stream, PWRI_VERSION, DEFAULT_TAG );
	if( derivationInfoSize > 0 )
		swrite( stream, derivationInfo, derivationInfoSize );
	swrite( stream, kekInfo, kekInfoSize );
	return( writeOctetString( stream, encryptedKey, encryptedKeyLength,
							  DEFAULT_TAG ) );
	}

#ifdef USE_PGP

/* Read/write PGP KEK data.

	SKE:
		byte	ctb = PGP_PACKET_SKE
		byte[]	length
		byte	version = PGP_VERSION_OPENPGP
		byte	cryptAlgo
		byte	stringToKey specifier, 0, 1, or 3
		byte[]	stringToKey data
				0x00: byte		hashAlgo
				0x01: byte[8]	salt
				0x03: byte		iterations */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpKek( INOUT STREAM *stream, 
					   OUT QUERY_INFO *queryInfo )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Make sure that the packet header is in order and check the packet
	   version.  This is an OpenPGP-only packet */
	status = getPgpPacketInfo( stream, queryInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sgetc( stream ) != PGP_VERSION_OPENPGP )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->version = PGP_VERSION_OPENPGP;

	/* Get the password hash algorithm */
	status = readPgpAlgo( stream, &queryInfo->cryptAlgo, 
						  PGP_ALGOCLASS_PWCRYPT );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the S2K information */
	return( readPgpS2K( stream, &queryInfo->keySetupAlgo, queryInfo->salt,  
						PGP_SALTSIZE, &queryInfo->saltLength,
						&queryInfo->keySetupIterations ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writePgpKek( INOUT STREAM *stream, 
						IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						STDC_UNUSED const BYTE *encryptedKey, 
						STDC_UNUSED const int encryptedKeyLength )
	{
	BYTE salt[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashAlgo = DUMMY_INIT, kekCryptAlgo = DUMMY_INIT;	/* int vs.enum */
	int pgpKekCryptAlgo, pgpHashAlgo = DUMMY_INIT, keySetupIterations;
	int count = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKey == NULL && encryptedKeyLength == 0 );

	/* Get the key derivation information */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
						&keySetupIterations, CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
						&hashAlgo, CRYPT_CTXINFO_KEYING_ALGO );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
						&kekCryptAlgo, CRYPT_CTXINFO_ALGO );
		}
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, salt, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
						&msgData, CRYPT_CTXINFO_KEYING_SALT );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = cryptlibToPgpAlgo( kekCryptAlgo, &pgpKekCryptAlgo );
	if( cryptStatusOK( status ) )
		status = cryptlibToPgpAlgo( hashAlgo, &pgpHashAlgo );
	ENSURES( cryptStatusOK( status ) );

	/* Calculate the PGP "iteration count" from the value used to derive
	   the key.  The "iteration count" is actually a count of how many bytes
	   are hashed, this is because the "iterated hashing" treats the salt +
	   password as an infinitely-repeated sequence of values and hashes the
	   resulting string for PGP-iteration-count bytes worth.  Instead of
	   being written directly the count is encoded in a complex manner that
	   saves a whole byte, so before we can write it we have to encode it
	   into the base + exponent form expected by PGP.  This has a default
	   base of 16 + the user-supplied base value, we can set this to zero
	   since the iteration count used by cryptlib is always a multiple of
	   16, the remainder is just log2 of what's left of the iteration
	   count */
	REQUIRES( keySetupIterations % 16 == 0 );
	keySetupIterations /= 32;	/* Remove fixed offset before log2 op.*/
	while( keySetupIterations > 0 )
		{
		count++;
		keySetupIterations >>= 1;
		}
	count <<= 4;				/* Exponent comes first */
	ENSURES( count >= 0 && count <= 0xFF );

	/* Write the SKE packet */
	pgpWritePacketHeader( stream, PGP_PACKET_SKE, 
						  PGP_VERSION_SIZE + PGP_ALGOID_SIZE + 1 + \
							PGP_ALGOID_SIZE + PGP_SALTSIZE + 1 );
	sputc( stream, PGP_VERSION_OPENPGP );
	sputc( stream, pgpKekCryptAlgo );
	sputc( stream, 3 );		/* S2K = salted, iterated hash */
	sputc( stream, pgpHashAlgo );
	swrite( stream, salt, PGP_SALTSIZE );
	return( sputc( stream, count ) );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*						Public-key Encrypted Key Routines					*
*																			*
****************************************************************************/

/* Read/write CMS key transport data:

	SEQUENCE {
		version						INTEGER (0),
		issuerAndSerial				IssuerAndSerialNumber,
		algorithm					AlgorithmIdentifier,
		encryptedKey				OCTET STRING
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCmsKeytrans( INOUT STREAM *stream, 
							OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	long value;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_INTLENGTH );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the header and version number */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != KEYTRANS_VERSION )
		return( CRYPT_ERROR_BADDATA );

	/* Read the key ID and PKC algorithm information.  Since we're recording 
	   the position of the issuerAndSerialNumber as a blob we have to use
	   getStreamObjectLength() to get the overall blob data size */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->iAndSStart = stell( stream ) - startPos;
	queryInfo->iAndSLength = length;
	sSkip( stream, length );
	status = readAlgoID( stream, &queryInfo->cryptAlgo, ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) )
		return( status );

	/* Finally, read the start of the encrypted key */
	status = readOctetStringHole( stream, &queryInfo->dataLength, 
								  MIN_PKCSIZE, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining key data is present */
	return( sSkip( stream, queryInfo->dataLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int writeCmsKeytrans( INOUT STREAM *stream,
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							 IN_BUFFER( encryptedKeyLength ) \
								const BYTE *encryptedKey, 
							 IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
								const int encryptedKeyLength,
							 IN_BUFFER( auxInfoLength ) const void *auxInfo, 
							 IN_LENGTH_SHORT const int auxInfoLength )
	{
	const int algoIdInfoSize = \
				sizeofContextAlgoID( iCryptContext, CRYPT_ALGO_NONE );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );
	assert( isReadPtr( auxInfo, auxInfoLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKeyLength >= MIN_PKCSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( auxInfoLength > 0 && auxInfoLength < MAX_INTLENGTH_SHORT );

	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );

	writeSequence( stream, sizeofShortInteger( KEYTRANS_VERSION ) +
				   auxInfoLength + algoIdInfoSize + \
				   ( int ) sizeofObject( encryptedKeyLength ) );
	writeShortInteger( stream, KEYTRANS_VERSION, DEFAULT_TAG );
	swrite( stream, auxInfo, auxInfoLength );
	writeContextAlgoID( stream, iCryptContext, CRYPT_ALGO_NONE );
	return( writeOctetString( stream, encryptedKey, encryptedKeyLength, 
							  DEFAULT_TAG ) );
	}

/* Read/write cryptlib key transport data:

	SEQUENCE {
		version						INTEGER (2),
		keyID					[0]	SubjectKeyIdentifier,
		algorithm					AlgorithmIdentifier,
		encryptedKey				OCTET STRING
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCryptlibKeytrans( INOUT STREAM *stream, 
								 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_INTLENGTH );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the header and version number */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != KEYTRANS_EX_VERSION )
		return( CRYPT_ERROR_BADDATA );

	/* Read the key ID and PKC algorithm information */
	readOctetStringTag( stream, queryInfo->keyID, &queryInfo->keyIDlength,
						8, CRYPT_MAX_HASHSIZE, CTAG_KT_SKI );
	status = readAlgoID( stream, &queryInfo->cryptAlgo, 
						 ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) )
		return( status );

	/* Finally, read the start of the encrypted key */
	status = readOctetStringHole( stream, &queryInfo->dataLength, 
								  MIN_KEYSIZE, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining key data is present */
	return( sSkip( stream, queryInfo->dataLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeCryptlibKeytrans( INOUT STREAM *stream,
								  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
								  IN_BUFFER( encryptedKeyLength ) \
									const BYTE *encryptedKey, 
								  IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
									const int encryptedKeyLength,
								  STDC_UNUSED const void *auxInfo,
								  STDC_UNUSED const int auxInfoLength )
	{
	MESSAGE_DATA msgData;
	BYTE keyID[ CRYPT_MAX_HASHSIZE + 8 ];
	const int algoIdInfoSize = \
				sizeofContextAlgoID( iCryptContext, CRYPT_ALGO_NONE );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKeyLength >= MIN_PKCSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( auxInfo == NULL && auxInfoLength == 0 );

	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );

	setMessageData( &msgData, keyID, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEYID );
	if( cryptStatusError( status ) )
		return( status );
	writeSequence( stream, sizeofShortInteger( KEYTRANS_EX_VERSION ) +
				   ( int ) sizeofObject( msgData.length ) + algoIdInfoSize + \
				   ( int ) sizeofObject( encryptedKeyLength ) );
	writeShortInteger( stream, KEYTRANS_EX_VERSION, DEFAULT_TAG );
	writeOctetString( stream, msgData.data, msgData.length, CTAG_KT_SKI );
	writeContextAlgoID( stream, iCryptContext, CRYPT_ALGO_NONE );
	return( writeOctetString( stream, encryptedKey, encryptedKeyLength, 
							  DEFAULT_TAG ) );
	}

#ifdef USE_PGP

/* Read/write PGP key transport data:

	PKE:
		byte	ctb = PGP_PACKET_PKE
		byte[]	length
		byte	version = PGP_VERSION_PGP2 or 3 (= OpenPGP, not the expected PGP3)
		byte[8]	keyID
		byte	PKC algo
		mpi(s)	encrypted session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpKeytrans( INOUT STREAM *stream, 
							OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_INTLENGTH );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Make sure that the packet header is in order and check the packet
	   version.  For this packet type a version number of 3 denotes OpenPGP
	   whereas for signatures it denotes PGP 2.x, so we translate the value
	   that we return to the caller */
	status = getPgpPacketInfo( stream, queryInfo );
	if( cryptStatusError( status ) )
		return( status );
	value = sgetc( stream );
	if( value != PGP_VERSION_2 && value != 3 )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->version = ( value == PGP_VERSION_2 ) ? \
						 PGP_VERSION_2 : PGP_VERSION_OPENPGP;

	/* Get the PGP key ID and algorithm */
	status = sread( stream, queryInfo->keyID, PGP_KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->keyIDlength = PGP_KEYID_SIZE;
	status = readPgpAlgo( stream, &queryInfo->cryptAlgo, 
						  PGP_ALGOCLASS_PKCCRYPT );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the RSA-encrypted key, recording the position and length of the 
	   raw RSA-encrypted integer value.  We have to be careful how we handle 
	   this because readInteger16Ubits() returns the canonicalised form of
	   the values (with leading zeroes truncated) so an stell() before the 
	   read doesn't necessarily represent the start of the payload:

		startPos	dataStart		 stell()
			|			|				|
			v			v <-- length -->v
		+---+-----------+---------------+
		|	|			|///////////////| Stream
		+---+-----------+---------------+ */
	if( queryInfo->cryptAlgo == CRYPT_ALGO_RSA )
		{
		status = readInteger16Ubits( stream, NULL, &queryInfo->dataLength,
									 MIN_PKCSIZE, CRYPT_MAX_PKCSIZE );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->dataStart = ( stell( stream ) - startPos ) - \
							   queryInfo->dataLength;
		}
	else
		{
		const int dataStartPos = stell( stream );
		int dummy;

		REQUIRES( dataStartPos >= 0 && dataStartPos < MAX_INTLENGTH );
		REQUIRES( queryInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL );

		/* Read the Elgamal-encrypted key, recording the position and
		   combined lengths of the MPI pair.  Again, we can't use the length
		   returned by readInteger16Ubits() to determine the overall size 
		   but have to calculate it from the position in the stream */
		status = readInteger16Ubits( stream, NULL, &dummy, MIN_PKCSIZE,
									 CRYPT_MAX_PKCSIZE );
		if( cryptStatusOK( status ) )
			status = readInteger16Ubits( stream, NULL, &dummy, MIN_PKCSIZE,
										 CRYPT_MAX_PKCSIZE );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->dataStart = dataStartPos - startPos;
		queryInfo->dataLength = stell( stream ) - dataStartPos;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writePgpKeytrans( INOUT STREAM *stream,
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							 IN_BUFFER( encryptedKeyLength ) \
								const BYTE *encryptedKey, 
							 IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
								const int encryptedKeyLength,
							 STDC_UNUSED const void *auxInfo, 
							 STDC_UNUSED const int auxInfoLength )
	{
	BYTE keyID[ PGP_KEYID_SIZE + 8 ];
	int algorithm, pgpAlgo, status;	/* int vs.enum */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( encryptedKeyLength >= MIN_PKCSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( auxInfo == NULL && auxInfoLength == 0 );

	/* Get the key information */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, keyID, PGP_KEYID_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_KEYID_OPENPGP );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = cryptlibToPgpAlgo( algorithm, &pgpAlgo );
	ENSURES( cryptStatusOK( status ) );

	/* Write the PKE packet */
	pgpWritePacketHeader( stream, PGP_PACKET_PKE,
						  PGP_VERSION_SIZE + PGP_KEYID_SIZE + PGP_ALGOID_SIZE + \
						  ( ( algorithm == CRYPT_ALGO_RSA ) ? \
							sizeofInteger16U( encryptedKeyLength ) : \
							encryptedKeyLength ) );
	sputc( stream, 3 );		/* Version = 3 (OpenPGP) */
	swrite( stream, keyID, PGP_KEYID_SIZE );
	sputc( stream, pgpAlgo );
	return( ( algorithm == CRYPT_ALGO_RSA ) ? \
			writeInteger16Ubits( stream, encryptedKey, encryptedKeyLength ) :
			swrite( stream, encryptedKey, encryptedKeyLength ) );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*					Key Exchange Read/Write Access Function					*
*																			*
****************************************************************************/

typedef struct {
	const KEYEX_TYPE type;
	const READKEYTRANS_FUNCTION function;
	} KEYTRANS_READ_INFO;
static const KEYTRANS_READ_INFO keytransReadTable[] = {
	{ KEYEX_CMS, readCmsKeytrans },
	{ KEYEX_CRYPTLIB, readCryptlibKeytrans },
#ifdef USE_PGP
	{ KEYEX_PGP, readPgpKeytrans },
#endif /* USE_PGP */
	{ KEYEX_NONE, NULL }, { KEYEX_NONE, NULL }
	};

typedef struct {
	const KEYEX_TYPE type;
	const WRITEKEYTRANS_FUNCTION function;
	} KEYTRANS_WRITE_INFO;
static const KEYTRANS_WRITE_INFO keytransWriteTable[] = {
	{ KEYEX_CMS, writeCmsKeytrans },
	{ KEYEX_CRYPTLIB, writeCryptlibKeytrans },
#ifdef USE_PGP
	{ KEYEX_PGP, writePgpKeytrans },
#endif /* USE_PGP */
	{ KEYEX_NONE, NULL }, { KEYEX_NONE, NULL }
	};

typedef struct {
	const KEYEX_TYPE type;
	const READKEK_FUNCTION function;
	} KEK_READ_INFO;
static const KEK_READ_INFO kekReadTable[] = {
	{ KEYEX_CMS, readCryptlibKek },
	{ KEYEX_CRYPTLIB, readCryptlibKek },
#ifdef USE_PGP
	{ KEYEX_PGP, readPgpKek },
#endif /* USE_PGP */
	{ KEYEX_NONE, NULL }, { KEYEX_NONE, NULL }
	};

typedef struct {
	const KEYEX_TYPE type;
	const WRITEKEK_FUNCTION function;
	} KEK_WRITE_INFO;
static const KEK_WRITE_INFO kekWriteTable[] = {
	{ KEYEX_CMS, writeCryptlibKek },
	{ KEYEX_CRYPTLIB, writeCryptlibKek },
#ifdef USE_PGP
	{ KEYEX_PGP, writePgpKek },
#endif /* USE_PGP */
	{ KEYEX_NONE, NULL }, { KEYEX_NONE, NULL }
	};

CHECK_RETVAL_PTR \
READKEYTRANS_FUNCTION getReadKeytransFunction( IN_ENUM( KEYEX ) \
												const KEYEX_TYPE keyexType )
	{
	int i;

	REQUIRES_N( keyexType > KEYEX_NONE && keyexType < KEYEX_LAST );

	for( i = 0; 
		 keytransReadTable[ i ].type != KEYEX_NONE && \
			i < FAILSAFE_ARRAYSIZE( keytransReadTable, KEYTRANS_READ_INFO ); 
		 i++ )
		{
		if( keytransReadTable[ i ].type == keyexType )
			return( keytransReadTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( keytransReadTable, KEYTRANS_READ_INFO ) );

	return( NULL );
	}
CHECK_RETVAL_PTR \
WRITEKEYTRANS_FUNCTION getWriteKeytransFunction( IN_ENUM( KEYEX ) \
													const KEYEX_TYPE keyexType )
	{
	int i;

	REQUIRES_N( keyexType > KEYEX_NONE && keyexType < KEYEX_LAST );

	for( i = 0; 
		 keytransWriteTable[ i ].type != KEYEX_NONE && \
			i < FAILSAFE_ARRAYSIZE( keytransWriteTable, KEYTRANS_WRITE_INFO ); 
		 i++ )
		{
		if( keytransWriteTable[ i ].type == keyexType )
			return( keytransWriteTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( keytransWriteTable, KEYTRANS_WRITE_INFO ) );

	return( NULL );
	}
CHECK_RETVAL_PTR \
READKEK_FUNCTION getReadKekFunction( IN_ENUM( KEYEX ) \
										const KEYEX_TYPE keyexType )
	{
	int i;

	REQUIRES_N( keyexType > KEYEX_NONE && keyexType < KEYEX_LAST );

	for( i = 0; 
		 kekReadTable[ i ].type != KEYEX_NONE && \
			i < FAILSAFE_ARRAYSIZE( kekReadTable, KEK_READ_INFO ); 
		 i++ )
		{
		if( kekReadTable[ i ].type == keyexType )
			return( kekReadTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( kekReadTable, KEK_READ_INFO ) );
		
	return( NULL );
	}
CHECK_RETVAL_PTR \
WRITEKEK_FUNCTION getWriteKekFunction( IN_ENUM( KEYEX ) \
										const KEYEX_TYPE keyexType )
	{
	int i;

	REQUIRES_N( keyexType > KEYEX_NONE && keyexType < KEYEX_LAST );

	for( i = 0; 
		 kekWriteTable[ i ].type != KEYEX_NONE && \
			i < FAILSAFE_ARRAYSIZE( kekWriteTable, KEK_WRITE_INFO ); 
		 i++ )
		{
		if( kekWriteTable[ i ].type == keyexType )
			return( kekWriteTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( kekWriteTable, KEK_WRITE_INFO ) );

	return( NULL );
	}
