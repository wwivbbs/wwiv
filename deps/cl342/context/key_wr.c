/****************************************************************************
*																			*
*						Public/Private Key Write Routines					*
*						Copyright Peter Gutmann 1992-2009					*
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

/* Although there is a fair amount of commonality between public and private-
   key functions, we keep them distinct to enforce red/black separation.

   The DLP algorithms split the key components over the information in the
   AlgorithmIdentifier and the actual public/private key components, with the
   (p, q, g) set classed as domain parameters and included in the
   AlgorithmIdentifier and y being the actual key.

	params = SEQ {
		p INTEGER,
		q INTEGER,				-- q for DSA
		g INTEGER,				-- g for DSA
		j INTEGER OPTIONAL,		-- X9.42 only
		validationParams [...]	-- X9.42 only
		}

	key = y INTEGER				-- g^x mod p

   For peculiar historical reasons (copying errors and the use of obsolete
   drafts as reference material) the X9.42 interpretation used in PKIX 
   reverses the second two parameters from FIPS 186 (so it uses p, g, q 
   instead of p, q, g), so when we read/write the parameter information we 
   have to switch the order in which we read the values if the algorithm 
   isn't DSA */

#define hasReversedParams( cryptAlgo ) \
		( ( cryptAlgo ) == CRYPT_ALGO_DH || \
		  ( cryptAlgo ) == CRYPT_ALGO_ELGAMAL )

/* Prototypes for functions in key_rd.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getECCOidTbl( OUT const OID_INFO **oidTblPtr,
				  OUT_INT_Z int *noOidTblEntries );

#ifdef USE_PKC

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

#if defined( USE_SSH )

/* Write a bignum as a fixed-length value, needed by several encoding 
   types and formats */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeFixedBignum( INOUT STREAM *stream, const BIGNUM *bignum,
							 IN_LENGTH_SHORT_MIN( 20 ) const int fixedSize )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int bnLength, noZeroes, i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES( fixedSize >= 20 && fixedSize <= CRYPT_MAX_PKCSIZE );

	/* Extract the bignum data and get its length */
	status = exportBignum( buffer, CRYPT_MAX_PKCSIZE, &bnLength, bignum );
	ENSURES( cryptStatusOK( status ) );
	noZeroes = fixedSize - bnLength;
	REQUIRES( noZeroes >= 0 && noZeroes < fixedSize );

	/* Write the leading zeroes followed by the bignum value */
	for( i = 0; i < noZeroes; i++ )
		sputc( stream, 0 );
	status = swrite( stream, buffer, bnLength );
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	
	return( status );
	}
#endif /* USE_SSH */

/****************************************************************************
*																			*
*								Write Public Keys							*
*																			*
****************************************************************************/

/* Write X.509 SubjectPublicKeyInfo public keys */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRsaSubjectPublicKey( INOUT STREAM *stream, 
									 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const int length = sizeofBignum( &rsaKey->rsaParam_n ) + \
					   sizeofBignum( &rsaKey->rsaParam_e );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	/* Write the SubjectPublicKeyInfo header field (the +1 is for the 
	   bitstring) */
	writeSequence( stream, sizeofAlgoID( CRYPT_ALGO_RSA ) + \
						   ( int ) sizeofObject( \
										sizeofObject( length ) + 1 ) );
	writeAlgoID( stream, CRYPT_ALGO_RSA );

	/* Write the BIT STRING wrapper and the PKC information */
	writeBitStringHole( stream, ( int ) sizeofObject( length ), 
						DEFAULT_TAG );
	writeSequence( stream, length );
	writeBignum( stream, &rsaKey->rsaParam_n );
	return( writeBignum( stream, &rsaKey->rsaParam_e ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeDlpSubjectPublicKey( INOUT STREAM *stream, 
									 const CONTEXT_INFO *contextInfoPtr )
	{
	const CRYPT_ALGO_TYPE cryptAlgo = contextInfoPtr->capabilityInfo->cryptAlgo;
	const PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const int parameterSize = ( int ) sizeofObject( \
								sizeofBignum( &dlpKey->dlpParam_p ) + \
								sizeofBignum( &dlpKey->dlpParam_q ) + \
								sizeofBignum( &dlpKey->dlpParam_g ) );
	const int componentSize = sizeofBignum( &dlpKey->dlpParam_y );
	int totalSize;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );

	/* If it's an Elgamal key created by PGP or a DH key from SSL/SSH then 
	   the q parameter isn't present so we can't write the key in this format */
	if( BN_is_zero( &dlpKey->dlpParam_q ) )
		{
		DEBUG_DIAG(( "Can't write Elgamal key due to missing q parameter" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_NOTAVAIL );
		}

	/* Determine the size of the AlgorithmIdentifier and the BIT STRING-
	   encapsulated public-key data (the +1 is for the bitstring) */
	totalSize = sizeofAlgoIDex( cryptAlgo, CRYPT_ALGO_NONE, parameterSize ) + \
				( int ) sizeofObject( componentSize + 1 );

	/* Write the SubjectPublicKeyInfo header field */
	writeSequence( stream, totalSize );
	writeAlgoIDparam( stream, cryptAlgo, parameterSize );

	/* Write the parameter data */
	writeSequence( stream, sizeofBignum( &dlpKey->dlpParam_p ) + \
						   sizeofBignum( &dlpKey->dlpParam_q ) + \
						   sizeofBignum( &dlpKey->dlpParam_g ) );
	writeBignum( stream, &dlpKey->dlpParam_p );
	if( hasReversedParams( cryptAlgo ) )
		{
		writeBignum( stream, &dlpKey->dlpParam_g );
		writeBignum( stream, &dlpKey->dlpParam_q );
		}
	else
		{
		writeBignum( stream, &dlpKey->dlpParam_q );
		writeBignum( stream, &dlpKey->dlpParam_g );
		}

	/* Write the BIT STRING wrapper and the PKC information */
	writeBitStringHole( stream, componentSize, DEFAULT_TAG );
	return( writeBignum( stream, &dlpKey->dlpParam_y ) );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeEccSubjectPublicKey( INOUT STREAM *stream, 
									 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const OID_INFO *oidTbl;
	const BYTE *oid = NULL;
	BYTE buffer[ MAX_PKCSIZE_ECCPOINT + 8 ];
	int oidTblSize, fieldSize = DUMMY_INIT, encodedPointSize, totalSize;
	int i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDH ) );

	/* Get the information that we'll need to encode the key.  Note that 
	   this assumes that we'll be using a known (named) curve rather than
	   arbitrary curve parameters, which has been enforced by the higher-
	   level code */
	status = getECCOidTbl( &oidTbl, &oidTblSize );
	if( cryptStatusOK( status ) )
		status = getECCFieldSize( eccKey->curveType, &fieldSize );
	if( cryptStatusError( status ) )
		return( status );
	for( i = 0; i < oidTblSize && oidTbl[ i ].oid != NULL; i++ )
		{
		if( oidTbl[ i ].selectionID == eccKey->curveType )
			{
			oid = oidTbl[ i ].oid;
			break;
			}
		}
	ENSURES( i < oidTblSize );
	ENSURES( oid != NULL );

	/* Get the encoded point data */
	status = exportECCPoint( buffer, MAX_PKCSIZE_ECCPOINT, &encodedPointSize, 
							 &eccKey->eccParam_qx, &eccKey->eccParam_qy, 
							 fieldSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine the size of the AlgorithmIdentifier and the BIT STRING-
	   encapsulated public-key data (the final +1 is for the bitstring),
	   ECC algorithms are a bit strange because there's no specific type of 
	   "ECDSA key" or "ECDH key" or whatever, just a generic "ECC key", so 
	   if we're given an ECDH key we write it as a generic ECC key, denoted 
	   using the generic identifier CRYPT_ALGO_ECDSA */
	totalSize = sizeofAlgoIDex( CRYPT_ALGO_ECDSA, CRYPT_ALGO_NONE, 
								sizeofOID( oid ) ) + \
				( int ) sizeofObject( encodedPointSize + 1 );

	/* Write the SubjectPublicKeyInfo header field */
	writeSequence( stream, totalSize );
	writeAlgoIDparam( stream, CRYPT_ALGO_ECDSA, sizeofOID( oid ) );

	/* Write the parameter data */
	writeOID( stream, oid );

	/* Write the BIT STRING wrapper and the PKC information */
	writeBitStringHole( stream, encodedPointSize, DEFAULT_TAG );
	status = swrite( stream, buffer, encodedPointSize );
	zeroise( buffer, MAX_PKCSIZE_ECCPOINT );
	return( status );
	}
#endif /* USE_ECDH || USE_ECDSA */

#ifdef USE_SSH1

/* Write SSHv1 public keys:

	uint32		keysize_bits
	mpint		exponent
	mpint		modulus */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSsh1RsaPublicKey( INOUT STREAM *stream, 
								  const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	writeUint32( stream, BN_num_bits( &rsaKey->rsaParam_n ) );
	writeBignumInteger16Ubits( stream, &rsaKey->rsaParam_e );
	return( writeBignumInteger16Ubits( stream, &rsaKey->rsaParam_n ) );
	}
#endif /* USE_SSH1 */

#ifdef USE_SSH

/* Write SSHv2 public keys:

   RSA/DSA:

	string		[ server key/certificate ]
		string	"ssh-rsa"	"ssh-dss"
		mpint	e			p
		mpint	n			q
		mpint				g
		mpint				y

   ECDSA:

	string		[ server key/certificate ]
		string	"ecdsa-sha2-*"
		string	"*"				-- The "*" portion from the above field
		string	Q */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSshRsaPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	writeUint32( stream, sizeofString32( "ssh-rsa", 7 ) + \
						 sizeofBignumInteger32( &rsaKey->rsaParam_e ) + \
						 sizeofBignumInteger32( &rsaKey->rsaParam_n ) );
	writeString32( stream, "ssh-rsa", 7 );
	writeBignumInteger32( stream, &rsaKey->rsaParam_e );
	return( writeBignumInteger32( stream, &rsaKey->rsaParam_n ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSshDlpPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *dsaKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA ) );

	/* SSHv2 uses PKCS #3 rather than X9.42-style DH keys so we have to 
	   treat this algorithm type specially */
	if( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH )
		{
		writeUint32( stream, sizeofString32( "ssh-dh", 6 ) + \
							 sizeofBignumInteger32( &dsaKey->dlpParam_p ) + \
							 sizeofBignumInteger32( &dsaKey->dlpParam_g ) );
		writeString32( stream, "ssh-dh", 6 );
		writeBignumInteger32( stream, &dsaKey->dlpParam_p );
		return( writeBignumInteger32( stream, &dsaKey->dlpParam_g ) );
		}

	writeUint32( stream, sizeofString32( "ssh-dss", 7 ) + \
						 sizeofBignumInteger32( &dsaKey->dlpParam_p ) + \
						 sizeofBignumInteger32( &dsaKey->dlpParam_q ) + \
						 sizeofBignumInteger32( &dsaKey->dlpParam_g ) + \
						 sizeofBignumInteger32( &dsaKey->dlpParam_y ) );
	writeString32( stream, "ssh-dss", 7 );
	writeBignumInteger32( stream, &dsaKey->dlpParam_p );
	writeBignumInteger32( stream, &dsaKey->dlpParam_q );
	writeBignumInteger32( stream, &dsaKey->dlpParam_g );
	return( writeBignumInteger32( stream, &dsaKey->dlpParam_y ) );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSshEccPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const char *algoName, *paramName;
	BYTE buffer[ MAX_PKCSIZE_ECCPOINT + 8 ];
	int fieldSize, encodedPointSize = DUMMY_INIT;
	int algoNameLen, paramNameLen, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDSA );

	/* Get the string form of the curve parameters */
	switch( eccKey->curveType )
		{
		case CRYPT_ECCCURVE_P256:
			algoName = "ecdsa-sha2-nistp256";
			algoNameLen = 19;
			paramName = "nistp256";
			paramNameLen = 8;
			break;

		case CRYPT_ECCCURVE_P384:
			algoName = "ecdsa-sha2-nistp384";
			algoNameLen = 19;
			paramName = "nistp384";
			paramNameLen = 8;
			break;

		case CRYPT_ECCCURVE_P521:
			algoName = "ecdsa-sha2-nistp521";
			algoNameLen = 19;
			paramName = "nistp521";
			paramNameLen = 8;
			break;

		default:
			retIntError();
		}

	/* Get the information that we'll need to encode the key and the encoded
	   point data.  Note that this assumes that we'll be using a known 
	   (named) curve rather than arbitrary curve parameters, which has been 
	   enforced by the higher-level code */
	status = getECCFieldSize( eccKey->curveType, &fieldSize );
	if( cryptStatusOK( status ) )
		{
		status = exportECCPoint( buffer, MAX_PKCSIZE_ECCPOINT, 
								 &encodedPointSize, &eccKey->eccParam_qx, 
								 &eccKey->eccParam_qy, fieldSize );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the PKC information */
	writeUint32( stream, sizeofString32( algoName, algoNameLen ) + \
						 sizeofString32( paramName, paramNameLen ) + \
						 sizeofString32( buffer, encodedPointSize ) );
	writeString32( stream, algoName, algoNameLen );
	writeString32( stream, paramName, paramNameLen );
	status = writeString32( stream, buffer, encodedPointSize );
	zeroise( buffer, MAX_PKCSIZE_ECCPOINT );
	return( status );
	}
#endif /* USE_ECDH || USE_ECDSA */

#endif /* USE_SSH */

#ifdef USE_SSL

/* Write SSL public keys:

	DH:
		uint16		dh_pLen
		byte[]		dh_p
		uint16		dh_gLen
		byte[]		dh_g
	  [	uint16		dh_YsLen ]
	  [	byte[]		dh_Ys	 ]

	ECDH:
		byte		curveType
		uint16		namedCurve
	  [	uint8		ecPointLen	-- NB uint8 not uint16 ]
	  [	byte[]		ecPoint ]

   The DH y value is nominally attached to the DH p and g values but isn't 
   processed at this level since this is a pure PKCS #3 DH key and not a 
   generic DLP key.  The same holds for the ECDH Q value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSslDlpPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *dhKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH );

	writeBignumInteger16U( stream, &dhKey->dlpParam_p );
	return( writeBignumInteger16U( stream, &dhKey->dlpParam_g ) );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

static const MAP_TABLE sslCurveInfo[] = {
	{ CRYPT_ECCCURVE_P192, 19 },
	{ CRYPT_ECCCURVE_P224, 21 },
	{ CRYPT_ECCCURVE_P256, 23 },
	{ CRYPT_ECCCURVE_P384, 24 },
	{ CRYPT_ECCCURVE_P521, 25 },
	{ CRYPT_ERROR, 0 }, 
		{ CRYPT_ERROR, 0 }
	};

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getEccSslInfoTbl( OUT const MAP_TABLE **sslInfoTblPtr,
							 OUT_INT_Z int *noSslInfoTblEntries )
	{
	assert( isReadPtr( sslInfoTblPtr, sizeof( MAP_TABLE * ) ) );
	assert( isWritePtr( noSslInfoTblEntries, sizeof( int ) ) );

	*sslInfoTblPtr = sslCurveInfo;
	*noSslInfoTblEntries = FAILSAFE_ARRAYSIZE( sslCurveInfo, MAP_TABLE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeSslEccPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const MAP_TABLE *sslCurveInfoPtr;
	int curveID, sslCurveInfoNoEntries, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDH );

	/* Look up the SSL NamedCurve ID based on the curve ID */
	status = getEccSslInfoTbl( &sslCurveInfoPtr, &sslCurveInfoNoEntries );
	if( cryptStatusError( status ) )
		return( status );
	status = mapValue( eccKey->curveType, &curveID, sslCurveInfoPtr, 
					   sslCurveInfoNoEntries );
	if( cryptStatusError( status ) )
		return( status );

	sputc( stream, 0x03 );	/* NamedCurve */
	return( writeUint16( stream, curveID ) );
	}
#endif /* USE_ECDH || USE_ECDSA */

#endif /* USE_SSL */

#ifdef USE_PGP

/* Write PGP public keys:

	byte		version
	uint32		creationTime
	[ uint16	validity - version 2 or 3 only ]
	byte		RSA		DSA		Elgamal
	mpi			n		p		p
	mpi			e		q		g
	mpi					g		y
	mpi					y */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writePgpRsaPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	sputc( stream, PGP_VERSION_OPENPGP );
	if( rsaKey->pgpCreationTime < MIN_TIME_VALUE )
		writeUint32( stream, 0 );
	else
		writeUint32Time( stream, rsaKey->pgpCreationTime );
	sputc( stream, PGP_ALGO_RSA );
	writeBignumInteger16Ubits( stream, &rsaKey->rsaParam_n );
	return( writeBignumInteger16Ubits( stream, &rsaKey->rsaParam_e ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writePgpDlpPublicKey( INOUT STREAM *stream, 
								 const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CRYPT_ALGO_TYPE cryptAlgo = contextInfoPtr->capabilityInfo->cryptAlgo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );

	sputc( stream, PGP_VERSION_OPENPGP );
	if( dlpKey->pgpCreationTime < MIN_TIME_VALUE )
		writeUint32( stream, 0 );
	else
		writeUint32Time( stream, dlpKey->pgpCreationTime );
	sputc( stream, ( cryptAlgo == CRYPT_ALGO_DSA ) ? \
		   PGP_ALGO_DSA : PGP_ALGO_ELGAMAL );
	writeBignumInteger16Ubits( stream, &dlpKey->dlpParam_p );
	if( cryptAlgo == CRYPT_ALGO_DSA )
		writeBignumInteger16Ubits( stream, &dlpKey->dlpParam_q );
	writeBignumInteger16Ubits( stream, &dlpKey->dlpParam_g );
	return( writeBignumInteger16Ubits( stream, &dlpKey->dlpParam_y ) );
	}
#endif /* USE_PGP */

/* Umbrella public-key write functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePublicKeyRsaFunction( INOUT STREAM *stream, 
									  const CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									  IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									  IN_LENGTH_FIXED( 10 ) \
										const int accessKeyLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 10 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 10 || memcmp( accessKey, "public_key", 10 ) )
		retIntError();

	switch( formatType )
		{
		case KEYFORMAT_CERT:
			return( writeRsaSubjectPublicKey( stream, contextInfoPtr ) );

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			return( writeSshRsaPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSH */

#ifdef USE_SSH1
		case KEYFORMAT_SSH1:
			return( writeSsh1RsaPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSH1 */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			return( writePgpRsaPublicKey( stream, contextInfoPtr ) );
#endif /* USE_PGP */
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePublicKeyDlpFunction( INOUT STREAM *stream, 
									  const CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									  IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									  IN_LENGTH_FIXED( 10 ) \
										const int accessKeyLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 10 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 10 || memcmp( accessKey, "public_key", 10 ) )
		retIntError();

	switch( formatType )
		{
		case KEYFORMAT_CERT:
			return( writeDlpSubjectPublicKey( stream, contextInfoPtr ) );

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			return( writeSshDlpPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSH */

#ifdef USE_SSL
		case KEYFORMAT_SSL:
			return( writeSslDlpPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSL */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			return( writePgpDlpPublicKey( stream, contextInfoPtr ) );
#endif /* USE_PGP */
		}

	retIntError();
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePublicKeyEccFunction( INOUT STREAM *stream, 
									  const CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									  IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									  IN_LENGTH_FIXED( 10 ) \
										const int accessKeyLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDH ) );
	REQUIRES( formatType == KEYFORMAT_CERT || formatType == KEYFORMAT_SSL || \
			  formatType == KEYFORMAT_SSH );
	REQUIRES( accessKeyLen == 10 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 10 || memcmp( accessKey, "public_key", 10 ) )
		retIntError();

	switch( formatType )
		{
		case KEYFORMAT_CERT:
			return( writeEccSubjectPublicKey( stream, contextInfoPtr ) );

#ifdef USE_SSL
		case KEYFORMAT_SSL:
			return( writeSslEccPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSL */

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			return( writeSshEccPublicKey( stream, contextInfoPtr ) );
#endif /* USE_SSH */
		}

	retIntError();
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*								Write Private Keys							*
*																			*
****************************************************************************/

/* Write private keys */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRsaPrivateKey( INOUT STREAM *stream, 
							   const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	int length = sizeofBignum( &rsaKey->rsaParam_p ) + \
				 sizeofBignum( &rsaKey->rsaParam_q );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	/* Add the length of any optional components that may be present */
	if( !BN_is_zero( &rsaKey->rsaParam_exponent1 ) )
		{
		length += sizeofBignum( &rsaKey->rsaParam_exponent1 ) + \
				  sizeofBignum( &rsaKey->rsaParam_exponent2 ) + \
				  sizeofBignum( &rsaKey->rsaParam_u );
		}

	/* Write the the PKC fields */
	writeSequence( stream, length );
	writeBignumTag( stream, &rsaKey->rsaParam_p, 3 );
	if( BN_is_zero( &rsaKey->rsaParam_exponent1 ) )
		return( writeBignumTag( stream, &rsaKey->rsaParam_q, 4 ) );
	writeBignumTag( stream, &rsaKey->rsaParam_q, 4 );
	writeBignumTag( stream, &rsaKey->rsaParam_exponent1, 5 );
	writeBignumTag( stream, &rsaKey->rsaParam_exponent2, 6 );
	return( writeBignumTag( stream, &rsaKey->rsaParam_u, 7 ) );
	}

#ifdef USE_PKCS12

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRsaPrivateKeyOld( INOUT STREAM *stream, 
								  const CONTEXT_INFO *contextInfoPtr )
	{
	const PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const int length = sizeofShortInteger( 0 ) + \
					   sizeofBignum( &rsaKey->rsaParam_n ) + \
					   sizeofBignum( &rsaKey->rsaParam_e ) + \
					   sizeofBignum( &rsaKey->rsaParam_d ) + \
					   sizeofBignum( &rsaKey->rsaParam_p ) + \
					   sizeofBignum( &rsaKey->rsaParam_q ) + \
					   sizeofBignum( &rsaKey->rsaParam_exponent1 ) + \
					   sizeofBignum( &rsaKey->rsaParam_exponent2 ) + \
					   sizeofBignum( &rsaKey->rsaParam_u );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );

	/* The older format is somewhat restricted in terms of what can be
	   written since all components must be present, even the ones that are
	   never used.  If anything is missing we can't write the key since
	   nothing would be able to read it */
	if( BN_is_zero( &rsaKey->rsaParam_n ) || \
		BN_is_zero( &rsaKey->rsaParam_d ) || \
		BN_is_zero( &rsaKey->rsaParam_p ) || \
		BN_is_zero( &rsaKey->rsaParam_exponent1 ) )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Write the the PKC fields */
	writeSequence( stream, sizeofShortInteger( 0 ) + \
						   sizeofAlgoID( CRYPT_ALGO_RSA ) + \
						   ( int ) sizeofObject( \
										sizeofObject( length ) ) );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	writeAlgoID( stream, CRYPT_ALGO_RSA );
	writeOctetStringHole( stream, ( int ) sizeofObject( length ), 
						  DEFAULT_TAG );
	writeSequence( stream, length );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	writeBignum( stream, &rsaKey->rsaParam_n );
	writeBignum( stream, &rsaKey->rsaParam_e );
	writeBignum( stream, &rsaKey->rsaParam_d );
	writeBignum( stream, &rsaKey->rsaParam_p );
	writeBignum( stream, &rsaKey->rsaParam_q );
	writeBignum( stream, &rsaKey->rsaParam_exponent1 );
	writeBignum( stream, &rsaKey->rsaParam_exponent2 );
	return( writeBignum( stream, &rsaKey->rsaParam_u ) );
	}
#endif /* USE_PKCS12 */

/* Umbrella private-key write functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePrivateKeyRsaFunction( INOUT STREAM *stream, 
									   const CONTEXT_INFO *contextInfoPtr,
									   IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									   IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									   IN_LENGTH_FIXED( 11 ) \
										const int accessKeyLen )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_RSA );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 11 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 11 || memcmp( accessKey, "private_key", 11 ) || \
		( formatType != KEYFORMAT_PRIVATE && \
		  formatType != KEYFORMAT_PRIVATE_OLD ) )
		retIntError();

	switch( formatType )
		{
		case KEYFORMAT_PRIVATE:
			return( writeRsaPrivateKey( stream, contextInfoPtr ) );

#ifdef USE_PKCS12
		case KEYFORMAT_PRIVATE_OLD:
			return( writeRsaPrivateKeyOld( stream, contextInfoPtr ) );
#endif /* USE_PKCS12 */
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePrivateKeyDlpFunction( INOUT STREAM *stream, 
									   const CONTEXT_INFO *contextInfoPtr,
									   IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									   IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									   IN_LENGTH_FIXED( 11 ) \
										const int accessKeyLen )
	{
	const PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DH || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_DSA || \
				contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 11 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 11 || memcmp( accessKey, "private_key", 11 ) || \
		formatType != KEYFORMAT_PRIVATE )
		retIntError();

	/* When we're generating a DH key ID only p, q, and g are initialised so 
	   we write a special-case zero y value.  This is a somewhat ugly side-
	   effect of the odd way in which DH "public keys" work */
	if( BN_is_zero( &dlpKey->dlpParam_y ) )
		return( writeShortInteger( stream, 0, DEFAULT_TAG ) );

	/* Write the key components */
	return( writeBignum( stream, &dlpKey->dlpParam_x ) );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int writePrivateKeyEccFunction( INOUT STREAM *stream, 
									   const CONTEXT_INFO *contextInfoPtr,
									   IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									   IN_BUFFER( accessKeyLen ) \
										const char *accessKey, 
									   IN_LENGTH_FIXED( 11 ) \
										const int accessKeyLen )
	{
	const PKC_INFO *eccKey = contextInfoPtr->ctxPKC;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtr( accessKey, accessKeyLen ) );

	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  contextInfoPtr->capabilityInfo->cryptAlgo == CRYPT_ALGO_ECDSA );
	REQUIRES( formatType > KEYFORMAT_NONE && formatType < KEYFORMAT_LAST );
	REQUIRES( accessKeyLen == 11 );

	/* Make sure that we really intended to call this function */
	if( accessKeyLen != 11 || memcmp( accessKey, "private_key", 11 ) || \
		formatType != KEYFORMAT_PRIVATE )
		retIntError();

	/* Write the key components */
	return( writeBignum( stream, &eccKey->eccParam_d ) );
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*							Write Flat Public Key Data						*
*																			*
****************************************************************************/

/* If the keys are stored in a crypto device rather than being held in the
   context all that we'll have available are the public components in flat 
   format.  The following code writes flat-format public components in the 
   X.509 SubjectPublicKeyInfo format.  The parameters are:

	Algo	Comp1	Comp2	Comp3	Comp4
	----	-----	-----	-----	-----
	RSA		  n		  e		  -		  -
	DLP		  p		  q		  g		  y */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 7 ) ) \
int writeFlatPublicKey( OUT_BUFFER_OPT( bufMaxSize, *bufSize ) void *buffer, 
						IN_LENGTH_SHORT_Z const int bufMaxSize, 
						OUT_LENGTH_SHORT_Z int *bufSize,
						IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo, 
						IN_BUFFER( component1Length ) const void *component1, 
						IN_LENGTH_PKC const int component1Length,
						IN_BUFFER( component2Length ) const void *component2, 
						IN_LENGTH_PKC const int component2Length,
						IN_BUFFER_OPT( component3Length ) const void *component3, 
						IN_LENGTH_PKC_Z const int component3Length,
						IN_BUFFER_OPT( component4Length ) const void *component4, 
						IN_LENGTH_PKC_Z const int component4Length )
	{
	STREAM stream;
	const int comp1Size = sizeofInteger( component1, component1Length );
	const int comp2Size = sizeofInteger( component2, component2Length );
	const int comp3Size = ( component3 == NULL ) ? 0 : \
						  sizeofInteger( component3, component3Length );
	int parameterSize, componentSize, totalSize, status;

	assert( ( buffer == NULL && bufMaxSize == 0 ) || \
			isWritePtr( buffer, bufMaxSize ) );
	assert( isWritePtr( bufSize, sizeof( int ) ) );
	assert( isReadPtr( component1, component1Length ) );
	assert( isReadPtr( component2, component2Length ) );
	assert( component3 == NULL || \
			isReadPtr( component3, component3Length ) );
	assert( component4 == NULL || \
			isReadPtr( component4, component4Length ) );

	REQUIRES( ( buffer == NULL && bufMaxSize == 0 ) || \
			  ( buffer != NULL && \
			    bufMaxSize > 64 && bufMaxSize < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isPkcAlgo( cryptAlgo ) && !isEccAlgo( cryptAlgo ) );
	REQUIRES( component1Length >= MIN_PKCSIZE && \
			  component1Length <= CRYPT_MAX_PKCSIZE );
	REQUIRES( component2Length >= 1 && component2Length <= CRYPT_MAX_PKCSIZE );
	REQUIRES( ( component3 == NULL && component3Length == 0 ) || \
			  ( component3 != NULL && \
				component3Length >= 1 && component3Length <= CRYPT_MAX_PKCSIZE ) );
	REQUIRES( ( component4 == NULL && component4Length == 0 ) || \
			  ( component4 != NULL && \
				component4Length >= 1 && component4Length <= CRYPT_MAX_PKCSIZE ) );

	/* Clear return values */
	if( buffer != NULL )
		memset( buffer, 0, min( 16, bufMaxSize ) );
	*bufSize = 0;

	/* Calculate the size of the algorithm parameters and the public key 
	   components */
	switch( cryptAlgo )
		{
		case CRYPT_ALGO_DH:
		case CRYPT_ALGO_DSA:
		case CRYPT_ALGO_ELGAMAL:
			REQUIRES( component3 != NULL && component4 != NULL );

			parameterSize = ( int ) sizeofObject( comp1Size + comp2Size + \
												  comp3Size );
			componentSize = sizeofInteger( component4, component4Length );
			break;

		case CRYPT_ALGO_RSA:
			REQUIRES( component3 == NULL && component4 == NULL );

			parameterSize = 0;
			componentSize = ( int ) sizeofObject( comp1Size + comp2Size );
			break;

		default:
			retIntError();
		}

	/* Determine the size of the AlgorithmIdentifier and the BIT STRING-
	   encapsulated public-key data (the +1 is for the bitstring) */
	status = totalSize = sizeofAlgoIDex( cryptAlgo, CRYPT_ALGO_NONE, \
										 parameterSize );
	if( cryptStatusError( status ) )
		return( status );
	totalSize += ( int ) sizeofObject( componentSize + 1 );
	if( buffer == NULL )
		{
		/* It's a size-check call, return the overall size */
		*bufSize = ( int ) sizeofObject( totalSize );

		return( CRYPT_OK );
		}

	sMemOpen( &stream, buffer, bufMaxSize );

	/* Write the SubjectPublicKeyInfo header field */
	writeSequence( &stream, totalSize );
	writeAlgoIDparam( &stream, cryptAlgo, parameterSize );

	/* Write the parameter data if necessary */
	if( isDlpAlgo( cryptAlgo ) )
		{
		writeSequence( &stream, comp1Size + comp2Size + comp3Size );
		writeInteger( &stream, component1, component1Length, DEFAULT_TAG );
		if( hasReversedParams( cryptAlgo ) )
			{
			writeInteger( &stream, component3, component3Length, DEFAULT_TAG );
			writeInteger( &stream, component2, component2Length, DEFAULT_TAG );
			}
		else
			{
			writeInteger( &stream, component2, component2Length, DEFAULT_TAG );
			writeInteger( &stream, component3, component3Length, DEFAULT_TAG );
			}
		}

	/* Write the BIT STRING wrapper and the PKC information */
	writeBitStringHole( &stream, componentSize, DEFAULT_TAG );
	if( cryptAlgo == CRYPT_ALGO_RSA )
		{
		writeSequence( &stream, comp1Size + comp2Size );
		writeInteger( &stream, component1, component1Length, DEFAULT_TAG );
		status = writeInteger( &stream, component2, component2Length, 
							   DEFAULT_TAG );
		}
	else
		{
		status = writeInteger( &stream, component4, component4Length, 
							   DEFAULT_TAG );
		}
	if( cryptStatusOK( status ) )
		*bufSize = stell( &stream );

	/* Clean up */
	sMemDisconnect( &stream );
	return( status );
	}

/****************************************************************************
*																			*
*								Write DL Values								*
*																			*
****************************************************************************/

/* Unlike the simpler RSA PKC, DL-based PKCs produce a pair of values that
   need to be encoded as structured data.  The following two functions 
   perform this en/decoding.  SSH assumes that DLP values are two fixed-size
   blocks of 20 bytes so we can't use the normal read/write routines to 
   handle these values */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int encodeDLValuesFunction( OUT_BUFFER( bufMaxSize, \
											   *bufSize ) BYTE *buffer, 
								   IN_LENGTH_SHORT_MIN( 20 + 20 ) \
									const int bufMaxSize, 
								   OUT_LENGTH_SHORT_Z int *bufSize, 
								   const BIGNUM *value1, 
								   const BIGNUM *value2, 
								   IN_ENUM( CRYPT_FORMAT ) \
									const CRYPT_FORMAT_TYPE formatType )
	{
	STREAM stream;
	int length = DUMMY_INIT, status;

	assert( isWritePtr( buffer, bufMaxSize ) );
	assert( isWritePtr( bufSize, sizeof( int ) ) );
	assert( isReadPtr( value1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( value2, sizeof( BIGNUM ) ) );

	REQUIRES( bufMaxSize >= 40 && bufMaxSize < MAX_INTLENGTH_SHORT );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST );

	/* Clear return values */
	memset( buffer, 0, min( 16, bufMaxSize ) );
	*bufSize = 0;

	sMemOpen( &stream, buffer, bufMaxSize );

	/* Write the DL components to the buffer */
	switch( formatType )
		{
		case CRYPT_FORMAT_CRYPTLIB:
			writeSequence( &stream, sizeofBignum( value1 ) + \
									sizeofBignum( value2 ) );
			writeBignum( &stream, value1 );
			status = writeBignum( &stream, value2 );
			break;

#ifdef USE_PGP
		case CRYPT_FORMAT_PGP:
			writeBignumInteger16Ubits( &stream, value1 );
			status = writeBignumInteger16Ubits( &stream, value2 );
			break;
#endif /* USE_PGP */

#ifdef USE_SSH
		case CRYPT_IFORMAT_SSH:
			/* SSH uses an awkward and horribly inflexible fixed format with 
			   each of the nominally 160-bit DLP values at fixed positions 
			   in a 2 x 20-byte buffer, so we have to write the bignums as
			   fixed-size value */
			status = writeFixedBignum( &stream, value1, 20 );
			if( cryptStatusOK( status ) )
				status = writeFixedBignum( &stream, value2, 20 );
			break;
#endif /* USE_SSH */

		default:
			retIntError();
		}
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	*bufSize = length;

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int encodeECDLValuesFunction( OUT_BUFFER( bufMaxSize, \
												 *bufSize ) BYTE *buffer, 
									 IN_LENGTH_SHORT_MIN( 20 + 20 ) \
										const int bufMaxSize, 
									 OUT_LENGTH_SHORT_Z int *bufSize, 
									 const BIGNUM *value1, 
									 const BIGNUM *value2, 
									 IN_ENUM( CRYPT_FORMAT ) \
										const CRYPT_FORMAT_TYPE formatType )
	{
	STREAM stream;
	int length = DUMMY_INIT, status;

	assert( isWritePtr( buffer, bufMaxSize ) );
	assert( isWritePtr( bufSize, sizeof( int ) ) );
	assert( isReadPtr( value1, sizeof( BIGNUM ) ) );
	assert( isReadPtr( value2, sizeof( BIGNUM ) ) );

	REQUIRES( bufMaxSize >= 40 && bufMaxSize < MAX_INTLENGTH_SHORT );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST );

	/* Clear return values */
	memset( buffer, 0, min( 16, bufMaxSize ) );
	*bufSize = 0;

	/* In most cases the DLP and ECDLP formats are identical and we can just
	   pass the call on to the DLP form, however SSH uses totally different 
	   signature formats depending on whether the signature is DSA or ECDSA, 
	   so we handle the SSH format explicitly here */
	if( formatType != CRYPT_IFORMAT_SSH )
		{
		return( encodeDLValuesFunction( buffer, bufMaxSize, bufSize, 
										value1, value2, formatType ) );
		}
	sMemOpen( &stream, buffer, bufMaxSize );
	writeBignumInteger32( &stream, value1 );
	status = writeBignumInteger32( &stream, value2 );
	if( cryptStatusOK( status ) )
		length = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	*bufSize = length;

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*							Context Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyWrite( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	const CRYPT_ALGO_TYPE cryptAlgo = contextInfoPtr->capabilityInfo->cryptAlgo;
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) && \
			contextInfoPtr->type == CONTEXT_PKC );

	/* Set the access method pointers */
	if( isDlpAlgo( cryptAlgo ) )
		{
		pkcInfo->writePublicKeyFunction = writePublicKeyDlpFunction;
		pkcInfo->writePrivateKeyFunction = writePrivateKeyDlpFunction;
		pkcInfo->encodeDLValuesFunction = encodeDLValuesFunction;

		return;
		}
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		pkcInfo->writePublicKeyFunction = writePublicKeyEccFunction;
		pkcInfo->writePrivateKeyFunction = writePrivateKeyEccFunction;
		pkcInfo->encodeDLValuesFunction = encodeECDLValuesFunction;

		return;
		}
#endif /* USE_ECDH || USE_ECDSA */
	pkcInfo->writePublicKeyFunction = writePublicKeyRsaFunction;
	pkcInfo->writePrivateKeyFunction = writePrivateKeyRsaFunction;
	}
#else

STDC_NONNULL_ARG( ( 1 ) ) \
void initKeyWrite( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	}
#endif /* USE_PKC */
