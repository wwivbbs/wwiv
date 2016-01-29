/****************************************************************************
*																			*
*					cryptlib Signature Mechanism Routines					*
*					  Copyright Peter Gutmann 1992-2013						*
*																			*
****************************************************************************/

#ifdef INC_ALL
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "mech_int.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "mechs/mech_int.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Utility Routines - ASN.1 Replacement				*
*																			*
****************************************************************************/

/* If we're building cryptlib without ASN.1 support then we still need some
   minimal ASN.1 functionality in order to encode and decode MessageDigest
   records in signatures.  The following bare-bones functions provide this
   support, these are extremely cut-down versions of code normally found in 
   asn1_rd.c and asn1_ext.c */

#ifndef USE_INT_ASN1

#define readTag					sgetc
#define writeTag				sputc
#define sizeofObject( length )	( 1 + 1 + ( length ) )
#define BER_OCTETSTRING			0x04
#define BER_NULL				0x05
#define BER_SEQUENCE			0x30
#define MKOID( value )			( ( const BYTE * ) value )
#define sizeofOID( oid )		( 1 + 1 + ( int ) oid[ 1 ] )
#define sizeofNull()			( 1 + 1 )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readRawObject( INOUT STREAM *stream, 
						  OUT_BUFFER( bufferMaxLength, *bufferLength ) \
								BYTE *buffer,
						  IN_LENGTH_SHORT_MIN( 3 ) \
								const int bufferMaxLength, 
						  OUT_LENGTH_BOUNDED_Z( bufferMaxLength ) \
								int *bufferLength, 
						  IN_TAG_ENCODED const int tag )
	{
	const int objectTag = readTag( stream );
	int length, offset = 0;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, bufferMaxLength ) );
	assert( isWritePtr( bufferLength, sizeof( int ) ) );

	REQUIRES_S( bufferMaxLength > 16 && \
				bufferMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == BER_SEQUENCE );

	/* Clear return values */
	memset( buffer, 0, min( 16, bufferMaxLength ) );
	*bufferLength = 0;

	/* Read the identifier field and length.  We need to remember each byte 
	   as it's read so we can't just call readLengthValue() for the length, 
	   but since we only need to handle lengths that can be encoded in one 
	   byte this isn't a problem */
	if( cryptStatusError( objectTag ) )
		return( objectTag );
	if( objectTag != BER_SEQUENCE )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	buffer[ offset++ ] = intToByte( objectTag );
	length = sgetc( stream );
	if( cryptStatusError( length ) )
		return( length );
	if( length <= 0 || length > 0x7F )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	buffer[ offset++ ] = intToByte( length );
	if( offset + length > bufferMaxLength )
		return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );

	/* Read in the rest of the data */
	*bufferLength = offset + length;
	return( sread( stream, buffer + offset, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeHeader( INOUT STREAM *stream, 
						IN_TAG_ENCODED const int tag,
						IN_LENGTH_SHORT const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( tag >= BER_OCTETSTRING && tag <= BER_SEQUENCE );
	REQUIRES_S( length >= 0 && length <= 255 );

	writeTag( stream, tag );
	return( sputc( stream, length ) );
	}

CHECK_RETVAL_PTR \
static const BYTE *getOID( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo )
	{
	REQUIRES_N( isHashAlgo( hashAlgo ) );

	switch( hashAlgo )
		{
		case CRYPT_ALGO_SHA1:
			return( MKOID( "\x06\x05\x2B\x0E\x03\x02\x1A" ) );

		case CRYPT_ALGO_SHA2:
			return( MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01" ) );

		default:
			retIntError_Null();
		}

	retIntError_Null();
	}

CHECK_RETVAL_LENGTH \
static int sizeofAlgoIDex( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo )
	{
	const BYTE *oid = getOID( hashAlgo );

	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( oid != NULL );

	return( ( int ) sizeofObject( sizeofOID( oid ) + sizeofNull() ) );
	}

CHECK_RETVAL_LENGTH \
static int sizeofMessageDigest( IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
								IN_LENGTH_HASH const int hashSize )
	{
	const int hashParam = isParameterisedHashAlgo( hashAlgo ) ? hashSize : 0;
	int algoInfoSize, hashInfoSize;

	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( hashParam == 0 );

	algoInfoSize = sizeofAlgoIDex( hashAlgo );
	hashInfoSize = sizeofObject( hashSize );
	ENSURES( algoInfoSize > 8 && algoInfoSize < MAX_INTLENGTH_SHORT );
	ENSURES( hashInfoSize > hashSize && hashInfoSize < MAX_INTLENGTH_SHORT );

	return( sizeofObject( algoInfoSize + hashInfoSize ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int writeMessageDigest( INOUT STREAM *stream, 
							   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							   IN_BUFFER( hashSize ) const void *hash, 
							   IN_LENGTH_HASH const int hashSize )
	{
	const int hashParam = isParameterisedHashAlgo( hashAlgo ) ? hashSize : 0;
	const BYTE *oid = getOID( hashAlgo );
	
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( hash, hashSize ) );

	REQUIRES_S( isHashAlgo( hashAlgo ) );
	REQUIRES_S( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES_S( hashParam == 0 );

	/* writeSequence() */
	writeHeader( stream, BER_SEQUENCE, 
				 sizeofAlgoIDex( hashAlgo ) + sizeofObject( hashSize ) );

	/* writeAlgoIDex() */
	writeHeader( stream, BER_SEQUENCE, sizeofOID( oid ) + sizeofNull() );
	swrite( stream, oid, sizeofOID( oid ) );
	writeHeader( stream, BER_NULL, 0 );

	/* writeOctetString() */
	writeHeader( stream, BER_OCTETSTRING, hashSize );
	return( swrite( stream, hash, hashSize ) );
	}
#endif /* USE_INT_ASN1 */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Unlike PKCS #1 encryption there isn't any minimum-height requirement for 
   the PKCS #1 signature padding, however we require a set minimum number of 
   bytes of 0xFF padding because if they're not present then there's 
   something funny going on.  For a given key size we require that all but
   ( 3 bytes PKCS #1 formatting + ( 2 + 15 + 2 ) bytes ASN.1 wrapper + 
     CRYPT_MAX_HASHSIZE bytes hash ) be 0xFF padding */

#define getMinPadBytes( length ) \
		( ( length ) - ( 3 + 19 + CRYPT_MAX_HASHSIZE ) )

/* Decode PKCS #1 signature formatting */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int decodePKCS1( INOUT STREAM *stream, 
						IN_LENGTH_PKC const int length )
	{
	int ch, i;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( length >= MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* Decode the payload using the PKCS #1 format:

		[ 0 ][ 1 ][ 0xFF padding ][ 0 ][ payload ]

	   Note that some implementations may have bignum code that zero-
	   truncates the RSA data, which would remove the leading zero from the 
	   PKCS #1 padding and produce a CRYPT_ERROR_BADDATA error.  It's the 
	   responsibility of the lower-level crypto layer to reformat the data 
	   to return a correctly-formatted result if necessary */
	if( sgetc( stream ) != 0 || sgetc( stream ) != 1 )
		{
		/* No [ 0 ][ 1 ] at start */
		return( CRYPT_ERROR_BADDATA );
		}
	for( i = 2, ch = 0xFF; ( i < length - 16 ) && ( ch == 0xFF ); i++ )
		{
		ch = sgetc( stream );
		if( cryptStatusError( ch ) )
			return( CRYPT_ERROR_BADDATA );
		}
	if( ch != 0 || i < getMinPadBytes( length ) || i >= length - 16 )
		{
		/* No [ 0 ] at end or insufficient/excessive 0xFF padding */
		return( CRYPT_ERROR_BADDATA );
		}

	return( CRYPT_OK );
	}

/* Compare the ASN.1-encoded hash value in the signature with the hash 
   information that we've been given.  We have to be very careful how we 
   handle this because we don't want to allow an attacker to inject random 
   data into gaps in the encoding, which would allow for signature forgery 
   if small exponents are used (although cryptlib disallows any exponents 
   that make this easy).  The obvious approach of using 
   checkObjectEncoding() doesn't work because an attacker can still encode 
   the signature in a form that's syntactically valid ASN.1, just not the 
   correct ASN.1 for a MessageDigest record.  To avoid having to have every 
   function that handles reading the hash value be anal-retentive about 
   every data element that it reads, we take the hash value that we've been 
   given and encode it correctly as a MessageDigest record and then do a 
   straight memcmp() of the encoded form rather than trying to validity-
   check the externally-supplied value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int compareHashInfo( INOUT STREAM *stream, 
							IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							IN_BUFFER( hashSize ) const void *hash, 
							IN_LENGTH_HASH const int hashSize )
	{
	STREAM mdStream;
	BYTE encodedMD[ 32 + CRYPT_MAX_HASHSIZE + 8 ];
	BYTE recreatedMD[ 32 + CRYPT_MAX_HASHSIZE + 8 ];
	int encodedMdLength, recreatedMdLength DUMMY_INIT;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( hash, hashSize ) );

	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );

	/* Read the encoded hash data as a blob */
	status = readRawObject( stream, encodedMD, 32 + CRYPT_MAX_HASHSIZE, 
							&encodedMdLength, BER_SEQUENCE );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the supplied hash information into an encoded blob */
	sMemOpen( &mdStream, recreatedMD, 32 + CRYPT_MAX_HASHSIZE );
	status = writeMessageDigest( &mdStream, hashAlgo, hash, hashSize );
	if( cryptStatusOK( status ) )
		recreatedMdLength = stell( &mdStream );
	sMemDisconnect( &mdStream );
	if( cryptStatusError( status ) )
		return( status );

	/* Compare the two encoded blobs */
	if( encodedMdLength != recreatedMdLength || \
		!compareDataConstTime( encodedMD, recreatedMD, encodedMdLength ) )
		status = CRYPT_ERROR_SIGNATURE;

	zeroise( encodedMD, 32 + CRYPT_MAX_HASHSIZE );
	zeroise( recreatedMD, 32 + CRYPT_MAX_HASHSIZE );

	return( status );
	}

/* Make sure that the recovered signature data matches the data that we 
   originally signed.  The rationale behind this operation is covered (in 
   great detail) in ctx_rsa.c */

static int checkRecoveredSignature( IN_HANDLE const CRYPT_CONTEXT iSignContext,
									IN_BUFFER( sigDataLen ) const void *sigData,
									IN_LENGTH_PKC const int sigDataLen,
									IN_BUFFER( sigLen ) const void *signature,
									IN_LENGTH_PKC const int sigLen )
	{
	BYTE recoveredSignature[ CRYPT_MAX_PKCSIZE + 8 ];
	int status;

	assert( isReadPtr( sigData, sigDataLen ) );
	assert( isReadPtr( signature, sigLen ) );

	REQUIRES( sigDataLen >= MIN_PKCSIZE && sigDataLen <= CRYPT_MAX_PKCSIZE );
	REQUIRES( sigLen >= MIN_PKCSIZE && sigLen <= CRYPT_MAX_PKCSIZE );

	/* Recover the original signature data, unless we're in the unlikely 
	   situation that the key isn't valid for signature checking */
	memcpy( recoveredSignature, signature, sigLen );
	status = krnlSendMessage( iSignContext, IMESSAGE_CTX_SIGCHECK, 
							  recoveredSignature, sigLen );
	if( status == CRYPT_ERROR_PERMISSION || status == CRYPT_ERROR_NOTAVAIL )
		{
		/* The key can't be used for signature checking, there's not much 
		   that we can do */
		return( CRYPT_OK );
		}
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_FAILED );

	/* Make sure that the recovered data matches the original data */
	if( sigDataLen != sigLen || \
		!compareDataConstTime( sigData, recoveredSignature, sigLen ) )
		{
		DEBUG_DIAG(( "Signature consistency check failed" ));
		assert( DEBUG_WARN );
		status = CRYPT_ERROR_FAILED;
		}
	zeroise( recoveredSignature, CRYPT_MAX_PKCSIZE );

	return( status );
	}

/****************************************************************************
*																			*
*								Signature Mechanisms 						*
*																			*
****************************************************************************/

/* Perform signing.  There are several variations of this that are handled 
   through common signature mechanism functions */

typedef enum { SIGN_NONE, SIGN_PKCS1, SIGN_SSL, SIGN_LAST } SIGN_TYPE;

/* Perform PKCS #1 signing/sig.checking */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sign( INOUT MECHANISM_SIGN_INFO *mechanismInfo, 
				 IN_ENUM( SIGN ) const SIGN_TYPE type )
	{
	CRYPT_ALGO_TYPE hashAlgo DUMMY_INIT;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ], hash2[ CRYPT_MAX_HASHSIZE + 8 ];
	BYTE preSigData[ CRYPT_MAX_PKCSIZE + 8 ];
	int sideChannelProtectionLevel DUMMY_INIT;
	int hashSize, hashSize2 DUMMY_INIT, length, i, status;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );

	REQUIRES( type > SIGN_NONE && type < SIGN_LAST );

	/* Clear return value */
	if( mechanismInfo->signature != NULL )
		{
		memset( mechanismInfo->signature, 0,
				mechanismInfo->signatureLength );
		}

	/* Get various algorithm and config parameters */
	status = getPkcAlgoParams( mechanismInfo->signContext, NULL,
							   &length );
	if( cryptStatusOK( status ) )
		status = getHashAlgoParams( mechanismInfo->hashContext,
									&hashAlgo, NULL );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->signContext, 
								  IMESSAGE_GETATTRIBUTE, 
								  &sideChannelProtectionLevel,
								  CRYPT_OPTION_MISC_SIDECHANNELPROTECTION );
		}
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* If this is just a length check, we're done */
	if( mechanismInfo->signature == NULL )
		{
		mechanismInfo->signatureLength = length;

		return( CRYPT_OK );
		}

	/* Get the hash data and determine the encoded payload size */
	setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( mechanismInfo->hashContext,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( status );
	hashSize = msgData.length;
	if( type == SIGN_SSL )
		{
		setMessageData( &msgData, hash2, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( mechanismInfo->hashContext2,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_HASHVALUE );
		if( cryptStatusError( status ) )
			return( status );
		hashSize2 = msgData.length;
		}

	/* Encode the payload as required */
	sMemOpen( &stream, mechanismInfo->signature, length );
	switch( type )
		{
		case SIGN_PKCS1:
			{
			int payloadSize;

			/* Encode the payload using the PKCS #1 format:

				[ 0 ][ 1 ][ 0xFF padding ][ 0 ][ payload ] */
			payloadSize = sizeofMessageDigest( hashAlgo, hashSize );
			sputc( &stream, 0 );
			sputc( &stream, 1 );
			for( i = 0; i < length - ( payloadSize + 3 ); i++ )
				sputc( &stream, 0xFF );
			sputc( &stream, 0 );
			status = writeMessageDigest( &stream, hashAlgo, hash, hashSize );
			break;
			}

		case SIGN_SSL:
			REQUIRES( hashAlgo == CRYPT_ALGO_MD5 );

			/* Encode the payload using the PKCS #1 SSL format:

				[ 0 ][ 1 ][ 0xFF padding ][ 0 ][ MD5 hash ][ SHA1 hash ] */
			sputc( &stream, 0 );
			sputc( &stream, 1 );
			for( i = 0; i < length - ( hashSize + hashSize2 + 3 ); i++ )
				sputc( &stream, 0xFF );
			sputc( &stream, 0 );
			swrite( &stream, hash, hashSize );
			status = swrite( &stream, hash2, hashSize2 );
			break;

		default:
			retIntError();
		}
	ENSURES( cryptStatusError( status ) || stell( &stream ) == length );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->signature, mechanismInfo->signatureLength );
		return( status );
		}

	/* If we're using side-channel protection remember a copy of the 
	   signature data for later so that we can check it against the 
	   recovered signature data */
	if( sideChannelProtectionLevel > 0 )
		memcpy( preSigData, mechanismInfo->signature, length );

	/* Sign the data */
	status = krnlSendMessage( mechanismInfo->signContext,
							  IMESSAGE_CTX_SIGN, mechanismInfo->signature,
							  length );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->signature, mechanismInfo->signatureLength );
		return( status );
		}
	mechanismInfo->signatureLength = length;

	/* If we're using side-channel protection check that the signature 
	   verifies */
	if( sideChannelProtectionLevel > 0 )
		{
		status = checkRecoveredSignature( mechanismInfo->signContext, 
										  preSigData, length,
										  mechanismInfo->signature, length );
		zeroise( preSigData, CRYPT_MAX_PKCSIZE );
		if( cryptStatusError( status ) )
			{
			zeroise( mechanismInfo->signature, length );
			mechanismInfo->signatureLength = 0;
			return( status );
			}
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sigcheck( INOUT MECHANISM_SIGN_INFO *mechanismInfo, 
					 IN_ENUM( SIGN ) const SIGN_TYPE type )
	{
	CRYPT_ALGO_TYPE contextHashAlgo DUMMY_INIT;
	MESSAGE_DATA msgData;
	STREAM stream;
	BYTE decryptedSignature[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int length, hashSize DUMMY_INIT, status;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );
	
	REQUIRES( type > SIGN_NONE && type < SIGN_LAST );

	/* Get various algorithm parameters */
	status = getPkcAlgoParams( mechanismInfo->signContext, NULL,
							   &length );
	if( cryptStatusOK( status ) )
		status = getHashAlgoParams( mechanismInfo->hashContext,
									&contextHashAlgo, NULL );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, hash, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( mechanismInfo->hashContext, 
								  IMESSAGE_GETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_HASHVALUE );
		if( cryptStatusOK( status ) )
			hashSize = msgData.length;
		}
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( length > MIN_PKCSIZE && length <= CRYPT_MAX_PKCSIZE );

	/* Format the input data as required for the signatue check to work */
	status = adjustPKCS1Data( decryptedSignature, CRYPT_MAX_PKCSIZE,
					mechanismInfo->signature, mechanismInfo->signatureLength,
					length );
	if( cryptStatusError( status ) )
		return( status );

	/* Recover the signed data */
	status = krnlSendMessage( mechanismInfo->signContext,
							  IMESSAGE_CTX_SIGCHECK, decryptedSignature,
							  length );
	if( cryptStatusError( status ) )
		return( status );

	/* Decode the payload as required */
	sMemConnect( &stream, decryptedSignature, length );
	switch( type )
		{
		case SIGN_PKCS1:
			/* The payload is an ASN.1-encoded hash, process it very 
			   carefully */
			status = decodePKCS1( &stream, length );
			if( cryptStatusError( status ) )
				break;
			status = compareHashInfo( &stream, contextHashAlgo, hash, 
									  hashSize );
			break;

		case SIGN_SSL:
			{
			BYTE hash2[ CRYPT_MAX_HASHSIZE + 8 ];

			REQUIRES( contextHashAlgo == CRYPT_ALGO_MD5 );

			/* The payload is [ MD5 hash ][ SHA1 hash ] */
			status = decodePKCS1( &stream, length );
			if( cryptStatusError( status ) )
				break;
			status = sread( &stream, hash, 16 );
			if( cryptStatusOK( status ) )
				status = sread( &stream, hash2, 20 );
			if( cryptStatusError( status ) )
				break;

			/* Make sure that the two hash values match */
			setMessageData( &msgData, hash, 16 );
			status = krnlSendMessage( mechanismInfo->hashContext, 
									  IMESSAGE_COMPARE, &msgData, 
									  MESSAGE_COMPARE_HASH );
			if( cryptStatusOK( status ) )
				{
				setMessageData( &msgData, hash2, 20 );
				status = krnlSendMessage( mechanismInfo->hashContext2, 
										  IMESSAGE_COMPARE, &msgData, 
										  MESSAGE_COMPARE_HASH );
				}

			/* Clean up */
			zeroise( hash2, CRYPT_MAX_HASHSIZE );
			break;
			}

		default:
			retIntError();
		}
	if( cryptStatusOK( status ) && sMemDataLeft( &stream ) != 0 )
		{
		/* Make sure that's all that there is.  This is already checked 
		   implicitly anderswhere but we make the check explicit here */
		status = CRYPT_ERROR_BADDATA;
		}
	sMemDisconnect( &stream );

	/* Clean up */
	zeroise( decryptedSignature, CRYPT_MAX_PKCSIZE );
	zeroise( hash, CRYPT_MAX_HASHSIZE );
	return( cryptStatusError( status ) ? CRYPT_ERROR_SIGNATURE : status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int signPKCS1( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_SIGN_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );

	return( sign( mechanismInfo, SIGN_PKCS1 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int sigcheckPKCS1( STDC_UNUSED void *dummy, 
				   INOUT MECHANISM_SIGN_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );

	return( sigcheck( mechanismInfo, SIGN_PKCS1 ) );
	}

#ifdef USE_SSL

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int signSSL( STDC_UNUSED void *dummy, 
			 INOUT MECHANISM_SIGN_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );

	return( sign( mechanismInfo, SIGN_SSL ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int sigcheckSSL( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_SIGN_INFO *mechanismInfo )
	{
	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_SIGN_INFO ) ) );

	return( sigcheck( mechanismInfo, SIGN_SSL ) );
	}
#endif /* USE_SSL */
