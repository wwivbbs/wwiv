/****************************************************************************
*																			*
*							Private Key Read Routines						*
*						Copyright Peter Gutmann 1992-2012					*
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

#if defined( USE_KEYSETS ) && defined( USE_PKC )

/****************************************************************************
*																			*
*							Read PKCS #15 Private Keys						*
*																			*
****************************************************************************/

#ifdef USE_INT_ASN1

/* Read private key components.  These functions assume that the public
   portions of the context have already been set up */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readRsaPrivateKey( INOUT STREAM *stream, 
							  INOUT CONTEXT_INFO *contextInfoPtr,
							  const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	READ_BIGNUM_FUNCTION readBignumFunction = checkRead ? \
									checkBignumRead : readBignumTag;
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	/* Read the header */
	status = readSequence( stream, NULL );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 0 ) )
		{
		/* Erroneously written in older code */
		status = readConstructed( stream, NULL, 0 );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Read the key components */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG_PRIMITIVE( 0 ) )
		{
		/* The public components may already have been read when we read a
		   corresponding public key or certificate so we only read them if
		   they're not already present */
		if( BN_is_zero( &rsaKey->rsaParam_n ) && \
			BN_is_zero( &rsaKey->rsaParam_e ) )
			{
			status = readBignumFunction( stream, &rsaKey->rsaParam_n, 
										 RSAPARAM_MIN_N, RSAPARAM_MAX_N, 
										 NULL, 0 );
			if( cryptStatusOK( status ) )
				{
				status = readBignumFunction( stream, &rsaKey->rsaParam_e, 
											 RSAPARAM_MIN_E, RSAPARAM_MAX_E, 
											 &rsaKey->rsaParam_n, 1 );
				}
			}
		else
			{
			/* The key components are already present, skip them */
			REQUIRES( !BN_is_zero( &rsaKey->rsaParam_n ) && \
					  !BN_is_zero( &rsaKey->rsaParam_e ) );
			readUniversal( stream );
			status = readUniversal( stream );
			}
		}
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG_PRIMITIVE( 2 ) )
		{
		/* d isn't used so we skip it */
		status = readUniversal( stream );
		}
	if( cryptStatusError( status ) )
		return( status );
	status = readBignumFunction( stream, &rsaKey->rsaParam_p, 
								 RSAPARAM_MIN_P, RSAPARAM_MAX_P, 
								 &rsaKey->rsaParam_n, 3 );
	if( cryptStatusOK( status ) )
		status = readBignumFunction( stream, &rsaKey->rsaParam_q, 
									 RSAPARAM_MIN_Q, RSAPARAM_MAX_Q, 
									 &rsaKey->rsaParam_n, 4 );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG_PRIMITIVE( 5 ) )
		{
		status = readBignumFunction( stream, &rsaKey->rsaParam_exponent1, 
									 RSAPARAM_MIN_EXP1, RSAPARAM_MAX_EXP1, 
									 &rsaKey->rsaParam_n, 5 );
		if( cryptStatusOK( status ) )
			status = readBignumFunction( stream, &rsaKey->rsaParam_exponent2, 
										 RSAPARAM_MIN_EXP2, RSAPARAM_MAX_EXP2, 
										 &rsaKey->rsaParam_n, 6 );
		if( cryptStatusOK( status ) )
			status = readBignumFunction( stream, &rsaKey->rsaParam_u, 
										 RSAPARAM_MIN_U, RSAPARAM_MAX_U, 
										 &rsaKey->rsaParam_n, 7 );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readDlpPrivateKey( INOUT STREAM *stream, 
							  INOUT CONTEXT_INFO *contextInfoPtr,
							  const BOOLEAN checkRead )
	{
	PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const DH_DOMAINPARAMS *domainParams = dlpKey->domainParams;
	const BIGNUM *p = ( domainParams != NULL ) ? \
					  &domainParams->p : &dlpKey->dlpParam_p;
	READ_BIGNUM_FUNCTION readBignumFunction = checkRead ? \
									checkBignumRead : readBignumTag;
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	/* Read the key components */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( tag == BER_SEQUENCE )
		{
		/* Erroneously written in older code */
		status = readSequence( stream, NULL );
		if( cryptStatusOK( status ) )
			{
			status = readBignumFunction( stream, &dlpKey->dlpParam_x,
										 DLPPARAM_MIN_X, DLPPARAM_MAX_X, 
										 p, 0 );
			}
		return( status );
		}
	status = readBignumFunction( stream, &dlpKey->dlpParam_x,
								 DLPPARAM_MIN_X, DLPPARAM_MAX_X, p,
								 DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dlpKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readEccPrivateKey( INOUT STREAM *stream, 
							  INOUT CONTEXT_INFO *contextInfoPtr,
							  const BOOLEAN checkRead )
	{
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	READ_BIGNUM_FUNCTION readBignumFunction = checkRead ? \
									checkBignumRead : readBignumTag;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	/* Read the key components.  Note that we can't use the ECC p value for
	   a range check because it hasn't been set yet, all that we have at 
	   this point is a curve ID */
	status = readBignumFunction( stream, &eccKey->eccParam_d,
								 ECCPARAM_MIN_D, ECCPARAM_MAX_D, NULL,
								 DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */
#endif /* USE_INT_ASN1 */

/****************************************************************************
*																			*
*							Read PKCS #12 Private Keys						*
*																			*
****************************************************************************/

/* Read private key components.  These functions assume that the public
   portions of the context have already been set up */

#if defined( USE_PKCS12 ) && defined( USE_INT_ASN1 )

#define OID_X509_KEYUSAGE	MKOID( "\x06\x03\x55\x1D\x0F" )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readRsaPrivateKeyOld( INOUT STREAM *stream, 
								 INOUT CONTEXT_INFO *contextInfoPtr )
	{
	CRYPT_ALGO_TYPE cryptAlgo DUMMY_INIT;
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const int startPos = stell( stream );
	int length, endPos, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );

	/* Skip the PKCS #8 wrapper.  When we read the OCTET STRING 
	   encapsulation we use MIN_PKCSIZE_THRESHOLD rather than MIN_PKCSIZE
	   so that a too-short key will get to readBignumChecked(), which
	   returns an appropriate error code */
	readSequence( stream, &length );			/* Outer wrapper */
	status = readShortInteger( stream, NULL );	/* Version */
	if( cryptStatusOK( status ) )
		status = readAlgoID( stream, &cryptAlgo, ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) || cryptAlgo != CRYPT_ALGO_RSA )
		return( CRYPT_ERROR_BADDATA );
	status = readOctetStringHole( stream, NULL, 
								  ( 2 * MIN_PKCSIZE_THRESHOLD ) + \
									( 5 * ( MIN_PKCSIZE_THRESHOLD / 2 ) ), 
								  DEFAULT_TAG );
	if( cryptStatusError( status ) )			/* OCTET STRING encaps.*/
		return( status );

	/* Read the header */
	readSequence( stream, NULL );
	status = readShortInteger( stream, NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the RSA key components, skipping n and e if we've already got 
	   them via the associated public key/certificate */
	if( BN_is_zero( &rsaKey->rsaParam_n ) )
		{
		status = readBignumChecked( stream, &rsaKey->rsaParam_n,
									RSAPARAM_MIN_N, RSAPARAM_MAX_N, 
									NULL );
		if( cryptStatusOK( status ) )
			status = readBignum( stream, &rsaKey->rsaParam_e,
								 RSAPARAM_MIN_E, RSAPARAM_MAX_E,
								 &rsaKey->rsaParam_n );
		}
	else
		{
		readUniversal( stream );
		status = readUniversal( stream );
		}
	if( cryptStatusOK( status ) )
		{
		/* d isn't used so we skip it */
		status = readUniversal( stream );
		}
	if( cryptStatusOK( status ) )
		status = readBignum( stream, &rsaKey->rsaParam_p,
							 RSAPARAM_MIN_P, RSAPARAM_MAX_P,
							 &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignum( stream, &rsaKey->rsaParam_q,
							 RSAPARAM_MIN_Q, RSAPARAM_MAX_Q,
							 &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignum( stream, &rsaKey->rsaParam_exponent1,
							 RSAPARAM_MIN_EXP1, RSAPARAM_MAX_EXP1,
							 &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignum( stream, &rsaKey->rsaParam_exponent2,
							 RSAPARAM_MIN_EXP2, RSAPARAM_MAX_EXP2,
							 &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignum( stream, &rsaKey->rsaParam_u,
							 RSAPARAM_MIN_U, RSAPARAM_MAX_U,
							 &rsaKey->rsaParam_n );
	if( cryptStatusError( status ) )
		return( status );

	/* Check whether there are any attributes present */
	if( stell( stream ) >= startPos + length )
		return( CRYPT_OK );

	/* Read the attribute wrapper */
	status = readConstructed( stream, &length, 0 );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;

	/* Read the collection of attributes.  Unlike any other key-storage 
	   format, PKCS #8 stores the key usage information as an X.509 
	   attribute alongside the encrypted private key data so we have to
	   process whatever attributes may be present in order to find the
	   keyUsage (if there is any) in order to set the object action 
	   permissions */
	LOOP_MED_CHECK( stell( stream ) < endPos )
		{
		BYTE oid[ MAX_OID_SIZE + 8 ];
		int oidLength, actionFlags, value;

		/* Read the attribute.  Since there's only one attribute type that 
		   we can use, we hardcode the read in here rather than performing a 
		   general-purpose attribute read */
		readSequence( stream, NULL );
		status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
								 BER_OBJECT_IDENTIFIER );
		if( cryptStatusError( status ) )
			return( status );

		/* If it's not a key-usage attribute, we can't do much with it */
		if( oidLength != sizeofOID( OID_X509_KEYUSAGE ) || \
			memcmp( oid, OID_X509_KEYUSAGE, oidLength ) )
			{
			status = readUniversal( stream );
			if( cryptStatusError( status ) )
				return( status );
			continue;
			}

		/* Read the keyUsage attribute and convert it into cryptlib action 
		   permissions */
		readSet( stream, NULL );
		status = readBitString( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		actionFlags = ACTION_PERM_NONE;
		if( value & ( KEYUSAGE_SIGN | KEYUSAGE_CA ) )
			{
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
										   ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
										   ACTION_PERM_ALL );
			}
		if( value & KEYUSAGE_CRYPT )
			{
			actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
										   ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
										   ACTION_PERM_ALL );
			}
#if 0	/* 11/6/13 Windows sets these flags to what are effectively
				   gibberish values (dataEncipherment for a signing key,
				   digitalSignature for an encryption key) so in order
				   to be able to use the key we have to ignore the keyUsage 
				   settings, in the same way that every other application 
				   seems to */
		if( actionFlags == ACTION_PERM_NONE )
			return( CRYPT_ERROR_NOTAVAIL );
		status = krnlSendMessage( contextInfoPtr->objectHandle, 
								  IMESSAGE_SETATTRIBUTE, &actionFlags, 
								  CRYPT_IATTRIBUTE_ACTIONPERMS );
		if( cryptStatusError( status ) )
			return( status );
#else
		assert( actionFlags != ACTION_PERM_NONE );	/* Warn in debug mode */
#endif /* 0 */
		}
	ENSURES( LOOP_BOUND_OK );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readDsaPrivateKeyOld( INOUT STREAM *stream, 
								 INOUT CONTEXT_INFO *contextInfoPtr )
	{
	CRYPT_ALGO_TYPE cryptAlgo DUMMY_INIT;
	PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int dummy, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA );

	/* Skip the PKCS #8 wrapper */
	readSequence( stream, NULL );				/* Outer wrapper */
	status = readShortInteger( stream, NULL );	/* Version */
	if( cryptStatusOK( status ) )
		status = readAlgoIDparam( stream, &cryptAlgo, &dummy, 
								  ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) || cryptAlgo != CRYPT_ALGO_DSA )
		return( CRYPT_ERROR_BADDATA );

	/* Read the DSA parameters if we haven't already got them via the
	   associated public key/certificate */
	if( BN_is_zero( &dlpKey->dlpParam_p ) )
		{
		readSequence( stream, NULL );	/* Parameter wrapper */
		status = readBignumChecked( stream, &dlpKey->dlpParam_p,
									DLPPARAM_MIN_P, DLPPARAM_MAX_P, 
									NULL );
		if( cryptStatusOK( status ) )
			status = readBignumChecked( stream, &dlpKey->dlpParam_q,
										DLPPARAM_MIN_Q, DLPPARAM_MAX_Q, 
										NULL );
		if( cryptStatusOK( status ) )
			status = readBignumChecked( stream, &dlpKey->dlpParam_g,
										DLPPARAM_MIN_G, DLPPARAM_MAX_G, 
										NULL );
		}
	else
		status = readUniversal( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the DSA private key component */
	status = readOctetStringHole( stream, NULL, 20, DEFAULT_TAG );
	if( cryptStatusOK( status ) )	/* OCTET STRING encapsulation */
		status = readBignum( stream, &dlpKey->dlpParam_x,
							 DLPPARAM_MIN_X, DLPPARAM_MAX_X,
							 &dlpKey->dlpParam_p );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dlpKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

#define OID_ECPUBLICKEY		MKOID( "\x06\x07\x2A\x86\x48\xCE\x3D\x02\x01" )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readEccPrivateKeyOld( INOUT STREAM *stream, 
								 INOUT CONTEXT_INFO *contextInfoPtr )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	long value;
	int tag, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA );

	/* Read the ECC key components.  These were never standardised in any 
	   PKCS standard, nor in the PKCS #12 RFC.  RFC 5915 "Elliptic Curve 
	   Private Key Structure" specifies the format for PKCS #8 as:

		ECPrivateKey ::= SEQUENCE {
			version			INTEGER (1),
			privateKey		OCTET STRING,
			parameters	[0]	ECParameters {{ NamedCurve }} OPTIONAL,
			publicKey	[1]	BIT STRING OPTIONAL
			}

	   but this isn't what's present in the encoded form created by OpenSSL.
	   Instead it's:

		ECSomething ::= SEQUENCE {
			version			INTEGER (0),
			parameters		SEQUENCE {
				type		OBJECT IDENTIFIER ecPublicKey,
				namedCurve	OBJECT IDENTIFIER
				}
			something		OCTET STRING {
				key			ECPrivateKey		-- As above
				}
			}

	   so we have to tunnel into this in order to find the PKCS #8-like
	   data that we're actually interested in.

	   Note that we can't use the ECC p value for a range check because it 
	   hasn't been set yet, all that we have at this point is a curve ID */
	readSequence( stream, NULL );		/* Outer wrapper */
	status = readShortInteger( stream, &value );/* Version */
	if( cryptStatusError( status ) || value != 0 )
		return( CRYPT_ERROR_BADDATA );
	status = readAlgoIDparam( stream, &cryptAlgo, &length, 
							  ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) || cryptAlgo != CRYPT_ALGO_ECDSA )
		return( CRYPT_ERROR_BADDATA );
	readUniversal( stream );			/* Named curve */
	readOctetStringHole( stream, NULL, MIN_PKCSIZE_ECC_THRESHOLD, 
						 DEFAULT_TAG );		/* OCTET STRING hole wrapper */
	readSequence( stream, NULL );				/* ECPrivateKey wrapper */
	status = readShortInteger( stream, &value );	/* Version */
	if( cryptStatusError( status ) || value != 1 )
		return( CRYPT_ERROR_BADDATA );

	/* We've finalled made it down to the private key value.  At this point 
	   we can't use readBignumTag() directly because it's designed to read 
	   either standard INTEGERs (via DEFAULT_TAG) or context-specific tagged 
	   items, so passing in a BER_OCTETSTRING will be interpreted as 
	   [4] IMPLICIT INTEGER rather than an OCTET STRING-tagged integer.  To 
	   get around this we read the tag separately and tell readBignumTag() 
	   to skip the tag read */
	tag = readTag( stream );
	if( cryptStatusError( tag ) || tag != BER_OCTETSTRING )
		return( CRYPT_ERROR_BADDATA );
	status = readBignumTag( stream, &eccKey->eccParam_d,
							ECCPARAM_MIN_D, ECCPARAM_MAX_D, NULL,
							NO_TAG );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */
#endif /* USE_PKCS12 && USE_INT_ASN1 */

/****************************************************************************
*																			*
*							Read PGP Private Keys							*
*																			*
****************************************************************************/

#ifdef USE_PGP 

/* Read PGP private key components.  This function assumes that the public
   portion of the context has already been set up */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpRsaPrivateKey( INOUT STREAM *stream, 
								 INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );

	/* Read the PGP private key information.  Note that we have to read the 
	   d value here because we need it to calculate e1 and e2 */
	status = readBignumInteger16Ubits( stream, &rsaKey->rsaParam_d, 
									   bytesToBits( RSAPARAM_MIN_D ), 
									   bytesToBits( RSAPARAM_MAX_D ), 
									   &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignumInteger16Ubits( stream, &rsaKey->rsaParam_p, 
										   bytesToBits( RSAPARAM_MIN_P ), 
										   bytesToBits( RSAPARAM_MAX_P ),
										   &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignumInteger16Ubits( stream, &rsaKey->rsaParam_q, 
										   bytesToBits( RSAPARAM_MIN_Q ), 
										   bytesToBits( RSAPARAM_MAX_Q ),
										   &rsaKey->rsaParam_n );
	if( cryptStatusOK( status ) )
		status = readBignumInteger16Ubits( stream, &rsaKey->rsaParam_u, 
										   bytesToBits( RSAPARAM_MIN_U ), 
										   bytesToBits( RSAPARAM_MAX_U ),
										   &rsaKey->rsaParam_n );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpDlpPrivateKey( INOUT STREAM *stream, 
								 INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );

	/* Read the PGP private key information */
	status = readBignumInteger16Ubits( stream, &dlpKey->dlpParam_x, 
									   bytesToBits( DLPPARAM_MIN_X ), 
									   bytesToBits( DLPPARAM_MAX_X ),
									   &dlpKey->dlpParam_p );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dlpKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*							Private-Key Read Interface						*
*																			*
****************************************************************************/

/* Umbrella private-key read functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPrivateKeyRsaFunction( INOUT STREAM *stream, 
									  INOUT CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT ) \
										const KEYFORMAT_TYPE formatType,
									  const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );
	REQUIRES( isEnumRange( formatType, KEYFORMAT ) );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	switch( formatType )
		{
#ifdef USE_INT_ASN1
		case KEYFORMAT_PRIVATE:
			return( readRsaPrivateKey( stream, contextInfoPtr, checkRead ) );
#endif /* USE_INT_ASN1 */

#if defined( USE_PKCS12 ) && defined( USE_INT_ASN1 )
		case KEYFORMAT_PRIVATE_OLD:
			return( readRsaPrivateKeyOld( stream, contextInfoPtr ) );
#endif /* USE_PKCS12 && USE_INT_ASN1 */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			return( readPgpRsaPrivateKey( stream, contextInfoPtr ) );
#endif /* USE_PGP */
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPrivateKeyDlpFunction( INOUT STREAM *stream, 
									  INOUT CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									  const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );
	REQUIRES( isEnumRange( formatType, KEYFORMAT ) );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	switch( formatType )
		{
#ifdef USE_INT_ASN1
		case KEYFORMAT_PRIVATE:
			return( readDlpPrivateKey( stream, contextInfoPtr, checkRead ) );
#endif /* USE_INT_ASN1 */

#if defined( USE_PKCS12 ) && defined( USE_INT_ASN1 )
		case KEYFORMAT_PRIVATE_OLD:
			return( readDsaPrivateKeyOld( stream, contextInfoPtr ) );
#endif /* USE_PKCS12 && USE_INT_ASN1 */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			return( readPgpDlpPrivateKey( stream, contextInfoPtr ) );
#endif /* USE_PGP */
		}

	retIntError();
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPrivateKeyEccFunction( INOUT STREAM *stream, 
									  INOUT CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									  const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA );
	REQUIRES( isEnumRange( formatType, KEYFORMAT ) );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	switch( formatType )
		{
#ifdef USE_INT_ASN1
		case KEYFORMAT_PRIVATE:
			return( readEccPrivateKey( stream, contextInfoPtr, checkRead ) );
#endif /* USE_INT_ASN1 */

#if defined( USE_PKCS12 ) && defined( USE_INT_ASN1 )
		case KEYFORMAT_PRIVATE_OLD:
			return( readEccPrivateKeyOld( stream, contextInfoPtr ) );
#endif /* USE_PKCS12 && USE_INT_ASN1 */
		}

	retIntError();
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*							Context Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initPrivKeyRead( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	CRYPT_ALGO_TYPE cryptAlgo;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_V( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_V( contextInfoPtr->type == CONTEXT_PKC );
	REQUIRES_V( capabilityInfoPtr != NULL );

	cryptAlgo = capabilityInfoPtr->cryptAlgo;

	/* Set the access method pointers */
	if( isDlpAlgo( cryptAlgo ) )
		{
		FNPTR_SET( pkcInfo->readPrivateKeyFunction, readPrivateKeyDlpFunction );
		return;
		}
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		FNPTR_SET( pkcInfo->readPrivateKeyFunction, readPrivateKeyEccFunction );
		return;
		}
#endif /* USE_ECDH || USE_ECDSA */
	FNPTR_SET( pkcInfo->readPrivateKeyFunction, readPrivateKeyRsaFunction );
	}
#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPrivKeyNullFunction( INOUT STREAM *stream, 
									INOUT CONTEXT_INFO *contextInfoPtr,
									IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									const BOOLEAN checkRead )
	{
	UNUSED_ARG( stream );
	UNUSED_ARG( contextInfoPtr );

	return( CRYPT_ERROR_NOTAVAIL );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void initPrivKeyRead( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	PKC_INFO *pkcInfo = contextInfoPtr->ctxPKC;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES_V( sanityCheckContext( contextInfoPtr ) );
	REQUIRES_V( contextInfoPtr->type == CONTEXT_PKC );

	/* Set the access method pointers */
	FNPTR_SET( pkcInfo->readPrivateKeyFunction, readPrivKeyNullFunction );
	}
#endif /* USE_KEYSETS && USE_PKC */
