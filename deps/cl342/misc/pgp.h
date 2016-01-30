/****************************************************************************
*																			*
*							PGP Definitions Header File						*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

#ifndef _PGP_DEFINED

#define _PGP_DEFINED

#ifndef _STREAM_DEFINED
  #if defined( INC_ALL )
	#include "stream.h"
  #else
	#include "io/stream.h"
  #endif /* Compiler-specific includes */
#endif /* _STREAM_DEFINED */

/* PGP packet types, encoded into the CTB */

typedef enum {
	PGP_PACKET_NONE,			/* No packet type */
	PGP_PACKET_PKE,				/* PKC-encrypted session key */
	PGP_PACKET_SIGNATURE,		/* Signature */
	PGP_PACKET_SKE,				/* Secret-key-encrypted session key */
	PGP_PACKET_SIGNATURE_ONEPASS,/* One-pass signature */
	PGP_PACKET_SECKEY,			/* Secret key */
	PGP_PACKET_PUBKEY,			/* Public key */
	PGP_PACKET_SECKEY_SUB,		/* Secret key subkey */
	PGP_PACKET_COPR,			/* Compressed data */
	PGP_PACKET_ENCR,			/* Encrypted data */
	PGP_PACKET_MARKER,			/* Obsolete marker packet */
	PGP_PACKET_DATA,			/* Raw data */
	PGP_PACKET_TRUST,			/* Trust information */
	PGP_PACKET_USERID,			/* Userid */
	PGP_PACKET_PUBKEY_SUB,		/* Public key subkey */
	PGP_PACKET_DUMMY1, PGP_PACKET_DUMMY2,	/* 15, 16 unused */
	PGP_PACKET_USERATTR,		/* User attributes */
	PGP_PACKET_ENCR_MDC,		/* Encrypted data with MDC */
	PGP_PACKET_MDC,				/* MDC */
	PGP_PACKET_LAST				/* Last possible PGP packet type */
	} PGP_PACKET_TYPE;

/* PGP signature subpacket types */

#define PGP_SUBPACKET_TIME	2		/* Signing time */
#define PGP_SUBPACKET_KEYID	16		/* Key ID */
#define PGP_SUBPACKET_TYPEANDVALUE 20	/* Type-and-value pairs */
#define PGP_SUBPACKET_LAST	29		/* Last valid subpacket type */

/* A special-case packet type that denotes a signature that follows on from 
   a one-pass signature packet.  When generating a signature of this type PGP
   splits the information in the normal signature packet across the one-pass
   signature packet and the signature packet itself, so we have to read the 
   data on two parts, with half the information in the one-pass packet and 
   the other half in the signature packet */

#define PGP_PACKET_SIGNATURE_SPECIAL	1000

/* The PGP packet format (via the CTB) is:

	+---------------+
	|7 6 5 4 3 2 1 0|
	+---------------+

	Bit 7: Always one
	Bit 6: OpenPGP (new) format if set

		PGP 2.x:						OpenPGP:
	Bits 5-2: Packet type		Bits 5-0: Packet type
	Bits 1-0: Length type

   All CTBs have the MSB set, and OpenPGP CTBs have the next-to-MSB set.  We 
   also have a special-case CTB that's used for indefinite-length compressed 
   data */

#define PGP_CTB				0x80	/* PGP 2.x CTB template */
#define PGP_CTB_OPENPGP		0xC0	/* OpenPGP CTB template */
#define PGP_CTB_COMPRESSED	0xA3	/* Compressed indef-length data */

/* Macros to extract packet information from the CTB */

#define pgpIsCTB( ctb )				( ( ctb ) & PGP_CTB )
#define pgpGetPacketVersion( ctb ) \
		( ( ( ( ctb ) & PGP_CTB_OPENPGP ) == PGP_CTB_OPENPGP ) ? \
		  PGP_VERSION_OPENPGP : PGP_VERSION_2 )
#define pgpGetPacketType( ctb ) \
		( ( ( ( ctb ) & PGP_CTB_OPENPGP ) == PGP_CTB_OPENPGP ) ? \
			( ( ctb ) & 0x3F ) : ( ( ( ctb ) >> 2 ) & 0x0F ) )
#define pgpIsReservedPacket( type )	( ( type ) >= 60 && ( type ) <= 63 )

/* Version information */

#define PGP_VERSION_2		2		/* Version number byte for PGP 2.0 */
#define PGP_VERSION_3		3		/* Version number byte for legal-kludged PGP 2.0 */
#define PGP_VERSION_OPENPGP	4		/* Version number for OpenPGP */

/* Public-key algorithms */

#define PGP_ALGO_RSA		1		/* RSA */
#define PGP_ALGO_RSA_ENCRYPT 2		/* RSA encrypt-only */
#define PGP_ALGO_RSA_SIGN	3		/* RSA sign-only */
#define PGP_ALGO_ELGAMAL	16		/* ElGamal */
#define PGP_ALGO_DSA		17		/* DSA */
#define PGP_ALGO_ECC_RES	18		/* Reserved for "ECC" */
#define PGP_ALGO_ECDSA_RES	19		/* Reserved for ECDSA */
#define PGP_ALGO_PKC_RES1	20		/* Reserved, formerly Elgamal sign */
#define PGP_ALGO_PKC_RES2	21		/* Reserved for "X9.42" */

/* Conventional encryption algorithms */

#define PGP_ALGO_NONE		0		/* No CKE algorithm */
#define PGP_ALGO_IDEA		1		/* IDEA */
#define PGP_ALGO_3DES		2		/* Triple DES */
#define PGP_ALGO_CAST5		3		/* CAST-128 */
#define PGP_ALGO_BLOWFISH	4		/* Blowfish */
#define PGP_ALGO_ENC_RES1	5		/* Reserved, formerly Safer-SK */
#define PGP_ALGO_ENC_RES2	6		/* Reserved */
#define PGP_ALGO_AES_128	7		/* AES with 128-bit key */
#define PGP_ALGO_AES_192	8		/* AES with 192-bit key */
#define PGP_ALGO_AES_256	9		/* AES with 256-bit key */
#define PGP_ALGO_TWOFISH	10		/* Twofish */

/* Hash algorithms */

#define PGP_ALGO_MD5		1		/* MD5 */
#define PGP_ALGO_SHA		2		/* SHA-1 */
#define PGP_ALGO_RIPEMD160	3		/* RIPEMD-160 */
#define PGP_ALGO_HASH_RES1	4		/* Reserved */
#define PGP_ALGO_HASH_RES2	5		/* Reserved, formerly MD2 */
#define PGP_ALGO_HASH_RES3	6		/* Reserved, formerly Tiger/192 */
#define PGP_ALGO_HASH_RES4	7		/* Reserved, formerly Haval */
#define PGP_ALGO_SHA2_256	8		/* SHA-2 256bit */
#define PGP_ALGO_SHA2_384	9		/* SHA-2 384bit */
#define PGP_ALGO_SHA2_512	10		/* SHA-2 512bit */
#define PGP_ALGO_SHA2_224	11		/* SHA-2 224bit */

/* Compression algorithms */

#define PGP_ALGO_ZIP		1		/* ZIP compression */
#define PGP_ALGO_ZLIB		2		/* zlib compression */
#define PGP_ALGO_BZIP2		3		/* Bzip2 compression */

/* Highest possible algorithm value, for range checking */

#define PGP_ALGO_LAST		PGP_ALGO_DSA

/* S2K specifier */

#define PGP_S2K				0xFF	/* Standard S2K */
#define PGP_S2K_HASHED		0xFE	/* S2K with hashed key */

/* Signature types */

#define PGP_SIG_NONE		0x00	/* Same as PGP_SIG_DATA, for range chk.*/
#define PGP_SIG_DATA		0x00	/* Binary data */
#define PGP_SIG_TEXT		0x01	/* Canonicalised text data */
#define PGP_SIG_STANDALONE	0x02	/* Unknown purpose, from RFC 4880 */
#define	PGP_SIG_CERT0		0x10	/* Key certificate, unknown assurance */
#define	PGP_SIG_CERT1		0x11	/* Key certificate, no assurance */
#define	PGP_SIG_CERT2		0x12	/* Key certificate, casual assurance */
#define	PGP_SIG_CERT3		0x13	/* Key certificate, strong assurance */
#define PGP_SIG_SUBKEY		0x18	/* Subkey binding signature */
#define PGP_SIG_PRIMKEY		0x19	/* Primary key binding signature */
#define PGP_SIG_DIRECTKEY	0x1F	/* Key self-signature */
#define PGP_SIG_KEYREV		0x20	/* Key revocation */
#define PGP_SIG_SUBKEYREV	0x28	/* Subkey revocation */
#define PGP_SIG_CRL			0x30	/* Certificate revocation */
#define	PGP_SIG_TS			0x40	/* Timestamp signature */
#define	PGP_SIG_COUNTERSIG	0x50	/* Third-party countersignature */
#define PGP_SIG_LAST		0x51	/* Last possible signature type */

/* The size of the PGP version ID and algorithm ID */

#define PGP_VERSION_SIZE	1
#define PGP_ALGOID_SIZE		1

/* The maximum size of a PGP user ID.  Note that this is larger than the
   cryptlib-wide maximum user ID size */

#define PGP_MAX_USERIDSIZE	256

/* The size of the salt used for password hashing and the number of 
   setup "iterations".  This isn't a true iteration count but the number of 
   salt+password bytes hashed, and in fact it isn't even that but the
   actual count scaled by dividing it by 64, which is how PGP encodes the
   count in the data packet */

#define PGP_SALTSIZE		8
#define PGP_ITERATIONS		1024

/* Various PGP packet header sizes, used to estimate how much data we still 
   need to process */

#define PGP_MIN_HEADER_SIZE	2		/* CTB + length */
#define PGP_MAX_HEADER_SIZE	6		/* CTB + 0xFF + 4-byte length */
#define PGP_DATA_HEADER		"b\x00\x00\x00\x00\x00"
#define PGP_DATA_HEADER_SIZE ( 1 + 1 + 4 )
#define PGP_MDC_PACKET_SIZE	( 1 + 1 + 20 )	/* Size of MDC packet */

/* Since PGP only provides a subset of cryptlib's algorithm types and uses
   different identifiers, we have to both check that there's a mapping
   possible and map from one to the other.  When going from PGP -> cryptlib
   we specify both the algorithm ID and the algorithm class we expect to 
   find it in to allow type checking */

typedef enum {
	PGP_ALGOCLASS_NONE,		/* No algorithm class */
	PGP_ALGOCLASS_CRYPT,	/* Conventional encryption algorithms */
	PGP_ALGOCLASS_PWCRYPT,	/* Password-based encryption algorithms */
	PGP_ALGOCLASS_PKCCRYPT,	/* PKC algorithms */
	PGP_ALGOCLASS_SIGN,		/* Signature algorithms */
	PGP_ALGOCLASS_HASH,		/* Hash algorithms */
	PGP_ALGOCLASS_LAST		/* Last possible algorithm class */
	} PGP_ALGOCLASS_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int pgpToCryptlibAlgo( IN_RANGE( PGP_ALGO_NONE, 0xFF ) \
							const int pgpAlgo, 
					   IN_ENUM( PGP_ALGOCLASS ) \
							const PGP_ALGOCLASS_TYPE pgpAlgoClass,
					   OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int cryptlibToPgpAlgo( IN_ALGO const CRYPT_ALGO_TYPE cryptlibAlgo,
					   OUT_RANGE( PGP_ALGO_NONE, PGP_ALGO_LAST ) \
							int *pgpAlgo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readPgpAlgo( INOUT STREAM *stream, 
				 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo, 
				 IN_ENUM( PGP_ALGOCLASS ) \
					const PGP_ALGOCLASS_TYPE pgpAlgoClass );

/* Prototypes for functions in pgp_misc.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int pgpPasswordToKey( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
					  IN_LENGTH_SHORT_OPT const int optKeyLength,
					  IN_BUFFER( passwordLength ) const char *password, 
					  IN_LENGTH_SHORT const int passwordLength, 
					  IN_ALGO const CRYPT_ALGO_TYPE hashAlgo, 
					  IN_BUFFER_OPT( saltSize ) const BYTE *salt, 
					  IN_RANGE( 0, CRYPT_MAX_HASHSIZE ) const int saltSize,
					  IN_INT const int iterations );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int pgpProcessIV( IN_HANDLE const CRYPT_CONTEXT iCryptContext, 
				  INOUT_BUFFER_FIXED( ivInfoSize ) BYTE *ivInfo, 
				  IN_RANGE( 8 + 2, CRYPT_MAX_IVSIZE + 2 ) \
						const int ivInfoSize, 
				  IN_LENGTH_IV const int ivDataSize, 
				  IN_HANDLE_OPT const CRYPT_CONTEXT iMdcContext,
				  const BOOLEAN isEncrypt );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int readPgpS2K( INOUT STREAM *stream, 
				OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
				OUT_BUFFER( saltMaxLen, *saltLen ) BYTE *salt, 
				IN_LENGTH_SHORT_MIN( PGP_SALTSIZE ) const int saltMaxLen, 
				OUT_LENGTH_SHORT_Z int *saltLen,
				OUT_INT_SHORT_Z int *iterations );

#endif /* _PGP_DEFINED */
