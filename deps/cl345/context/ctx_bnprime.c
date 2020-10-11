/****************************************************************************
*																			*
*					cryptlib SSHv2/SSL/TLS DH Public Parameters				*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

/* Define the following to convert from the fixed-format DH and ECC domain
   parameters to pre-encoded the bignum format.  This will also require
   changing loadDHparams() to load each of 1536, 2048 and 3072 bit keys,
   and changing loadECCparams() to load each of CRYPT_ECCCURVE_P256, 
   CRYPT_ECCCURVE_BRAINPOOL_P256, CRYPT_ECCCURVE_P384, 
   CRYPT_ECCCURVE_BRAINPOOL_P384, CRYPT_ECCCURVE_P521 and 
   CRYPT_ECCCURVE_BRAINPOOL_P512 */

/* #define CREATE_BIGNUM_VALUES */

#if defined( USE_SSH ) || defined( USE_SSL )

/****************************************************************************
*																			*
*							DH Domain Parameters							*
*																			*
****************************************************************************/

/* Predefined DH values from RFC 2409 and RFC 3526.  These have been 
   independently verified by Henrick Hellström <henrick@streamsec.se> to 
   check that:

	a: The numbers match the numbers in the RFCs.
	b: The numbers are safe primes (using both Miller-Rabin tests and Lucas 
	   tests on q = (p-1)/2, and then the Pocklington Criterion on p).
	c: The small constant k is indeed the least positive integer such that 
	   the number is a safe prime.

   Safely converting these to pre-encoded bignum values is a multi-stage
   process, first we encode the original values as SSL-format values, which
   is the closest format to the original spec and one for which we can
   use SHA-1 hashes to verify the primes.
   
   Then we read them into a bignum and print the result.  This finally 
   allows us to use the values as pre-generated bignums.  To enable the
   code for this process, define the following value */

#ifdef CREATE_BIGNUM_VALUES

#ifdef USE_DH1024

/* 1024-bit DH parameters from RFC 2409 */

static const BYTE dh1024SSL[] = {
	0x00, 0x80,		/* p */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
		0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
		0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
		0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
		0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
		0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
		0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
		0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
		0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
		0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
		0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
		0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
		0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
		0x49, 0x28, 0x66, 0x51, 0xEC, 0xE6, 0x53, 0x81,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x01,		/* g */
		0x02
	};
#endif /* USE_DH1024 */

/* 1536-, 2048- and 3072-bit parameters from RFC 3526.  There is also a 
   4096-bit value given in the RFC, but using that, and the even larger
   values of 6144 and 8192 bits, is just silly.  See also the comment on 
   defences against clients that request stupid key sizes in processDHE() 
   in ssh2_svr.c */

static const BYTE dh1536SSL[] = {
	0x00, 0xC0,		/* p */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
		0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
		0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
		0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
		0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
		0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
		0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
		0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
		0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
		0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
		0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
		0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
		0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
		0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
		0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
		0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
		0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
		0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
		0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
		0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
		0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
		0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x23, 0x73, 0x27,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x01,		/* g */
		0x02
	};

static const BYTE dh2048SSL[] = {
	0x01, 0x00,		/* p */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
		0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
		0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
		0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
		0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
		0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
		0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
		0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
		0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
		0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
		0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
		0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
		0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
		0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
		0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
		0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
		0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
		0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
		0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
		0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
		0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
		0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
		0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
		0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
		0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
		0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
		0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
		0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
		0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
		0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x01,		/* g */
		0x02
	};

static const BYTE dh3072SSL[] = {
	0x01, 0x80,		/* p */
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,
		0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,
		0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,
		0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,
		0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,
		0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,
		0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,
		0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,
		0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,
		0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,
		0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,
		0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,
		0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,
		0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,
		0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,
		0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,
		0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,
		0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,
		0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,
		0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,
		0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,
		0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,
		0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,
		0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,
		0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,
		0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,
		0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,
		0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,
		0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,
		0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D,
		0xAD, 0x33, 0x17, 0x0D, 0x04, 0x50, 0x7A, 0x33,
		0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64,
		0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A,
		0x8A, 0xEA, 0x71, 0x57, 0x5D, 0x06, 0x0C, 0x7D,
		0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,
		0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7,
		0x1E, 0x8C, 0x94, 0xE0, 0x4A, 0x25, 0x61, 0x9D,
		0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B,
		0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64,
		0xD8, 0x76, 0x02, 0x73, 0x3E, 0xC8, 0x6A, 0x64,
		0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,
		0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C,
		0x77, 0x09, 0x88, 0xC0, 0xBA, 0xD9, 0x46, 0xE2,
		0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31,
		0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E,
		0x4B, 0x82, 0xD1, 0x20, 0xA9, 0x3A, 0xD2, 0xCA,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x01,		/* g */
		0x02
	};

/* Checksum values for the DH data */

#ifdef USE_DH1024
static int dh1024checksum;
#endif /* USE_DH1024 */

static int dh1536checksum, dh2048checksum, dh3072checksum;

/* Check that the stored DH key data is valid */

CHECK_RETVAL \
static int checkDHdata( void )
	{
	HASH_FUNCTION_ATOMIC hashFunctionAtomic;
	BYTE hashValue[ 20 + 8 ];

	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );

	/* Generate/check the SHA-1 values for the primes.  This doesn't cover 
	   all of the data, but is needed to bootstrap the full check because 
	   only the hashes of the primes have been published */
#if 0
	hashFunctionAtomic( hashValue, 20, dh1024SSL + 2, sizeof( dh1024SSL ) - 5 );
	hashFunctionAtomic( hashValue, 20, dh1536SSL + 2, sizeof( dh1536SSL ) - 5 );
	hashFunctionAtomic( hashValue, 20, dh2048SSL + 2, sizeof( dh2048SSL ) - 5 );
	hashFunctionAtomic( hashValue, 20, dh3072SSL + 2, sizeof( dh3072SSL ) - 5 );
#endif /* 0 */

	/* Check the SHA-1 values for the DH data */
#ifndef CONFIG_FUZZ
  #ifdef USE_DH1024
	hashFunctionAtomic( hashValue, 20, dh1024SSL, sizeof( dh1024SSL ) );
	if( memcmp( hashValue, \
				"\x46\xF4\x47\xC3\x69\x00\x6F\x22\x91\x0E\x24\xF5\x73\x68\xE6\xF7", 16 ) )
		retIntError();
  #endif /* USE_DH1024 */
	hashFunctionAtomic( hashValue, 20, dh1536SSL, sizeof( dh1536SSL ) );
	if( memcmp( hashValue, \
				"\xA3\xEC\x2F\x85\xC7\xD3\x74\xD2\x56\x53\x22\xED\x53\x87\x6F\xDC", 16 ) )
		retIntError();
	hashFunctionAtomic( hashValue, 20, dh2048SSL, sizeof( dh2048SSL ) );
	if( memcmp( hashValue, \
				"\x0E\x8E\x49\x9F\xD9\x18\x02\xCB\x5D\x42\x03\xA5\xE1\x67\x51\xDB", 16 ) )
		retIntError();
	hashFunctionAtomic( hashValue, 20, dh3072SSL, sizeof( dh3072SSL ) );
	if( memcmp( hashValue, \
				"\x8F\x4A\x19\xEF\x85\x1D\x91\xEA\x18\xE8\xB8\xB9\x9F\xF0\xFD\xF8", 16 ) )
		retIntError();
#endif /* !CONFIG_FUZZ */

	/* Now that we've verified that the DH data is valid, calculate a 
	   checksum for each value to allow it to be quickly checked before it's
	   loaded into a context */
#ifdef USE_DH1024
	dh1024checksum = checksumData( dh1024SSL, sizeof( dh1024SSL ) );
#endif /* USE_DH1024 */
	dh1536checksum = checksumData( dh1536SSL, sizeof( dh1536SSL ) );
	dh2048checksum = checksumData( dh2048SSL, sizeof( dh2048SSL ) );
	dh3072checksum = checksumData( dh3072SSL, sizeof( dh3072SSL ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
static int loadDHparamsFixed( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
							  IN_LENGTH_PKC const int keySize )
	{
	MESSAGE_DATA msgData;
	const void *keyData;
	int keyDataLength, keyDataChecksum;

	REQUIRES( isHandleRangeValid( iCryptContext ) );
	REQUIRES( keySize >= MIN_PKCSIZE && keySize <= CRYPT_MAX_PKCSIZE );

	switch( keySize )
		{
#ifdef USE_DH1024
		case bitsToBytes( 1024 ):
			keyData = dh1024SSL;
			keyDataLength = sizeof( dh1024SSL );
			keyDataChecksum = dh1024checksum;
			break;
#endif /* USE_DH1024 */

		case bitsToBytes( 1536 ):
			keyData = dh1536SSL;
			keyDataLength = sizeof( dh1536SSL );
			keyDataChecksum = dh1536checksum;
			break;

		case bitsToBytes( 2048 ):
			keyData = dh2048SSL;
			keyDataLength = sizeof( dh2048SSL );
			keyDataChecksum = dh2048checksum;
			break;

		case bitsToBytes( 3072 ):
		default:			/* Hier ist der mast zu ende */
			keyData = dh3072SSL;
			keyDataLength = sizeof( dh3072SSL );
			keyDataChecksum = dh3072checksum;
			break;
		}

	/* Make sure that the key data hasn't been corrupted */
	if( keyDataChecksum != checksumData( keyData, keyDataLength ) )
		{
		DEBUG_DIAG(( "Fixed DH value for %d-bit key has been corrupted",
					 bytesToBits( keySize ) ));
		retIntError();
		}

	/* Load the fixed DH key into the context */
	setMessageData( &msgData, ( MESSAGE_CAST ) keyData, keyDataLength );
	return( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							 &msgData, CRYPT_IATTRIBUTE_KEY_SSL ) );
	}
#endif /* CREATE_BIGNUM_VALUES */

/* DH values as pre-encoded bignums, allowing them to be used directly.  
   Note that this includes the somewhat unsound 1024-bit values, we never 
   offer these ourselves but still provide built-in values to deal with 
   other implementations that use them */

#if BN_BITS2 == 64

static const DH_DOMAINPARAMS dh1024params = {	/* See note above */
	/* p */
	{ 16, FALSE, BN_FLG_STATIC_DATA,{
	0xFFFFFFFFFFFFFFFFULL, 0x49286651ECE65381ULL,
	0xAE9F24117C4B1FE6ULL, 0xEE386BFB5A899FA5ULL,
	0x0BFF5CB6F406B7EDULL, 0xF44C42E9A637ED6BULL,
	0xE485B576625E7EC6ULL, 0x4FE1356D6D51C245ULL,
	0x302B0A6DF25F1437ULL, 0xEF9519B3CD3A431BULL,
	0x514A08798E3404DDULL, 0x020BBEA63B139B22ULL,
	0x29024E088A67CC74ULL, 0xC4C6628B80DC1CD1ULL,
	0xC90FDAA22168C234ULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* q */
	{ 16, FALSE, BN_FLG_STATIC_DATA,{
	0xFFFFFFFFFFFFFFFFULL, 0x24943328F67329C0ULL,
	0xD74F9208BE258FF3ULL, 0xF71C35FDAD44CFD2ULL,
	0x85FFAE5B7A035BF6ULL, 0x7A262174D31BF6B5ULL,
	0xF242DABB312F3F63ULL, 0xA7F09AB6B6A8E122ULL,
	0x98158536F92F8A1BULL, 0xF7CA8CD9E69D218DULL,
	0x28A5043CC71A026EULL, 0x0105DF531D89CD91ULL,
	0x948127044533E63AULL, 0x62633145C06E0E68ULL,
	0xE487ED5110B4611AULL, 0x7FFFFFFFFFFFFFFFULL } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA,{
	0x0000000000000002ULL } },
	/* Checksums */
	0x3E2F059DBCCFD195ULL, 0x5503C71D5F777AE8ULL, 0x28C47BE7F867ED54ULL
	};

static const DH_DOMAINPARAMS dh1536params = {
	/* p */
	{ 24, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0xF1746C08CA237327ULL,
	0x670C354E4ABC9804ULL, 0x9ED529077096966DULL,
	0x1C62F356208552BBULL, 0x83655D23DCA3AD96ULL,
	0x69163FA8FD24CF5FULL, 0x98DA48361C55D39AULL,
	0xC2007CB8A163BF05ULL, 0x49286651ECE45B3DULL,
	0xAE9F24117C4B1FE6ULL, 0xEE386BFB5A899FA5ULL,
	0x0BFF5CB6F406B7EDULL, 0xF44C42E9A637ED6BULL,
	0xE485B576625E7EC6ULL, 0x4FE1356D6D51C245ULL,
	0x302B0A6DF25F1437ULL, 0xEF9519B3CD3A431BULL,
	0x514A08798E3404DDULL, 0x020BBEA63B139B22ULL,
	0x29024E088A67CC74ULL, 0xC4C6628B80DC1CD1ULL,
	0xC90FDAA22168C234ULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* q */
	{ 24, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0x78BA36046511B993ULL,
	0xB3861AA7255E4C02ULL, 0xCF6A9483B84B4B36ULL,
	0x0E3179AB1042A95DULL, 0xC1B2AE91EE51D6CBULL,
	0x348B1FD47E9267AFULL, 0xCC6D241B0E2AE9CDULL,
	0xE1003E5C50B1DF82ULL, 0x24943328F6722D9EULL,
	0xD74F9208BE258FF3ULL, 0xF71C35FDAD44CFD2ULL,
	0x85FFAE5B7A035BF6ULL, 0x7A262174D31BF6B5ULL,
	0xF242DABB312F3F63ULL, 0xA7F09AB6B6A8E122ULL,
	0x98158536F92F8A1BULL, 0xF7CA8CD9E69D218DULL,
	0x28A5043CC71A026EULL, 0x0105DF531D89CD91ULL,
	0x948127044533E63AULL, 0x62633145C06E0E68ULL,
	0xE487ED5110B4611AULL, 0x7FFFFFFFFFFFFFFFULL } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000002ULL } },
	/* Checksums */
	0x9472AA5034F03DC6ULL, 0x7E4E90EF890616AFULL, 0x28C47BE7F867ED54ULL
	};

static const DH_DOMAINPARAMS dh2048params = {
	/* p */
	{ 32, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0x15728E5A8AACAA68ULL,
	0x15D2261898FA0510ULL, 0x3995497CEA956AE5ULL,
	0xDE2BCBF695581718ULL, 0xB5C55DF06F4C52C9ULL,
	0x9B2783A2EC07A28FULL, 0xE39E772C180E8603ULL,
	0x32905E462E36CE3BULL, 0xF1746C08CA18217CULL,
	0x670C354E4ABC9804ULL, 0x9ED529077096966DULL,
	0x1C62F356208552BBULL, 0x83655D23DCA3AD96ULL,
	0x69163FA8FD24CF5FULL, 0x98DA48361C55D39AULL,
	0xC2007CB8A163BF05ULL, 0x49286651ECE45B3DULL,
	0xAE9F24117C4B1FE6ULL, 0xEE386BFB5A899FA5ULL,
	0x0BFF5CB6F406B7EDULL, 0xF44C42E9A637ED6BULL,
	0xE485B576625E7EC6ULL, 0x4FE1356D6D51C245ULL,
	0x302B0A6DF25F1437ULL, 0xEF9519B3CD3A431BULL,
	0x514A08798E3404DDULL, 0x020BBEA63B139B22ULL,
	0x29024E088A67CC74ULL, 0xC4C6628B80DC1CD1ULL,
	0xC90FDAA22168C234ULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* q */
	{ 32, FALSE, BN_FLG_STATIC_DATA, {
	0x7FFFFFFFFFFFFFFFULL, 0x0AB9472D45565534ULL,
	0x8AE9130C4C7D0288ULL, 0x1CCAA4BE754AB572ULL,
	0xEF15E5FB4AAC0B8CULL, 0xDAE2AEF837A62964ULL,
	0xCD93C1D17603D147ULL, 0xF1CF3B960C074301ULL,
	0x19482F23171B671DULL, 0x78BA3604650C10BEULL,
	0xB3861AA7255E4C02ULL, 0xCF6A9483B84B4B36ULL,
	0x0E3179AB1042A95DULL, 0xC1B2AE91EE51D6CBULL,
	0x348B1FD47E9267AFULL, 0xCC6D241B0E2AE9CDULL,
	0xE1003E5C50B1DF82ULL, 0x24943328F6722D9EULL,
	0xD74F9208BE258FF3ULL, 0xF71C35FDAD44CFD2ULL,
	0x85FFAE5B7A035BF6ULL, 0x7A262174D31BF6B5ULL,
	0xF242DABB312F3F63ULL, 0xA7F09AB6B6A8E122ULL,
	0x98158536F92F8A1BULL, 0xF7CA8CD9E69D218DULL,
	0x28A5043CC71A026EULL, 0x0105DF531D89CD91ULL,
	0x948127044533E63AULL, 0x62633145C06E0E68ULL,
	0xE487ED5110B4611AULL, 0x7FFFFFFFFFFFFFFFULL } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000002ULL } },
	/* Checksums */
	0x405BD3182D6B262CULL, 0x22EEE58D97A843AFULL, 0x28C47BE7F867ED54ULL
	};

#if CRYPT_MAX_PKCSIZE >= bitsToBytes( 3072 )

static const DH_DOMAINPARAMS dh3072params = {
	/* p */
	{ 48, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0x4B82D120A93AD2CAULL,
	0x43DB5BFCE0FD108EULL, 0x08E24FA074E5AB31ULL,
	0x770988C0BAD946E2ULL, 0xBBE117577A615D6CULL,
	0x521F2B18177B200CULL, 0xD87602733EC86A64ULL,
	0xF12FFA06D98A0864ULL, 0xCEE3D2261AD2EE6BULL,
	0x1E8C94E04A25619DULL, 0xABF5AE8CDB0933D7ULL,
	0xB3970F85A6E1E4C7ULL, 0x8AEA71575D060C7DULL,
	0xECFB850458DBEF0AULL, 0xA85521ABDF1CBA64ULL,
	0xAD33170D04507A33ULL, 0x15728E5A8AAAC42DULL,
	0x15D2261898FA0510ULL, 0x3995497CEA956AE5ULL,
	0xDE2BCBF695581718ULL, 0xB5C55DF06F4C52C9ULL,
	0x9B2783A2EC07A28FULL, 0xE39E772C180E8603ULL,
	0x32905E462E36CE3BULL, 0xF1746C08CA18217CULL,
	0x670C354E4ABC9804ULL, 0x9ED529077096966DULL,
	0x1C62F356208552BBULL, 0x83655D23DCA3AD96ULL,
	0x69163FA8FD24CF5FULL, 0x98DA48361C55D39AULL,
	0xC2007CB8A163BF05ULL, 0x49286651ECE45B3DULL,
	0xAE9F24117C4B1FE6ULL, 0xEE386BFB5A899FA5ULL,
	0x0BFF5CB6F406B7EDULL, 0xF44C42E9A637ED6BULL,
	0xE485B576625E7EC6ULL, 0x4FE1356D6D51C245ULL,
	0x302B0A6DF25F1437ULL, 0xEF9519B3CD3A431BULL,
	0x514A08798E3404DDULL, 0x020BBEA63B139B22ULL,
	0x29024E088A67CC74ULL, 0xC4C6628B80DC1CD1ULL,
	0xC90FDAA22168C234ULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* q */
	{ 48, FALSE, BN_FLG_STATIC_DATA, {
	0x7FFFFFFFFFFFFFFFULL, 0x25C16890549D6965ULL,
	0xA1EDADFE707E8847ULL, 0x047127D03A72D598ULL,
	0x3B84C4605D6CA371ULL, 0x5DF08BABBD30AEB6ULL,
	0x290F958C0BBD9006ULL, 0x6C3B01399F643532ULL,
	0xF897FD036CC50432ULL, 0xE771E9130D697735ULL,
	0x8F464A702512B0CEULL, 0xD5FAD7466D8499EBULL,
	0xD9CB87C2D370F263ULL, 0x457538ABAE83063EULL,
	0x767DC2822C6DF785ULL, 0xD42A90D5EF8E5D32ULL,
	0xD6998B8682283D19ULL, 0x0AB9472D45556216ULL,
	0x8AE9130C4C7D0288ULL, 0x1CCAA4BE754AB572ULL,
	0xEF15E5FB4AAC0B8CULL, 0xDAE2AEF837A62964ULL,
	0xCD93C1D17603D147ULL, 0xF1CF3B960C074301ULL,
	0x19482F23171B671DULL, 0x78BA3604650C10BEULL,
	0xB3861AA7255E4C02ULL, 0xCF6A9483B84B4B36ULL,
	0x0E3179AB1042A95DULL, 0xC1B2AE91EE51D6CBULL,
	0x348B1FD47E9267AFULL, 0xCC6D241B0E2AE9CDULL,
	0xE1003E5C50B1DF82ULL, 0x24943328F6722D9EULL,
	0xD74F9208BE258FF3ULL, 0xF71C35FDAD44CFD2ULL,
	0x85FFAE5B7A035BF6ULL, 0x7A262174D31BF6B5ULL,
	0xF242DABB312F3F63ULL, 0xA7F09AB6B6A8E122ULL,
	0x98158536F92F8A1BULL, 0xF7CA8CD9E69D218DULL,
	0x28A5043CC71A026EULL, 0x0105DF531D89CD91ULL,
	0x948127044533E63AULL, 0x62633145C06E0E68ULL,
	0xE487ED5110B4611AULL, 0x7FFFFFFFFFFFFFFFULL } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000002ULL } },
	/* Checksums */
	0x576642F5C4CD8D96ULL, 0x22F5A1C20DEE267BULL, 0x28C47BE7F867ED54ULL
	};
#endif /* 3072-bit bignums */

#else

static const DH_DOMAINPARAMS dh1024params = {	/* See note above */
	/* p */
	{ 32, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xECE65381, 0x49286651, 
	0x7C4B1FE6, 0xAE9F2411, 0x5A899FA5, 0xEE386BFB, 
	0xF406B7ED, 0x0BFF5CB6, 0xA637ED6B, 0xF44C42E9, 
	0x625E7EC6, 0xE485B576, 0x6D51C245, 0x4FE1356D, 
	0xF25F1437, 0x302B0A6D, 0xCD3A431B, 0xEF9519B3, 
	0x8E3404DD, 0x514A0879, 0x3B139B22, 0x020BBEA6, 
	0x8A67CC74, 0x29024E08, 0x80DC1CD1, 0xC4C6628B, 
	0x2168C234, 0xC90FDAA2, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* q */
	{ 32, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xF67329C0, 0x24943328, 
	0xBE258FF3, 0xD74F9208, 0xAD44CFD2, 0xF71C35FD, 
	0x7A035BF6, 0x85FFAE5B, 0xD31BF6B5, 0x7A262174, 
	0x312F3F63, 0xF242DABB, 0xB6A8E122, 0xA7F09AB6, 
	0xF92F8A1B, 0x98158536, 0xE69D218D, 0xF7CA8CD9, 
	0xC71A026E, 0x28A5043C, 0x1D89CD91, 0x0105DF53, 
	0x4533E63A, 0x94812704, 0xC06E0E68, 0x62633145, 
	0x10B4611A, 0xE487ED51, 0xFFFFFFFF, 0x7FFFFFFF } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000002 } },
	/* Checksums */
	0x8CE0F339, 0xA165CE72, 0x847050E5
	};

static const DH_DOMAINPARAMS dh1536params = { 
	/* p */
	{ 48, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xCA237327, 0xF1746C08,
	0x4ABC9804, 0x670C354E, 0x7096966D, 0x9ED52907, 
	0x208552BB, 0x1C62F356, 0xDCA3AD96, 0x83655D23, 
	0xFD24CF5F, 0x69163FA8, 0x1C55D39A, 0x98DA4836,
	0xA163BF05, 0xC2007CB8, 0xECE45B3D, 0x49286651, 
	0x7C4B1FE6, 0xAE9F2411, 0x5A899FA5, 0xEE386BFB, 
	0xF406B7ED, 0x0BFF5CB6, 0xA637ED6B, 0xF44C42E9,
	0x625E7EC6, 0xE485B576, 0x6D51C245, 0x4FE1356D, 
	0xF25F1437, 0x302B0A6D, 0xCD3A431B, 0xEF9519B3, 
	0x8E3404DD, 0x514A0879, 0x3B139B22, 0x020BBEA6,
	0x8A67CC74, 0x29024E08, 0x80DC1CD1, 0xC4C6628B, 
	0x2168C234, 0xC90FDAA2, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* q */
	{ 48, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0x6511B993, 0x78BA3604, 
	0x255E4C02, 0xB3861AA7, 0xB84B4B36, 0xCF6A9483, 
	0x1042A95D, 0x0E3179AB, 0xEE51D6CB, 0xC1B2AE91, 
	0x7E9267AF, 0x348B1FD4, 0x0E2AE9CD, 0xCC6D241B, 
	0x50B1DF82, 0xE1003E5C, 0xF6722D9E, 0x24943328, 
	0xBE258FF3, 0xD74F9208, 0xAD44CFD2, 0xF71C35FD, 
	0x7A035BF6, 0x85FFAE5B, 0xD31BF6B5, 0x7A262174, 
	0x312F3F63, 0xF242DABB, 0xB6A8E122, 0xA7F09AB6, 
	0xF92F8A1B, 0x98158536, 0xE69D218D, 0xF7CA8CD9, 
	0xC71A026E, 0x28A5043C, 0x1D89CD91, 0x0105DF53, 
	0x4533E63A, 0x94812704, 0xC06E0E68, 0x62633145, 
	0x10B4611A, 0xE487ED51, 0xFFFFFFFF, 0x7FFFFFFF } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000002 } },
	/* Checksums */
	0x7A168323, 0x6A1B6597, 0x847050E5
	};

static const DH_DOMAINPARAMS dh2048params = { 
	/* p */
	{ 64, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0x8AACAA68, 0x15728E5A,
	0x98FA0510, 0x15D22618, 0xEA956AE5, 0x3995497C,
	0x95581718, 0xDE2BCBF6, 0x6F4C52C9, 0xB5C55DF0,
	0xEC07A28F, 0x9B2783A2, 0x180E8603, 0xE39E772C,
	0x2E36CE3B, 0x32905E46, 0xCA18217C, 0xF1746C08,
	0x4ABC9804, 0x670C354E, 0x7096966D, 0x9ED52907,
	0x208552BB, 0x1C62F356, 0xDCA3AD96, 0x83655D23,
	0xFD24CF5F, 0x69163FA8, 0x1C55D39A, 0x98DA4836,
	0xA163BF05, 0xC2007CB8, 0xECE45B3D, 0x49286651,
	0x7C4B1FE6, 0xAE9F2411, 0x5A899FA5, 0xEE386BFB,
	0xF406B7ED, 0x0BFF5CB6, 0xA637ED6B, 0xF44C42E9,
	0x625E7EC6, 0xE485B576, 0x6D51C245, 0x4FE1356D,
	0xF25F1437, 0x302B0A6D, 0xCD3A431B, 0xEF9519B3,
	0x8E3404DD, 0x514A0879, 0x3B139B22, 0x020BBEA6,
	0x8A67CC74, 0x29024E08, 0x80DC1CD1, 0xC4C6628B,
	0x2168C234, 0xC90FDAA2, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* q */
	{ 64, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0x7FFFFFFF, 0x45565534, 0x0AB9472D, 
	0x4C7D0288, 0x8AE9130C, 0x754AB572, 0x1CCAA4BE, 
	0x4AAC0B8C, 0xEF15E5FB, 0x37A62964, 0xDAE2AEF8, 
	0x7603D147, 0xCD93C1D1, 0x0C074301, 0xF1CF3B96, 
	0x171B671D, 0x19482F23, 0x650C10BE, 0x78BA3604, 
	0x255E4C02, 0xB3861AA7, 0xB84B4B36, 0xCF6A9483, 
	0x1042A95D, 0x0E3179AB, 0xEE51D6CB, 0xC1B2AE91, 
	0x7E9267AF, 0x348B1FD4, 0x0E2AE9CD, 0xCC6D241B, 
	0x50B1DF82, 0xE1003E5C, 0xF6722D9E, 0x24943328, 
	0xBE258FF3, 0xD74F9208, 0xAD44CFD2, 0xF71C35FD, 
	0x7A035BF6, 0x85FFAE5B, 0xD31BF6B5, 0x7A262174, 
	0x312F3F63, 0xF242DABB, 0xB6A8E122, 0xA7F09AB6, 
	0xF92F8A1B, 0x98158536, 0xE69D218D, 0xF7CA8CD9, 
	0xC71A026E, 0x28A5043C, 0x1D89CD91, 0x0105DF53, 
	0x4533E63A, 0x94812704, 0xC06E0E68, 0x62633145, 
	0x10B4611A, 0xE487ED51, 0xFFFFFFFF, 0x7FFFFFFF } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000002 } },
	/* Checksums */
	0x82C18653, 0xF0162B14, 0x847050E5 
	};

#if CRYPT_MAX_PKCSIZE >= bitsToBytes( 3072 )

static const DH_DOMAINPARAMS dh3072params = { 
	/* p */
	{ 96, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xA93AD2CA, 0x4B82D120,
	0xE0FD108E, 0x43DB5BFC, 0x74E5AB31, 0x08E24FA0,
	0xBAD946E2, 0x770988C0, 0x7A615D6C, 0xBBE11757,
	0x177B200C, 0x521F2B18, 0x3EC86A64, 0xD8760273,
	0xD98A0864, 0xF12FFA06, 0x1AD2EE6B, 0xCEE3D226,
	0x4A25619D, 0x1E8C94E0, 0xDB0933D7, 0xABF5AE8C,
	0xA6E1E4C7, 0xB3970F85, 0x5D060C7D, 0x8AEA7157,
	0x58DBEF0A, 0xECFB8504, 0xDF1CBA64, 0xA85521AB,
	0x04507A33, 0xAD33170D, 0x8AAAC42D, 0x15728E5A,
	0x98FA0510, 0x15D22618, 0xEA956AE5, 0x3995497C,
	0x95581718, 0xDE2BCBF6, 0x6F4C52C9, 0xB5C55DF0,
	0xEC07A28F, 0x9B2783A2, 0x180E8603, 0xE39E772C,
	0x2E36CE3B, 0x32905E46, 0xCA18217C, 0xF1746C08,
	0x4ABC9804, 0x670C354E, 0x7096966D, 0x9ED52907,
	0x208552BB, 0x1C62F356, 0xDCA3AD96, 0x83655D23,
	0xFD24CF5F, 0x69163FA8, 0x1C55D39A, 0x98DA4836,
	0xA163BF05, 0xC2007CB8, 0xECE45B3D, 0x49286651,
	0x7C4B1FE6, 0xAE9F2411, 0x5A899FA5, 0xEE386BFB,
	0xF406B7ED, 0x0BFF5CB6, 0xA637ED6B, 0xF44C42E9,
	0x625E7EC6, 0xE485B576, 0x6D51C245, 0x4FE1356D,
	0xF25F1437, 0x302B0A6D, 0xCD3A431B, 0xEF9519B3,
	0x8E3404DD, 0x514A0879, 0x3B139B22, 0x020BBEA6,
	0x8A67CC74, 0x29024E08, 0x80DC1CD1, 0xC4C6628B,
	0x2168C234, 0xC90FDAA2, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* q */
	{ 96, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0x7FFFFFFF, 0x549D6965, 0x25C16890, 
	0x707E8847, 0xA1EDADFE, 0x3A72D598, 0x047127D0, 
	0x5D6CA371, 0x3B84C460, 0xBD30AEB6, 0x5DF08BAB, 
	0x0BBD9006, 0x290F958C, 0x9F643532, 0x6C3B0139, 
	0x6CC50432, 0xF897FD03, 0x0D697735, 0xE771E913, 
	0x2512B0CE, 0x8F464A70, 0x6D8499EB, 0xD5FAD746, 
	0xD370F263, 0xD9CB87C2, 0xAE83063E, 0x457538AB, 
	0x2C6DF785, 0x767DC282, 0xEF8E5D32, 0xD42A90D5, 
	0x82283D19, 0xD6998B86, 0x45556216, 0x0AB9472D, 
	0x4C7D0288, 0x8AE9130C, 0x754AB572, 0x1CCAA4BE, 
	0x4AAC0B8C, 0xEF15E5FB, 0x37A62964, 0xDAE2AEF8, 
	0x7603D147, 0xCD93C1D1, 0x0C074301, 0xF1CF3B96, 
	0x171B671D, 0x19482F23, 0x650C10BE, 0x78BA3604, 
	0x255E4C02, 0xB3861AA7, 0xB84B4B36, 0xCF6A9483, 
	0x1042A95D, 0x0E3179AB, 0xEE51D6CB, 0xC1B2AE91, 
	0x7E9267AF, 0x348B1FD4, 0x0E2AE9CD, 0xCC6D241B, 
	0x50B1DF82, 0xE1003E5C, 0xF6722D9E, 0x24943328, 
	0xBE258FF3, 0xD74F9208, 0xAD44CFD2, 0xF71C35FD, 
	0x7A035BF6, 0x85FFAE5B, 0xD31BF6B5, 0x7A262174, 
	0x312F3F63, 0xF242DABB, 0xB6A8E122, 0xA7F09AB6, 
	0xF92F8A1B, 0x98158536, 0xE69D218D, 0xF7CA8CD9, 
	0xC71A026E, 0x28A5043C, 0x1D89CD91, 0x0105DF53, 
	0x4533E63A, 0x94812704, 0xC06E0E68, 0x62633145, 
	0x10B4611A, 0xE487ED51, 0xFFFFFFFF, 0x7FFFFFFF } },
	/* g */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000002 } },
	/* Checksums */
	0xFB7DF2C8, 0x1D70CEE7, 0x847050E5
	};
#endif /* 3072-bit bignums */

#endif /* 64- vs 32-bit */

/****************************************************************************
*																			*
*								DH Access Functions							*
*																			*
****************************************************************************/

/* Load a fixed DH key of the appropriate size */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int loadDHparams( INOUT CONTEXT_INFO *contextInfoPtr, 
				  IN_LENGTH_PKC const int requestedKeySize )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const DH_DOMAINPARAMS *domainParams;
#ifdef CREATE_BIGNUM_VALUES
	int status;
#endif /* CREATE_BIGNUM_VALUES */

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( requestedKeySize >= MIN_PKCSIZE && \
			  requestedKeySize <= CRYPT_MAX_PKCSIZE );

	/* Convert the fixed-format bignums to encoded BIGNUMs */
#ifdef CREATE_BIGNUM_VALUES
	checkDHdata();
	status = loadDHparamsFixed( contextInfoPtr->objectHandle, 
								bitsToBytes( 1024 ) );
	if( cryptStatusError( status ) )
		return( status );
	if( pkcInfo->domainParams != NULL )
		{
		/* If we're loading known domain parameters in order to calculate q 
		   from { p, g } or to calculate the parameter checksum, the values
		   won't be stored in the dlpParam values but as constant 
		   domainParams data */
		domainParams = pkcInfo->domainParams;
		BN_copy( &pkcInfo->dlpParam_q, &domainParams->p );
		}
	else
		BN_copy( &pkcInfo->dlpParam_q, &pkcInfo->dlpParam_p );
	BN_sub_word( &pkcInfo->dlpParam_q, 1 );
	BN_rshift( &pkcInfo->dlpParam_q, &pkcInfo->dlpParam_q, 1 );	/* q = (p-1)/2 */
	printBignum( &pkcInfo->dlpParam_p, "p" );
	printBignum( &pkcInfo->dlpParam_q, "q" );
	printBignum( &pkcInfo->dlpParam_g, "g" );
	DEBUG_PRINT(( "\t/* Checksums */\n\t" ));
	printBignumChecksum( &pkcInfo->dlpParam_p );
	printBignumChecksum( &pkcInfo->dlpParam_q );
	printBignumChecksum( &pkcInfo->dlpParam_g );
	DEBUG_PRINT(( "\n" ));
#endif /* CREATE_BIGNUM_VALUES */

	/* Get the built-in DH key value that corresponds best to the client's 
	   requested key size.  We allow for a bit of slop to avoid having 
	   something like a 2049-bit requested key size lead to the use of a 
	   3072-bit key value.

	   In theory for protocols that support the use of arbitrary DH 
	   parameters we should probably generate a new DH key each time:

		status = krnlSendMessage( iDHContext, IMESSAGE_SETATTRIBUTE,
								  ( MESSAGE_CAST ) &requestedKeySize,
								  CRYPT_CTXINFO_KEYSIZE );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( iDHContext, IMESSAGE_CTX_GENKEY, 
									  NULL, FALSE );

	   however because the handshake is set up so that the client (rather 
	   than the server) chooses the key size we can't actually perform the 
	   generation until we're in the middle of the handshake.  This means 
	   that the server will grind to a halt during each handshake as it 
	   generates a new key of whatever size takes the client's fancy (it 
	   also leads to a nice potential DoS attack on the server).  To avoid 
	   this problem we use fixed keys of various common sizes.
	   
	   As late as 2014 Java still can't handle DH keys over 1024 bits (it 
	   only allows keys ranging from 512-1024 bits):

		java.security.InvalidAlgorithmParameterException: Prime size must be 
		multiple of 64, and can only range from 512 to 1024 (inclusive)

	   so if you need to talk to a system built in Java you need to first
	   enable the use of unsound 1024-bit keys using USE_DH1024 and then 
	   hardcode the key size to 1024 bits, the largest size that Java will 
	   allow */
#ifdef USE_DH1024
	if( requestedKeySize <= 128 + 8 )
		domainParams = &dh1024params;
	else
#endif /* USE_DH1024 */
	if( requestedKeySize <= 192 + 8 )
		domainParams = &dh1536params;
	else
#if CRYPT_MAX_PKCSIZE >= bitsToBytes( 3072 )
	if( requestedKeySize <= 256 + 8 )
		domainParams = &dh2048params;
	else
		domainParams = &dh3072params;
#else
		domainParams = &dh2048params;
#endif /* 3072-bit bignums */

	/* Make sure that the domain parameters are in order */
	if( !checksumDomainParameters( domainParams, FALSE ) )
		{
		DEBUG_DIAG(( "Fixed DH value for requested %d-bit key has been "
					 "corrupted", requestedKeySize ));
		retIntError();
		}

	pkcInfo->domainParams = domainParams;

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );
	
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int checkFixedDHparams( const INOUT PKC_INFO *pkcInfo,
						OUT const DH_DOMAINPARAMS **domainParamsPtr )
	{
	const BIGNUM *p = &pkcInfo->dlpParam_p, *g = &pkcInfo->dlpParam_g;

	assert( isReadPtr( pkcInfo, sizeof( PKC_INFO ) ) );
	assert( isReadPtr( domainParamsPtr, sizeof( DH_DOMAINPARAMS * ) ) );

	REQUIRES( sanityCheckPKCInfo( pkcInfo ) );

	/* Clear return value */
	*domainParamsPtr = NULL;

	/* Compare the { p, g } value that we've been given against our known
	   { p, g } values.  We check for the values in order of likelihood */
	if( !BN_cmp( &dh2048params.p, p ) && !BN_cmp( &dh2048params.g, g ) )
		{
		DEBUG_DIAG(( "Using built-in DH value for requested 2048-bit key" ));
		*domainParamsPtr = &dh2048params;
		return( OK_SPECIAL );
		}
	if( !BN_cmp( &dh1536params.p, p ) && !BN_cmp( &dh1536params.g, g ) )
		{
		DEBUG_DIAG(( "Using built-in DH value for requested 1536-bit key" ));
		*domainParamsPtr = &dh1536params;
		return( OK_SPECIAL );
		}
	if( !BN_cmp( &dh1024params.p, p ) && !BN_cmp( &dh1024params.g, g ) )
		{
		DEBUG_DIAG(( "Using built-in DH value for requested 1024-bit key" ));
		*domainParamsPtr = &dh1024params;
		return( OK_SPECIAL );
		}
#if CRYPT_MAX_PKCSIZE >= bitsToBytes( 3072 )
	if( !BN_cmp( &dh3072params.p, p ) && !BN_cmp( &dh3072params.g, g ) )
		{
		DEBUG_DIAG(( "Using built-in DH value for requested 3072-bit key" ));
		*domainParamsPtr = &dh3072params;
		return( OK_SPECIAL );
		}
#endif /* 3072-bit bignums */

	return( CRYPT_OK );
	}
#endif /* USE_SSH || USE_SSL */

/****************************************************************************
*																			*
*							ECC Domain Parameters							*
*																			*
****************************************************************************/

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/* We always use pre-generated parameters both because it's unlikely that 
   anyone will ever decide to generate nonstandard parameters when standard 
   ones are available (or at least no sane person would, no doubt every 
   little standards committee wanting to make their mark will feel the need 
   to have their own personal incompatible parameters).  In addition using 
   externally-generated parameters can (as for DSA) lead to problems with 
   maliciously-generated values (see "CM-Curves with good Cryptography 
   Properties", Neal Koblitz, Proceedings of Crypto'91, p.279), and finally 
   (also like DSA) it can lead to problems with parameter-substitution 
   attacks (see "Digital Signature Schemes with Domain Parameters", Serge 
   Vaudenay, Proceedings of ACISP'04, p.188) */

#ifdef CREATE_BIGNUM_VALUES

typedef struct {
	CRYPT_ECCCURVE_TYPE paramType;
	const int curveSizeBits;
	const BYTE *p, *a, *b, *gx, *gy, *n, *h;
	const BYTE *hashValue;
	} ECC_DOMAIN_PARAMS;

static const ECC_DOMAIN_PARAMS domainParamTbl[] = {
	/* NIST P-256, X9.62 p256r1, SECG p256r1 */
	{ CRYPT_ECCCURVE_P256, 256,
	  MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x01" \
			  "\x00\x00\x00\x00\x00\x00\x00\x00" \
			  "\x00\x00\x00\x00\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x01" \
			  "\x00\x00\x00\x00\x00\x00\x00\x00" \
			  "\x00\x00\x00\x00\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x5A\xC6\x35\xD8\xAA\x3A\x93\xE7" \
			  "\xB3\xEB\xBD\x55\x76\x98\x86\xBC" \
			  "\x65\x1D\x06\xB0\xCC\x53\xB0\xF6" \
			  "\x3B\xCE\x3C\x3E\x27\xD2\x60\x4B" ),
	  MKDATA( "\x6B\x17\xD1\xF2\xE1\x2C\x42\x47" \
			  "\xF8\xBC\xE6\xE5\x63\xA4\x40\xF2" \
			  "\x77\x03\x7D\x81\x2D\xEB\x33\xA0" \
			  "\xF4\xA1\x39\x45\xD8\x98\xC2\x96" ),
	  MKDATA( "\x4F\xE3\x42\xE2\xFE\x1A\x7F\x9B" \
			  "\x8E\xE7\xEB\x4A\x7C\x0F\x9E\x16" \
			  "\x2B\xCE\x33\x57\x6B\x31\x5E\xCE" \
			  "\xCB\xB6\x40\x68\x37\xBF\x51\xF5" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xBC\xE6\xFA\xAD\xA7\x17\x9E\x84" \
			  "\xF3\xB9\xCA\xC2\xFC\x63\x25\x51" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\x40\x31\x2F\x8C\x04\xBB\x5E\x5C" \
			  "\x0F\x12\x32\x75\x91\xE8\x71\x65" ) },

	/* Brainpool p256r1 */
	{ CRYPT_ECCCURVE_BRAINPOOL_P256, 256,
	  MKDATA( "\xA9\xFB\x57\xDB\xA1\xEE\xA9\xBC" \
			  "\x3E\x66\x0A\x90\x9D\x83\x8D\x72" \
			  "\x6E\x3B\xF6\x23\xD5\x26\x20\x28" \
			  "\x20\x13\x48\x1D\x1F\x6E\x53\x77" ),
	  MKDATA( "\x7D\x5A\x09\x75\xFC\x2C\x30\x57" \
			  "\xEE\xF6\x75\x30\x41\x7A\xFF\xE7" \
			  "\xFB\x80\x55\xC1\x26\xDC\x5C\x6C" \
			  "\xE9\x4A\x4B\x44\xF3\x30\xB5\xD9" ),
	  MKDATA( "\x26\xDC\x5C\x6C\xE9\x4A\x4B\x44" \
			  "\xF3\x30\xB5\xD9\xBB\xD7\x7C\xBF" \
			  "\x95\x84\x16\x29\x5C\xF7\xE1\xCE" \
			  "\x6B\xCC\xDC\x18\xFF\x8C\x07\xB6" ),
	  MKDATA( "\x8B\xD2\xAE\xB9\xCB\x7E\x57\xCB" \
			  "\x2C\x4B\x48\x2F\xFC\x81\xB7\xAF" \
			  "\xB9\xDE\x27\xE1\xE3\xBD\x23\xC2" \
			  "\x3A\x44\x53\xBD\x9A\xCE\x32\x62" ),
	  MKDATA( "\x54\x7E\xF8\x35\xC3\xDA\xC4\xFD" \
			  "\x97\xF8\x46\x1A\x14\x61\x1D\xC9" \
			  "\xC2\x77\x45\x13\x2D\xED\x8E\x54" \
			  "\x5C\x1D\x54\xC7\x2F\x04\x69\x97" ),
	  MKDATA( "\xA9\xFB\x57\xDB\xA1\xEE\xA9\xBC" \
			  "\x3E\x66\x0A\x90\x9D\x83\x8D\x71" \
			  "\x8C\x39\x7A\xA3\xB5\x61\xA6\xF7" \
			  "\x90\x1E\x0E\x82\x97\x48\x56\xA7" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\x3F\xBF\x15\xA2\x89\x27\x68\x83" \
			  "\x67\xAC\x82\x26\x48\xFA\x44\x8E" ) },

	/* NIST P-384, SECG p384r1 */
	{ CRYPT_ECCCURVE_P384, 384,
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\x00\x00\x00\x00" \
			  "\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\x00\x00\x00\x00" \
			  "\x00\x00\x00\x00\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\xB3\x31\x2F\xA7\xE2\x3E\xE7\xE4" \
			  "\x98\x8E\x05\x6B\xE3\xF8\x2D\x19" \
			  "\x18\x1D\x9C\x6E\xFE\x81\x41\x12" \
			  "\x03\x14\x08\x8F\x50\x13\x87\x5A" \
			  "\xC6\x56\x39\x8D\x8A\x2E\xD1\x9D" \
			  "\x2A\x85\xC8\xED\xD3\xEC\x2A\xEF" ),
	  MKDATA( "\xAA\x87\xCA\x22\xBE\x8B\x05\x37" \
			  "\x8E\xB1\xC7\x1E\xF3\x20\xAD\x74" \
			  "\x6E\x1D\x3B\x62\x8B\xA7\x9B\x98" \
			  "\x59\xF7\x41\xE0\x82\x54\x2A\x38" \
			  "\x55\x02\xF2\x5D\xBF\x55\x29\x6C" \
			  "\x3A\x54\x5E\x38\x72\x76\x0A\xB7" ),
	  MKDATA( "\x36\x17\xDE\x4A\x96\x26\x2C\x6F" \
			  "\x5D\x9E\x98\xBF\x92\x92\xDC\x29" \
			  "\xF8\xF4\x1D\xBD\x28\x9A\x14\x7C" \
			  "\xE9\xDA\x31\x13\xB5\xF0\xB8\xC0" \
			  "\x0A\x60\xB1\xCE\x1D\x7E\x81\x9D" \
			  "\x7A\x43\x1D\x7C\x90\xEA\x0E\x5F" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xC7\x63\x4D\x81\xF4\x37\x2D\xDF" \
			 "\x58\x1A\x0D\xB2\x48\xB0\xA7\x7A" \
			 "\xEC\xEC\x19\x6A\xCC\xC5\x29\x73" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\xC2\xDF\x12\x1E\x86\x76\xF9\x5A" \
			  "\x6C\xD9\xA9\x3C\x18\xA3\xA8\x79" ) },

	/* Brainpool p384r1 */
	{ CRYPT_ECCCURVE_BRAINPOOL_P384, 384,
	  MKDATA( "\x8C\xB9\x1E\x82\xA3\x38\x6D\x28" \
			  "\x0F\x5D\x6F\x7E\x50\xE6\x41\xDF" \
			  "\x15\x2F\x71\x09\xED\x54\x56\xB4" \
			  "\x12\xB1\xDA\x19\x7F\xB7\x11\x23" \
			  "\xAC\xD3\xA7\x29\x90\x1D\x1A\x71" \
			  "\x87\x47\x00\x13\x31\x07\xEC\x53" ),
	  MKDATA( "\x7B\xC3\x82\xC6\x3D\x8C\x15\x0C" \
			  "\x3C\x72\x08\x0A\xCE\x05\xAF\xA0" \
			  "\xC2\xBE\xA2\x8E\x4F\xB2\x27\x87" \
			  "\x13\x91\x65\xEF\xBA\x91\xF9\x0F" \
			  "\x8A\xA5\x81\x4A\x50\x3A\xD4\xEB" \
			  "\x04\xA8\xC7\xDD\x22\xCE\x28\x26" ),
	  MKDATA( "\x04\xA8\xC7\xDD\x22\xCE\x28\x26" \
			  "\x8B\x39\xB5\x54\x16\xF0\x44\x7C" \
			  "\x2F\xB7\x7D\xE1\x07\xDC\xD2\xA6" \
			  "\x2E\x88\x0E\xA5\x3E\xEB\x62\xD5" \
			  "\x7C\xB4\x39\x02\x95\xDB\xC9\x94" \
			  "\x3A\xB7\x86\x96\xFA\x50\x4C\x11" ),
	  MKDATA( "\x1D\x1C\x64\xF0\x68\xCF\x45\xFF" \
			  "\xA2\xA6\x3A\x81\xB7\xC1\x3F\x6B" \
			  "\x88\x47\xA3\xE7\x7E\xF1\x4F\xE3" \
			  "\xDB\x7F\xCA\xFE\x0C\xBD\x10\xE8" \
			  "\xE8\x26\xE0\x34\x36\xD6\x46\xAA" \
			  "\xEF\x87\xB2\xE2\x47\xD4\xAF\x1E" ),
	  MKDATA( "\x8A\xBE\x1D\x75\x20\xF9\xC2\xA4" \
			  "\x5C\xB1\xEB\x8E\x95\xCF\xD5\x52" \
			  "\x62\xB7\x0B\x29\xFE\xEC\x58\x64" \
			  "\xE1\x9C\x05\x4F\xF9\x91\x29\x28" \
			  "\x0E\x46\x46\x21\x77\x91\x81\x11" \
			  "\x42\x82\x03\x41\x26\x3C\x53\x15" ),
	  MKDATA( "\x8C\xB9\x1E\x82\xA3\x38\x6D\x28" \
			  "\x0F\x5D\x6F\x7E\x50\xE6\x41\xDF" \
			  "\x15\x2F\x71\x09\xED\x54\x56\xB3" \
			  "\x1F\x16\x6E\x6C\xAC\x04\x25\xA7" \
			  "\xCF\x3A\xB6\xAF\x6B\x7F\xC3\x10" \
			  "\x3B\x88\x32\x02\xE9\x04\x65\x65" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\x04\xDE\xA5\xE1\x39\x3B\xE0\xB5" \
			  "\x6F\x2C\x0B\xC7\xAF\x9E\xF9\x07" ) },

	/* NIST P-521, SECG p521r1 */
	{ CRYPT_ECCCURVE_P521, 521,
	  MKDATA( "\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF" ),
	  MKDATA( "\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFC" ),
	  MKDATA( "\x00\x51\x95\x3E\xB9\x61\x8E\x1C" \
			  "\x9A\x1F\x92\x9A\x21\xA0\xB6\x85" \
			  "\x40\xEE\xA2\xDA\x72\x5B\x99\xB3" \
			  "\x15\xF3\xB8\xB4\x89\x91\x8E\xF1" \
			  "\x09\xE1\x56\x19\x39\x51\xEC\x7E" \
			  "\x93\x7B\x16\x52\xC0\xBD\x3B\xB1" \
			  "\xBF\x07\x35\x73\xDF\x88\x3D\x2C" \
			  "\x34\xF1\xEF\x45\x1F\xD4\x6B\x50" \
			  "\x3F\x00" ),
	  MKDATA( "\x00\xC6\x85\x8E\x06\xB7\x04\x04" \
			  "\xE9\xCD\x9E\x3E\xCB\x66\x23\x95" \
			  "\xB4\x42\x9C\x64\x81\x39\x05\x3F" \
			  "\xB5\x21\xF8\x28\xAF\x60\x6B\x4D" \
			  "\x3D\xBA\xA1\x4B\x5E\x77\xEF\xE7" \
			  "\x59\x28\xFE\x1D\xC1\x27\xA2\xFF" \
			  "\xA8\xDE\x33\x48\xB3\xC1\x85\x6A" \
			  "\x42\x9B\xF9\x7E\x7E\x31\xC2\xE5" \
			  "\xBD\x66" ),
	  MKDATA( "\x01\x18\x39\x29\x6A\x78\x9A\x3B" \
			  "\xC0\x04\x5C\x8A\x5F\xB4\x2C\x7D" \
			  "\x1B\xD9\x98\xF5\x44\x49\x57\x9B" \
			  "\x44\x68\x17\xAF\xBD\x17\x27\x3E" \
			  "\x66\x2C\x97\xEE\x72\x99\x5E\xF4" \
			  "\x26\x40\xC5\x50\xB9\x01\x3F\xAD" \
			  "\x07\x61\x35\x3C\x70\x86\xA2\x72" \
			  "\xC2\x40\x88\xBE\x94\x76\x9F\xD1" \
			  "\x66\x50" ),
	  MKDATA( "\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFA\x51\x86\x87\x83\xBF\x2F" \
			  "\x96\x6B\x7F\xCC\x01\x48\xF7\x09" \
			  "\xA5\xD0\x3B\xB5\xC9\xB8\x89\x9C" \
			  "\x47\xAE\xBB\x6F\xB7\x1E\x91\x38" \
			  "\x64\x09" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\xC4\x0E\xC4\xCC\x78\x19\x0A\xA6" \
			  "\x8C\x7E\xA0\xB1\x14\xA4\xBC\x23" ) },

	/* Brainpool p512r1 */
	{ CRYPT_ECCCURVE_BRAINPOOL_P512, 512,
	  MKDATA( "\xAA\xDD\x9D\xB8\xDB\xE9\xC4\x8B" \
			  "\x3F\xD4\xE6\xAE\x33\xC9\xFC\x07" \
			  "\xCB\x30\x8D\xB3\xB3\xC9\xD2\x0E" \
			  "\xD6\x63\x9C\xCA\x70\x33\x08\x71" \
			  "\x7D\x4D\x9B\x00\x9B\xC6\x68\x42" \
			  "\xAE\xCD\xA1\x2A\xE6\xA3\x80\xE6" \
			  "\x28\x81\xFF\x2F\x2D\x82\xC6\x85" \
			  "\x28\xAA\x60\x56\x58\x3A\x48\xF3" ),
	  MKDATA( "\x78\x30\xA3\x31\x8B\x60\x3B\x89" \
			  "\xE2\x32\x71\x45\xAC\x23\x4C\xC5" \
			  "\x94\xCB\xDD\x8D\x3D\xF9\x16\x10" \
			  "\xA8\x34\x41\xCA\xEA\x98\x63\xBC" \
			  "\x2D\xED\x5D\x5A\xA8\x25\x3A\xA1" \
			  "\x0A\x2E\xF1\xC9\x8B\x9A\xC8\xB5" \
			  "\x7F\x11\x17\xA7\x2B\xF2\xC7\xB9" \
			  "\xE7\xC1\xAC\x4D\x77\xFC\x94\xCA" ),
	  MKDATA( "\x3D\xF9\x16\x10\xA8\x34\x41\xCA" \
			  "\xEA\x98\x63\xBC\x2D\xED\x5D\x5A" \
			  "\xA8\x25\x3A\xA1\x0A\x2E\xF1\xC9" \
			  "\x8B\x9A\xC8\xB5\x7F\x11\x17\xA7" \
			  "\x2B\xF2\xC7\xB9\xE7\xC1\xAC\x4D" \
			  "\x77\xFC\x94\xCA\xDC\x08\x3E\x67" \
			  "\x98\x40\x50\xB7\x5E\xBA\xE5\xDD" \
			  "\x28\x09\xBD\x63\x80\x16\xF7\x23" ),
	  MKDATA( "\x81\xAE\xE4\xBD\xD8\x2E\xD9\x64" \
			  "\x5A\x21\x32\x2E\x9C\x4C\x6A\x93" \
			  "\x85\xED\x9F\x70\xB5\xD9\x16\xC1" \
			  "\xB4\x3B\x62\xEE\xF4\xD0\x09\x8E" \
			  "\xFF\x3B\x1F\x78\xE2\xD0\xD4\x8D" \
			  "\x50\xD1\x68\x7B\x93\xB9\x7D\x5F" \
			  "\x7C\x6D\x50\x47\x40\x6A\x5E\x68" \
			  "\x8B\x35\x22\x09\xBC\xB9\xF8\x22" ),
	  MKDATA( "\x7D\xDE\x38\x5D\x56\x63\x32\xEC" \
			  "\xC0\xEA\xBF\xA9\xCF\x78\x22\xFD" \
			  "\xF2\x09\xF7\x00\x24\xA5\x7B\x1A" \
			  "\xA0\x00\xC5\x5B\x88\x1F\x81\x11" \
			  "\xB2\xDC\xDE\x49\x4A\x5F\x48\x5E" \
			  "\x5B\xCA\x4B\xD8\x8A\x27\x63\xAE" \
			  "\xD1\xCA\x2B\x2F\xA8\xF0\x54\x06" \
			  "\x78\xCD\x1E\x0F\x3A\xD8\x08\x92" ),
	  MKDATA( "\xAA\xDD\x9D\xB8\xDB\xE9\xC4\x8B" \
			  "\x3F\xD4\xE6\xAE\x33\xC9\xFC\x07" \
			  "\xCB\x30\x8D\xB3\xB3\xC9\xD2\x0E" \
			  "\xD6\x63\x9C\xCA\x70\x33\x08\x70" \
			  "\x55\x3E\x5C\x41\x4C\xA9\x26\x19" \
			  "\x41\x86\x61\x19\x7F\xAC\x10\x47" \
			  "\x1D\xB1\xD3\x81\x08\x5D\xDA\xDD" \
			  "\xB5\x87\x96\x82\x9C\xA9\x00\x69" ),
	  MKDATA( "\x01" ),
	  MKDATA( "\x2C\x2A\xE4\xF6\x05\xB6\xAF\xB5"
			  "\x78\xC5\x90\x1E\x45\x9F\x4B\x5C" ) },

	/* End-of-list marker */
	{ CRYPT_ECCCURVE_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
		{ CRYPT_ECCCURVE_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
	};

/* Checksum values for the ECC data */

static BOOLEAN eccChecksumsSet = FALSE;
static int eccChecksums[ 8 + 8 ];

/* Check that the DH key data is valid */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN hashECCparams( const ECC_DOMAIN_PARAMS *eccParams,
							  IN_BUFFER( 20 ) const void *hashValue )
	{
	HASH_FUNCTION hashFunction;
	HASHINFO hashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	const int curveSize = bitsToBytes( eccParams->curveSizeBits );
	int hashSize;

	assert( isReadPtr( eccParams, sizeof( ECC_DOMAIN_PARAMS ) ) );

	getHashParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, &hashSize );
	hashFunction( hashInfo, NULL, 0, eccParams->p, curveSize, HASH_STATE_START );
	hashFunction( hashInfo, NULL, 0, eccParams->a, curveSize, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, NULL, 0, eccParams->b, curveSize, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, NULL, 0, eccParams->gx, curveSize, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, NULL, 0, eccParams->gy, curveSize, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, NULL, 0, eccParams->n, curveSize, HASH_STATE_CONTINUE );
	hashFunction( hashInfo, hash, CRYPT_MAX_HASHSIZE, eccParams->h, 1, HASH_STATE_END );

	return( !memcmp( hash, hashValue, 16 ) ? TRUE : FALSE );
	}

CHECK_RETVAL \
static int initCheckECCdata( void )
	{
	int i, LOOP_ITERATOR;

	/* Check the SHA-1 values for the ECC data and set up the corresponding
	   checksums for the overall ECC parameters */
	LOOP_MED( i = 0, domainParamTbl[ i ].paramType != CRYPT_ECCCURVE_NONE && \
					 i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ), 
			  i++ )
		{
		const ECC_DOMAIN_PARAMS *eccParams = &domainParamTbl[ i ];

		if( !isEnumRange( eccParams->paramType, CRYPT_ECCCURVE ) || \
			eccParams->curveSizeBits < 192 || \
			eccParams->curveSizeBits > 521 )
			retIntError();
		if( !hashECCparams( eccParams, eccParams->hashValue ) )
			retIntError();
		eccChecksums[ i ] = checksumData( eccParams, 
										  sizeof( ECC_DOMAIN_PARAMS ) );
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ) );
	eccChecksumsSet = TRUE;

	return( CRYPT_OK );
	}

/* Initialise the bignums for the domain parameter values { p, a, b, gx, gy, 
   n }.  Note that although the cofactor h is given for the standard named 
   curves it's not used for any ECC operations so we don't bother allocating 
   a bignum to it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int loadECCparamsFixed( INOUT CONTEXT_INFO *contextInfoPtr,
						IN_ENUM( CRYPT_ECCCURVE ) \
							const CRYPT_ECCCURVE_TYPE curveType )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const ECC_DOMAIN_PARAMS *eccParams = NULL;
	BIGNUM bignums[ 7 ];
	int curveSize, checksum DUMMY_INIT, i, LOOP_ITERATOR;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isEnumRange( curveType, CRYPT_ECCCURVE ) );

	/* Set up the ECC parameter checksums if required */
	if( !eccChecksumsSet )
		{
		status = initCheckECCdata();
		if( cryptStatusError( status ) )
			return( status );
		eccChecksumsSet = TRUE;
		}

	/* Find the parameter info for this curve */
	LOOP_MED( i = 0, domainParamTbl[ i ].paramType != CRYPT_ECCCURVE_NONE && \
					 i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ), 
			  i++ )
		{
		if( domainParamTbl[ i ].paramType == curveType )
			{
			eccParams = &domainParamTbl[ i ];
			checksum = eccChecksums[ i ];
			break;
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ) );
	ENSURES( eccParams != NULL );
	if( checksum != checksumData( eccParams, sizeof( ECC_DOMAIN_PARAMS ) ) )
		retIntError();

	/* For the named curve the key size is defined by exective fiat based
	   on the curve type rather than being taken from the public-key value 
	   (which in this case is the magnitude of the point Q on the curve), so 
	   we set it explicitly based on the curve type */
	pkcInfo->curveType = curveType;
	pkcInfo->keySizeBits = eccParams->curveSizeBits;
	curveSize = bitsToBytes( eccParams->curveSizeBits );

	/* Load the parameters into the context bignums */
	LOOP_MED( i = 0, i < 7, i++ )
		BN_init( &bignums[ i ] );
	ENSURES( LOOP_BOUND_OK );
	CKPTR( BN_bin2bn( eccParams->p, curveSize, &bignums[ 0 ] ) );
	CKPTR( BN_bin2bn( eccParams->a, curveSize, &bignums[ 1 ] ) );
	CKPTR( BN_bin2bn( eccParams->b, curveSize, &bignums[ 2 ] ) );
	CKPTR( BN_bin2bn( eccParams->gx, curveSize, &bignums[ 3 ] ) );
	CKPTR( BN_bin2bn( eccParams->gy, curveSize, &bignums[ 4 ] ) );
	CKPTR( BN_bin2bn( eccParams->n, curveSize, &bignums[ 5 ] ) );
	CKPTR( BN_bin2bn( eccParams->h, 1, &bignums[ 6 ] ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	printBignum( &bignums[ 0 ], "p" );
	printBignum( &bignums[ 1 ], "a" );
	printBignum( &bignums[ 2 ], "b" );
	printBignum( &bignums[ 3 ], "gx" );
	printBignum( &bignums[ 4 ], "gy" );
	printBignum( &bignums[ 5 ], "n" );
	printBignum( &bignums[ 6 ], "h" );
	DEBUG_PRINT(( "\t/* Checksums */\n\t" ));
	printBignumChecksum( &bignums[ 0 ] );
	printBignumChecksum( &bignums[ 1 ] );
#if BN_BITS2 == 64
	DEBUG_PRINT( ( "\n\t" ) );
#endif /* 64- vs 32-bit */
	printBignumChecksum( &bignums[ 2 ] );
	printBignumChecksum( &bignums[ 3 ] );
	DEBUG_PRINT(( "\n\t" ));
	printBignumChecksum( &bignums[ 4 ] );
	printBignumChecksum( &bignums[ 5 ] );
#if BN_BITS2 == 64
	DEBUG_PRINT( ( "\n\t" ) );
#endif /* 64- vs 32-bit */
	printBignumChecksum( &bignums[ 6 ] );
	DEBUG_PRINT(( "\n" ));

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}
#endif /* CREATE_BIGNUM_VALUES */

/* ECC values as pre-encoded bignums, allowing them to be used directly */

#if BN_BITS2 == 64

/* NIST P-256, X9.62 p256r1, SECG p256r1, CRYPT_ECCCURVE_P256 */

static const ECC_DOMAINPARAMS p256params = {
	/* p */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0x00000000FFFFFFFFULL,
	0x0000000000000000ULL, 0xFFFFFFFF00000001ULL } },
	/* a */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFCULL, 0x00000000FFFFFFFFULL,
	0x0000000000000000ULL, 0xFFFFFFFF00000001ULL } },
	/* b */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x3BCE3C3E27D2604BULL, 0x651D06B0CC53B0F6ULL,
	0xB3EBBD55769886BCULL, 0x5AC635D8AA3A93E7ULL } },
	/* gx */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xF4A13945D898C296ULL, 0x77037D812DEB33A0ULL,
	0xF8BCE6E563A440F2ULL, 0x6B17D1F2E12C4247ULL } },
	/* gy */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xCBB6406837BF51F5ULL, 0x2BCE33576B315ECEULL,
	0x8EE7EB4A7C0F9E16ULL, 0x4FE342E2FE1A7F9BULL } },
	/* n */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xF3B9CAC2FC632551ULL, 0xBCE6FAADA7179E84ULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFF00000000ULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0xA72822FA39201D4CULL, 0x77FB7550AD79DEE0ULL, 
	0x4E550413B3F54BB8ULL, 0xE5D81ACCD382A875ULL,
	0xA2C6307087289DE6ULL, 0xBB9E6DDEC3E0E95AULL, 
	0x26402E6EE1F4833EULL
	};

/* Brainpool p256r1, CRYPT_ECCCURVE_BRAINPOOL_P256 */

static const ECC_DOMAINPARAMS brainpool256params = {
	/* p */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x2013481D1F6E5377ULL, 0x6E3BF623D5262028ULL,
	0x3E660A909D838D72ULL, 0xA9FB57DBA1EEA9BCULL } },
	/* a */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0xE94A4B44F330B5D9ULL, 0xFB8055C126DC5C6CULL,
	0xEEF67530417AFFE7ULL, 0x7D5A0975FC2C3057ULL } },
	/* b */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x6BCCDC18FF8C07B6ULL, 0x958416295CF7E1CEULL,
	0xF330B5D9BBD77CBFULL, 0x26DC5C6CE94A4B44ULL } },
	/* gx */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x3A4453BD9ACE3262ULL, 0xB9DE27E1E3BD23C2ULL,
	0x2C4B482FFC81B7AFULL, 0x8BD2AEB9CB7E57CBULL } },
	/* gy */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x5C1D54C72F046997ULL, 0xC27745132DED8E54ULL,
	0x97F8461A14611DC9ULL, 0x547EF835C3DAC4FDULL } },
	/* n */
	{ 4, FALSE, BN_FLG_STATIC_DATA, {
	0x901E0E82974856A7ULL, 0x8C397AA3B561A6F7ULL,
	0x3E660A909D838D71ULL, 0xA9FB57DBA1EEA9BCULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0x8C58020DDBF6A665ULL, 0x50890F90EEF60A86ULL, 
	0xEA0C456931CC1EEFULL, 0x70E10A10445E70FAULL,
	0xAD55C5C5D51EC478ULL, 0x2AB6E0C318795E4FULL, 
	0x26402E6EE1F4833EULL
	};

/* NIST P-384, SECG p384r1, CRYPT_ECCCURVE_P384 */

static const ECC_DOMAINPARAMS p384params = {
	/* p */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x00000000FFFFFFFFULL, 0xFFFFFFFF00000000ULL,
	0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* a */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x00000000FFFFFFFCULL, 0xFFFFFFFF00000000ULL,
	0xFFFFFFFFFFFFFFFEULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* b */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x2A85C8EDD3EC2AEFULL, 0xC656398D8A2ED19DULL,
	0x0314088F5013875AULL, 0x181D9C6EFE814112ULL,
	0x988E056BE3F82D19ULL, 0xB3312FA7E23EE7E4ULL } },
	/* gx */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x3A545E3872760AB7ULL, 0x5502F25DBF55296CULL,
	0x59F741E082542A38ULL, 0x6E1D3B628BA79B98ULL,
	0x8EB1C71EF320AD74ULL, 0xAA87CA22BE8B0537ULL } },
	/* gy */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x7A431D7C90EA0E5FULL, 0x0A60B1CE1D7E819DULL,
	0xE9DA3113B5F0B8C0ULL, 0xF8F41DBD289A147CULL,
	0x5D9E98BF9292DC29ULL, 0x3617DE4A96262C6FULL } },
	/* n */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0xECEC196ACCC52973ULL, 0x581A0DB248B0A77AULL,
	0xC7634D81F4372DDFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0x9A9A47599E9FA193ULL, 0x8A54B4DE8DBA0429ULL,
	0xFB1CEA2E3FFD90F0ULL, 0x14A04AC2065B13E6ULL,
	0xBC63FBAFCEB8C35DULL, 0xA679E5A399BA581CULL,
	0x26402E6EE1F4833EULL 
	};

/* Brainpool p384r1, CRYPT_ECCCURVE_BRAINPOOL_P384 */

static const ECC_DOMAINPARAMS brainpool384params = {
	/* p */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x874700133107EC53ULL, 0xACD3A729901D1A71ULL,
	0x12B1DA197FB71123ULL, 0x152F7109ED5456B4ULL,
	0x0F5D6F7E50E641DFULL, 0x8CB91E82A3386D28ULL } },
	/* a */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x04A8C7DD22CE2826ULL, 0x8AA5814A503AD4EBULL,
	0x139165EFBA91F90FULL, 0xC2BEA28E4FB22787ULL,
	0x3C72080ACE05AFA0ULL, 0x7BC382C63D8C150CULL } },
	/* b */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x3AB78696FA504C11ULL, 0x7CB4390295DBC994ULL,
	0x2E880EA53EEB62D5ULL, 0x2FB77DE107DCD2A6ULL,
	0x8B39B55416F0447CULL, 0x04A8C7DD22CE2826ULL } },
	/* gx */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0xEF87B2E247D4AF1EULL, 0xE826E03436D646AAULL,
	0xDB7FCAFE0CBD10E8ULL, 0x8847A3E77EF14FE3ULL,
	0xA2A63A81B7C13F6BULL, 0x1D1C64F068CF45FFULL } },
	/* gy */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x42820341263C5315ULL, 0x0E46462177918111ULL,
	0xE19C054FF9912928ULL, 0x62B70B29FEEC5864ULL,
	0x5CB1EB8E95CFD552ULL, 0x8ABE1D7520F9C2A4ULL } },
	/* n */
	{ 6, FALSE, BN_FLG_STATIC_DATA, {
	0x3B883202E9046565ULL, 0xCF3AB6AF6B7FC310ULL,
	0x1F166E6CAC0425A7ULL, 0x152F7109ED5456B3ULL,
	0x0F5D6F7E50E641DFULL, 0x8CB91E82A3386D28ULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0x6A9E0847ABC8E154ULL, 0x61C83241A722DD92ULL,
	0xD325B1754DE15E11ULL, 0x350A61B48CD1B7F9ULL,
	0xFAB31A42AC6B58C9ULL, 0xE36D9145DA891AF4ULL,
	0x26402E6EE1F4833EULL
	};

/* NIST P-521, SECG p521r1, CRYPT_ECCCURVE_P521 */

static const ECC_DOMAINPARAMS p521params = {
	/* p */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0x00000000000001FFULL } },
	/* a */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFFFFFFFFFCULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0x00000000000001FFULL } },
	/* b */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0xEF451FD46B503F00ULL, 0x3573DF883D2C34F1ULL,
	0x1652C0BD3BB1BF07ULL, 0x56193951EC7E937BULL,
	0xB8B489918EF109E1ULL, 0xA2DA725B99B315F3ULL,
	0x929A21A0B68540EEULL, 0x953EB9618E1C9A1FULL,
	0x0000000000000051ULL } },
	/* gx */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0xF97E7E31C2E5BD66ULL, 0x3348B3C1856A429BULL,
	0xFE1DC127A2FFA8DEULL, 0xA14B5E77EFE75928ULL,
	0xF828AF606B4D3DBAULL, 0x9C648139053FB521ULL,
	0x9E3ECB662395B442ULL, 0x858E06B70404E9CDULL,
	0x00000000000000C6ULL } },
	/* gy */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0x88BE94769FD16650ULL, 0x353C7086A272C240ULL,
	0xC550B9013FAD0761ULL, 0x97EE72995EF42640ULL,
	0x17AFBD17273E662CULL, 0x98F54449579B4468ULL,
	0x5C8A5FB42C7D1BD9ULL, 0x39296A789A3BC004ULL,
	0x0000000000000118ULL } },
	/* n */
	{ 9, FALSE, BN_FLG_STATIC_DATA, {
	0xBB6FB71E91386409ULL, 0x3BB5C9B8899C47AEULL,
	0x7FCC0148F709A5D0ULL, 0x51868783BF2F966BULL,
	0xFFFFFFFFFFFFFFFAULL, 0xFFFFFFFFFFFFFFFFULL,
	0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL,
	0x00000000000001FFULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0xDA99C55253FF5436ULL, 0x2847E17D2F34D05BULL,
	0x11D14F0DFE0C6BF1ULL, 0x5F95A0E0F9B321C8ULL,
	0x2EEA29DDC3441128ULL, 0x2595D584A94B217DULL,
	0x26402E6EE1F4833EULL
	};

/* Brainpool p512r1, CRYPT_ECCCURVE_BRAINPOOL_P512 */

static const ECC_DOMAINPARAMS brainpool512params = {
	/* p */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x28AA6056583A48F3ULL, 0x2881FF2F2D82C685ULL,
	0xAECDA12AE6A380E6ULL, 0x7D4D9B009BC66842ULL,
	0xD6639CCA70330871ULL, 0xCB308DB3B3C9D20EULL,
	0x3FD4E6AE33C9FC07ULL, 0xAADD9DB8DBE9C48BULL } },
	/* a */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xE7C1AC4D77FC94CAULL, 0x7F1117A72BF2C7B9ULL,
	0x0A2EF1C98B9AC8B5ULL, 0x2DED5D5AA8253AA1ULL,
	0xA83441CAEA9863BCULL, 0x94CBDD8D3DF91610ULL,
	0xE2327145AC234CC5ULL, 0x7830A3318B603B89ULL } },
	/* b */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x2809BD638016F723ULL, 0x984050B75EBAE5DDULL,
	0x77FC94CADC083E67ULL, 0x2BF2C7B9E7C1AC4DULL,
	0x8B9AC8B57F1117A7ULL, 0xA8253AA10A2EF1C9ULL,
	0xEA9863BC2DED5D5AULL, 0x3DF91610A83441CAULL } },
	/* gx */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x8B352209BCB9F822ULL, 0x7C6D5047406A5E68ULL,
	0x50D1687B93B97D5FULL, 0xFF3B1F78E2D0D48DULL,
	0xB43B62EEF4D0098EULL, 0x85ED9F70B5D916C1ULL,
	0x5A21322E9C4C6A93ULL, 0x81AEE4BDD82ED964ULL } },
	/* gy */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x78CD1E0F3AD80892ULL, 0xD1CA2B2FA8F05406ULL,
	0x5BCA4BD88A2763AEULL, 0xB2DCDE494A5F485EULL,
	0xA000C55B881F8111ULL, 0xF209F70024A57B1AULL,
	0xC0EABFA9CF7822FDULL, 0x7DDE385D566332ECULL } },
	/* n */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xB58796829CA90069ULL, 0x1DB1D381085DDADDULL,
	0x418661197FAC1047ULL, 0x553E5C414CA92619ULL,
	0xD6639CCA70330870ULL, 0xCB308DB3B3C9D20EULL,
	0x3FD4E6AE33C9FC07ULL, 0xAADD9DB8DBE9C48BULL } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x0000000000000001ULL } },
	/* Checksums */
	0xC1DA14200D9335A0ULL, 0x0576ECE4ABA7F09BULL,
	0x4319F6256D8AAFCFULL, 0x1042D928E89F3778ULL,
	0x0A26038C333433C3ULL, 0x1B30D8586B566AFDULL,
	0x26402E6EE1F4833EULL
	};

#else

/* NIST P-256, X9.62 p256r1, SECG p256r1, CRYPT_ECCCURVE_P256 */

static const ECC_DOMAINPARAMS p256params = { 
	/* p */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 
	0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF } },
	/* a */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFC, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 
	0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF } },
	/* b */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0, 
	0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8 } },
	/* gx */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81, 
	0x63A440F2, 0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2 } },
	/* gy */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357, 
	0x7C0F9E16, 0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2 } },
	/* n */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD, 
	0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0xF3897EFA, 0xA860F782, 0x3994862C, 0x1E8D2321, 
	0x15B99322, 0x4943FE0A, 0x75E7E997 
	};

/* Brainpool p256r1, CRYPT_ECCCURVE_BRAINPOOL_P256 */

static const ECC_DOMAINPARAMS brainpool256params = { 
	/* p */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x1F6E5377, 0x2013481D, 0xD5262028, 0x6E3BF623, 
	0x9D838D72, 0x3E660A90, 0xA1EEA9BC, 0xA9FB57DB } },
	/* a */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xF330B5D9, 0xE94A4B44, 0x26DC5C6C, 0xFB8055C1, 
	0x417AFFE7, 0xEEF67530, 0xFC2C3057, 0x7D5A0975 } },
	/* b */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0xFF8C07B6, 0x6BCCDC18, 0x5CF7E1CE, 0x95841629, 
	0xBBD77CBF, 0xF330B5D9, 0xE94A4B44, 0x26DC5C6C } },
	/* gx */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x9ACE3262, 0x3A4453BD, 0xE3BD23C2, 0xB9DE27E1, 
	0xFC81B7AF, 0x2C4B482F, 0xCB7E57CB, 0x8BD2AEB9 } },
	/* gy */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x2F046997, 0x5C1D54C7, 0x2DED8E54, 0xC2774513, 
	0x14611DC9, 0x97F8461A, 0xC3DAC4FD, 0x547EF835 } },
	/* n */
	{ 8, FALSE, BN_FLG_STATIC_DATA, {
	0x974856A7, 0x901E0E82, 0xB561A6F7, 0x8C397AA3, 
	0x9D838D71, 0x3E660A90, 0xA1EEA9BC, 0xA9FB57DB } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0x3498DCF4, 0x3BCBEBC2, 0x69495E03, 0x2AD8069D, 
	0x11DAF3CF, 0xCA928D15, 0x75E7E997
	};

/* NIST P-384, SECG p384r1, CRYPT_ECCCURVE_P384 */

static const ECC_DOMAINPARAMS p384params = { 
	/* p */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 
	0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* a */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFC, 0x00000000, 0x00000000, 0xFFFFFFFF, 
	0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* b */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xD3EC2AEF, 0x2A85C8ED, 0x8A2ED19D, 0xC656398D, 
	0x5013875A, 0x0314088F, 0xFE814112, 0x181D9C6E, 
	0xE3F82D19, 0x988E056B, 0xE23EE7E4, 0xB3312FA7 } },
	/* gx */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x72760AB7, 0x3A545E38, 0xBF55296C, 0x5502F25D, 
	0x82542A38, 0x59F741E0, 0x8BA79B98, 0x6E1D3B62, 
	0xF320AD74, 0x8EB1C71E, 0xBE8B0537, 0xAA87CA22 } },
	/* gy */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x90EA0E5F, 0x7A431D7C, 0x1D7E819D, 0x0A60B1CE, 
	0xB5F0B8C0, 0xE9DA3113, 0x289A147C, 0xF8F41DBD, 
	0x9292DC29, 0x5D9E98BF, 0x96262C6F, 0x3617DE4A } },
	/* n */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xCCC52973, 0xECEC196A, 0x48B0A77A, 0x581A0DB2, 
	0xF4372DDF, 0xC7634D81, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0x7D715AAC, 0x1FC7CA5A, 0x114D671D, 0x9173FBBF, 
	0x26E973C0, 0x8D05F26F, 0x75E7E997 
	};

/* Brainpool p384r1, CRYPT_ECCCURVE_BRAINPOOL_P384 */

static const ECC_DOMAINPARAMS brainpool384params = { 
	/* p */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x3107EC53, 0x87470013, 0x901D1A71, 0xACD3A729, 
	0x7FB71123, 0x12B1DA19, 0xED5456B4, 0x152F7109, 
	0x50E641DF, 0x0F5D6F7E, 0xA3386D28, 0x8CB91E82 } },
	/* a */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x22CE2826, 0x04A8C7DD, 0x503AD4EB, 0x8AA5814A, 
	0xBA91F90F, 0x139165EF, 0x4FB22787, 0xC2BEA28E, 
	0xCE05AFA0, 0x3C72080A, 0x3D8C150C, 0x7BC382C6 } },
	/* b */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xFA504C11, 0x3AB78696, 0x95DBC994, 0x7CB43902, 
	0x3EEB62D5, 0x2E880EA5, 0x07DCD2A6, 0x2FB77DE1, 
	0x16F0447C, 0x8B39B554, 0x22CE2826, 0x04A8C7DD } },
	/* gx */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x47D4AF1E, 0xEF87B2E2, 0x36D646AA, 0xE826E034, 
	0x0CBD10E8, 0xDB7FCAFE, 0x7EF14FE3, 0x8847A3E7, 
	0xB7C13F6B, 0xA2A63A81, 0x68CF45FF, 0x1D1C64F0 } },
	/* gy */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0x263C5315, 0x42820341, 0x77918111, 0x0E464621, 
	0xF9912928, 0xE19C054F, 0xFEEC5864, 0x62B70B29, 
	0x95CFD552, 0x5CB1EB8E, 0x20F9C2A4, 0x8ABE1D75 } },
	/* n */
	{ 12, FALSE, BN_FLG_STATIC_DATA, {
	0xE9046565, 0x3B883202, 0x6B7FC310, 0xCF3AB6AF, 
	0xAC0425A7, 0x1F166E6C, 0xED5456B3, 0x152F7109, 
	0x50E641DF, 0x0F5D6F7E, 0xA3386D28, 0x8CB91E82 } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0xA104D6F2, 0x0154CB82, 0xF8B07960, 0xACC086CC, 
	0x7F4924D6, 0x0B03676F, 0x75E7E997 
	};

/* NIST P-521, SECG p521r1, CRYPT_ECCCURVE_P521 */

static const ECC_DOMAINPARAMS p521params = { 
	/* p */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0x000001FF } },
	/* a */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0xFFFFFFFC, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0x000001FF } },
	/* b */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0x6B503F00, 0xEF451FD4, 0x3D2C34F1, 0x3573DF88, 
	0x3BB1BF07, 0x1652C0BD, 0xEC7E937B, 0x56193951, 
	0x8EF109E1, 0xB8B48991, 0x99B315F3, 0xA2DA725B, 
	0xB68540EE, 0x929A21A0, 0x8E1C9A1F, 0x953EB961, 
	0x00000051 } },
	/* gx */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0xC2E5BD66, 0xF97E7E31, 0x856A429B, 0x3348B3C1, 
	0xA2FFA8DE, 0xFE1DC127, 0xEFE75928, 0xA14B5E77, 
	0x6B4D3DBA, 0xF828AF60, 0x053FB521, 0x9C648139, 
	0x2395B442, 0x9E3ECB66, 0x0404E9CD, 0x858E06B7, 
	0x000000C6 } },
	/* gy */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0x9FD16650, 0x88BE9476, 0xA272C240, 0x353C7086, 
	0x3FAD0761, 0xC550B901, 0x5EF42640, 0x97EE7299, 
	0x273E662C, 0x17AFBD17, 0x579B4468, 0x98F54449, 
	0x2C7D1BD9, 0x5C8A5FB4, 0x9A3BC004, 0x39296A78, 
	0x00000118 } },
	/* n */
	{ 17, FALSE, BN_FLG_STATIC_DATA, {
	0x91386409, 0xBB6FB71E, 0x899C47AE, 0x3BB5C9B8, 
	0xF709A5D0, 0x7FCC0148, 0xBF2F966B, 0x51868783, 
	0xFFFFFFFA, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 
	0x000001FF } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0xF0CB48FC, 0xC89C4882, 0xB1A12B3C, 0x22B097CC, 
	0xEE2FFB2E, 0xA2E7127F, 0x75E7E997 
	};

/* Brainpool p512r1, CRYPT_ECCCURVE_BRAINPOOL_P512 */

static const ECC_DOMAINPARAMS brainpool512params = { 
	/* p */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0x583A48F3, 0x28AA6056, 0x2D82C685, 0x2881FF2F, 
	0xE6A380E6, 0xAECDA12A, 0x9BC66842, 0x7D4D9B00, 
	0x70330871, 0xD6639CCA, 0xB3C9D20E, 0xCB308DB3, 
	0x33C9FC07, 0x3FD4E6AE, 0xDBE9C48B, 0xAADD9DB8 } },
	/* a */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0x77FC94CA, 0xE7C1AC4D, 0x2BF2C7B9, 0x7F1117A7, 
	0x8B9AC8B5, 0x0A2EF1C9, 0xA8253AA1, 0x2DED5D5A, 
	0xEA9863BC, 0xA83441CA, 0x3DF91610, 0x94CBDD8D, 
	0xAC234CC5, 0xE2327145, 0x8B603B89, 0x7830A331 } },
	/* b */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0x8016F723, 0x2809BD63, 0x5EBAE5DD, 0x984050B7, 
	0xDC083E67, 0x77FC94CA, 0xE7C1AC4D, 0x2BF2C7B9, 
	0x7F1117A7, 0x8B9AC8B5, 0x0A2EF1C9, 0xA8253AA1, 
	0x2DED5D5A, 0xEA9863BC, 0xA83441CA, 0x3DF91610 } },
	/* gx */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0xBCB9F822, 0x8B352209, 0x406A5E68, 0x7C6D5047, 
	0x93B97D5F, 0x50D1687B, 0xE2D0D48D, 0xFF3B1F78, 
	0xF4D0098E, 0xB43B62EE, 0xB5D916C1, 0x85ED9F70, 
	0x9C4C6A93, 0x5A21322E, 0xD82ED964, 0x81AEE4BD } },
	/* gy */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0x3AD80892, 0x78CD1E0F, 0xA8F05406, 0xD1CA2B2F, 
	0x8A2763AE, 0x5BCA4BD8, 0x4A5F485E, 0xB2DCDE49, 
	0x881F8111, 0xA000C55B, 0x24A57B1A, 0xF209F700, 
	0xCF7822FD, 0xC0EABFA9, 0x566332EC, 0x7DDE385D } },
	/* n */
	{ 16, FALSE, BN_FLG_STATIC_DATA, {
	0x9CA90069, 0xB5879682, 0x085DDADD, 0x1DB1D381, 
	0x7FAC1047, 0x41866119, 0x4CA92619, 0x553E5C41, 
	0x70330870, 0xD6639CCA, 0xB3C9D20E, 0xCB308DB3, 
	0x33C9FC07, 0x3FD4E6AE, 0xDBE9C48B, 0xAADD9DB8 } },
	/* h */
	{ 1, FALSE, BN_FLG_STATIC_DATA, {
	0x00000001 } },
	/* Checksums */
	0x8A2C6186, 0x8FBBE214, 0x60D7593D, 0x02F580D8, 
	0xD6823856, 0xBD31D0D9, 0x75E7E997 
	};
#endif /* 64- vs 32-bit */

/****************************************************************************
*																			*
*								ECC Access Functions						*
*																			*
****************************************************************************/

/* Initialise the bignums for the domain parameter values { p, a, b, gx, gy, 
   n }.  Note that although the cofactor h is given for the standard named 
   curves it's not used for any ECC operations, so we could quite easily 
   omit it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int loadECCparams( INOUT CONTEXT_INFO *contextInfoPtr,
				   IN_ENUM( CRYPT_ECCCURVE ) \
						const CRYPT_ECCCURVE_TYPE curveType )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const ECC_DOMAINPARAMS *domainParams;
	int curveSizeBits, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isEnumRange( curveType, CRYPT_ECCCURVE ) );

	/* Convert the fixed-format bignums to encoded BIGNUMs */
#ifdef CREATE_BIGNUM_VALUES
	status = loadECCparamsFixed( contextInfoPtr, CRYPT_ECCCURVE_P256 );
	if( cryptStatusError( status ) )
		return( status );
#endif /* CREATE_BIGNUM_VALUES */

	/* Get the requested domain parameters */
	switch( curveType )
		{
		case CRYPT_ECCCURVE_P256:
			domainParams = &p256params;
			break;

		case CRYPT_ECCCURVE_BRAINPOOL_P256:
			domainParams = &brainpool256params;
			break;

		case CRYPT_ECCCURVE_P384:
			domainParams = &p384params;
			break;

		case CRYPT_ECCCURVE_BRAINPOOL_P384:
			domainParams = &brainpool384params;
			break;

		case CRYPT_ECCCURVE_P521:
			domainParams = &p521params;
			break;

		case CRYPT_ECCCURVE_BRAINPOOL_P512:
			domainParams = &brainpool512params;
			break;

		default:
			retIntError();
		}

	/* For the named curve the key size is defined by exective fiat based
	   on the curve type rather than being taken from the public-key value 
	   (which in this case is the magnitude of the point Q on the curve), so 
	   we set it explicitly based on the curve type */
	status = getECCFieldSize( curveType, &curveSizeBits, TRUE );
	if( cryptStatusError( status ) )
		return( status );
	pkcInfo->curveType = curveType;
	pkcInfo->keySizeBits = curveSizeBits;

	/* Make sure that the domain parameters are in order */
	if( !checksumDomainParameters( domainParams, TRUE ) )
		{
		DEBUG_DIAG(( "ECC domain parameters for curve ID %d key has been "
					 "corrupted", curveType ));
		retIntError();
		}

	pkcInfo->domainParams = domainParams;

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */
