/****************************************************************************
*																			*
*				cryptlib ECC Key Generation/Checking Routines				*
*					Copyright Peter Gutmann 2006-2008						*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
  #include "keygen.h"
#else
  #include "crypt.h"
  #include "context/context.h"
  #include "context/keygen.h"
#endif /* Compiler-specific includes */

#if defined( USE_ECDH ) || defined( USE_ECDSA )

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Enable various side-channel protection mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int enableSidechannelProtection( INOUT PKC_INFO *pkcInfo,
										IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( isEccAlgo( cryptAlgo ) );

	/* Use constant-time modexp() to protect the private key from timing 
	   channels.

	   (There really isn't much around in the way of side-channel protection 
	   for the ECC computations, for every operation we use a new random 
	   secret value as the point multiplier so there doesn't seem to be much 
	   scope for timing attacks.  In the absence of anything better to do, 
	   we set the constant-time modexp() flag just for warm fuzzies) */
	BN_set_flags( &pkcInfo->eccParam_d, BN_FLG_EXP_CONSTTIME );

	/* Checksum the bignums to try and detect fault attacks.  Since we're 
	   setting the checksum at this point there's no need to check the 
	   return value */
	( void ) calculateBignumChecksum( pkcInfo, cryptAlgo );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Fixed ECC Parameters						*
*																			*
****************************************************************************/

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

typedef struct {
	CRYPT_ECCCURVE_TYPE paramType;
	const int curveSizeBits;
	const BYTE *p, *a, *b, *gx, *gy, *n, *h;
	} ECC_DOMAIN_PARAMS;

static const ECC_DOMAIN_PARAMS domainParamTbl[] = {
	/* NIST P-192, X9.62 p192r1, SECG p192r1 */
	{ CRYPT_ECCCURVE_P192, 192,
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x64\x21\x05\x19\xE5\x9C\x80\xE7" \
			  "\x0F\xA7\xE9\xAB\x72\x24\x30\x49" \
			  "\xFE\xB8\xDE\xEC\xC1\x46\xB9\xB1" ),
	  MKDATA( "\x18\x8D\xA8\x0E\xB0\x30\x90\xF6" \
			  "\x7C\xBF\x20\xEB\x43\xA1\x88\x00" \
			  "\xF4\xFF\x0A\xFD\x82\xFF\x10\x12" ),
	  MKDATA( "\x07\x19\x2B\x95\xFF\xC8\xDA\x78" \
			  "\x63\x10\x11\xED\x6B\x24\xCD\xD5" \
			  "\x73\xF9\x77\xA1\x1E\x79\x48\x11" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
			  "\xFF\xFF\xFF\xFF\x99\xDE\xF8\x36" \
			  "\x14\x6B\xC9\xB1\xB4\xD2\x28\x31" ),
	  MKDATA( "\x01" ) },
#if 0
	/* X9.62 P192v2 */
	{ CRYPT_ECCCURVE_P192v2, 192,
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\xCC\x22\xD6\xDF\xB9\x5C\x6B\x25" \
			  "\xE4\x9C\x0D\x63\x64\xA4\xE5\x98" \
			  "\x0C\x39\x3A\xA2\x16\x68\xD9\x53" ),
	  MKDATA( "\xEE\xA2\xBA\xE7\xE1\x49\x78\x42" \
			  "\xF2\xDE\x77\x69\xCF\xE9\xC9\x89" \
			  "\xC0\x72\xAD\x69\x6F\x48\x03\x4A" ),
	  MKDATA( "\x65\x74\xD1\x1D\x69\xB6\xEC\x7A" \
			  "\x67\x2B\xB8\x2A\x08\x3D\xF2\xF2" \
			  "\xB0\x84\x7D\xE9\x70\xB2\xDE\x15" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFE\x5F\xB1\xA7\x24" \
			  "\xDC\x80\x41\x86\x48\xD8\xDD\x31" ) },
	/* X9.62 P192v3 */
	{ CRYPT_ECCCURVE_P192v3, 192,
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x22\x12\x3D\xC2\x39\x5A\x05\xCA" \
			  "\xA7\x42\x3D\xAE\xCC\xC9\x47\x60" \
			  "\xA7\xD4\x62\x25\x6B\xD5\x69\x16" ),
	  MKDATA( "\x7D\x29\x77\x81\x00\xC6\x5A\x1D" \
			  "\xA1\x78\x37\x16\x58\x8D\xCE\x2B" \
			  "\x8B\x4A\xEE\x8E\x22\x8F\x18\x96" ),
	  MKDATA( "\x38\xA9\x0F\x22\x63\x73\x37\x33" \
			  "\x4B\x49\xDC\xB6\x6A\x6D\xC8\xF9" \
			  "\x97\x8A\xCA\x76\x48\xA9\x43\xB0" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7A\x62\xD0\x31" \
			  "\xC8\x3F\x42\x94\xF6\x40\xEC\x13" ) },
#endif /* 0 */

	/* NIST P-224, X9.62 P224r1, SECG p224r1 */
	{ CRYPT_ECCCURVE_P224, 224,
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\x00\x00\x00\x00\x00\x00\x00\x00" \
			  "\x00\x00\x00\x01" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFE" ),
	  MKDATA( "\xB4\x05\x0A\x85\x0C\x04\xB3\xAB" \
			  "\xF5\x41\x32\x56\x50\x44\xB0\xB7" \
			  "\xD7\xBF\xD8\xBA\x27\x0B\x39\x43" \
			  "\x23\x55\xFF\xB4" ),
	  MKDATA( "\xB7\x0E\x0C\xBD\x6B\xB4\xBF\x7F" \
			  "\x32\x13\x90\xB9\x4A\x03\xC1\xD3" \
			  "\x56\xC2\x11\x22\x34\x32\x80\xD6" \
			  "\x11\x5C\x1D\x21" ),
	  MKDATA( "\xBD\x37\x63\x88\xB5\xF7\x23\xFB" \
			  "\x4C\x22\xDF\xE6\xCD\x43\x75\xA0" \
			  "\x5A\x07\x47\x64\x44\xD5\x81\x99" \
			  "\x85\x00\x7E\x34" ),
	  MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\xFF\xFF\x16\xA2" \
			  "\xE0\xB8\xF0\x3E\x13\xDD\x29\x45" \
			  "\x5C\x5C\x2A\x3D" ),
	  MKDATA( "\x01" ) },
#if 0
	/* X9.62 P239v1 */
	{ CRYPT_ECCCURVE_P239, 239,
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x6B\x01\x6C\x3B\xDC\xF1\x89\x41" \
			  "\xD0\xD6\x54\x92\x14\x75\xCA\x71" \
			  "\xA9\xDB\x2F\xB2\x7D\x1D\x37\x79" \
			  "\x61\x85\xC2\x94\x2C\x0A" ),
	  MKDATA( "\x0F\xFA\x96\x3C\xDC\xA8\x81\x6C" \
			  "\xCC\x33\xB8\x64\x2B\xED\xF9\x05" \
			  "\xC3\xD3\x58\x57\x3D\x3F\x27\xFB" \
			  "\xBD\x3B\x3C\xB9\xAA\xAF" ),
	  MKDATA( "\x7D\xEB\xE8\xE4\xE9\x0A\x5D\xAE" \
			  "\x6E\x40\x54\xCA\x53\x0B\xA0\x46" \
			  "\x54\xB3\x68\x18\xCE\x22\x6B\x39" \
			  "\xFC\xCB\x7B\x02\xF1\xAE" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\x9E" \
			  "\x5E\x9A\x9F\x5D\x90\x71\xFB\xD1" \
			  "\x52\x26\x88\x90\x9D\x0B" ) },
	/* X9.62 P239v2 */
	{ CRYPT_ECCCURVE_P239v2, 239,
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x61\x7F\xAB\x68\x32\x57\x6C\xBB" \
			  "\xFE\xD5\x0D\x99\xF0\x24\x9C\x3F" \
			  "\xEE\x58\xB9\x4B\xA0\x03\x8C\x7A" \
			  "\xE8\x4C\x8C\x83\x2F\x2C" ),
	  MKDATA( "\x38\xAF\x09\xD9\x87\x27\x70\x51" \
			  "\x20\xC9\x21\xBB\x5E\x9E\x26\x29" \
			  "\x6A\x3C\xDC\xF2\xF3\x57\x57\xA0" \
			  "\xEA\xFD\x87\xB8\x30\xE7" ),
	  MKDATA( "\x5B\x01\x25\xE4\xDB\xEA\x0E\xC7" \
			  "\x20\x6D\xA0\xFC\x01\xD9\xB0\x81" \
			  "\x32\x9F\xB5\x55\xDE\x6E\xF4\x60" \
			  "\x23\x7D\xFF\x8B\xE4\xBA" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x80\x00\x00\xCF" \
			  "\xA7\xE8\x59\x43\x77\xD4\x14\xC0" \
			  "\x38\x21\xBC\x58\x20\x63" ) },
	/* X9.62 P239v3 */
	{ CRYPT_ECCCURVE_P239v3, 239,
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFF" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\xFF" \
			  "\xFF\xFF\x80\x00\x00\x00\x00\x00" \
			  "\x7F\xFF\xFF\xFF\xFF\xFC" ),
	  MKDATA( "\x25\x57\x05\xFA\x2A\x30\x66\x54" \
			  "\xB1\xF4\xCB\x03\xD6\xA7\x50\xA3" \
			  "\x0C\x25\x01\x02\xD4\x98\x87\x17" \
			  "\xD9\xBA\x15\xAB\x6D\x3E" ),
	  MKDATA( "\x67\x68\xAE\x8E\x18\xBB\x92\xCF" \
			  "\xCF\x00\x5C\x94\x9A\xA2\xC6\xD9" \
			  "\x48\x53\xD0\xE6\x60\xBB\xF8\x54" \
			  "\xB1\xC9\x50\x5F\xE9\x5A" ),
	  MKDATA( "\x16\x07\xE6\x89\x8F\x39\x0C\x06" \
			  "\xBC\x1D\x55\x2B\xAD\x22\x6F\x3B" \
			  "\x6F\xCF\xE4\x8B\x6E\x81\x84\x99" \
			  "\xAF\x18\xE3\xED\x6C\xF3" ),
	  MKDATA( "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF" \
			  "\xFF\xFF\xFF\xFF\x7F\xFF\xFF\x97" \
			  "\x5D\xEB\x41\xB3\xA6\x05\x7C\x3C" \
			  "\x43\x21\x46\x52\x65\x51" ) },
#endif /* 0 */

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
	  MKDATA( "\x01" ) },

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
	  MKDATA( "\x01" ) },

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
	  MKDATA( "\x01" ) },

	/* End-of-list marker */
	{ CRYPT_ECCCURVE_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
		{ CRYPT_ECCCURVE_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
	};

/* Initialise the bignums for the domain parameter values { p, a, b, gx, gy, 
   n }.  Note that although the cofactor h is given for the standard named 
   curves it's not used for any ECC operations so we don't bother allocating 
   a bignum to it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int loadECCparams( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const ECC_DOMAIN_PARAMS *eccParams = NULL;
	int curveSize, i, bnStatus = BN_STATUS;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* Find the parameter info for this curve */
	for( i = 0; domainParamTbl[ i ].paramType != CRYPT_ECCCURVE_NONE && \
				i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ); 
		 i++ )
		{
		if( domainParamTbl[ i ].paramType == pkcInfo->curveType )
			{
			eccParams = &domainParamTbl[ i ];
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ) );
	ENSURES( eccParams != NULL );

	/* For the named curve the key size is defined by exective fiat based
	   on the curve type rather than being taken from the public-key value 
	   (which in this case is the magnitude of the point Q on the curve), so 
	   we set it explicitly based on the curve type */
	pkcInfo->keySizeBits = eccParams->curveSizeBits;
	curveSize = bitsToBytes( eccParams->curveSizeBits );

	/* Load the parameters into the context bignums */
	CKPTR( BN_bin2bn( eccParams->p, curveSize, &pkcInfo->eccParam_p ) );
	CKPTR( BN_bin2bn( eccParams->a, curveSize, &pkcInfo->eccParam_a ) );
	CKPTR( BN_bin2bn( eccParams->b, curveSize, &pkcInfo->eccParam_b ) );
	CKPTR( BN_bin2bn( eccParams->gx, curveSize, &pkcInfo->eccParam_gx ) );
	CKPTR( BN_bin2bn( eccParams->gy, curveSize, &pkcInfo->eccParam_gy ) );
	CKPTR( BN_bin2bn( eccParams->n, curveSize, &pkcInfo->eccParam_n ) );
	CKPTR( BN_bin2bn( eccParams->h, curveSize, &pkcInfo->eccParam_h ) );

	return( getBnStatus( bnStatus ) );
	}

/****************************************************************************
*																			*
*						ECC Information Access Functions					*
*																			*
****************************************************************************/

/* Get the nominal size of an ECC field based on an CRYPT_ECCCURVE_TYPE */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int getECCFieldSize( IN_ENUM( CRYPT_ECCCURVE ) \
						const CRYPT_ECCCURVE_TYPE fieldID,
					 OUT_INT_Z int *fieldSize )
	{
	int i;

	assert( isWritePtr( fieldSize, sizeof( int ) ) );

	REQUIRES( fieldID > CRYPT_ECCCURVE_NONE && \
			  fieldID < CRYPT_ECCCURVE_LAST );

	/* Clear return value */
	*fieldSize = 0;

	/* Find the nominal field size for the given fieldID */
	for( i = 0; 
		 domainParamTbl[ i ].paramType != CRYPT_ECCCURVE_NONE && \
			i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ); 
		 i++ )
		{
		if( domainParamTbl[ i ].paramType == fieldID )
			{
			*fieldSize = bitsToBytes( domainParamTbl[ i ].curveSizeBits );
			return( CRYPT_OK );
			}
		}

	retIntError();
	}

/* Get a CRYPT_ECCCURVE_TYPE based on a nominal ECC field size */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int getECCFieldID( IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC ) \
								const int fieldSize,
						  OUT_ENUM_OPT( CRYPT_ECCCURVE ) 
								CRYPT_ECCCURVE_TYPE *fieldID )
	{
	int i;

	assert( isWritePtr( fieldID, sizeof( CRYPT_ECCCURVE_TYPE ) ) );

	REQUIRES( fieldSize >= MIN_PKCSIZE_ECC && \
			  fieldSize <= CRYPT_MAX_PKCSIZE_ECC );

	/* Clear return value */
	*fieldID = CRYPT_ECCCURVE_NONE;

	/* Find the fieldID for the given nominal field size */
	for( i = 0; 
		 domainParamTbl[ i ].paramType != CRYPT_ECCCURVE_NONE && \
			i < FAILSAFE_ARRAYSIZE( domainParamTbl, ECC_DOMAIN_PARAMS ); 
		 i++ )
		{
		if( bitsToBytes( domainParamTbl[ i ].curveSizeBits ) >= fieldSize )
			{
			*fieldID = domainParamTbl[ i ].paramType;
			return( CRYPT_OK );
			}
		}

	/* Because of rounding issues in parameter bounds checking, combined 
	   with the odd-length sizes chosen for some of the standard curves,
	   we can end up with a situation where the requested curve size is a 
	   few bits over even the largest field size.  If we run into this 
	   situation then we use the largest field size */
	if( fieldSize >= bitsToBytes( 521 ) )
		{
		*fieldID = CRYPT_ECCCURVE_P521;
		return( CRYPT_OK );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*								Generate an ECC Key							*
*																			*
****************************************************************************/

/* Generate the ECC private value d and public value Q */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateECCPrivateValue( INOUT PKC_INFO *pkcInfo,
									IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC * 8 ) \
										const int keyBits )
	{
	BIGNUM *p = &pkcInfo->eccParam_p, *d = &pkcInfo->eccParam_d;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	REQUIRES( keyBits >= bytesToBits( MIN_PKCSIZE_ECC ) && \
			  keyBits <= bytesToBits( CRYPT_MAX_PKCSIZE_ECC ) );

	/* Generate the ECC private value d s.t. 2 <= d <= p-2.  Because the mod 
	   p-2 is expensive we do a quick check to make sure that it's really 
	   necessary before calling it */
	status = generateBignum( d, keyBits, 0xC0, 0 );
	if( cryptStatusError( status ) )
		return( status );
	CK( BN_sub_word( p, 2 ) );
	if( BN_cmp( d, p ) > 0 )
		{
		/* Trim d down to size.  Actually we get the upper bound as p-3, 
		   but over a 192-bit (minimum) number range this doesn't matter */
		CK( BN_mod( d, d, p, pkcInfo->bnCTX ) );

		/* If the value that we ended up with is too small, just generate a 
		   new value one bit shorter, which guarantees that it'll fit the 
		   criteria (the target is a suitably large random value, not the 
		   closest possible fit within the range) */
		if( bnStatusOK( bnStatus ) && BN_num_bits( d ) < keyBits - 5 )
			status = generateBignum( d, keyBits - 1, 0xC0, 0 );
		}
	CK( BN_add_word( p, 2 ) );

	return( cryptStatusError( status ) ? status : getBnStatus( bnStatus ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int generateECCPublicValue( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *d = &pkcInfo->eccParam_d;
	BIGNUM *qx = &pkcInfo->eccParam_qx, *qy = &pkcInfo->eccParam_qy;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *q = pkcInfo->tmpPoint;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Calculate the public-key value Q = d * G */
	CK( EC_POINT_mul( ecCTX, q, d, NULL, NULL, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	CK( EC_POINT_get_affine_coordinates_GFp( ecCTX, q, qx, qy,
											 pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Check an ECC Key							*
*																			*
****************************************************************************/

/* Perform validity checks on the public key.  In the tradition of these
   documents, FIPS 186 and the X9.62 standard that it's derived from 
   occasionally use different names for the parameters, the following 
   mapping can be used to go from one to the other:

	FIPS 186	X9.62/SECG		Description
	--------	----------		-----------
		p			p			Finite field Fp
	  a, b		  a, b			Elliptic curve y^2 == x^3 + ax + b
	 xg, yg		 xg, yg			Base point G on the curve
		n			n			Prime n of order G
		h			h			Cofactor
	--------	---------		-----------
	 wx, wy		 xq, yq			Public key Q
		d			d			Private key

   We have to make the PKC_INFO data non-const because the bignum code wants 
   to modify some of the values as it's working with them */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN checkComponentLength( const BIGNUM *component,
									 const BIGNUM *p,
									 const BOOLEAN lowerBoundZero )
	{
	int length;

	assert( isReadPtr( component, sizeof( BIGNUM ) ) );
	assert( isReadPtr( p, sizeof( BIGNUM ) ) );

	/* Make sure that the component is in the range 0...p - 1 (optionally
	   MIN_PKCSIZE_ECC if lowerBoundZero is false) */
	length = BN_num_bytes( component );
	if( length < ( lowerBoundZero ? 0 : MIN_PKCSIZE_ECC ) || \
		length > CRYPT_MAX_PKCSIZE_ECC )
		return( FALSE );
	if( BN_cmp( component, p ) >= 0 )
		return( FALSE );

	return( TRUE );
	}

/* Check that y^2 = x^3 + a*x + b is in the field GF(p) */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
static BOOLEAN isPointOnCurve( const BIGNUM *y, const BIGNUM *x, 
							   const BIGNUM *a, const BIGNUM *b, 
							   INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->eccParam_p;
	BIGNUM *tmp1 = &pkcInfo->tmp1, *tmp2 = &pkcInfo->tmp2;
	BN_CTX *ctx = pkcInfo->bnCTX;
	int bnStatus = BN_STATUS;

	assert( isReadPtr( y, sizeof( BIGNUM ) ) );
	assert( isReadPtr( x, sizeof( BIGNUM ) ) );
	assert( isReadPtr( a, sizeof( BIGNUM ) ) );
	assert( isReadPtr( b, sizeof( BIGNUM ) ) );
	assert( isReadPtr( p, sizeof( BIGNUM ) ) );
	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	CK( BN_mod_mul( tmp1, y, y, p, ctx ) );
	CK( BN_mod_mul( tmp2, x, x, p, ctx ) );
	CK( BN_mod_add( tmp2, tmp2, a, p, ctx ) );
	CK( BN_mod_mul( tmp2, tmp2, x, p, ctx ) );
	CK( BN_mod_add( tmp2, tmp2, b, p, ctx ) );
	if( bnStatusError( bnStatus ) )
		return( FALSE );
	if( BN_cmp( tmp1, tmp2 ) != 0 )
		return( FALSE );
	
	return( TRUE );
	}

/* Check the ECC domain parameters.  X9.62 specifies curves in the field 
   GF(q) of size q, where q is either a prime integer or equal to 2^m (with 
   m prime).  The following checks only handle curves where q is a prime 
   integer (the de facto universal standard), referred to as 'p' in the 
   following code */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkECCDomainParameters( INOUT PKC_INFO *pkcInfo,
									 const BOOLEAN isFullyInitialised )
	{
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	BIGNUM *p = &pkcInfo->eccParam_p;
	BIGNUM *a = &pkcInfo->eccParam_a, *b = &pkcInfo->eccParam_b;
	BIGNUM *n = &pkcInfo->eccParam_n, *h = &pkcInfo->eccParam_h;
	BIGNUM *gx = &pkcInfo->eccParam_gx, *gy = &pkcInfo->eccParam_gy;
	BIGNUM *tmp1 = &pkcInfo->tmp1, *tmp2 = &pkcInfo->tmp2;
	BIGNUM *tmp3_h = &pkcInfo->tmp3;
	EC_POINT *vp = pkcInfo->tmpPoint;
	const BOOLEAN isStandardCurve = \
			( pkcInfo->curveType != CRYPT_ECCCURVE_NONE ) ? TRUE : FALSE;
	int length, i, plen, bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that the domain parameter sizes are valid:

		pLen >= MIN_PKCSIZE_ECC, pLen <= CRYPT_MAX_PKCSIZE_ECC
		a, b >= 0, a, b <= p - 1
		gx, gy >= 0, gx, gy <= p - 1
		nLen >= MIN_PKCSIZE_ECC, nLen <= CRYPT_MAX_PKCSIZE_ECC
		hLen >= 1, hLen <= CRYPT_MAX_PKCSIZE_ECC (if present) */
	length = BN_num_bytes( p );
	if( length < MIN_PKCSIZE_ECC || length > CRYPT_MAX_PKCSIZE_ECC )
		return( CRYPT_ARGERROR_STR1 );
	if( !checkComponentLength( a, p, TRUE ) || \
		!checkComponentLength( b, p, TRUE ) )
		return( CRYPT_ARGERROR_STR1 );
	if( !checkComponentLength( gx, p, TRUE ) || \
		!checkComponentLength( gy, p, TRUE ) )
		return( CRYPT_ARGERROR_STR1 );
	length = BN_num_bytes( n );
	if( length < MIN_PKCSIZE_ECC || length > CRYPT_MAX_PKCSIZE_ECC )
		return( CRYPT_ARGERROR_STR1 );
	if( !BN_is_zero( h ) )
		{
		length = BN_num_bytes( h );
		if( length < 1 || length > CRYPT_MAX_PKCSIZE_ECC )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Whether the domain parameters need to be validated each time that 
	   they're loaded is left unclear in the specifications.  In fact 
	   whether they, or the public key data, needs to be validated at all, 
	   is left unspecified.  FIPS 186-3 is entirely silent on the matter and 
	   X9.62 defers to an annex that contains two whole sentences that 
	   merely say that it's someone else's problem.  In the absence of any 
	   guidance we only validate domain parameters if they're supplied 
	   externally, although it could be argued that to prevent memory 
	   corruption side-channel attacks we should really validate the hard-
	   coded internal ones as well.
	   
	   To activate domain validation on known curves, comment out the check
	   below */
	if( isStandardCurve )
		return( CRYPT_OK );

	/* Verify that p is not (obviously) composite */
	if( !primeSieve( &pkcInfo->eccParam_p ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that a, b and gx, gy are integers in the range 0...p - 1.  
	   This has already been done in the range checks performed earlier */

	/* Verify that (4a^3 + 27b^2) is not congruent to zero (mod p) */
	CK( BN_sqr( tmp1, a, pkcInfo->bnCTX ) );
	CK( BN_mul( tmp1, tmp1, a, pkcInfo->bnCTX ) );
	CK( BN_mul_word( tmp1, 4 ) );
	CK( BN_sqr( tmp2, b, pkcInfo->bnCTX ) );
	CK( BN_mul_word( tmp2, 27 ) );
	CK( BN_add( tmp1, tmp1, tmp2 ) );
	CK( BN_mod( tmp1, tmp1, p, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_is_zero( tmp1 ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that gy^2 is congruent to gx^3 + a*gx + b (mod p) */
	if( !isPointOnCurve( gy, gx, a, b, pkcInfo ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that:

		n is not (obviously) composite
		n > 2^160 and n > 2^( 2s-1 ), where s is the "security level"

		The second test is about ensuring that the subgroup order is large 
		enough to make the ECDLP infeasible.  X9.62 measures infeasibility 
		by the security level, and the test on 2^160 means that X9.62 
		considers that there is no worthwhile security below level 80.5.  
		The security level has already been implicitly checked via the 
		MIN_PKCSIZE_ECC constant, and n has already been tested above */
	if( !primeSieve( &pkcInfo->eccParam_n ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that n * G is the point at infinity.  This runs into a nasty 
	   catch-22 where we have to initialise at least some of the ECC 
	   parameter information in order to perform the check, but we can't 
	   (safely) use the parameters before we've checked them.  To get around
	   this, if the values haven't been fully set up yet then we perform the 
	   check inline in the initCheckECCKey() code */
	if( isFullyInitialised )
		{
		CK( EC_POINT_mul( ecCTX, vp, n, NULL, NULL, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		if( !EC_POINT_is_at_infinity( ecCTX, vp ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Compute the cofactor h.  The domain parameters may include h, but we 
	   still want to compute it and check that we get the same value.  The 
	   cofactor will be needed for verification of the "anomalous condition".

	   The curve size is #E = q+1-t, where t is the trace of Frobenius.  
	   Hasse's theorem guarantees that |t| <= 2*sqrt(q).  Since we work over 
	   a subgroup of size n, we know that n divides #E; the cofactor h is 
	   the integer such that #E = n*h.

	   In the X9.62 world, h is much smaller than n. It follows that h can 
	   be recomputed accurately from q and n, since there's only one integer 
	   value for h that puts t in the proper range.  The formula given in 
	   X9.62 is:

			h = floor( ( sqrt( q ) + 1 )^2 / n )

	   which expands to:

			h = floor( ( q + 1 + 2*sqrt( q ) ) / n )

	   which can be seen to map to the proper cofactor: we compute a value 
	   which is one more than the maximum possible curve order, and divide 
	   by n.  If we then verify that h is small enough (which is a 
	   requirement of X9.62) then we know that we've found the proper 
	   cofactor.

	   The X9.62 formula is cumbersome because it requires a square root, 
	   and we have to keep track of fractional bits as well.  The expanded 
	   formula requires less fractional bits, but it still needs a square 
	   root.  Instead, we can take advantage of the fact that the conditions 
	   on the "security level" (n >= 2^160, n >= 2^(2s-1) and h <= 2^(s/8)
	   for an integer s) imply that n >= h^15.  A consequence of this is 
	   that n*h <= n^(1+1/15).  n*h is the curve order, which is close to q 
	   and lower than 2*q.

	   Now, let qlen = log2(q) (the size of q in bits; 
	   2^( qlen-1 ) <= q < 2^qlen).  We then compute:

	      z = q + 2^( floor( qlen/2 ) + 3 )

	   which is easily obtained by shifting one bit and performing an 
	   addition.  It's then easily seen that z > #E.  Since we work in a 
	   field of sufficiently large size (according to X9.62, qlen >= 160, 
	   but we may accept slightly smaller values of q, but no less than 
	   qlen = 128), we can show that n is much larger than
	   2^( floor( qlen/2 ) + 4 ) (the size of n is at least 15/16ths of that 
	   of q).  This implies that z-n is much smaller than the minimal 
	   possible size of E.

	   We conclude that h = floor( z / n ), which is then the formula that 
	   we apply.  We also check that h is no larger than 1/14th of n (using 
	   14 instead of 15 avoids rounding issues since the size in bits of h 
	   is not exactly equal to log2(h), but it's still good enough for the 
	   analysis above).

	   For the standard named curves in GF(p) (the P-* NIST curves), h = 1
	   and is implicitly present. */
	plen = BN_num_bits( p );
	CK( BN_one( tmp1 ) );
	CK( BN_lshift( tmp1, tmp1, ( plen >> 1 ) + 3 ) );
	CK( BN_add( tmp1, tmp1, p ) );
	CK( BN_div( tmp3_h, NULL, tmp1, n, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( ( BN_num_bits( tmp3_h ) * 14 ) > BN_num_bits( n ) )
		return( CRYPT_ARGERROR_STR1 );
	if( isStandardCurve )
		{
		/* This code is executed only if the early exit for known curves 
		   above has been deactivated */
		if ( !BN_is_one( tmp3_h ) )
			return( CRYPT_ARGERROR_STR1 );
		}
	else
		{
		if( !BN_is_zero( h ) && BN_cmp( h, tmp3_h ) != 0 )
			return( CRYPT_ARGERROR_STR1 );
		}
	h = tmp3_h;

	/* Verify that the MOV condition holds.  We use an MOV threshold B = 100
	   as per X9.62:2005 ('98 used B = 20).
	   
	   Mathematically, we want to make sure that the embedding degree of the 
	   curve (for the subgroup that we're interested in) is greater than 100.  
	   The embedding degree is the lowest integer k such that r divides q^k-1 
	   (using the X9.62 notations where q is the field size, here held in 
	   the local variable p, while r is the subgroup order size, which the 
	   code below calls n).  The embedding degree always exists, except if 
	   the curve is anomalous, i.e. the size of the curve is equal to the 
	   field size.  Anomalous curves are a bad thing to have, so they're
	   checked for later on.

	   The Weil and Tate pairings allow the conversion of the ECDLP problem
	   into the "plain" DLP problem in GF(q^k), the field extension of GF(q) 
	   of degree k, for which subexponential algorithms are known.  For 
	   example if a curve is defined over a field GF(q) such that the size 
	   of q is 256 bits and n also has a size close to 256 bits then we 
	   expect the ECDLP to have a cost of about O( 2^128 ), i.e. it's very 
	   much infeasible.  However if that curve happens to have an extension 
	   degree k = 2 then a Weil or Tate pairing transforms the ECLDP into 
	   the DLP in GF(q^2), a field of size 512 bits, for which the DLP is 
	   non-trivial but technologically feasible.

	   For a random curve, k should be a large integer of the same size as 
	   n, thus making q^k astronomically high.  For our purposes we want to 
	   make sure that k isn't very small.  X9.62 checks that k > 100, which 
	   is good enough, since if if k = 101 then the 256-bit ECDLP is 
	   transformed into the 25856-bit DLP, which is awfully big and totally 
	   infeasible as far as we know.

	   Computing the value of k is feasible if you can factor r-1, but this 
	   is expensive.  The X9.62 test computes q, q^2, q^3... q^100 modulo n 
	   and checks that none of these equal one */
	CK( BN_one( tmp1 ) );
	CK( BN_mod( tmp2, p, n, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	for( i = 0; i < 100; i++ )
		{
		CK( BN_mod_mul( tmp1, tmp1, tmp2, n, pkcInfo->bnCTX ) );
		if( bnStatusError( bnStatus ) )
			return( getBnStatus( bnStatus ) );
		if( BN_is_one( tmp1 ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Verify that the anomalous condition holds.  An "anomalous curve" is a 
	   curve E defined over a field of size q such that #E = q, i.e. the 
	   curve size is equal to the field size.  ECDLP on an anomalous curve 
	   can be computed very efficiently, hence we want to avoid anomalous 
	   curves.

	   The curve order is r*h, where h is the cofactor, which we computed 
	   (and checked) above */
	CK( BN_mul( tmp1, n, h, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( tmp1, p ) == 0 )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkECCPublicKey( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->eccParam_p, *n = &pkcInfo->eccParam_n;
	BIGNUM *qx = &pkcInfo->eccParam_qx, *qy = &pkcInfo->eccParam_qy;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *q = pkcInfo->tmpPoint;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that the public key parameter sizes are valid:

		qx, qy >= MIN_PKCSIZE_ECC, qx, qy <= p - 1 */
	if( !checkComponentLength( qx, p, FALSE ) || \
		!checkComponentLength( qy, p, FALSE ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that Q is not the point at infinity */
	CK( EC_POINT_set_affine_coordinates_GFp( ecCTX, q, qx, qy,
											 pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( EC_POINT_is_at_infinity( ecCTX, q ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that qx, qy are elements in the field Fq, i.e. in the range
	   0...p - 1.  This has already been done in the range checks performed
	   earlier */

	/* Verify that qy^2 is congruent to qx^3 + a*qx + b (mod p) */
	if( !isPointOnCurve( qy, qx, &pkcInfo->eccParam_a, &pkcInfo->eccParam_b, 
						 pkcInfo ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that n * Q is the point at infinity */
	CK( EC_POINT_mul( ecCTX, q, NULL, q, n, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( !EC_POINT_is_at_infinity( ecCTX, q ) )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

/* Perform validity checks on the private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkECCPrivateKey( INOUT PKC_INFO *pkcInfo )
	{
	BIGNUM *p = &pkcInfo->eccParam_p, *d = &pkcInfo->eccParam_d;
	BIGNUM *tmp1 = &pkcInfo->tmp1, *tmp2 = &pkcInfo->tmp2;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *q = pkcInfo->tmpPoint;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	/* Verify that the private key parameter sizes are valid:

		d >= MIN_PKCSIZE_ECC, d <= p - 1 */
	if( !checkComponentLength( d, p, FALSE ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Verify that Q = d * G */
	CK( EC_POINT_mul( ecCTX, q, d, NULL, NULL, pkcInfo->bnCTX ) );
	CK( EC_POINT_get_affine_coordinates_GFp( ecCTX, q, tmp1, tmp2,
											 pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( BN_cmp( tmp1, &pkcInfo->eccParam_qx ) != 0 || \
		BN_cmp( tmp2, &pkcInfo->eccParam_qy ) != 0 )
		return( CRYPT_ARGERROR_STR1 );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Generate/Initialise an ECC Key					*
*																			*
****************************************************************************/

/* Initialise the mass of variables required by the ECC operations.  
   Unfortunately there's no ECC analogue of BN_init() so we have to 
   dynamically allocate values.  In any case this is probably a better 
   tradeoff since the EC_GROUP structure is huge and would blow out the 
   size of the PKC_INFO for all other PKC contexts */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initECCVariables( INOUT PKC_INFO *pkcInfo )
	{
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	EC_POINT *ecPoint = pkcInfo->ecPoint;
	int bnStatus = BN_STATUS;

	assert( isWritePtr( pkcInfo, sizeof( PKC_INFO ) ) );

	CK( EC_GROUP_set_curve_GFp( ecCTX, &pkcInfo->eccParam_p, 
								&pkcInfo->eccParam_a, &pkcInfo->eccParam_b, 
								pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	CK( EC_POINT_set_affine_coordinates_GFp( ecCTX, ecPoint, 
											 &pkcInfo->eccParam_gx, 
											 &pkcInfo->eccParam_gy, 
											 pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	CK( EC_GROUP_set_generator( ecCTX, ecPoint, &pkcInfo->eccParam_n, 
								&pkcInfo->eccParam_h ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );

	return( CRYPT_OK );
	}

/* Generate and check a generic ECC key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int generateECCkey( INOUT CONTEXT_INFO *contextInfoPtr, 
					IN_LENGTH_SHORT_MIN( MIN_PKCSIZE_ECC * 8 ) \
						const int keyBits )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	CRYPT_ECCCURVE_TYPE fieldID;
	int keySizeBits, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( keyBits >= bytesToBits( MIN_PKCSIZE_ECC ) && \
			  keyBits <= bytesToBits( CRYPT_MAX_PKCSIZE_ECC ) );

	/* Find the fieldID matching the requested key size.  This gets a bit
	   complicated because with fixed-parameter curves the key size is taken 
	   to indicate the closest matching curve size (if we didn't do this and
	   required that the caller specify exact sizes to match the predefined
	   curves then they'd end up having to play guessing games to match
	   byte-valued key sizes to oddball curve sizes like P521).  To handle
	   this we first map the key size to the matching curve, and then 
	   retrieve the actual key size in bits from the ECC parameter data */
	status = getECCFieldID( bitsToBytes( keyBits ), &fieldID );
	if( cryptStatusError( status ) )
		return( status );
	pkcInfo->curveType = fieldID;
	status = loadECCparams( contextInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	keySizeBits = pkcInfo->keySizeBits;

	/* Initialise the mass of variables required by the ECC operations */
	status = initECCVariables( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Generate the private key */
	status = generateECCPrivateValue( pkcInfo, keySizeBits );
	if( cryptStatusError( status ) )
		return( status );

	/* Calculate the public-key value Q = d * G */
	status = generateECCPublicValue( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the generated values are valid */
	status = checkECCDomainParameters( pkcInfo, TRUE );
	if( cryptStatusOK( status ) )
		status = checkECCPublicKey( pkcInfo );
	if( cryptStatusOK( status ) )
		status = checkECCPrivateKey( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, 
							contextInfoPtr->capabilityInfo->cryptAlgo ) );
	}

/* Initialise and check an ECC key.  If the isECDH flag is set then it's an 
   ECDH key and we generate the d value (and by extension the Q value) if 
   it's not present */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCheckECCkey( INOUT CONTEXT_INFO *contextInfoPtr,
					 const BOOLEAN isECDH )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	EC_GROUP *ecCTX = pkcInfo->ecCTX;
	BIGNUM *p = &pkcInfo->eccParam_p;
	const BOOLEAN isPrivateKey = \
		( contextInfoPtr->flags & CONTEXT_FLAG_ISPUBLICKEY ) ? FALSE : TRUE;
	BOOLEAN generatedD = FALSE;
	int bnStatus = BN_STATUS, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	/* If we're loading a named curve then the public-key parameters may not 
	   have been set yet, in which case we have to set them before we can
	   continue */
	if( pkcInfo->curveType != CRYPT_ECCCURVE_NONE && \
		BN_is_zero( &pkcInfo->eccParam_p ) )
		{
		status = loadECCparams( contextInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that the necessary key parameters have been initialised */
	if( BN_is_zero( &pkcInfo->eccParam_p ) || \
		BN_is_zero( &pkcInfo->eccParam_a ) || \
		BN_is_zero( &pkcInfo->eccParam_b ) || \
		BN_is_zero( &pkcInfo->eccParam_gx ) || \
		BN_is_zero( &pkcInfo->eccParam_gy ) || \
		BN_is_zero( &pkcInfo->eccParam_n ) )
		return( CRYPT_ARGERROR_STR1 );
	if( !isECDH )
		{
		if( BN_is_zero( &pkcInfo->eccParam_qx ) || \
			BN_is_zero( &pkcInfo->eccParam_qy ) )
			return( CRYPT_ARGERROR_STR1 );
		if( isPrivateKey && BN_is_zero( &pkcInfo->eccParam_d ) )
			return( CRYPT_ARGERROR_STR1 );
		}

	/* Make sure that the domain parameters are valid */
	status = checkECCDomainParameters( pkcInfo, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Initialise the mass of variables required by the ECC operations */
	status = initECCVariables( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Additional verification for ECC domain parameters, verify that 
	   n * G is the point at infinity.  We have to do this at this point 
	   rather than in checkECCDomainParameters() because it requires 
	   initialisation of values that haven't been set up yet when 
	   checkECCDomainParameters() is called */
	CK( EC_POINT_mul( ecCTX, pkcInfo->tmpPoint, &pkcInfo->eccParam_n, 
					  NULL, NULL, pkcInfo->bnCTX ) );
	if( bnStatusError( bnStatus ) )
		return( getBnStatus( bnStatus ) );
	if( !EC_POINT_is_at_infinity( ecCTX, pkcInfo->tmpPoint ) )
		return( CRYPT_ARGERROR_STR1 );

	/* If it's an ECDH key and there's no d value present, generate one 
	   now.  This is needed because all ECDH keys are effectively private 
	   keys.  We also update the context flags to reflect this change in 
	   status */
	if( isECDH && BN_is_zero( &pkcInfo->eccParam_d ) )
		{
		status = generateECCPrivateValue( pkcInfo, pkcInfo->keySizeBits );
		if( cryptStatusError( status ) )
			return( status );
		contextInfoPtr->flags &= ~CONTEXT_FLAG_ISPUBLICKEY;
		generatedD = TRUE;
		}

	/* Some sources (specifically PKCS #11) don't make Q available for
	   private keys so if the caller is trying to load a private key with a
	   zero Q value we calculate it for them.  First, we check to make sure
	   that we have d available to calculate Q */
	if( BN_is_zero( &pkcInfo->eccParam_qx ) && \
		BN_is_zero( &pkcInfo->eccParam_d ) )
		return( CRYPT_ARGERROR_STR1 );

	/* Calculate Q if required.  This is a bit odd because if we've 
	   generated a new d value it'll cause any existing Q value to be 
	   overwritten by a new one based on the newly-generated Q value.  This 
	   means that if we load an ECDH key from a certificate containing an 
	   existing Q value then this process will overwrite the value with a 
	   new one, so that the context associated with the certificate will 
	   contain a Q value that differs from the one in the certificate.  This 
	   is unfortunate, but again because of the ECDH key duality we need to 
	   have both a d and a Q value present otherwise the key is useless and 
	   in order to get a d value we have to recreate the Q value */
	if( BN_is_zero( &pkcInfo->eccParam_qx ) || generatedD )
		{
		status = generateECCPublicValue( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Make sure that the public key is valid */
	status = checkECCPublicKey( pkcInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the private key is valid */
	if( isPrivateKey || generatedD )
		{
		status = checkECCPrivateKey( pkcInfo );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* ECCs are somewhat weird in that the nominal key size is defined by
	   executive fiat based on the curve type.  For named curves this is 
	   fairly simple but for curves loaded as raw parameters we have to
	   recover the nominal value from the ECC p parameter */
	if( pkcInfo->keySizeBits <= 0 )
		pkcInfo->keySizeBits = BN_num_bits( p );

	/* Enable side-channel protection if required */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_SIDECHANNELPROTECTION ) )
		return( CRYPT_OK );
	return( enableSidechannelProtection( pkcInfo, 
							contextInfoPtr->capabilityInfo->cryptAlgo ) );
	}
#endif /* USE_ECDH || USE_ECDSA */
