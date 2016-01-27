/****************************************************************************
*																			*
*				cryptlib Key Derivation Mechanism Routines					*
*					Copyright Peter Gutmann 1992-2009						*
*																			*
****************************************************************************/

#ifdef INC_ALL
  #include "crypt.h"
  #include "asn1.h"
  #include "mech_int.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "mechs/mech_int.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								PRF Building Blocks							*
*																			*
****************************************************************************/

/* HMAC-based PRF used for PKCS #5 v2 and TLS */

#define HMAC_DATASIZE		64

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7, 8 ) ) \
static int prfInit( IN const HASHFUNCTION hashFunction, 
					IN const HASHFUNCTION_ATOMIC hashFunctionAtomic,
					INOUT TYPECAST( HASHINFO ) void *hashState, 
					IN_LENGTH_HASH const int hashSize, 
					OUT_BUFFER( processedKeyMaxLength, *processedKeyLength ) \
						void *processedKey, 
					IN_LENGTH_FIXED( HMAC_DATASIZE ) \
						const int processedKeyMaxLength,
					OUT_RANGE( 0, HMAC_DATASIZE ) int *processedKeyLength, 
					IN_BUFFER( keyLength ) const void *key, 
					IN_LENGTH_SHORT const int keyLength )
	{
	BYTE hashBuffer[ HMAC_DATASIZE + 8 ], *keyPtr = processedKey;
	int i;

	assert( isWritePtr( hashState, sizeof( HASHINFO ) ) );
	assert( isWritePtr( processedKey, processedKeyMaxLength ) );
	assert( isWritePtr( processedKeyLength, sizeof( int ) ) );
	assert( isReadPtr( key, keyLength ) );

	REQUIRES( hashFunction != NULL && hashFunctionAtomic != NULL );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( processedKeyMaxLength == HMAC_DATASIZE );
	REQUIRES( keyLength > 0 && keyLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( processedKey, 0, min( 16, processedKeyMaxLength ) );
	*processedKeyLength = 0;

	/* If the key size is larger than the hash data size reduce it to the 
	   hash size before processing it (yuck.  You're required to do this
	   though) */
	if( keyLength > HMAC_DATASIZE )
		{
		/* Hash the user key down to the hash size and use the hashed form of
		   the key */
		hashFunctionAtomic( processedKey, processedKeyMaxLength, key, 
							keyLength );
		*processedKeyLength = hashSize;
		}
	else
		{
		/* Copy the key to internal storage */
		memcpy( processedKey, key, keyLength );
		*processedKeyLength = keyLength;
		}

	/* Perform the start of the inner hash using the zero-padded key XORed
	   with the ipad value */
	memset( hashBuffer, HMAC_IPAD, HMAC_DATASIZE );
	for( i = 0; i < *processedKeyLength; i++ )
		hashBuffer[ i ] ^= keyPtr[ i ];
	hashFunction( hashState, NULL, 0, hashBuffer, HMAC_DATASIZE, 
				  HASH_STATE_START );
	zeroise( hashBuffer, HMAC_DATASIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
static int prfEnd( IN const HASHFUNCTION hashFunction, 
				   INOUT TYPECAST( HASHINFO ) void *hashState,
				   IN_LENGTH_HASH const int hashSize, 
				   OUT_BUFFER_FIXED( hashMaxSize ) void *hash, 
				   IN_LENGTH_HASH const int hashMaxSize, 
				   IN_BUFFER( processedKeyLength ) const void *processedKey, 
				   IN_RANGE( 1, HMAC_DATASIZE ) const int processedKeyLength )
	{
	BYTE hashBuffer[ HMAC_DATASIZE + 8 ];
	BYTE digestBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int i;

	assert( isWritePtr( hashState, sizeof( HASHINFO ) ) );
	assert( isWritePtr( hash, hashMaxSize ) );
	assert( isReadPtr( processedKey, processedKeyLength ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( hashMaxSize >= 16 && hashMaxSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( processedKeyLength >= 1 && \
			  processedKeyLength <= HMAC_DATASIZE );

	/* Complete the inner hash and extract the digest */
	hashFunction( hashState, digestBuffer, CRYPT_MAX_HASHSIZE, NULL, 0, 
				  HASH_STATE_END );

	/* Perform the outer hash using the zero-padded key XORed with the opad
	   value followed by the digest from the inner hash */
	memset( hashBuffer, HMAC_OPAD, HMAC_DATASIZE );
	memcpy( hashBuffer, processedKey, processedKeyLength );
	for( i = 0; i < processedKeyLength; i++ )
		hashBuffer[ i ] ^= HMAC_OPAD;
	hashFunction( hashState, NULL, 0, hashBuffer, HMAC_DATASIZE, 
				  HASH_STATE_START );
	zeroise( hashBuffer, HMAC_DATASIZE );
	hashFunction( hashState, hash, hashMaxSize, digestBuffer, hashSize, 
				  HASH_STATE_END );
	zeroise( digestBuffer, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							PKCS #5v2 Key Derivation 						*
*																			*
****************************************************************************/

/* Implement one round of the PKCS #5v2 PRF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 8 ) ) \
static int pbkdf2Hash( OUT_BUFFER_FIXED( outLength ) BYTE *out, 
					   IN_RANGE( 1, CRYPT_MAX_HASHSIZE ) const int outLength, 
					   IN const HASHFUNCTION hashFunction, 
					   INOUT TYPECAST( HASHINFO ) void *initialHashState,
					   IN_LENGTH_HASH const int hashSize, 
					   IN_BUFFER( keyLength ) const void *key, 
					   IN_RANGE( 1, HMAC_DATASIZE ) const int keyLength,
					   IN_BUFFER( saltLength ) const void *salt, 
					   IN_RANGE( 4, 512 ) const int saltLength,
					   IN_INT const int iterations, 
					   IN_RANGE( 1, 1000 ) const int blockCount )
	{
	HASHINFO hashInfo;
	BYTE block[ CRYPT_MAX_HASHSIZE + 8 ], countBuffer[ 4 + 8 ];
	int i, status;

	assert( isWritePtr( out, outLength ) );
	assert( isWritePtr( initialHashState, sizeof( HASHINFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( salt, saltLength ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( outLength > 0 && outLength <= hashSize && \
			  outLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( keyLength >= 1 && keyLength <= HMAC_DATASIZE );
	REQUIRES( saltLength >= 4 && saltLength <= 512 );
	REQUIRES( iterations > 0 && iterations < MAX_INTLENGTH );
	REQUIRES( blockCount > 0 && blockCount <= 1000 );

	/* Clear return value */
	memset( out, 0, outLength );

	/* Set up the block counter buffer.  This will never have more than the
	   last few bits set (8 bits = 5100 bytes of key) so we only change the
	   last byte */
	memset( countBuffer, 0, 4 );
	countBuffer[ 3 ] = ( BYTE ) blockCount;

	/* Calculate HMAC( salt || counter ) */
	memcpy( hashInfo, initialHashState, sizeof( HASHINFO ) );
	hashFunction( hashInfo, NULL, 0, salt, saltLength, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, NULL, 0, countBuffer, 4, HASH_STATE_CONTINUE );
	status = prfEnd( hashFunction, hashInfo, hashSize, block, 
					 CRYPT_MAX_HASHSIZE, key, keyLength );
	if( cryptStatusError( status ) )
		{
		zeroise( hashInfo, sizeof( HASHINFO ) );
		return( status );
		}
	memcpy( out, block, outLength );

	/* Calculate HMAC( T1 ) ^ HMAC( T2 ) ^ ... HMAC( Tc ) */
	for( i = 0; i < iterations - 1 && i < FAILSAFE_ITERATIONS_MAX; i++ )
		{
		int j;

		/* Generate the PRF output for the current iteration */
		memcpy( hashInfo, initialHashState, sizeof( HASHINFO ) );
		hashFunction( hashInfo, NULL, 0, block, hashSize, HASH_STATE_CONTINUE );
		status = prfEnd( hashFunction, hashInfo, hashSize, block, 
						 CRYPT_MAX_HASHSIZE, key, keyLength );
		if( cryptStatusError( status ) )
			{
			zeroise( hashInfo, sizeof( HASHINFO ) );
			zeroise( block, CRYPT_MAX_HASHSIZE );
			return( status );
			}

		/* XOR the new PRF output into the existing PRF output */
		for( j = 0; j < outLength; j++ )
			out[ j ] ^= block[ j ];
		}
	ENSURES( i < FAILSAFE_ITERATIONS_MAX );

	zeroise( hashInfo, sizeof( HASHINFO ) );
	zeroise( block, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* Perform PKCS #5v2 derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePKCS5( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	CRYPT_ALGO_TYPE hashAlgo;
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	HASHFUNCTION hashFunction;
	HASHINFO initialHashInfo;
	BYTE processedKey[ HMAC_DATASIZE + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	static const MAP_TABLE mapTbl[] = {
		{ CRYPT_ALGO_HMAC_MD5, CRYPT_ALGO_MD5 },
		{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_SHA1 },
		{ CRYPT_ALGO_HMAC_RIPEMD160, CRYPT_ALGO_RIPEMD160 },
		{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_SHA2 },
		{ CRYPT_ALGO_HMAC_SHAng, CRYPT_ALGO_SHAng },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	int hashSize, keyIndex, processedKeyLength, blockCount = 1;
	int value, iterationCount, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	/* Convert the HMAC algorithm into the equivalent hash algorithm for use 
	   with the PRF */
	status = mapValue( mechanismInfo->hashAlgo, &value, mapTbl, 
					   FAILSAFE_ARRAYSIZE( mapTbl, MAP_TABLE ) );
	if( cryptStatusError( status ) )
		return( status );
	hashAlgo = value;
	if( !algoAvailable( hashAlgo ) )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Initialise the HMAC information with the user key.  Although the user
	   has specified the algorithm in terms of an HMAC we're synthesising it 
	   from the underlying hash algorithm since this allows us to perform the
	   PRF setup once and reuse the initial value for any future hashing */
	getHashAtomicParameters( hashAlgo, mechanismInfo->hashParam, 
							 &hashFunctionAtomic, &hashSize );
	getHashParameters( hashAlgo, mechanismInfo->hashParam, &hashFunction, 
					   NULL );
	status = prfInit( hashFunction, hashFunctionAtomic, initialHashInfo, 
					  hashSize, processedKey, HMAC_DATASIZE, 
					  &processedKeyLength, mechanismInfo->dataIn, 
					  mechanismInfo->dataInLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Produce enough blocks of output to fill the key (nil sine magno 
	   labore) */
	for( keyIndex = 0, iterationCount = 0; 
		 keyIndex < mechanismInfo->dataOutLength && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 	
		 keyIndex += hashSize, dataOutPtr += hashSize, iterationCount++ )
		{
		const int noKeyBytes = \
			( mechanismInfo->dataOutLength - keyIndex > hashSize ) ? \
			hashSize : mechanismInfo->dataOutLength - keyIndex;

		status = pbkdf2Hash( dataOutPtr, noKeyBytes, 
							 hashFunction, initialHashInfo, hashSize,
							 processedKey, processedKeyLength,
							 mechanismInfo->salt, mechanismInfo->saltLength,
							 mechanismInfo->iterations, blockCount++ );
		if( cryptStatusError( status ) )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( initialHashInfo, sizeof( HASHINFO ) );
	zeroise( processedKey, HMAC_DATASIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Apply PKCS #5v2 as a pure (single-round) KDF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int kdfPKCS5( STDC_UNUSED void *dummy, 
			  INOUT MECHANISM_KDF_INFO *mechanismInfo )
	{
	MECHANISM_DERIVE_INFO mechanismDeriveInfo;
	MESSAGE_DATA msgData;
	BYTE masterSecretBuffer[ CRYPT_MAX_KEYSIZE + 8 ];
	BYTE keyBuffer[ CRYPT_MAX_KEYSIZE + 8 ];
	int masterSecretSize, keySize = DUMMY_INIT, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Get the key payload details from the key contexts */
	status = krnlSendMessage( mechanismInfo->masterKeyContext, 
							  IMESSAGE_GETATTRIBUTE, &masterSecretSize,
							  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( mechanismInfo->keyContext, 
								  IMESSAGE_GETATTRIBUTE, &keySize,
								  CRYPT_CTXINFO_KEYSIZE );
		}
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( masterSecretSize > 0 && \
			 masterSecretSize <= CRYPT_MAX_KEYSIZE );

	/* Extract the master secret value from the generic-secret context and 
	   derive the key from it using PBKDF2 as the KDF */
	status = extractKeyData( mechanismInfo->masterKeyContext,
							 masterSecretBuffer, CRYPT_MAX_KEYSIZE, 
							 "keydata", 7 );
	if( cryptStatusError( status ) )
		return( status );
	setMechanismDeriveInfo( &mechanismDeriveInfo, keyBuffer, keySize,
							masterSecretBuffer, masterSecretSize,
							mechanismInfo->hashAlgo, mechanismInfo->salt,
							mechanismInfo->saltLength, 1 );
	mechanismDeriveInfo.hashParam = mechanismInfo->hashParam;
	status = derivePKCS5( NULL, &mechanismDeriveInfo );
	zeroise( masterSecretBuffer, CRYPT_MAX_KEYSIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( keyBuffer, CRYPT_MAX_KEYSIZE );
		return( status );
		}

	/* Load the derived key into the context */
	setMessageData( &msgData, keyBuffer, keySize );
	status = krnlSendMessage( mechanismInfo->keyContext, 
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	zeroise( keyBuffer, CRYPT_MAX_KEYSIZE );

	return( status );
	}

/****************************************************************************
*																			*
*							PKCS #12 Key Derivation 						*
*																			*
****************************************************************************/

#ifdef USE_PKCS12

/* The nominal block size for PKCS #12 derivation, based on the MD5/SHA-1 
   input size of 512 bits */

#define P12_BLOCKSIZE		64

/* The maximum size of the expanded diversifier, salt, and password (DSP),
   one block for the expanded diversifier, one block for the expanded salt,
   and up to three blocks for the password converted to Unicode with a
   terminating \x00 appended, which for a maximum password length of
   CRYPT_MAX_TEXTSIZE can be ( 64 + 1 ) * 2, rounded up to a multiple of
   P12_BLOCKSIZE */

#define P12_DSPSIZE			( P12_BLOCKSIZE + P12_BLOCKSIZE + \
							  ( P12_BLOCKSIZE * 3 ) )

/* Add two P12_BLOCKSIZE-byte blocks as 64-byte big-endian values:

	dest = (dest + src + 1) mod 2^P12_BLOCKSIZE */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int add64( INOUT_BUFFER_FIXED( destLen ) BYTE *dest,
				  IN_LENGTH_FIXED( P12_BLOCKSIZE ) const int destLen,
				  IN_BUFFER( srcLen ) const BYTE *src,
				  IN_LENGTH_FIXED( P12_BLOCKSIZE ) const int srcLen )
	{
	int destIndex, srcIndex, carry = 1;

	assert( isWritePtr( dest, destLen ) );
	assert( isReadPtr( src, srcLen ) );

	REQUIRES( destLen == P12_BLOCKSIZE );
	REQUIRES( srcLen == P12_BLOCKSIZE );

	/* dest = (dest + src + 1) mod 2^P12_BLOCKSIZE */
	for( destIndex = P12_BLOCKSIZE - 1, srcIndex = P12_BLOCKSIZE - 1;
		 destIndex >= 0; destIndex--, srcIndex-- )
		{
		const int value = dest[ destIndex ] + src[ srcIndex ] + carry;
		dest[ destIndex ] = intToByte( value & 0xFF );
		carry = value >> 8;
		}

	return( CRYPT_OK );
	}

/* Concantenate enough copies of the input data together to fill an output 
   buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int expandData( OUT_BUFFER_FIXED( destLen ) BYTE *dest, 
					   IN_LENGTH_SHORT const int destLen, 
					   IN_BUFFER( srcLen ) const BYTE *src, 
					   IN_LENGTH_SHORT const int srcLen )
	{
	int index, iterationCount;

	assert( isWritePtr( dest, destLen ) );
	assert( isReadPtr( src, srcLen ) );

	REQUIRES( destLen > 0 && destLen < MAX_INTLENGTH_SHORT );
	REQUIRES( srcLen > 0 && srcLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( dest, 0, min( 16, destLen ) );

	for( index = 0, iterationCount = 0;
		 index < destLen && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		const int bytesToCopy = min( srcLen, destLen - index );

		memcpy( dest, src, bytesToCopy );
		dest += bytesToCopy;
		index += bytesToCopy;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/* Build the diversifier/salt/password (DSP) string to use as the initial 
   input to the hash function:

	<---- 64 bytes ----><------- 64 bytes -------><-- Mult.64 bytes ->
	[ ID | ID | ID ... ][ salt | salt | salt ... ][ pw | pw | pw ... ] */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6 ) ) \
static int initDSP( OUT_BUFFER( dspMaxLen, *dspLen ) BYTE *dsp,
					IN_RANGE( P12_DSPSIZE, 512 ) const int dspMaxLen,
					OUT_RANGE( 0, 512 ) int *dspLen,
					IN_BUFFER( keyLength ) const BYTE *key,
					IN_LENGTH_TEXT const int keyLength,
					IN_BUFFER( saltLength ) const BYTE *salt,
					IN_RANGE( 1, P12_BLOCKSIZE ) const int saltLength,
					IN_RANGE( 1, 3 ) const int diversifier )
	{
	BYTE bmpString[ ( ( CRYPT_MAX_TEXTSIZE + 1 ) * 2 ) + 8 ];
	BYTE *dspPtr = dsp;
	int keyIndex, bmpIndex, i, status;

	assert( isWritePtr( dsp, dspMaxLen ) );
	assert( isWritePtr( dspLen, sizeof( int ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( salt, saltLength ) );

	REQUIRES( dspMaxLen >= P12_DSPSIZE && dspMaxLen <= 512 );
	REQUIRES( diversifier >= 1 && diversifier <= 3 );
	REQUIRES( saltLength >= 1 && saltLength <= P12_BLOCKSIZE );
	REQUIRES( keyLength >= MIN_NAME_LENGTH && \
			  keyLength <= CRYPT_MAX_TEXTSIZE );

	/* Clear return values */
	memset( dsp, 0, min( 16, dspMaxLen ) );
	*dspLen = 0;

	/* Set up the diversifier in the first P12_BLOCKSIZE bytes */
	for( i = 0; i < P12_BLOCKSIZE; i++ )
		dsp[ i ] = intToByte( diversifier );
	dspPtr += P12_BLOCKSIZE;

	/* Set up the salt in the next P12_BLOCKSIZE bytes */
	status = expandData( dspPtr, P12_BLOCKSIZE, salt, saltLength );
	if( cryptStatusError( status ) )
		return( status );
	dspPtr += P12_BLOCKSIZE;

	/* Convert the password to a null-terminated Unicode string, a Microsoft
	   bug that was made part of the standard */
	for( keyIndex = 0, bmpIndex = 0; 
		 keyIndex < keyLength && keyIndex < CRYPT_MAX_TEXTSIZE; 
		 keyIndex++, bmpIndex += 2 )
		{
		bmpString[ bmpIndex ] = '\0';
		bmpString[ bmpIndex + 1 ] = key[ keyIndex ];
		}
	ENSURES( keyIndex < CRYPT_MAX_TEXTSIZE );
	bmpString[ bmpIndex++ ] = '\0';
	bmpString[ bmpIndex++ ] = '\0';
	ENSURES( dspMaxLen >= P12_BLOCKSIZE + P12_BLOCKSIZE + \
						  roundUp( bmpIndex, P12_BLOCKSIZE ) );

	/* Set up the Unicode password string as the remaining bytes */
	status = expandData( dspPtr, roundUp( bmpIndex, P12_BLOCKSIZE ),
						 bmpString, bmpIndex );
	zeroise( bmpString, ( CRYPT_MAX_TEXTSIZE + 1 ) * 2 );
	if( cryptStatusError( status ) )
		{
		zeroise( dsp, dspMaxLen );
		return( status );
		}
	*dspLen = P12_BLOCKSIZE + P12_BLOCKSIZE + \
			  roundUp( bmpIndex, P12_BLOCKSIZE );
	
	return( CRYPT_OK );
	}

/* Perform PKCS #12 derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePKCS12( STDC_UNUSED void *dummy, 
				  INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE p12_DSP[ P12_DSPSIZE + 8 ];
	BYTE p12_Ai[ CRYPT_MAX_HASHSIZE + 8 ], p12_B[ P12_BLOCKSIZE + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	const BYTE *saltPtr = mechanismInfo->salt;
	int dspLen, hashSize, keyIndex, i, iterationCount, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	getHashAtomicParameters( mechanismInfo->hashAlgo, 0, &hashFunctionAtomic, 
							 &hashSize );

	/* Set up the diversifier/salt/password (DSP) string.  The first byte of 
	   the PKCS #12 salt acts as a diversifier so we separate this out from 
	   the rest of the salt data */
	status = initDSP( p12_DSP, P12_DSPSIZE, &dspLen, mechanismInfo->dataIn,
					  mechanismInfo->dataInLength, saltPtr + 1,
					  mechanismInfo->saltLength - 1, byteToInt( *saltPtr ) );
	if( cryptStatusError( status ) )
		return( status );

	/* Produce enough blocks of output to fill the key */
	for( keyIndex = 0, iterationCount = 0; 
		 keyIndex < mechanismInfo->dataOutLength && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 
		 keyIndex += hashSize, iterationCount++ )
		{
		const int noKeyBytes = \
			( mechanismInfo->dataOutLength - keyIndex > hashSize ) ? \
			hashSize : mechanismInfo->dataOutLength - keyIndex;
		int dspIndex;

		/* Hash the keying material the required number of times to obtain the
		   output value */
		hashFunctionAtomic( p12_Ai, CRYPT_MAX_HASHSIZE, p12_DSP, dspLen );
		for( i = 1; i < mechanismInfo->iterations && \
					i < FAILSAFE_ITERATIONS_MAX; i++ )
			{
			hashFunctionAtomic( p12_Ai, CRYPT_MAX_HASHSIZE, 
								p12_Ai, hashSize );
			}
		ENSURES( i < FAILSAFE_ITERATIONS_MAX );
		memcpy( dataOutPtr + keyIndex, p12_Ai, noKeyBytes );

		/* Update the input keying material for the next iteration by 
		   adding the output value expanded to P12_BLOCKSIZE, to 
		   P12_BLOCKSIZE-sized blocks of the salt/password portion of the 
		   DSP string */
		status = expandData( p12_B, P12_BLOCKSIZE, p12_Ai, hashSize );
		if( cryptStatusError( status ) )
			break;
		for( dspIndex = P12_BLOCKSIZE; dspIndex < dspLen; 
			 dspIndex += P12_BLOCKSIZE )
			{
			status = add64( p12_DSP + dspIndex, P12_BLOCKSIZE,
							p12_B, P12_BLOCKSIZE );
			if( cryptStatusError( status ) )
				break;
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( p12_DSP, P12_DSPSIZE );
	zeroise( p12_Ai, CRYPT_MAX_HASHSIZE );
	zeroise( p12_B, P12_BLOCKSIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_PKCS12 */

/****************************************************************************
*																			*
*							SSL/TLS Key Derivation 							*
*																			*
****************************************************************************/

#ifdef USE_SSL

/* Structure used to store TLS PRF state information */

typedef struct {
	/* The hash functions and hash info */
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	HASHFUNCTION hashFunction;
	int hashSize;

	/* The initial hash state from prfInit() and the current hash state */
	HASHINFO initialHashInfo, hashInfo;

	/* The HMAC processed key and intermediate data value */
	BUFFER( HMAC_DATASIZE, processedKeyLength ) \
	BYTE processedKey[ HMAC_DATASIZE + 8 ];
	BUFFER( HMAC_DATASIZE, hashSize ) \
	BYTE hashA[ CRYPT_MAX_HASHSIZE + 8 ];
	int processedKeyLength;
	} TLS_PRF_INFO;

/* Perform SSL key derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveSSL( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	HASHFUNCTION md5HashFunction, shaHashFunction;
	HASHINFO hashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ], counterData[ 16 + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	int md5HashSize, shaHashSize, counter = 0, keyIndex, iterationCount;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	getHashParameters( CRYPT_ALGO_MD5, 0, &md5HashFunction, &md5HashSize );
	getHashParameters( CRYPT_ALGO_SHA1, 0, &shaHashFunction, &shaHashSize );

	/* Produce enough blocks of output to fill the key */
	for( keyIndex = 0, iterationCount = 0; 
		 keyIndex < mechanismInfo->dataOutLength && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 	
		 keyIndex += md5HashSize, iterationCount++ )
		{
		const int noKeyBytes = \
			( mechanismInfo->dataOutLength - keyIndex > md5HashSize ) ? \
			md5HashSize : mechanismInfo->dataOutLength - keyIndex;
		int i;

		/* Set up the counter data */
		for( i = 0; i <= counter && i < 16; i++ )
			counterData[ i ] = intToByte( 'A' + counter );
		ENSURES( i < 16 );
		counter++;

		/* Calculate SHA1( 'A'/'BB'/'CCC'/... || keyData || salt ) */
		shaHashFunction( hashInfo, NULL, 0, counterData, counter, 
						 HASH_STATE_START );
		shaHashFunction( hashInfo, NULL, 0, mechanismInfo->dataIn,
						 mechanismInfo->dataInLength, HASH_STATE_CONTINUE );
		shaHashFunction( hashInfo, hash, CRYPT_MAX_HASHSIZE, 
						 mechanismInfo->salt, mechanismInfo->saltLength, 
						 HASH_STATE_END );

		/* Calculate MD5( keyData || SHA1-hash ) */
		md5HashFunction( hashInfo, NULL, 0, mechanismInfo->dataIn,
						 mechanismInfo->dataInLength, HASH_STATE_START );
		md5HashFunction( hashInfo, hash, CRYPT_MAX_HASHSIZE,
						 hash, shaHashSize, HASH_STATE_END );

		/* Copy the result to the output */
		memcpy( dataOutPtr + keyIndex, hash, noKeyBytes );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( hashInfo, sizeof( HASHINFO ) );
	zeroise( hash, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* Initialise the TLS PRF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int tlsPrfInit( INOUT TLS_PRF_INFO *prfInfo, 
					   IN_BUFFER( keyLength ) const void *key, 
					   IN_LENGTH_SHORT const int keyLength,
					   IN_BUFFER( saltLength ) const void *salt, 
					   IN_LENGTH_SHORT const int saltLength )
	{
	int status;

	assert( isWritePtr( prfInfo, sizeof( TLS_PRF_INFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( salt, saltLength ) );

	REQUIRES( keyLength > 0 && keyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( saltLength > 0 && saltLength < MAX_INTLENGTH_SHORT );

	/* Initialise the hash information with the keying info.  This is
	   reused for any future hashing since it's constant */
	status = prfInit( prfInfo->hashFunction, prfInfo->hashFunctionAtomic, 
					  prfInfo->initialHashInfo, prfInfo->hashSize, 
					  prfInfo->processedKey, HMAC_DATASIZE, 
					  &prfInfo->processedKeyLength, key, keyLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate A1 = HMAC( salt ) */
	memcpy( prfInfo->hashInfo, prfInfo->initialHashInfo, sizeof( HASHINFO ) );
	prfInfo->hashFunction( prfInfo->hashInfo, NULL, 0, salt, saltLength, 
						  HASH_STATE_CONTINUE );
	return( prfEnd( prfInfo->hashFunction, prfInfo->hashInfo, 
					prfInfo->hashSize, prfInfo->hashA,
					CRYPT_MAX_HASHSIZE, prfInfo->processedKey, 
					prfInfo->processedKeyLength ) );
	}

/* Implement one round of the TLS PRF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int tlsPrfHash( OUT_BUFFER_FIXED( outLength ) BYTE *out, 
					   IN_LENGTH_SHORT const int outLength, 
					   INOUT TLS_PRF_INFO *prfInfo, 
					   IN_BUFFER( saltLength ) const void *salt, 
					   IN_LENGTH_SHORT const int saltLength )
	{
	HASHINFO hashInfo, AnHashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int i, status;

	assert( isWritePtr( out, outLength ) );
	assert( isWritePtr( prfInfo, sizeof( TLS_PRF_INFO ) ) );
	assert( isReadPtr( salt, saltLength ) );

	REQUIRES( outLength > 0 && outLength <= prfInfo->hashSize && \
			  outLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( saltLength >= 13 && saltLength <= 512 );

	/* The result of the hashing is XORd to the output so we don't clear the 
	   return value like usual */

	/* Calculate HMAC( An || salt ) */
	memcpy( hashInfo, prfInfo->initialHashInfo, sizeof( HASHINFO ) );
	prfInfo->hashFunction( hashInfo, NULL, 0, prfInfo->hashA, 
						   prfInfo->hashSize, HASH_STATE_CONTINUE );
	memcpy( AnHashInfo, hashInfo, sizeof( HASHINFO ) );
	prfInfo->hashFunction( hashInfo, NULL, 0, salt, saltLength, 
						   HASH_STATE_CONTINUE );
	status = prfEnd( prfInfo->hashFunction, hashInfo, prfInfo->hashSize, 
					 hash, CRYPT_MAX_HASHSIZE, prfInfo->processedKey, 
					 prfInfo->processedKeyLength );
	if( cryptStatusError( status ) )
		{
		zeroise( AnHashInfo, sizeof( HASHINFO ) );
		zeroise( hashInfo, sizeof( HASHINFO ) );
		zeroise( hash, CRYPT_MAX_HASHSIZE );
		return( status );
		}

	/* Calculate An+1 = HMAC( An ) */
	memcpy( hashInfo, AnHashInfo, sizeof( HASHINFO ) );
	status = prfEnd( prfInfo->hashFunction, hashInfo, prfInfo->hashSize, 
					 prfInfo->hashA, prfInfo->hashSize, 
					 prfInfo->processedKey, prfInfo->processedKeyLength );
	if( cryptStatusError( status ) )
		{
		zeroise( AnHashInfo, sizeof( HASHINFO ) );
		zeroise( hashInfo, sizeof( HASHINFO ) );
		zeroise( hash, CRYPT_MAX_HASHSIZE );
		return( status );
		}

	/* Copy the result to the output */
	for( i = 0; i < outLength; i++ )
		out[ i ] ^= hash[ i ];

	zeroise( AnHashInfo, sizeof( HASHINFO ) );
	zeroise( hashInfo, sizeof( HASHINFO ) );
	zeroise( hash, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* Perform TLS key derivation.  This implements the function described as 
   'PRF()' in the TLS spec */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveTLS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	TLS_PRF_INFO md5Info, shaInfo;
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	const void *s1, *s2;
	const int dataOutLength = mechanismInfo->dataOutLength;
	const int sLen = ( mechanismInfo->dataInLength + 1 ) / 2;
	int md5Index, shaIndex, iterationCount, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	memset( &md5Info, 0, sizeof( TLS_PRF_INFO ) );
	getHashAtomicParameters( CRYPT_ALGO_MD5, 0, &md5Info.hashFunctionAtomic, 
							 &md5Info.hashSize );
	getHashParameters( CRYPT_ALGO_MD5, 0, &md5Info.hashFunction, NULL );
	memset( &shaInfo, 0, sizeof( TLS_PRF_INFO ) );
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &shaInfo.hashFunctionAtomic, 
							 &shaInfo.hashSize );
	getHashParameters( CRYPT_ALGO_SHA1, 0, &shaInfo.hashFunction, NULL );

	/* Find the start of the two halves of the keying info used for the
	   HMACing.  The size of each half is given by ceil( dataInLength / 2 ) 
	   so there's a one-byte overlap if the input is an odd number of bytes 
	   long */
	s1 = mechanismInfo->dataIn;
	s2 = ( BYTE * ) mechanismInfo->dataIn + \
		 ( mechanismInfo->dataInLength - sLen );

	/* The two hash functions have different block sizes that would require
	   complex buffering to handle leftover bytes from SHA-1, a simpler
	   method is to zero the output data block and XOR in the values from
	   each hash mechanism using separate output location indices for MD5 and
	   SHA-1 */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	/* Initialise the TLS PRF and calculate A1 = HMAC( salt ) */
	status = tlsPrfInit( &md5Info, s1, sLen, mechanismInfo->salt,
						 mechanismInfo->saltLength );
	if( cryptStatusOK( status ) )
		status = tlsPrfInit( &shaInfo, s2, sLen, mechanismInfo->salt,
							 mechanismInfo->saltLength );
	if( cryptStatusError( status ) )
		{
		zeroise( &md5Info, sizeof( TLS_PRF_INFO ) );
		zeroise( &shaInfo, sizeof( TLS_PRF_INFO ) );
		return( status );
		}

	/* Produce enough blocks of output to fill the key.  We use the MD5 hash
	   size as the loop increment since this produces the smaller output
	   block */
	for( md5Index = shaIndex = 0, iterationCount = 0; 
		 md5Index < dataOutLength && iterationCount < FAILSAFE_ITERATIONS_MED; 	
		 iterationCount++ )
		{
		const int md5NoKeyBytes = min( dataOutLength - md5Index, \
									   md5Info.hashSize );
		const int shaNoKeyBytes = min( dataOutLength - shaIndex, \
									   shaInfo.hashSize );

		status = tlsPrfHash( dataOutPtr + md5Index, md5NoKeyBytes, 
							 &md5Info, mechanismInfo->salt, 
							 mechanismInfo->saltLength );
		if( cryptStatusError( status ) )
			break;
		if( shaNoKeyBytes > 0 )
			{
			/* Since the SHA-1 counter advances faster than the MD5 one we
			   can end up with zero bytes left to process for SHA-1 when MD5 
			   is processing it's last block */
			status = tlsPrfHash( dataOutPtr + shaIndex, shaNoKeyBytes, 
								 &shaInfo, mechanismInfo->salt, 
								 mechanismInfo->saltLength );
			if( cryptStatusError( status ) )
				break;
			}

		md5Index += md5NoKeyBytes;
		shaIndex += shaNoKeyBytes;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( &md5Info, sizeof( TLS_PRF_INFO ) );
	zeroise( &shaInfo, sizeof( TLS_PRF_INFO ) );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Perform TLS 1.2 key derivation using a gratuitously incompatible PRF 
   invented for TLS 1.2 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveTLS12( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	TLS_PRF_INFO shaInfo;
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	const int dataOutLength = mechanismInfo->dataOutLength;
	int keyIndex, iterationCount, status;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	memset( &shaInfo, 0, sizeof( TLS_PRF_INFO ) );
	getHashAtomicParameters( mechanismInfo->hashAlgo, 
							 mechanismInfo->hashParam,
							 &shaInfo.hashFunctionAtomic, 
							 &shaInfo.hashSize );
	getHashParameters( mechanismInfo->hashAlgo, mechanismInfo->hashParam,
					   &shaInfo.hashFunction, NULL );

	/* Initialise the TLS PRF and calculate A1 = HMAC( salt ) */
	status = tlsPrfInit( &shaInfo, mechanismInfo->dataIn, 
						 mechanismInfo->dataInLength, mechanismInfo->salt,
						 mechanismInfo->saltLength );
	if( cryptStatusError( status ) )
		{
		zeroise( &shaInfo, sizeof( TLS_PRF_INFO ) );
		return( status );
		}

	/* Produce enough blocks of output to fill the key */
	for( keyIndex = 0, iterationCount = 0; 
		 keyIndex < dataOutLength && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 
		 keyIndex += shaInfo.hashSize, iterationCount++ )
		{
		const int noKeyBytes = min( dataOutLength - keyIndex, \
									shaInfo.hashSize );

		status = tlsPrfHash( dataOutPtr + keyIndex, noKeyBytes, &shaInfo, 
							 mechanismInfo->salt, mechanismInfo->saltLength );
		if( cryptStatusError( status ) )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	zeroise( &shaInfo, sizeof( TLS_PRF_INFO ) );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_SSL */

/****************************************************************************
*																			*
*							PGP Key Derivation 								*
*																			*
****************************************************************************/

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

/* Implement one round of the OpenPGP PRF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6, 8, 10 ) ) \
static int pgpPrfHash( OUT_BUFFER_FIXED( outLength ) BYTE *out, 
					   IN_LENGTH_HASH const int outLength, 
					   IN const HASHFUNCTION hashFunction, 
					   INOUT TYPECAST( HASHINFO ) void *hashInfo,
					   IN_LENGTH_HASH const int hashSize, 
					   IN_BUFFER( keyLength ) const void *key, 
					   IN_LENGTH_SHORT_MIN( 2 ) const int keyLength,
					   IN_BUFFER( saltLength ) const void *salt, 
					   IN_LENGTH_FIXED( PGP_SALTSIZE ) const int saltLength,
					   INOUT_LENGTH_Z long *byteCount, 
					   IN_RANGE( CRYPT_UNUSED, 1 ) const int preloadLength )
	{
	long count = *byteCount;

	assert( isWritePtr( out, outLength ) );
	assert( isWritePtr( hashInfo, sizeof( HASHINFO ) ) );
	assert( isReadPtr( key, keyLength ) );
	assert( isReadPtr( salt, saltLength ) );
	assert( isWritePtr( byteCount, sizeof( int ) ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( outLength == hashSize );
	REQUIRES( hashSize >= 16 && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( keyLength >= 2 && keyLength <= MAX_INTLENGTH_SHORT );
	REQUIRES( saltLength == PGP_SALTSIZE );
	REQUIRES( preloadLength == CRYPT_UNUSED || \
			  ( preloadLength >= 0 && preloadLength <= 1 ) );
	REQUIRES( count > 0 && count < MAX_INTLENGTH );

	/* Clear return value */
	memset( out, 0, outLength );

	/* If it's a subsequent round of hashing, preload the hash with zero 
	   bytes.  If it's the first round (preloadLength == 0) it's handled
	   specially below */
	if( preloadLength > 0 )
		{
		hashFunction( hashInfo, NULL, 0, ( const BYTE * ) "\x00\x00\x00\x00", 
					  preloadLength, HASH_STATE_START );
		}

	/* Hash the next round of salt || password.  Since we're being asked to 
	   stop once we've processed 'count' input bytes we implement an early-
	   out mechanism that exits if the length of the item being hashed is 
	   sufficient to reach 'count' */
	if( count <= saltLength )
		{
		hashFunction( hashInfo, out, outLength, salt, count, 
					  HASH_STATE_END );
		*byteCount = 0;
		
		return( CRYPT_OK );
		}
	hashFunction( hashInfo, NULL, 0, salt, saltLength, 
				  ( preloadLength == 0 ) ? HASH_STATE_START : \
										   HASH_STATE_CONTINUE );
	count -= saltLength;
	if( count <= keyLength )
		{
		hashFunction( hashInfo, out, outLength, key, count, HASH_STATE_END );
		*byteCount = 0;

		return( CRYPT_OK );
		}
	hashFunction( hashInfo, NULL, 0, key, keyLength, HASH_STATE_CONTINUE );
	count -= keyLength;
	ENSURES( count > 0 && count < MAX_INTLENGTH );

	*byteCount = count;
	return( CRYPT_OK );
	}

/* Perform OpenPGP S2K key derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePGP( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	long byteCount = ( long ) mechanismInfo->iterations << 6;
	long secondByteCount = 0;
	int hashSize, i, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	REQUIRES( mechanismInfo->iterations >= 0 && \
			  mechanismInfo->iterations <= MAX_KEYSETUP_HASHSPECIFIER );
	REQUIRES( byteCount >= 0 && byteCount < MAX_INTLENGTH );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	getHashParameters( mechanismInfo->hashAlgo, 0, &hashFunction, 
					   &hashSize );

	REQUIRES( mechanismInfo->dataOutLength < 2 * hashSize );

	/* If it's a non-iterated hash or the count won't allow even a single
	   pass over the 8-byte salt and password, adjust it to make sure that 
	   we run at least one full iteration */
	if( byteCount < PGP_SALTSIZE + mechanismInfo->dataInLength )
		byteCount = PGP_SALTSIZE + mechanismInfo->dataInLength;

	/* If the hash output size is less than the required key size we run a 
	   second round of hashing after the first one to provide the total 
	   required amount of keying material */
	if( hashSize < mechanismInfo->dataOutLength )
		secondByteCount = byteCount;

	/* Repeatedly hash the salt and password until we've met the byte count.  
	   In effect this hashes:

		salt || password || salt || password || ...

	   until we've processed 'byteCount' bytes of data.

	   This processing is complicated by the ridiculous number of iterations
	   of processing specified by some versions of GPG (see the long comment
	   in misc/consts.h), so that we can no longer employ the standard
	   FAILSAFE_ITERATIONS_MAX as a failsafe value but have to use a 
	   multiple of that.  There's no clean way to do this, the following
	   hardcodes an upper bound that'll have to be varied based on what
	   MAX_KEYSETUP_HASHSPECIFIER is set to */
	#define GPG_FAILSAFE_ITERATIONS_MAX	( 3 * FAILSAFE_ITERATIONS_MAX )
	static_assert( MAX_KEYSETUP_HASHSPECIFIER < GPG_FAILSAFE_ITERATIONS_MAX,
				   "Failsafe value for PGP hashing" );
	for( i = 0, iterationCount = 0;
		 byteCount > 0 && cryptStatusOK( status ) && \
			iterationCount < GPG_FAILSAFE_ITERATIONS_MAX;
		 i++, iterationCount++ )
		{
		status = pgpPrfHash( mechanismInfo->dataOut, 
							 hashSize, hashFunction, hashInfo, hashSize, 
							 mechanismInfo->dataIn,
							 mechanismInfo->dataInLength,
							 mechanismInfo->salt, 
							 mechanismInfo->saltLength, &byteCount, 
							 ( i <= 0 ) ? 0 : CRYPT_UNUSED );
		}
	ENSURES( iterationCount < GPG_FAILSAFE_ITERATIONS_MAX );
	if( cryptStatusOK( status ) && secondByteCount > 0 )
		{
		for( i = 0, iterationCount = 0;
			 secondByteCount > 0 && cryptStatusOK( status ) && \
				iterationCount < GPG_FAILSAFE_ITERATIONS_MAX;
			 i++, iterationCount++ )
			{
			status = pgpPrfHash( ( BYTE * ) mechanismInfo->dataOut + hashSize, 
								 hashSize, hashFunction, hashInfo, hashSize, 
								 mechanismInfo->dataIn, 
								 mechanismInfo->dataInLength,
								 mechanismInfo->salt, 
								 mechanismInfo->saltLength, &secondByteCount, 
								 ( i <= 0 ) ? 1 : CRYPT_UNUSED );
			}
		ENSURES( iterationCount < GPG_FAILSAFE_ITERATIONS_MAX );
		}
	zeroise( hashInfo, sizeof( HASHINFO ) );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}
#endif /* USE_PGP || USE_PGPKEYS */

/****************************************************************************
*																			*
*								Misc Key Derivation 						*
*																			*
****************************************************************************/

#ifdef USE_CMP

/* Perform CMP/Entrust key derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveCMP( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	int hashSize, iterations;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	/* Calculate SHA1( password || salt ) */
	getHashAtomicParameters( mechanismInfo->hashAlgo, 0, 
							 &hashFunctionAtomic, &hashSize );
	getHashParameters( mechanismInfo->hashAlgo, 0, &hashFunction, NULL );
	hashFunction( hashInfo, NULL, 0, mechanismInfo->dataIn,
				  mechanismInfo->dataInLength, HASH_STATE_START );
	hashFunction( hashInfo, mechanismInfo->dataOut, 
				  mechanismInfo->dataOutLength, mechanismInfo->salt,
				  mechanismInfo->saltLength, HASH_STATE_END );

	/* Iterate the hashing the remaining number of times.  We start the 
	   count at one since the first iteration has already been performed */
	for( iterations = 1; iterations < mechanismInfo->iterations && \
						 iterations < FAILSAFE_ITERATIONS_MAX; iterations++ )
		{
		hashFunctionAtomic( mechanismInfo->dataOut, 
							mechanismInfo->dataOutLength, 
							mechanismInfo->dataOut, hashSize );
		}
	ENSURES( iterations < FAILSAFE_ITERATIONS_MAX );
	zeroise( hashInfo, sizeof( HASHINFO ) );

	return( CRYPT_OK );
	}
#endif /* USE_CMP */
