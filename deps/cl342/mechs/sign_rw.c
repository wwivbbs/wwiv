/****************************************************************************
*																			*
*						  Signature Read/Write Routines						*
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

/* Context-specific tags for the SignerInfo record */

enum { CTAG_SI_SKI };

/****************************************************************************
*																			*
*							X.509 Signature Routines						*
*																			*
****************************************************************************/

#ifdef USE_INT_ASN1

/* Read/write raw signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readRawSignature( INOUT STREAM *stream, 
							 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the start of the signature */
	status = readBitStringHole( stream, &queryInfo->dataLength, 18 + 18,
								DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, queryInfo->dataLength, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeRawSignature( INOUT STREAM *stream, 
							  STDC_UNUSED const CRYPT_CONTEXT iSignContext,
							  STDC_UNUSED const CRYPT_ALGO_TYPE hashAlgo,
							  STDC_UNUSED const int hashParam,
							  STDC_UNUSED const CRYPT_ALGO_TYPE signAlgo,
							  IN_BUFFER( signatureLength ) const BYTE *signature,
							  IN_LENGTH_SHORT_MIN( 40 ) const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( signatureLength >= 40 && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	/* Write the BIT STRING wrapper and signature */
	writeBitStringHole( stream, signatureLength, DEFAULT_TAG );
	return( writeRawObject( stream, signature, signatureLength ) );
	}

/* Read/write X.509 signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readX509Signature( INOUT STREAM *stream, 
							  OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the signature/hash algorithm information followed by the start
	   of the signature */
	status = readAlgoIDex( stream, &queryInfo->cryptAlgo,
						   &queryInfo->hashAlgo, &queryInfo->hashAlgoParam, 
						   ALGOID_CLASS_PKCSIG );
	if( cryptStatusOK( status ) )
		{
		status = readBitStringHole( stream, &queryInfo->dataLength, 18 + 18,
									DEFAULT_TAG );
		}
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, queryInfo->dataLength, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeX509Signature( INOUT STREAM *stream,
							   IN_HANDLE const CRYPT_CONTEXT iSignContext,
							   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							   STDC_UNUSED const int hashParam,
							   STDC_UNUSED const CRYPT_ALGO_TYPE signAlgo,
							   IN_BUFFER( signatureLength ) const BYTE *signature,
							   IN_LENGTH_SHORT_MIN( 40 ) const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( signatureLength >= 40 && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	/* Write the hash+signature algorithm identifier followed by the BIT
	   STRING wrapper and signature */
	writeContextAlgoID( stream, iSignContext, hashAlgo );
	writeBitStringHole( stream, signatureLength, DEFAULT_TAG );
	return( writeRawObject( stream, signature, signatureLength ) );
	}
#endif /* USE_INT_ASN1 */

/****************************************************************************
*																			*
*							CMS Signature Routines							*
*																			*
****************************************************************************/

#ifdef USE_INT_CMS

/* Read/write PKCS #7/CMS (issuerAndSerialNumber) signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCmsSignature( INOUT STREAM *stream, 
							 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	long value, endPos;
	int tag, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = startPos + length;

	/* Read the header */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != SIGNATURE_VERSION )
		return( CRYPT_ERROR_BADDATA );

	/* Read the issuer and serial number and hash algorithm ID.  Since we're 
	   recording the position of the issuerAndSerialNumber as a blob we have 
	   to use getStreamObjectLength() to get the overall blob data size */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->iAndSStart = stell( stream ) - startPos;
	queryInfo->iAndSLength = length;
	status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoIDex( stream, &queryInfo->hashAlgo, NULL, 
							   &queryInfo->hashAlgoParam, 
							   ALGOID_CLASS_HASH );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the authenticated attributes if there are any present */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 0 ) )
		{
		status = getStreamObjectLength( stream, &length );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->attributeStart = stell( stream ) - startPos;
		queryInfo->attributeLength = length;
		status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the CMS/cryptlib signature algorithm and the start of the 
	   signature.  CMS separates the signature algorithm from the hash 
	   algorithm so we read it as ALGOID_CLASS_PKC and not 
	   ALGOID_CLASS_PKCSIG.  Unfortunately some buggy implementations get 
	   this wrong and write an algorithm+hash algoID, to get around this the
	   decoding table contains an alternative interpretation of the
	   ALGOID_CLASS_PKCSIG information pretending to be an 
	   ALGOID_CLASS_PKC.  This broken behaviour was codified in RFC 5652
	   (section 10.1.2, "SignatureAlgorithmIdentifier") so it's now part
	   of the standard */
	status = readAlgoID( stream, &queryInfo->cryptAlgo, 
						 ALGOID_CLASS_PKC );
	if( cryptStatusOK( status ) )
		{
		status = readOctetStringHole( stream, &queryInfo->dataLength, 
									  18 + 18, DEFAULT_TAG );
		}
	if( cryptStatusOK( status ) )
		{
		queryInfo->dataStart = stell( stream ) - startPos;
		status = sSkip( stream, queryInfo->dataLength, MAX_INTLENGTH_SHORT );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the unauthenticated attributes if there are any present */
	if( stell( stream ) < endPos && \
		checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 1 ) )
		{
		status = getStreamObjectLength( stream, &length );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->unauthAttributeStart = stell( stream ) - startPos;
		queryInfo->unauthAttributeLength = length;
		status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		}

	return( cryptStatusError( status ) ? status : CRYPT_OK );
	}		/* checkStatusPeekTag() can return tag as status */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeCmsSignature( INOUT STREAM *stream,
							  IN_HANDLE const CRYPT_CONTEXT iSignContext,
							  STDC_UNUSED const CRYPT_ALGO_TYPE hashAlgo,
							  STDC_UNUSED const int hashParam,
							  STDC_UNUSED const CRYPT_ALGO_TYPE signAlgo,
							  IN_BUFFER( signatureLength ) const BYTE *signature,
							  IN_LENGTH_SHORT_MIN( 40 ) const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( signatureLength >= 40 && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	/* Write the signature algorithm identifier and signature data.  The
	   handling of CMS signatures is non-orthogonal to readCmsSignature()
	   because creating a CMS signature involves adding assorted additional
	   data like iAndS and signed attributes that present too much
	   information to pass into a basic writeSignature() call */
	writeContextAlgoID( stream, iSignContext, CRYPT_ALGO_NONE );
	return( writeOctetString( stream, signature, signatureLength, DEFAULT_TAG ) );
	}

/* Read/write cryptlib/CMS (keyID) signatures */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readCryptlibSignature( INOUT STREAM *stream, 
								  OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the header */
	readSequence( stream, NULL );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value != SIGNATURE_EX_VERSION )
		return( CRYPT_ERROR_BADDATA );

	/* Read the key ID and hash algorithm identifier */
	status = readOctetStringTag( stream, queryInfo->keyID, 
								 &queryInfo->keyIDlength, 8, 
								 CRYPT_MAX_HASHSIZE, CTAG_SI_SKI );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoIDex( stream, &queryInfo->hashAlgo, NULL,
							   &queryInfo->hashAlgoParam, 
							   ALGOID_CLASS_HASH );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the CMS/cryptlib signature algorithm and the start of the 
	   signature.  CMS separates the signature algorithm from the hash
	   algorithm so we we use ALGOID_CLASS_PKC rather than 
	   ALGOID_CLASS_PKCSIG */
	status = readAlgoID( stream, &queryInfo->cryptAlgo, ALGOID_CLASS_PKC );
	if( cryptStatusOK( status ) )
		{
		status = readOctetStringHole( stream, &queryInfo->dataLength, 
									  18 + 18, DEFAULT_TAG );
		}
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->dataStart = stell( stream ) - startPos;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, queryInfo->dataLength, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeCryptlibSignature( INOUT STREAM *stream,
								   IN_HANDLE const CRYPT_CONTEXT iSignContext,
								   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
								   STDC_UNUSED const int hashParam,
								   STDC_UNUSED const CRYPT_ALGO_TYPE signAlgo,
								   IN_BUFFER( signatureLength ) \
									const BYTE *signature,
								   IN_LENGTH_SHORT_MIN( 40 ) \
									const int signatureLength )
	{
	BYTE keyID[ 128 + 8 ];
	const int signAlgoIdSize = \
				sizeofContextAlgoID( iSignContext, CRYPT_ALGO_NONE );
	const int hashAlgoIdSize = sizeofAlgoID( hashAlgo );
	int keyIDlength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( signatureLength >= 40 && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	if( cryptStatusError( signAlgoIdSize ) )
		return( signAlgoIdSize );
	if( cryptStatusError( hashAlgoIdSize ) )
		return( hashAlgoIdSize );

	/* Get the key ID */
	status = getCmsKeyIdentifier( iSignContext, keyID, 128, &keyIDlength );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the header */
	writeSequence( stream, sizeofShortInteger( SIGNATURE_EX_VERSION ) + \
				   sizeofObject( keyIDlength ) + \
				   signAlgoIdSize + hashAlgoIdSize + \
				   sizeofObject( signatureLength ) );

	/* Write the version, key ID and algorithm identifier */
	writeShortInteger( stream, SIGNATURE_EX_VERSION, DEFAULT_TAG );
	writeOctetString( stream, keyID, keyIDlength, CTAG_SI_SKI );
	writeAlgoID( stream, hashAlgo );
	writeContextAlgoID( stream, iSignContext, CRYPT_ALGO_NONE );
	return( writeOctetString( stream, signature, signatureLength, DEFAULT_TAG ) );
	}
#endif /* USE_INT_CMS */

/****************************************************************************
*																			*
*							PGP Signature Routines							*
*																			*
****************************************************************************/

#ifdef USE_PGP

/* Read a PGP type-and-value packet and check whether it's one of ours */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readTypeAndValue( INOUT STREAM *stream, 
							 INOUT QUERY_INFO *queryInfo,
							 IN_LENGTH_Z const int startPos )
	{
	BYTE nameBuffer[ 32 + 8 ];
	static const char FAR_BSS *nameString = "issuerAndSerialNumber";
	int nameLength, valueLength, status;	/* length = 21 */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );
	REQUIRES( startPos < stell( stream ) );

	/* Skip the flags */
	status = sSkip( stream, UINT32_SIZE, UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the attribute length information and make sure that it looks 
	   valid */
	nameLength = readUint16( stream );
	status = valueLength = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( nameLength < 1 || nameLength > 255 || \
		valueLength < 1 || valueLength >= MAX_INTLENGTH_SHORT )
		{
		/* The RFC is, as usual, silent on what's a sane size for a type-
		   and-value pair, so we define our own, hopefully sensible, 
		   limits */
		return( CRYPT_ERROR_BADDATA );
		}
	if( nameLength != 21 || valueLength < 16 || valueLength > 2048 )
		{
		/* This is a somewhat different check to the above one in that out-
		   of-range (but plausible) sizes are skipped rather than being 
		   counted as an error */
		return( sSkip( stream, nameLength + valueLength, 
					   MAX_INTLENGTH_SHORT ) );
		}

	/* Read the name and check whether it's one that we recognise */
	status = sread( stream, nameBuffer, nameLength );
	if( cryptStatusError( status ) )
		return( status );
	if( !memcmp( nameBuffer, nameString, nameLength ) )
		{
		/* It's an issuerAndSerialNumber, remember it for later */
		queryInfo->iAndSStart = stell( stream ) - startPos;
		queryInfo->iAndSLength = valueLength;
		}
	return( sSkip( stream, valueLength, MAX_INTLENGTH_SHORT ) );
	}

/* Read signature subpackets.  In theory we could do something with the 
   isAuthenticated flag but at the moment we don't rely on any attributes 
   that require authentication.  The most that an attacker can do by 
   changing the keyID/iAndS field is cause the signature check to fail, 
   which they can do just as easily by flipping a bit */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readSignatureSubpackets( INOUT STREAM *stream, 
									INOUT QUERY_INFO *queryInfo,
									IN_LENGTH_SHORT const int length, 
									IN_DATALENGTH const int startPos,
									const BOOLEAN isAuthenticated )
	{
	BOOLEAN seenTimestamp = FALSE;
	const int endPos = stell( stream ) + length;
	int noSubpackets;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );
	REQUIRES( startPos < stell( stream ) );
	REQUIRES( endPos > 0 && endPos < MAX_BUFFER_SIZE );

	for( noSubpackets = 0;
		 stell( stream ) < endPos && \
			noSubpackets < FAILSAFE_ITERATIONS_MED; 
		 noSubpackets++ )
		{
		int subpacketLength, type DUMMY_INIT, status;

		/* Read the subpacket length and type */
		status = pgpReadShortLength( stream, &subpacketLength, 
									 PGP_CTB_OPENPGP );
		if( cryptStatusOK( status ) && subpacketLength < 1 )
			{
			/* We must have at least a packet-type indicator present */
			status = CRYPT_ERROR_BADDATA;
			}
		if( cryptStatusOK( status ) )
			status = type = sgetc( stream );
		if( cryptStatusError( status ) )
			return( status );

		/* If it's an unrecognised subpacket with the critical flag set,
		   reject the signature.  The range check isn't complete since there
		   are a few holes in the range, but since the holes presumably exist
		   because of deprecated subpacket types any new packets will be 
		   added at the end so it's safe to use */
		if( ( type & 0x80 ) && ( ( type & 0x7F ) > PGP_SUBPACKET_LAST ) )
			return( CRYPT_ERROR_NOTAVAIL );

		switch( type )
			{
			case PGP_SUBPACKET_TIME:
				status = sSkip( stream, UINT32_SIZE, UINT32_SIZE );

				/* Remember that we've seen the (mandatory) timestamp 
				   packet */
				seenTimestamp = TRUE;
				break;

			case PGP_SUBPACKET_KEYID:
				/* Make sure that the length is valid */
				if( subpacketLength != PGP_KEYID_SIZE + 1 )
					return( CRYPT_ERROR_BADDATA );

				/* If it's a key ID and we haven't already set this from a 
				   preceding one-pass signature packet (which can happen 
				   with detached sigs), set it now */
				if( queryInfo->keyIDlength <= 0 )
					{
					status = sread( stream, queryInfo->keyID, PGP_KEYID_SIZE );
					queryInfo->keyIDlength = PGP_KEYID_SIZE;
					}
				else
					{
					/* We've already got the ID, skip it and continue.  The 
					   -1 is for the packet type, which we've already read */
					status = sSkip( stream, subpacketLength - 1, 
									MAX_INTLENGTH_SHORT );
					}
				break;

			case PGP_SUBPACKET_TYPEANDVALUE:
				/* It's a type-and-value packet, check whether it's one of 
				   ours */
				status = readTypeAndValue( stream, queryInfo, startPos );
				break;

			default:
				/* It's something else, skip it and continue.  The -1 is for 
				   the packet type, which we've already read */
				if( subpacketLength > 1 )
					{
					status = sSkip( stream, subpacketLength - 1, 
									MAX_INTLENGTH_SHORT );
					}
			}
		if( cryptStatusError( status ) )
			return( status );
		}
	if( noSubpackets >= FAILSAFE_ITERATIONS_MED )
		{
		/* If we've found this many packets in a row all supposedly 
		   belonging to the same signature then there's something wrong */
		DEBUG_DIAG(( "Encountered more than %d subpackets for a single "
					 "signature", noSubpackets ));
		assert_nofuzz( DEBUG_WARN );
		return( CRYPT_ERROR_BADDATA );
		}

	/* Make sure that the mandatory fields are present in the subpacket 
	   data.  We also need to check for the presence of the keyID but this
	   can be in either the authenticated or unauthenticated attributes, so
	   it has to be checked by the calling function */
	if( isAuthenticated && !seenTimestamp )
		return( CRYPT_ERROR_INVALID );

	return( CRYPT_OK );
	}

/* Signature info:

	byte	ctb = PGP_PACKET_SIGNATURE_ONEPASS
	byte[]	length
	byte	version = 3 (= OpenPGP, not the expected PGP3)
	byte	sigType
	byte	hashAlgo
	byte	sigAlgo
	byte[8]	keyID
	byte	1 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readPgpOnepassSigPacket( INOUT STREAM *stream, 
							 INOUT QUERY_INFO *queryInfo )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Make sure that the packet header is in order and check the packet
	   version.  This is an OpenPGP-only packet */
	status = getPgpPacketInfo( stream, queryInfo );
	if( cryptStatusError( status ) )
		return( status );
	if( sgetc( stream ) != 3 )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->version = PGP_VERSION_OPENPGP;

	/* Skip the sig.type and get the hash algorithm and signature 
	   algorithms */
	status = sgetc( stream );	/* Skip signature type */
	if( !cryptStatusError( status ) )
		{
		status = readPgpAlgo( stream, &queryInfo->hashAlgo, 
							  &queryInfo->hashAlgoParam, 
							  PGP_ALGOCLASS_HASH );
		}
	if( cryptStatusOK( status ) )
		{
		status = readPgpAlgo( stream, &queryInfo->cryptAlgo, 
							  &queryInfo->cryptAlgoParam, 
							  PGP_ALGOCLASS_SIGN );
		}
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->type = CRYPT_OBJECT_SIGNATURE;

	/* Get the PGP key ID and make sure that this isn't a nested signature */
	status = sread( stream, queryInfo->keyID, PGP_KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->keyIDlength = PGP_KEYID_SIZE;
	return( ( sgetc( stream ) != 1 ) ? CRYPT_ERROR_BADDATA : CRYPT_OK );
	}

/* Read/write PGP signatures.

		byte	ctb = PGP_PACKET_SIGNATURE
		byte[]	length
	v3:	byte	version = PGP_2,3	v4: byte	version = PGP_VERSION_OPENPGP
		byte	infoLen = 5				byte	sigType
			byte	sigType				byte	sigAlgo
			byte[4]	sig.time			byte	hashAlgo
		byte[8]	keyID					uint16	length of auth.attributes
		byte	sigAlgo					byte[]	authenticated attributes
		byte	hashAlgo				uint16	length of unauth.attributes
		byte[2]	hash check				byte[]	unauthenticated attributes
		mpi(s)	signature							-- Contains keyID
										byte[2]	hash check
										mpi(s)	signature */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgp2SigInfo( INOUT STREAM *stream, 
							INOUT QUERY_INFO *queryInfo,
							IN_DATALENGTH_Z const int startPos )
	{
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );
	REQUIRES( startPos < stell( stream ) );

	/* Read PGP 2.x additional signature information */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != 5 )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->attributeStart = stell( stream ) - startPos;
	queryInfo->attributeLength = 5;
	status = sSkip( stream, 5, 5 );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the signer keyID and signature and hash algorithms */
	status = sread( stream, queryInfo->keyID, PGP_KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->keyIDlength = PGP_KEYID_SIZE;
	status = readPgpAlgo( stream, &queryInfo->cryptAlgo, 
						  &queryInfo->cryptAlgoParam, PGP_ALGOCLASS_SIGN );
	if( cryptStatusOK( status ) )
		{
		status = readPgpAlgo( stream, &queryInfo->hashAlgo, 
							  &queryInfo->hashAlgoParam, 
							  PGP_ALGOCLASS_HASH );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readOpenPgpSigInfo( INOUT STREAM *stream, 
							   INOUT QUERY_INFO *queryInfo,
							   IN_DATALENGTH_Z const int startPos )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );
	REQUIRES( startPos < stell( stream ) );

	/* Remember the extra data to be hashed and read the signature and hash 
	   algorithms.  Since the extra data starts at the version byte that 
	   we've already read, we add a -1 offset to the start position, as well
	   as including it in the overall length calculation */
	queryInfo->attributeStart = ( stell( stream ) - 1 ) - startPos;
	queryInfo->attributeLength = PGP_VERSION_SIZE + 1 + \
								 PGP_ALGOID_SIZE + PGP_ALGOID_SIZE;
	status = sgetc( stream );	/* Skip signature type */
	if( !cryptStatusError( status ) )
		{
		status = readPgpAlgo( stream, &queryInfo->cryptAlgo, 
							  &queryInfo->cryptAlgoParam, 
							  PGP_ALGOCLASS_SIGN );
		}
	if( cryptStatusOK( status ) )
		{
		status = readPgpAlgo( stream, &queryInfo->hashAlgo, 
							  &queryInfo->hashAlgoParam, 
							  PGP_ALGOCLASS_HASH );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Process the authenticated attributes */
	status = length = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 0 || length > 2048 )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->attributeLength += UINT16_SIZE + length;
	if( length > 0 )
		{
		status = readSignatureSubpackets( stream, queryInfo, length,
										  startPos, TRUE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Process the unauthenticated attributes */
	queryInfo->unauthAttributeStart = stell( stream ) - startPos;
	status = length = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 0 || length > 2048 )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->unauthAttributeLength = UINT16_SIZE + length;
	if( length > 0 )
		{
		status = readSignatureSubpackets( stream, queryInfo, length, 
										  startPos, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Check the the presence of required attributes.  The mandatory ones 
	   per the RFC are checked when the authenticated attributes are being
	   read, however the keyID, which is required to check the signature,
	   can be present in either the authenticated or unauthenticated
	   attributes depending on the mood of the implementer, so we have to
	   check for it outside the attribute-read code */
	if( queryInfo->keyIDlength <= 0 )
		return( CRYPT_ERROR_INVALID );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpSignature( INOUT STREAM *stream, 
							 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Make sure that the packet header is in order and check the packet
	   version.  For this packet type a version number of 3 denotes PGP 2.x
	   whereas for key transport it denotes OpenPGP */
	status = getPgpPacketInfo( stream, queryInfo );
	if( cryptStatusError( status ) )
		return( status );
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != PGP_VERSION_2 && value != PGP_VERSION_3 && \
		value != PGP_VERSION_OPENPGP )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->type = CRYPT_OBJECT_SIGNATURE;
	queryInfo->version = ( value == PGP_VERSION_OPENPGP ) ? \
						 PGP_VERSION_OPENPGP : PGP_VERSION_2;

	/* Read the signing attributes and skip the hash check */
	if( value != PGP_VERSION_OPENPGP )
		status = readPgp2SigInfo( stream, queryInfo, startPos );
	else
		status = readOpenPgpSigInfo( stream, queryInfo, startPos );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, 2, 2 );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the signature, recording the position and length of the raw RSA 
	   signature data.  We have to be careful how we handle this because 
	   readInteger16Ubits() returns the canonicalised form of the values 
	   (with leading zeroes truncated) so an stell() before the read doesn't 
	   necessarily represent the start of the payload:

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

		REQUIRES( dataStartPos >= 0 && dataStartPos < MAX_BUFFER_SIZE );
		REQUIRES( queryInfo->cryptAlgo == CRYPT_ALGO_DSA );

		/* Read the DSA signature, recording the position and combined 
		   lengths of the MPI pair.  Again, we can't use the length returned 
		   by readInteger16Ubits() to determine the overall size but have to 
		   calculate it from the position in the stream */
		status = readInteger16Ubits( stream, NULL, &dummy, 16, 20 );
		if( cryptStatusOK( status ) )
			status = readInteger16Ubits( stream, NULL, &dummy, 16, 20 );
		if( cryptStatusError( status ) )
			return( status );
		queryInfo->dataStart = dataStartPos - startPos;
		queryInfo->dataLength = stell( stream ) - dataStartPos;
		}

	/* Make sure that we've read the entire object.  This check is necessary 
	   to detect corrupted length values, which can result in reading past 
	   the end of the object */
	if( ( stell( stream ) - startPos ) != queryInfo->size )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writePgpSignature( INOUT STREAM *stream,
							  STDC_UNUSED const CRYPT_CONTEXT iSignContext,
							  STDC_UNUSED const CRYPT_ALGO_TYPE hashAlgo,
							  STDC_UNUSED const int hashParam,
							  IN_ALGO const CRYPT_ALGO_TYPE signAlgo,
							  IN_BUFFER( signatureLength ) const BYTE *signature,
							  IN_LENGTH_SHORT_MIN( 18 + 18 + 1 ) \
								const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( isPkcAlgo( signAlgo ) );
	REQUIRES( signatureLength > ( 18 + 18 ) && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	/* If it's a DLP/ECDLP algorithm then we've already specified the low-
	   level signature routines' output format as PGP so there's no need for 
	   further processing.  The handling of PGP signatures is non-orthogonal 
	   to readPgpSignature() because creating a PGP signature involves 
	   adding assorted additional data like key IDs and authenticated 
	   attributes, which present too much information to pass into a basic 
	   writeSignature() call */
	if( isDlpAlgo( signAlgo ) || isEccAlgo( signAlgo ) )
		return( swrite( stream, signature, signatureLength ) );

	/* Write the signature as a PGP MPI */
	return( writeInteger16Ubits( stream, signature, signatureLength ) );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*						Miscellaneous Signature Routines					*
*																			*
****************************************************************************/

#ifdef USE_SSH

/* Read/write SSH signatures.  SSH signature data is treated as a blob
   encoded as an SSH string rather than properly-formatted data so we don't
   encode/decode it as SSH MPIs */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readSshSignature( INOUT STREAM *stream, 
							 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	BYTE buffer[ CRYPT_MAX_TEXTSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the signature record size and algorithm information */
	readUint32( stream );
	status = readString32( stream, buffer, CRYPT_MAX_TEXTSIZE, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length == 7 )
		{
		/* If it's a string of length 7 it's a conventional signature 
		   algorithm */
		if( !memcmp( buffer, "ssh-rsa", 7 ) )
			queryInfo->cryptAlgo = CRYPT_ALGO_RSA;
		else
			{
			if( !memcmp( buffer, "ssh-dss", 7 ) )
				queryInfo->cryptAlgo = CRYPT_ALGO_DSA;
			else
				return( CRYPT_ERROR_BADDATA );
			}
		queryInfo->hashAlgo = CRYPT_ALGO_SHA1;
		}
	else
		{
		if( length == 12 )
			{
			if( memcmp( buffer, "rsa-sha2-256", 12 ) )
				return( CRYPT_ERROR_BADDATA );
			queryInfo->cryptAlgo = CRYPT_ALGO_RSA;
			queryInfo->hashAlgo = CRYPT_ALGO_SHA2;
			}
		else
			{
			/* It's probably an ECC signature algorithm.  We don't bother 
			   checking the exact type since this is implicitly specified by 
			   the signature-check key */
			if( length < 19 )		/* "ecdsa-sha2-nistXXXX" */
				return( CRYPT_ERROR_BADDATA );
			if( memcmp( buffer, "ecdsa-sha2-", 11 ) )
				return( CRYPT_ERROR_BADDATA );
			queryInfo->cryptAlgo = CRYPT_ALGO_ECDSA;
			queryInfo->hashAlgo = CRYPT_ALGO_SHA2;
			}
		}

	/* Read the start of the signature */
	status = length = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( queryInfo->cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
			if( length < MIN_PKCSIZE || length > CRYPT_MAX_PKCSIZE )
				return( CRYPT_ERROR_BADDATA );
			break;

		case CRYPT_ALGO_DSA:
			if( length != ( 20 + 20 ) )
				return( CRYPT_ERROR_BADDATA );
			break;
		
		case CRYPT_ALGO_ECDSA:
			if( length < MIN_PKCSIZE_ECCPOINT || \
				length > MAX_PKCSIZE_ECCPOINT )
				return( CRYPT_ERROR_BADDATA );
			break;

		default:
			retIntError();
		}
	queryInfo->dataStart = stell( stream ) - startPos;
	queryInfo->dataLength = length;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, length, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeSshSignature( INOUT STREAM *stream,
#ifdef USE_ECDSA
							  const CRYPT_CONTEXT iSignContext,
#else
							  STDC_UNUSED const CRYPT_CONTEXT iSignContext,
#endif /* !USE_ECDSA */
							  const CRYPT_ALGO_TYPE hashAlgo,
							  STDC_UNUSED const int hashParam,
							  IN_ALGO const CRYPT_ALGO_TYPE signAlgo,
							  IN_BUFFER( signatureLength ) const BYTE *signature,
							  IN_LENGTH_SHORT_MIN( 40 ) const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( hashAlgo == CRYPT_ALGO_SHA1 || hashAlgo == CRYPT_ALGO_SHA2 );
	REQUIRES( signAlgo == CRYPT_ALGO_RSA || signAlgo == CRYPT_ALGO_DSA || \
			  signAlgo == CRYPT_ALGO_ECDSA );
	REQUIRES( signatureLength >= ( 20 + 20 ) && \
			  signatureLength < MAX_INTLENGTH_SHORT );

#ifdef USE_ECDSA
	/* ECC signatures require all sorts of calisthenics that aren't 
	   necessary for standard signatures, specifically we have to encode the
	   curve type in the algorithm name.  See the long comment in 
	   session/ssh.c on the possible problems that the following can run 
	   into */
	if( signAlgo == CRYPT_ALGO_ECDSA )
		{
		const char *algoName;
		int keySize, algoNameLen, status;

		status = krnlSendMessage( iSignContext, IMESSAGE_GETATTRIBUTE, 
								  &keySize, CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusError( status ) )
			return( status );
		switch( keySize )
			{
			case bitsToBytes( 256 ):
				algoName = "ecdsa-sha2-nistp256";
				algoNameLen = 19;
				break;

			case bitsToBytes( 384 ):
				algoName = "ecdsa-sha2-nistp384";
				algoNameLen = 19;
				break;

			case bitsToBytes( 521 ):
				algoName = "ecdsa-sha2-nistp521";
				algoNameLen = 19;
				break;

			default:
				retIntError();
			}

		writeUint32( stream, sizeofString32( algoNameLen ) + \
							 sizeofString32( signatureLength ) );
		writeString32( stream, algoName, algoNameLen );
		return( writeString32( stream, signature, signatureLength ) );
		}
#endif /* USE_ECDSA */

	/* Write a non-ECC signature */
	if( hashAlgo == CRYPT_ALGO_SHA1 )
		{
		writeUint32( stream, sizeofString32( 7 ) + \
							 sizeofString32( signatureLength ) );
		writeString32( stream, ( signAlgo == CRYPT_ALGO_RSA ) ? \
							   "ssh-rsa" : "ssh-dss", 7 );
		}
	else
		{
		REQUIRES( signAlgo == CRYPT_ALGO_RSA && \
				  hashAlgo == CRYPT_ALGO_SHA2 );

		writeUint32( stream, sizeofString32( 12 ) + \
							 sizeofString32( signatureLength ) );
		writeString32( stream, "rsa-sha2-256", 12 );
		}
	return( writeString32( stream, signature, signatureLength ) );
	}
#endif /* USE_SSH */

#ifdef USE_SSL

/* Read/write SSL signatures.  This is just a raw signature without any
   encapsulation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readSslSignature( INOUT STREAM *stream, 
							 OUT QUERY_INFO *queryInfo )
	{
	const int startPos = stell( stream );
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the start of the signature */
	status = length = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < min( MIN_PKCSIZE, MIN_PKCSIZE_ECCPOINT ) || \
		length > CRYPT_MAX_PKCSIZE )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->dataStart = stell( stream ) - startPos;
	queryInfo->dataLength = length;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, length, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeSslSignature( INOUT STREAM *stream,
							  STDC_UNUSED const CRYPT_CONTEXT iSignContext,
							  STDC_UNUSED const CRYPT_ALGO_TYPE hashAlgo,
							  STDC_UNUSED const int hashParam,
							  STDC_UNUSED const CRYPT_ALGO_TYPE signAlgo,
							  IN_BUFFER( signatureLength ) const BYTE *signature,
							  IN_LENGTH_SHORT_MIN( 18 + 18 + 1 ) \
								const int signatureLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( signatureLength > ( 18 + 18 ) && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	writeUint16( stream, signatureLength );
	return( swrite( stream, signature, signatureLength ) );
	}

/* Read/write TLS 1.2 signatures, which specify a hash algorithm before the 
   signature and use PKCS #1 formatting instead of SSL's raw dual-hash */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readTls12Signature( INOUT STREAM *stream, 
							   OUT QUERY_INFO *queryInfo )
	{
	static const MAP_TABLE hashAlgoIDTbl[] = {
		{ /* TLS_HASHALGO_MD5 */ 1, CRYPT_ALGO_MD5 },
		{ /* TLS_HASHALGO_SHA1 */ 2, CRYPT_ALGO_SHA1 },
		{ /* TLS_HASHALGO_SHA2 */ 4, CRYPT_ALGO_SHA2 },
		{ /* TLS_HASHALGO_SHA384 */ 5, CRYPT_ALGO_SHA2 },
		{ /* TLS_HASHALGO_SHA512 */ 6, CRYPT_ALGO_SHA2 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	static const MAP_TABLE sigAlgoIDTbl[] = {
		{ /* TLS_SIGALGO_RSA */ 1, CRYPT_ALGO_RSA },
		{ /* TLS_SIGALGO_DSA */ 2, CRYPT_ALGO_DSA },
		{ /* TLS_SIGALGO_ECDSA */ 3, CRYPT_ALGO_ECDSA },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	const int startPos = stell( stream );
	int hashAlgoID, sigAlgoID, value, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the hash and signature algorithm data */
	hashAlgoID = sgetc( stream );
	status = sigAlgoID = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( hashAlgoID <= 0 || hashAlgoID >= 7 || \
		sigAlgoID <= 0 || sigAlgoID >= 4 )
		return( CRYPT_ERROR_BADDATA );
	status = mapValue( hashAlgoID, &value, hashAlgoIDTbl, 
					   FAILSAFE_ARRAYSIZE( hashAlgoIDTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->hashAlgo = value;	/* int vs.enum */
	if( isHashMacExtAlgo( value ) )
		{
		/* If it's a parameterised algorithm then we have to return extra
		   information to indicate the sub-algorithm type */
		switch( hashAlgoID )
			{
			case 4:
				queryInfo->hashAlgoParam = bitsToBytes( 256 );
				break;

#ifdef USE_SHA2_EXT
			case 5:
				queryInfo->hashAlgoParam = bitsToBytes( 384 );
				break;

			case 6:
				queryInfo->hashAlgoParam = bitsToBytes( 512 );
				break;
#endif /* USE_SHA2_EXT */

			default:
				return( CRYPT_ERROR_BADDATA );
			}
		} 
	status = mapValue( sigAlgoID, &value, sigAlgoIDTbl, 
					   FAILSAFE_ARRAYSIZE( sigAlgoIDTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		return( status ); 
	queryInfo->cryptAlgo = value;	/* int vs.enum */

	/* Read the start of the signature */
	status = length = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < min( MIN_PKCSIZE, MIN_PKCSIZE_ECCPOINT ) || \
		length > CRYPT_MAX_PKCSIZE )
		return( CRYPT_ERROR_BADDATA );
	queryInfo->dataStart = stell( stream ) - startPos;
	queryInfo->dataLength = length;

	/* Make sure that the remaining signature data is present */
	return( sSkip( stream, length, MAX_INTLENGTH_SHORT ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
static int writeTls12Signature( INOUT STREAM *stream,
								STDC_UNUSED const CRYPT_CONTEXT iSignContext,
								IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
								IN_INT_SHORT_Z const int hashParam,
								IN_ALGO const CRYPT_ALGO_TYPE signAlgo,
								IN_BUFFER( signatureLength ) const BYTE *signature,
								IN_LENGTH_SHORT_MIN( 18 + 18 + 1 ) \
									const int signatureLength )
	{
	static const MAP_TABLE hashAlgoIDTbl[] = {
		{ CRYPT_ALGO_MD5, /* TLS_HASHALGO_MD5 */ 1 },
		{ CRYPT_ALGO_SHA1, /* TLS_HASHALGO_SHA1 */ 2 },
		{ CRYPT_ALGO_SHA2, /* TLS_HASHALGO_SHA2 */ 4 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	static const MAP_TABLE sigAlgoIDTbl[] = {
		{ CRYPT_ALGO_RSA, /* TLS_SIGALGO_RSA */ 1 },
		{ CRYPT_ALGO_DSA, /* TLS_SIGALGO_DSA */ 2 },
		{ CRYPT_ALGO_ECDSA, /* TLS_SIGALGO_ECDSA */ 3 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	int hashAlgoID, sigAlgoID, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( signature, signatureLength ) );
			/* Other parameters aren't used for this format */

	REQUIRES( hashAlgo == CRYPT_ALGO_SHA1 || hashAlgo == CRYPT_ALGO_SHA2 );
	REQUIRES( hashParam >= 0 && hashParam < MAX_INTLENGTH_SHORT );
	REQUIRES( signatureLength > ( 18 + 18 ) && \
			  signatureLength < MAX_INTLENGTH_SHORT );

	/* Write the hash and siagnature algorithm data */
	status = mapValue( hashAlgo, &hashAlgoID, hashAlgoIDTbl, 
					   FAILSAFE_ARRAYSIZE( hashAlgoIDTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
#ifdef USE_SHA2_EXT
	if( hashAlgo == CRYPT_ALGO_SHA2 && hashParam == bitsToBytes( 384 ) )
		hashAlgoID = 5;	/* SHA-384 */
#endif /* USE_SHA2_EXT */
	status = mapValue( signAlgo, &sigAlgoID, sigAlgoIDTbl, 
					   FAILSAFE_ARRAYSIZE( sigAlgoIDTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	sputc( stream, hashAlgoID );
	sputc( stream, sigAlgoID );

	/* Write the signature itself */
	writeUint16( stream, signatureLength );
	return( swrite( stream, signature, signatureLength ) );
	}
#endif /* USE_SSL */

/****************************************************************************
*																			*
*					Signature Read/Write Access Functions					*
*																			*
****************************************************************************/

typedef struct {
	const SIGNATURE_TYPE type;
	const READSIG_FUNCTION function;
	} SIG_READ_INFO;
static const SIG_READ_INFO sigReadTable[] = {
#ifdef USE_INT_ASN1
	{ SIGNATURE_RAW, readRawSignature },
	{ SIGNATURE_X509, readX509Signature },
#endif /* USE_INT_ASN1 */
#ifdef USE_INT_CMS 
	{ SIGNATURE_CMS, readCmsSignature },
	{ SIGNATURE_CRYPTLIB, readCryptlibSignature },
#endif /* USE_INT_CMS */
#ifdef USE_PGP
	{ SIGNATURE_PGP, readPgpSignature },
#endif /* USE_PGP */
#ifdef USE_SSH
	{ SIGNATURE_SSH, readSshSignature },
#endif /* USE_SSH */
#ifdef USE_SSL
	{ SIGNATURE_SSL, readSslSignature },
	{ SIGNATURE_TLS12, readTls12Signature },
#endif /* USE_SSL */
	{ SIGNATURE_NONE, NULL }, { SIGNATURE_NONE, NULL }
	};

typedef struct {
	const SIGNATURE_TYPE type;
	const WRITESIG_FUNCTION function;
	} SIG_WRITE_INFO;
static const SIG_WRITE_INFO sigWriteTable[] = {
#ifdef USE_INT_ASN1
	{ SIGNATURE_RAW, writeRawSignature },
	{ SIGNATURE_X509, writeX509Signature },
#endif /* USE_INT_ASN1 */
#ifdef USE_INT_CMS 
	{ SIGNATURE_CMS, writeCmsSignature },
	{ SIGNATURE_CRYPTLIB, writeCryptlibSignature },
#endif /* USE_INT_CMS */
#ifdef USE_PGP
	{ SIGNATURE_PGP, writePgpSignature },
#endif /* USE_PGP */
#ifdef USE_SSH
	{ SIGNATURE_SSH, writeSshSignature },
#endif /* USE_SSH */
#ifdef USE_SSL
	{ SIGNATURE_SSL, writeSslSignature },
	{ SIGNATURE_TLS12, writeTls12Signature },
#endif /* USE_SSH */
	{ SIGNATURE_NONE, NULL }, { SIGNATURE_NONE, NULL }
	};

CHECK_RETVAL_PTR \
READSIG_FUNCTION getReadSigFunction( IN_ENUM( SIGNATURE ) \
										const SIGNATURE_TYPE sigType )
	{
	int i;

	REQUIRES_N( sigType > SIGNATURE_NONE && sigType < SIGNATURE_LAST );

	for( i = 0; 
		 sigReadTable[ i ].type != SIGNATURE_NONE && \
			i < FAILSAFE_ARRAYSIZE( sigReadTable, SIG_READ_INFO ); 
		 i++ )
		{
		if( sigReadTable[ i ].type == sigType )
			return( sigReadTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( sigReadTable, SIG_READ_INFO ) );

	return( NULL );
	}
CHECK_RETVAL_PTR \
WRITESIG_FUNCTION getWriteSigFunction( IN_ENUM( SIGNATURE ) \
										const SIGNATURE_TYPE sigType )
	{
	int i;

	REQUIRES_N( sigType > SIGNATURE_NONE && sigType < SIGNATURE_LAST );

	for( i = 0; 
		 sigWriteTable[ i ].type != SIGNATURE_NONE && \
			i < FAILSAFE_ARRAYSIZE( sigWriteTable, SIG_WRITE_INFO ); 
		 i++ )
		{
		if( sigWriteTable[ i ].type == sigType )
			return( sigWriteTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( sigWriteTable, SIG_WRITE_INFO ) );

	return( NULL );
	}
