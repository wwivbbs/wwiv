/****************************************************************************
*																			*
*						ASN.1 Algorithm Identifier Routines					*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Object/Algorithm Identifier Routines				*
*																			*
****************************************************************************/

/* Pull in the AlgorithmIdentifier OID table */

#if defined( INC_ALL )
  #include "asn1_oids.h"
#else
  #include "enc_dec/asn1_oids.h"
#endif /* Compiler-specific includes */

/* Map an OID to an algorithm type */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5, 6 ) ) \
static int oidToAlgorithm( IN_BUFFER( oidLength ) const BYTE *oid, 
						   IN_RANGE( 1, MAX_OID_SIZE ) const int oidLength, 
						   IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type,
						   OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
						   OUT_INT_Z int *param1, 
						   OUT_INT_Z int *param2 )
	{
	BYTE oidByte;
	int i;

	assert( isReadPtr( oid, oidLength ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( param1, sizeof( int ) ) );
	assert( isWritePtr( param2, sizeof( int ) ) );

	REQUIRES( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );
	REQUIRES( type > ALGOID_CLASS_NONE && type < ALGOID_CLASS_LAST );

	/* Clear return values */
	*cryptAlgo = CRYPT_ALGO_NONE;
	*param1 = *param2 = 0;

	/* If the OID is shorter than the minimum possible algorithm OID value, 
	   don't try and process it */
	if( oidLength < 7 )
		return( CRYPT_ERROR_BADDATA );
	oidByte = oid[ 6 ];

	/* Look for a matching OID.  For quick-reject matching we check the byte
	   furthest inside the OID that's likely to not match (large groups of 
	   OIDs have common prefixes due to being in the same arc), this rejects 
	   the majority of mismatches without requiring a full comparison */
	for( i = 0; algoIDinfoTbl[ i ].algorithm != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ); i++ )
		{
		const ALGOID_INFO *algoIDinfoPtr = &algoIDinfoTbl[ i ];

		if( algoIDinfoPtr->algoClass == type && \
			sizeofOID( algoIDinfoPtr->oid ) == oidLength && \
			algoIDinfoPtr->oid[ 6 ] == oidByte && \
			!memcmp( algoIDinfoPtr->oid, oid, oidLength ) )
			{
			*cryptAlgo = algoIDinfoPtr->algorithm;
			*param1 = algoIDinfoPtr->param1;
			*param2 = algoIDinfoPtr->param2;

			return( CRYPT_OK );
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) );

	/* No algorithm for this OID found */
	return( CRYPT_ERROR_NOTAVAIL );
	}

/* Map an algorithm and optional parameters (sub-algorithm/mode/block size) 
   and other parameters to an OID.  This can be called either to check 
   whether an algorithm is encodable (checkValid = FALSE) or as part of an 
   actual encoding, throwing an exception if the parameters can't be encoded 
   (checkValid = TRUE) */

#define ALGOTOOID_REQUIRE_VALID		TRUE
#define ALGOTOOID_CHECK_VALID		FALSE

CHECK_RETVAL_PTR \
static const BYTE *algorithmToOID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
								   IN_RANGE( 0, 999 ) const int param1,
								   IN_RANGE( 0, 999 ) const int param2,
								   const BOOLEAN checkValid )
	{
	const BYTE *oid = NULL;
	int i;

	REQUIRES_N( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES_N( ( param1 == 0 && param2 == 0 ) || \
				( ( param1 > 0 && param1 <= 999 ) && param2 == 0 ) || \
				( ( param1 > 0 && param1 <= 999 ) && \
				  ( param2 > 0 && param2 <= 999 ) ) );

	for( i = 0; algoIDinfoTbl[ i ].algorithm != CRYPT_ALGO_NONE && \
				i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ); i++ )
		{
		if( algoIDinfoTbl[ i ].algorithm == cryptAlgo )
			{
			oid = algoIDinfoTbl[ i ].oid;
			break;
			}
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) );
	if( param1 != 0 )
		{
		oid = NULL;
		while( algoIDinfoTbl[ i ].algorithm == cryptAlgo && \
			   i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) )
			{
			if( algoIDinfoTbl[ i ].param1 == param1 )
				{
				oid = algoIDinfoTbl[ i ].oid;
				break;
				}
			i++;
			}
		ENSURES_N( i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) );
		}
	if( param2 != 0 )
		{
		oid = NULL;
		while( algoIDinfoTbl[ i ].algorithm == cryptAlgo && \
			   algoIDinfoTbl[ i ].param1 == param1 && \
			   i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) )
			{
			if( algoIDinfoTbl[ i ].param2 == param2 )
				{
				oid = algoIDinfoTbl[ i ].oid;
				break;
				}
			i++;
			}
		ENSURES_N( i < FAILSAFE_ARRAYSIZE( algoIDinfoTbl, ALGOID_INFO ) );
		}
	if( oid != NULL )
		return( oid );
	if( !checkValid )
		return( NULL );
	retIntError_Null();
	}

/* Read the start of an AlgorithmIdentifier record, used by a number of
   routines.  The parameters are as follows:
   
	Encryption: Mode for algorithm
	PKC sig: Hash algorithm used with signature algorithm + optional hash 
			 width for variable-width hashes.
	Hash: Optional hash width for variable-width hashes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readAlgoIDheader( INOUT STREAM *stream, 
							 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
							 OUT_OPT_RANGE( 0, 999 ) int *param1, 
							 OUT_OPT_RANGE( 0, 999 ) int *param2, 
							 OUT_OPT_LENGTH_SHORT_Z int *extraLength, 
							 IN_TAG const int tag,
							 IN_ENUM( ALGOID_CLASS ) \
									const ALGOID_CLASS_TYPE type )
	{
	CRYPT_ALGO_TYPE localCryptAlgo;
	BYTE oidBuffer[ MAX_OID_SIZE + 8 ];
	int oidLength, algoParam1, algoParam2, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( param1 == NULL || isWritePtr( param1, sizeof( int ) ) );
	assert( param2 == NULL || isWritePtr( param2, sizeof( int ) ) );
	assert( extraLength == NULL || \
			isWritePtr( extraLength, sizeof( int ) ) );

	REQUIRES_S( ( param1 == NULL && param2 == NULL ) || \
				( param1 != NULL && param2 != NULL ) );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );
	REQUIRES_S( type > ALGOID_CLASS_NONE && type < ALGOID_CLASS_LAST );
	
	/* Clear the return values */
	*cryptAlgo = CRYPT_ALGO_NONE;
	if( param1 != NULL )
		*param1 = *param2 = 0;
	if( extraLength != NULL )
		*extraLength = 0;

	/* Determine the algorithm information based on the AlgorithmIdentifier
	   field */
	if( tag == DEFAULT_TAG )
		readSequence( stream, &length );
	else
		readConstructed( stream, &length, tag );
	status = readEncodedOID( stream, oidBuffer, MAX_OID_SIZE, &oidLength, 
							 BER_OBJECT_IDENTIFIER );
	if( cryptStatusError( status ) )
		return( status );
	length -= oidLength;
	if( oidLength != sizeofOID( oidBuffer ) || \
		length < 0 || length >= MAX_INTLENGTH_SHORT )
		{
		/* It's a stream-related error, make it persistent */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	status = oidToAlgorithm( oidBuffer, oidLength, type, &localCryptAlgo, 
							 &algoParam1, &algoParam2 );
	if( cryptStatusError( status ) )
		return( status );
	*cryptAlgo = localCryptAlgo;
	if( param1 != NULL )
		{
		*param1 = algoParam1;
		*param2 = algoParam2;
		}

	/* If the caller has specified that there should be no parameters 
	   present, make sure that there's either no data or an ASN.1 NULL
	   present and nothing else */
	if( extraLength == NULL )
		return( ( length > 0 ) ? readNull( stream ) : CRYPT_OK );

	/* If the parameters are null parameters, check them and exit */
	if( length == sizeofNull() )
		return( readNull( stream ) );

	/* Handle any remaining parameters */
	*extraLength = ( int ) length;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					EncryptionAlgorithmIdentifier Routines					*
*																			*
****************************************************************************/

/* EncryptionAlgorithmIdentifier parameters:

	aesXcbc, aesXofb: AES FIPS

		iv				OCTET STRING SIZE (16)

	aesXcfb: AES FIPS

		SEQUENCE {
			iv			OCTET STRING SIZE (16),
			noOfBits	INTEGER (128)
			}

	blowfishCBC, desCBC, desEDE3-CBC: Blowfish RFC/OIW
		iv				OCTET STRING SIZE (8)

	blowfishCFB, blowfishOFB, desCFB, desOFB: Blowfish RFC/OIW
		SEQUENCE {
			iv			OCTET STRING SIZE (8),
			noBits		INTEGER (64)
			}

	rc2CBC: RFC 2311
		SEQUENCE {
			rc2Param	INTEGER (58),	-- 128 bit key
			iv			OCTET STRING SIZE (8)
			}

	rc4: (Unsure where this one is from)
		NULL

	rc5: RFC 2040
		SEQUENCE {
			version		INTEGER (16),
			rounds		INTEGER (12),
			blockSize	INTEGER (64),
			iv			OCTET STRING OPTIONAL
			}

   Because of the somewhat haphazard nature of encryption
   AlgorithmIdentifier definitions we can only handle the following
   algorithm/mode combinations:

	AES ECB, CBC, CFB, OFB
	Blowfish ECB, CBC, CFB, OFB
	DES ECB, CBC, CFB, OFB
	3DES ECB, CBC, CFB, OFB
	RC2 ECB, CBC
	RC4
	RC5 CBC

   In addition to the standard AlgorithmIdentifiers there's also a generic-
   secret pseudo-algorithm used for key-diversification purposes:

	authEnc128/authEnc256: RFC 6476
		SEQUENCE {
			prf			AlgorithmIdentifier DEFAULT PBKDF2,
			encAlgo		AlgorithmIdentifier,
			macAlgo		AlgorithmIdentifier */

/* Magic value to denote 128-bit RC2 keys */

#define RC2_KEYSIZE_MAGIC		58

/* Read an EncryptionAlgorithmIdentifier/DigestAlgorithmIdentifier */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readAuthEncParamData( INOUT STREAM *stream,
								 OUT_LENGTH_Z int *offset,
								 OUT_LENGTH_SHORT_Z int *length,
								 IN_LENGTH_SHORT const int maxLength )
	{
	const int paramStart = stell( stream );
	int paramLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( offset, sizeof( int ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( maxLength > 0 && maxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( !cryptStatusError( paramStart ) );

	/* Clear return values */
	*offset = *length = 0;

	/* Get the start and length of the parameter data */
	status = readUniversal( stream );
	if( cryptStatusError( status ) )
		return( status );
	paramLength = stell( stream ) - paramStart;

	/* Make sure that it appears valid */
	if( paramLength < 8 || paramLength > maxLength )
		return( CRYPT_ERROR_BADDATA );

	*offset = paramStart;
	*length = paramLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readAlgoIDInfo( INOUT STREAM *stream, 
						   INOUT QUERY_INFO *queryInfo,
						   IN_TAG const int tag,
						   IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type )
	{
	const int offset = stell( stream );
	int param1, param2, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );
	REQUIRES_S( type > ALGOID_CLASS_NONE && type < ALGOID_CLASS_LAST );

	/* Read the AlgorithmIdentifier header and OID */
	status = readAlgoIDheader( stream, &queryInfo->cryptAlgo, &param1, 
							   &param2, &length, tag, type );
	if( cryptStatusError( status ) )
		return( status );
	if( isConvAlgo( queryInfo->cryptAlgo ) )
		{
		/* For conventional algorithms, the parameter is the encryption mode
		   and the optional second parameter is the key size */
		queryInfo->cryptMode = param1;
		if( param2 != 0 )
			queryInfo->keySize = param2;
		}
	else
		{
		if( isHashAlgo( queryInfo->cryptAlgo ) )
			{
			/* For hash algorithms, the optional parameter is the hash 
			   width */
			if( param1 != 0 )
				queryInfo->hashAlgoParam = param1;
			}
		}

	/* Some broken implementations use sign + hash algoIDs in places where
	   a hash algoID is called for, if we find one of these we modify the
	   read AlgorithmIdentifier information to make it look like a hash
	   algoID */
	if( isPkcAlgo( queryInfo->cryptAlgo ) && isHashAlgo( param1 ) )
		queryInfo->cryptAlgo = param1;	/* Turn pkcWithHash into hash */

	/* Hash algorithms will either have NULL parameters or none at all
	   depending on which interpretation of which standard the sender used
	   so if it's not a conventional encryption algorithm we process the
	   NULL if required and return */
	if( isHashAlgo( queryInfo->cryptAlgo ) || \
		isMacAlgo( queryInfo->cryptAlgo ) )
		return( ( length > 0 ) ? readNull( stream ) : CRYPT_OK );

	/* If it's not a hash/MAC algorithm it has to be a conventional
	   encryption (or at least authenticated-encryption, handled via a
	   generic-secret context) algorithm */
	if( !isConvAlgo( queryInfo->cryptAlgo ) && \
		!isSpecialAlgo( queryInfo->cryptAlgo ) )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Read the algorithm-specific parameters.  In theory we should do
	   something with some of the values like the IV size parameter, but
	   since the standard never explains what to do if it's something other
	   than the algorithm block size (Left pad? Right pad? Sign-extend?
	   Repeat the data?) it's safer not to do anything ("Never check for an
	   error that you don't know how to handle").  In any case there are no
	   known cases of these strange values ever being used (probably because
	   all existing software would break) so we make sure that they're 
	   present but otherwise ignore them */
	switch( queryInfo->cryptAlgo )
		{
		case CRYPT_ALGO_3DES:
		case CRYPT_ALGO_AES:
		case CRYPT_ALGO_BLOWFISH:
		case CRYPT_ALGO_DES:
			if( queryInfo->cryptMode == CRYPT_MODE_ECB )
				{
				/* The NULL parameter has already been read in
				   readAlgoIDheader() */
				return( CRYPT_OK );
				}
			if( ( queryInfo->cryptMode == CRYPT_MODE_CBC ) || \
				( queryInfo->cryptAlgo == CRYPT_ALGO_AES && \
				  queryInfo->cryptMode == CRYPT_MODE_OFB ) )
				{
				return( readOctetString( stream, queryInfo->iv,
								&queryInfo->ivLength,
								( queryInfo->cryptAlgo == CRYPT_ALGO_AES ) ? \
									16 : 8, CRYPT_MAX_IVSIZE ) );
				}
			readSequence( stream, NULL );
			readOctetString( stream, queryInfo->iv, &queryInfo->ivLength,
							 8, CRYPT_MAX_IVSIZE );
			return( readShortInteger( stream, NULL ) );

#ifdef USE_RC2
		case CRYPT_ALGO_RC2:
			/* In theory we should check that the parameter value ==
			   RC2_KEYSIZE_MAGIC (corresponding to a 128-bit key) but in
			   practice this doesn't really matter, we just use whatever we
			   find inside the PKCS #1 padding */
			readSequence( stream, NULL );
			if( queryInfo->cryptMode != CRYPT_MODE_CBC )
				return( readShortInteger( stream, NULL ) );
			readShortInteger( stream, NULL );
			return( readOctetString( stream, queryInfo->iv,
									 &queryInfo->ivLength,
									 8, CRYPT_MAX_IVSIZE ) );
#endif /* USE_RC2 */

#ifdef USE_RC4
		case CRYPT_ALGO_RC4:
			/* The NULL parameter has already been read in
			   readAlgoIDheader() */
			return( CRYPT_OK );
#endif /* USE_RC4 */

#ifdef USE_RC5
		case CRYPT_ALGO_RC5:
			{
			long val1, val2, val3;

			readSequence( stream, NULL );
			readShortInteger( stream, &val1 );			/* Version */
			readShortInteger( stream, &val2 );			/* Rounds */
			status = readShortInteger( stream, &val3 );	/* Block size */
			if( cryptStatusError( status ) )
				return( status );
			if( val1 != 16 || val2 != 12 || val3 != 64 )
				{
				/* This algorithm makes enough of a feature of its variable
				   parameters that we do actually check to make sure that
				   they're sensible since it may just be possible that 
				   someone playing with an implementation decides to use
				   weird values */
				return( CRYPT_ERROR_NOTAVAIL );
				}
			return( readOctetString( stream, queryInfo->iv,
									 &queryInfo->ivLength,
									 8, CRYPT_MAX_IVSIZE ) );
			}
#endif /* USE_RC5 */

		case CRYPT_IALGO_GENERIC_SECRET:
			{
			int maxLength = 128 - 8;	/* -8 for outer wrapper + OID */

			/* For AuthEnc data we need to MAC the encoded parameter data 
			   after we've processed it, so we save a copy for the caller.  
			   In addition the caller needs a copy of the encryption and MAC
			   parameters to use when creating the encryption and MAC
			   contexts, so we record the position within the encoded 
			   parameter data.  First we tunnel down into the parmaeter
			   data to find the locations of the encryption and MAC
			   parameters */
			status = readSequence( stream, NULL );
			if( cryptStatusOK( status ) )
				{
				/* Encryption algorithm parameters */
				status = readAuthEncParamData( stream,
									&queryInfo->encParamStart, 
									&queryInfo->encParamLength, 
									maxLength - 8 );/* -8 for MAC param.*/
				}
			if( cryptStatusOK( status ) )
				{
				/* MAC algorithm parameters */
				status = readAuthEncParamData( stream,
									&queryInfo->macParamStart, 
									&queryInfo->macParamLength,
									maxLength - queryInfo->encParamLength );
				}
			if( cryptStatusError( status ) )
				return( status );

			/* The encryption/MAC parameter positions are taken from the 
			   start of the encoded data, not from the start of the 
			   stream */
			queryInfo->encParamStart -= offset;
			queryInfo->macParamStart -= offset;

			/* Then we save the overall encoded parameter data */
			length = stell( stream ) - offset;
			if( length <= 0 || length > 128 )
				return( CRYPT_ERROR_OVERFLOW );
			status = sseek( stream, offset );
			if( cryptStatusOK( status ) )
				status = sread( stream, queryInfo->authEncParamData,
								length );
			if( cryptStatusOK( status ) )
				queryInfo->authEncParamLength = length;
			return( status );
			}
		}

	retIntError();
	}

/* Get the size of an EncryptionAlgorithmIdentifier record */

CHECK_RETVAL_LENGTH \
int sizeofCryptContextAlgoID( IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	STREAM nullStream;
	int status;

	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Determine how large the algoID and associated parameters are.  
	   Because this is a rather complex operation the easiest way to do it 
	   is to write to a null stream and get its size */
	sMemNullOpen( &nullStream );
	status = writeCryptContextAlgoID( &nullStream, iCryptContext );
	if( cryptStatusOK( status ) )
		status = stell( &nullStream );
	sMemClose( &nullStream );
	return( status );
	}

/* Write an EncryptionAlgorithmIdentifier record */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeCryptContextAlgoID( INOUT STREAM *stream,
							 IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	const BYTE *oid;
	BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];
	int algorithm, mode = CRYPT_MODE_NONE;	/* enum vs.int */
	int algoParam = 0, oidSize, ivSize = 0, sizeofIV = 0, paramSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( isHandleRangeValid( iCryptContext ) );

	/* Extract the information that we need to write the
	   AlgorithmIdentifier */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) && algorithm != CRYPT_IALGO_GENERIC_SECRET )
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) && !isStreamCipher( algorithm ) && \
		needsIV( mode ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, iv, CRYPT_MAX_IVSIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CTXINFO_IV );
		if( cryptStatusOK( status ) )
			{
			ivSize = msgData.length;
			sizeofIV = ( int ) sizeofObject( ivSize );
			}
		}
	if( cryptStatusOK( status ) && isParameterisedConvAlgo( algorithm ) )
		{
		/* Some algorithms are parameterised, so we have to extract 
		   additional information to deal with them */
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &algoParam, CRYPT_CTXINFO_KEYSIZE );
		}
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Couldn't extract information needed to write "
					 "AlgoID" ));
		assert( DEBUG_WARN );
		return( status );
		}

	ENSURES_S( isConvAlgo( algorithm ) || \
			   algorithm == CRYPT_IALGO_GENERIC_SECRET );

	/* Get the OID for this algorithm */
	if( ( oid = algorithmToOID( algorithm, mode, algoParam, \
								ALGOTOOID_CHECK_VALID ) ) == NULL )
		{
		/* Some algorithm+mode combinations can't be encoded using the
		   available PKCS #7 OIDs, the best that we can do in this case is
		   alert the user in debug mode and return a CRYPT_ERROR_NOTAVAIL */
		DEBUG_DIAG(( "Tried to write non-PKCS #7 algorithm ID" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	oidSize = sizeofOID( oid );
	ENSURES_S( oidSize >= MIN_OID_SIZE && oidSize <= MAX_OID_SIZE );

	/* Write the algorithm-specific parameters */
	switch( algorithm )
		{
		case CRYPT_ALGO_3DES:
		case CRYPT_ALGO_AES:
		case CRYPT_ALGO_BLOWFISH:
		case CRYPT_ALGO_DES:
			{
			const int noBits = ( algorithm == CRYPT_ALGO_AES ) ? 128 : 64;

			paramSize = \
				( mode == CRYPT_MODE_ECB ) ? \
					sizeofNull() : \
				( ( mode == CRYPT_MODE_CBC ) || \
				  ( algorithm == CRYPT_ALGO_AES && mode == CRYPT_MODE_OFB ) ) ? \
				  sizeofIV : \
				  ( int ) sizeofObject( sizeofIV + sizeofShortInteger( noBits ) );
			writeSequence( stream, oidSize + paramSize );
			swrite( stream, oid, oidSize );
			if( mode == CRYPT_MODE_ECB )
				return( writeNull( stream, DEFAULT_TAG ) );
			if( ( mode == CRYPT_MODE_CBC ) || \
				( algorithm == CRYPT_ALGO_AES && mode == CRYPT_MODE_OFB ) )
				return( writeOctetString( stream, iv, ivSize, DEFAULT_TAG ) );
			writeSequence( stream, sizeofIV + sizeofShortInteger( noBits ) );
			writeOctetString( stream, iv, ivSize, DEFAULT_TAG );
			return( writeShortInteger( stream, noBits, DEFAULT_TAG ) );
			}

#ifdef USE_RC2
		case CRYPT_ALGO_RC2:
			paramSize = ( ( mode == CRYPT_MODE_ECB ) ? 0 : sizeofIV ) + \
						sizeofShortInteger( RC2_KEYSIZE_MAGIC );
			writeSequence( stream, oidSize + \
								   ( int ) sizeofObject( paramSize ) );
			swrite( stream, oid, oidSize );
			writeSequence( stream, paramSize );
			if( mode != CRYPT_MODE_CBC )
				{
				return( writeShortInteger( stream, RC2_KEYSIZE_MAGIC,
										   DEFAULT_TAG ) );
				}
			writeShortInteger( stream, RC2_KEYSIZE_MAGIC, DEFAULT_TAG );
			return( writeOctetString( stream, iv, ivSize, DEFAULT_TAG ) );
#endif /* USE_RC2 */

#ifdef USE_RC4
		case CRYPT_ALGO_RC4:
			writeSequence( stream, oidSize + sizeofNull() );
			swrite( stream, oid, oidSize );
			return( writeNull( stream, DEFAULT_TAG ) );
#endif /* USE_RC4 */

#ifdef USE_RC5
		case CRYPT_ALGO_RC5:
			paramSize = sizeofShortInteger( 16 ) + \
						sizeofShortInteger( 12 ) + \
						sizeofShortInteger( 64 ) + \
						sizeofIV;
			writeSequence( stream, oidSize + \
								   ( int ) sizeofObject( paramSize ) );
			swrite( stream, oid, oidSize );
			writeSequence( stream, paramSize );
			writeShortInteger( stream, 16, DEFAULT_TAG );	/* Version */
			writeShortInteger( stream, 12, DEFAULT_TAG );	/* Rounds */
			writeShortInteger( stream, 64, DEFAULT_TAG );	/* Block size */
			return( writeOctetString( stream, iv, ivSize, DEFAULT_TAG ) );
#endif /* USE_RC5 */

		case CRYPT_IALGO_GENERIC_SECRET:
			{
			MESSAGE_DATA msgData;
			BYTE encAlgoData[ CRYPT_MAX_TEXTSIZE + 8 ];
			BYTE macAlgoData[ CRYPT_MAX_TEXTSIZE + 8 ];
			int encAlgoDataSize, macAlgoDataSize;

			/* Get the encoded parameters for the encryption and MAC 
			   contexts that will be derived from the generic-secret 
			   context */
			setMessageData( &msgData, encAlgoData, CRYPT_MAX_TEXTSIZE );
			status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_ENCPARAMS );
			if( cryptStatusError( status ) )
				return( status );
			encAlgoDataSize = msgData.length;
			setMessageData( &msgData, macAlgoData, CRYPT_MAX_TEXTSIZE );
			status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_MACPARAMS );
			if( cryptStatusError( status ) )
				return( status );
			macAlgoDataSize = msgData.length;

			/* Write the pre-encoded AuthEnc parameter data */
			writeSequence( stream, oidSize + \
						   sizeofObject( encAlgoDataSize + macAlgoDataSize ) );
			swrite( stream, oid, oidSize );
			writeSequence( stream, encAlgoDataSize + macAlgoDataSize );
			swrite( stream, encAlgoData, encAlgoDataSize );
			return( swrite( stream, macAlgoData, macAlgoDataSize ) );
			}
		}

	retIntError();
	}

/****************************************************************************
*																			*
*							AlgorithmIdentifier Routines					*
*																			*
****************************************************************************/

/* Because AlgorithmIdentifiers are only defined for a subset of the
   algorithms that cryptlib supports we have to check that the algorithm
   and mode being used can be represented in encoded data before we try to
   do anything with it */

CHECK_RETVAL_BOOL \
BOOLEAN checkAlgoID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					 IN_MODE const CRYPT_MODE_TYPE cryptMode )
	{
	REQUIRES_B( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES_B( cryptMode >= CRYPT_MODE_NONE && cryptMode < CRYPT_MODE_LAST );

	return( ( algorithmToOID( cryptAlgo, cryptMode, 0, \
							  ALGOTOOID_CHECK_VALID ) != NULL ) ? \
			TRUE : FALSE );
	}

/* Determine the size of an AlgorithmIdentifier record.  For algorithms with
   sub-parameters (AES, SHA-2) the OIDs are the same size so there's no need
   to explicitly deal with them */

CHECK_RETVAL_LENGTH \
int sizeofAlgoIDex( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					IN_RANGE( 0, 999 ) const int parameter, 
					IN_LENGTH_SHORT_Z const int extraLength )
	{
	const BYTE *oid = algorithmToOID( cryptAlgo, parameter, 0, \
									  ALGOTOOID_REQUIRE_VALID );

	REQUIRES( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES( parameter >= 0 && parameter <= 999 );
	REQUIRES( extraLength >= 0 && extraLength < MAX_INTLENGTH_SHORT );
	REQUIRES( oid != NULL );

	return( ( int ) sizeofObject( sizeofOID( oid ) + \
								  ( ( extraLength > 0 ) ? extraLength : \
														  sizeofNull() ) ) );
	}

CHECK_RETVAL_LENGTH \
int sizeofAlgoID( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	REQUIRES( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );

	return( sizeofAlgoIDex( cryptAlgo, CRYPT_ALGO_NONE, 0 ) );
	}

/* Write an AlgorithmIdentifier record.  There are three versions of 
   this:

	writeAlgoID: Write an AlgorithmIdentifier record.

	writeAlgoIDext: Write an AlgorithmIdentifier record.  The parameter 
		value is used for aWithB algorithms like rsaWithSHA1, with the 
		context containing the 'A' algorithm and the parameter indicating 
		the 'B' algorithm, and for algorithms that have subtypes like 
		SHA2's SHA2-256, SHA2-384, and SHA2-512 

	writeAlgoIDparams: Write an AlgorithmIdentifier record, leaving extra
		space for algorithm parameters at the end */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoID( INOUT STREAM *stream, 
				 IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );

	return( writeAlgoIDex( stream, cryptAlgo, CRYPT_ALGO_NONE, 0 ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoIDex( INOUT STREAM *stream, 
				   IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
				   IN_RANGE( 0, 999 ) const int parameter, 
				   IN_LENGTH_SHORT_Z const int extraLength )
	{
	const BYTE *oid = algorithmToOID( cryptAlgo, parameter, 0, \
									  ALGOTOOID_REQUIRE_VALID );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES_S( parameter == CRYPT_ALGO_NONE || \
				( parameter >= CRYPT_ALGO_FIRST_HASH && \
				  parameter <= CRYPT_ALGO_LAST_HASH ) || \
				( isHashExtAlgo( cryptAlgo ) && \
				  parameter >= 32 && parameter <= CRYPT_MAX_HASHSIZE ) );
	REQUIRES_S( extraLength >= 0 && extraLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( oid != NULL );

	/* Write the AlgorithmIdentifier field */
	writeSequence( stream, sizeofOID( oid ) + \
				   ( ( extraLength > 0 ) ? extraLength : sizeofNull() ) );
	status = swrite( stream, oid, sizeofOID( oid ) );
	if( extraLength > 0 )
		{
		/* Parameters will be written by the caller */
		return( status );
		}

	/* No extra parameters so we need to write a NULL */
	return( writeNull( stream, DEFAULT_TAG ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeAlgoIDparam( INOUT STREAM *stream, 
					  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo,
					  IN_LENGTH_SHORT_Z const int extraLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( cryptAlgo > CRYPT_ALGO_NONE && cryptAlgo < CRYPT_ALGO_LAST );
	REQUIRES_S( extraLength >= 0 && extraLength < MAX_INTLENGTH_SHORT );

	return( writeAlgoIDex( stream, cryptAlgo, CRYPT_ALGO_NONE, extraLength ) );
	}

/* Read an AlgorithmIdentifier record.  There are three versions of 
   this:

	readAlgoID: Reads an algorithm, assumes that there are no secondary 
		algorithm or mode and algorithm parameters present and returns an 
		error if there are.

	readAlgoIDex: Reads an algorithm, secondary algorithm or mode, 
		and optional algorithm parameter (e.g. SHA-2 subtype when the 
		algorithm is SHA-2).  Assumes that there are no explicit 
		algorithm parameters present and returns an error if there are.

	readAlgoIDparams: Reads an algorithm and the length of the extra 
		information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readAlgoID( INOUT STREAM *stream, 
				OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
				IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES_S( type == ALGOID_CLASS_HASH || type == ALGOID_CLASS_PKC || \
				type == ALGOID_CLASS_PKCSIG );

	return( readAlgoIDheader( stream, cryptAlgo, NULL, NULL, NULL, 
							  DEFAULT_TAG, type ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readAlgoIDex( INOUT STREAM *stream, 
				  OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
				  OUT_OPT_ALGO_Z CRYPT_ALGO_TYPE *altCryptAlgo,
				  OUT_INT_Z int *parameter,
				  IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( altCryptAlgo == NULL || \
			isWritePtr( altCryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( parameter, sizeof( int ) ) );

	REQUIRES_S( ( type == ALGOID_CLASS_PKCSIG && altCryptAlgo != NULL ) || \
				( type == ALGOID_CLASS_HASH && altCryptAlgo == NULL ) );

	/* Clear return value (the others are cleared by readAlgoIDheader()) */
	if( altCryptAlgo != NULL )
		*altCryptAlgo = CRYPT_ALGO_NONE;
	*parameter = 0;

	/* If we're reading a signature algorithm then there's a secondary 
	   algorithm (e.g. RSA with SHA-1) and an optional parameter (e.g. 
	   SHA2-512), otherwise it's a hash algorithm and there's an optional
	   parameter (e.g. SHA2-512) */
	if( type == ALGOID_CLASS_PKCSIG )
		{
		int altAlgo;	/* 'altAlgo' must be type integer */

		status = readAlgoIDheader( stream, cryptAlgo, &altAlgo, parameter, 
								   NULL, DEFAULT_TAG, type );
		if( cryptStatusOK( status ) )
			*altCryptAlgo = altAlgo;	/* CRYPT_MODE_TYPE vs. integer */
		}
	else
		{
		int dummy;

		status = readAlgoIDheader( stream, cryptAlgo, parameter, &dummy, 
								   NULL, DEFAULT_TAG, type );
		}
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readAlgoIDparam( INOUT STREAM *stream, 
					 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo, 
					 OUT_LENGTH_SHORT_Z int *extraLength,
					 IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( extraLength, sizeof( int ) ) );

	REQUIRES_S( type == ALGOID_CLASS_PKC );

	return( readAlgoIDheader( stream, cryptAlgo, NULL, NULL, extraLength, 
							  DEFAULT_TAG, type ) );
	}

/* Determine the size of an AlgorithmIdentifier record from a context.  See
   the comment for sizeofAlgoIDex() for why we don't have to deal with
   parameterised algorithms */

CHECK_RETVAL_LENGTH \
int sizeofContextAlgoID( IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						 IN_RANGE( 0, 999 ) const int parameter )
	{
	int algorithm, status;

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( parameter >= 0 && parameter <= 999 );

	/* Write the algoID only */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( isHashExtAlgo( algorithm ) )
		{
		int blockSize;

		REQUIRES( parameter == 0 );

		/* The extended hash algorithms can have various different hash 
		   sizes, to get the exact variant that's being used we have to 
		   query the block size */
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		return( sizeofAlgoIDex( algorithm, blockSize, 0 ) );
		}
	return( sizeofAlgoIDex( algorithm, parameter, 0 ) );
	}

/* Write an AlgorithmIdentifier record from a context.  The associatedAlgo 
   parameter is used for aWithB algorithms like rsaWithSHA1, with the 
   context containing the 'A' algorithm and the parameter indicating the 'B' 
   algorithm */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeContextAlgoID( INOUT STREAM *stream, 
						IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						IN_ALGO_OPT const int associatedAlgo )
	{
	int algorithm, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( isHandleRangeValid( iCryptContext ) );
	REQUIRES_S( associatedAlgo == CRYPT_ALGO_NONE || \
				isHashAlgo( associatedAlgo ) );

	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( isHashExtAlgo( algorithm ) )
		{
		int blockSize;

		REQUIRES( associatedAlgo == CRYPT_ALGO_NONE );

		/* The extended hash algorithms can have various different hash 
		   sizes, to get the exact variant that's being used we have to 
		   query the block size */
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &blockSize, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		return( writeAlgoIDex( stream, algorithm, blockSize, 0 ) );
		}
	return( writeAlgoIDex( stream, algorithm, associatedAlgo, 0 ) );
	}

/* Turn an AlgorithmIdentifier into a context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readContextAlgoID( INOUT STREAM *stream, 
					   OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
					   OUT_OPT QUERY_INFO *queryInfo, 
					   IN_TAG const int tag,
					   IN_ENUM( ALGOID_CLASS ) const ALGOID_CLASS_TYPE type )
	{
	QUERY_INFO localQueryInfo, *queryInfoPtr = queryInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	int mode, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( iCryptContext == NULL || \
			isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( queryInfo == NULL || \
			isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );
	REQUIRES_S( type == ALGOID_CLASS_CRYPT || type == ALGOID_CLASS_HASH || \
				type == ALGOID_CLASS_AUTHENC );

	/* Clear return value */
	if( iCryptContext != NULL )
		*iCryptContext = CRYPT_ERROR;

	/* If the user isn't interested in the algorithm details, use a local 
	   query structure to contain them */
	if( queryInfo == NULL )
		queryInfoPtr = &localQueryInfo;

	/* Clear optional return value */
	memset( queryInfoPtr, 0, sizeof( QUERY_INFO ) );

	/* Read the algorithm info.  If we're not creating a context from the
	   info, we're done */
	status = readAlgoIDInfo( stream, queryInfoPtr, tag, type );
	if( cryptStatusError( status ) || iCryptContext == NULL )
		return( status );

	/* Create the object from it */
	setMessageCreateObjectInfo( &createInfo, queryInfoPtr->cryptAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	if( isHashExtAlgo( queryInfoPtr->cryptAlgo ) )
		{
		/* It's a variable-width hash algorithm, set the output width */
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE, 
								  &queryInfoPtr->hashAlgoParam, 
								  CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( queryInfoPtr->cryptAlgo > CRYPT_ALGO_LAST_CONVENTIONAL )
		{
		/* If it's not a conventional encryption algorithm, we're done */
		*iCryptContext = createInfo.cryptHandle;
		return( CRYPT_OK );
		}
	ENSURES_S( isConvAlgo( queryInfoPtr->cryptAlgo ) );
	mode = queryInfoPtr->cryptMode;	/* int vs.enum */
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) && \
		!isStreamCipher( queryInfoPtr->cryptAlgo ) )
		{
		int ivLength;

		/* It's a block cipher, get the IV information as well */
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_GETATTRIBUTE, &ivLength,
								  CRYPT_CTXINFO_IVSIZE );
		if( cryptStatusOK( status ) )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, queryInfoPtr->iv,
							min( ivLength, queryInfoPtr->ivLength ) );
			status = krnlSendMessage( createInfo.cryptHandle,
									  IMESSAGE_SETATTRIBUTE_S, &msgData,
									  CRYPT_CTXINFO_IV );
			}
		}
	if( cryptStatusError( status ) )
		{
		/* If there's an error in the parameters stored with the key we'll
		   get an arg or attribute error when we try to set the attribute so
		   we translate it into an error code which is appropriate for the
		   situation.  In addition since this is (arguably) a stream format
		   error (the data read from the stream is invalid) we also set the
		   stream status */
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		if( cryptArgError( status ) )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		return( status );
		}
	*iCryptContext = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Read/write a non-crypto algorithm identifier, used for things like 
   content types.  This just wraps the given OID up in the 
   AlgorithmIdentifier and writes it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGenericAlgoID( INOUT STREAM *stream, 
					   IN_BUFFER( oidLength ) const BYTE *oid, 
					   IN_LENGTH_OID const int oidLength )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oid, oidLength ) && \
			oidLength == sizeofOID( oid ) );

	REQUIRES_S( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );

	/* Read the AlgorithmIdentifier wrapper and OID.  One possible 
	   complication here is the standard NULL vs.absent AlgorithmIdentifier 
	   parameter issue, to handle this we allow either option */
	status = readSequence( stream, &length );
	if( cryptStatusOK( status ) )
		status = readFixedOID( stream, oid, oidLength );
	if( cryptStatusError( status ) )
		return( status );
	length -= oidLength;
	if( length > 0 )
		return( readNull( stream ) );

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeGenericAlgoID( INOUT STREAM *stream, 
						IN_BUFFER( oidLength ) const BYTE *oid, 
						IN_LENGTH_OID const int oidLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oid, oidLength ) && \
			oidLength == sizeofOID( oid ) );

	REQUIRES_S( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );

	writeSequence( stream, oidLength );
	return( writeOID( stream, oid ) );
	}
