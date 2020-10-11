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

/* HMAC-based PRF used for PBKDF2 / PKCS #5v2 and TLS */

#define HMAC_DATASIZE		64

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 7, 8 ) ) \
static int prfInit( IN const HASH_FUNCTION hashFunction, 
					IN const HASH_FUNCTION_ATOMIC hashFunctionAtomic,
					OUT TYPECAST( HASHINFO ) void *hashState, 
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
	int i, LOOP_ITERATOR;

	assert( isWritePtr( hashState, sizeof( HASHINFO ) ) );
	assert( isWritePtrDynamic( processedKey, processedKeyMaxLength ) );
	assert( isWritePtr( processedKeyLength, sizeof( int ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( hashFunction != NULL && hashFunctionAtomic != NULL );
	REQUIRES( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( processedKeyMaxLength == HMAC_DATASIZE );
	REQUIRES( isShortIntegerRangeNZ( keyLength ) );

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
		/* Copy the key to internal storage.  Note that this has the 
		   potential to leak a tiny amount of timing information about the
		   key length, but given the mass of operations that follow this is
		   unlikely to be significant */
		REQUIRES( rangeCheck( keyLength, 1, processedKeyMaxLength ) );
		memcpy( processedKey, key, keyLength );
		*processedKeyLength = keyLength;
		}

	/* Perform the start of the inner hash using the zero-padded key XORed
	   with the ipad value.  This could be done slightly more efficiently, 
	   but the following sequence of operations minimises timing channels 
	   leaking the key length */
	REQUIRES( rangeCheck( *processedKeyLength, 1, HMAC_DATASIZE ) );
	memcpy( hashBuffer, keyPtr, *processedKeyLength );
	if( *processedKeyLength < HMAC_DATASIZE )
		{
		memset( hashBuffer + *processedKeyLength, 0, 
				HMAC_DATASIZE - *processedKeyLength );
		}
	LOOP_EXT( i = 0, i < HMAC_DATASIZE, i++, HMAC_DATASIZE + 1 )
		hashBuffer[ i ] ^= HMAC_IPAD;
	ENSURES( LOOP_BOUND_OK );
	hashFunction( hashState, NULL, 0, hashBuffer, HMAC_DATASIZE, 
				  HASH_STATE_START );
	zeroise( hashBuffer, HMAC_DATASIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 6 ) ) \
static int prfEnd( IN const HASH_FUNCTION hashFunction, 
				   INOUT TYPECAST( HASHINFO ) void *hashState,
				   IN_LENGTH_HASH const int hashSize, 
				   OUT_BUFFER_FIXED( hashMaxSize ) void *hash, 
				   IN_LENGTH_HASH const int hashMaxSize, 
				   IN_BUFFER( processedKeyLength ) const void *processedKey, 
				   IN_RANGE( 1, HMAC_DATASIZE ) const int processedKeyLength )
	{
	BYTE hashBuffer[ HMAC_DATASIZE + 8 ];
	BYTE digestBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int i, LOOP_ITERATOR;

	assert( isWritePtr( hashState, sizeof( HASHINFO ) ) );
	assert( isWritePtrDynamic( hash, hashMaxSize ) );
	assert( isReadPtrDynamic( processedKey, processedKeyLength ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( hashMaxSize >= MIN_HASHSIZE && \
			  hashMaxSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( processedKeyLength >= 1 && \
			  processedKeyLength <= HMAC_DATASIZE );

	/* Complete the inner hash and extract the digest */
	hashFunction( hashState, digestBuffer, CRYPT_MAX_HASHSIZE, NULL, 0, 
				  HASH_STATE_END );

	/* Perform the outer hash using the zero-padded key XORed with the opad
	   value followed by the digest from the inner hash.  As with the init
	   function this could be done slightly more efficiently, but the 
	   following sequence of operations minimises timing channels leaking 
	   the key length */
	REQUIRES( rangeCheck( processedKeyLength, 1, HMAC_DATASIZE ) );
	memcpy( hashBuffer, processedKey, processedKeyLength );
	if( processedKeyLength < HMAC_DATASIZE )
		{
		memset( hashBuffer + processedKeyLength, 0, 
				HMAC_DATASIZE - processedKeyLength );
		}
	LOOP_EXT( i = 0, i < HMAC_DATASIZE, i++, HMAC_DATASIZE + 1 )
		hashBuffer[ i ] ^= HMAC_OPAD;
	ENSURES( LOOP_BOUND_OK );
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
					   IN const HASH_FUNCTION hashFunction, 
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
	int i, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( out, outLength ) );
	assert( isWritePtr( initialHashState, sizeof( HASHINFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtrDynamic( salt, saltLength ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( outLength > 0 && outLength <= hashSize && \
			  outLength <= CRYPT_MAX_HASHSIZE );
	REQUIRES( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( keyLength >= 1 && keyLength <= HMAC_DATASIZE );
	REQUIRES( saltLength >= 4 && saltLength <= 512 );
	REQUIRES( isIntegerRangeNZ( iterations ) );
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
	LOOP_MAX( i = 0, i < iterations - 1, i++ )
		{
		int j, LOOP_ITERATOR_ALT;

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
		LOOP_EXT_ALT( j = 0, j < outLength, j++, CRYPT_MAX_HASHSIZE + 1 )
			out[ j ] ^= block[ j ];
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );

	zeroise( hashInfo, sizeof( HASHINFO ) );
	zeroise( block, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* Perform PBKDF2 / PKCS #5v2 derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePBKDF2( STDC_UNUSED void *dummy, 
				  INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	CRYPT_ALGO_TYPE hashAlgo;
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	HASH_FUNCTION hashFunction;
	HASHINFO initialHashInfo;
	BYTE processedKey[ HMAC_DATASIZE + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	static const MAP_TABLE mapTbl[] = {
		{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_SHA1 },
		{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_SHA2 },
		{ CRYPT_ALGO_HMAC_SHAng, CRYPT_ALGO_SHAng },
		{ CRYPT_ERROR, CRYPT_ERROR }, { CRYPT_ERROR, CRYPT_ERROR }
		};
	int hashSize, keyIndex, processedKeyLength, blockCount = 1;
	int value, status, LOOP_ITERATOR;

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
	LOOP_MED( keyIndex = 0, keyIndex < mechanismInfo->dataOutLength,
			  ( keyIndex += hashSize, dataOutPtr += hashSize ) )
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
	ENSURES( LOOP_BOUND_OK );
	zeroise( initialHashInfo, sizeof( HASHINFO ) );
	zeroise( processedKey, HMAC_DATASIZE );
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->dataOut, mechanismInfo->dataOutLength );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Apply PBKDF2 / PKCS #5v2 as a pure (single-round) KDF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int kdfPBKDF2( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_KDF_INFO *mechanismInfo )
	{
	MECHANISM_DERIVE_INFO mechanismDeriveInfo;
	MESSAGE_DATA msgData;
	BYTE masterSecretBuffer[ CRYPT_MAX_KEYSIZE + 8 ];
	BYTE keyBuffer[ CRYPT_MAX_KEYSIZE + 8 ];
	int masterSecretSize, keySize DUMMY_INIT, status;

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
	status = derivePBKDF2( NULL, &mechanismDeriveInfo );
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
	int destIndex, srcIndex, carry = 1, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destLen ) );
	assert( isReadPtrDynamic( src, srcLen ) );

	REQUIRES( destLen == P12_BLOCKSIZE );
	REQUIRES( srcLen == P12_BLOCKSIZE );

	/* dest = (dest + src + 1) mod 2^P12_BLOCKSIZE */
	LOOP_EXT( ( destIndex = P12_BLOCKSIZE - 1, srcIndex = P12_BLOCKSIZE - 1 ),
			  destIndex >= 0, ( destIndex--, srcIndex-- ), P12_BLOCKSIZE + 1 )
		{
		const int value = dest[ destIndex ] + src[ srcIndex ] + carry;
		dest[ destIndex ] = intToByte( value & 0xFF );
		carry = value >> 8;
		}
	ENSURES( LOOP_BOUND_OK );

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
	int index, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destLen ) );
	assert( isReadPtrDynamic( src, srcLen ) );

	REQUIRES( isShortIntegerRangeNZ( destLen ) );
	REQUIRES( isShortIntegerRangeNZ( srcLen ) );

	/* Clear return value */
	memset( dest, 0, min( 16, destLen ) );

	LOOP_MED_INITCHECK( index = 0, index < destLen )
		{
		const int bytesToCopy = min( srcLen, destLen - index );

		REQUIRES( boundsCheckZ( index, bytesToCopy, destLen ) );
		memcpy( dest, src, bytesToCopy );
		dest += bytesToCopy;
		index += bytesToCopy;
		}
	ENSURES( LOOP_BOUND_OK );

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
	int keyIndex, bmpIndex, i, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dsp, dspMaxLen ) );
	assert( isWritePtr( dspLen, sizeof( int ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtrDynamic( salt, saltLength ) );

	REQUIRES( dspMaxLen >= P12_DSPSIZE && dspMaxLen <= 512 );
	REQUIRES( diversifier >= 1 && diversifier <= 3 );
	REQUIRES( saltLength >= 1 && saltLength <= P12_BLOCKSIZE );
	REQUIRES( keyLength >= MIN_NAME_LENGTH && \
			  keyLength <= CRYPT_MAX_TEXTSIZE );

	/* Clear return values */
	memset( dsp, 0, min( 16, dspMaxLen ) );
	*dspLen = 0;

	/* Set up the diversifier in the first P12_BLOCKSIZE bytes */
	LOOP_EXT( i = 0, i < P12_BLOCKSIZE, i++, P12_BLOCKSIZE + 1 )
		dsp[ i ] = intToByte( diversifier );
	ENSURES( LOOP_BOUND_OK );
	dspPtr += P12_BLOCKSIZE;

	/* Set up the salt in the next P12_BLOCKSIZE bytes */
	status = expandData( dspPtr, P12_BLOCKSIZE, salt, saltLength );
	if( cryptStatusError( status ) )
		return( status );
	dspPtr += P12_BLOCKSIZE;

	/* Convert the password to a null-terminated Unicode string, a Microsoft
	   bug that was made part of the standard */
	LOOP_EXT( ( keyIndex = 0, bmpIndex = 0 ), keyIndex < keyLength, 
			  ( keyIndex++, bmpIndex += 2 ), CRYPT_MAX_TEXTSIZE + 1 )
		{
		bmpString[ bmpIndex ] = '\0';
		bmpString[ bmpIndex + 1 ] = key[ keyIndex ];
		}
	ENSURES( LOOP_BOUND_OK );
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
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	BYTE p12_DSP[ P12_DSPSIZE + 8 ];
	BYTE p12_Ai[ CRYPT_MAX_HASHSIZE + 8 ], p12_B[ P12_BLOCKSIZE + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	const BYTE *saltPtr = mechanismInfo->salt;
	int dspLen, hashSize, keyIndex, i, status, LOOP_ITERATOR;

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
	LOOP_MED( keyIndex = 0, keyIndex < mechanismInfo->dataOutLength, 
			  keyIndex += hashSize )
		{
		const int noKeyBytes = \
			( mechanismInfo->dataOutLength - keyIndex > hashSize ) ? \
			hashSize : mechanismInfo->dataOutLength - keyIndex;
		int dspIndex, LOOP_ITERATOR_ALT;

		/* Hash the keying material the required number of times to obtain the
		   output value */
		hashFunctionAtomic( p12_Ai, CRYPT_MAX_HASHSIZE, p12_DSP, dspLen );
		LOOP_MAX_ALT( i = 1, i < mechanismInfo->iterations, i++ )
			{
			hashFunctionAtomic( p12_Ai, CRYPT_MAX_HASHSIZE, 
								p12_Ai, hashSize );
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		REQUIRES( boundsCheckZ( keyIndex, noKeyBytes, 
							    mechanismInfo->dataOutLength ) );
		memcpy( dataOutPtr + keyIndex, p12_Ai, noKeyBytes );

		/* Update the input keying material for the next iteration by 
		   adding the output value expanded to P12_BLOCKSIZE, to 
		   P12_BLOCKSIZE-sized blocks of the salt/password portion of the 
		   DSP string */
		status = expandData( p12_B, P12_BLOCKSIZE, p12_Ai, hashSize );
		if( cryptStatusError( status ) )
			break;
		LOOP_LARGE_ALT( dspIndex = P12_BLOCKSIZE, dspIndex < dspLen, 
						dspIndex += P12_BLOCKSIZE )
			{
			status = add64( p12_DSP + dspIndex, P12_BLOCKSIZE,
							p12_B, P12_BLOCKSIZE );
			if( cryptStatusError( status ) )
				break;
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );
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
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	HASH_FUNCTION hashFunction;
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
	HASH_FUNCTION md5HashFunction, shaHashFunction;
	HASHINFO hashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ], counterData[ 16 + 8 ];
	BYTE *dataOutPtr = mechanismInfo->dataOut;
	int md5HashSize, shaHashSize, counter = 0, keyIndex;
	int LOOP_ITERATOR;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	getHashParameters( CRYPT_ALGO_MD5, 0, &md5HashFunction, &md5HashSize );
	getHashParameters( CRYPT_ALGO_SHA1, 0, &shaHashFunction, &shaHashSize );

	/* Produce enough blocks of output to fill the key */
	LOOP_MED( keyIndex = 0, keyIndex < mechanismInfo->dataOutLength, 
			  keyIndex += md5HashSize )
		{
		const int noKeyBytes = \
			( mechanismInfo->dataOutLength - keyIndex > md5HashSize ) ? \
			md5HashSize : mechanismInfo->dataOutLength - keyIndex;
		int i, LOOP_ITERATOR_ALT;

		/* Set up the counter data */
		LOOP_EXT_ALT( i = 0, i <= counter, i++, 16 + 1 )
			counterData[ i ] = intToByte( 'A' + counter );
		ENSURES( LOOP_BOUND_OK_ALT );
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
		REQUIRES( boundsCheckZ( keyIndex, noKeyBytes, 
								mechanismInfo->dataOutLength ) );
		memcpy( dataOutPtr + keyIndex, hash, noKeyBytes );
		}
	ENSURES( LOOP_BOUND_OK );
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
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtrDynamic( salt, saltLength ) );

	REQUIRES( isShortIntegerRangeNZ( keyLength ) );
	REQUIRES( isShortIntegerRangeNZ( saltLength ) );

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
static int tlsPrfHash( INOUT_BUFFER_FIXED( outLength ) BYTE *out, 
					   IN_LENGTH_SHORT const int outLength, 
					   INOUT TLS_PRF_INFO *prfInfo, 
					   IN_BUFFER( saltLength ) const void *salt, 
					   IN_LENGTH_SHORT const int saltLength )
	{
	HASHINFO hashInfo, AnHashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int i, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( out, outLength ) );
	assert( isWritePtr( prfInfo, sizeof( TLS_PRF_INFO ) ) );
	assert( isReadPtrDynamic( salt, saltLength ) );

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
	LOOP_EXT( i = 0, i < outLength, i++, CRYPT_MAX_HASHSIZE + 1 )
		out[ i ] ^= hash[ i ];
	ENSURES( LOOP_BOUND_OK );

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
	int md5Index, shaIndex, status, LOOP_ITERATOR;

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
	LOOP_MED_INITCHECK( md5Index = shaIndex = 0, md5Index < dataOutLength )
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
	ENSURES( LOOP_BOUND_OK );
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
	int keyIndex, status, LOOP_ITERATOR;

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
	LOOP_MED( keyIndex = 0, keyIndex < dataOutLength, 
			  keyIndex += shaInfo.hashSize )
		{
		const int noKeyBytes = min( dataOutLength - keyIndex, \
									shaInfo.hashSize );

		status = tlsPrfHash( dataOutPtr + keyIndex, noKeyBytes, &shaInfo, 
							 mechanismInfo->salt, mechanismInfo->saltLength );
		if( cryptStatusError( status ) )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
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
					   IN const HASH_FUNCTION hashFunction, 
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

	assert( isWritePtrDynamic( out, outLength ) );
	assert( isWritePtr( hashInfo, sizeof( HASHINFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );
	assert( isReadPtrDynamic( salt, saltLength ) );
	assert( isWritePtr( byteCount, sizeof( int ) ) );

	REQUIRES( hashFunction != NULL );
	REQUIRES( outLength == hashSize );
	REQUIRES( hashSize >= MIN_HASHSIZE && hashSize <= CRYPT_MAX_HASHSIZE );
	REQUIRES( keyLength >= 2 && keyLength <= MAX_INTLENGTH_SHORT );
	REQUIRES( saltLength == PGP_SALTSIZE );
	REQUIRES( preloadLength == CRYPT_UNUSED || \
			  ( preloadLength >= 0 && preloadLength <= 1 ) );
	REQUIRES( isIntegerRangeNZ( count ) );

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
	ENSURES( isIntegerRangeNZ( count ) );

	*byteCount = count;
	return( CRYPT_OK );
	}

/* Perform OpenPGP S2K key derivation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePGP( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	HASH_FUNCTION hashFunction;
	HASHINFO hashInfo;
	long byteCount = ( long ) mechanismInfo->iterations << 6;
	long secondByteCount = 0;
	int hashSize, i, status = CRYPT_OK, LOOP_ITERATOR;

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	REQUIRES( mechanismInfo->iterations >= 0 && \
			  mechanismInfo->iterations <= MAX_KEYSETUP_HASHSPECIFIER );
	REQUIRES( byteCount >= 0 && byteCount < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( mechanismInfo->dataOut, 0, mechanismInfo->dataOutLength );

	/* Set up the hash parameters */
	getHashParameters( mechanismInfo->hashAlgo, 0, &hashFunction, 
					   &hashSize );
	memset( hashInfo, 0, sizeof( HASHINFO ) );

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
	#define GPG_FAILSAFE_ITERATIONS_MAX		( MAX_KEYSETUP_HASHSPECIFIER * ( 64 / 16 ) )
	LOOP_EXT( i = 0, byteCount > 0 && cryptStatusOK( status ), i++, 
			  GPG_FAILSAFE_ITERATIONS_MAX )
		{
		status = pgpPrfHash( mechanismInfo->dataOut, 
							 hashSize, hashFunction, hashInfo, hashSize, 
							 mechanismInfo->dataIn,
							 mechanismInfo->dataInLength,
							 mechanismInfo->salt, 
							 mechanismInfo->saltLength, &byteCount, 
							 ( i <= 0 ) ? 0 : CRYPT_UNUSED );
		}
	ENSURES( LOOP_BOUND_OK );
	if( cryptStatusOK( status ) && secondByteCount > 0 )
		{
		LOOP_EXT( i = 0, secondByteCount > 0 && cryptStatusOK( status ), i++,
				  GPG_FAILSAFE_ITERATIONS_MAX )
			{
			status = pgpPrfHash( ( BYTE * ) mechanismInfo->dataOut + hashSize, 
								 hashSize, hashFunction, hashInfo, hashSize, 
								 mechanismInfo->dataIn, 
								 mechanismInfo->dataInLength,
								 mechanismInfo->salt, 
								 mechanismInfo->saltLength, &secondByteCount, 
								 ( i <= 0 ) ? 1 : CRYPT_UNUSED );
			}
		ENSURES( LOOP_BOUND_OK );
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
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	HASH_FUNCTION hashFunction;
	HASHINFO hashInfo;
	int hashSize, iterations, LOOP_ITERATOR;

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
	LOOP_MAX( iterations = 1, iterations < mechanismInfo->iterations, 
			  iterations++ )
		{
		hashFunctionAtomic( mechanismInfo->dataOut, 
							mechanismInfo->dataOutLength, 
							mechanismInfo->dataOut, hashSize );
		}
	ENSURES( LOOP_BOUND_OK );
	zeroise( hashInfo, sizeof( HASHINFO ) );

	return( CRYPT_OK );
	}
#endif /* USE_CMP */

/****************************************************************************
*																			*
*							Derivation Self-test Functions 					*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

/* Test key derivation mechanisms.  The externally-defined 
   MECHANISM_FUNCTION_INFO.function uses 'void *mechanismInfo' for the 
   second parameter because it could be one of several types while here we 
   use the specific 'MECHANISM_DERIVE_INFO *mechanismInfo', this leads to
   warnings from some compilers about assigning incompatible pointer types
   so we explicitly cast the function type via the FUNCTION_CAST below */

#define FUNCTION_CAST		const MECHANISM_FUNCTION

typedef struct {
	MECHANISM_FUNCTION_INFO mechanismFunctionInfo;
	MECHANISM_DERIVE_INFO mechanismInfo;
	} MECHANISM_TEST_INFO;

#define MECHANISM_OUTPUT_SIZE		32
#define MECHANISM_INPUT_SIZE		32
#define MECHANISM_SALT_SIZE			16

#define MECHANISM_OUTPUT_SIZE_SSL	48
#define MECHANISM_INPUT_SIZE_SSL	48
#define MECHANISM_SALT_SIZE_SSL		64

static const BYTE inputValue[] = {
	/* More than a single hash block size for SHA-1 */
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 
	0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
	0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
	};
static const BYTE saltValue[] = {
	/* At least 64 bytes for SSL/TLS PRF */
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
	0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F, 
	0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
	0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x80, 0x91, 0xA2, 0xB3, 0xC4, 0xD5, 0xE6, 0xF7, 
	0x08, 0x19, 0x2A, 0x3B, 0x4C, 0x5D, 0x6E, 0x7F
	};
#ifdef USE_PKCS12
static const BYTE pkcs12saltValue[] = {
	/* PKCS #12 has a single-byte diversifier at the start of the salt */
	0x01,
	0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87, 
	0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
	};
#endif /* USE_PKCS12 */

static const MECHANISM_TEST_INFO deriveMechanismTestInfo[] = {
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PBKDF2, ( FUNCTION_CAST ) derivePBKDF2 },
	  { MKDATA( "\x73\xF7\x8A\xBE\x3C\x9C\x65\x80\x97\x60\x56\xDE\x04\x2A\x0C\x97"
				"\x99\xF5\x06\x0F\x43\x06\xA5\xD0\x74\xC9\xD5\xC5\xA5\x05\xB5\x7F" ), MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_HMAC_SHA1, 0,
		saltValue, MECHANISM_SALT_SIZE, 10 } },
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PGP, ( FUNCTION_CAST ) derivePGP },
	  { MKDATA( "\x4A\x4B\x90\x09\x27\xF8\xD0\x93\x56\x16\xEA\xC1\x45\xCD\xEE\x05"
				"\x67\xE1\x09\x38\x66\xEB\xB2\xB2\xB9\x1F\xD3\xF7\x48\x2B\xDC\xCA" ), MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		saltValue, 8, 10 } },
#endif /* USE_PGP || USE_PGPKEYS */
#ifdef USE_SSL
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_SSL, ( FUNCTION_CAST ) deriveSSL },
	  { MKDATA( "\x87\x46\xDD\x7D\xAD\x5F\x48\xB6\xFC\x8D\x92\xC4\xDB\x38\x79\x9A"
				"\x3D\xEA\x22\xFA\xCD\x7E\x86\xD5\x23\x6E\x10\x4C\xBD\x84\x89\xDF"
				"\x1C\x87\x60\xBF\xFA\x2B\xCA\xFE\xFE\x65\xC7\xA2\xCF\x04\xFF\xEB" ), MECHANISM_OUTPUT_SIZE_SSL,
		inputValue, MECHANISM_INPUT_SIZE_SSL, ( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 0,
		saltValue, MECHANISM_SALT_SIZE_SSL, 1 } },				  /* Both MD5 and SHA1 */
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_TLS, ( FUNCTION_CAST ) deriveTLS },
	  { MKDATA( "\xD3\xD4\x2F\xD6\xE3\x7D\xC0\x3C\xA6\x9F\x92\xDF\x3E\x40\x0A\x64"
				"\x49\xB4\x0E\xC4\x14\x04\x2F\xC8\xDD\x27\xD5\x1C\x62\xD2\x2C\x97"
				"\x90\xAE\x08\x4B\xEE\xF4\x8D\x22\xF0\x2A\x1E\x38\x2D\x31\xCB\x68" ), MECHANISM_OUTPUT_SIZE_SSL,
		inputValue, MECHANISM_INPUT_SIZE_SSL, ( CRYPT_ALGO_TYPE ) CRYPT_USE_DEFAULT, 0,
		saltValue, MECHANISM_SALT_SIZE_SSL, 1 } },				  /* Both MD5 and SHA1 */
#endif /* USE_SSL */
#ifdef USE_CMP
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_CMP, ( FUNCTION_CAST ) deriveCMP },
	  { MKDATA( "\x80\x0B\x95\x73\x74\x3B\xC1\x63\x6B\x28\x2B\x04\x47\xFD\xF0\x04"
				"\x80\x40\x31\xB1" ), 20,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		saltValue, MECHANISM_SALT_SIZE, 10 } },
#endif /* USE_CMP */
#ifdef USE_PKCS12
  #if 0		/* Additional check value from OpenSSL, this only uses 1 iteration */
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( FUNCTION_CAST ) derivePKCS12 },
	  { MKDATA( "\x8A\xAA\xE6\x29\x7B\x6C\xB0\x46\x42\xAB\x5B\x07\x78\x51\x28\x4E"
				"\xB7\x12\x8F\x1A\x2A\x7F\xBC\xA3" ), 24,
		MKDATA( "smeg" ), 4, CRYPT_ALGO_SHA1, 0,
		MKDATA( "\x01\x0A\x58\xCF\x64\x53\x0D\x82\x3F" ), 1 + 8, 1 } },
  #endif /* 0 */
  #if 0		/* Additional check value from OpenSSL, now with 1,000 iterations */
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( FUNCTION_CAST ) derivePKCS12 },
	  { MKDATA( "\xED\x20\x34\xE3\x63\x28\x83\x0F\xF0\x9D\xF1\xE1\xA0\x7D\xD3\x57"
				"\x18\x5D\xAC\x0D\x4F\x9E\xB3\xD4" ), 24,
		MKDATA( "queeg" ), 5, CRYPT_ALGO_SHA1, 0,
		MKDATA( "\x01\x05\xDE\xC9\x59\xAC\xFF\x72\xF7" ), 1 + 8, 1000 } },
  #endif /* 0 */
	{ { MESSAGE_DEV_DERIVE, MECHANISM_DERIVE_PKCS12, ( FUNCTION_CAST ) derivePKCS12 },
	  { MKDATA( "\x8B\xFB\x1D\x77\xFE\x78\xFF\xE8\xE9\x69\x76\xE0\xC5\x0A\xB6\xD2"
				"\x64\xEC\xA3\x01\xE9\xD2\xE0\xC0\xBC\x60\x3D\x63\xB2\x4A\xB2\x63" ), MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_SHA1, 0,
		pkcs12saltValue, 1 + MECHANISM_SALT_SIZE, 10 } },
#endif /* USE_PKCS12 */
	{ { MESSAGE_NONE } }, { { MESSAGE_NONE } }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveSelftest( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	BYTE buffer[ MECHANISM_OUTPUT_SIZE_SSL + 8 ];
	int i, status, LOOP_ITERATOR;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	LOOP_LARGE( i = 0, 
				deriveMechanismTestInfo[ i ].mechanismFunctionInfo.action != MESSAGE_NONE && \
					i < FAILSAFE_ARRAYSIZE( deriveMechanismTestInfo, \
											MECHANISM_TEST_INFO ),
				i++ )
		{
		const MECHANISM_FUNCTION_INFO *mechanismFunctionInfo = \
					&deriveMechanismTestInfo[ i ].mechanismFunctionInfo;
		const MECHANISM_TEST_INFO *mechanismTestInfoPtr = \
											&deriveMechanismTestInfo[ i ];
		MECHANISM_DERIVE_INFO testMechanismInfo;

		memcpy( &testMechanismInfo, &mechanismTestInfoPtr->mechanismInfo, 
				sizeof( MECHANISM_DERIVE_INFO ) );
		testMechanismInfo.dataOut = buffer;
		status = mechanismFunctionInfo->function( NULL, &testMechanismInfo );
		if( cryptStatusError( status ) )
			return( status );
		if( memcmp( mechanismTestInfoPtr->mechanismInfo.dataOut, buffer, 
					mechanismTestInfoPtr->mechanismInfo.dataOutLength ) )
			{
			DEBUG_PRINT(( "Mechanism self-test #%d for mechanism %d failed.\n", 
						  i, mechanismFunctionInfo->mechanism ));
			return( CRYPT_ERROR_FAILED );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < FAILSAFE_ARRAYSIZE( deriveMechanismTestInfo, \
									 MECHANISM_TEST_INFO ) );

	return( CRYPT_OK );
	}

/* Test KDF mechanisms */

static const MECHANISM_TEST_INFO kdfMechanismTestInfo[] = {
	/* For the PBKDF2 test we call derivePBKDF2() rather than kdfPBKDF2() 
	   since the latter works directly on encryption contexts, and in any 
	   case is just a wrapper for derivePBKDF2() with the iteration count
	   hardcoded to 1 */
	{ { MESSAGE_DEV_KDF, MECHANISM_DERIVE_PBKDF2, ( FUNCTION_CAST ) derivePBKDF2 },
	  { "\x46\x9D\x41\x22\x45\x10\x28\x4A\xF9\x80\x62\xCF\xD6\x4F\x4D\x66"
		"\x4B\x76\xEC\x7E\xF0\x48\x7A\xC3\x9A\xDB\x2E\xAE\x56\x94\x65\x01", MECHANISM_OUTPUT_SIZE,
		inputValue, MECHANISM_INPUT_SIZE, CRYPT_ALGO_HMAC_SHA2, 0,
		saltValue, MECHANISM_SALT_SIZE, 1 } },
	{ { MESSAGE_NONE, MECHANISM_NONE, NULL } }, 
			{ { MESSAGE_NONE, MECHANISM_NONE, NULL } }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int kdfSelftest( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_DERIVE_INFO *mechanismInfo )
	{
	BYTE buffer[ MECHANISM_OUTPUT_SIZE_SSL + 8 ];
	int i, status, LOOP_ITERATOR;

	UNUSED_ARG( dummy );
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_DERIVE_INFO ) ) );

	LOOP_LARGE( i = 0, 
				kdfMechanismTestInfo[ i ].mechanismFunctionInfo.mechanism != MECHANISM_NONE && \
					i < FAILSAFE_ARRAYSIZE( kdfMechanismTestInfo, \
											MECHANISM_TEST_INFO ),
				i++ )
		{
		const MECHANISM_FUNCTION_INFO *mechanismFunctionInfo = \
					&kdfMechanismTestInfo[ i ].mechanismFunctionInfo;
		const MECHANISM_TEST_INFO *mechanismTestInfoPtr = \
											&kdfMechanismTestInfo[ i ];
		MECHANISM_DERIVE_INFO testMechanismInfo;

		memcpy( &testMechanismInfo, &mechanismTestInfoPtr->mechanismInfo, 
				sizeof( MECHANISM_DERIVE_INFO ) );
		testMechanismInfo.dataOut = buffer;
		status = mechanismFunctionInfo->function( NULL, &testMechanismInfo );
		if( cryptStatusError( status ) )
			return( status );
		if( memcmp( mechanismTestInfoPtr->mechanismInfo.dataOut, buffer, 
					mechanismTestInfoPtr->mechanismInfo.dataOutLength ) )
			{
			DEBUG_PRINT(( "Mechanism self-test #%d for mechanism %d failed.\n", 
						  i, mechanismFunctionInfo->mechanism ));
			return( CRYPT_ERROR_FAILED );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < FAILSAFE_ARRAYSIZE( kdfMechanismTestInfo, \
									 MECHANISM_TEST_INFO ) );

	return( CRYPT_OK );
	}
#endif /* CONFIG_NO_SELFTEST */
