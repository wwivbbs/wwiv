/****************************************************************************
*																			*
*						cryptlib SSHv1 Session Management					*
*						Copyright Peter Gutmann 1998-2001					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "session.h"
  #include "ssh.h"
#else
  #include "crypt.h"
  #include "session/session.h"
  #include "session/ssh.h"
#endif /* Compiler-specific includes */

#ifdef USE_SSH1

#error The SSHv1 protocol is insecure and obsolete, and this code is completely 
#error unsupported.  You should only enable this code if it's absolutely necessary, 
#error and your warranty is void when you do so.  Use this code at your own risk.

/* Determine the number of padding bytes required to make the packet size a
   multiple of 8 bytes */

#define getPadLength( length ) \
		( 8 - ( ( ID_SIZE + ( length ) + SSH1_CRC_SIZE ) & 7 ) )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Byte-reverse an array of 32-bit words, needed for Blowfish encryption,
   which in the original SSH implementation got the endianness wrong.  This
   code is safe even for CPUs with a word size > 32 bits since on a little-
   endian CPU the important 32 bits are stored first, so that by zeroizing
   the first 32 bits and or-ing the reversed value back in we don't have to
   rely on the processor only writing 32 bits into memory */

static void longReverse( unsigned long *buffer, int count )
	{
#if defined( SYSTEM_64BIT )
	BYTE *bufPtr = ( BYTE * ) buffer, temp;

	assert( ( count % 4 ) == 0 );

	count /= 4;		/* sizeof( unsigned long ) != 4 */
	while( count-- > 0 )
		{
  #if 0
		unsigned long temp;

		/* This code is cursed */
		temp = value = *buffer & 0xFFFFFFFFUL;
		value = ( ( value & 0xFF00FF00UL ) >> 8  ) | \
				( ( value & 0x00FF00FFUL ) << 8 );
		value = ( ( value << 16 ) | ( value >> 16 ) ) ^ temp;
		*buffer ^= value;
		buffer = ( unsigned long * ) ( ( BYTE * ) buffer + 4 );
  #endif /* 0 */
		/* There's really no nice way to do this - the above code generates
		   misaligned accesses on processors with a word size > 32 bits, so
		   we have to work at the byte level (either that or turn misaligned
		   access warnings off by trapping the signal the access corresponds
		   to, however a context switch per memory access is probably
		   somewhat slower than the current byte-twiddling mess) */
		temp = bufPtr[ 3 ];
		bufPtr[ 3 ] = bufPtr[ 0 ];
		bufPtr[ 0 ] = temp;
		temp = bufPtr[ 2 ];
		bufPtr[ 2 ] = bufPtr[ 1 ];
		bufPtr[ 1 ] = temp;
		bufPtr += 4;
		}
#elif defined( __WIN32__ )
	assert( ( count % 4 ) == 0 );

	/* The following code, which makes use of bswap, is rather faster than
	   what the compiler would otherwise generate */
__asm {
	mov ecx, count
	mov edx, buffer
	shr ecx, 2
swapLoop:
	mov eax, [edx]
	bswap eax
	mov [edx], eax
	add edx, 4
	dec ecx
	jnz swapLoop
	}
#else
	unsigned long value;

	assert( ( count % 4 ) == 0 );
	assert( sizeof( unsigned long ) == 4 );

	count /= sizeof( unsigned long );
	while( count-- > 0 )
		{
		value = *buffer;
		value = ( ( value & 0xFF00FF00UL ) >> 8  ) | \
				( ( value & 0x00FF00FFUL ) << 8 );
		*buffer++ = ( value << 16 ) | ( value >> 16 );
		}
#endif /* SYSTEM_64BIT */
	}

/* Calculate the CRC32 for a data block.  This uses the slightly nonstandard
   variant from the original SSH code, which calculates the UART-style
   reflected value and doesn't pre-set the value to all ones (done to to
   catch leading zero bytes, which happens quite a bit with SSH because of
   the 32-bit length at the start) or XOR it with all ones before returning
   it.  This means that the resulting CRC is not the same as the one in
   Ethernet, Pkzip, and most other implementations */

static const FAR_BSS unsigned long crc32table[] = {
	0x00000000UL, 0x77073096UL, 0xEE0E612CUL, 0x990951BAUL,
	0x076DC419UL, 0x706AF48FUL, 0xE963A535UL, 0x9E6495A3UL,
	0x0EDB8832UL, 0x79DCB8A4UL, 0xE0D5E91EUL, 0x97D2D988UL,
	0x09B64C2BUL, 0x7EB17CBDUL, 0xE7B82D07UL, 0x90BF1D91UL,
	0x1DB71064UL, 0x6AB020F2UL, 0xF3B97148UL, 0x84BE41DEUL,
	0x1ADAD47DUL, 0x6DDDE4EBUL, 0xF4D4B551UL, 0x83D385C7UL,
	0x136C9856UL, 0x646BA8C0UL, 0xFD62F97AUL, 0x8A65C9ECUL,
	0x14015C4FUL, 0x63066CD9UL, 0xFA0F3D63UL, 0x8D080DF5UL,
	0x3B6E20C8UL, 0x4C69105EUL, 0xD56041E4UL, 0xA2677172UL,
	0x3C03E4D1UL, 0x4B04D447UL, 0xD20D85FDUL, 0xA50AB56BUL,
	0x35B5A8FAUL, 0x42B2986CUL, 0xDBBBC9D6UL, 0xACBCF940UL,
	0x32D86CE3UL, 0x45DF5C75UL, 0xDCD60DCFUL, 0xABD13D59UL,
	0x26D930ACUL, 0x51DE003AUL, 0xC8D75180UL, 0xBFD06116UL,
	0x21B4F4B5UL, 0x56B3C423UL, 0xCFBA9599UL, 0xB8BDA50FUL,
	0x2802B89EUL, 0x5F058808UL, 0xC60CD9B2UL, 0xB10BE924UL,
	0x2F6F7C87UL, 0x58684C11UL, 0xC1611DABUL, 0xB6662D3DUL,
	0x76DC4190UL, 0x01DB7106UL, 0x98D220BCUL, 0xEFD5102AUL,
	0x71B18589UL, 0x06B6B51FUL, 0x9FBFE4A5UL, 0xE8B8D433UL,
	0x7807C9A2UL, 0x0F00F934UL, 0x9609A88EUL, 0xE10E9818UL,
	0x7F6A0DBBUL, 0x086D3D2DUL, 0x91646C97UL, 0xE6635C01UL,
	0x6B6B51F4UL, 0x1C6C6162UL, 0x856530D8UL, 0xF262004EUL,
	0x6C0695EDUL, 0x1B01A57BUL, 0x8208F4C1UL, 0xF50FC457UL,
	0x65B0D9C6UL, 0x12B7E950UL, 0x8BBEB8EAUL, 0xFCB9887CUL,
	0x62DD1DDFUL, 0x15DA2D49UL, 0x8CD37CF3UL, 0xFBD44C65UL,
	0x4DB26158UL, 0x3AB551CEUL, 0xA3BC0074UL, 0xD4BB30E2UL,
	0x4ADFA541UL, 0x3DD895D7UL, 0xA4D1C46DUL, 0xD3D6F4FBUL,
	0x4369E96AUL, 0x346ED9FCUL, 0xAD678846UL, 0xDA60B8D0UL,
	0x44042D73UL, 0x33031DE5UL, 0xAA0A4C5FUL, 0xDD0D7CC9UL,
	0x5005713CUL, 0x270241AAUL, 0xBE0B1010UL, 0xC90C2086UL,
	0x5768B525UL, 0x206F85B3UL, 0xB966D409UL, 0xCE61E49FUL,
	0x5EDEF90EUL, 0x29D9C998UL, 0xB0D09822UL, 0xC7D7A8B4UL,
	0x59B33D17UL, 0x2EB40D81UL, 0xB7BD5C3BUL, 0xC0BA6CADUL,
	0xEDB88320UL, 0x9ABFB3B6UL, 0x03B6E20CUL, 0x74B1D29AUL,
	0xEAD54739UL, 0x9DD277AFUL, 0x04DB2615UL, 0x73DC1683UL,
	0xE3630B12UL, 0x94643B84UL, 0x0D6D6A3EUL, 0x7A6A5AA8UL,
	0xE40ECF0BUL, 0x9309FF9DUL, 0x0A00AE27UL, 0x7D079EB1UL,
	0xF00F9344UL, 0x8708A3D2UL, 0x1E01F268UL, 0x6906C2FEUL,
	0xF762575DUL, 0x806567CBUL, 0x196C3671UL, 0x6E6B06E7UL,
	0xFED41B76UL, 0x89D32BE0UL, 0x10DA7A5AUL, 0x67DD4ACCUL,
	0xF9B9DF6FUL, 0x8EBEEFF9UL, 0x17B7BE43UL, 0x60B08ED5UL,
	0xD6D6A3E8UL, 0xA1D1937EUL, 0x38D8C2C4UL, 0x4FDFF252UL,
	0xD1BB67F1UL, 0xA6BC5767UL, 0x3FB506DDUL, 0x48B2364BUL,
	0xD80D2BDAUL, 0xAF0A1B4CUL, 0x36034AF6UL, 0x41047A60UL,
	0xDF60EFC3UL, 0xA867DF55UL, 0x316E8EEFUL, 0x4669BE79UL,
	0xCB61B38CUL, 0xBC66831AUL, 0x256FD2A0UL, 0x5268E236UL,
	0xCC0C7795UL, 0xBB0B4703UL, 0x220216B9UL, 0x5505262FUL,
	0xC5BA3BBEUL, 0xB2BD0B28UL, 0x2BB45A92UL, 0x5CB36A04UL,
	0xC2D7FFA7UL, 0xB5D0CF31UL, 0x2CD99E8BUL, 0x5BDEAE1DUL,
	0x9B64C2B0UL, 0xEC63F226UL, 0x756AA39CUL, 0x026D930AUL,
	0x9C0906A9UL, 0xEB0E363FUL, 0x72076785UL, 0x05005713UL,
	0x95BF4A82UL, 0xE2B87A14UL, 0x7BB12BAEUL, 0x0CB61B38UL,
	0x92D28E9BUL, 0xE5D5BE0DUL, 0x7CDCEFB7UL, 0x0BDBDF21UL,
	0x86D3D2D4UL, 0xF1D4E242UL, 0x68DDB3F8UL, 0x1FDA836EUL,
	0x81BE16CDUL, 0xF6B9265BUL, 0x6FB077E1UL, 0x18B74777UL,
	0x88085AE6UL, 0xFF0F6A70UL, 0x66063BCAUL, 0x11010B5CUL,
	0x8F659EFFUL, 0xF862AE69UL, 0x616BFFD3UL, 0x166CCF45UL,
	0xA00AE278UL, 0xD70DD2EEUL, 0x4E048354UL, 0x3903B3C2UL,
	0xA7672661UL, 0xD06016F7UL, 0x4969474DUL, 0x3E6E77DBUL,
	0xAED16A4AUL, 0xD9D65ADCUL, 0x40DF0B66UL, 0x37D83BF0UL,
	0xA9BCAE53UL, 0xDEBB9EC5UL, 0x47B2CF7FUL, 0x30B5FFE9UL,
	0xBDBDF21CUL, 0xCABAC28AUL, 0x53B39330UL, 0x24B4A3A6UL,
	0xBAD03605UL, 0xCDD70693UL, 0x54DE5729UL, 0x23D967BFUL,
	0xB3667A2EUL, 0xC4614AB8UL, 0x5D681B02UL, 0x2A6F2B94UL,
	0xB40BBE37UL, 0xC30C8EA1UL, 0x5A05DF1BUL, 0x2D02EF8DUL
	};

static unsigned long calculateCRC( const BYTE *data, const int dataLength )
	{
	unsigned long crc32 = 0;
	int i;

	for( i = 0; i < dataLength; i++ )
		crc32 = crc32table[ ( int ) ( crc32 ^ data[ i ] ) & 0xFF ] ^ ( crc32 >> 8 );

	return( crc32 );
	}

/* If we detect that the other side is also running cryptlib, we use a
   truncated MAC instead of a CRC32 once we're running in secure mode.  This
   prevents extension attacks on the CRC-protected data.  The reason for
   the truncation is to make it the same length as the CRC, this makes it a
   bit weaker than a full-length MAC but no weaker than a cryptographically
   strong CRC (in any case the chances of a successful attack are only 1/2^31,
   since you only get one chance to get it right) */

static unsigned long calculateTruncatedMAC( const CRYPT_CONTEXT iMacContext,
											const BYTE *data,
											const int dataLength )
	{
	MESSAGE_DATA msgData;
	BYTE macBuffer[ CRYPT_MAX_HASHSIZE + 8 ], *bufPtr = macBuffer;
	unsigned long macValue;
	int status;

	/* MAC the data and return the 32-bit truncated MAC value */
	krnlSendMessage( iMacContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_HASHVALUE );
	krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, ( void * ) data,
					 dataLength );
	krnlSendMessage( iMacContext, IMESSAGE_CTX_HASH, "", 0 );
	setMessageData( &msgData, macBuffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iMacContext, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CTXINFO_HASHVALUE );
	if( cryptStatusError( status ) )
		return( 0 );	/* Will cause a protocol failure */
	macValue = mgetLong( bufPtr );

	return( macValue );
	}

/* Convert an SSHv1 algorithm ID to a cryptlib ID in preferred-algorithm
   order, and return a list of available algorithms in SSHv1 cipher-mask
   format.  We can't use 3DES since this uses inner-CBC which is both
   nonstandard and has known (although not serious) weaknesses.  If we
   wanted to implement it in a portable manner (i.e. usable with external
   drivers and devices) we'd have to synthesize it using three lots of
   DES-CBC since nothing implements the variant that SSHv1 uses.  SSHv2
   fixed this so it's no longer a problem in that case */

static CRYPT_ALGO_TYPE maskToAlgoID( const int value )
	{
	if( ( value & ( 1 << SSH1_CIPHER_BLOWFISH ) ) && \
		algoAvailable( CRYPT_ALGO_BLOWFISH ) )
		return( CRYPT_ALGO_BLOWFISH );
	if( ( value & ( 1 << SSH1_CIPHER_IDEA ) ) && \
		algoAvailable( CRYPT_ALGO_IDEA ) )
		return( CRYPT_ALGO_IDEA );
	if( ( value & ( 1 << SSH1_CIPHER_RC4 ) ) && \
		algoAvailable( CRYPT_ALGO_RC4 ) )
		return( CRYPT_ALGO_RC4 );
	if( value & ( 1 << SSH1_CIPHER_DES ) )
		return( CRYPT_ALGO_DES );

	return( CRYPT_ALGO_NONE );
	}

static CRYPT_ALGO_TYPE sshIDToAlgoID( const int value )
	{
	switch( value )
		{
		case SSH1_CIPHER_IDEA:
			if( algoAvailable( CRYPT_ALGO_IDEA ) )
				return( CRYPT_ALGO_IDEA );
			break;

		case SSH1_CIPHER_DES:
			return( CRYPT_ALGO_DES );

		case SSH1_CIPHER_RC4:
			if( algoAvailable( CRYPT_ALGO_RC4 ) )
				return( CRYPT_ALGO_RC4 );
			break;

		case SSH1_CIPHER_BLOWFISH:
			if( algoAvailable( CRYPT_ALGO_BLOWFISH ) )
				return( CRYPT_ALGO_BLOWFISH );
			break;
		}

	return( CRYPT_ALGO_NONE );
	}

static long getAlgorithmMask( void )
	{
	long value = 0;

	if( algoAvailable( CRYPT_ALGO_DES ) )
		value |= 1 << SSH1_CIPHER_DES;
	if( algoAvailable( CRYPT_ALGO_BLOWFISH ) )
		value |= 1 << SSH1_CIPHER_BLOWFISH;
	if( algoAvailable( CRYPT_ALGO_IDEA ) )
		value |= 1 << SSH1_CIPHER_IDEA;
	if( algoAvailable( CRYPT_ALGO_RC4 ) )
		value |= 1 << SSH1_CIPHER_RC4;

	return( value );
	}

/* Encode a value as an SSH string */

static int encodeString( BYTE *buffer, const BYTE *string,
						 const int stringLength )
	{
	BYTE *bufPtr = buffer;
	const int length = ( stringLength > 0 ) ? stringLength : strlen( string );

	if( buffer != NULL )
		{
		mputLong( bufPtr, length );
		memcpy( bufPtr, string, length );
		}
	return( LENGTH_SIZE + length );
	}

/* Generate an SSH session ID */

static void generateSessionID( SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;

	/* Get the hash algorithm information and hash the server key modulus,
	   host key modulus, and cookie.  The SSH documentation and source code
	   are quite confusing on this issue, giving the key components to be
	   hashed multiple names (server key, host key, session key, public key,
	   etc etc).  The correct order is:
		hash( host modulus || server modulus || cookie ) */
	getHashParameters( CRYPT_ALGO_MD5, &hashFunction, NULL );
	hashFunction( hashInfo, NULL, handshakeInfo->hostModulus,
				  handshakeInfo->hostModulusLength, HASH_START );
	hashFunction( hashInfo, NULL, handshakeInfo->serverModulus,
				  handshakeInfo->serverModulusLength, HASH_CONTINUE );
	hashFunction( hashInfo, handshakeInfo->sessionID,
				  handshakeInfo->cookie, SSH1_COOKIE_SIZE, HASH_END );
	handshakeInfo->sessionIDlength = SSH1_SESSIONID_SIZE;
	}

/* Generate/check an SSHv1 key fingerprint.  This is the same easily-
   spoofable MD5 hash of the raw RSA n and e values used by PGP 2.x */

static int processKeyFingerprint( SESSION_INFO *sessionInfoPtr,
								  const void *n, const int nLength,
								  const void *e, int eLength )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	const ATTRIBUTE_LIST *attributeListPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_SERVER_FINGERPRINT );
	BYTE fingerPrint[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashSize;

	getHashParameters( CRYPT_ALGO_MD5, &hashFunction, &hashSize );
	hashFunction( hashInfo, NULL, n, nLength, HASH_START );
	hashFunction( hashInfo, fingerPrint, e, eLength, HASH_END );
	if( attributeListPtr == NULL )
		/* Remember the value for the caller */
		return( addSessionInfo( &sessionInfoPtr->attributeList,
								CRYPT_SESSINFO_SERVER_FINGERPRINT,
								fingerPrint, hashSize ) );

	/* There's an existing fingerprint value, make sure that it matches what
	   we just calculated */
	if( attributeListPtr->valueLength != hashSize || \
		memcmp( attributeListPtr->value, fingerPrint, hashSize ) )
		retExt( sessionInfoPtr, CRYPT_ERROR_WRONGKEY,
				"Server key fingerprint doesn't match requested "
				"fingerprint" );
	return( CRYPT_OK );
	}

/* Generate a response to an RSA authentication challenge */

static void generateChallengeResponse( BYTE *response,
									   const SSH_HANDSHAKE_INFO *handshakeInfo,
									   const BYTE *challenge )
	{
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;

	/* Hash the session ID and challenge:
		hash( sessionID || challenge ) */
	getHashParameters( CRYPT_ALGO_MD5, &hashFunction, NULL );
	hashFunction( hashInfo, NULL, ( BYTE * ) handshakeInfo->sessionID,
				  handshakeInfo->sessionIDlength, HASH_START );
	hashFunction( hashInfo, response, ( BYTE * ) challenge,
				  SSH1_CHALLENGE_SIZE, HASH_END );
	}

/* Process the public key data.  The preceding key length value isn't useful
   because it contains the nominal key size in bits rather than the size of
   the following data, so we have to dig into the data to find out how much
   there is.  In addition we need to take a copy of the key modulus since
   it's needed later for calculating the session ID */

static int processPublickeyData( SSH_HANDSHAKE_INFO *handshakeInfo,
								 const void *data, const int dataLength,
								 const BOOLEAN isServerKey,
								 SESSION_INFO *sessionInfoPtr )
	{
	BYTE *dataPtr = ( BYTE * ) data, *ePtr;
	int nominalLength, eLength, nLength;

	nominalLength = ( int ) mgetLong( dataPtr );
	nominalLength = bitsToBytes( nominalLength );
	if( nominalLength < bitsToBytes( MIN_PKCSIZE_BITS ) || \
		nominalLength > CRYPT_MAX_PKCSIZE )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid public key size %d", nominalLength );
	eLength = mgetWord( dataPtr );
	eLength = bitsToBytes( eLength );
	if( LENGTH_SIZE + SSH1_MPI_LENGTH_SIZE + eLength + \
					  SSH1_MPI_LENGTH_SIZE + nominalLength > dataLength )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid exponent size %d for key size %d", eLength,
				nominalLength );
	ePtr = dataPtr;
	dataPtr += eLength;
	nLength = mgetWord( dataPtr );
	nLength = bitsToBytes( nLength );
	if( nLength != nominalLength )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Public key size %d doesn't match modulus size %d",
				nominalLength, nLength );
	if( isServerKey )
		{
		memcpy( handshakeInfo->serverModulus, dataPtr, nLength );
		handshakeInfo->serverModulusLength = nLength;
		}
	else
		{
		memcpy( handshakeInfo->hostModulus, dataPtr, nLength );
		handshakeInfo->hostModulusLength = nLength;
		}
	if( sessionInfoPtr != NULL )
		{
		int status;

		status = processKeyFingerprint( sessionInfoPtr, dataPtr, nLength,
										ePtr, eLength );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( LENGTH_SIZE + SSH1_MPI_LENGTH_SIZE + eLength + \
						  SSH1_MPI_LENGTH_SIZE + nLength );
	}

/* Set up the security information required for the session */

static int initSecurityInfoSSH1( SESSION_INFO *sessionInfoPtr,
								 SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_DATA msgData;
	int keySize, ivSize, status;

	/* Create the security contexts required for the session */
	status = initSecurityContextsSSH( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_BLOWFISH )
		/* For Blowfish the session key size doesn't match the default
		   Blowfish key size so we explicitly specify its length */
		keySize = SSH1_SECRET_SIZE;
	else
		krnlSendMessage( sessionInfoPtr->iCryptInContext,
						 IMESSAGE_GETATTRIBUTE, &keySize,
						 CRYPT_CTXINFO_KEYSIZE );
	if( krnlSendMessage( sessionInfoPtr->iCryptInContext,
						 IMESSAGE_GETATTRIBUTE, &ivSize,
						 CRYPT_CTXINFO_IVSIZE ) == CRYPT_ERROR_NOTAVAIL )
		/* It's a stream cipher */
		ivSize = 0;

	/* Load the keys.  For RC4, which is IV-less, the session key is split
	   into two parts, with the first part being the receive key and the
	   second part being the send key.  For other algorithms, the entire
	   session key is used for both send and receive contexts, leading to
	   a simple attack on the first data block since the initial IV is all
	   zeroes */
	setMessageData( &msgData, ( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_RC4 ) ? \
					handshakeInfo->secretValue + 16 : handshakeInfo->secretValue,
					keySize );
	status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_KEY );
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, handshakeInfo->secretValue, keySize );
		status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_KEY );
		}
	if( cryptStatusOK( status ) && ivSize > 0 )
		{
		static const char iv[ CRYPT_MAX_IVSIZE ] = { 0 };

		setMessageData( &msgData, ( void * ) iv, ivSize );
		krnlSendMessage( sessionInfoPtr->iCryptOutContext,
						 IMESSAGE_SETATTRIBUTE_S, &msgData, CRYPT_CTXINFO_IV );
		setMessageData( &msgData, ( void * ) iv, ivSize );
		krnlSendMessage( sessionInfoPtr->iCryptInContext,
						 IMESSAGE_SETATTRIBUTE_S, &msgData, CRYPT_CTXINFO_IV );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If we're talking to a cryptlib peer, set up the MAC context which is
	   used instead of a CRC32.  The key we use for this is taken from the
	   end of the SSH secret data, which isn't used for any cipher except
	   Blowfish */
	if( sessionInfoPtr->flags & SESSION_ISCRYPTLIB )
		{
		setMessageData( &msgData,
				handshakeInfo->secretValue + ( SSH1_SECRET_SIZE - 16 ), 16 );
		status = krnlSendMessage( sessionInfoPtr->iAuthInContext,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_CTXINFO_KEY );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* We've set up the security information, from now on all data is 
	   encrypted */
	sessionInfoPtr->flags |= SESSION_ISSECURE_READ | SESSION_ISSECURE_WRITE;

	return( CRYPT_OK );
	}

/* Read an SSH packet */

static int decryptPayload( SESSION_INFO *sessionInfoPtr, BYTE *buffer,
						   const int length )
	{
	int status;

	/* Decrypt the payload, with handling for SSH's Blowfish endianness bug.
	   This may not be a true bug but more a problem in the spec, since the
	   original was rather vague about the endianness of the byte -> long
	   conversion */
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_BLOWFISH )
		longReverse( ( unsigned long * ) buffer, length );
	status = krnlSendMessage( sessionInfoPtr->iCryptInContext,
							  IMESSAGE_CTX_DECRYPT, buffer, length );
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_BLOWFISH )
		longReverse( ( unsigned long * ) buffer, length );
	return( status );
	}

static BOOLEAN checksumPayload( SESSION_INFO *sessionInfoPtr,
								const BYTE *buffer, const int length )
	{
	const int dataLength = length - SSH1_CRC_SIZE;	/* CRC isn't part of payload */
	BYTE *bufPtr = ( BYTE * ) buffer + dataLength;
	unsigned long crc32, storedCrc32;

	/* Calculate the checksum over the padding, type, and data and make sure
	   that it matches the transmitted value */
	if( ( sessionInfoPtr->flags & ( SESSION_ISCRYPTLIB | SESSION_ISSECURE_READ ) ) == \
								  ( SESSION_ISCRYPTLIB | SESSION_ISSECURE_READ ) )
		crc32 = calculateTruncatedMAC( sessionInfoPtr->iAuthInContext,
									   buffer, dataLength );
	else
		crc32 = calculateCRC( buffer, dataLength );
	storedCrc32 = mgetLong( bufPtr );
	return( ( crc32 == storedCrc32 ) ? TRUE : FALSE );
	}

static int getDisconnectInfoSSH1( SESSION_INFO *sessionInfoPtr, BYTE *bufPtr )
	{
	int length;

	/* Server is disconnecting, find out why */
	length = mgetLong( bufPtr );
	if( length > MAX_ERRMSG_SIZE - 32 )
		retExt( sessionInfoPtr, CRYPT_ERROR_OVERFLOW,
				"Invalid error information size %d", length );
	strlcpy_s( sessionInfoPtr->errorMessage, MAX_ERRMSG_SIZE,
			   "Received SSHv1 server message: " );
	memcpy( sessionInfoPtr->errorMessage + 31, bufPtr, length );
	sessionInfoPtr->errorMessage[ 31 + length ] = '\0';

	return( CRYPT_ERROR_READ );
	}

static int readPacketSSH1( SESSION_INFO *sessionInfoPtr, int expectedType )
	{
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer;
	long length;
	int padLength, packetType, iterationCount = 0;

	/* Alongside the expected packets the server can also send us all sorts
	   of no-op messages, ranging from explicit no-ops (SSH_MSG_IGNORE)
	   through to general chattiness (SSH_MSG_DEBUG).  Because we can
	   receive any quantity of these at any time, we have to run the receive
	   code in a loop to strip them out */
	do
		{
		const BYTE *lengthPtr = bufPtr;
		int status;

		/* Read the SSHv1 packet header:

			uint32		length (excluding padding)
			byte[]		padding
			byte		type
			byte[]		data
			uint32		crc32

		  The padding length is implicitly calculated as
		  8 - ( length & 7 ) bytes, and the CRC is calculated over the
		  padding, type, and data */
		assert( sessionInfoPtr->receiveBufEnd == 0 );
		status = readFixedHeader( sessionInfoPtr, LENGTH_SIZE );
		if( cryptStatusError( status ) )
			return( status );
		assert( status == LENGTH_SIZE );
		length = mgetLong( lengthPtr );
		padLength = 8 - ( length & 7 );
		if( length < SSH1_HEADER_SIZE || \
			length + padLength >= sessionInfoPtr->receiveBufSize )
			retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
					"Invalid packet length %d", length );
		status = sread( &sessionInfoPtr->stream,
						sessionInfoPtr->receiveBuffer, padLength + length );
		if( cryptStatusError( status ) )
			{
			sNetGetErrorInfo( &sessionInfoPtr->stream,
							  &sessionInfoPtr->errorInfo );
			return( status );
			}
		if( status != padLength + length )
			retExt( sessionInfoPtr, CRYPT_ERROR_TIMEOUT,
					"Timeout during packet remainder read, only got %d of "
					"%d bytes", status, padLength + length );
		if( sessionInfoPtr->flags & SESSION_ISSECURE_READ )
			{
			status = decryptPayload( sessionInfoPtr,
									 sessionInfoPtr->receiveBuffer,
									 padLength + length );
			if( cryptStatusError( status ) )
				return( status );
			}
		if( !checksumPayload( sessionInfoPtr, sessionInfoPtr->receiveBuffer,
							  padLength + length ) )
			/* If we're expecting a success packet after a key exchange or an
			   immediate post key-exchange packet and don't get it then it's
			   more likely that the problem is due to the wrong key being
			   used than data corruption, so we return a wrong key error
			   instead of bad data */
			retExt( sessionInfoPtr, ( expectedType == SSH1_SMSG_SUCCESS ) ? \
						CRYPT_ERROR_WRONGKEY : CRYPT_ERROR_BADDATA,
					"Bad message checksum" );
		packetType = sessionInfoPtr->receiveBuffer[ padLength ];
		}
	while( ( packetType == SSH1_MSG_IGNORE || \
			 packetType == SSH1_MSG_DEBUG ) && iterationCount++ < 1000 );
	if( iterationCount >= 1000 )
		retExt( sessionInfoPtr, CRYPT_ERROR_OVERFLOW,
				"Peer sent excessive number of no-op packets" );
	length -= ID_SIZE + UINT_SIZE;	/* Remove fixed fields */

	/* Make sure we either got what we asked for or one of the allowed
	   special-case packets */
	if( packetType == SSH1_MSG_DISCONNECT )
		return( getDisconnectInfoSSH1( sessionInfoPtr,
					sessionInfoPtr->receiveBuffer + padLength + ID_SIZE ) );
	if( expectedType == SSH1_MSG_SPECIAL_USEROPT )
		{
		/* Sending an SSH1_CMSG_USER can result in an SSH1_SMSG_FAILURE if the
		   user needs some form of authentiction to log on, so we have to
		   filter this and convert it into a CRYPT_OK/OK_SPECIAL value to
		   let the caller know whether they have to send a password or not */
		if( packetType == SSH1_SMSG_SUCCESS )
			return( CRYPT_OK );
		if( packetType == SSH1_SMSG_FAILURE )
			return( OK_SPECIAL );
		}
	if( expectedType == SSH1_MSG_SPECIAL_PWOPT )
		{
		/* If we're reading a response to a password then getting a failure
		   response is valid (even if it's not what we're expecting) since
		   it's an indication that an incorrect password was used rather than
		   that there was some general type of failure */
		if( packetType == SSH1_SMSG_FAILURE )
			retExt( sessionInfoPtr, CRYPT_ERROR_WRONGKEY,
					"Server indicated incorrect password was used" );
		expectedType = SSH1_SMSG_SUCCESS;
		}
	if( expectedType == SSH1_MSG_SPECIAL_RSAOPT )
		{
		/* If we're reading a response to an RSA key ID then getting a
		   failure response is valid (even if it's not what we're expecting)
		   since it's an indication that an incorrect key was used rather
		   than that there was some general type of failure */
		if( packetType == SSH1_SMSG_FAILURE )
			retExt( sessionInfoPtr, CRYPT_ERROR_WRONGKEY,
					"Server indicated incorrect RSA key was used" );
		expectedType = SSH1_SMSG_AUTH_RSA_CHALLENGE;
		}
	if( expectedType == SSH1_MSG_SPECIAL_ANY )
		{
		/* If we're expecting any kind of data, save the type at the start
		   of the buffer.  The length increment means that we move one byte
		   of data too much down, but this isn't a big deal since the
		   returned data length excludes it, and it's not processed anyway -
		   this packet pseudo-type is only used to eat client or server
		   chattiness at the end of the handshake */
		*bufPtr++ = packetType;
		length += ID_SIZE;
		}
	else
		if( packetType != expectedType )
			retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
					"Invalid packet type 0x%02X, expected 0x%02X",
					packetType, expectedType );

	/* Move the data down in the buffer to get rid of the padding.  This
	   isn't as inefficient as it seems since it's only used for short
	   handshake messages */
	memmove( bufPtr, sessionInfoPtr->receiveBuffer + padLength + ID_SIZE,
			 length );

	return( length );
	}

/* Send an SSHv1 packet.  SSHv1 uses an awkward variable-length padding at
   the start, when we're sending short control packets we can pre-calculate
   the data start position or memmove() it into place, with longer payload
   data quantities we can no longer easily do this so we build the header
   backwards from the start of the data in the buffer.  Because of this we
   perform all operations on the data relative to a variable start position
   given by the parameter delta */

static int sendPacketSsh1( SESSION_INFO *sessionInfoPtr,
						   const int packetType, const int dataLength,
						   const int delta )
	{
	MESSAGE_DATA msgData;
	BYTE *bufStartPtr = sessionInfoPtr->sendBuffer + \
						( ( delta != CRYPT_UNUSED ) ? delta : 0 );
	BYTE *bufPtr = bufStartPtr;
	unsigned long crc32;
	const int length = ID_SIZE + dataLength + SSH1_CRC_SIZE;
	const int padLength = getPadLength( dataLength );
	int status;

	/* Add the SSH packet header:

		uint32		length
		byte[]		padding, 8 - ( length & 7 ) bytes
		byte		type
		byte[]		data
		uint32		crc32	- Calculated over padding, type, and data */
	mputLong( bufPtr, ( long ) length );
	setMessageData( &msgData, bufPtr, padLength );
	krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
					 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	bufPtr[ padLength ] = packetType;
	if( ( sessionInfoPtr->flags & ( SESSION_ISCRYPTLIB | SESSION_ISSECURE_WRITE ) ) == \
								  ( SESSION_ISCRYPTLIB | SESSION_ISSECURE_WRITE ) )
		crc32 = calculateTruncatedMAC( sessionInfoPtr->iAuthInContext,
									   bufPtr,
									   padLength + ID_SIZE + dataLength );
	else
		crc32 = calculateCRC( bufPtr, padLength + ID_SIZE + dataLength );
	bufPtr += padLength + ID_SIZE + dataLength;
	mputLong( bufPtr, crc32 );
	if( sessionInfoPtr->flags & SESSION_ISSECURE_WRITE )
		{
		/* Encrypt the payload with handling for SSH's Blowfish
		   endianness bug */
		if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_BLOWFISH )
			longReverse( ( unsigned long * ) ( bufStartPtr + LENGTH_SIZE ),
						 padLength + length );
		status = krnlSendMessage( sessionInfoPtr->iCryptOutContext,
								  IMESSAGE_CTX_ENCRYPT,
								  bufStartPtr + LENGTH_SIZE,
								  padLength + length );
		if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_BLOWFISH )
			longReverse( ( unsigned long * ) ( bufStartPtr + LENGTH_SIZE ),
						 padLength + length );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( delta != CRYPT_UNUSED )
		{
		/* If we're sending payload data the send is interruptible, it's the
		   caller's responsibility to handle this.  Since the caller expects
		   to write data from the start of the buffer, we have to move it
		   down to line it up correctly.  This is somewhat inefficient, but
		   since SSHv1 is virtually extinct it's not worth going to a lot of
		   effort to work around this (the same applies to other SSHv1
		   issues like the Blowfish endianness problem and the weird 3DES
		   mode) */
		if( delta )
			memmove( sessionInfoPtr->sendBuffer, bufStartPtr,
					 LENGTH_SIZE + padLength + length );
		return( LENGTH_SIZE + padLength + length );
		}
	status = swrite( &sessionInfoPtr->stream, bufStartPtr,
					 LENGTH_SIZE + padLength + length );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	return( CRYPT_OK );	/* swrite() returns a byte count */
	}

/****************************************************************************
*																			*
*							Client-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the server */

static int beginClientHandshake( SESSION_INFO *sessionInfoPtr,
								 SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE *bufPtr;
	const BOOLEAN hasPassword = \
			( findSessionInfo( sessionInfoPtr->attributeList,
							   CRYPT_SESSINFO_PASSWORD ) != NULL ) ? \
			TRUE : FALSE;
	const BOOLEAN hasPrivkey = \
			( findSessionInfo( sessionInfoPtr->attributeList,
							   CRYPT_SESSINFO_PRIVATEKEY ) != NULL ) ? \
			TRUE : FALSE;
	BOOLEAN rsaOK, pwOK;
	int hostKeyLength, serverKeyLength, keyDataLength, length, value, status;

	/* The higher-level code has already read the server session information, 
	   send back our own version information (SSHv1 uses only a LF as 
	   terminator).  For SSHv1 we use the lowest common denominator of our 
	   version (1.5, described in the only existing spec for SSHv1) and 
	   whatever the server can handle */
	strlcpy_s( sessionInfoPtr->sendBuffer, 128, SSH1_ID_STRING "\n" );
	if( sessionInfoPtr->receiveBuffer[ 2 ] < \
							SSH1_ID_STRING[ SSH_ID_SIZE + 2 ] )
		sessionInfoPtr->sendBuffer[ SSH_ID_SIZE + 2 ] = \
								sessionInfoPtr->receiveBuffer[ 2 ];
	status = swrite( &sessionInfoPtr->stream, sessionInfoPtr->sendBuffer,
					 strlen( sessionInfoPtr->sendBuffer ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* Create the contexts to hold the server and host keys */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	handshakeInfo->iServerCryptContext = createInfo.cryptHandle;
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	sessionInfoPtr->iKeyexCryptContext = createInfo.cryptHandle;

	/* If the peer is using cryptlib, we use HMAC-SHA instead of CRC32 */
	if( sessionInfoPtr->flags & SESSION_ISCRYPTLIB )
		{
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_HMAC_SHA );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->iAuthInContext = createInfo.cryptHandle;
		}

	/* Process the server public key packet:

		byte[8]		cookie
		uint32		keysize_bits		- Usually 768 bits
		mpint		serverkey_exponent
		mpint		serverkey_modulus
		uint32		keysize_bits		- Usually 1024 bits
		mpint		hostkey_exponent
		mpint		hostkey_modulus
		uint32		protocol_flags		- Not used
		uint32		offered_ciphers
		uint32		offered_authent */
	length = readPacketSSH1( sessionInfoPtr, SSH1_SMSG_PUBLIC_KEY );
	if( cryptStatusError( length ) )
		return( length );
	bufPtr = sessionInfoPtr->receiveBuffer;
	memcpy( handshakeInfo->cookie, bufPtr, SSH1_COOKIE_SIZE );
	bufPtr += SSH1_COOKIE_SIZE;
	length -= SSH1_COOKIE_SIZE;
	keyDataLength = status = processPublickeyData( handshakeInfo, bufPtr,
												   length, TRUE, NULL );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, bufPtr, keyDataLength );
	status = krnlSendMessage( handshakeInfo->iServerCryptContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH1 );
	if( cryptStatusError( status ) )
		return( status );
	serverKeyLength = mgetLong( bufPtr );
	bufPtr += keyDataLength - LENGTH_SIZE;
	length -= keyDataLength;
	keyDataLength = status = processPublickeyData( handshakeInfo, bufPtr,
												   length, FALSE,
												   sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, bufPtr, keyDataLength );
	status = krnlSendMessage( sessionInfoPtr->iKeyexCryptContext,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH1 );
	if( cryptStatusError( status ) )
		return( status );
	hostKeyLength = mgetLong( bufPtr );
	bufPtr += keyDataLength - LENGTH_SIZE + UINT_SIZE;	/* Skip protocol flags */
	length -= keyDataLength + UINT_SIZE;
	if( length != UINT_SIZE + UINT_SIZE )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid length %d, should be %d", length,
				UINT_SIZE + UINT_SIZE );
	value = ( int ) mgetLong( bufPtr );		/* Offered ciphers */
	sessionInfoPtr->cryptAlgo = maskToAlgoID( value );
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_NONE )
		retExt( sessionInfoPtr, CRYPT_ERROR_NOTAVAIL,
				"No crypto algorithm compatible with the remote system "
				"could be found" );
	value = ( int ) mgetLong( bufPtr );		/* Offered authentication */
	pwOK = hasPassword && ( value & ( 1 << SSH1_AUTH_PASSWORD ) );
	rsaOK = hasPrivkey && ( value & ( 1 << SSH1_AUTH_RSA ) );
	if( !pwOK )
		{
		/* If neither RSA nor password authentication is possible, we can't
		   authenticate ourselves */
		if( !rsaOK )
			{
			if( value & ( 1 << SSH1_AUTH_PASSWORD ) )
				{
				setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				retExt( sessionInfoPtr, CRYPT_ERROR_NOTINITED,
						"Server requested password authentication but no "
						"password was available" );
				}
			setErrorInfo( sessionInfoPtr, CRYPT_SESSINFO_PRIVATEKEY,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			retExt( sessionInfoPtr, CRYPT_ERROR_NOTINITED,
					"Server requested public-key authentication but no "
					"key was available" );
			}

		/* Either the client or the server won't do passwords, turn it off
		   explicitly at the client in case it's the server */
		if( hasPassword )
			{
			ATTRIBUTE_LIST *attributeListPtr = ( ATTRIBUTE_LIST * ) \
					findSessionInfo( sessionInfoPtr->attributeList,
									 CRYPT_SESSINFO_PASSWORD );

			deleteSessionInfo( &sessionInfoPtr->attributeList,
							   attributeListPtr );
			}
		}

	/* Although in theory the server key has to fit inside the host key, the
	   spec is vague enough to allow either of the keys to be larger (and at
	   least one SSH implementation has them the wrong way around), requiring
	   that the two be swapped before they can be used (which makes you
	   wonder why there's any distinction, since the two must be
	   interchangeable in order for this to work) */
	if( hostKeyLength < serverKeyLength )
		{
		int temp;

		/* Swap the two keys around */
		temp = sessionInfoPtr->iKeyexCryptContext;
		sessionInfoPtr->iKeyexCryptContext = \
								handshakeInfo->iServerCryptContext;
		handshakeInfo->iServerCryptContext = temp;
		temp = hostKeyLength;
		hostKeyLength = serverKeyLength;
		serverKeyLength = temp;
		}

	/* Make sure that the smaller of the two keys will fit inside the
	   larger */
	if( hostKeyLength < serverKeyLength + 128 )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid host vs.server key lengths %d:%d bytes",
				hostKeyLength, serverKeyLength );

	return( CRYPT_OK );
	}

/* Exchange keys with the server */

static int exchangeClientKeys( SESSION_INFO *sessionInfoPtr,
							   SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *bufPtr = sessionInfoPtr->sendBuffer;
	int length, dataLength, value, i, status;

	/* Output the start of the session key packet:

		byte		cipher_type
		byte[8]		cookie
		mpint		double_enc_sessionkey
		uint32		protocol_flags */
	switch( sessionInfoPtr->cryptAlgo )
		{
		case CRYPT_ALGO_BLOWFISH:
			value = SSH1_CIPHER_BLOWFISH;
			break;
		case CRYPT_ALGO_DES:
			value = SSH1_CIPHER_DES;
			break;
		case CRYPT_ALGO_IDEA:
			value = SSH1_CIPHER_IDEA;
			break;
		case CRYPT_ALGO_RC4:
			value = SSH1_CIPHER_RC4;
			break;
		default:
			retIntError();
		}
	*bufPtr++ = value;
	memcpy( bufPtr, handshakeInfo->cookie, SSH1_COOKIE_SIZE );
	bufPtr += SSH1_COOKIE_SIZE;

	/* Generate the session ID and secure state information and XOR the
	   secure state with the session ID */
	generateSessionID( handshakeInfo );
	setMessageData( &msgData, handshakeInfo->secretValue, SSH1_SECRET_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_RANDOM );
	if( cryptStatusError( status ) )
		return( status );
	handshakeInfo->secretValueLength = SSH1_SECRET_SIZE;
	for( i = 0; i < SSH1_SESSIONID_SIZE; i++ )
		handshakeInfo->secretValue[ i ] ^= handshakeInfo->sessionID[ i ];

	/* Export the secure state information in double-encrypted form,
	   encrypted first with the server key, then with the host key */
	setMechanismWrapInfo( &mechanismInfo, buffer, CRYPT_MAX_PKCSIZE,
						  handshakeInfo->secretValue, SSH1_SECRET_SIZE,
						  CRYPT_UNUSED, handshakeInfo->iServerCryptContext,
						  CRYPT_UNUSED );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusError( status ) )
		return( status );
	length = mechanismInfo.wrappedDataLength;
	setMechanismWrapInfo( &mechanismInfo,
						  bufPtr + SSH1_MPI_LENGTH_SIZE, CRYPT_MAX_PKCSIZE,
						  buffer, length, CRYPT_UNUSED,
						  sessionInfoPtr->iKeyexCryptContext, CRYPT_UNUSED );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_EXPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusError( status ) )
		return( status );
	length = bytesToBits( mechanismInfo.wrappedDataLength );
	mputWord( bufPtr, length );
	bufPtr += mechanismInfo.wrappedDataLength;
	clearMechanismInfo( &mechanismInfo );

	/* XOR the state with the session ID to recover the actual state */
	for( i = 0; i < SSH1_SESSIONID_SIZE; i++ )
		handshakeInfo->secretValue[ i ] ^= handshakeInfo->sessionID[ i ];

	/* Write the various flags */
	mputLong( bufPtr, 0 );		/* Protocol flags */

	/* Move the data up in the buffer to allow for the variable-length
	   padding and send it to the server */
	dataLength = bufPtr - sessionInfoPtr->sendBuffer;
	memmove( sessionInfoPtr->sendBuffer + LENGTH_SIZE + \
			 getPadLength( dataLength ) + ID_SIZE,
			 sessionInfoPtr->sendBuffer, dataLength );
	return( sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_SESSION_KEY,
							dataLength, CRYPT_UNUSED ) );
	}

/* Complete the handshake with the server */

static int completeClientHandshake( SESSION_INFO *sessionInfoPtr,
									SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_USERNAME );
	const ATTRIBUTE_LIST *passwordPtr = \
				findSessionInfo( sessionInfoPtr->attributeList,
								 CRYPT_SESSINFO_PASSWORD );
	BYTE *bufPtr;
	int padLength, modulusLength, length, status;

	/* Set up the security information required for the session */
	status = initSecurityInfoSSH1( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Read back the server ack and send the user name:
		string		username */
	status = readPacketSSH1( sessionInfoPtr, SSH1_SMSG_SUCCESS );
	if( cryptStatusError( status ) )
		return( status );
	padLength = getPadLength( LENGTH_SIZE + userNamePtr->valueLength );
	bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE + padLength + ID_SIZE;
	encodeString( bufPtr, userNamePtr->value, userNamePtr->valueLength );
	status = sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_USER,
							 LENGTH_SIZE + userNamePtr->valueLength,
							 CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );

	/* Read back the server ack and send the authentication information if
	   required.  This information is optional, if the server returns a
	   failure packet (converted to an OK_SPECIAL return status) it means
	   authentication is required, otherwise it isn't and we're already
	   logged in */
	status = readPacketSSH1( sessionInfoPtr, SSH1_MSG_SPECIAL_USEROPT );
	if( status == OK_SPECIAL )
		{
		/* If there's a password present, we're using password-based
		   authentication:
			string		password */
		if( passwordPtr != NULL )
			{
			int maxLen, packetType, i;

			/* Since SSHv1 sends the packet length in the clear and uses
			   implicit-length padding, it reveals the length of the
			   encrypted password to an observer.  To get around this, we
			   send a series of packets of length 4...maxLen to the server,
			   one of which is the password, the rest are SSH_MSG_IGNOREs.
			   It's still possible for an attacker who can perform very
			   precise timing measurements to determine which one is the
			   password based on server response time, but it's a lot less
			   problematic than with a single packet.  Unfortunately, it's
			   also possible for an attacker to determine the password packet
			   by checking which one generates the password response, unless
			   the server somehow knows how many packets are coming reads
			   them all in a loop and only then sends a response, which
			   means that this defence isn't as effective as it seems */
			status = CRYPT_OK;
			for( maxLen = 16; maxLen <= passwordPtr->valueLength;
				 maxLen <<= 1 );
			for( i = min( 4, passwordPtr->valueLength );
				 i < maxLen && cryptStatusOK( status ); i++ )
				{
				padLength = getPadLength( LENGTH_SIZE + i );
				bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE + \
													  padLength + ID_SIZE;
				if( i == passwordPtr->valueLength )
					{
					encodeString( bufPtr, passwordPtr->value,
								  passwordPtr->valueLength );
					packetType = SSH1_CMSG_AUTH_PASSWORD;
					}
				else
					{
					MESSAGE_DATA msgData;

					setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
									i );
					krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									 IMESSAGE_GETATTRIBUTE_S, &msgData,
									 CRYPT_IATTRIBUTE_RANDOM_NONCE );
					encodeString( bufPtr, sessionInfoPtr->receiveBuffer, i );
					packetType = SSH1_MSG_IGNORE;
					}
				status = sendPacketSsh1( sessionInfoPtr, packetType,
										 LENGTH_SIZE + i, CRYPT_UNUSED );
				if( cryptStatusOK( status ) && \
					packetType == SSH1_CMSG_AUTH_PASSWORD )
					status = readPacketSSH1( sessionInfoPtr,
											 SSH1_MSG_SPECIAL_PWOPT );
				}
			}
		else
			{
			MECHANISM_WRAP_INFO mechanismInfo;
			MESSAGE_DATA msgData;
			BYTE challenge[ SSH1_CHALLENGE_SIZE + 8 ];
			BYTE response[ SSH1_RESPONSE_SIZE ];
			BYTE modulusBuffer[ ( ( SSH1_MPI_LENGTH_SIZE + \
									CRYPT_MAX_PKCSIZE ) * 2 ) + 8 ];
			BYTE *modulusPtr;

			/* We're using RSA authentication, initially we send just the user's
			   public-key information:

		        mpint	identity_public_modulus

			   First we get the modulus used to identify the client's key.
			   For no adequately explored reason this is only the modulus and
			   not the usual exponent+modulus combination, which means that
			   there's no way for a cryptlib server to identify the public
			   key from it, although at a pinch we could try with e=17, 257,
			   or F4 */
			setMessageData( &msgData, modulusBuffer,
							( SSH1_MPI_LENGTH_SIZE + CRYPT_MAX_PKCSIZE ) * 2 );
			status = krnlSendMessage( sessionInfoPtr->privateKey,
									  IMESSAGE_GETATTRIBUTE_S, &msgData,
									  CRYPT_IATTRIBUTE_KEY_SSH1 );
			if( cryptStatusError( status ) )
				return( status );

			/* Skip the key size and exponent to get to the modulus.  We
			   don't have to perform safety checks here since the data is
			   coming from within cryptlib */
			modulusPtr = modulusBuffer + sizeof( LENGTH_SIZE );
			length = mgetWord( modulusPtr );			/* Exponent */
			length = bitsToBytes( length );
			modulusPtr += length;
			modulusLength = mgetWord( modulusPtr );		/* Modulus */
			modulusLength = bitsToBytes( modulusLength );
			length = SSH1_MPI_LENGTH_SIZE + modulusLength;
			modulusPtr -= SSH1_MPI_LENGTH_SIZE;

			/* Send the modulus to the server */
			padLength = getPadLength( length );
			bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE + \
												  padLength + ID_SIZE;
			memcpy( bufPtr, modulusPtr, length );
			status = sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_AUTH_RSA,
									 length, CRYPT_UNUSED );
			if( cryptStatusOK( status ) )
				status = readPacketSSH1( sessionInfoPtr,
										 SSH1_MSG_SPECIAL_RSAOPT );
			if( cryptStatusError( status ) )
				return( status );

			/* The server recognises our key (no mean feat, considering that
			   e could be anything) and has sent an RSA challenge, decrypt
			   it:

				mpint	encrypted_challenge */
			bufPtr = sessionInfoPtr->receiveBuffer;
			length = mgetWord( bufPtr );
			length = bitsToBytes( length );
			if( length < modulusLength - 8 || length > modulusLength )
				retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
						"Invalid encrypted challenge length %d for modulus "
						"length %d", length, modulusLength );
			setMechanismWrapInfo( &mechanismInfo, bufPtr, length,
								  challenge, SSH1_CHALLENGE_SIZE, CRYPT_UNUSED,
								  sessionInfoPtr->privateKey, CRYPT_UNUSED );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_DEV_IMPORT, &mechanismInfo,
									  MECHANISM_ENC_PKCS1_RAW );
			clearMechanismInfo( &mechanismInfo );
			if( cryptStatusError( status ) )
				return( status );

			/* Send the response to the challenge:

				byte[16]	MD5 of decrypted challenge

			   Since this completes the authentication, we expect to see
			   either a success or failure packet once we're done */
			generateChallengeResponse( response, handshakeInfo, challenge );
			padLength = getPadLength( SSH1_RESPONSE_SIZE );
			bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE +
												  padLength + ID_SIZE;
			memcpy( bufPtr, response, SSH1_RESPONSE_SIZE );
			status = sendPacketSsh1( sessionInfoPtr,
									 SSH1_CMSG_AUTH_RSA_RESPONSE,
									 SSH1_RESPONSE_SIZE, CRYPT_UNUSED );
			if( cryptStatusOK( status ) )
				status = readPacketSSH1( sessionInfoPtr,
										 SSH1_MSG_SPECIAL_PWOPT );
			}
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Tell the server to adjust its maximum packet size if required:

		uint32		packet_size */
	if( sessionInfoPtr->sendBufSize < EXTRA_PACKET_SIZE + MAX_PACKET_SIZE )
		{
		const int maxLength = sessionInfoPtr->sendBufSize - \
							  EXTRA_PACKET_SIZE;

		padLength = getPadLength( LENGTH_SIZE );
		bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE + padLength + ID_SIZE;
		mputLong( bufPtr, maxLength );
		status = sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_MAX_PACKET_SIZE,
								 LENGTH_SIZE, CRYPT_UNUSED );
		if( cryptStatusOK( status ) )
			status = readPacketSSH1( sessionInfoPtr, SSH1_SMSG_SUCCESS );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Request a pty from the server:

		string		TERM environment variable = "vt100"
		uint32		rows = 24
		uint32		cols = 80
		uint32		pixel_width = 0
		uint32		pixel_height = 0
		byte		tty_mode_info = 0 */
	padLength = getPadLength( ( LENGTH_SIZE + 5 ) + ( UINT_SIZE * 4 ) + 1 );
	bufPtr = sessionInfoPtr->sendBuffer + LENGTH_SIZE + padLength + ID_SIZE;
	bufPtr += encodeString( bufPtr, "vt100", 0 );	/* Generic terminal type */
	mputLong( bufPtr, 24 );
	mputLong( bufPtr, 80 );			/* 24 x 80 */
	mputLong( bufPtr, 0 );
	mputLong( bufPtr, 0 );			/* No graphics capabilities */
	*bufPtr = 0;					/* No special TTY modes */
	status = sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_REQUEST_PTY,
							 ( LENGTH_SIZE + 5 ) + ( UINT_SIZE * 4 ) + 1,
							 CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		status = readPacketSSH1( sessionInfoPtr, SSH1_SMSG_SUCCESS );
	if( cryptStatusError( status ) )
		return( status );

	/* Tell the server to create a shell for us.  This moves the server into
	   the interactive session mode, if we're talking to a standard Unix
	   server implementing a remote shell we could read the stdout data
	   response from starting the shell but this may not be the case so we
	   leave the response for the user to process explicitly */
	return( sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_EXEC_SHELL, 0,
							CRYPT_UNUSED ) );
	}

/****************************************************************************
*																			*
*							Server-side Connect Functions					*
*																			*
****************************************************************************/

/* Perform the initial part of the handshake with the client */

static int beginServerHandshake( SESSION_INFO *sessionInfoPtr,
								 SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE *bufPtr = sessionInfoPtr->sendBuffer;
	static const int keyLength = bitsToBytes( 768 );
	long value;
	int length, status;

	krnlSendMessage( sessionInfoPtr->privateKey, IMESSAGE_GETATTRIBUTE,
					 &handshakeInfo->serverKeySize, CRYPT_CTXINFO_KEYSIZE );

	/* Generate the 768-bit RSA server key.  It would be better to do this
	   before the listen on the socket, but we can't do it until we know that
	   the client is v1, which we only know after the initial message
	   exchange */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, "SSH server key", 14 );
	status = krnlSendMessage( createInfo.cryptHandle,
							  IMESSAGE_SETATTRIBUTE_S, &msgData,
							  CRYPT_CTXINFO_LABEL );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE, ( int * ) &keyLength,
								  CRYPT_CTXINFO_KEYSIZE );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_CTX_GENKEY, NULL, FALSE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		return( status );
		}
	handshakeInfo->iServerCryptContext = createInfo.cryptHandle;

	/* Send the server public key packet:

		byte[8]		cookie
		uint32		keysize_bits		- Usually 768 bits
		mpint		serverkey_exponent
		mpint		serverkey_modulus
		uint32		keysize_bits		- Usually 1024 bits
		mpint		hostkey_exponent
		mpint		hostkey_modulus
		uint32		protocol_flags		- Not used
		uint32		offered_ciphers
		uint32		offered_authent */
	setMessageData( &msgData, handshakeInfo->cookie, SSH1_COOKIE_SIZE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusError( status ) )
		return( status );
	memcpy( bufPtr, handshakeInfo->cookie, SSH1_COOKIE_SIZE );
	bufPtr += SSH1_COOKIE_SIZE;
	setMessageData( &msgData, bufPtr, LENGTH_SIZE + \
					( ( SSH1_MPI_LENGTH_SIZE + CRYPT_MAX_PKCSIZE ) * 2 ) );
	status = krnlSendMessage( handshakeInfo->iServerCryptContext,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH1 );
	if( cryptStatusError( status ) )
		return( status );
	length = processPublickeyData( handshakeInfo, bufPtr, msgData.length,
								   TRUE, NULL );
	bufPtr += length;
	setMessageData( &msgData, bufPtr, LENGTH_SIZE + \
					( ( SSH1_MPI_LENGTH_SIZE + CRYPT_MAX_PKCSIZE ) * 2 ) );
	status = krnlSendMessage( sessionInfoPtr->privateKey,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_KEY_SSH1 );
	if( cryptStatusError( status ) )
		return( status );
	length = processPublickeyData( handshakeInfo, bufPtr, msgData.length,
								   FALSE, NULL );
	bufPtr += length;
	mputLong( bufPtr, 0 );		/* No protocol flags */
	value = getAlgorithmMask();
	mputLong( bufPtr, value );	/* Cipher algorithms */
	value = 1 << SSH1_AUTH_PASSWORD;
	if( sessionInfoPtr->cryptKeyset != CRYPT_ERROR )
		value |= 1 << SSH1_AUTH_RSA;
	mputLong( bufPtr, value );	/* Authent algorithms */

	/* Move the data up in the buffer to allow for the variable-length
	   padding and send it to the client */
	length = bufPtr - sessionInfoPtr->sendBuffer;
	memmove( sessionInfoPtr->sendBuffer + LENGTH_SIZE + \
			 getPadLength( length ) + ID_SIZE, sessionInfoPtr->sendBuffer,
			 length );
	status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_PUBLIC_KEY, length,
							 CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );

	/* If the peer is using cryptlib, we use HMAC-SHA instead of CRC32 */
	if( sessionInfoPtr->flags & SESSION_ISCRYPTLIB )
		{
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_HMAC_SHA );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		sessionInfoPtr->iAuthInContext = createInfo.cryptHandle;
		}

	return( CRYPT_OK );
	}

/* Exchange keys with the client */

static int exchangeServerKeys( SESSION_INFO *sessionInfoPtr,
							   SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	MECHANISM_WRAP_INFO mechanismInfo;
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer;
	int length, keyLength, i, status;

	/* Read the client's encrypted session key information:

		byte		cipher_type
		byte[8]		cookie
		mpint		double_enc_sessionkey
		uint32		protocol_flags */
	length = readPacketSSH1( sessionInfoPtr, SSH1_CMSG_SESSION_KEY );
	if( cryptStatusError( length ) )
		return( length );
	sessionInfoPtr->cryptAlgo = sshIDToAlgoID( bufPtr[ 0 ] );
	if( sessionInfoPtr->cryptAlgo == CRYPT_ALGO_NONE )
		retExt( sessionInfoPtr, CRYPT_ERROR_NOTAVAIL,
				"No crypto algorithm compatible with the remote system "
				"could be found" );
	if( memcmp( bufPtr + 1, handshakeInfo->cookie, SSH1_COOKIE_SIZE ) )
		retExt( sessionInfoPtr, CRYPT_ERROR_INVALID,
				"Client cookie doesn't match server cookie" );
	bufPtr += 1 + SSH1_COOKIE_SIZE;
	length -= 1 + SSH1_COOKIE_SIZE;
	keyLength = mgetWord( bufPtr );
	keyLength = bitsToBytes( keyLength );
	if( length != SSH1_MPI_LENGTH_SIZE + keyLength + UINT_SIZE || \
		keyLength < handshakeInfo->serverKeySize - 8 || \
		keyLength > handshakeInfo->serverKeySize )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid encrypted session key packet length %d, key "
				"length %d", length, keyLength );

	/* Import the double-encrypted secure state information, first decrypting
	   with the host key, then with the server key */
	setMechanismWrapInfo( &mechanismInfo, bufPtr, keyLength,
						  buffer, CRYPT_MAX_PKCSIZE, CRYPT_UNUSED,
						  sessionInfoPtr->privateKey, CRYPT_UNUSED );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusError( status ) )
		return( status );
	setMechanismWrapInfo( &mechanismInfo, buffer, mechanismInfo.keyDataLength,
						  handshakeInfo->secretValue, SSH1_SECRET_SIZE, CRYPT_UNUSED,
						  handshakeInfo->iServerCryptContext, CRYPT_UNUSED );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT,
							  &mechanismInfo, MECHANISM_ENC_PKCS1_RAW );
	if( cryptStatusOK( status ) && \
		mechanismInfo.keyDataLength != SSH1_SECRET_SIZE )
		return( CRYPT_ERROR_BADDATA );
	clearMechanismInfo( &mechanismInfo );
	if( cryptStatusError( status ) )
		return( status );
	handshakeInfo->secretValueLength = SSH1_SECRET_SIZE;

	/* Generate the session ID from the handshake information and XOR it 
	   with the recovered secure state information to get the final secure 
	   state data */
	generateSessionID( handshakeInfo );
	for( i = 0; i < SSH1_SESSIONID_SIZE; i++ )
		handshakeInfo->secretValue[ i ] ^= handshakeInfo->sessionID[ i ];

	return( CRYPT_OK );
	}

/* Complete the handshake with the client */

static int completeServerHandshake( SESSION_INFO *sessionInfoPtr,
									SSH_HANDSHAKE_INFO *handshakeInfo )
	{
	BYTE *bufPtr;
	int packetType, stringLength, length, iterationCount = 0, status;

	/* Set up the security information required for the session */
	status = initSecurityInfoSSH1( sessionInfoPtr, handshakeInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Send the server ack and read back the user name:

		string		username */
	status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_SUCCESS, 0,
							 CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		length = status = readPacketSSH1( sessionInfoPtr, SSH1_CMSG_USER );
	if( cryptStatusError( status ) )
		return( status );
	bufPtr = sessionInfoPtr->receiveBuffer;
	stringLength = ( int ) mgetLong( bufPtr );
	if( length != LENGTH_SIZE + stringLength || \
		stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid user name packet length %d, name length %d",
				length, stringLength );
	updateSessionInfo( &sessionInfoPtr->attributeList,
					   CRYPT_SESSINFO_USERNAME, bufPtr,
					   stringLength, CRYPT_MAX_TEXTSIZE, ATTR_FLAG_NONE );

	/* Send the server ack (which is actually a nack since the user needs
	   to submit a password) and read back the password */
	status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_FAILURE, 0,
							 CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		length = status = readPacketSSH1( sessionInfoPtr,
										  SSH1_CMSG_AUTH_PASSWORD );
	if( cryptStatusError( status ) )
		return( status );
	bufPtr = sessionInfoPtr->receiveBuffer;
	stringLength = ( int ) mgetLong( bufPtr );
	if( length != LENGTH_SIZE + stringLength || \
		stringLength <= 0 || stringLength > CRYPT_MAX_TEXTSIZE )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid password packet length %d, password length %d",
				length, stringLength );
	updateSessionInfo( &sessionInfoPtr->attributeList,
					   CRYPT_SESSINFO_PASSWORD, bufPtr,
					   stringLength, CRYPT_MAX_TEXTSIZE, ATTR_FLAG_NONE );

	/* Send the server ack and process any further junk that the caller may
	   throw at us until we get an exec shell or command request.  At the
	   moment it's set up in allow-all mode, it may be necessary to switch
	   to deny-all instead if clients pop up which submit things that cause
	   problems */
	status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_SUCCESS, 0,
							 CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		return( status );
	do
		{
		status = readPacketSSH1( sessionInfoPtr, SSH1_MSG_SPECIAL_ANY );
		if( cryptStatusError( status ) )
			break;
		packetType = sessionInfoPtr->receiveBuffer[ 0 ];
		switch( packetType )
			{
			case SSH1_CMSG_REQUEST_COMPRESSION:
			case SSH1_CMSG_X11_REQUEST_FORWARDING:
			case SSH1_CMSG_PORT_FORWARD_REQUEST:
			case SSH1_CMSG_AGENT_REQUEST_FORWARDING:
				/* Special operations aren't supported by cryptlib */
				status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_FAILURE,
										 0, CRYPT_UNUSED );
				break;

			case SSH1_CMSG_EXEC_SHELL:
			case SSH1_CMSG_EXEC_CMD:
				/* These commands move the server into the interactive
				   session mode and aren't explicitly acknowledged */
				break;

			case SSH1_CMSG_REQUEST_PTY:
			default:
				status = sendPacketSsh1( sessionInfoPtr, SSH1_SMSG_SUCCESS,
										 0, CRYPT_UNUSED );
			}
		}
	while( !cryptStatusError( status ) && \
		   ( packetType != SSH1_CMSG_EXEC_SHELL && \
			 packetType != SSH1_CMSG_EXEC_CMD ) && iterationCount++ < 50 );
	if( iterationCount >= 50 )
		retExt( sessionInfoPtr, CRYPT_ERROR_OVERFLOW,
				"Peer sent excessive number of session open packets" );

	return( cryptStatusError( status ) ? status : CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Get/Put Data Functions						*
*																			*
****************************************************************************/

/* Read data over the SSHv1 link */

static int readHeaderFunction( SESSION_INFO *sessionInfoPtr,
							   READSTATE_INFO *readInfo )
	{
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	long length;
	int status;

	/* Clear return value */
	*readInfo = READINFO_NONE;

	/* Try and read the header data from the remote system */
	assert( sessionInfoPtr->receiveBufPos == sessionInfoPtr->receiveBufEnd );
	status = readFixedHeader( sessionInfoPtr, LENGTH_SIZE );
	if( status <= 0 )
		return( status );

	/* Process the header data.  Since data errors are always fatal, we make
	   all errors fatal until we've finished handling the header */
	*readInfo = READINFO_FATAL;
	assert( status == LENGTH_SIZE );
	length = mgetLong( bufPtr );
	if( length < SSH1_HEADER_SIZE || \
		length > sessionInfoPtr->receiveBufSize - 8 )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid packet length %d", length );

	/* Determine how much data we'll be expecting.  We set the remaining
	   data length to the actual length plus the padding length since we
	   need to read this much to get to the end of the packet */
	sessionInfoPtr->pendingPacketLength = length;
	sessionInfoPtr->pendingPacketRemaining = length + \
				( 8 - ( sessionInfoPtr->pendingPacketLength & 7 ) );

	/* Indicate that we got the header.  Since the header is out-of-band data
	   in SSHv1, we mark it as a no-op read */
	*readInfo = READINFO_NOOP;
	return( OK_SPECIAL );
	}

static int processBodyFunction( SESSION_INFO *sessionInfoPtr,
								READSTATE_INFO *readInfo )
	{
	BYTE *bufPtr = sessionInfoPtr->receiveBuffer + \
				   sessionInfoPtr->receiveBufPos;
	BYTE *bufStartPtr = bufPtr;
	long length;
	int padLength, status;

	/* All errors processing the payload are fatal */
	*readInfo = READINFO_FATAL;

	/* Decrypt the packet in the buffer and checksum the payload */
	padLength = 8 - ( sessionInfoPtr->pendingPacketLength & 7 );
	assert( bufPtr == sessionInfoPtr->receiveBuffer + \
					  sessionInfoPtr->receiveBufEnd - \
					  ( padLength + sessionInfoPtr->pendingPacketLength ) );
	status = decryptPayload( sessionInfoPtr, bufPtr,
							 padLength + sessionInfoPtr->pendingPacketLength );
	if( cryptStatusError( status ) )
		return( status );
	if( !checksumPayload( sessionInfoPtr, bufPtr,
						  padLength + sessionInfoPtr->pendingPacketLength ) )
		retExt( sessionInfoPtr, CRYPT_ERROR_SIGNATURE,
				"Bad message checksum" );

	/* See what we got */
	bufPtr += padLength;
	if( *bufPtr == SSH1_MSG_IGNORE || *bufPtr == SSH1_MSG_DEBUG || \
		*bufPtr == SSH1_CMSG_WINDOW_SIZE )
		{
		/* Nothing to see here, move along, move along */
		sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos;
		sessionInfoPtr->pendingPacketLength = 0;
		*readInfo = READINFO_NOOP;
		return( OK_SPECIAL );	/* Tell the caller to try again */
		}
	if( *bufPtr == SSH1_MSG_DISCONNECT )
		return( getDisconnectInfoSSH1( sessionInfoPtr,
					sessionInfoPtr->receiveBuffer + padLength + ID_SIZE ) );
	if( *bufPtr == SSH1_SMSG_EXITSTATUS )
		{
		/* Confirm the server exit and bail out */
		sendPacketSsh1( sessionInfoPtr, SSH1_CMSG_EXIT_CONFIRMATION, 0,
						CRYPT_UNUSED );
		sessionInfoPtr->receiveBufEnd = sessionInfoPtr->receiveBufPos;
		sessionInfoPtr->pendingPacketLength = 0;
		return( 0 );
		}
	bufPtr++;

	/* It's payload data, move it down next to the previous data.  This
	   doesn't help performance much, but it shouldn't be a major problem
	   since SSHv1 is deprecated anyway */
	length = mgetLong( bufPtr );
	if( length < 0 || length > sessionInfoPtr->pendingPacketLength - \
							   ( ID_SIZE + LENGTH_SIZE + SSH1_CRC_SIZE ) )
		retExt( sessionInfoPtr, CRYPT_ERROR_BADDATA,
				"Invalid payload length %d", length );
	if( length > 0 )
		memmove( bufStartPtr, bufPtr, length );
	sessionInfoPtr->receiveBufEnd -= padLength + ID_SIZE + LENGTH_SIZE + \
									 SSH1_CRC_SIZE;
	sessionInfoPtr->receiveBufPos = sessionInfoPtr->receiveBufEnd;
	sessionInfoPtr->pendingPacketLength = 0;

	if( length < 1 )
		{
		*readInfo = READINFO_NOOP;
		return( OK_SPECIAL );
		}
	*readInfo = READINFO_NONE;
	return( length );
	}

/* Write data over the SSHv1 link */

static int preparePacketFunction( SESSION_INFO *sessionInfoPtr )
	{
	BYTE *bufPtr = sessionInfoPtr->sendBuffer + \
				   SSH1_MAX_HEADER_SIZE - LENGTH_SIZE;
	const int dataLength = sessionInfoPtr->sendBufPos - SSH1_MAX_HEADER_SIZE;
	const int delta = SSH1_MAX_HEADER_SIZE - \
					( LENGTH_SIZE + getPadLength( LENGTH_SIZE + dataLength ) + \
					  ID_SIZE + LENGTH_SIZE );

	/* Wrap up the payload ready for sending.  This doesn't actually send
	   the data since we're specifying the delta parameter, which tells the
	   send-packet code that it's an interruptible write that'll be handled
	   by the caller:

		string		data */
	mputLong( bufPtr, dataLength );
	return( sendPacketSsh1( sessionInfoPtr, isServer( sessionInfoPtr ) ? \
								SSH1_SMSG_STDOUT_DATA : SSH1_CMSG_STDIN_DATA,
							LENGTH_SIZE + dataLength, delta ) );
	}

/* Close a previously-opened SSH session */

static void shutdownFunction( SESSION_INFO *sessionInfoPtr )
	{
	sNetDisconnect( &sessionInfoPtr->stream );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

void initSSH1processing( SESSION_INFO *sessionInfoPtr,
						 SSH_HANDSHAKE_INFO *handshakeInfo,
						 const BOOLEAN isServer )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		FALSE,						/* Request-response protocol */
		SESSION_NONE,				/* Flags */
		SSH_PORT,					/* SSH port */
		SESSION_NEEDS_USERID |		/* Client attributes */
			SESSION_NEEDS_PASSWORD | \
			SESSION_NEEDS_KEYORPASSWORD | \
			SESSION_NEEDS_PRIVKEYCRYPT,
				/* The client private key is optional but if present, it has
				   to be encryption-capable */
		SESSION_NEEDS_PRIVATEKEY |	/* Server attributes */
			SESSION_NEEDS_PRIVKEYCRYPT,
		1, 1, 2,					/* Version 1 */
		NULL, NULL,					/* Content-type */

		/* Protocol-specific information */
		EXTRA_PACKET_SIZE + \
			DEFAULT_PACKET_SIZE,	/* Send/receive buffer size */
		SSH1_MAX_HEADER_SIZE,		/* Payload data start */
		DEFAULT_PACKET_SIZE,		/* (Default) maximum packet size */
		NULL,						/* Alt.transport protocol */
		128							/* Required priv.key size */
			/* The SSHv1 host key has to be at least 1024 bits long so that
			   it can be used to wrap the 768-bit server key */
		};

	sessionInfoPtr->protocolInfo = &protocolInfo;
	sessionInfoPtr->readHeaderFunction = readHeaderFunction;
	sessionInfoPtr->processBodyFunction = processBodyFunction;
	sessionInfoPtr->preparePacketFunction = preparePacketFunction;
	if( handshakeInfo != NULL )
		{
		if( isServer )
			{
			handshakeInfo->beginHandshake = beginServerHandshake;
			handshakeInfo->exchangeKeys = exchangeServerKeys;
			handshakeInfo->completeHandshake = completeServerHandshake;
			}
		else
			{
			handshakeInfo->beginHandshake = beginClientHandshake;
			handshakeInfo->exchangeKeys = exchangeClientKeys;
			handshakeInfo->completeHandshake = completeClientHandshake;
			}
		}

	/* SSHv1 has slightly different data handling than SSHv2, if we're
	   targeted at SSHv2 we need to override the SSHv2 shutdown function
	   with a default one */
	sessionInfoPtr->shutdownFunction = shutdownFunction;
	}
#endif /* USE_SSH1 */
