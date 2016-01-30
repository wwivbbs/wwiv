/****************************************************************************
*																			*
*							cryptlib Internal API							*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

/* A generic module that implements a rug under which all problems not
   solved elsewhere are swept */

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "stream.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/* Perform the FIPS-140 statistical checks that are feasible on a byte
   string.  The full suite of tests assumes that an infinite source of
   values (and time) is available, the following is a scaled-down version
   used to sanity-check keys and other short random data blocks.  Note that
   this check requires at least 64 bits of data in order to produce useful
   results */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkNontrivialKey( IN_BUFFER( dataLength ) const BYTE *data, 
								   IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int dataLength )
	{
	int i;

	for( i = 0; i < dataLength; i++	)
		{
		if( !isAlnum( data[ i ] ) )
			break;
		}
	if( i >= dataLength )
		return( FALSE );

	for( i = 0; i < dataLength - 1; i++ )
		{
		const int delta = abs( data[ i ] - data[ i + 1 ] );

		if( delta > 8 )
			break;
		}
	if( i >= dataLength - 1 )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkEntropy( IN_BUFFER( dataLength ) const BYTE *data, 
					  IN_LENGTH_SHORT_MIN( MIN_KEYSIZE ) const int dataLength )
	{
	const int delta = ( dataLength < 16 ) ? 1 : 0;
	int bitCount[ 8 + 8 ], noOnes, i;

	assert( isReadPtr( data, dataLength ) );
	
	REQUIRES_B( dataLength >= MIN_KEYSIZE && dataLength < MAX_INTLENGTH_SHORT );

	memset( bitCount, 0, sizeof( int ) * 9 );
	for( i = 0; i < dataLength; i++ )
		{
		const int value = byteToInt( data[ i ] );

		bitCount[ value & 3 ]++;
		bitCount[ ( value >> 2 ) & 3 ]++;
		bitCount[ ( value >> 4 ) & 3 ]++;
		bitCount[ ( value >> 6 ) & 3 ]++;
		}

	/* Monobit test: Make sure that at least 1/4 of the bits are ones and 1/4
	   are zeroes */
	noOnes = bitCount[ 1 ] + bitCount[ 2 ] + ( 2 * bitCount[ 3 ] );
	if( noOnes < dataLength * 2 || noOnes > dataLength * 6 )
		{
		zeroise( bitCount, 8 * sizeof( int ) );
		return( FALSE );
		}

	/* Poker test (almost): Make sure that each bit pair is present at least
	   1/16 of the time.  The FIPS 140 version uses 4-bit values but the
	   numer of samples available from the keys is far too small for this so
	   we can only use 2-bit values.

	   This isn't precisely 1/16, for short samples (< 128 bits) we adjust
	   the count by one because of the small sample size and for odd-length
	   data we're getting four more samples so the actual figure is slightly
	   less than 1/16 */
	if( ( bitCount[ 0 ] + delta < dataLength / 2 ) || \
		( bitCount[ 1 ] + delta < dataLength / 2 ) || \
		( bitCount[ 2 ] + delta < dataLength / 2 ) || \
		( bitCount[ 3 ] + delta < dataLength / 2 ) )
		{
		zeroise( bitCount, 8 * sizeof( int ) );
		return( FALSE );
		}

	zeroise( bitCount, 8 * sizeof( int ) );
	return( TRUE );
	}

/* Copy a string attribute to external storage, with various range checks
   to follow the cryptlib semantics (these will already have been done by
   the caller, this is just a backup check).  There are two forms for this
   function, one that takes a MESSAGE_DATA parameter containing all of the 
   result parameters in one place and the other that takes distinct result
   parameters, typically because they've been passed down through several
   levels of function call beyond the point where they were in a 
   MESSAGE_DATA */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int attributeCopyParams( OUT_BUFFER_OPT( destMaxLength, \
										 *destLength ) void *dest, 
						 IN_LENGTH_SHORT_Z const int destMaxLength, 
						 OUT_LENGTH_BOUNDED_SHORT_Z( destMaxLength ) \
							int *destLength, 
						 IN_BUFFER_OPT( sourceLength ) const void *source, 
						 IN_LENGTH_SHORT_Z const int sourceLength )
	{
	assert( ( dest == NULL && destMaxLength == 0 ) || \
			( isWritePtr( dest, destMaxLength ) ) );
	assert( isWritePtr( destLength, sizeof( int ) ) );
	assert( ( source == NULL && sourceLength == 0 ) || \
			isReadPtr( source, sourceLength ) );

	REQUIRES( ( dest == NULL && destMaxLength == 0 ) || \
			  ( dest != NULL && \
				destMaxLength > 0 && \
				destMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( source == NULL && sourceLength == 0 ) || \
			  ( source != NULL && \
			    sourceLength > 0 && \
				sourceLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return value */
	*destLength = 0;

	if( sourceLength <= 0 )
		return( CRYPT_ERROR_NOTFOUND );
	if( dest != NULL )
		{
		assert( isReadPtr( source, sourceLength ) );

		if( sourceLength > destMaxLength || \
			!isWritePtr( dest, sourceLength ) )
			return( CRYPT_ERROR_OVERFLOW );
		memcpy( dest, source, sourceLength );
		}
	*destLength = sourceLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int attributeCopy( INOUT MESSAGE_DATA *msgData, 
				   IN_BUFFER( attributeLength ) const void *attribute, 
				   IN_LENGTH_SHORT_Z const int attributeLength )
	{
	assert( isWritePtr( msgData, sizeof( MESSAGE_DATA ) ) );
	assert( attributeLength == 0 || \
			isReadPtr( attribute, attributeLength ) );

	REQUIRES( attributeLength >= 0 && \
			  attributeLength < MAX_INTLENGTH_SHORT );

	return( attributeCopyParams( msgData->data, msgData->length, 
								 &msgData->length, attribute, 
								 attributeLength ) );
	}

/* Check whether a given algorithm is available */

CHECK_RETVAL_BOOL \
BOOLEAN algoAvailable( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_QUERY_INFO queryInfo;

	REQUIRES_B( cryptAlgo > CRYPT_ALGO_NONE && \
				cryptAlgo < CRYPT_ALGO_LAST );

	/* Short-circuit check for always-available algorithms.  The kernel 
	   won't initialise without the symmetric and hash algorithms being 
	   present (and SHA-x implies HMAC-SHAx) so it's safe to hardcode them 
	   in here */
	if( cryptAlgo == CRYPT_ALGO_3DES || \
		cryptAlgo == CRYPT_ALGO_AES || \
		cryptAlgo == CRYPT_ALGO_SHA1 || \
		cryptAlgo == CRYPT_ALGO_HMAC_SHA1 || \
		cryptAlgo == CRYPT_ALGO_SHA2 || \
		cryptAlgo == CRYPT_ALGO_HMAC_SHA2 || \
		cryptAlgo == CRYPT_ALGO_RSA )
		return( TRUE );

	return( cryptStatusOK( krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									IMESSAGE_DEV_QUERYCAPABILITY, &queryInfo,
									cryptAlgo ) ) ? TRUE : FALSE );
	}

/* For a given hash algorithm pair, check whether the first is stronger than 
   the second.  The order is:

	SNAng > SHA2 > SHA-1 > all others */

CHECK_RETVAL_BOOL \
BOOLEAN isStrongerHash( IN_ALGO const CRYPT_ALGO_TYPE algorithm1,
						IN_ALGO const CRYPT_ALGO_TYPE algorithm2 )
	{
	static const CRYPT_ALGO_TYPE algoPrecedence[] = {
		CRYPT_ALGO_SHAng, CRYPT_ALGO_SHA2, CRYPT_ALGO_SHA1, 
		CRYPT_ALGO_NONE, CRYPT_ALGO_NONE };
	int algo1index, algo2index;

	REQUIRES_B( isHashAlgo( algorithm1 ) );
	REQUIRES_B( isHashAlgo( algorithm2 ) );

	/* Find the relative positions on the scale of the two algorithms */
	for( algo1index = 0; 
		 algoPrecedence[ algo1index ] != algorithm1 && \
			algo1index < FAILSAFE_ARRAYSIZE( algoPrecedence, CRYPT_ALGO_TYPE );
		 algo1index++ )
		{
		/* If we've reached an unrated algorithm, it can't be stronger than 
		   the other one */
		if( algoPrecedence[ algo1index ] == CRYPT_ALGO_NONE )
			return( FALSE );
		}
	ENSURES_B( algo1index < FAILSAFE_ARRAYSIZE( algoPrecedence, \
												CRYPT_ALGO_TYPE ) );
	for( algo2index = 0; 
		 algoPrecedence[ algo2index ] != algorithm2 && \
			algo2index < FAILSAFE_ARRAYSIZE( algoPrecedence, CRYPT_ALGO_TYPE );
		 algo2index++ )
		{
		/* If we've reached an unrated algorithm, it's weaker than the other 
		   one */
		if( algoPrecedence[ algo2index ] == CRYPT_ALGO_NONE )
			return( TRUE );
		}
	ENSURES_B( algo2index < FAILSAFE_ARRAYSIZE( algoPrecedence, \
												CRYPT_ALGO_TYPE ) );

	/* If the first algorithm has a smaller index than the second, it's a
	   stronger algorithm */
	return( ( algo1index < algo2index ) ? TRUE : FALSE );
	}

/* Return a random small integer.  This is used to perform lightweight 
   randomisation of various algorithms in order to make DoS attacks harder.  
   Because of this the values don't have to be cryptographically strong, so 
   all that we do is cache the data from CRYPT_IATTRIBUTE_RANDOM_NONCE and 
   pull out a small integer's worth on each call */

#define RANDOM_BUFFER_SIZE	16

CHECK_RETVAL_RANGE_NOERROR( 0, 32767 ) \
int getRandomInteger( void )
	{
	static BYTE nonceData[ RANDOM_BUFFER_SIZE + 8 ];
	static int nonceIndex = 0;
	int returnValue, status;

	REQUIRES_EXT( !( nonceIndex & 1 ), 0 );

	/* Initialise/reinitialise the nonce data if necessary.  See the long 
	   coment for getNonce() in system.c for the reason why we don't bail 
	   out on error but continue with a lower-quality generator */
	if( nonceIndex <= 0 )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, nonceData, RANDOM_BUFFER_SIZE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( ( int ) getTime() & 0x7FFF );
		}

	/* Extract the next random integer value from the buffered data */
	returnValue = ( byteToInt( nonceData[ nonceIndex ] ) << 8 ) | \
					byteToInt( nonceData[ nonceIndex + 1 ] );
	nonceIndex = ( nonceIndex + 2 ) % RANDOM_BUFFER_SIZE;
	ENSURES_EXT( nonceIndex >= 0 && nonceIndex < RANDOM_BUFFER_SIZE, 0 );

	/* Return the value constrained to lie within the range 0...32767 */
	return( returnValue & 0x7FFF );
	}

/* Map one value to another, used to map values from one representation 
   (e.g. PGP algorithms or HMAC algorithms) to another (cryptlib algorithms
   or the underlying hash used for the HMAC algorithm) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int mapValue( IN_INT_SHORT_Z const int srcValue,
			  OUT_INT_SHORT_Z int *destValue,
			  IN_ARRAY( mapTblSize ) const MAP_TABLE *mapTbl,
			  IN_LENGTH_SHORT const int mapTblSize )
	{
	int i;

	assert( isWritePtr( destValue, sizeof( int ) ) );
	assert( isReadPtr( mapTbl, mapTblSize * sizeof( MAP_TABLE ) ) );

	REQUIRES( srcValue >= 0 && srcValue < MAX_INTLENGTH_SHORT );
	REQUIRES( mapTblSize > 0 && mapTblSize < 100 );
	REQUIRES( mapTbl[ mapTblSize ].source == CRYPT_ERROR );

	/* Clear return value */
	*destValue = 0;

	/* Convert the hash algorithm into the equivalent HMAC algorithm */
	for( i = 0; i < mapTblSize && mapTbl[ i ].source != CRYPT_ERROR && \
				i < FAILSAFE_ITERATIONS_LARGE; i++ )
		{
		if( mapTbl[ i ].source == srcValue )
			{
			*destValue = mapTbl[ i ].destination;

			return( CRYPT_OK );
			}
		}
	ENSURES( i < mapTblSize );

	return( CRYPT_ERROR_NOTAVAIL );
	}

/****************************************************************************
*																			*
*							Checksum/Hash Functions							*
*																			*
****************************************************************************/

/* Calculate a 16-bit Fletcher-like checksum for a block of data.  This 
   isn't quite a pure Fletcher checksum, this isn't a big deal since all we 
   need is consistent results for identical data, the value itself is never 
   communicated externally.  In cases where it's used in critical checks 
   it's merely used as a quick pre-check for a full hash-based check, so it 
   doesn't have to be perfect.  In addition it's not in any way 
   cryptographically secure for the same reason, there's no particular need 
   for it to be secure.  If a requirement for at least some sort of 
   unpredictability did arise then something like Pearson hashing could be 
   substituted transparently */

RETVAL_RANGE( MAX_ERROR, 0xFFFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int checksumData( IN_BUFFER( dataLength ) const void *data, 
				  IN_DATALENGTH const int dataLength )
	{
	const BYTE *dataPtr = data;
	int sum1 = 1, sum2 = 0, i;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( data != NULL );
	REQUIRES( dataLength > 0 && dataLength < MAX_BUFFER_SIZE )

	for( i = 0; i < dataLength; i++ )
		{
		sum1 += dataPtr[ i ];
		sum2 += sum1;
		}

#if defined( SYSTEM_16BIT )
	return( sum2 & 0x7FFF );
#else
	return( ( ( sum2 & 0x7FFF ) << 16 ) | ( sum1 & 0xFFFF ) );
#endif /* 16- vs. 32-bit systems */
	}

/* Calculate the hash of a block of data.  We use SHA-1 because it's the 
   built-in default, but any algorithm will do since we're only using it
   to transform a variable-length value to a fixed-length one for easy
   comparison purposes */

STDC_NONNULL_ARG( ( 1, 3 ) ) \
void hashData( OUT_BUFFER_FIXED( hashMaxLength ) BYTE *hash, 
			   IN_LENGTH_HASH const int hashMaxLength, 
			   IN_BUFFER( dataLength ) const void *data, 
			   IN_DATALENGTH const int dataLength )
	{
	static HASHFUNCTION_ATOMIC hashFunctionAtomic = NULL;
	static int hashSize;
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];

	assert( isWritePtr( hash, hashMaxLength ) );
	assert( hashMaxLength >= 16 && \
			hashMaxLength <= CRYPT_MAX_HASHSIZE );
	assert( isReadPtr( data, dataLength ) );
	assert( dataLength > 0 && dataLength < MAX_BUFFER_SIZE );

	/* Get the hash algorithm information if necessary */
	if( hashFunctionAtomic == NULL )
		{
		getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
								 &hashSize );
		}

	/* Error handling: If there's a problem, return a zero hash.  We use 
	   this strategy since this is a void function and so the usual 
	   REQUIRES() predicate won't be effective.  Note that this can lead to 
	   a false-positive match if we're called multiple times with invalid 
	   input, in theory we could fill the return buffer with nonce data to 
	   ensure that we never get a false-positive match but since this is a 
	   should-never-occur condition anyway it's not certain whether forcing 
	   a match or forcing a non-match is the preferred behaviour */
	if( data == NULL || dataLength <= 0 || dataLength >= MAX_BUFFER_SIZE || \
		hashMaxLength < 16 || hashMaxLength > hashSize || \
		hashMaxLength > CRYPT_MAX_HASHSIZE || hashFunctionAtomic == NULL )
		{
		memset( hash, 0, hashMaxLength );
		retIntError_Void();
		}

	/* Hash the data and copy as many bytes as the caller has requested to
	   the output.  Typically they'll require only a subset of the full 
	   amount since all that we're doing is transforming a variable-length
	   value to a fixed-length value for easy comparison purposes */
	hashFunctionAtomic( hashBuffer, hashSize, data, dataLength );
	memcpy( hash, hashBuffer, hashMaxLength );
	zeroise( hashBuffer, hashSize );
	}

/* Compare two blocks of memory in a time-independent manner.  This is used 
   to avoid potential timing attacks on memcmp(), which bails out as soon as
   it finds a mismatch */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN compareDataConstTime( IN_BUFFER( length ) const void *src,
							  IN_BUFFER( length ) const void *dest,
							  IN_LENGTH_SHORT const int length )
	{
	const BYTE *srcPtr = src, *destPtr = dest;
	int value = 0, i;

	assert( isReadPtr( src, length ) );
	assert( isReadPtr( dest, length ) );

	REQUIRES_B( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* Compare the two values in a time-independent manner */
	for( i = 0; i < length; i++ )
		value |= srcPtr[ i ] ^ destPtr[ i ];

	return( !value );
	}

/****************************************************************************
*																			*
*							Stream Export/Import Routines					*
*																			*
****************************************************************************/

/* Export attribute or certificate data to a stream.  In theory we would
   have to export this via a dynbuf and then write it to the stream but we 
   can save some overhead by writing it directly to the stream's buffer.
   
   Some attributes have a user-defined size (e.g. 
   CRYPT_IATTRIBUTE_RANDOM_NONCE) so we allow the caller to specify an 
   optional length parameter indicating how much of the attribute should be 
   exported */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exportAttr( INOUT STREAM *stream, 
					   IN_HANDLE const CRYPT_HANDLE cryptHandle,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeType,
					   IN_LENGTH_INDEF const int length )
							/* Declared as LENGTH_INDEF because SHORT_INDEF
							   doesn't make sense */
	{
	MESSAGE_DATA msgData;
	void *dataPtr = NULL;
	int attrLength = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( sStatusOK( stream ) );

	REQUIRES( cryptHandle == SYSTEM_OBJECT_HANDLE || \
			  isHandleRangeValid( cryptHandle ) );
	REQUIRES( isAttribute( attributeType ) || \
			  isInternalAttribute( attributeType ) );
	REQUIRES( ( length == CRYPT_UNUSED ) || \
			  ( length >= 8 && length < MAX_INTLENGTH_SHORT ) );

	/* Get access to the stream buffer if required */
	if( !sIsNullStream( stream ) )
		{
		if( length != CRYPT_UNUSED )
			{
			/* It's an explicit-length attribute, make sure that there's 
			   enough room left in the stream for it */
			attrLength = length;
			status = sMemGetDataBlock( stream, &dataPtr, length );
			}
		else
			{
			/* It's an implicit-length attribute whose maximum length is 
			   defined by the stream size */
			status = sMemGetDataBlockRemaining( stream, &dataPtr, 
												&attrLength );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Export the attribute directly into the stream buffer */
	setMessageData( &msgData, dataPtr, attrLength );
	status = krnlSendMessage( cryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, attributeType );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, msgData.length, SSKIP_MAX );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int exportAttributeToStream( INOUT TYPECAST( STREAM * ) void *streamPtr, 
							 IN_HANDLE const CRYPT_HANDLE cryptHandle,
							 IN_ATTRIBUTE \
								const CRYPT_ATTRIBUTE_TYPE attributeType )
	{
	assert( isWritePtr( streamPtr, sizeof( STREAM ) ) );

	REQUIRES( isHandleRangeValid( cryptHandle ) );
	REQUIRES( isAttribute( attributeType ) || \
			  isInternalAttribute( attributeType ) );

	return( exportAttr( streamPtr, cryptHandle, attributeType, \
						CRYPT_UNUSED ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int exportVarsizeAttributeToStream( INOUT TYPECAST( STREAM * ) void *streamPtr,
									IN_HANDLE const CRYPT_HANDLE cryptHandle,
									IN_LENGTH_FIXED( CRYPT_IATTRIBUTE_RANDOM_NONCE ) \
										const CRYPT_ATTRIBUTE_TYPE attributeType,
									IN_RANGE( 8, 1024 ) \
										const int attributeDataLength )
	{
	assert( isWritePtr( streamPtr, sizeof( STREAM ) ) );

	REQUIRES( cryptHandle == SYSTEM_OBJECT_HANDLE );
	REQUIRES( attributeType == CRYPT_IATTRIBUTE_RANDOM_NONCE );
	REQUIRES( attributeDataLength >= 8 && \
			  attributeDataLength <= MAX_ATTRIBUTE_SIZE );

	return( exportAttr( streamPtr, cryptHandle, attributeType, 
						attributeDataLength ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int exportCertToStream( INOUT TYPECAST( STREAM * ) void *streamPtr,
						IN_HANDLE const CRYPT_CERTIFICATE cryptCertificate,
						IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormatType )
	{
	MESSAGE_DATA msgData;
	STREAM *stream = streamPtr;
	void *dataPtr = NULL;
	int length = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( sStatusOK( stream ) );

	REQUIRES( isHandleRangeValid( cryptCertificate ) );
	REQUIRES( certFormatType > CRYPT_CERTFORMAT_NONE && \
			  certFormatType < CRYPT_CERTFORMAT_LAST );

	/* Get access to the stream buffer if required */
	if( !sIsNullStream( stream ) )
		{
		status = sMemGetDataBlockRemaining( stream, &dataPtr, &length );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Export the certificate directly into the stream buffer */
	setMessageData( &msgData, dataPtr, length );
	status = krnlSendMessage( cryptCertificate, IMESSAGE_CRT_EXPORT,
							  &msgData, certFormatType );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, msgData.length, SSKIP_MAX );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int importCertFromStream( INOUT void *streamPtr,
						  OUT_HANDLE_OPT CRYPT_CERTIFICATE *cryptCertificate,
						  IN_HANDLE const CRYPT_USER iCryptOwner,
						  IN_ENUM( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE certType, 
						  IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
							const int certDataLength,
						  IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM *stream = streamPtr;
	void *dataPtr;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( sStatusOK( stream ) );
	assert( isWritePtr( cryptCertificate, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( certType > CRYPT_CERTTYPE_NONE && \
			  certType < CRYPT_CERTTYPE_LAST );
	REQUIRES( certDataLength >= MIN_CRYPT_OBJECTSIZE && \
			  certDataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_FLAG_DATAONLY_CERT ) == 0 );

	/* Clear return value */
	*cryptCertificate = CRYPT_ERROR;

	/* Get access to the stream buffer and skip over the certificate data */
	status = sMemGetDataBlock( stream, &dataPtr, certDataLength );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, certDataLength, SSKIP_MAX );
	if( cryptStatusError( status ) )
		return( status );

	/* Import the certificate directly from the stream buffer */
	setMessageCreateObjectIndirectInfoEx( &createInfo, dataPtr, 
						certDataLength, certType,
						( options & KEYMGMT_FLAG_DATAONLY_CERT ) ? \
							KEYMGMT_FLAG_DATAONLY_CERT : KEYMGMT_FLAG_NONE );
	createInfo.cryptOwner = iCryptOwner;
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	*cryptCertificate = createInfo.cryptHandle;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Public-key Import Routines						*
*																			*
****************************************************************************/

#ifdef USE_INT_ASN1

/* Read a public key from an X.509 SubjectPublicKeyInfo record, creating the
   context necessary to contain it in the process.  This is used by a variety
   of modules including certificate-management, keyset, and crypto device. 
   
   The use of the void * instead of STREAM * is necessary because the STREAM
   type isn't visible at the global level */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkKeyLength( INOUT STREAM *stream,
						   const CRYPT_ALGO_TYPE cryptAlgo,
						   const BOOLEAN hasAlgoParameters )
	{
	const long startPos = stell( stream );
	int keyLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* ECC algorithms are a complete mess to handle because of the arbitrary
	   manner in which the algorithm parameters can be represented.  To deal
	   with this we skip the parameters and read the public key value, which 
	   is a point on a curve stuffed in a variety of creative ways into an 
	   BIT STRING.  Since this contains two values (the x and y coordinates) 
	   we divide the lengths used by two to get an approximation of the 
	   nominal key size */
	if( isEccAlgo( cryptAlgo ) )
		{
		readUniversal( stream );	/* Skip algorithm parameters */
		status = readBitStringHole( stream, &keyLength, 
									MIN_PKCSIZE_ECCPOINT_THRESHOLD, 
									DEFAULT_TAG );
		if( cryptStatusOK( status ) && isShortECCKey( keyLength / 2 ) )
			status = CRYPT_ERROR_NOSECURE;
		if( cryptStatusError( status ) )
			return( status );

		return( sseek( stream, startPos ) );
		}

	/* Read the key component that defines the nominal key size, either the 
	   first algorithm parameter or the first public-key component */
	if( hasAlgoParameters )
		{
		readSequence( stream, NULL );
		status = readGenericHole( stream, &keyLength, MIN_PKCSIZE_THRESHOLD, 
								  BER_INTEGER );
		}
	else
		{
		readBitStringHole( stream, NULL, MIN_PKCSIZE_THRESHOLD, DEFAULT_TAG );
		readSequence( stream, NULL );
		status = readGenericHole( stream, &keyLength, MIN_PKCSIZE_THRESHOLD, 
								  BER_INTEGER );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Check whether the nominal keysize is within the range defined as 
	   being a weak key */
	if( isShortPKCKey( keyLength ) )
		return( CRYPT_ERROR_NOSECURE );

	return( sseek( stream, startPos ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int iCryptReadSubjectPublicKey( INOUT TYPECAST( STREAM * ) void *streamPtr, 
								OUT_HANDLE_OPT CRYPT_CONTEXT *iPubkeyContext,
								IN_HANDLE const CRYPT_DEVICE iCreatorHandle, 
								const BOOLEAN deferredLoad )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	STREAM *stream = streamPtr;
	void *spkiPtr DUMMY_INIT_PTR;
	int spkiLength, length, status;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( iPubkeyContext, sizeof( CRYPT_CONTEXT ) ) );

	REQUIRES( iCreatorHandle == SYSTEM_OBJECT_HANDLE || \
			  isHandleRangeValid( iCreatorHandle ) );

	/* Clear return value */
	*iPubkeyContext = CRYPT_ERROR;

	/* Pre-parse the SubjectPublicKeyInfo, both to ensure that it's (at 
	   least generally) valid before we go to the extent of creating an 
	   encryption context to contain it and to get access to the 
	   SubjectPublicKeyInfo data and algorithm information.  Because all 
	   sorts of bizarre tagging exist due to things like CRMF we read the 
	   wrapper as a generic hole rather than the more obvious SEQUENCE */
	status = getStreamObjectLength( stream, &spkiLength );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( stream, &spkiPtr, spkiLength );
	if( cryptStatusOK( status ) )
		{
		status = readGenericHole( stream, NULL, 
								  MIN_PKCSIZE_ECCPOINT_THRESHOLD, 
								  DEFAULT_TAG );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = readAlgoIDparam( stream, &cryptAlgo, &length, 
							  ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform minimal key-length checking.  We need to do this at this 
	   point (rather than having it done implicitly in the 
	   SubjectPublicKeyInfo read code) because a too-short key (or at least 
	   too-short key data) will result in the kernel rejecting the 
	   SubjectPublicKeyInfo before it can be processed, leading to a rather 
	   misleading CRYPT_ERROR_BADDATA return status rather the correct 
	   CRYPT_ERROR_NOSECURE */
	status = checkKeyLength( stream, cryptAlgo,
							 ( length > 0 ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Skip the remainder of the key components in the stream, first the
	   algorithm parameters (if there are any) and then the public-key 
	   data */
	if( length > 0 )
		readUniversal( stream );
	status = readUniversal( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the public-key context and send the public-key data to it */
	setMessageCreateObjectInfo( &createInfo, cryptAlgo );
	status = krnlSendMessage( iCreatorHandle, IMESSAGE_DEV_CREATEOBJECT, 
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;
	setMessageData( &msgData, spkiPtr, spkiLength );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, deferredLoad ? \
								CRYPT_IATTRIBUTE_KEY_SPKI_PARTIAL : \
								CRYPT_IATTRIBUTE_KEY_SPKI );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		if( status == CRYPT_ARGERROR_STR1 || \
			status == CRYPT_ARGERROR_NUM1 )
			{
			/* If the key data was rejected by the kernel before it got to 
			   the SubjectPublicKeyInfo read code (see the comment above) 
			   then it'll be rejected with an argument-error code, which we
			   have to convert to a bad-data error before returning it to 
			   the caller */
			return( CRYPT_ERROR_BADDATA );
			}
		if( cryptArgError( status ) )
			{
			DEBUG_DIAG(( "Public-key load returned argError status" ));
			assert( DEBUG_WARN );
			status = CRYPT_ERROR_BADDATA;
			}
		return( status );
		}
	*iPubkeyContext = iCryptContext;
	assert( cryptStatusError( \
				krnlSendMessage( iCryptContext, IMESSAGE_CHECK, 
								 NULL, MESSAGE_CHECK_PKC_PRIVATE ) ) );
	return( CRYPT_OK );
	}
#endif /* USE_INT_ASN1 */

/****************************************************************************
*																			*
*							Safe Text-line Read Functions					*
*																			*
****************************************************************************/

#if defined( USE_HTTP ) || defined( USE_BASE64 ) || defined( USE_SSH )

/* Read a line of text data ending in an EOL.  If we get more data than will 
   fit into the read buffer we discard it until we find an EOL.  As a 
   secondary concern we want to strip leading, trailing, and repeated 
   whitespace.  Leading whitespace is handled by setting the seen-whitespace 
   flag to true initially, this treats any whitespace at the start of the 
   line as superfluous and strips it.  Stripping of repeated whitespace is 
   also handled by the seenWhitespace flag, and stripping of trailing 
   whitespace is handled by walking back through any final whitespace once we 
   see the EOL. 
   
   We optionally handle continued lines denoted by the MIME convention of a 
   semicolon as the last non-whitespace character by setting the 
   seenContinuation flag if we see a semicolon as the last non-whitespace 
   character.

   Finally, we also need to handle generic DoS attacks.  If we see more than
   MAX_LINE_LENGTH chars in a line we bail out */

#define MAX_LINE_LENGTH		4096

/* The extra level of indirection provided by this function is necessary 
   because the the extended error information isn't accessible from outside 
   the stream code so we can't set it in formatTextLineError() in the usual 
   manner via a retExt().  Instead we call retExtFn() directly and then pass 
   the result down to the stream layer via an ioctl */

CHECK_RETVAL_ERROR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int exitTextLineError( INOUT STREAM *stream,
							  FORMAT_STRING const char *format, 
							  const int value1, const int value2,
							  OUT_OPT_BOOL BOOLEAN *localError,
							  IN_ERROR const int status )
	{
	ERROR_INFO errorInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( format, 4 ) );
	assert( localError == NULL || \
			isReadPtr( localError, sizeof( BOOLEAN ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* If the stream doesn't support extended error information, we're 
	   done */
	if( localError == NULL )
		return( status );

	/* The CRYPT_ERROR_BADDATA is a dummy code used in order to be able to 
	   call retExtFn() to format the error string */
	*localError = TRUE;
	( void ) retExtFn( CRYPT_ERROR_BADDATA, &errorInfo, format, 
					   value1, value2 );
	sioctlSetString( stream, STREAM_IOCTL_ERRORINFO, &errorInfo, 
					 sizeof( ERROR_INFO ) );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readTextLine( READCHARFUNCTION readCharFunction, 
				  INOUT void *streamPtr,
				  OUT_BUFFER( lineBufferMaxLen, *lineBufferSize ) \
						char *lineBuffer,
				  IN_LENGTH_SHORT_MIN( 16 ) const int lineBufferMaxLen, 
				  OUT_RANGE( 0, lineBufferMaxLen ) int *lineBufferSize, 
				  OUT_OPT_BOOL BOOLEAN *localError,
				  const BOOLEAN allowContinuation )
	{
	BOOLEAN seenWhitespace, seenContinuation = FALSE;
	int totalChars, bufPos = 0;

	assert( isWritePtr( streamPtr, sizeof( STREAM ) ) );
	assert( isWritePtr( lineBuffer, lineBufferMaxLen ) );
	assert( isWritePtr( lineBufferSize, sizeof( int ) ) );
	assert( localError == NULL || \
			isWritePtr( localError, sizeof( BOOLEAN ) ) );

	REQUIRES( readCharFunction != NULL );
	REQUIRES( lineBufferMaxLen >= 16 && \
			  lineBufferMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( lineBuffer, 0, min( 16, lineBufferMaxLen ) );
	*lineBufferSize = 0;
	if( localError != NULL )
		*localError = FALSE;

	/* Set the seen-whitespace flag initially to strip leading whitespace */
	seenWhitespace = TRUE;

	/* Read up to MAX_LINE_LENGTH chars.  Anything longer than this is 
	   probably a DoS */
	for( totalChars = 0; totalChars < MAX_LINE_LENGTH; totalChars++ )
		{
		int ch;

		/* Get the next input character */
		ch = readCharFunction( streamPtr );
		if( cryptStatusError( ch ) )
			return( ch );

		/* If it's an EOL or a continuation marker, strip trailing 
		   whitespace */
		if( ch == '\n' || ( allowContinuation && ch == ';' ) )
			{
			/* Strip trailing whitespace.  At this point it's all been
			   canonicalised so we don't need to check for anything other 
			   than spaces */
			while( bufPos > 0 && lineBuffer[ bufPos - 1 ] == ' ' )
				bufPos--;
			}

		/* Process EOL */
		if( ch == '\n' )
			{
			/* If we've seen a continuation marker, the line continues on 
			   the next one */
			if( seenContinuation )
				{
				seenContinuation = FALSE;
				continue;
				}

			/* We're done */
			break;
			}

		/* Ignore any additional decoration that may accompany EOLs */
		if( ch == '\r' )
			continue;

		/* If we're over the maximum buffer size discard any further input
		   until we either hit EOL or exceed the DoS threshold at
		   MAX_LINE_LENGTH */
		if( bufPos >= lineBufferMaxLen )
			{
			/* If we've run off into the weeds (for example we're reading 
			   binary data following the text header), bail out */
			if( !isValidTextChar( ch ) )
				{
				return( exitTextLineError( streamPtr, "Invalid character "
										   "0x%02X at position %d", ch, 
										   totalChars, localError,
										   CRYPT_ERROR_BADDATA ) );
				}
			continue;
			}

		/* Process whitespace.  We can't use isspace() for this because it
		   includes all sorts of extra control characters that we don't want
		   to allow */
		if( ch == ' ' || ch == '\t' )
			{
			if( seenWhitespace )
				{
				/* Ignore leading and repeated whitespace */
				continue;
				}
			ch = ' ';	/* Canonicalise whitespace */
			}

		/* Process any remaining chars */
		if( !isValidTextChar( ch ) )
			{
			return( exitTextLineError( streamPtr, "Invalid character "
									   "0x%02X at position %d", ch, 
									   totalChars, localError,
									   CRYPT_ERROR_BADDATA ) );
			}
		lineBuffer[ bufPos++ ] = intToByte( ch );
		ENSURES( bufPos > 0 && bufPos <= totalChars + 1 );
				 /* The 'totalChars + 1' is because totalChars is the loop
				    iterator and won't have been incremented yet at this 
					point */

		/* Update the state variables.  If the character that we've just 
		   processed was whitespace or if we've seen a continuation 
		   character or we're processing whitespace after having seen a 
		   continuation character (which makes it effectively leading 
		   whitespace to be stripped), remember this */
		seenWhitespace = ( ch == ' ' ) ? TRUE : FALSE;
		seenContinuation = allowContinuation && \
						   ( ch == ';' || \
						     ( seenContinuation && seenWhitespace ) ) ? \
						   TRUE : FALSE;
		}
	if( totalChars >= MAX_LINE_LENGTH )
		{
		return( exitTextLineError( streamPtr, "Text line too long, more "
								   "than %d characters", MAX_LINE_LENGTH, 0, 
								   localError, CRYPT_ERROR_OVERFLOW ) );
		}
	*lineBufferSize = bufPos;

	return( CRYPT_OK );
	}
#endif /* USE_HTTP || USE_BASE64 || USE_SSH */

/****************************************************************************
*																			*
*								Self-test Functions							*
*																			*
****************************************************************************/

/* Test code for the above functions */

#ifndef NDEBUG

#if defined( USE_HTTP ) || defined( USE_BASE64 ) || defined( USE_SSH )

#include "io/stream.h"

CHECK_RETVAL_RANGE( 0, 255 ) STDC_NONNULL_ARG( ( 1 ) ) \
static int readCharFunction( INOUT TYPECAST( STREAM * ) void *streamPtr )
	{
	STREAM *stream = streamPtr;
	BYTE ch;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = sread( stream, &ch, 1 );
	return( cryptStatusError( status ) ? status : byteToInt( ch ) );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN testReadLine( IN_BUFFER( dataInLength ) char *dataIn,
							 IN_LENGTH_SHORT_MIN( 8 ) const int dataInLength, 
							 IN_BUFFER( dataOutLength ) char *dataOut,
							 IN_LENGTH_SHORT_MIN( 1 ) const int dataOutLength,
							 const BOOLEAN allowContinuation )
	{
	STREAM stream;
	BYTE buffer[ 16 + 8 ];
	int length, status;

	assert( isReadPtr( dataIn, dataInLength ) );
	assert( isReadPtr( dataOut, dataOutLength ) );

	REQUIRES( dataInLength >= 8 && dataInLength < MAX_INTLENGTH_SHORT );
	REQUIRES( dataOutLength >= 1 && dataOutLength < MAX_INTLENGTH_SHORT );

	sMemPseudoConnect( &stream, dataIn, dataInLength );
	status = readTextLine( readCharFunction, &stream, buffer, 16, 
						   &length, NULL, allowContinuation );
	if( cryptStatusError( status ) )
		return( FALSE );
	if( length != dataOutLength || memcmp( buffer, dataOut, dataOutLength ) )
		return( FALSE );
	sMemDisconnect( &stream );

	return( TRUE );
	}
#endif /* USE_HTTP || USE_BASE64 || USE_SSH */

CHECK_RETVAL_BOOL \
BOOLEAN testIntAPI( void )
	{
#if 0
checkNontrivialKey( "\x2E\x19\x76\x57\xDB\x30\xE6\x26", 8 );
checkNontrivialKey( "\x14\xF3\x3C\x5A\xB8\x63\x13\xFB", 8 );
checkNontrivialKey( "\x7B\xE0\xE4\x14\x5C\x7C\x2C\x07", 8 );
checkNontrivialKey( "\xD3\x9C\x16\x37\xAD\x12\x19\xA2", 8 );
checkNontrivialKey( "\x7F\x6B\x30\xAD\x02\x83\x96\xF9", 8 );
checkNontrivialKey( "\x79\x92\xF9\xD1\x75\x43\x56\x87", 8 );
checkNontrivialKey( "\x62\xAF\x14\xCF\x1F\x5F\xA7\xC6", 8 );
checkNontrivialKey( "\xAE\x57\xF3\x63\x45\x03\x2E\x6B", 8 );
checkNontrivialKey( "abcdefgh", 8 );
checkNontrivialKey( "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8 );
checkNontrivialKey( "\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A", 8 );
#endif

	/* Test the entropy-check code */
	if( !checkEntropy( "\x2E\x19\x76\x57\xDB\x30\xE6\x26", 8 ) || \
		!checkEntropy( "\x14\xF3\x3C\x5A\xB8\x63\x13\xFB", 8 ) || \
		!checkEntropy( "\x7B\xE0\xE4\x14\x5C\x7C\x2C\x07", 8 ) || \
		!checkEntropy( "\xD3\x9C\x16\x37\xAD\x12\x19\xA2", 8 ) || \
		!checkEntropy( "\x7F\x6B\x30\xAD\x02\x83\x96\xF9", 8 ) || \
		!checkEntropy( "\x79\x92\xF9\xD1\x75\x43\x56\x87", 8 ) || \
		!checkEntropy( "\x62\xAF\x14\xCF\x1F\x5F\xA7\xC6", 8 ) || \
		!checkEntropy( "\xAE\x57\xF3\x63\x45\x03\x2E\x6B", 8 ) )
		return( FALSE );
	if( checkEntropy( "\xA5\x5A\xA5\x5A\xA5\x5A\xA5\x5A", 8 ) )
		return( FALSE );
	if( checkNontrivialKey( "abcdefgh", 8 ) )
		return( FALSE );

	/* Test the hash algorithm-strength code */
	if( isStrongerHash( CRYPT_ALGO_SHA1, CRYPT_ALGO_SHA2 ) || \
		!isStrongerHash( CRYPT_ALGO_SHA2, CRYPT_ALGO_SHA1 ) || \
		isStrongerHash( CRYPT_ALGO_MD5, CRYPT_ALGO_SHA2 ) || \
		!isStrongerHash( CRYPT_ALGO_SHA2, CRYPT_ALGO_MD5 ) )
		return( FALSE );

	/* Test the checksumming code */
	if( checksumData( "12345678", 8 ) != checksumData( "12345678", 8 ) || \
		checksumData( "12345678", 8 ) == checksumData( "12345778", 8 ) || \
		checksumData( "12345678", 8 ) == checksumData( "12345\xB7" "78", 8 ) || \
		checksumData( "12345678", 8 ) == checksumData( "12345\x00" "78", 8 ) )
		return( FALSE );

	/* Test the text-line read code */
#if defined( USE_HTTP ) || defined( USE_BASE64 ) || defined( USE_SSH )
	if( !testReadLine( "abcdefgh\n", 9, "abcdefgh", 8, FALSE ) || \
		!testReadLine( "abcdefghijklmnopq\n", 18, 
					   "abcdefghijklmnop", 16, FALSE ) || \
		!testReadLine( " abcdefgh\n", 10, "abcdefgh", 8, FALSE ) || \
		!testReadLine( "abcdefgh \n", 10, "abcdefgh", 8, FALSE ) || \
		!testReadLine( " ab cdefgh \n", 12, "ab cdefgh", 9, FALSE ) || \
		!testReadLine( "   ab   cdefgh   \n", 18, "ab cdefgh", 9, FALSE ) )
		return( FALSE );
	if( !testReadLine( "abcdefgh\r\n", 10, "abcdefgh", 8, FALSE ) || \
		!testReadLine( "abcdefgh\r\r\n", 11, "abcdefgh", 8, FALSE ) )
		return( FALSE );
	if( testReadLine( "   \t   \n", 8, "", 1, FALSE ) || \
		testReadLine( "abc\x12" "efgh\n", 9, "", 1, FALSE ) )
		return( FALSE );
	if( !testReadLine( "abcdefgh;\nabc\n", 14, 
					   "abcdefgh;", 9, FALSE ) || \
		!testReadLine( "abcdefgh;\nabc\n", 14, 
					   "abcdefgh;abc", 12, TRUE ) || \
		!testReadLine( "abcdefgh; \n abc\n", 16, 
					   "abcdefgh;abc", 12, TRUE ) || \
		!testReadLine( "abcdefgh ; \n abc\n", 17, 
					   "abcdefgh;abc", 12, TRUE ) || \
		!testReadLine( "abcdefgh;abc\nabc\n", 17, 
					   "abcdefgh;abc", 12, TRUE ) )
		return( FALSE );
	if( testReadLine( "abcdefgh;\n", 10, "", 1, TRUE ) || \
		testReadLine( "abcdefgh;\n\n", 11, "", 1, TRUE ) || \
		testReadLine( "abcdefgh;\n \n", 12, "", 1, TRUE ) )
		return( FALSE );
#endif /* USE_HTTP || USE_BASE64 || USE_SSH */

	return( TRUE );
	}
#endif /* !NDEBUG */
