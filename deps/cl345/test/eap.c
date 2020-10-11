/****************************************************************************
*																			*
*							cryptlib EAP Test Code							*
*						Copyright Peter Gutmann 2014-2018					*
*																			*
****************************************************************************/

#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* EBCDIC systems */

#if defined( TEST_SESSION ) 

/****************************************************************************
*																			*
*					Obsolete Crypto Algorithms for MS-CHAP					*
*																			*
****************************************************************************/

/* Public-domain MD4 implementation, see comment below */

#define MD4_BLOCK_LENGTH			64
#define MD4_DIGEST_LENGTH			16

typedef struct {
	unsigned long state[ 4 ];		/* State */
	long count;						/* Number of bits */
	BYTE buffer[ MD4_BLOCK_LENGTH ];/* input buffer */
	} MD4_CTX;

/* ===== start - public domain MD4 implementation ===== */
/*      $OpenBSD: md4.c,v 1.7 2005/08/08 08:05:35 espie Exp $   */

/*
 * This code implements the MD4 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 * Todd C. Miller modified the MD5 code to do MD4 based on RFC 1186.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD4Context structure, pass it to MD4Init, call MD4Update as
 * needed on buffers full of bytes, and then call MD4Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#define MD4_DIGEST_STRING_LENGTH        (MD4_DIGEST_LENGTH * 2 + 1)

static void
MD4Transform(unsigned long state[4], const BYTE block[MD4_BLOCK_LENGTH]);

#define PUT_64BIT_LE(cp, value) do {                \
        (cp)[7] = 0;                                \
        (cp)[6] = 0;                                \
        (cp)[5] = 0;                                \
        (cp)[4] = 0;                                \
        (cp)[3] = (BYTE)((value) >> 24);            \
        (cp)[2] = (BYTE)((value) >> 16);            \
        (cp)[1] = (BYTE)((value) >> 8);             \
        (cp)[0] = (BYTE)(value); } while (0)

#define PUT_32BIT_LE(cp, value) do {                \
        (cp)[3] = (BYTE)((value) >> 24);            \
        (cp)[2] = (BYTE)((value) >> 16);            \
        (cp)[1] = (BYTE)((value) >> 8);             \
        (cp)[0] = (BYTE)(value); } while (0)

static BYTE PADDING[MD4_BLOCK_LENGTH] = {
        0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/*
 * Start MD4 accumulation.
 * Set bit count to 0 and buffer to mysterious initialization constants.
 */
static void MD4Init(MD4_CTX *ctx)
{
        ctx->count = 0;
        ctx->state[0] = 0x67452301;
        ctx->state[1] = 0xefcdab89;
        ctx->state[2] = 0x98badcfe;
        ctx->state[3] = 0x10325476;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD4Update(MD4_CTX *ctx, const unsigned char *input, size_t len)
{
        size_t have, need;

        /* Check how many bytes we already have and how many more we need. */
        have = (size_t)((ctx->count >> 3) & (MD4_BLOCK_LENGTH - 1));
        need = MD4_BLOCK_LENGTH - have;

        /* Update bitcount */
        ctx->count += (unsigned long)len << 3;

        if (len >= need) {
                if (have != 0) {
                        memcpy(ctx->buffer + have, input, need);
                        MD4Transform(ctx->state, ctx->buffer);
                        input += need;
                        len -= need;
                        have = 0;
                }

                /* Process data in MD4_BLOCK_LENGTH-byte chunks. */
                while (len >= MD4_BLOCK_LENGTH) {
                        MD4Transform(ctx->state, input);
                        input += MD4_BLOCK_LENGTH;
                        len -= MD4_BLOCK_LENGTH;
                }
        }

        /* Handle any remaining bytes of data. */
        if (len != 0)
                memcpy(ctx->buffer + have, input, len);
}

/*
 * Pad pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void MD4Pad(MD4_CTX *ctx)
{
        BYTE count[8];
        size_t padlen;

        /* Convert count to 8 bytes in little endian order. */
        PUT_64BIT_LE(count, ctx->count);

        /* Pad out to 56 mod 64. */
        padlen = MD4_BLOCK_LENGTH -
            ((ctx->count >> 3) & (MD4_BLOCK_LENGTH - 1));
        if (padlen < 1 + 8)
                padlen += MD4_BLOCK_LENGTH;
        MD4Update(ctx, PADDING, padlen - 8);            /* padlen - 8 <= 64 */
        MD4Update(ctx, count, 8);
}

/*
 * Final wrapup--call MD4Pad, fill in digest and zero out ctx.
 */
static void MD4Final(unsigned char digest[MD4_DIGEST_LENGTH], MD4_CTX *ctx)
{
        int i;

        MD4Pad(ctx);
        if (digest != NULL) {
                for (i = 0; i < 4; i++)
                        PUT_32BIT_LE(digest + i * 4, ctx->state[i]);
                memset(ctx, 0, sizeof(*ctx));
        }
}

/* The three core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) ((x & y) | (x & z) | (y & z))
#define F3(x, y, z) (x ^ y ^ z)

/* This is the central step in the MD4 algorithm. */
#define MD4STEP(f, w, x, y, z, data, s) \
        ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s) )

/*
 * The core of the MD4 algorithm, this alters an existing MD4 hash to
 * reflect the addition of 16 longwords of new data.  MD4Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
MD4Transform(unsigned long state[4], const BYTE block[MD4_BLOCK_LENGTH])
{
        unsigned long a, b, c, d, in[MD4_BLOCK_LENGTH / 4];

        for (a = 0; a < MD4_BLOCK_LENGTH / 4; a++) {
                in[a] = (unsigned long)(
                    (unsigned long)(block[a * 4 + 0]) |
                    (unsigned long)(block[a * 4 + 1]) <<  8 |
                    (unsigned long)(block[a * 4 + 2]) << 16 |
                    (unsigned long)(block[a * 4 + 3]) << 24);
        }

        a = state[0];
        b = state[1];
        c = state[2];
        d = state[3];

        MD4STEP(F1, a, b, c, d, in[ 0],  3);
        MD4STEP(F1, d, a, b, c, in[ 1],  7);
        MD4STEP(F1, c, d, a, b, in[ 2], 11);
        MD4STEP(F1, b, c, d, a, in[ 3], 19);
        MD4STEP(F1, a, b, c, d, in[ 4],  3);
        MD4STEP(F1, d, a, b, c, in[ 5],  7);
        MD4STEP(F1, c, d, a, b, in[ 6], 11);
        MD4STEP(F1, b, c, d, a, in[ 7], 19);
        MD4STEP(F1, a, b, c, d, in[ 8],  3);
        MD4STEP(F1, d, a, b, c, in[ 9],  7);
        MD4STEP(F1, c, d, a, b, in[10], 11);
        MD4STEP(F1, b, c, d, a, in[11], 19);
        MD4STEP(F1, a, b, c, d, in[12],  3);
        MD4STEP(F1, d, a, b, c, in[13],  7);
        MD4STEP(F1, c, d, a, b, in[14], 11);
        MD4STEP(F1, b, c, d, a, in[15], 19);

        MD4STEP(F2, a, b, c, d, in[ 0] + 0x5a827999,  3);
        MD4STEP(F2, d, a, b, c, in[ 4] + 0x5a827999,  5);
        MD4STEP(F2, c, d, a, b, in[ 8] + 0x5a827999,  9);
        MD4STEP(F2, b, c, d, a, in[12] + 0x5a827999, 13);
        MD4STEP(F2, a, b, c, d, in[ 1] + 0x5a827999,  3);
        MD4STEP(F2, d, a, b, c, in[ 5] + 0x5a827999,  5);
        MD4STEP(F2, c, d, a, b, in[ 9] + 0x5a827999,  9);
        MD4STEP(F2, b, c, d, a, in[13] + 0x5a827999, 13);
        MD4STEP(F2, a, b, c, d, in[ 2] + 0x5a827999,  3);
        MD4STEP(F2, d, a, b, c, in[ 6] + 0x5a827999,  5);
        MD4STEP(F2, c, d, a, b, in[10] + 0x5a827999,  9);
        MD4STEP(F2, b, c, d, a, in[14] + 0x5a827999, 13);
        MD4STEP(F2, a, b, c, d, in[ 3] + 0x5a827999,  3);
        MD4STEP(F2, d, a, b, c, in[ 7] + 0x5a827999,  5);
        MD4STEP(F2, c, d, a, b, in[11] + 0x5a827999,  9);
        MD4STEP(F2, b, c, d, a, in[15] + 0x5a827999, 13);

        MD4STEP(F3, a, b, c, d, in[ 0] + 0x6ed9eba1,  3);
        MD4STEP(F3, d, a, b, c, in[ 8] + 0x6ed9eba1,  9);
        MD4STEP(F3, c, d, a, b, in[ 4] + 0x6ed9eba1, 11);
        MD4STEP(F3, b, c, d, a, in[12] + 0x6ed9eba1, 15);
        MD4STEP(F3, a, b, c, d, in[ 2] + 0x6ed9eba1,  3);
        MD4STEP(F3, d, a, b, c, in[10] + 0x6ed9eba1,  9);
        MD4STEP(F3, c, d, a, b, in[ 6] + 0x6ed9eba1, 11);
        MD4STEP(F3, b, c, d, a, in[14] + 0x6ed9eba1, 15);
        MD4STEP(F3, a, b, c, d, in[ 1] + 0x6ed9eba1,  3);
        MD4STEP(F3, d, a, b, c, in[ 9] + 0x6ed9eba1,  9);
        MD4STEP(F3, c, d, a, b, in[ 5] + 0x6ed9eba1, 11);
        MD4STEP(F3, b, c, d, a, in[13] + 0x6ed9eba1, 15);
        MD4STEP(F3, a, b, c, d, in[ 3] + 0x6ed9eba1,  3);
        MD4STEP(F3, d, a, b, c, in[11] + 0x6ed9eba1,  9);
        MD4STEP(F3, c, d, a, b, in[ 7] + 0x6ed9eba1, 11);
        MD4STEP(F3, b, c, d, a, in[15] + 0x6ed9eba1, 15);

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
}
/* ===== end - public domain MD4 implementation ===== */

static void md4Hash( const void *data, const int dataLength, BYTE *hashValue )
	{
	MD4_CTX md4CTX;

	MD4Init( &md4CTX );
	MD4Update( &md4CTX, data, dataLength );
	MD4Final( hashValue, &md4CTX );
	}

/****************************************************************************
*																			*
*									MS-CHAPv2								*
*																			*
****************************************************************************/

/* MSCHAPv2 functionality as specified in RFC 2759.  This is a great
   teaching example of every mistake you can make in challenge-response 
   authentication, it gets pretty much everything wrong from start to 
   finish, but we have to implement it because so much stuff uses it */

/* ChallengeHash(), RFC 2759 page 8 */

static int ChallengeHash( const BYTE PeerChallenge[ 16 ], 
						  const BYTE AuthenticatorChallenge[ 16 ],
						  const BYTE *UserName, const int UserNameLength,
						  BYTE Challenge[ 8 ] )
	{
	CRYPT_CONTEXT hashContext;
	BYTE hashValue[ CRYPT_MAX_HASHSIZE ];
	int hashSize, status;

	status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_SHA1 );
	if( cryptStatusError( status ) )
		return( status );
	cryptEncrypt( hashContext, ( void * ) PeerChallenge, 16 );
	cryptEncrypt( hashContext, ( void * ) AuthenticatorChallenge, 16 );
	cryptEncrypt( hashContext, ( void * ) UserName, UserNameLength );
	status = cryptEncrypt( hashContext, "", 0 );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttributeString( hashContext, 
										  CRYPT_CTXINFO_HASHVALUE,
										  hashValue, &hashSize );
		}
	cryptDestroyContext( hashContext );
	if( cryptStatusError( status ) )
		return( status );
	memcpy( Challenge, hashValue, 8 );
	memset( hashValue, 0, CRYPT_MAX_HASHSIZE );

	return( CRYPT_OK );
	}

/* NtPasswordHash(), RFC 2759 page 9 */

static int NtPasswordHash( const BYTE *Password, const int PasswordLength,
						   BYTE PasswordHash[ 16 ] )
	{
	md4Hash( Password, PasswordLength, PasswordHash );

	return( CRYPT_OK );
	}

/* ChallengeResponse(), RFC 2759 page 9.
   DesEncrypt() RFC 2759 page 10 */

static int DesEncrypt( const BYTE Clear[ 8 ], 
					   const BYTE Key[ 7 ], BYTE Cypher[ 8 ] )
	{
	CRYPT_CONTEXT cryptContext;
	BYTE desKey[ 8 ];
	int i, status;

	/* Convert the 56-bit Key value into the eight 7-bit key data bytes 
	   required by DES.  This involves first expanding the 56 input bits
	   into 64 7-bit bytes, and the shifting each byte up by one since the
	   parity bits are in the LSB, not the MSB */
	desKey[ 0 ] = Key[0] >> 0x01;
	desKey[ 1 ] = ( ( Key[ 0 ] & 0x01 ) << 6 ) | ( Key[ 1 ] >> 2 );
	desKey[ 2 ] = ( ( Key[ 1 ] & 0x03 ) << 5 ) | ( Key[ 2 ] >> 3 );
 	desKey[ 3 ] = ( ( Key[ 2 ] & 0x07 ) << 4 ) | ( Key[ 3 ] >> 4 );
	desKey[ 4 ] = ( ( Key[ 3 ] & 0x0F ) << 3 ) | ( Key[ 4 ] >> 5 );
	desKey[ 5 ] = ( ( Key[ 4 ] & 0x1F ) << 2 ) | ( Key[ 5 ] >> 6 );
	desKey[ 6 ] = ( ( Key[ 5 ] & 0x3F ) << 1 ) | ( Key[ 6 ] >> 7 );
	desKey[ 7 ] = Key[ 6 ] & 0x7F;
	for( i = 0; i < 8; i++ )
		desKey[ i ] = ( desKey[ i ] << 1 ) & 0xFE;

	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_DES );
	if( cryptStatusError( status ) )
		return( status );
	cryptSetAttribute( cryptContext, CRYPT_CTXINFO_MODE, CRYPT_MODE_ECB );
	status = cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY, 
									  desKey, 8 );
	if( cryptStatusOK( status ) )
		{
		memcpy( Cypher, Clear, 8 );
		status = cryptEncrypt( cryptContext, Cypher, 8 );
		}
	cryptDestroyContext( cryptContext );
	memset( desKey, 0, 8 );

	return( status );
	}

static int ChallengResponse( const BYTE Challenge[ 8 ], 
							 const BYTE PasswordHash[ 16 ],
							 BYTE Response[ 24 ] )
	{
	BYTE ZPasswordHash[ 21 ];
	int status;

	memset( ZPasswordHash, 0, 21 );
	memcpy( ZPasswordHash, PasswordHash, 16 );

	status = DesEncrypt( Challenge, ZPasswordHash, Response );
	if( cryptStatusOK( status ) )
		status = DesEncrypt( Challenge, ZPasswordHash + 7, Response + 8 );
	if( cryptStatusOK( status ) )
		status = DesEncrypt( Challenge, ZPasswordHash + 14, Response + 16 );
	memset( ZPasswordHash, 0, 21 );

	return( status );
	}

/* GenerateNTResponse, RFC 2759 p.7 */

static int GenerateNTResponse( const BYTE AuthenticatorChallenge[ 16 ],
							   const BYTE PeerChallenge[ 16 ],
							   const BYTE *UserName, const int UserNameLength,
							   const BYTE *Password, const int PasswordLength,
							   BYTE Response[ 24 ] )
	{
	BYTE Challenge[ 8 ], PasswordHash[ 16 ];
	int status;

	status = ChallengeHash( PeerChallenge, AuthenticatorChallenge,
							UserName, UserNameLength, Challenge );
	if( cryptStatusOK( status ) )
		status = NtPasswordHash( Password, PasswordLength, PasswordHash );
	if( cryptStatusOK( status ) )
		status = ChallengResponse( Challenge, PasswordHash, Response );
	memset( Challenge, 0, 8 );
	memset( PasswordHash, 0, 16 );

	return( status );
	}

/****************************************************************************
*																			*
*								Interface Routines							*
*																			*
****************************************************************************/

/* Test the implementation using the test vectors from RFC 2759:

	0-to-256-char UserName:
	55 73 65 72			// "User"

	0-to-256-unicode-char Password:
	63 00 6C 00 69 00 65 00 6E 00 74 00 50 00 61 00 73 00 73 00

	16-octet AuthenticatorChallenge:
	5B 5D 7C 7D 7B 3F 2F 3E 3C 2C 60 21 32 26 26 28

	16-octet PeerChallenge:
	21 40 23 24 25 5E 26 2A 28 29 5F 2B 3A 33 7C 7E

	8-octet Challenge:
	D0 2E 43 86 BC E9 12 26

	16-octet PasswordHash:
	44 EB BA 8D 53 12 B8 D6 11 47 44 11 F5 69 89 AE

	24 octet NT-Response:
	82 30 9E CD 8D 70 8B 5E A0 8F AA 39 81 CD 83 54 42 33 11 4A 3D 85 D6 DF

	16-octet PasswordHashHash:
	41 C0 0C 58 4B D2 D9 1C 40 17 A2 A1 2F A5 9F 3F

	42-octet AuthenticatorResponse:
	"S=407A5589115FD0D6209F510FE9C04566932CDA56" */

int testMSCHAPv2( void )
	{
	const BYTE UserName[] = "User";
	const BYTE Password[] = "\x63\x00\x6C\x00\x69\x00\x65\x00" \
							"\x6E\x00\x74\x00\x50\x00\x61\x00" \
							"\x73\x00\x73\x00";
	const BYTE AuthenticatorChallenge[] = \
							"\x5B\x5D\x7C\x7D\x7B\x3F\x2F\x3E" \
							"\x3C\x2C\x60\x21\x32\x26\x26\x28";
	const BYTE PeerChallenge[] = \
							"\x21\x40\x23\x24\x25\x5E\x26\x2A" \
							"\x28\x29\x5F\x2B\x3A\x33\x7C\x7E";
	BYTE Challenge[ 8 ], PasswordHash[ 16 ], Response[ 24 ];
	int status;

	/* Run through each step verifying that the result is correct */
	status = ChallengeHash( PeerChallenge, AuthenticatorChallenge,
							UserName, 4, Challenge );
	if( cryptStatusError( status ) || \
		memcmp( Challenge, "\xD0\x2E\x43\x86\xBC\xE9\x12\x26", 8 ) )
		return( -1 );
	status = NtPasswordHash( Password, 20, PasswordHash );
	if( cryptStatusError( status ) || \
		memcmp( PasswordHash, "\x44\xEB\xBA\x8D\x53\x12\xB8\xD6" \
							  "\x11\x47\x44\x11\xF5\x69\x89\xAE", 16 ) )
		return( -1 );
	status = ChallengResponse( Challenge, PasswordHash, Response );
	if( cryptStatusError( status ) || \
		memcmp( Response, "\x82\x30\x9E\xCD\x8D\x70\x8B\x5E" \
						  "\xA0\x8F\xAA\x39\x81\xCD\x83\x54" \
						  "\x42\x33\x11\x4A\x3D\x85\xD6\xDF", 24 ) )
		return( -1 );

	/* Now verify that the overall function is correct */
	status = GenerateNTResponse( AuthenticatorChallenge, PeerChallenge, 
								 UserName, 4, Password, 20, Response );
	if( cryptStatusError( status ) || \
		memcmp( Response, "\x82\x30\x9E\xCD\x8D\x70\x8B\x5E" \
						  "\xA0\x8F\xAA\x39\x81\xCD\x83\x54" \
						  "\x42\x33\x11\x4A\x3D\x85\xD6\xDF", 24 ) )
		return( -1 );

	return( CRYPT_OK );
	}

/* Create a TTLS AVP encoding of the MSCHAPv2 data:

	uint32	type = 1				// User-Name
	byte	flags = 0x40			// Mandatory attribute
	uint24	length
		byte[]	data				// User name
	
	uint32	type = 11 / 0x0B		// MS-CHAP-Challenge
	byte	flags = 0xC0			// Vendor-ID present + 0x40 as above
	uint24	length = 28 / 0x1C		// 12 + payload len
	uint32	vendor = 0x137			// Microsoft
		byte[16] vendorData			// MS-CHAP challenge data

	uint32	type = 25 / 0x19		// MS-CHAP2-Response
	byte	flags = 0xC0			// Vendor-ID present + 0x40 as above
	uint24	length = 0x3E			// 12 + payload len
	uint32	vendor = 0x137			// Microsoft
		byte	vendorIdent
		byte	vendorFlags = 0
		byte[16] vendorPeerChallenge // MS-CHAP challenge data
		byte[8]	vendorReserved = { 0 }
		byte[24] vendorResponse		// MS-CHAP response data */

int createTTLSAVPMSCHAPv2( BYTE *ttlsAVP, int *ttlsAVPlength,
						   const void *userName, const int userNameLength,
						   const void *password, const int passwordLength,
						   const void *eapChallenge )
	{
	static const BYTE radiusUserName[] = \
				"\x00\x00\x00\x01\x40\x00\x00\xFF";		/* RADIUS UserName */
	static const BYTE radiusMSCHAPChallenge[] = \
				"\x00\x00\x00\x0B\xC0\x00\x00\x1C\x00\x00\x01\x37"
		/* 12 */	"****************";					/* RADIUS MS-CHAP-Challenge */
	static const BYTE radiusMSCHAPResponse[] = \
				"\x00\x00\x00\x19\xC0\x00\x00\x3E\x00\x00\x01\x37"
		/* 12 */	"\xFF\x00"							/* RADIUS MS-CHAP-Response */
		/* 14 */	"****************"						/* Peer-Challange */
		/* 30 */	"\x00\x00\x00\x00\x00\x00\x00\x00"		/* RFU */
		/* 38 */	"************************";				/* Response */
	const int radiusUserNameLength = sizeof( radiusUserName ) - 1;
	const int radiusMSCHAPChallengeLength = sizeof( radiusMSCHAPChallenge ) - 1;
	const int radiusMSCHAPResponseLength = sizeof( radiusMSCHAPResponse ) - 1;
	const BYTE *eapChallengePtr = eapChallenge, *passwordPtr = password;
	BYTE unicodePassword[ 256 ], *ttlsAVPptr = ttlsAVP;
	const int unicodePasswordLength = passwordLength * 2;
	int i;

	*ttlsAVPlength = radiusUserNameLength + userNameLength + \
					 radiusMSCHAPChallengeLength + radiusMSCHAPResponseLength;

	/* Convert the password to Windows-format (little-endian) Unicode */
	if( passwordLength > 128 )
		return( CRYPT_ERROR_OVERFLOW );
	for( i = 0; i < unicodePasswordLength; i += 2 )
		{
		unicodePassword[ i ] = *passwordPtr++;
		unicodePassword[ i + 1 ] = '\0';
		}

	/* Set up the RADIUS UserName attribute */
	memcpy( ttlsAVP, radiusUserName, radiusUserNameLength );
	ttlsAVP[ radiusUserNameLength - 1 ] = radiusUserNameLength + userNameLength;
	memcpy( ttlsAVP + radiusUserNameLength, userName, userNameLength );
	ttlsAVPptr += radiusUserNameLength + userNameLength;

	/* Set up the RADIUS MS-CHAP-Challenge attribute */
	memcpy( ttlsAVPptr, radiusMSCHAPChallenge, radiusMSCHAPChallengeLength );
	memcpy( ttlsAVPptr + 12, eapChallengePtr, 16 );
	ttlsAVPptr += radiusMSCHAPChallengeLength;

	/* Set up the RADIUS RADIUS MS-CHAP-Response */
	memcpy( ttlsAVPptr, radiusMSCHAPResponse, radiusMSCHAPResponseLength );
	ttlsAVPptr[ 12 ] = eapChallengePtr[ 16 ];				/* MS-CHAP ident */
	memcpy( ttlsAVPptr + 14, eapChallengePtr + 16, 16 );	/* MS-CHAP peer challenge */

	return( GenerateNTResponse( eapChallengePtr, eapChallengePtr + 16,
								userName, userNameLength,
								unicodePassword, unicodePasswordLength, 
								ttlsAVPptr + 38 ) );
	}

void blem( void )
	{
	testMSCHAPv2();
	}
#endif /* TEST_SESSION  */
