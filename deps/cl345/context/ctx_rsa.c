/****************************************************************************
*																			*
*						cryptlib RSA Encryption Routines					*
*						Copyright Peter Gutmann 1993-2014					*
*																			*
****************************************************************************/

/* I suppose if we all used pure RSA, the Illuminati would blackmail God into
   putting a trapdoor into the laws of mathematics.
														-- Lyle Seaman */

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Algorithm Self-test							*
*																			*
****************************************************************************/

/* Perform a pairwise consistency test on a public/private key pair */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN pairwiseConsistencyTest( CONTEXT_INFO *contextInfoPtr )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = getRSACapability();
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_B( sanityCheckContext( contextInfoPtr ) );

	/* Encrypt with the public key */
	memset( buffer, 0, CRYPT_MAX_PKCSIZE );
	memcpy( buffer + 1, "abcde", 5 );
	status = capabilityInfoPtr->encryptFunction( contextInfoPtr, buffer,
						 bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Decrypt with the private key */
	status = capabilityInfoPtr->decryptFunction( contextInfoPtr, buffer,
						 bitsToBytes( contextInfoPtr->ctxPKC->keySizeBits ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Make sure that we're recovered the original, including correct
	   handling of leading zeroes */
	return( !memcmp( buffer, "\x00" "abcde" "\x00\x00\x00\x00", 10 ) );
	}

#ifndef CONFIG_NO_SELFTEST

/* Test the RSA implementation using a sample key.  Because a lot of the
   high-level encryption routines don't exist yet, we cheat a bit and set
   up a dummy encryption context with just enough information for the
   following code to work */

typedef struct {
	const int nLen; const BYTE n[ 128 ];
	const int eLen; const BYTE e[ 3 ];
	const int dLen; const BYTE d[ 128 ];
	const int pLen; const BYTE p[ 64 ];
	const int qLen; const BYTE q[ 64 ];
	const int uLen; const BYTE u[ 64 ];
	const int e1Len; const BYTE e1[ 64 ];
	const int e2Len; const BYTE e2[ 64 ];
	} RSA_KEY;

static const RSA_KEY rsaTestKey = {
	/* n */
	128,
	{ 0x9C, 0x4D, 0x98, 0x18, 0x67, 0xF9, 0x45, 0xBC,
	  0xB6, 0x75, 0x53, 0x5D, 0x2C, 0xFA, 0x55, 0xE4,
	  0x51, 0x54, 0x9F, 0x0C, 0x16, 0xB1, 0xAF, 0x89,
	  0xF6, 0xF3, 0xE7, 0x78, 0xB1, 0x2B, 0x07, 0xFB,
	  0xDC, 0xDE, 0x64, 0x23, 0x34, 0x87, 0xDA, 0x0B,
	  0xE5, 0xB3, 0x17, 0x16, 0xA4, 0xE3, 0x7F, 0x23,
	  0xDF, 0x96, 0x16, 0x28, 0xA6, 0xD2, 0xF0, 0x0A,
	  0x59, 0xEE, 0x06, 0xB3, 0x76, 0x6C, 0x64, 0x19,
	  0xD9, 0x76, 0x41, 0x25, 0x66, 0xD1, 0x93, 0x51,
	  0x52, 0x06, 0x6B, 0x71, 0x50, 0x0E, 0xAB, 0x30,
	  0xA5, 0xC8, 0x41, 0xFC, 0x30, 0xBC, 0x32, 0xD7,
	  0x4B, 0x22, 0xF2, 0x45, 0x4C, 0x94, 0x68, 0xF1,
	  0x92, 0x8A, 0x4C, 0xF9, 0xD4, 0x5E, 0x87, 0x92,
	  0xA8, 0x54, 0x93, 0x92, 0x94, 0x48, 0xA4, 0xA3,
	  0xEE, 0x19, 0x7F, 0x6E, 0xD3, 0x14, 0xB1, 0x48,
	  0xCE, 0x93, 0xD1, 0xEA, 0x4C, 0xE1, 0x9D, 0xEF },

	/* e */
	3,
	{ 0x01, 0x00, 0x01 },

	/* d */
	128,
	{ 0x37, 0xE2, 0x66, 0x67, 0x13, 0x85, 0xC4, 0xB1,
	  0x5C, 0x6B, 0x46, 0x8B, 0x21, 0xF1, 0xBF, 0x94,
	  0x0A, 0xA0, 0x3E, 0xDD, 0x8B, 0x9F, 0xAC, 0x2B,
	  0x9F, 0xE8, 0x44, 0xF2, 0x9A, 0x25, 0xD0, 0x8C,
	  0xF4, 0xC3, 0x6E, 0xFA, 0x47, 0x65, 0xEB, 0x48,
	  0x25, 0xB0, 0x8A, 0xA8, 0xC5, 0xFB, 0xB1, 0x11,
	  0x9A, 0x77, 0x87, 0x24, 0xB1, 0xC0, 0xE9, 0xA2,
	  0x49, 0xD5, 0x19, 0x00, 0x41, 0x6F, 0x2F, 0xBA,
	  0x9F, 0x28, 0x47, 0xF9, 0xB8, 0xBA, 0xFF, 0xF4,
	  0x8B, 0x20, 0xC9, 0xC9, 0x39, 0xAB, 0x52, 0x0E,
	  0x8A, 0x5A, 0xAF, 0xB3, 0xA3, 0x93, 0x4D, 0xBB,
	  0xFE, 0x62, 0x9B, 0x02, 0xCC, 0xA7, 0xB4, 0xAE,
	  0x86, 0x65, 0x88, 0x19, 0xD7, 0x44, 0xA7, 0xE4,
	  0x18, 0xB6, 0xCE, 0x01, 0xCD, 0xDF, 0x36, 0x81,
	  0xD5, 0xE1, 0x62, 0xF8, 0xD0, 0x27, 0xF1, 0x86,
	  0xA8, 0x58, 0xA7, 0xEB, 0x39, 0x79, 0x56, 0x41 },

	/* p */
	64,
	{ 0xCF, 0xDA, 0xF9, 0x99, 0x6F, 0x05, 0x95, 0x84,
	  0x09, 0x90, 0xB3, 0xAB, 0x39, 0xB7, 0xDD, 0x1D,
	  0x7B, 0xFC, 0xFD, 0x10, 0x35, 0xA0, 0x18, 0x1D,
	  0x9A, 0x11, 0x30, 0x90, 0xD4, 0x3B, 0xF0, 0x5A,
	  0xC1, 0xA6, 0xF4, 0x53, 0xD0, 0x94, 0xA0, 0xED,
	  0xE0, 0xE4, 0xE0, 0x8E, 0x44, 0x18, 0x42, 0x42,
	  0xE1, 0x2C, 0x0D, 0xF7, 0x30, 0xE2, 0xB8, 0x09,
	  0x73, 0x50, 0x28, 0xF6, 0x55, 0x85, 0x57, 0x03 },

	/* q */
	64,
	{ 0xC0, 0x81, 0xC4, 0x82, 0x6E, 0xF6, 0x1C, 0x92,
	  0x83, 0xEC, 0x17, 0xFB, 0x30, 0x98, 0xED, 0x6E,
	  0x89, 0x92, 0xB2, 0xA1, 0x21, 0x0D, 0xC1, 0x95,
	  0x49, 0x99, 0xD3, 0x79, 0xD3, 0xBD, 0x94, 0x93,
	  0xB9, 0x28, 0x68, 0xFF, 0xDE, 0xEB, 0xE8, 0xD2,
	  0x0B, 0xED, 0x7C, 0x08, 0xD0, 0xD5, 0x59, 0xE3,
	  0xC1, 0x76, 0xEA, 0xC1, 0xCD, 0xB6, 0x8B, 0x39,
	  0x4E, 0x29, 0x59, 0x5F, 0xFA, 0xCE, 0x83, 0xA5 },

	/* u */
	64,
	{ 0x4B, 0x87, 0x97, 0x1F, 0x27, 0xED, 0xAA, 0xAF,
	  0x42, 0xF4, 0x57, 0x82, 0x3F, 0xEC, 0x80, 0xED,
	  0x1E, 0x91, 0xF8, 0xB4, 0x33, 0xDA, 0xEF, 0xC3,
	  0x03, 0x53, 0x0F, 0xCE, 0xB9, 0x5F, 0xE4, 0x29,
	  0xCC, 0xEE, 0x6A, 0x5E, 0x11, 0x0E, 0xFA, 0x66,
	  0x85, 0xDC, 0xFC, 0x48, 0x31, 0x0C, 0x00, 0x97,
	  0xC6, 0x0A, 0xF2, 0x34, 0x60, 0x6B, 0xF7, 0x68,
	  0x09, 0x4E, 0xCF, 0xB1, 0x9E, 0x33, 0x9A, 0x41 },

	/* exponent1 */
	64,
	{ 0x6B, 0x2A, 0x0D, 0xF8, 0x22, 0x7A, 0x71, 0x8C,
	  0xE2, 0xD5, 0x9D, 0x1C, 0x91, 0xA4, 0x8F, 0x37,
	  0x0D, 0x5E, 0xF1, 0x26, 0x73, 0x4F, 0x78, 0x3F,
	  0x82, 0xD8, 0x8B, 0xFE, 0x8F, 0xBD, 0xDB, 0x7D,
	  0x1F, 0x4C, 0xB1, 0xB9, 0xA8, 0xD7, 0x88, 0x65,
	  0x3C, 0xC7, 0x24, 0x53, 0x95, 0x1E, 0x20, 0xC3,
	  0x94, 0x8E, 0x7F, 0x20, 0xCC, 0x2E, 0x88, 0x0E,
	  0x2F, 0x4A, 0xCB, 0xE3, 0xBD, 0x52, 0x02, 0xFB },

	/* exponent2 */
	64,
	{ 0x10, 0x27, 0xD3, 0xD2, 0x0E, 0x75, 0xE1, 0x17,
	  0xFA, 0xB2, 0x49, 0xA0, 0xEF, 0x07, 0x26, 0x85,
	  0xEC, 0x4D, 0xBF, 0x67, 0xFE, 0x5A, 0x25, 0x30,
	  0xDE, 0x28, 0x66, 0xB3, 0x06, 0xAE, 0x16, 0x55,
	  0xFF, 0x68, 0x00, 0xC7, 0xD8, 0x71, 0x7B, 0xEC,
	  0x84, 0xCB, 0xBD, 0x69, 0x0F, 0xFD, 0x97, 0xB9,
	  0xA1, 0x76, 0xD5, 0x64, 0xC6, 0x5A, 0xD7, 0x7C,
	  0x4B, 0xAE, 0xF4, 0xAD, 0x35, 0x63, 0x37, 0x71 }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initContext( OUT CONTEXT_INFO *contextInfo,
						OUT PKC_INFO *pkcInfo )
	{
	int status;

	assert( isWritePtr( contextInfo, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Clear return values */
	memset( contextInfo, 0, sizeof( CONTEXT_INFO ) );
	memset( pkcInfo, 0, sizeof( PKC_INFO ) );

	status = staticInitContext( contextInfo, CONTEXT_PKC, 
								getRSACapability(), pkcInfo, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( status );
	status = importBignum( &pkcInfo->rsaParam_n, rsaTestKey.n, 
						   rsaTestKey.nLen, RSAPARAM_MIN_N, 
						   RSAPARAM_MAX_N, NULL, KEYSIZE_CHECK_PKC );
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_e, rsaTestKey.e, 
							   rsaTestKey.eLen, RSAPARAM_MIN_E, 
							   RSAPARAM_MAX_E, &pkcInfo->rsaParam_n, 
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_d, rsaTestKey.d, 
							   rsaTestKey.dLen, RSAPARAM_MIN_D, 
							   RSAPARAM_MAX_D, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_p, rsaTestKey.p, 
							   rsaTestKey.pLen, RSAPARAM_MIN_P, 
							   RSAPARAM_MAX_P, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_q, rsaTestKey.q, 
							   rsaTestKey.qLen, RSAPARAM_MIN_Q, 
							   RSAPARAM_MAX_Q, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_u, rsaTestKey.u, 
							   rsaTestKey.uLen, RSAPARAM_MIN_U, 
							   RSAPARAM_MAX_U, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_exponent1, rsaTestKey.e1, 
							   rsaTestKey.e1Len, RSAPARAM_MIN_EXP1, 
							   RSAPARAM_MAX_EXP1, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusOK( status ) )
		{
		status = importBignum( &pkcInfo->rsaParam_exponent2, rsaTestKey.e2, 
							   rsaTestKey.e2Len, RSAPARAM_MIN_EXP2, 
							   RSAPARAM_MAX_EXP2, &pkcInfo->rsaParam_n,
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
static int selfTest( void )
	{
	CONTEXT_INFO contextInfo;
	PKC_INFO contextData, *pkcInfo = &contextData;
	const CAPABILITY_INFO *capabilityInfoPtr;
	BYTE buffer[ 128 + 8 ];
	int status;

	/* Initialise the key components */
	status = initContext( &contextInfo, pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform the test en/decryption of a block of data */
	capabilityInfoPtr = DATAPTR_GET( contextInfo.capabilityInfo );
	REQUIRES( capabilityInfoPtr != NULL );
	status = capabilityInfoPtr->initKeyFunction( &contextInfo, NULL, 0 );
	if( cryptStatusError( status ) || \
		!pairwiseConsistencyTest( &contextInfo ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}
	staticDestroyContext( &contextInfo );

	/* Try it again with side-channel protection/blinding enabled.  Note 
	   that this uses the randomness subsystem, which can significantly slow 
	   down the self-test if it's being performed before the polling has 
	   completed.
	   
	   Since we're still using the same key but changing the way that it's
	   used, we have to call initKeyFunction() on the existing data, which
	   isn't normally done.  Because the re-init with blinding changed the
	   bignum state, we reset the checksums to force them to be re-
	   calculated */
	memcpy( buffer, "abcde", 5 );
	memset( buffer + 5, 0, rsaTestKey.nLen - 5 );
	status = initContext( &contextInfo, pkcInfo );
	if( cryptStatusError( status ) )
		return( status );
	SET_FLAG( contextInfo.flags, CONTEXT_FLAG_SIDECHANNELPROTECTION );
	status = capabilityInfoPtr->initKeyFunction( &contextInfo, NULL, 0 );
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfoPtr->encryptFunction( &contextInfo,
											buffer, rsaTestKey.nLen );
		}
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfoPtr->decryptFunction( &contextInfo,
											buffer, rsaTestKey.nLen );
		}
	if( cryptStatusError( status ) || memcmp( buffer, "abcde", 5 ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}

	/* And one last time to ensure that the blinding value update works */
	memcpy( buffer, "fghij", 5 );
	memset( buffer + 5, 0, rsaTestKey.nLen - 5 );
	status = capabilityInfoPtr->encryptFunction( &contextInfo,
											buffer, rsaTestKey.nLen );
	if( cryptStatusOK( status ) )
		{
		status = capabilityInfoPtr->decryptFunction( &contextInfo,
											buffer, rsaTestKey.nLen );
		}
	if( cryptStatusError( status ) || memcmp( buffer, "fghij", 5 ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}

	/* The checking for memory faults is performed at the 
	   MESSAGE_CTX_ENCRYPT level, so it won't be detected when we call the
	   function directly via an internal code pointer */
#if 0
	/* Finally, make sure that the memory fault-detection is working */
	pkcInfo->rsaParam_n.d[ 8 ] ^= 0x0100;
	status = capabilityInfoPtr->encryptFunction( &contextInfo,
												 buffer, rsaTestKey.nLen );
	if( cryptStatusOK( status ) )
		{
		/* The fault-detection couldn't detect a bit-flip, there's a 
		   problem */
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}
#else
	/* Emulation of what the above code would do */
	pkcInfo->rsaParam_n.d[ 8 ] ^= 0x0100;
	status = checksumContextData( pkcInfo, CRYPT_ALGO_RSA, TRUE );
	if( !cryptStatusError( status ) )
		{
		staticDestroyContext( &contextInfo );
		return( CRYPT_ERROR_FAILED );
		}
#endif /* 0 */

	/* Clean up */
	staticDestroyContext( &contextInfo );

	return( CRYPT_OK );
	}
#else
	#define selfTest	NULL
#endif /* !CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*							Encrypt/Decrypt a Data Block					*
*																			*
****************************************************************************/

/* Encrypt/signature check a single block of data.  We have to append the
   distinguisher 'Fn' to the name since some systems already have 'encrypt'
   and 'decrypt' in their standard headers */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int encryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_SHORT int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *n = &pkcInfo->rsaParam_n, *e = &pkcInfo->rsaParam_e;
	BIGNUM *data = &pkcInfo->tmp1;
	const int length = bitsToBytes( pkcInfo->keySizeBits );
	int offset, dummy, bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, length ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == length );
	REQUIRES( isShortIntegerRangeNZ( noBytes ) );

	/* Move the data from the buffer into a bignum */
	status = importBignum( data, buffer, length, 
						   MIN_PKCSIZE - 8, CRYPT_MAX_PKCSIZE, n, 
						   KEYSIZE_CHECK_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* Perform the modexp and move the result back into the buffer.  Since 
	   this function takes a fixed-length input and produces a fixed-length
	   result but the bignum code performs leading-zero truncation, we have 
	   to adjust where we copy the result to in the buffer to take into 
	   account extra zero bytes that aren't extracted from the bignum */
	CK( BN_mod_exp_mont( data, data, e, n, &pkcInfo->bnCTX,
						 &pkcInfo->rsaParam_mont_n ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	offset = length - BN_num_bytes( data );
	ENSURES( offset >= 0 && offset <= length );
	if( offset > 0 )
		{
		/* If the resulting value has more than 128 bits of leading zeroes
		   then there's something wrong */
		if( offset > 16 )
			return( CRYPT_ERROR_BADDATA );
		memset( buffer, 0, offset );
		}
	status = exportBignum( buffer + offset, noBytes - offset, &dummy, data );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/* Use the Chinese Remainder Theorem shortcut for RSA decryption/signature
   generation.  n isn't needed because of this.

   There are two types of side-channel attack protection that we employ for
   prvate-key operations, the first being standard blinding included in the
   code below.  The second type applies to CRT-based RSA implementations and
   is based on the fact that if a fault occurs during the computation of p2
   or q2 (to give, say, p2') then applying the CRT will yield a faulty
   signature M'.  An attacker can then compute q from
   gcd( M' ** e - ( C mod n ), n ), and the same for q2' and p.  The chances
   of this actually occurring are... unlikely, given that it requires a
   singleton failure inside the CPU (with data running over ECC-protected
   buses) at the exact moment of CRT computation (the original threat model
   assumed a fault-injection attack on a smart card), however we can still
   provide protection against the problem for people who consider it a
   genuine threat.

   The problem was originally pointed out by Marc Joye, Arjen Lenstra, and
   Jean-Jacques Quisquater in "Chinese Remaindering Based Cryptosystems in
   the Presence of Faults", Journal of Cryptology, Vol.12, No.4 (Autumn
   1999), p.241, based on an earlier result "On the importance of checking
   cryptographic protocols for faults", Dan Boneh, Richard DeMillo, and
   Richard Lipton, Proceedings of EuroCrypt'97, Springer-Verlag LNCS 
   Vol.1233, May 1997, p.37.  Adi Shamir presented one possible solution to 
   the problem in the conference's rump session in "How to check modular 
   exponentiation", which performs a parallel computation of the potentially 
   fault-affected portions of the CRT operation in blinded form and then 
   checks that the two outputs are consistent.  This has three drawbacks: 
   It's slow, Shamir patented it (US Patent 5,991,415), and if one CRT is 
   faulty there's no guarantee that the parallel CRT won't be faulty as 
   well.  Better solutions were suggested by Sung-Ming Yen, Seungjoo Kim, 
   Seongan Lim, and Sangjae Moon in "RSA Speedup with Residue Number System 
   Immune against Hardware Fault Cryptanalysis", Proceedings of the 
   Information Security and Cryptology Conference (ICISC'01), Springer-
   Verlag LNCS Vol.2288, December 2001, p.397.  These have less overhead 
   than Shamir's approach and are also less patented, but like Shamir's 
   approach they involve messing around with the CRT computation.  A further 
   update to this given by Sung-Ming Yen, Sangjae Kim, and Jae-Cheol Ha in 
   "Hardware Fault Attack on RSA with CRT Revisited", Proceedings of the 
   Information Security and Cryptology Conference (ICISC'02), Springer-
   Verlag LNCS Vol.2587, November 2002, p.374, which updated the earlier 
   work and also found flaws in Shamir's solution.  Numerous further updates
   and ideas followed, for example "RSA with CRT: A New Cost-Effective 
   Solution to Thwart Fault Attacks", David Vigilant, Proceedings of the
   Workshop on Cryptographic Hardware and Embedded Systems (CHES'08), 
   Springer-Verlag LNCS No.5154, August 2008, p.130, "Fault Attacks and 
   Countermeasures on Vigilant's RSA-CRT Algorithm", Jean-Sébastien Coron, 
   Christophe Giraud, Nicolas Morin, Gilles Piret and David Vigilant, 
   Proceedings of the Workshop on Fault Diagnosis and Tolerance in 
   Cryptography (FDTC'10), August 2010, p.89, and "Formal Analysis of 
   CRT-RSA Vigilant's Countermeasure Against the BellCoRe Attack", Pablo 
   Rauzy and Sylvain Guilley, Proceedings of the Program Protection and 
   Reverse Engineering Workshop (PPREW'14), January 2014, Article No.2.  
   More updates, with comprehensive surveys of existing approaches, are
   "The Fault Attack Jungle - A Classification Model to Guide You", 
   Proceedings of the Workshop on Fault Diagnosis and Tolerance in 
   Cryptography (FDTC'11), p.3 and "Countermeasures Against High-Order 
   Fault-Injection Attacks on CRT-RSA" by Pablo Rauzy and Sylvain Guilley, 
   Proceedings of the Workshop on Fault Diagnosis and Tolerance in 
   Cryptography (FDTC'14), currently unpublished but the paper is available 
   as https://eprint.iacr.org/2014/559.

   A much simpler solution is just to verify the CRT-based private-key
   operation with the matching public-key operation after we perform it.
   Since this is only required for signatures (the output of a decrypt is
   (a) never visible to an attacker and (b) verified via the PKCS #1
   padding), we perform this operation at a higher level, performing a
   signature verify after each signature generation at the crypto mechanism
   level */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decryptFn( INOUT CONTEXT_INFO *contextInfoPtr, 
					  INOUT_BUFFER_FIXED( noBytes ) BYTE *buffer, 
					  IN_LENGTH_SHORT int noBytes )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	BIGNUM *p = &pkcInfo->rsaParam_p, *q = &pkcInfo->rsaParam_q;
	BIGNUM *u = &pkcInfo->rsaParam_u, *e1 = &pkcInfo->rsaParam_exponent1;
	BIGNUM *e2 = &pkcInfo->rsaParam_exponent2;
	BIGNUM *data = &pkcInfo->tmp1, *p2 = &pkcInfo->tmp2, *q2 = &pkcInfo->tmp3;
	BOOLEAN dummyAdd = TRUE;
	const int length = bitsToBytes( pkcInfo->keySizeBits );
	int offset, dummy, bnStatus = BN_STATUS, status, LOOP_ITERATOR;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtrDynamic( buffer, length ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( noBytes == length );
	REQUIRES( isShortIntegerRangeNZ( noBytes ) );

	/* Move the data from the buffer into a bignum.  We need to make an 
	   unfortunate exception to the valid-length check for SSL's weird 
	   signatures, which sign a raw concatenated MD5 and SHA-1 hash with a 
	   total length of 36 bytes */
	status = importBignum( data, buffer, length, 36, CRYPT_MAX_PKCSIZE, 
						   &pkcInfo->rsaParam_n, KEYSIZE_CHECK_NONE );
	if( cryptStatusError( status ) )
		return( status );
	if( BN_num_bytes( data ) != 36 && \
		BN_num_bytes( data ) < MIN_PKCSIZE - 8 )
		{
		/* If it's not the weird SSL signature and it's outside the valid 
		   length range, reject it */
		return( CRYPT_ERROR_BADDATA );
		}

	/* If we're blinding the RSA operation, set:

		data = ( ( rand^e ) * data ) mod n (where k = rand^e) 
	
	   In theory we could use BN_mod_mul_montgomery() for this and the later
	   un-blinding and k update, but that would require converting data to
	   its Montgomery form (as well as k/kInv, but we could keep that in
	   the Montgomery form all the time).  It's not clear how much benefit
	   would be gained by this, since it requires two Montgomery-form 
	   conversions for each RSA operation */
	if( TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		{
		BIGNUM *k = &pkcInfo->rsaParam_blind_k;

		CK( BN_mod_mul( data, data, k, &pkcInfo->rsaParam_n, 
						&pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}

	/* Rather than decrypting by computing a modexp with full mod n
	   precision, compute a shorter modexp with mod p and mod q precision:

		p2 = ( ( C mod p ) ** exponent1 ) mod p
		q2 = ( ( C mod q ) ** exponent2 ) mod q */
	REQUIRES( BN_cmp( p, q ) > 0 );		/* Precondition for CRT shortcut */
	CK( BN_mod( p2, data, p,			/* p2 = C mod p  */
				&pkcInfo->bnCTX ) );
	CK( BN_mod_exp_mont( p2, p2, e1, p, &pkcInfo->bnCTX,
						 &pkcInfo->rsaParam_mont_p ) );
	CK( BN_mod( q2, data, q,			/* q2 = C mod q  */
				&pkcInfo->bnCTX ) );
	CK( BN_mod_exp_mont( q2, q2, e2, q, &pkcInfo->bnCTX,
						 &pkcInfo->rsaParam_mont_q ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* p2 = p2 - q2; if p2 < 0 then p2 = p2 + p.  In some extremely rare
	   cases (q2 large, p2 small) we have to add p twice to get p2
	   positive */
	CK( BN_sub( p2, p2, q2 ) );
	LOOP_SMALL_CHECK( bnStatusOK( bnStatus ) && BN_is_negative( p2 ) )
		{
		CK( BN_add( p2, p2, p ) );
		dummyAdd = FALSE;
		}
	ENSURES( LOOP_BOUND_OK );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* M = ( ( ( p2 * u ) mod p ) * q ) + q2 */
	CK( BN_mod_mul( data, p2, u, p,		/* data = ( p2 * u ) mod p */
					&pkcInfo->bnCTX ) );
	CK( BN_mul( p2, data, q,			/* p2 = data * q (bn can't reuse data) */
				&pkcInfo->bnCTX ) );
	CK( BN_add( data, p2, q2 ) );		/* data = p2 + q2 */
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	/* If we didn't adjust p above, perform a dummy add operation that takes
	   roughly the same time as the p-adjust would have.  The second add of p
	   is virtually nonexistent so we don't bother performing a second 
	   compensating add */
	if( dummyAdd )
		( void ) BN_add( q2, q2, p );

	/* If we're blinding the RSA operation, set

		data = ( ( data^e ) / rand ) mod n
			 = ( rand^-1 * data ) mod n */
	if( TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		{
		BIGNUM *n = &pkcInfo->rsaParam_n;
		BIGNUM *k = &pkcInfo->rsaParam_blind_k;
		BIGNUM *kInv = &pkcInfo->rsaParam_blind_kInv;

		CK( BN_mod_mul( data, data, kInv, n, &pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );

		/* Update the blinding values in such a way that we get new random
		   (that is, unpredictable to an outsider) numbers of the correct
		   form without having to do a full modexp as we would if starting
		   with new random data:

			k = ( k^2 ) mod n 
		
		   In theory we could replace the random blinding value periodically
		   in order to avoid an attacker being able to build up stats on it 
		   and the values derived from it, but it seems unlikely that they 
		   can do much with this and leads to problems of its own since the
		   process of calculating the new blinding value is itself 
		   susceptible to side-channel attacks */
		CK( BN_mod_mul( k, k, k, n, &pkcInfo->bnCTX ) );
		CK( BN_mod_mul( kInv, kInv, kInv, n, &pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		}

	/* Perform the modexp and move the result back into the buffer.  Since 
	   this function takes a fixed-length input and produces a fixed-length
	   result but the bignum code performs leading-zero truncation, we have 
	   to adjust where we copy the result to in the buffer to take into 
	   account extra zero bytes that aren't extracted from the bignum */
	offset = length - BN_num_bytes( data );
	ENSURES( offset >= 0 && offset <= length );
	if( offset > 0 )
		{
		/* If the resulting value has more than 128 bits of leading zeroes
		   then there's something wrong */
		if( offset > 16 )
			return( CRYPT_ERROR_BADDATA );
		memset( buffer, 0, offset );
		}
	status = exportBignum( buffer + offset, noBytes - offset, &dummy, data );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( pkcInfo ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Load Key Components							*
*																			*
****************************************************************************/

/* Load key components into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initKey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_BUFFER_OPT( keyLength ) const void *key,
					IN_LENGTH_SHORT_OPT const int keyLength )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( ( key == NULL && keyLength == 0 ) || \
			( isReadPtrDynamic( key, keyLength ) && \
			  keyLength == sizeof( CRYPT_PKCINFO_RSA ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( ( key == NULL && keyLength == 0 ) || \
			  ( key != NULL && keyLength == sizeof( CRYPT_PKCINFO_RSA ) ) );

#ifndef USE_FIPS140
	/* Load the key component from the external representation into the
	   internal bignums unless we're doing an internal load */
	if( key != NULL )
		{
		PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
		const CRYPT_PKCINFO_RSA *rsaKey = ( CRYPT_PKCINFO_RSA * ) key;
		int status;

		if( rsaKey->isPublicKey )
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPUBLICKEY );
		else
			SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_ISPRIVATEKEY );
		status = importBignum( &pkcInfo->rsaParam_n, rsaKey->n, 
							   bitsToBytes( rsaKey->nLen ), 
							   RSAPARAM_MIN_N, RSAPARAM_MAX_N, NULL, 
							   KEYSIZE_CHECK_PKC );
		if( cryptStatusOK( status ) )
			{
			status = importBignum( &pkcInfo->rsaParam_e, rsaKey->e, 
								   bitsToBytes( rsaKey->eLen ),
								   RSAPARAM_MIN_E, RSAPARAM_MAX_E,
								   &pkcInfo->rsaParam_n, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusOK( status ) && !rsaKey->isPublicKey )
			{
			status = importBignum( &pkcInfo->rsaParam_d, rsaKey->d, 
								   bitsToBytes( rsaKey->dLen ),
								   RSAPARAM_MIN_D, RSAPARAM_MAX_D,
								   &pkcInfo->rsaParam_n, 
								   KEYSIZE_CHECK_NONE );
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->rsaParam_p, rsaKey->p, 
									   bitsToBytes( rsaKey->pLen ),
									   RSAPARAM_MIN_P, RSAPARAM_MAX_P,
									   &pkcInfo->rsaParam_n, 
									   KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) )
				{
				status = importBignum( &pkcInfo->rsaParam_q, rsaKey->q, 
									   bitsToBytes( rsaKey->qLen ),
									   RSAPARAM_MIN_Q, RSAPARAM_MAX_Q,
									   &pkcInfo->rsaParam_n, 
									   KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) && rsaKey->uLen > 0 )
				{
				status = importBignum( &pkcInfo->rsaParam_u, rsaKey->u, 
									   bitsToBytes( rsaKey->uLen ),
									   RSAPARAM_MIN_U, RSAPARAM_MAX_U,
									   &pkcInfo->rsaParam_n, 
									   KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) && rsaKey->e1Len > 0 )
				{
				status = importBignum( &pkcInfo->rsaParam_exponent1, rsaKey->e1, 
									   bitsToBytes( rsaKey->e1Len ),
									   RSAPARAM_MIN_EXP1, RSAPARAM_MAX_EXP1,
									   &pkcInfo->rsaParam_n, 
									   KEYSIZE_CHECK_NONE );
				}
			if( cryptStatusOK( status ) && rsaKey->e2Len > 0 )
				{
				status = importBignum( &pkcInfo->rsaParam_exponent2, rsaKey->e2, 
									   bitsToBytes( rsaKey->e2Len ),
									   RSAPARAM_MIN_EXP2, RSAPARAM_MAX_EXP2,
									   &pkcInfo->rsaParam_n, 
									   KEYSIZE_CHECK_NONE );
				}
			}
		SET_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_PBO );
		if( cryptStatusError( status ) )
			return( status );

		ENSURES( sanityCheckPKCInfo( pkcInfo ) );
		}
#endif /* USE_FIPS140 */

	/* Complete the key checking and setup */
	return( initCheckRSAkey( contextInfoPtr ) );
	}

/* Generate a key into an encryption context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateKey( INOUT CONTEXT_INFO *contextInfoPtr, 
						IN_LENGTH_SHORT_MIN( MIN_PKCSIZE * 8 ) \
							const int keySizeBits )
	{
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keySizeBits >= bytesToBits( MIN_PKCSIZE ) && \
			  keySizeBits <= bytesToBits( CRYPT_MAX_PKCSIZE ) );

	status = generateRSAkey( contextInfoPtr, keySizeBits );
	if( cryptStatusOK( status ) &&
#ifndef USE_FIPS140
		TEST_FLAG( contextInfoPtr->flags, 
				   CONTEXT_FLAG_SIDECHANNELPROTECTION ) &&
#endif /* USE_FIPS140 */
		!pairwiseConsistencyTest( contextInfoPtr ) )
		{
		DEBUG_DIAG(( "Consistency check of freshly-generated RSA key "
					 "failed" ));
		assert( DEBUG_WARN );
		status = CRYPT_ERROR_FAILED;
		}
	return( cryptArgError( status ) ? CRYPT_ERROR_FAILED : status );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO capabilityInfo = {
	CRYPT_ALGO_RSA, bitsToBytes( 0 ), "RSA", 3,
	MIN_PKCSIZE, bitsToBytes( 1536 ), CRYPT_MAX_PKCSIZE,
	selfTest, getDefaultInfo, NULL, NULL, initKey, generateKey, 
	encryptFn, decryptFn, NULL, NULL, NULL, NULL, NULL, NULL, 
	decryptFn, encryptFn
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getRSACapability( void )
	{
	return( &capabilityInfo );
	}
