/****************************************************************************
*																			*
*							  PGP Support Routines							*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

/****************************************************************************
*																			*
*						PGP <-> Cryptlib Algorithm Conversion				*
*																			*
****************************************************************************/

/* Convert algorithm IDs from cryptlib to PGP and back */

typedef struct {
	const int pgpAlgo;
	const PGP_ALGOCLASS_TYPE pgpAlgoClass;
	const CRYPT_ALGO_TYPE cryptlibAlgo;
	} PGP_ALGOMAP_INFO;
static const PGP_ALGOMAP_INFO FAR_BSS pgpAlgoMap[] = {
	/* Encryption algos */
	{ PGP_ALGO_3DES, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_3DES },
#ifdef USE_BLOWFISH
	{ PGP_ALGO_BLOWFISH, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_BLOWFISH },
#endif /* USE_BLOWFISH */
#ifdef USE_CAST
	{ PGP_ALGO_CAST5, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_CAST },
#endif /* USE_CAST */
#ifdef USE_IDEA
	{ PGP_ALGO_IDEA, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_IDEA },
#endif /* USE_IDEA */
	{ PGP_ALGO_AES_128, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_AES },
	{ PGP_ALGO_AES_192, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_AES },
	{ PGP_ALGO_AES_256, PGP_ALGOCLASS_CRYPT, CRYPT_ALGO_AES },

	/* Password-based encryption algos */
	{ PGP_ALGO_3DES, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_3DES },
#ifdef USE_BLOWFISH
	{ PGP_ALGO_BLOWFISH, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_BLOWFISH },
#endif /* USE_BLOWFISH */
#ifdef USE_CAST
	{ PGP_ALGO_CAST5, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_CAST },
#endif /* USE_CAST */
#ifdef USE_IDEA
	{ PGP_ALGO_IDEA, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_IDEA },
#endif /* USE_IDEA */
	{ PGP_ALGO_AES_128, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_AES },
	{ PGP_ALGO_AES_192, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_AES },
	{ PGP_ALGO_AES_256, PGP_ALGOCLASS_PWCRYPT, CRYPT_ALGO_AES },

	/* PKC encryption algos */
	{ PGP_ALGO_RSA, PGP_ALGOCLASS_PKCCRYPT, CRYPT_ALGO_RSA },
	{ PGP_ALGO_RSA_ENCRYPT, PGP_ALGOCLASS_PKCCRYPT, CRYPT_ALGO_RSA },
#ifdef USE_ELGAMAL
	{ PGP_ALGO_ELGAMAL, PGP_ALGOCLASS_PKCCRYPT, CRYPT_ALGO_ELGAMAL },
#endif /* USE_ELGAMAL */

	/* PKC sig algos */
	{ PGP_ALGO_RSA, PGP_ALGOCLASS_SIGN, CRYPT_ALGO_RSA },
	{ PGP_ALGO_RSA_SIGN, PGP_ALGOCLASS_SIGN, CRYPT_ALGO_RSA },
	{ PGP_ALGO_DSA, PGP_ALGOCLASS_SIGN, CRYPT_ALGO_DSA },

	/* Hash algos */
#ifdef USE_MD5
	{ PGP_ALGO_MD5, PGP_ALGOCLASS_HASH, CRYPT_ALGO_MD5 },
#endif /* USE_MD5 */
	{ PGP_ALGO_SHA, PGP_ALGOCLASS_HASH, CRYPT_ALGO_SHA1 },
#ifdef USE_RIPEMD160
	{ PGP_ALGO_RIPEMD160, PGP_ALGOCLASS_HASH, CRYPT_ALGO_RIPEMD160 },
#endif /* USE_RIPEMD160 */
#ifdef USE_SHA2
	{ PGP_ALGO_SHA2_256, PGP_ALGOCLASS_HASH, CRYPT_ALGO_SHA2 },
#endif /* USE_SHA2 */

	{ PGP_ALGO_NONE, 0, CRYPT_ALGO_NONE },
	{ PGP_ALGO_NONE, 0, CRYPT_ALGO_NONE }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int pgpToCryptlibAlgo( IN_RANGE( PGP_ALGO_NONE, 0xFF ) const int pgpAlgo, 
					   IN_ENUM( PGP_ALGOCLASS ) \
							const PGP_ALGOCLASS_TYPE pgpAlgoClass,
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo )
	{
	int i;

	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( pgpAlgo >= 0 && pgpAlgo <= 0xFF );
	REQUIRES( pgpAlgoClass > PGP_ALGOCLASS_NONE && \
			  pgpAlgoClass < PGP_ALGOCLASS_LAST );

	/* Clear return value */
	*cryptAlgo = CRYPT_ALGO_NONE;

	for( i = 0;
		 ( pgpAlgoMap[ i ].pgpAlgo != pgpAlgo || \
		   pgpAlgoMap[ i ].pgpAlgoClass != pgpAlgoClass ) && \
			pgpAlgoMap[ i ].pgpAlgo != PGP_ALGO_NONE && \
			i < FAILSAFE_ARRAYSIZE( pgpAlgoMap, PGP_ALGOMAP_INFO ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( pgpAlgoMap, PGP_ALGOMAP_INFO ) );
	if( pgpAlgoMap[ i ].cryptlibAlgo == PGP_ALGO_NONE )
		return( CRYPT_ERROR_NOTAVAIL );
	*cryptAlgo = pgpAlgoMap[ i ].cryptlibAlgo;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int cryptlibToPgpAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptlibAlgo,
					   OUT_RANGE( PGP_ALGO_NONE, PGP_ALGO_LAST ) \
							int *pgpAlgo )
	{
	int i;

	assert( isWritePtr( pgpAlgo, sizeof( int ) ) );

	REQUIRES( cryptlibAlgo > CRYPT_ALGO_NONE && \
			  cryptlibAlgo < CRYPT_ALGO_LAST );

	/* Clear return value */
	*pgpAlgo = PGP_ALGO_NONE;

	for( i = 0; 
		 pgpAlgoMap[ i ].cryptlibAlgo != cryptlibAlgo && \
			pgpAlgoMap[ i ].cryptlibAlgo != CRYPT_ALGO_NONE && \
			i < FAILSAFE_ARRAYSIZE( pgpAlgoMap, PGP_ALGOMAP_INFO ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( pgpAlgoMap, PGP_ALGOMAP_INFO ) );
	if( pgpAlgoMap[ i ].cryptlibAlgo == CRYPT_ALGO_NONE )
		return( CRYPT_ERROR_NOTAVAIL );
	*pgpAlgo = pgpAlgoMap[ i ].pgpAlgo;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readPgpAlgo( INOUT STREAM *stream, 
				 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo, 
				 IN_ENUM( PGP_ALGOCLASS ) \
						const PGP_ALGOCLASS_TYPE pgpAlgoClass )
	{
	CRYPT_ALGO_TYPE algo;
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( pgpAlgoClass > PGP_ALGOCLASS_NONE && \
			  pgpAlgoClass < PGP_ALGOCLASS_LAST );

	/* Clear return value */
	*cryptAlgo = CRYPT_ALGO_NONE;

	value = sgetc( stream );
	if( cryptStatusError( value ) )
		return( value );
	status = pgpToCryptlibAlgo( value, pgpAlgoClass, &algo );
	if( cryptStatusError( status ) )
		return( status );
	*cryptAlgo = algo;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Misc. PGP-related Routines						*
*																			*
****************************************************************************/

/* Create an encryption key from a password */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int pgpPasswordToKey( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
					  IN_LENGTH_SHORT_OPT const int optKeyLength,
					  IN_BUFFER( passwordLength ) const char *password, 
					  IN_LENGTH_SHORT const int passwordLength, 
					  IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
					  IN_BUFFER_OPT( saltSize ) const BYTE *salt, 
					  IN_RANGE( 0, CRYPT_MAX_HASHSIZE ) const int saltSize,
					  IN_INT const int iterations )
	{
	MESSAGE_DATA msgData;
	BYTE hashedKey[ CRYPT_MAX_KEYSIZE + 8 ];
	int algorithm, keySize = DUMMY_INIT, status;	/* int vs.enum */

	assert( isReadPtr( password, passwordLength ) );
	assert( ( salt == NULL && saltSize == 0 ) || \
			isReadPtr( salt, saltSize ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( passwordLength > 0 && passwordLength < MAX_INTLENGTH );
	REQUIRES( ( optKeyLength == CRYPT_UNUSED ) || \
			  ( optKeyLength >= MIN_KEYSIZE && \
				optKeyLength <= CRYPT_MAX_KEYSIZE ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( ( salt == NULL && saltSize == 0 ) || \
			  ( saltSize > 0 && saltSize <= CRYPT_MAX_HASHSIZE ) );
	REQUIRES( iterations >= 0 && iterations < MAX_INTLENGTH );

	/* Get various parameters needed to process the password */
	status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
							  &algorithm, CRYPT_CTXINFO_ALGO );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE,
								  &keySize, CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( algorithm == CRYPT_ALGO_BLOWFISH )
		{
		/* PGP limits the Blowfish key size to 128 bits rather than the more
		   usual 448 bits */
		keySize = 16;
		}
	if( algorithm == CRYPT_ALGO_AES && optKeyLength != CRYPT_UNUSED )
		{
		/* PGP allows various AES key sizes and then encodes the size in the
		   algorithm ID (ugh), to handle this we allow the caller to specify
		   the actual size */
		keySize = optKeyLength;
		}
	ENSURES( keySize >= MIN_KEYSIZE && keySize <= CRYPT_MAX_KEYSIZE );

	/* Hash the password */
	if( salt != NULL )
		{
		MECHANISM_DERIVE_INFO mechanismInfo;

		/* Turn the user key into an encryption context key */
		setMechanismDeriveInfo( &mechanismInfo, hashedKey, keySize,
								password, passwordLength, hashAlgo,
								salt, saltSize, iterations );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_DERIVE,
								  &mechanismInfo, MECHANISM_DERIVE_PGP );
		if( cryptStatusError( status ) )
			return( status );

		/* Save the derivation info with the context */
		setMessageData( &msgData, ( MESSAGE_CAST ) salt, saltSize );
		status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
								  &msgData, CRYPT_CTXINFO_KEYING_SALT );
		if( cryptStatusOK( status ) && \
			iterations > 0 && iterations <= MAX_KEYSETUP_ITERATIONS )
			{
			/* The number of key setup iterations may be zero for non-
			   iterated hashing, and may be some ridiculous value for 
			   passwords used to protect private keys created by newer 
			   versions of GPG.  In the former case we leave the iteration 
			   count at zero, in the latter case we don't have to set the 
			   iteration count (or indeed any of the key setup values) 
			   because for private-key decryption keys they're never used 
			   once the user key has been converted into the decryption
			   key */
			status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &iterations, 
									  CRYPT_CTXINFO_KEYING_ITERATIONS );
			}
		if( cryptStatusOK( status ) )
			{
			const int value = hashAlgo;	/* int vs.enum */
			status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &value, 
									  CRYPT_CTXINFO_KEYING_ALGO );
			}
		if( cryptStatusError( status ) )
			{
			zeroise( hashedKey, CRYPT_MAX_KEYSIZE );
			return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
			}
		}
	else
		{
		HASHFUNCTION_ATOMIC hashFunctionAtomic;

		getHashAtomicParameters( hashAlgo, 0, &hashFunctionAtomic, NULL );
		hashFunctionAtomic( hashedKey, CRYPT_MAX_KEYSIZE, password, 
							passwordLength );
		}

	/* Load the key into the context */
	setMessageData( &msgData, hashedKey, keySize );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_KEY );
	zeroise( hashedKey, CRYPT_MAX_KEYSIZE );

	return( status );
	}

/* Process a PGP-style IV.  This isn't a standard IV but contains an extra
   two bytes of check value, which is why it's denoted as 'ivInfo' rather
   than a pure 'iv' */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int pgpProcessIV( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
				  INOUT_BUFFER_FIXED( ivInfoSize ) BYTE *ivInfo, 
				  IN_RANGE( 8 + 2, CRYPT_MAX_IVSIZE + 2 ) const int ivInfoSize, 
				  IN_LENGTH_IV const int ivDataSize, 
				  IN_HANDLE_OPT const CRYPT_CONTEXT iMdcContext,
				  const BOOLEAN isEncrypt )
	{
	static const BYTE zeroIV[ CRYPT_MAX_IVSIZE ] = { 0 };
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( ivInfo, ivInfoSize ) );

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( ivDataSize >= 8 && ivDataSize <= CRYPT_MAX_IVSIZE );
	REQUIRES( ivInfoSize == ivDataSize + 2 );
	REQUIRES( iMdcContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iCryptContext ) );

	/* PGP uses a bizarre way of handling IVs that resyncs the data on some 
	   boundaries and doesn't actually use an IV but instead prefixes the 
	   data with ivSize bytes of random information (which is effectively 
	   the IV) followed by two bytes of key check value after which there's a
	   resync boundary that requires reloading the IV from the last ivSize
	   bytes of ciphertext.  Two exceptions are the encrypted private key,
	   which does use an IV (although this can also be regarded as an
	   ivSize-byte prefix) without a key check or resync, and an encrypted 
	   packet with MDC, which doesn't do the resync (if it weren't for that 
	   it would be trivial to roll an MDC packet back to a non-MDC packet, 
	   only the non-resync prevents this since the first bytes of the
	   encapsulated data packet will be corrupted).
	   
	   First, we load the all-zero IV */
	setMessageData( &msgData, ( MESSAGE_CAST ) zeroIV, ivDataSize );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_IV );
	if( cryptStatusError( status ) )
		return( status );

	/* Then we encrypt or decrypt the IV info, which consists of the actual 
	   IV data plus two bytes of check value */
	if( isEncrypt )
		{
		/* Get some random data to serve as the IV and duplicate the last 
		   two bytes to create the check value */
		setMessageData( &msgData, ivInfo, ivDataSize );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
		memcpy( ivInfo + ivDataSize, ivInfo + ivDataSize - 2, 2 );

		/* If the caller is using an MDC then the plaintext IV data has to 
		   be hashed into the MDC value, effectively turning the straight 
		   hash into a basic secret-prefix MAC */
		if( iMdcContext != CRYPT_UNUSED )
			{
			status = krnlSendMessage( iMdcContext, IMESSAGE_CTX_HASH,
									  ivInfo, ivInfoSize );
			if( cryptStatusError( status ) )
				return( status );
			}

		/* Encrypt the IV and check value */
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
								  ivInfo, ivInfoSize );
		}
	else
		{
		BYTE ivInfoBuffer[ CRYPT_MAX_IVSIZE + 2 + 8 ];

		/* Decrypt the first ivSize bytes (the IV data) and the following 2-
		   byte check value.  There's a potential problem here in which an 
		   attacker that convinces us to act as an oracle for the valid/not
		   valid status of the checksum can determine the contents of 16
		   bits of the encrypted data in 2^15 queries on average.  This is
		   incredibly unlikely (which implementation would auto-respond to
		   32,000 reqpeated queries on a block of data and return the 
		   results to an external agent?), however if it's a concern then 
		   one ameliorating change would be to not perform the check for 
		   keys that were PKC-encrypted, because the PKC decryption process
		   would check the key for us */
		memcpy( ivInfoBuffer, ivInfo, ivInfoSize );
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_DECRYPT,
								  ivInfoBuffer, ivInfoSize );
		if( cryptStatusError( status ) )
			return( status );
		if( ivInfoBuffer[ ivDataSize - 2 ] != ivInfoBuffer[ ivDataSize + 0 ] || \
			ivInfoBuffer[ ivDataSize - 1 ] != ivInfoBuffer[ ivDataSize + 1 ] )
			return( CRYPT_ERROR_WRONGKEY );

		/* If the caller is using an MDC then the plaintext IV data has to 
		   be hashed into the MDC value, effectively turning the straight 
		   hash into a basic secret-prefix MAC */
		if( iMdcContext != CRYPT_UNUSED )
			status = krnlSendMessage( iMdcContext, IMESSAGE_CTX_HASH,
									  ivInfoBuffer, ivInfoSize );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* The IV-resync is only done if traditional PGP encryption is used, the 
	   processing was changed when MDC handling was added to skip this step, 
	   so we only perform the resync if we're using non-MDC encryption */
	if( iMdcContext != CRYPT_UNUSED )
		return( CRYPT_OK );

	/* Finally we've got the data the way that we want it, resync the IV by
	   setting it to the last ivSize bytes of data processed */
	setMessageData( &msgData, ivInfo + 2, ivDataSize );
	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S,
							 &msgData, CRYPT_CTXINFO_IV ) );
	}

/* Read PGP S2K information:

	byte	stringToKey specifier, 0, 1, or 3
	byte[]	stringToKey data
			0x00: byte		hashAlgo	-- S2K = 0, 1, 3
			0x01: byte[8]	salt		-- S2K = 1, 3
			0x03: byte		iterations	-- S2K = 3 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readPgpS2K( INOUT STREAM *stream, 
				OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
				OUT_BUFFER( saltSize, *saltLen ) BYTE *salt, 
				IN_LENGTH_SHORT_MIN( PGP_SALTSIZE ) const int saltMaxLen, 
				OUT_LENGTH_SHORT_Z int *saltLen,
				OUT_INT_SHORT_Z int *iterations )
	{
	long hashSpecifier;
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( hashAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( isWritePtr( salt, saltMaxLen ) );
	assert( isWritePtr( saltLen, sizeof( int ) ) );
	assert( isWritePtr( iterations, sizeof( int ) ) );

	REQUIRES( saltMaxLen >= PGP_SALTSIZE && \
			  saltMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*hashAlgo = CRYPT_ALGO_NONE;
	memset( salt, 0, min( 16, saltMaxLen ) );
	*saltLen = 0;
	*iterations = 0;

	/* Read the S2K information */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != 0 && value != 1 && value != 3 )
		return( CRYPT_ERROR_BADDATA );
	status = readPgpAlgo( stream, hashAlgo, PGP_ALGOCLASS_HASH );
	if( cryptStatusError( status ) )
		 return( status );

	/* If it's a straight hash, we're done */
	if( value <= 0 )
		return( CRYPT_OK );

	/* It's a salted hash, read the salt */
	status = sread( stream, salt, saltMaxLen );
	if( cryptStatusError( status ) )
		return( status );
	*saltLen = PGP_SALTSIZE;

	/* If it's a non-iterated hash, we're done */
	if( value < 3 )
		return( CRYPT_OK );

	/* It's a salted iterated hash, get the iteration count from the bizarre 
	   fixed-point encoded value, limited to a sane value:

		count = ( ( Int32 ) 16 + ( c & 15 ) ) << ( ( c >> 4 ) + 6 )
	   
	   The "iteration count" is actually a count of how many bytes are 
	   hashed, this is because the "iterated hashing" treats the salt + 
	   password as an infinitely-repeated sequence of values and hashes the 
	   resulting string for PGP-iteration-count bytes worth.  The value that 
	   we calculate here (to prevent overflow on 16-bit machines) is the 
	   count without the base * 64 scaling, this also puts the range within 
	   the value of the standard sanity check.  When we do the range check 
	   we knock a further four bits off the maximum allowed length to avoid 
	   performing calculations too close to INT_MAX in the S2K code.
		   
	   Note that there's a mutant GPG build used with loop-AES that uses 8M 
	   setup iterations for PGP private keys, why this is used and why it 
	   writes PGP keys with this setting is uncertain but cryptlib will 
	   reject keys with this value as being outside the range of sane values 
	   (for an 8-byte salt and a typical 8-byte password this would lead to 
	   8M / 16 = 512K iterations of the PRF, a value so extreme that it'd 
	   normally only be used in a DoS attack).
		   
	   Unfortunately PGP Desktop 9 (apparently) in its default config will 
	   use values up to 4M (= 256K iterations of the PRF), which the sanity-
	   check code would also reject.  It's uncertain at which point we 
	   should draw the line here, on the one hand we want to be able to 
	   handle PGP Desktop's data but we also want some protection against 
	   DoS attacks due to ridiculously high iteration counts.  For now we 
	   reject obviously invalid values (ones less than zero or which could 
	   cause an overflow once the scaling is applied) and in addition alert
	   the caller (and return a less fatal error code) if the iteration 
	   count would be larger than 128K for the typical 8-byte password case 
	   above */
	value = sgetc( stream );
	if( cryptStatusError( value ) )
		return( value );
	hashSpecifier = ( 16 + ( ( long ) value & 0x0F ) ) << ( value >> 4 );
	if( hashSpecifier <= 0 || \
		hashSpecifier >= ( MAX_INTLENGTH >> 4 ) / 64 )
		return( CRYPT_ERROR_BADDATA );
	if( hashSpecifier > MAX_KEYSETUP_HASHSPECIFIER )
		{
		/* The key requires more than 32K * 64 = 2M bytes of hashing or 128K 
		   iterations for 8 bytes salt + 8 bytes password, this is more 
		   likely a DoS than a genuine hash count */
		DEBUG_DIAG(( "Encountered key with an S2K hash count parameter "
					 "of %d, max.allowed is %d", hashSpecifier * 64,
					 MAX_KEYSETUP_HASHSPECIFIER * 64 ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	*iterations = ( int ) hashSpecifier;

	return( CRYPT_OK );
	}
#endif /* USE_PGP || USE_PGPKEYS */
