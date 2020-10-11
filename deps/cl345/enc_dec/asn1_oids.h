/****************************************************************************
*																			*
*					ASN.1 AlgorithmIdentifier OID Tables					*
*					Copyright Peter Gutmann 1992-2011						*
*																			*
****************************************************************************/

/* A table mapping OIDs to algorithm types.  We take advantage of the fact
   that object identifiers were designed to be handled in the encoded form
   (without any need for decoding) and compare expected OIDs with the raw
   encoded form.  Some OIDs are for pure algorithms, others are for aWithB
   type combinations (usually encryption + hash).  As a variation on this,
   some algorithms have two (or even more parameters), for example 3DES
   encryption + CBC mode, or AES encryption + CBC mode + 128-bit key size.
   In order to deal with this we allow two parameters alongside the main
   algorithm:

	Signature: param1 = hash algorithm.
	Encryption (PKC): param1 = encoding mech (if required).
	Encryption (conv): param1 = mode, param2 = key size (if required).
	Hash: param1 = block size (if required).

   In order for the table to work for encoding, the values have to be sorted 
   based on their parameters, so that a given algorithm is followed by its
   sub-OIDs sorted first on param1 and then param2.

   There are multiple OIDs for RSA, the main ones being rsa (which doesn't
   specify an exact data format and is deprecated), rsaEncryption (as per
   PKCS #1, recommended), and rsaSignature (ISO 9796).  We use rsaEncryption
   and its derived forms (e.g. md5WithRSAEncryption) rather than alternatives
   like md5WithRSA.  There is also an OID for rsaKeyTransport that uses
   PKCS #1 padding but isn't defined by RSADSI.

   There are a great many OIDs for DSA and/or SHA.  We list the less common
   ones after all the other OIDs so that we always encode the more common
   form, but can decode many forms (there are even more OIDs for SHA or DSA
   with common parameters that we don't bother with).

   AES has a whole series of OIDs that vary depending on the key size used,
   this usually isn't of any use since we can tell the keysize from other 
   places but is needed in a few situations such as when we're deriving a 
   key into an AES context */

typedef struct {
	const CRYPT_ALGO_TYPE algorithm;	/* The basic algorithm */
	const int subAlgo;					/* Algorithm subtype or mode */
	const int parameter;				/* Encoding format or key/hash size */
	const ALGOID_CLASS_TYPE algoClass;	/* Algorithm class */
	const BYTE *oid;					/* The OID for this algorithm */
#ifndef NDEBUG
	const char *description;			/* Description for this algorithm */
#endif /* !NDEBUG */
	} ALGOID_INFO;

#ifndef NDEBUG
  #define MKDESC( description )		, description
#else
  #define MKDESC( description )
#endif /* !NDEBUG */

static const ALGOID_INFO algoIDinfoTbl[] = {
	/* RSA and <hash>WithRSA */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01" )
	  MKDESC( "rsaEncryption (1 2 840 113549 1 1 1)" ) },
#ifdef USE_OAEP
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_OAEP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x07" )
	  MKDESC( "rsaOAEP (1 2 840 113549 1 1 7" ) },
#endif /* USE_OAEP */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_MD5, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x04" )
	  MKDESC( "md5withRSAEncryption (1 2 840 113549 1 1 4)" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05" )
	  MKDESC( "sha1withRSAEncryption (1 2 840 113549 1 1 5)" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1D" )
	  MKDESC( "Another sha-1WithRSAEncryption (1 3 14 3 2 29)" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x06\x2B\x24\x03\x03\x01\x01" ) 
	  MKDESC( "Another rsaSignatureWithsha1 (1 3 36 3 3 1 1)" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 32, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0B" )
	  MKDESC( "sha256withRSAEncryption (1 2 840 113549 1 1 11)" ) },
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 48, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0C" )
	  MKDESC( "sha384withRSAEncryption (1 2 840 113549 1 1 12)" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 64, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0D" )
	  MKDESC( "sha512withRSAEncryption (1 2 840 113549 1 1 13)" ) },
  #endif /* USE_SHA2_EXT */
	/* The following four ALGOID_CLASS_PKC entries are bug workarounds for 
	   implementations that erroneously use xxxWithRSA when they should be 
	   using straight RSA */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05" )
	  MKDESC( "Bug workaround for implementations using sha1WithRSA instead of RSA" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0B" )
	  MKDESC( "Bug workaround for implementations using sha256WithRSA instead of RSA" ) },
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0C" )
	  MKDESC( "Bug workaround for implementations using sha384WithRSA instead of RSA" ) },
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0D" )
	  MKDESC( "Bug workaround for implementations using sha512WithRSA instead of RSA" ) },
  #endif /* USE_SHA2_EXT */

	/* DSA and dsaWith<hash> */
#ifdef USE_DSA
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x01" ) 
	  MKDESC( "dsa (1 2 840 10040 4 1)" ) },
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x0C" ) 
	  MKDESC( "Peculiar deprecated dsa (1 3 14 3 2 12)" ) },
	  /* Peculiar deprecated dsa (1 3 14 3 2 12), but used by CDSA and the
	     German PKI profile */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x03" )
	  MKDESC( "dsaWithSha1 (1 2 840 10040 4 3)" ) },
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1B" )
	  MKDESC( "Another dsaWithSHA1 (1 3 14 3 2 27)" ) },
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x02\x01\x01\x02" )
	  MKDESC( "Yet another dsaWithSHA-1 (2 16 840 1 101 2 1 1 2)" ) },
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x0D" ) 
	  MKDESC( "JDK 1.1 erroneous dsaWithSHA(-0) used as dsaWithSHA-1" ) },
	  /* When they ran out of valid dsaWithSHA's, they started using invalid
	     ones.  This one is from JDK 1.1 and is actually dsaWithSHA(-0), but 
		 it's used as if it were dsaWithSHA-1 (1 3 14 3 2 13) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA2, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x03\x02" ) 
	  MKDESC( "dsaWithSha256 (2 16 840 1 101 3 4 3 2)" ) },
#endif /* USE_DSA */

	/* Elgamal */
#ifdef USE_ELGAMAL
	{ CRYPT_ALGO_ELGAMAL, CRYPT_ALGO_NONE, ALGOID_ENCODING_PKCS1, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x01\x02\x01" )
	  MKDESC( "elgamal (1 3 6 1 4 1 3029 1 2 1)" ) },
#endif /* USE_ELGAMAL */

	/* DH */
#ifdef USE_DH
	{ CRYPT_ALGO_DH, CRYPT_ALGO_NONE, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3E\x02\x01" )
	  MKDESC( "dhPublicKey (1 2 840 10046 2 1)" ) },
#endif /* USE_DH */

	/* ECDSA and ecdsaWith<hash>.  The xxxWithRecommended/Specified are a
	   complex mess and aren't normally used by anything (for example they
	   were explicitly excluded from the PKIX/SMIME specs because there's no
	   point to them) but due to the fact that there's no public use of ECC
	   certs by CAs there are oddball private-label CAs that use them with
	   no correcting factor in the form of rejection by implementations 
	   present.  For now we ignore them in the hope that the oddball private
	   uses will eventually go away */
#if defined( USE_ECDSA ) || defined( USE_ECDH )
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_NONE, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x02\x01" )
	  MKDESC( "ecPublicKey (1 2 840 10045 2 1)" ) },
#endif /* USE_ECDSA || USE_ECDH */
#ifdef USE_ECDSA
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x01" )
	  MKDESC( "ecdsaWithSHA1 (1 2 840 10045 4 1)" ) },
  #if 0		/* These are too awkward to support easily, and PKIX says they 
			   shouldn't be used anyway */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x02" )
	  MKDESC( "ecdsaWithRecommended (= ...withSHA1) (1 2 840 10045 4 2)" ) },
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, ALGOID_ENCODING_DLP, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x03" ) 
	  MKDESC( "ecdsaWithSpecified (= ...withSHA1) (1 2 840 10045 4 3)" ) },
  #endif /* 0 */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 32, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x02" )
	  MKDESC( "ecdsaWithSHA256 (1 2 840 10045 4 3 2)" ) },
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 48, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x03" )
	  MKDESC( "ecdsaWithSHA384 (1 2 840 10045 4 3 3)" ) },
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 64, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x04" )
	  MKDESC( "ecdsaWithSHA512 (1 2 840 10045 4 3 4)" ) },
  #endif /* USE_SHA2_EXT */
#endif /* USE_ECDSA */

	/* Hash algorithms */
#ifdef USE_MD5
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x05" )
	  MKDESC( "md5 (1 2 840 113549 2 5)" ) },
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x02\x82\x06\x01\x0A\x01\x03\x02" ) 
	  MKDESC( "Another md5 (0 2 262 1 10 1 3 2)" ) },
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x0B\x60\x86\x48\x01\x86\xF8\x37\x01\x02\x08\x32" )
	  MKDESC( "Yet another md5 (2 16 840 1 113719 1 2 8 50)" ) },
#endif /* USE_MD5 */
	{ CRYPT_ALGO_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1A" )
	  MKDESC( "sha1 (1 3 14 3 2 26)" ) },
	{ CRYPT_ALGO_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x0B\x60\x86\x48\x01\x86\xF8\x37\x01\x02\x08\x52" )
	  MKDESC( "Another sha1 (2 16 840 1 113719 1 2 8 82)" ) },
	{ CRYPT_ALGO_SHA2, CRYPT_ALGO_NONE, 32, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01" )
	  MKDESC( "sha2-256 (2 16 840 1 101 3 4 2 1)" ) },
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_SHA2, CRYPT_ALGO_NONE, 48, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x02" )
	  MKDESC( "sha2-384 (2 16 840 1 101 3 4 2 2)" ) },
	{ CRYPT_ALGO_SHA2, CRYPT_ALGO_NONE, 64, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x03" )
	  MKDESC( "sha2-512 (2 16 840 1 101 3 4 2 3)" ) },
  #endif /* USE_SHA2_EXT */

	/* MAC algorithms */
	{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x08\x01\x02" )
	  MKDESC( "hmac-SHA (1 3 6 1 5 5 8 1 2)" ) },
	{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x07" )
	  MKDESC( "Another hmacWithSHA1 (1 2 840 113549 2 7)" ) },
	{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_NONE, 32, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x09" )
	  MKDESC( "hmacWithSHA256 (1 2 840 113549 2 9)" ) },
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_NONE, 48, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x0A" )
	  MKDESC( "hmacWithSHA384 (1 2 840 113549 2 10)" ) },
	{ CRYPT_ALGO_HMAC_SHA2, CRYPT_ALGO_NONE, 64, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x0B" )
	  MKDESC( "hmacWithSHA512 (1 2 840 113549 2 11)" ) },
  #endif /* USE_SHA2_EXT */

	/* Encryption algorithms */
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x01" )
	  MKDESC( "aes128-ECB (2 16 840 1 101 3 4 1 1)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x15" )
	  MKDESC( "aes192-ECB (2 16 840 1 101 3 4 1 21)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x29" )
	  MKDESC( "aes256-ECB (2 16 840 1 101 3 4 1 41)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x02" )
	  MKDESC( "aes128-CBC (2 16 840 1 101 3 4 1 2)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x16" )
	  MKDESC( "aes192-CBC (2 16 840 1 101 3 4 1 22)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x2A" )
	  MKDESC( "aes256-CBC (2 16 840 1 101 3 4 1 42)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x04" )
	  MKDESC( "aes128-CFB (2 16 840 1 101 3 4 1 4)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x18" )
	  MKDESC( "aes192-CFB (2 16 840 1 101 3 4 1 24)" ) },
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x2C" )
	  MKDESC( "aes256-CFB (2 16 840 1 101 3 4 1 44)" ) },
#ifdef USE_CAST
	{ CRYPT_ALGO_CAST, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF6\x7D\x07\x42\x0A" )
	  MKDESC( "cast5CBC (1 2 840 113533 7 66 10)" ) },
#endif /* USE_CAST */
#ifdef USE_DES
	{ CRYPT_ALGO_DES, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x06" )
	  MKDESC( "desECB (1 3 14 3 2 6)" ) },
	{ CRYPT_ALGO_DES, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x01" )
	  MKDESC( "Another desECB (0 2 262 1 10 1 2 2 1)" ) },
	{ CRYPT_ALGO_DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x07" )
	  MKDESC( "desCBC (1 3 14 3 2 7)" ) },
	{ CRYPT_ALGO_DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x02" ) 
	  MKDESC( "Another desCBC (0 2 262 1 10 1 2 2 2)" ) },
	{ CRYPT_ALGO_DES, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x09" )
	  MKDESC( "desCFB (1 3 14 3 2 9)" ) },
	{ CRYPT_ALGO_DES, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x05" )
	  MKDESC( "Another desCFB (0 2 262 1 10 1 2 2 5)" ) },
#endif /* USE_DES */
	{ CRYPT_ALGO_3DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x07" )
	  MKDESC( "des-EDE3-CBC (1 2 840 113549 3 7)" ) },
	{ CRYPT_ALGO_3DES, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x09" )
	  MKDESC( "des-EDE3-CFB (1 2 840 113549 3 9)" ) },
	{ CRYPT_ALGO_3DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x03\x02" )
	  MKDESC( "Another des3CBC (0 2 262 1 10 1 2 3 2)" ) },
#ifdef USE_RC2
	{ CRYPT_ALGO_RC2, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x02" )
	  MKDESC( "rc2CBC (1 2 840 113549 3 2)" ) },
	{ CRYPT_ALGO_RC2, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x03" )
	  MKDESC( "rc2ECB (1 2 840 113549 3 3)" ) },
#endif /* USE_RC2 */
#ifdef USE_RC4
	{ CRYPT_ALGO_RC4, CRYPT_PSEUDOMODE_RC4, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x04" )
	  MKDESC( "rc4 (1 2 840 113549 3 4)" ) },
#endif /* USE_RC4 */

	/* Authenticated encryption algorithms */
	{ CRYPT_IALGO_GENERIC_SECRET, CRYPT_ALGO_NONE, 16, ALGOID_CLASS_AUTHENC,
	  MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x0F" )
	  MKDESC( "authEnc128 (1 2 840 113549 1 9 16 3 15)" ) },
	{ CRYPT_IALGO_GENERIC_SECRET, CRYPT_ALGO_NONE, 32, ALGOID_CLASS_AUTHENC,
	  MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x10" )
	  MKDESC( "authEnc256 (1 2 840 113549 1 9 16 3 16)" ) },

	{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_NONE, NULL MKDESC( "" ) },
		{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_NONE, NULL MKDESC( "" ) }
	};
