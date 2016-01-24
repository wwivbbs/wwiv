/****************************************************************************
*																			*
*					cryptlib Private-Key Wrap Mechanism Routines			*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#ifdef INC_ALL
  #include "crypt.h"
  #include "asn1.h"
  #include "misc_rw.h"
  #include "stream.h"
  #include "mech_int.h"
  #include "pgp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/misc_rw.h"
  #include "io/stream.h"
  #include "mechs/mech_int.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

/* Decrypt a PGP MPI */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int pgpReadDecryptMPI( INOUT STREAM *stream,
							  IN_HANDLE const CRYPT_CONTEXT iCryptContext,
							  IN_LENGTH_PKC const int minLength, 
							  IN_LENGTH_PKC const int maxLength )
	{
	void *mpiDataPtr = DUMMY_INIT_PTR;
	const long mpiDataStartPos = stell( stream ) + UINT16_SIZE;
	int mpiLength, dummy, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( minLength >= bitsToBytes( 155 ) && \
			  minLength <= maxLength && \
			  maxLength <= CRYPT_MAX_PKCSIZE );

	/* Get the MPI length and decrypt the payload data.  We have to be 
	   careful how we handle this because readInteger16Ubits() returns the 
	   canonicalised form of the values (with leading zeroes truncated) so 
	   the returned length value doesn't necessarily represent the amount
	   of data that we need to decrypt:

		startPos	dataStart		 stell()
			|			|				|
			v			v <-- length -->v
		+---+-----------+---------------+
		|	|			|///////////////| Stream
		+---+-----------+---------------+ */
	status = readInteger16Ubits( stream, NULL, &dummy, minLength, 
								 maxLength );
	if( cryptStatusError( status ) )
		return( status );
	mpiLength = stell( stream ) - mpiDataStartPos;
	status = sMemGetDataBlockAbs( stream, mpiDataStartPos, &mpiDataPtr, 
								  mpiLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_DECRYPT,
								  mpiDataPtr, mpiLength );
	return( status );
	}

/* The PGP 2.x key wrap encrypts only the MPI payload data rather than the 
   entire private key record so we have to read and then decrypt each 
   component separately.  This is a horrible way to handle things because we 
   have to repeatedly process the MPI data, first in the PGP keyring code to
   find out how much key data is present, then again during decryption to 
   find the MPI payload that needs to be decrypted, and finally again after
   decryption to find the MPI payload that needs to be checksummed (although
   we can shortcut the latter, see the comment in checkPgp2KeyIntegrity().
   
   Unfortunately we can't use the xxxPARAM_MIN_x / xxxPARAM_MAX_x values to
   perform range checking since they're only visible to the context-
   manipulation code, so we have to use approximations here (the actual
   values will be checked by the keyload code anyway, it just means that we
   can't perform very precise pre-filtering here) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int pgp2DecryptKey( IN_BUFFER( dataLength ) const void *data, 
						   IN_LENGTH_SHORT_MIN( 16 ) const int dataLength, 
						   OUT_LENGTH_SHORT_Z int *bytesToChecksum, 
						   IN_HANDLE const CRYPT_CONTEXT iCryptContext,
						   const BOOLEAN isDlpAlgo )
	{
	STREAM stream;
	int status;

	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( bytesToChecksum, sizeof( int ) ) );
	
	REQUIRES( dataLength >= 16 && \
			  dataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptContext ) );

	/* Clear return value */
	*bytesToChecksum = 0;

	sMemConnect( &stream, data, dataLength );
	status = pgpReadDecryptMPI( &stream, iCryptContext,			/* d or x */
								bitsToBytes( 155 ), CRYPT_MAX_PKCSIZE );
	if( isDlpAlgo )
		{
		if( cryptStatusOK( status ) )
			*bytesToChecksum = stell( &stream );
		sMemDisconnect( &stream );
		return( status );
		}
	if( cryptStatusOK( status ) )
		status = pgpReadDecryptMPI( &stream, iCryptContext,		/* p */
									MIN_PKCSIZE / 2, CRYPT_MAX_PKCSIZE );
	if( cryptStatusOK( status ) )
		status = pgpReadDecryptMPI( &stream, iCryptContext,		/* q */
									MIN_PKCSIZE / 2, CRYPT_MAX_PKCSIZE );
	if( cryptStatusOK( status ) )
		status = pgpReadDecryptMPI( &stream, iCryptContext,		/* u */
									MIN_PKCSIZE / 2, CRYPT_MAX_PKCSIZE  );
	if( cryptStatusOK( status ) )
		*bytesToChecksum = stell( &stream );
	sMemDisconnect( &stream );
	return( status );
	}
#endif /* USE_PGP || USE_PGPKEYS */

/* Check that the unwrapped data hasn't been corrupted */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkKeyIntegrity( IN_BUFFER( dataLength ) const void *data, 
							  IN_LENGTH_SHORT_MIN( MIN_PRIVATE_KEYSIZE ) \
								const int dataLength,
							  IN_RANGE( 8, CRYPT_MAX_IVSIZE ) \
								const int blockSize )
	{
	const BYTE *padPtr;
	int length, padSize, i, status;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength >= MIN_PRIVATE_KEYSIZE && \
			  dataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( blockSize >= 8 && blockSize <= CRYPT_MAX_IVSIZE );

	/* Get the length of the encapsulated ASN.1 object */
	status = getObjectLength( data, dataLength, &length );
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ERROR_BADDATA ) ? \
				CRYPT_ERROR_WRONGKEY : status );
		}

	/* Check that the PKCS #5 padding is as expected.  Performing the check 
	   this way is the reverse of the way that it's usually done because we 
	   already know the payload size from the ASN.1 and can use this to 
	   determine the expected padding value and thus check that the end of 
	   the encrypted data hasn't been subject to a bit-flipping attack.  For 
	   example for RSA private keys the end of the data is:

		[ INTEGER u ][ INTEGER keySize ][ padding ]

	   where the keySize is encoded as a 4-byte value and the padding is 1-8 
	   bytes.  If the low bits of u are flipped there's a 5/8 chance that 
	   either the keySize value (checked in the RSA read code) or padding 
	   will be messed up, both of which will be detected (in addition the 
	   RSA key load checks try and verify u when the key is loaded).  For 
	   DLP keys the end of the data is:

		[ INTEGER x ][ padding ]

	   for which bit flipping is rather harder to detect since 7/8 of the 
	   time the following block won't be affected, however the DLP key load 
	   checks also verify x when the key is loaded.  The padding checking is 
	   effectively free and helps make Klima-Rosa type attacks harder */
	padPtr = ( const BYTE * ) data + length;
	padSize = blockSize - ( length & ( blockSize - 1 ) );
	if( padSize < 1 || padSize > CRYPT_MAX_IVSIZE || \
		length + padSize > dataLength )
		return( CRYPT_ERROR_BADDATA );
	for( i = 0; i < padSize; i++ )
		{
		if( padPtr[ i ] != padSize )
			return( CRYPT_ERROR_BADDATA );
		}

	return( CRYPT_OK );
	}

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkPgp2KeyIntegrity( IN_BUFFER( dataLength ) const void *data, 
								  IN_LENGTH_SHORT_MIN( 16 ) const int dataLength, 
								  IN_LENGTH_SHORT_MIN( 16 ) const int keyDataLength )
	{
	STREAM stream;
	const BYTE *keyData = data;
	int checksum = 0, storedChecksum, i, status;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength >= 16 && dataLength < MAX_INTLENGTH_SHORT );
	REQUIRES( keyDataLength >= 16 && \
			  keyDataLength + UINT16_SIZE <= dataLength && \
			  keyDataLength < MAX_INTLENGTH_SHORT );

	/* Calculate the checksum for the MPIs.  In theory we'd have to process
	   them all over again but the checksumming procedure is inconsistent
	   with the encryption in that only the MPI data is encrypted but the
	   overall length and data are checksummed.  Since these data blocks are
	   stored consecutively in memory we can checksum all MPI data as one
	   continuous block */
	for( i = 0; i < keyDataLength; i++ )
		checksum += keyData[ i ];
	checksum &= 0xFFFF;		/* MPI checksum is a 16-bit value */

	/* Recover the stored checksum that follows the MPI data and compare it 
	   to the calculated checksum */
	sMemConnect( &stream, keyData + keyDataLength, dataLength - keyDataLength );
	status = storedChecksum = readUint16( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) || checksum != storedChecksum )
		return( CRYPT_ERROR_WRONGKEY );
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkOpenPgpKeyIntegrity( IN_BUFFER( dataLength ) const void *data, 
									 IN_LENGTH_SHORT_MIN( 16 ) const int dataLength )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE hashValue[ CRYPT_MAX_HASHSIZE + 8 ];
	const BYTE *hashValuePtr;
	int hashSize;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength >= 16 && dataLength < MAX_INTLENGTH_SHORT );

	/* Get the hash algorithm info and make sure that there's room for 
	   minimal-length data and the checksum */
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
							 &hashSize );
	if( dataLength < bitsToBytes( 155 ) + hashSize )
		return( CRYPT_ERROR_BADDATA );
	hashValuePtr = ( const BYTE * ) data + dataLength - hashSize; 

	/* Hash the data and make sure that it matches the stored MDC */
	hashFunctionAtomic( hashValue, CRYPT_MAX_HASHSIZE, data, 
						dataLength - hashSize );
	if( !compareDataConstTime( hashValue, hashValuePtr, hashSize ) )
		return( CRYPT_ERROR_WRONGKEY );

	return( CRYPT_OK );
	}
#endif /* USE_PGP || USE_PGPKEYS */

/****************************************************************************
*																			*
*					PKCS #15 Private Key Wrap/Unwrap Mechanisms				*
*																			*
****************************************************************************/

/* Perform private key wrapping/unwrapping.  There are several variations of
   this that are handled through common private key wrap mechanism
   functions */

typedef enum { PRIVATEKEY_WRAP_NONE, PRIVATEKEY_WRAP_NORMAL,
			   PRIVATEKEY_WRAP_OLD, PRIVATEKEY_WRAP_LAST } PRIVATEKEY_WRAP_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int privateKeyWrap( STDC_UNUSED void *dummy, 
						   INOUT MECHANISM_WRAP_INFO *mechanismInfo,
						   IN_ENUM( PRIVATEKEY_WRAP ) \
							const PRIVATEKEY_WRAP_TYPE type )
	{
	const KEYFORMAT_TYPE formatType = ( type == PRIVATEKEY_WRAP_NORMAL ) ? \
								KEYFORMAT_PRIVATE : KEYFORMAT_PRIVATE_OLD;
	STREAM stream;
	int payloadSize = DUMMY_INIT, blockSize, padSize, status;

	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	REQUIRES( type > PRIVATEKEY_WRAP_NONE && type < PRIVATEKEY_WRAP_LAST );

	/* Clear return value */
	if( mechanismInfo->wrappedData != NULL )
		{
		memset( mechanismInfo->wrappedData, 0,
				mechanismInfo->wrappedDataLength );
		}

	/* Get the payload details */
	sMemNullOpen( &stream );
	status = exportPrivateKeyData( &stream, mechanismInfo->keyContext,
								   formatType, "private_key", 11 );
	if( cryptStatusOK( status ) )
		payloadSize = stell( &stream );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( mechanismInfo->wrapContext, 
							  IMESSAGE_GETATTRIBUTE, &blockSize,
							  CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusError( status ) )
		return( status );
	padSize = roundUp( payloadSize + 1, blockSize ) - payloadSize;
	
	ENSURES( !( ( payloadSize + padSize ) & ( blockSize - 1 ) ) );
	ENSURES( padSize >= 1 && padSize <= CRYPT_MAX_IVSIZE );

	/* If this is just a length check, we're done */
	if( mechanismInfo->wrappedData == NULL )
		{
		mechanismInfo->wrappedDataLength = payloadSize + padSize;
		return( CRYPT_OK );
		}
	ANALYSER_HINT( mechanismInfo->wrappedDataLength > MIN_PKCSIZE && \
				   mechanismInfo->wrappedDataLength < MAX_INTLENGTH_SHORT );

	/* Make sure that the wrapped key fits in the output buffer */
	if( payloadSize + padSize > mechanismInfo->wrappedDataLength )
		return( CRYPT_ERROR_OVERFLOW );

	/* Write the private key data, PKCS #5-pad it, and encrypt it */
	sMemOpen( &stream, mechanismInfo->wrappedData,
			  mechanismInfo->wrappedDataLength );
	status = exportPrivateKeyData( &stream, mechanismInfo->keyContext,
								   formatType, "private_key", 11 );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		{
		BYTE startSample[ 8 + 8 ], endSample[ 8 + 8 ];
		BYTE *dataPtr = mechanismInfo->wrappedData;
		const void *dataEndPtr = dataPtr + payloadSize + padSize - 8;
		int i;

		/* Sample the first and last 8 bytes of data so that we can check
		   that they really have been encrypted */
		memcpy( startSample, dataPtr, 8 );
		memcpy( endSample, dataEndPtr, 8 );

		/* Add the PKCS #5 padding and encrypt the data */
		for( i = 0; i < padSize; i++ )
			dataPtr[ payloadSize + i ] = intToByte( padSize );
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_ENCRYPT,
								  mechanismInfo->wrappedData,
								  payloadSize + padSize );

		/* Make sure that the original data samples differ from the final
		   data.  We don't perform a retIntError() exit at this point because
		   we need to continue and zeroise the data that we're working with */
		if( cryptStatusOK( status ) && \
			( !memcmp( startSample, dataPtr, 8 ) || \
			  !memcmp( endSample, dataEndPtr, 8 ) ) )
			{
			DEBUG_DIAG(( "Failed to encrypt private-key data" ));
			assert( DEBUG_WARN );
			status = CRYPT_ERROR_FAILED;
			}
		zeroise( startSample, 8 );
		zeroise( endSample, 8 );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( mechanismInfo->wrappedData,
				 mechanismInfo->wrappedDataLength );
		return( status );
		}
	mechanismInfo->wrappedDataLength = payloadSize + padSize;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int privateKeyUnwrap( STDC_UNUSED void *dummy, 
							 INOUT MECHANISM_WRAP_INFO *mechanismInfo,
							 IN_ENUM( PRIVATEKEY_WRAP ) \
								const PRIVATEKEY_WRAP_TYPE type )
	{
	const KEYFORMAT_TYPE formatType = ( type == PRIVATEKEY_WRAP_NORMAL ) ? \
								KEYFORMAT_PRIVATE : KEYFORMAT_PRIVATE_OLD;
	void *buffer;
	int blockSize, status, altStatus;

	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );
	
	REQUIRES( type > PRIVATEKEY_WRAP_NONE && type < PRIVATEKEY_WRAP_LAST );

	/* Make sure that the data has a sane length and is a multiple of the
	   cipher block size.  Since we force the use of CBC mode we know that 
	   it has to have this property.  Any required length checks have 
	   already been enforced by the kernel ACLs */
	status = krnlSendMessage( mechanismInfo->wrapContext,
							  IMESSAGE_GETATTRIBUTE, &blockSize,
							  CRYPT_CTXINFO_IVSIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( mechanismInfo->wrappedDataLength & ( blockSize - 1 ) )
		return( CRYPT_ERROR_BADDATA );

	/* Copy the encrypted private key data to a temporary pagelocked buffer, 
	   decrypt it, and read it into the context.  If we get a corrupted-data 
	   error then it's far more likely to be because we decrypted with the 
	   wrong key than because any data was corrupted so we convert it to a 
	   wrong-key error */
	if( ( status = krnlMemalloc( &buffer, \
							mechanismInfo->wrappedDataLength ) ) != CRYPT_OK )
		return( status );
	memcpy( buffer, mechanismInfo->wrappedData,
			mechanismInfo->wrappedDataLength );
	status = krnlSendMessage( mechanismInfo->wrapContext,
							  IMESSAGE_CTX_DECRYPT, buffer,
							  mechanismInfo->wrappedDataLength );
	if( cryptStatusOK( status ) )
		{
		status = checkKeyIntegrity( buffer, 
									mechanismInfo->wrappedDataLength, 
									blockSize );
		}
	if( cryptStatusOK( status ) )
		{
		STREAM stream;

		sMemConnect( &stream, buffer, mechanismInfo->wrappedDataLength );
		status = importPrivateKeyData( &stream, mechanismInfo->keyContext,
									   formatType );
		sMemDisconnect( &stream );
		}
	zeroise( buffer, mechanismInfo->wrappedDataLength );
	altStatus = krnlMemfree( &buffer );
	ENSURES( cryptStatusOK( altStatus ) );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPrivateKey( STDC_UNUSED void *dummy, 
					  INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyWrap( dummy, mechanismInfo, PRIVATEKEY_WRAP_NORMAL ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKey( STDC_UNUSED void *dummy, 
					  INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyUnwrap( dummy, mechanismInfo, PRIVATEKEY_WRAP_NORMAL ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPrivateKeyPKCS8( STDC_UNUSED void *dummy, 
						   INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyWrap( dummy, mechanismInfo, PRIVATEKEY_WRAP_OLD ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyPKCS8( STDC_UNUSED void *dummy, 
						   INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyUnwrap( dummy, mechanismInfo, PRIVATEKEY_WRAP_OLD ) );
	}

/****************************************************************************
*																			*
*						PGP Private Key Wrap/Unwrap Mechanisms				*
*																			*
****************************************************************************/

#ifdef USE_PGPKEYS

/* Perform PGP private key wrapping/unwrapping.  There are several variations
   of this that are handled through common private key wrap mechanism
   functions.  The variations are:

	PGP2: mpi_enc( d ), mpi_enc( p ), mpi_enc( q ), mpi_enc( u ),
		  uint16 checksum
	
	OpenPGP_Old: enc( mpi [...], 
					  uint16 checksum )
	
	OpenPGP: enc( mpi [...], 
				  byte[20] mdc ) */

typedef enum { PRIVATEKEYPGP_WRAP_NONE, PRIVATEKEYPGP_WRAP_PGP2, 
			   PRIVATEKEYPGP_WRAP_OPENPGP_OLD,
			   PRIVATEKEYPGP_WRAP_OPENPGP, 
			   PRIVATEKEYPGP_WRAP_LAST } PRIVATEKEYPGP_WRAP_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int privateKeyUnwrapPGP( STDC_UNUSED void *dummy,
								INOUT MECHANISM_WRAP_INFO *mechanismInfo,
								IN_ENUM( PRIVATEKEYPGP_WRAP ) \
									const PRIVATEKEYPGP_WRAP_TYPE type )
	{
	STREAM stream;
	void *buffer;
	int pkcAlgorithm, bytesToChecksum = DUMMY_INIT, status, altStatus;

	UNUSED_ARG( dummy );

	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	REQUIRES( type > PRIVATEKEYPGP_WRAP_NONE && \
			  type < PRIVATEKEYPGP_WRAP_LAST );

	/* Get various algorithm parameters */
	status = krnlSendMessage( mechanismInfo->keyContext,
							  IMESSAGE_GETATTRIBUTE, &pkcAlgorithm,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );

	/* Copy the encrypted private key data to a temporary buffer and decrypt 
	   it */
	if( ( status = krnlMemalloc( &buffer, \
						mechanismInfo->wrappedDataLength ) ) != CRYPT_OK )
		return( status );
	memcpy( buffer, mechanismInfo->wrappedData,
			mechanismInfo->wrappedDataLength );
	if( type == PRIVATEKEYPGP_WRAP_PGP2 )
		{
		status = pgp2DecryptKey( buffer, mechanismInfo->wrappedDataLength,
								 &bytesToChecksum, mechanismInfo->wrapContext, 
								 ( pkcAlgorithm != CRYPT_ALGO_RSA ) ? \
									TRUE : FALSE );
		}
	else
		{
		status = krnlSendMessage( mechanismInfo->wrapContext,
								  IMESSAGE_CTX_DECRYPT, buffer,
								  mechanismInfo->wrappedDataLength );
		}
	if( cryptStatusError( status ) )
		{
		zeroise( buffer, mechanismInfo->wrappedDataLength );
		altStatus = krnlMemfree( &buffer );
		ENSURES( cryptStatusOK( altStatus ) );
		return( status );
		}

	/* Perform one of PGP's assorted key checksumming operations and read 
	   the key data into the context */
	if( type == PRIVATEKEYPGP_WRAP_PGP2 || \
		type == PRIVATEKEYPGP_WRAP_OPENPGP_OLD )
		{
		/* Before the use of MDCs for private-key data there was a mutant 
		   halfway stage that used the PGP 2.x checksum but encrypted all of 
		   the key data in the OpenPGP manner.  If we're using this halfway 
		   variant then the amount of data to checksum is the total amount
		   minus the size of the checksum */
		if( type == PRIVATEKEYPGP_WRAP_OPENPGP_OLD )
			bytesToChecksum = mechanismInfo->wrappedDataLength - UINT16_SIZE;
		status = checkPgp2KeyIntegrity( buffer, mechanismInfo->wrappedDataLength,
										bytesToChecksum );
		}
	else
		{
		status = checkOpenPgpKeyIntegrity( buffer, 
										   mechanismInfo->wrappedDataLength );
		}
	if( cryptStatusOK( status ) )
		{
		sMemConnect( &stream, buffer, mechanismInfo->wrappedDataLength );
		status = importPrivateKeyData( &stream, mechanismInfo->keyContext,
									   KEYFORMAT_PGP );
		sMemDisconnect( &stream );
		if( status == CRYPT_ERROR_BADDATA )
			status = CRYPT_ERROR_WRONGKEY;
		}
	zeroise( buffer, mechanismInfo->wrappedDataLength );
	altStatus = krnlMemfree( &buffer );
	ENSURES( cryptStatusOK( altStatus ) );

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyPGP2( STDC_UNUSED void *dummy, 
						  INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyUnwrapPGP( dummy, mechanismInfo,
								 PRIVATEKEYPGP_WRAP_PGP2 ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyOpenPGPOld( STDC_UNUSED void *dummy, 
								INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyUnwrapPGP( dummy, mechanismInfo,
								 PRIVATEKEYPGP_WRAP_OPENPGP_OLD ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyOpenPGP( STDC_UNUSED void *dummy, 
							 INOUT MECHANISM_WRAP_INFO *mechanismInfo )
	{
	assert( isWritePtr( mechanismInfo, sizeof( MECHANISM_WRAP_INFO ) ) );

	return( privateKeyUnwrapPGP( dummy, mechanismInfo,
								 PRIVATEKEYPGP_WRAP_OPENPGP ) );
	}
#endif /* USE_PGPKEYS */
