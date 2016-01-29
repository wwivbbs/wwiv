/****************************************************************************
*																			*
*					ASN.1 AlgorithmIdentifier OID Tables					*
*					Copyright Peter Gutmann 1992-2011						*
*																			*
****************************************************************************/

/* A table mapping OID's to algorithm types.  We take advantage of the fact
   that object identifiers were designed to be handled in the encoded form
   (without any need for decoding) and compare expected OID's with the raw
   encoded form.  Some OID's are for pure algorithms, others are for aWithB
   type combinations (usually encryption + hash).  As a variation on this,
   some algorithms have two (or even more parameters), for example 3DES
   encryption + CBC mode, or AES encryption + CBC mode + 128-bit key size.
   In order to deal with this we allow two parameters alongside the main
   algorithm:

	Signature: param1 = hash algorithm.
	Encryption: param1 = mode, param2 = key size (if required).
	Hash: param1 = block size (if required).

   In order for the table to work for encoding, the values have to be sorted 
   based on their parameters, so that a given algorithm is followed by its
   sub-OIDs sorted first on param1 and then param2.

   There are multiple OID's for RSA, the main ones being rsa (which doesn't
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
	const int param1, param2;			/* The algorithm subtype or mode */
	const ALGOID_CLASS_TYPE algoClass;	/* Algorithm class */
	const BYTE FAR_BSS *oid;			/* The OID for this algorithm */
	} ALGOID_INFO;

static const ALGOID_INFO FAR_BSS algoIDinfoTbl[] = {
	/* RSA and <hash>WithRSA */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01" ) },
	  /* rsaEncryption (1 2 840 113549 1 1 1) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_MD5, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x04" ) },
	  /* md5withRSAEncryption (1 2 840 113549 1 1 4) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05" ) },
	  /* sha1withRSAEncryption (1 2 840 113549 1 1 5) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05" ) },
	  /* Bug workaround for implementations that erroneously use
	     xxxWithRSA when they should be using straight RSA */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1D" ) },
	/* Another sha-1WithRSAEncryption (1 3 14 3 2 29) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x06\x2B\x24\x03\x03\x01\x01" ) },
	  /* Another rsaSignatureWithsha1 (1 3 36 3 3 1 1) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 32, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0B" ) },
	  /* sha256withRSAEncryption (1 2 840 113549 1 1 11) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_NONE, 32, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0B" ) },
	  /* Bug workaround for implementations that erroneously use
	     xxxWithRSA when they should be using straight RSA */
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 48, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0C" ) },
	  /* sha384withRSAEncryption (1 2 840 113549 1 1 12) */
	{ CRYPT_ALGO_RSA, CRYPT_ALGO_SHA2, 64, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x0D" ) },
	  /* sha512withRSAEncryption (1 2 840 113549 1 1 13) */
  #endif /* USE_SHA2_EXT */

	/* DSA and dsaWith<hash> */
#ifdef USE_DSA
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x01" ) },
	  /* dsa (1 2 840 10040 4 1) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x0C" ) },
	  /* Peculiar deprecated dsa (1 3 14 3 2 12), but used by CDSA and the
	     German PKI profile */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x03" ) },
	  /* dsaWithSha1 (1 2 840 10040 4 3) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x38\x04\x03" ) },
	  /* Bug workaround for implementations that erroneously use
	     dsaWithXXX when they should be using straight DSA */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1B" ) },
	  /* Another dsaWithSHA1 (1 3 14 3 2 27) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x02\x01\x01\x02" ) },
	  /* Yet another dsaWithSHA-1 (2 16 840 1 101 2 1 1 2) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x0D" ) },
	  /* When they ran out of valid dsaWithSHA's, they started using invalid
	     ones.  This one is from JDK 1.1 and is actually dsaWithSHA(-0), but 
		 it's used as if it were dsaWithSHA-1 (1 3 14 3 2 13) */
	{ CRYPT_ALGO_DSA, CRYPT_ALGO_SHA2, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x03\x02" ) },
	  /* dsaWithSha256 (2 16 840 1 101 3 4 3 2) */
#endif /* USE_DSA */

	/* Elgamal */
#ifdef USE_ELGAMAL
	{ CRYPT_ALGO_ELGAMAL, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x0A\x2B\x06\x01\x04\x01\x97\x55\x01\x02\x01" ) },
	  /* elgamal (1 3 6 1 4 1 3029 1 2 1) */
#endif /* USE_ELGAMAL */

	/* DH */
#ifdef USE_DH
	{ CRYPT_ALGO_DH, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3E\x02\x01" ) },
	  /* dhPublicKey (1 2 840 10046 2 1) */
#endif /* USE_DH */

	/* ECDSA and ecdsaWith<hash>.  The xxxWithRecommended/Specified are a
	   complex mess and aren't normally used by anything (for example they
	   were explicitly excluded from the PKIX/SMIME specs because there's no
	   point to them) but due to the fact that there's no public use of ECC
	   certs by CAs there are oddball private-label CAs that use them with
	   no correcting factor in the form of rejection by implementations 
	   present.  For now we ignore them in the hope that the oddball private
	   uses will eventually go away */
#ifdef USE_ECDSA
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x02\x01" ) },
	  /* ecPublicKey (1 2 840 10045 2 1) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x01" ) },
	  /* ecdsaWithSHA1 (1 2 840 10045 4 1) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x01" ) },
	  /* Bug workaround for implementations that erroneously use
	     ecdsaWithXXX when they should be using straight ECDSA */
  #if 0		/* These are too awkward to support easily, and PKIX says they 
			   shouldn't be used anyway */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x02" ) },
	  /* ecdsaWithRecommended (= ...withSHA1) (1 2 840 10045 4 2) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA1, 0, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x04\x03" ) },
	  /* ecdsaWithSpecified (= ...withSHA1) (1 2 840 10045 4 3) */
  #endif /* 0 */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 32, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x02" ) },
	  /* ecdsaWithSHA256 (1 2 840 10045 4 3 2) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 32, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x02" ) },
	  /* Bug workaround for implementations that erroneously use
	     ecdsaWithXXX when they should be using straight ECDSA */
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 48, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x03" ) },
	  /* ecdsaWithSHA384 (1 2 840 10045 4 3 3) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 48, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x03" ) },
	  /* Bug workaround for implementations that erroneously use
	     ecdsaWithXXX when they should be using straight ECDSA */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 64, ALGOID_CLASS_PKCSIG,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x04" ) },
	  /* ecdsaWithSHA512 (1 2 840 10045 4 3 4) */
	{ CRYPT_ALGO_ECDSA, CRYPT_ALGO_SHA2, 64, ALGOID_CLASS_PKC,
	  MKOID( "\x06\x08\x2A\x86\x48\xCE\x3D\x04\x03\x04" ) },
	  /* Bug workaround for implementations that erroneously use
	     ecdsaWithXXX when they should be using straight ECDSA */
  #endif /* USE_SHA2_EXT */
#endif /* USE_ECDSA */

	/* Hash algorithms */
#ifdef USE_MD5
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x05" ) },
	  /* md5 (1 2 840 113549 2 5) */
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x02\x82\x06\x01\x0A\x01\x03\x02" ) },
	  /* Another md5 (0 2 262 1 10 1 3 2) */
	{ CRYPT_ALGO_MD5, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x0B\x60\x86\x48\x01\x86\xF8\x37\x01\x02\x08\x32" ) },
	  /* Yet another md5 (2 16 840 1 113719 1 2 8 50) */
#endif /* USE_MD5 */
	{ CRYPT_ALGO_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x1A" ) },
	  /* sha1 (1 3 14 3 2 26) */
	{ CRYPT_ALGO_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x0B\x60\x86\x48\x01\x86\xF8\x37\x01\x02\x08\x52" ) },
	  /* Another sha1 (2 16 840 1 113719 1 2 8 82) */
	{ CRYPT_ALGO_SHA2, 32, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01" ) },
	  /* sha2-256 (2 16 840 1 101 3 4 2 1) */
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_SHA2, 48, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x02" ) },
	  /* sha2-384 (2 16 840 1 101 3 4 2 2) */
	{ CRYPT_ALGO_SHA2, 64, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x03" ) },
	  /* sha2-512 (2 16 840 1 101 3 4 2 3) */
  #endif /* USE_SHA2_EXT */

	/* MAC algorithms */
	{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2B\x06\x01\x05\x05\x08\x01\x02" ) },
	  /* hmac-SHA (1 3 6 1 5 5 8 1 2) */
	{ CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x07" ) },
	  /* Another hmacWithSHA1 (1 2 840 113549 2 7) */
	{ CRYPT_ALGO_HMAC_SHA2, 32, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x09" ) },
	  /* hmacWithSHA256 (1 2 840 113549 2 9) */
  #ifdef USE_SHA2_EXT
	{ CRYPT_ALGO_HMAC_SHA2, 48, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x0A" ) },
	  /* hmacWithSHA384 (1 2 840 113549 2 10) */
	{ CRYPT_ALGO_HMAC_SHA2, 64, 0, ALGOID_CLASS_HASH,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x02\x0B" ) },
	  /* hmacWithSHA512 (1 2 840 113549 2 11) */
  #endif /* USE_SHA2_EXT */

	/* Encryption algorithms */
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x01" ) },
	  /* aes128-ECB (2 16 840 1 101 3 4 1 1) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x15" ) },
	  /* aes192-ECB (2 16 840 1 101 3 4 1 21) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_ECB, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x29" ) },
	  /* aes256-ECB (2 16 840 1 101 3 4 1 41) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x02" ) },
	  /* aes128-CBC (2 16 840 1 101 3 4 1 2) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x16" ) },
	  /* aes192-CBC (2 16 840 1 101 3 4 1 22) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CBC, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x2A" ) },
	  /* aes256-CBC (2 16 840 1 101 3 4 1 42) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 16, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x04" ) },
	  /* aes128-CFB (2 16 840 1 101 3 4 1 4) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 24, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x18" ) },
	  /* aes192-CFB (2 16 840 1 101 3 4 1 24) */
	{ CRYPT_ALGO_AES, CRYPT_MODE_CFB, 32, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x60\x86\x48\x01\x65\x03\x04\x01\x2C" ) },
	  /* aes256-CFB (2 16 840 1 101 3 4 1 44) */
#ifdef USE_CAST
	{ CRYPT_ALGO_CAST, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x2A\x86\x48\x86\xF6\x7D\x07\x42\x0A" ) },
	  /* cast5CBC (1 2 840 113533 7 66 10) */
#endif /* USE_CAST */
	{ CRYPT_ALGO_DES, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x06" ) },
	  /* desECB (1 3 14 3 2 6) */
	{ CRYPT_ALGO_DES, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x01" ) },
	  /* Another desECB (0 2 262 1 10 1 2 2 1) */
	{ CRYPT_ALGO_DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x07" ) },
	  /* desCBC (1 3 14 3 2 7) */
	{ CRYPT_ALGO_DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x02" ) },
	  /* Another desCBC (0 2 262 1 10 1 2 2 2) */
	{ CRYPT_ALGO_DES, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x05\x2B\x0E\x03\x02\x09" ) },
	  /* desCFB (1 3 14 3 2 9) */
	{ CRYPT_ALGO_DES, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x02\x05" ) },
	  /* Another desCFB (0 2 262 1 10 1 2 2 5) */
	{ CRYPT_ALGO_3DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x07" ) },
	  /* des-EDE3-CBC (1 2 840 113549 3 7) */
	{ CRYPT_ALGO_3DES, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x09\x02\x82\x06\x01\x0A\x01\x02\x03\x02" ) },
	  /* Another des3CBC (0 2 262 1 10 1 2 3 2) */
#ifdef USE_RC2
	{ CRYPT_ALGO_RC2, CRYPT_MODE_CBC, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x02" ) },
	  /* rc2CBC (1 2 840 113549 3 2) */
	{ CRYPT_ALGO_RC2, CRYPT_MODE_ECB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x03" ) },
	  /* rc2ECB (1 2 840 113549 3 3) */
#endif /* USE_RC2 */
#ifdef USE_RC4
	{ CRYPT_ALGO_RC4, CRYPT_MODE_CFB, 0, ALGOID_CLASS_CRYPT,
	  MKOID( "\x06\x08\x2A\x86\x48\x86\xF7\x0D\x03\x04" ) },
	  /* rc4 (1 2 840 113549 3 4) */
#endif /* USE_RC4 */

	/* Authenticated encryption algorithms */
	{ CRYPT_IALGO_GENERIC_SECRET, 16, 0, ALGOID_CLASS_AUTHENC,
	  MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x0F" ) },
	  /* authEnc128 (1 2 840 113549 1 9 16 3 15) */
	{ CRYPT_IALGO_GENERIC_SECRET, 32, 0, ALGOID_CLASS_AUTHENC,
	  MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x09\x10\x03\x10" ) },
	  /* authEnc128 (1 2 840 113549 1 9 16 3 16) */

	{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_NONE, NULL },
		{ CRYPT_ALGO_NONE, CRYPT_ALGO_NONE, 0, ALGOID_CLASS_NONE, NULL }
	};
