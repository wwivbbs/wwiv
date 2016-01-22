/****************************************************************************
*																			*
*						Public-key ID Generation Routines					*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#include <stdio.h>
#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "context.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "pgp.h"
#else
  #include "context/context.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKC

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Instantiate static context data from raw encoded public-key data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int initStaticContext( OUT CONTEXT_INFO *staticContextInfo,
							  OUT PKC_INFO *contextData, 
							  const CAPABILITY_INFO *capabilityInfoPtr,
							  IN_BUFFER( publicKeyDataLength ) \
									const void *publicKeyData,
							  IN_LENGTH_SHORT_MIN( MIN_PKCSIZE ) \
									const int publicKeyDataLength )
	{
	STREAM stream;
	int status;

	assert( isWritePtr( staticContextInfo, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( contextData, sizeof( PKC_INFO ) ) );
	assert( isReadPtr( capabilityInfoPtr, sizeof( CAPABILITY_INFO ) ) );
	assert( isReadPtr( publicKeyData, publicKeyDataLength ) );

	REQUIRES( publicKeyDataLength >= MIN_PKCSIZE && \
			  publicKeyDataLength < MAX_INTLENGTH_SHORT );

	/* Initialise a static context to read the key data into */
	status = staticInitContext( staticContextInfo, CONTEXT_PKC, 
								capabilityInfoPtr, contextData, 
								sizeof( PKC_INFO ), NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the key data into the static context and calculate the keyIDs.
	   We can do this now that the data is in a native context rather than 
	   being present only in raw encoded form */
	sMemConnect( &stream, publicKeyData, publicKeyDataLength );
	status = contextData->readPublicKeyFunction( &stream, staticContextInfo, 
												 KEYFORMAT_CERT );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		{
		staticContextInfo->flags |= CONTEXT_FLAG_ISPUBLICKEY;
		status = capabilityInfoPtr->initKeyFunction( staticContextInfo, 
													 NULL, 0 );
		}
	if( cryptStatusError( status ) )
		{
		staticDestroyContext( staticContextInfo );
		return( status );
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*				KeyID-from-Encoded-Data Calculation Routines				*
*																			*
****************************************************************************/

/* Calculate a keyID when the only key data present is a raw encoded
   SubjectPublicKeyInfo record.  This is a bit more complicated than the 
   standard keyID calculation because while the hash-of-SPKI form is rather
   easier to calculate, the other oddball forms aren't since they first 
   require breaking down the SPKI into its components and then re-encoding 
   them in the various ways that we need to calculate the other forms of 
   keyID */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int calculateFlatKeyID( IN_BUFFER( keyInfoSize ) const void *keyInfo, 
							   IN_LENGTH_SHORT_MIN( 16 ) const int keyInfoSize,
							   OUT_BUFFER_FIXED( keyIdMaxLen ) BYTE *keyID, 
							   IN_LENGTH_FIXED( KEYID_SIZE ) const int keyIdMaxLen )
	{
	HASHFUNCTION_ATOMIC hashFunctionAtomic;

	assert( isReadPtr( keyInfo, keyInfoSize ) );
	assert( isWritePtr( keyID, keyIdMaxLen ) );

	REQUIRES( keyInfoSize >= 16 && keyInfoSize < MAX_INTLENGTH_SHORT );
	REQUIRES( keyIdMaxLen == KEYID_SIZE );

	/* Hash the key info to get the key ID */
	getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, NULL );
	hashFunctionAtomic( keyID, keyIdMaxLen, keyInfo, keyInfoSize );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int calculateKeyIDFromEncoded( INOUT CONTEXT_INFO *contextInfoPtr,
									  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CONTEXT_INFO staticContextInfo;
	PKC_INFO staticContextData, *publicKey = contextInfoPtr->ctxPKC;
	const BOOLEAN isPgpAlgo = \
		( cryptAlgo == CRYPT_ALGO_RSA || cryptAlgo == CRYPT_ALGO_DSA || \
		  cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? TRUE : FALSE;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Calculate the keyID for the pre-encoded key data */
	status = calculateFlatKeyID( publicKey->publicKeyInfo, 
								 publicKey->publicKeyInfoSize, 
								 publicKey->keyID, KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );

	/* At this point we're (technically) done, however a few special-case
	   situations require further processing.  These are explained in the
	   code blocks that handle them below */
	if( !( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY ) && !isPgpAlgo )
		return( CRYPT_OK );

	/* If the keys are held externally (e.g. in a crypto device) then 
	   there's no way to tell what the nominal keysize for the context 
	   should be.  In order to determine this we have to parse the key
	   data in order to extract the specific component that defines the
	   key's nominal length.

	   PGP keyIDs present a similar problem because we would in theory 
	   need to decode the flattened key data and then re-encode it in the 
	   format needed to generate the PGP IDs. 
	   
	   While it would be possible to do this with a lot of customised 
	   duplication of code from other parts of key_rd.c and from key_wr.c 
	   it's easier to just create a static public-key context from the 
	   encoded key data and let the standard key-read code take care of it.  
	   On the downside, it requires creation of a static (if not a full) 
	   public-key context just for this purpose */
	switch( cryptAlgo )
		{
#ifdef USE_DH
		case CRYPT_ALGO_DH:
			status = initStaticContext( &staticContextInfo, &staticContextData, 
										getDHCapability(), 
										publicKey->publicKeyInfo, 
										publicKey->publicKeyInfoSize );
			break;
#endif /* USE_DH */

		case CRYPT_ALGO_RSA:
			status = initStaticContext( &staticContextInfo, &staticContextData, 
										getRSACapability(), 
										publicKey->publicKeyInfo, 
										publicKey->publicKeyInfoSize );
			break;

#ifdef USE_DSA
		case CRYPT_ALGO_DSA:
			status = initStaticContext( &staticContextInfo, &staticContextData, 
										getDSACapability(), 
										publicKey->publicKeyInfo, 
										publicKey->publicKeyInfoSize );
			break;
#endif /* USE_DSA */

#ifdef USE_ELGAMAL
		case CRYPT_ALGO_ELGAMAL:
			status = initStaticContext( &staticContextInfo, &staticContextData, 
										getElgamalCapability(), 
										publicKey->publicKeyInfo, 
										publicKey->publicKeyInfoSize );
			break;
#endif /* USE_ELGAMAL */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a non-native context, explicitly set the key size.  For 
	   native contexts this is done by the init-key function but for non-
	   native contexts this function is never called since there are no key 
	   components present to initialise.  Because of this we have to 
	   explicitly copy the key size information from the static native 
	   context that we've created */
	if( contextInfoPtr->flags & CONTEXT_FLAG_DUMMY )
		contextInfoPtr->ctxPKC->keySizeBits = staticContextData.keySizeBits;

	/* If it's a PGP algorithm, copy across any relevant PGP keyIDs */
	if( isPgpAlgo )
		{
		if( staticContextInfo.flags & CONTEXT_FLAG_PGPKEYID_SET )
			{
			memcpy( publicKey->pgp2KeyID, staticContextData.pgp2KeyID, 
					PGP_KEYID_SIZE );
			contextInfoPtr->flags |= CONTEXT_FLAG_PGPKEYID_SET;
			}
		if( staticContextInfo.flags & CONTEXT_FLAG_OPENPGPKEYID_SET )
			{
			memcpy( publicKey->openPgpKeyID, staticContextData.openPgpKeyID, 
					PGP_KEYID_SIZE );
			contextInfoPtr->flags |= CONTEXT_FLAG_OPENPGPKEYID_SET;
			}
		}
	staticDestroyContext( &staticContextInfo );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							KeyID Calculation Routines						*
*																			*
****************************************************************************/

/* Generate a PGP keyID */

#if defined( USE_PGP ) || defined( USE_PGPKEYS )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int calculateOpenPGPKeyID( INOUT CONTEXT_INFO *contextInfoPtr,
								  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	PKC_INFO *publicKey = contextInfoPtr->ctxPKC;
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	STREAM stream;
	BYTE buffer[ ( CRYPT_MAX_PKCSIZE * 4 ) + 50 + 8 ];
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ], packetHeader[ 64 + 8 ];
	int hashSize, length, status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( isPkcAlgo( cryptAlgo ) );

	/* Generate an OpenPGP key ID.  Note that the creation date isn't 
	   necessarily present if the key came from a non-PGP source, in which 
	   case the date will just have a value of zero.  In theory we could 
	   also opportunistically set one on load so there'll be some value 
	   present if the key is used in a PGP context, but that would mean that
	   the key has in a new time being set each time it's instantiated from 
	   non-PGP key data so the same non-PGP key used for PGP purposes would
	   have a different ID each time it was loaded so we just leave the date
	   as an all-zero value which may look a bit odd when viewed but at 
	   least produces a constant ID:

		byte		ctb = 0x99
		byte[2]		length
		-- Key data --
		byte		version = 4
		byte[4]		key generation time 
		byte		algorithm
		byte[]		key data

	  We do this by writing the public key fields to a buffer and creating a 
	  separate PGP public key header, then hashing the two */
	sMemOpen( &stream, buffer, ( CRYPT_MAX_PKCSIZE * 4 ) + 50 );
	status = publicKey->writePublicKeyFunction( &stream, contextInfoPtr, 
												KEYFORMAT_PGP, 
												"public_key", 10 );
	if( cryptStatusError( status ) )
		{
		sMemClose( &stream );
		return( status );
		}
	length = stell( &stream );
	packetHeader[ 0 ] = 0x99;
	packetHeader[ 1 ] = ( length >> 8 ) & 0xFF;
	packetHeader[ 2 ] = length & 0xFF;

	/* Hash the data needed to generate the OpenPGP keyID */
	getHashParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, &hashSize );
	hashFunction( hashInfo, NULL, 0, packetHeader, 1 + 2, 
				  HASH_STATE_START );
	hashFunction( hashInfo, hash, CRYPT_MAX_HASHSIZE, buffer, length, 
				  HASH_STATE_END );
	memcpy( publicKey->openPgpKeyID, hash + hashSize - PGP_KEYID_SIZE, 
			PGP_KEYID_SIZE );
	sMemClose( &stream );
	contextInfoPtr->flags |= CONTEXT_FLAG_OPENPGPKEYID_SET;

	return( CRYPT_OK );
	}
#endif /* USE_PGP || USE_PGPKEYS */

/* Generate a keyID for a PKCS #3 key, which differs slightly from the 
   FIPS 186/X9.42 standard format in that there's no q value present, so we
   have to write a dummy zero value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writePKCS3Key( INOUT STREAM *stream, 
						  const PKC_INFO *dlpKey,
						  IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	const int parameterSize = ( int ) sizeofObject( \
								sizeofBignum( &dlpKey->dlpParam_p ) + \
								3 +		/* INTEGER value 0 */
								sizeofBignum( &dlpKey->dlpParam_g ) );
	const int componentSize = sizeofBignum( &dlpKey->dlpParam_y );
	int totalSize;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( dlpKey, sizeof( PKC_INFO ) ) );

	REQUIRES( isDlpAlgo( cryptAlgo ) );

	/* Implement a cut-down version of writeDlpSubjectPublicKey(), writing a 
	   zero value for q */
	totalSize = sizeofAlgoIDex( cryptAlgo, CRYPT_ALGO_NONE, parameterSize ) + \
				( int ) sizeofObject( componentSize + 1 );
	writeSequence( stream, totalSize );
	writeAlgoIDparam( stream, cryptAlgo, parameterSize );
	writeBignum( stream, &dlpKey->dlpParam_p );
	swrite( stream, "\x02\x01\x00", 3 );	/* Integer value 0 */
	writeBignum( stream, &dlpKey->dlpParam_g );
	writeBitStringHole( stream, componentSize, DEFAULT_TAG );
	return( writeBignum( stream, &dlpKey->dlpParam_y ) );
	}

/* Generate an X.509 key ID, which is the SHA-1 hash of the 
   SubjectPublicKeyInfo.  There are about half a dozen incompatible ways of 
   generating X.509 keyIdentifiers, the following is conformant with the 
   PKIX specification ("use whatever you like as long as it's unique") but 
   differs slightly from one common method that hashes the SubjectPublicKey 
   without the BIT STRING encapsulation.  The problem with that method is 
   that some DLP-based algorithms use a single integer as the 
   SubjectPublicKey, leading to potential key ID clashes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int calculateKeyID( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *publicKey = contextInfoPtr->ctxPKC;
	STREAM stream;
	BYTE buffer[ ( CRYPT_MAX_PKCSIZE * 4 ) + 50 + 8 ];
	const CRYPT_ALGO_TYPE cryptAlgo = contextInfoPtr->capabilityInfo->cryptAlgo;
	int status;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC );

	/* If the public key info is present in pre-encoded form, calculate the
	   key ID directly from that */
	if( publicKey->publicKeyInfo != NULL )
		return( calculateKeyIDFromEncoded( contextInfoPtr, cryptAlgo ) );

	/* Write the public key fields to a buffer and hash them to get the key
	   ID */
	sMemOpen( &stream, buffer, ( CRYPT_MAX_PKCSIZE * 4 ) + 50 );
	if( isDlpAlgo( cryptAlgo ) && BN_is_zero( &publicKey->dlpParam_q ) )
		{
		/* OpenPGP Elgamal keys and SSL/SSH DH keys don't have a q 
		   parameter, which makes it impossible to write them in the X.509 
		   format.  If this situation occurs we write them in a cut-down
		   version of the format, which is OK because the X.509 keyIDs are 
		   explicit and not implicitly generated from the key data like 
		   OpenPGP one */
		status = writePKCS3Key( &stream, publicKey, cryptAlgo );
		}
	else
		{
		status = publicKey->writePublicKeyFunction( &stream, contextInfoPtr, 
													KEYFORMAT_CERT, 
													"public_key", 10 );
		}
	if( cryptStatusOK( status ) )
		status = calculateFlatKeyID( buffer, stell( &stream ), 
									 publicKey->keyID, KEYID_SIZE );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );

#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	/* If it's an RSA key, we need to calculate the PGP 2 key ID alongside 
	   the cryptlib one */
	if( cryptAlgo == CRYPT_ALGO_RSA )
		{
		const PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
		int length;

		status = exportBignum( buffer, CRYPT_MAX_PKCSIZE, &length,
							   &pkcInfo->rsaParam_n );
		if( cryptStatusError( status ) )
			return( status );
		if( length > PGP_KEYID_SIZE )
			{
			memcpy( publicKey->pgp2KeyID, 
					buffer + length - PGP_KEYID_SIZE, PGP_KEYID_SIZE );
			contextInfoPtr->flags |= CONTEXT_FLAG_PGPKEYID_SET;
			}
		}

	/* If the OpenPGP ID is already set by having the key loaded from a PGP
	   keyset, we're done */
	if( contextInfoPtr->flags & CONTEXT_FLAG_OPENPGPKEYID_SET )
		return( CRYPT_OK );

	/* If it's a non-PGP algorithm then we can't do anything with it */
	if( cryptAlgo != CRYPT_ALGO_RSA && cryptAlgo != CRYPT_ALGO_DSA && \
		cryptAlgo != CRYPT_ALGO_ELGAMAL )
		return( CRYPT_OK );

	/* Finally, set the OpenPGP key ID */
	status = calculateOpenPGPKeyID( contextInfoPtr, cryptAlgo );
	if( cryptStatusError( status ) )
		return( status );
#endif /* USE_PGP || USE_PGPKEYS */

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Context Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyID( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_V( contextInfoPtr->type == CONTEXT_PKC );

	/* Set the access method pointers */
	pkcInfo->calculateKeyIDFunction = calculateKeyID;
	}
#else

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyID( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	}
#endif /* USE_PKC */
