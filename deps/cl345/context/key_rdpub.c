/****************************************************************************
*																			*
*							Public Key Read Routines						*
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

/* The DLP algorithms split the key components over the information in the
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

#ifdef USE_PKC

/****************************************************************************
*																			*
*							Read X.509 Public Keys							*
*																			*
****************************************************************************/

#ifdef USE_INT_ASN1

/* Read X.509 SubjectPublicKeyInfo public keys:

	SubjectPublicKeyInfo  ::=  SEQUENCE  {
		algorithm			AlgorithmIdentifier,
		subjectPublicKey	BIT STRING  
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readRsaSubjectPublicKey( INOUT STREAM *stream, 
									INOUT CONTEXT_INFO *contextInfoPtr,
									OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	CRYPT_ALGO_TYPE cryptAlgo DUMMY_INIT;
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the SubjectPublicKeyInfo header field and parameter data if
	   there's any present.  We read the outer wrapper in generic form since
	   it may be context-specific-tagged if it's coming from a keyset (RSA
	   public keys is the one place where PKCS #15 keys differ from X.509
	   ones) or something odd from CRMF */
	status = readGenericHole( stream, NULL, 8 + MIN_PKCSIZE_THRESHOLD + \
											RSAPARAM_MIN_E, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		status = readAlgoID( stream, &cryptAlgo, ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) )
		return( status );
	if( cryptAlgo != CRYPT_ALGO_RSA )
		return( CRYPT_ERROR_BADDATA );

	/* Set the maximum permitted actions.  More restrictive permissions may 
	   be set by higher-level code if required and in particular if the key 
	   is a pure public key rather than merely the public portions of a 
	   private key then the actions will be restricted at that point to 
	   encrypt and signature-check only */
	*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_ALL ) | \
				   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_ALL ) | \
				   MK_ACTION_PERM( MESSAGE_CTX_SIGN, ACTION_PERM_ALL ) | \
				   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, ACTION_PERM_ALL );

	/* Read the BIT STRING encapsulation and the public key fields */
	readBitStringHole( stream, NULL, MIN_PKCSIZE_THRESHOLD, DEFAULT_TAG );
	readSequence( stream, NULL );
	status = readBignumChecked( stream, &rsaKey->rsaParam_n, 
								RSAPARAM_MIN_N, RSAPARAM_MAX_N, NULL );
	if( cryptStatusOK( status ) )
		{
		status = readBignum( stream, &rsaKey->rsaParam_e,
							 RSAPARAM_MIN_E, RSAPARAM_MAX_E, 
							 &rsaKey->rsaParam_n );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readDlpSubjectPublicKey( INOUT STREAM *stream, 
									INOUT CONTEXT_INFO *contextInfoPtr,
									OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	CRYPT_ALGO_TYPE cryptAlgo DUMMY_INIT;
	int extraLength DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the SubjectPublicKeyInfo header field and make sure that the DLP 
	   parameter data is present */
	status = readGenericHole( stream, NULL, 
							  8 + MIN_PKCSIZE_THRESHOLD + DLPPARAM_MIN_G + \
							  DLPPARAM_MIN_Q + MIN_PKCSIZE_THRESHOLD, 
							  DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		status = readAlgoIDparam( stream, &cryptAlgo, &extraLength, 
								  ALGOID_CLASS_PKC );
	if( cryptStatusError( status ) )
		return( status );
	if( extraLength < MIN_PKCSIZE_THRESHOLD + DLPPARAM_MIN_G + \
					  DLPPARAM_MIN_Q || \
		extraLength > MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );
	if( capabilityInfoPtr->cryptAlgo != cryptAlgo )
		return( CRYPT_ERROR_BADDATA );

	/* Read the header and key parameters */
	readSequence( stream, NULL );
	status = readBignumChecked( stream, &dlpKey->dlpParam_p, 
								DLPPARAM_MIN_P, DLPPARAM_MAX_P, NULL );
	if( cryptStatusError( status ) )
		return( status );
	if( hasReversedParams( cryptAlgo ) )
		{
		status = readBignum( stream, &dlpKey->dlpParam_g, DLPPARAM_MIN_G, 
							 DLPPARAM_MAX_G, &dlpKey->dlpParam_p );
		if( cryptStatusOK( status ) )
			{
			status = readBignum( stream, &dlpKey->dlpParam_q, DLPPARAM_MIN_Q, 
								 DLPPARAM_MAX_Q, &dlpKey->dlpParam_p );
			}
		}
	else
		{
		status = readBignum( stream, &dlpKey->dlpParam_q, DLPPARAM_MIN_Q, 
							 DLPPARAM_MAX_Q, &dlpKey->dlpParam_p );
		if( cryptStatusOK( status ) )
			{
			status = readBignum( stream, &dlpKey->dlpParam_g, DLPPARAM_MIN_G, 
								 DLPPARAM_MAX_G, &dlpKey->dlpParam_p );
			}
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Set the maximum permitted actions.  Because of the special-case data 
	   formatting requirements for DLP algorithms we make the usage 
	   internal-only.  If the key is a pure public key rather than merely 
	   the public portions of a private key then the actions will be 
	   restricted by higher-level code to encrypt/signature-check only */
	if( cryptAlgo == CRYPT_ALGO_DSA )
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
									   ACTION_PERM_NONE_EXTERNAL );
		}
	else
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
									   ACTION_PERM_NONE_EXTERNAL );
		}

	/* Read the BIT STRING encapsulation and the public key fields */
	readBitStringHole( stream, NULL, MIN_PKCSIZE_THRESHOLD, DEFAULT_TAG );
	status = readBignumChecked( stream, &dlpKey->dlpParam_y,
								DLPPARAM_MIN_Y, DLPPARAM_MAX_Y,
								&dlpKey->dlpParam_p );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dlpKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readEccSubjectPublicKey( INOUT STREAM *stream, 
									INOUT CONTEXT_INFO *contextInfoPtr,
									OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	CRYPT_ALGO_TYPE cryptAlgo DUMMY_INIT;
	CRYPT_ECCCURVE_TYPE curveType;
	BYTE buffer[ MAX_PKCSIZE_ECCPOINT + 8 ];
	int fieldSize DUMMY_INIT;
	int length, extraLength DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the SubjectPublicKeyInfo header field and make sure that the ECC 
	   parameter data is present.  Because of the more or less arbitrary 
	   manner in which these parameters can be represented we have to be 
	   fairly open-ended in terms of the data size limits that we use, and 
	   in particular for named curves the lower bound is the size of a 
	   single OID that defines the curve */
	status = readGenericHole( stream, NULL, 
							  8 + MIN_OID_SIZE + \
							  MIN_PKCSIZE_ECCPOINT_THRESHOLD, 
							  DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		{
		status = readAlgoIDparam( stream, &cryptAlgo, &extraLength, 
								  ALGOID_CLASS_PKC );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( extraLength < MIN_OID_SIZE || extraLength > MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );
	if( capabilityInfoPtr->cryptAlgo != cryptAlgo )
		return( CRYPT_ERROR_BADDATA );

	/* Now things get messy, since the ECC standards authors carefully 
	   sidestepped having to make a decision about anything and instead
	   just created an open framework into which it's possible to drop
	   almost anything.  To keep things sane we require the use of named
	   curves (which most people seem to use) over a prime field */
	status = readECCOID( stream, &curveType );
	if( cryptStatusOK( status ) )
		status = getECCFieldSize( curveType, &fieldSize, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	eccKey->curveType = curveType;

	/* Set the maximum permitted actions.  Because of the special-case data 
	   formatting requirements for ECC algorithms (which are a part of the 
	   DLP algorithm family) we make the usage internal-only.  If the key is 
	   a pure public key rather than merely the public portions of a private 
	   key then the actions will be restricted by higher-level code to 
	   encrypt/signature-check only */
	if( cryptAlgo == CRYPT_ALGO_ECDSA )
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
									   ACTION_PERM_NONE_EXTERNAL );
		}
	else
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
									   ACTION_PERM_NONE_EXTERNAL );
		}

	/* Read the BIT STRING encapsulation and the public key fields.  Instead 
	   of encoding the necessary information as an obvious OID + SEQUENCE 
	   combination for the parameters it's all stuffed into an ad-hoc BIT 
	   STRING that we have to pick apart manually.  Note that we can't use 
	   the ECC p value for a range check because it hasn't been set yet, all 
	   that we have at this point is a curve ID */
	status = readBitStringHole( stream, &length, 
								MIN_PKCSIZE_ECCPOINT_THRESHOLD, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_PKCSIZE_ECCPOINT_THRESHOLD || \
		length > MAX_PKCSIZE_ECCPOINT )
		return( CRYPT_ERROR_BADDATA );
	status = sread( stream, buffer, length );
	if( cryptStatusError( status ) )
		return( status );
	status = importECCPoint( &eccKey->eccParam_qx, &eccKey->eccParam_qy,
							 buffer, length, MIN_PKCSIZE_ECC_THRESHOLD, 
							 CRYPT_MAX_PKCSIZE_ECC, fieldSize, NULL, 
							 KEYSIZE_CHECK_ECC );
	zeroise( buffer, length );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */
#endif /* USE_INT_ASN1 */

/****************************************************************************
*																			*
*								Read SSH Public Keys						*
*																			*
****************************************************************************/

#ifdef USE_SSH

/* Read SSHv2 public keys:

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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readSshRsaPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	char buffer[ 16 + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the wrapper and make sure that it's OK */
	readUint32( stream );
	status = readString32( stream, buffer, 7, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length != 7 || memcmp( buffer, "ssh-rsa", 7 ) )
		return( CRYPT_ERROR_BADDATA );

	/* Set the maximum permitted actions.  SSH keys are only used internally
	   so we restrict the usage to internal-only */
	*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
								   ACTION_PERM_NONE_EXTERNAL );

	/* Read the SSH public key information */
	status = readBignumInteger32( stream, &rsaKey->rsaParam_e, 
								  RSAPARAM_MIN_E, RSAPARAM_MAX_E, 
								  NULL );
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger32Checked( stream, &rsaKey->rsaParam_n,
											 RSAPARAM_MIN_N, RSAPARAM_MAX_N );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readSshDlpPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *dsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	char buffer[ 16 + 8 ];
	BOOLEAN isDH;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA ) );

	isDH = ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH ) ? TRUE : FALSE;

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the wrapper and make sure that it's OK.  SSHv2 uses PKCS #3 
	   rather than X9.42-style DH keys so we have to treat this algorithm 
	   type specially */
	readUint32( stream );
	if( isDH )
		{
		status = readString32( stream, buffer, 6, &length );
		if( cryptStatusError( status ) )
			return( status );
		if( length != 6 || memcmp( buffer, "ssh-dh", 6 ) )
			return( CRYPT_ERROR_BADDATA );

		/* Set the maximum permitted actions.  SSH keys are only used 
		   internally so we restrict the usage to internal-only.  Since DH 
		   keys can be both public and private keys we allow both usage 
		   types even though technically it's a public key */
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
									   ACTION_PERM_NONE_EXTERNAL );

		/* Read the SSH public key information */
		status = readBignumInteger32Checked( stream, &dsaKey->dlpParam_p,
											 DLPPARAM_MIN_P, DLPPARAM_MAX_P );
		if( cryptStatusOK( status ) )
			{
			status = readBignumInteger32( stream, &dsaKey->dlpParam_g,
										  DLPPARAM_MIN_G, DLPPARAM_MAX_G,
										  &dsaKey->dlpParam_p );
			}
		return( status );
		}

	/* It's a standard DLP key, read the wrapper and make sure that it's 
	   OK */
	status = readString32( stream, buffer, 7, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length != 7 || memcmp( buffer, "ssh-dss", 7 ) )
		return( CRYPT_ERROR_BADDATA );

	/* Set the maximum permitted actions.  SSH keys are only used internally
	   so we restrict the usage to internal-only */
	*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
								   ACTION_PERM_NONE_EXTERNAL );

	/* Read the SSH public key information */
	status = readBignumInteger32Checked( stream, &dsaKey->dlpParam_p,
										 DLPPARAM_MIN_P, DLPPARAM_MAX_P );
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger32( stream, &dsaKey->dlpParam_q,
									  DLPPARAM_MIN_Q, DLPPARAM_MAX_Q,
									  &dsaKey->dlpParam_p );
		}
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger32( stream, &dsaKey->dlpParam_g,
									  DLPPARAM_MIN_G, DLPPARAM_MAX_G,
									  &dsaKey->dlpParam_p );
		}
	if( cryptStatusOK( status ) && !isDH )
		{
		status = readBignumInteger32( stream, &dsaKey->dlpParam_y,
									  DLPPARAM_MIN_Y, DLPPARAM_MAX_Y,
									  &dsaKey->dlpParam_p );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dsaKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readSshEccPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	BYTE buffer[ MAX_PKCSIZE_ECCPOINT + 8 ];
	BOOLEAN isECDH;
	int length, fieldSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA );

	isECDH = ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDH ) ? TRUE : FALSE;

	/* No need to clear return value as usual since it's set in the 
	   following line of code, and clearing it leads to compiler warnings
	   about unused assignments */

	/* Set the maximum permitted actions.  SSH keys are only used 
	   internally so we restrict the usage to internal-only.  Since ECDH 
	   keys can be both public and private keys we allow both usage 
	   types even though technically it's a public key */
	if( isECDH )
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
									   ACTION_PERM_NONE_EXTERNAL ) | \
					   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
									   ACTION_PERM_NONE_EXTERNAL );
		}
	else
		{
		*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
									   ACTION_PERM_NONE_EXTERNAL );
		}

	/* Read the wrapper and make sure that it's OK.  The key parameter
	   information is repeated twice, so for the overall wrapper we only
	   check for the ECDH/ECDSA algorithm indication and get the parameter
	   information from the second version, which contains only the
	   parameter string */
	readUint32( stream );
	status = readString32( stream, buffer, CRYPT_MAX_TEXTSIZE, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 18 )		/* "ecdh-sha2-nistXXXX" */
		return( CRYPT_ERROR_BADDATA );
	if( isECDH )
		{
		if( memcmp( buffer, "ecdh-sha2-", 10 ) )
			return( CRYPT_ERROR_BADDATA );
		}
	else
		{
		if( memcmp( buffer, "ecdsa-sha2-", 11 ) )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Read and process the parameter information.  At this point we know 
	   that we've got valid ECC key data, so if we find anything unexpected 
	   we report it as an unavailable ECC field size rather than bad data */
	status = readString32( stream, buffer, CRYPT_MAX_TEXTSIZE, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length != 8 )		/* "nistXXXX" */
		return( CRYPT_ERROR_NOTAVAIL );
	if( !memcmp( buffer, "nistp256", 8 ) )
		eccKey->curveType = CRYPT_ECCCURVE_P256;
	else
		{
		if( !memcmp( buffer, "nistp384", 8 ) )
			eccKey->curveType = CRYPT_ECCCURVE_P384;
		else
			{
			if( !memcmp( buffer, "nistp521", 8 ) )
				eccKey->curveType = CRYPT_ECCCURVE_P521;
			else
				return( CRYPT_ERROR_NOTAVAIL );
			}
		}
	status = getECCFieldSize( eccKey->curveType, &fieldSize, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the ECC public key.  See the comments in 
	   readEccSubjectPublicKey() for why the checks are done the way they 
	   are */
	status = readString32( stream, buffer, MAX_PKCSIZE_ECCPOINT, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_PKCSIZE_ECCPOINT_THRESHOLD || \
		length > MAX_PKCSIZE_ECCPOINT )
		return( CRYPT_ERROR_BADDATA );
	status = importECCPoint( &eccKey->eccParam_qx, &eccKey->eccParam_qy,
							 buffer, length, MIN_PKCSIZE_ECC_THRESHOLD, 
							 CRYPT_MAX_PKCSIZE_ECC, fieldSize, NULL, 
							 KEYSIZE_CHECK_ECC );
	zeroise( buffer, length );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */
#endif /* USE_SSH */

/****************************************************************************
*																			*
*								Read SSL Public Keys						*
*																			*
****************************************************************************/

#ifdef USE_SSL

/* Read SSL public keys:

	DH:
		uint16		dh_pLen
		byte[]		dh_p
	  [	uint16		dh_qLen
		byte[]		dh_q		-- For TLS-ext format ]
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readSslDlpPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags,
								const BOOLEAN readExtKey )
	{
	PKC_INFO *dhKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DH );
	REQUIRES( readExtKey == TRUE || readExtKey == FALSE );

	/* No need to clear return value as usual since it's set in the 
	   following line of code, and clearing it leads to compiler warnings
	   about unused assignments */

	/* Set the maximum permitted actions.  SSL keys are only used 
	   internally so we restrict the usage to internal-only.  Since DH 
	   keys can be both public and private keys we allow both usage 
	   types even though technically it's a public key */
	*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
								   ACTION_PERM_NONE_EXTERNAL ) | \
				   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
								   ACTION_PERM_NONE_EXTERNAL );

	/* Read the SSL public key information */
	status = readBignumInteger16UChecked( stream, &dhKey->dlpParam_p,
										  DLPPARAM_MIN_P, DLPPARAM_MAX_P );
	if( cryptStatusOK( status ) && readExtKey )
		{
		status = readBignumInteger16U( stream, &dhKey->dlpParam_q, 
									   DLPPARAM_MIN_Q, DLPPARAM_MAX_Q,
									   &dhKey->dlpParam_p );
		}
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger16U( stream, &dhKey->dlpParam_g, 
									   DLPPARAM_MIN_G, DLPPARAM_MAX_G,
									   &dhKey->dlpParam_p );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dhKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH )

static const MAP_TABLE sslCurveInfo[] = {
	{ /* TLS_CURVE_SECP256R1 */ 23, CRYPT_ECCCURVE_P256 },
	{ /* TLS_CURVE_SECP384R1 */ 24, CRYPT_ECCCURVE_P384 },
	{ /* TLS_CURVE_SECP521R1 */ 25, CRYPT_ECCCURVE_P521 },
	{ /* TLS_CURVE_BRAINPOOLP256R1 */ 26, CRYPT_ECCCURVE_BRAINPOOL_P256 },
	{ /* TLS_CURVE_BRAINPOOLP384R1 */ 27, CRYPT_ECCCURVE_BRAINPOOL_P384 },
	{ /* TLS_CURVE_BRAINPOOLP512R1 */ 28, CRYPT_ECCCURVE_BRAINPOOL_P512 },
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readSslEccPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	const MAP_TABLE *sslCurveInfoPtr;
	int value, curveID, sslCurveInfoNoEntries, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDH );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Set the maximum permitted actions.  SSL keys are only used 
	   internally so we restrict the usage to internal-only.  Since ECDH 
	   keys can be both public and private keys we allow both usage 
	   types even though technically it's a public key */
	*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
								   ACTION_PERM_NONE_EXTERNAL ) | \
				   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
								   ACTION_PERM_NONE_EXTERNAL );

	/* Read the SSL public key information */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != 0x03 )		/* NamedCurve */
		return( CRYPT_ERROR_BADDATA );
	status = value = readUint16( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value < 19 || value > 28 )
		return( CRYPT_ERROR_NOTAVAIL );

	/* Look up the curve ID based on the SSL NamedCurve ID */
	status = getEccSslInfoTbl( &sslCurveInfoPtr, &sslCurveInfoNoEntries );
	if( cryptStatusError( status ) )
		return( status );
	status = mapValue( value, &curveID, sslCurveInfoPtr, 
					   sslCurveInfoNoEntries );
	if( cryptStatusError( status ) )
		return( status );
	eccKey->curveType = curveID;

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH */
#endif /* USE_SSL */

/****************************************************************************
*																			*
*								Read PGP Public Keys						*
*																			*
****************************************************************************/

#ifdef USE_PGP 

/* Read PGP public keys:

	byte		version
	uint32		creationTime
	[ uint16	validity - version 2 or 3 only ]
	byte		RSA		DSA		Elgamal		ECDH/ECDSA
	mpi			n		p		p			qx/qy
	mpi			e		q		g
	mpi					g		y
	mpi					y */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPgpHeader( INOUT STREAM *stream, 
						  OUT time_t *pgpCreationTime,
						  const BOOLEAN openPgpOnly )
	{
	int version, value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpCreationTime, sizeof( time_t ) ) );

	REQUIRES( openPgpOnly == TRUE || openPgpOnly == FALSE );

	/* Clear return value */
	*pgpCreationTime = 0;

	/* Read the PGP version info */
	status = version = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( openPgpOnly )
		{
		if( version != PGP_VERSION_OPENPGP )
			return( CRYPT_ERROR_BADDATA );
		}
	else
		{
		if( version != PGP_VERSION_2 && version != PGP_VERSION_3 && \
			version != PGP_VERSION_OPENPGP )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Read the creation time.  In theory we should do this with
	   readUint32Time(), however some keys coming from non-PGP sources can
	   have a creation time of zero which would result in the value being
	   rejected by the time read.  Because of this we have to read the value
	   as a standard 32-bit int and the perform the range check that's 
	   otherwise performed by readUint32Time(), with an exception for a time
	   value of 0 */
	status = value = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != 0 )
		{
		if( value < MIN_STORED_TIME_VALUE || \
			value >= MAX_TIME_VALUE )
			return( CRYPT_ERROR_BADDATA );
		}
	*pgpCreationTime = ( time_t ) value;

	/* Skip the validity time if it's present */
	if( version == PGP_VERSION_2 || version == PGP_VERSION_3 )
		return( sSkip( stream, 2, 2 ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readPgpRsaPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *rsaKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_RSA );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the header info */
	status = readPgpHeader( stream, &rsaKey->pgpCreationTime, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Set the maximum permitted actions.  If there are no restrictions we
	   allow external usage, if the keys are encryption-only or signature-
	   only we make the usage internal-only because of RSA's signature/
	   encryption duality.  If the key is a pure public key rather than 
	   merely the public portions of a private key then the actions will be 
	   restricted by higher-level code to encrypt/signature-check only */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( value )
		{
		case PGP_ALGO_RSA:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
										   ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
										   ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
										   ACTION_PERM_ALL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
										   ACTION_PERM_ALL );
			break;

		case PGP_ALGO_RSA_ENCRYPT:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
										   ACTION_PERM_NONE_EXTERNAL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
										   ACTION_PERM_NONE_EXTERNAL );
			break;

		case PGP_ALGO_RSA_SIGN:
			*actionFlags |= MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
											ACTION_PERM_NONE_EXTERNAL ) | \
							MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
											ACTION_PERM_NONE_EXTERNAL );
			break;

		default:
			return( CRYPT_ERROR_BADDATA );
		}

	/* Read the PGP public key information */
	status = readBignumInteger16UbitsChecked( stream, &rsaKey->rsaParam_n, 
											  bytesToBits( RSAPARAM_MIN_N ), 
											  bytesToBits( RSAPARAM_MAX_N ) );
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger16Ubits( stream, &rsaKey->rsaParam_e, 
										   bytesToBits( RSAPARAM_MIN_E ), 
										   bytesToBits( RSAPARAM_MAX_E ),
										   &rsaKey->rsaParam_n );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( rsaKey ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readPgpDlpPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	PKC_INFO *dlpKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_DSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ELGAMAL ) );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the header info */
	status = readPgpHeader( stream, &dlpKey->pgpCreationTime, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Set the maximum permitted actions.  Because of the special-case data 
	   formatting requirements for DLP algorithms we make the usage 
	   internal-only.  If the key is a pure public key rather than merely 
	   the public portions of a private key then the actions will be 
	   restricted by higher-level code to encrypt/signature-check only  */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( value )
		{
		case PGP_ALGO_DSA:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
										   ACTION_PERM_NONE_EXTERNAL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
										   ACTION_PERM_NONE_EXTERNAL );
			break;
		
		case PGP_ALGO_ELGAMAL:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
										   ACTION_PERM_NONE_EXTERNAL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
										   ACTION_PERM_NONE_EXTERNAL );
			break;

		default:
			return( CRYPT_ERROR_BADDATA );
		}

	/* Read the PGP public key information */
	status = readBignumInteger16UbitsChecked( stream, &dlpKey->dlpParam_p, 
											  bytesToBits( DLPPARAM_MIN_P ), 
											  bytesToBits( DLPPARAM_MAX_P ) );
	if( cryptStatusOK( status ) && value == PGP_ALGO_DSA )
		{
		status = readBignumInteger16Ubits( stream, &dlpKey->dlpParam_q, 
										   bytesToBits( DLPPARAM_MIN_Q ), 
										   bytesToBits( DLPPARAM_MAX_Q ),
										   &dlpKey->dlpParam_p );
		}
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger16Ubits( stream, &dlpKey->dlpParam_g, 
										   bytesToBits( DLPPARAM_MIN_G ), 
										   bytesToBits( DLPPARAM_MAX_G ),
										   &dlpKey->dlpParam_p );
		}
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger16Ubits( stream, &dlpKey->dlpParam_y, 
										   bytesToBits( DLPPARAM_MIN_Y ), 
										   bytesToBits( DLPPARAM_MAX_Y ),
										   &dlpKey->dlpParam_p );
		}
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckPKCInfo( dlpKey ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readPgpEccPublicKey( INOUT STREAM *stream, 
								INOUT CONTEXT_INFO *contextInfoPtr,
								OUT_FLAGS_Z( ACTION_PERM ) int *actionFlags )
	{
	CRYPT_ECCCURVE_TYPE curveType;
	PKC_INFO *eccKey = contextInfoPtr->ctxPKC;
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	STREAM oidStream;
	BYTE oidBuffer[ 2 + MAX_OID_SIZE + 8 ];
	int value, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isWritePtr( actionFlags, sizeof( int ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDH ) );

	/* Clear return value */
	*actionFlags = ACTION_PERM_NONE;

	/* Read the header info */
	status = readPgpHeader( stream, &eccKey->pgpCreationTime, TRUE );
	if( cryptStatusError( status ) )
		return( status );

	/* Set the maximum permitted actions.  Because of the special-case data 
	   formatting requirements for ECC algorithms (which are a part of the 
	   DLP algorithm family) we make the usage internal-only.  If the key is 
	   a pure public key rather than merely the public portions of a private 
	   key then the actions will be restricted by higher-level code to 
	   encrypt/signature-check only */
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	switch( value )
		{
		case PGP_ALGO_ECDH:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, \
										   ACTION_PERM_NONE_EXTERNAL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, \
										   ACTION_PERM_NONE_EXTERNAL );
			break;

		case PGP_ALGO_ECDSA:
			*actionFlags = MK_ACTION_PERM( MESSAGE_CTX_SIGN, \
										   ACTION_PERM_NONE_EXTERNAL ) | \
						   MK_ACTION_PERM( MESSAGE_CTX_SIGCHECK, \
										   ACTION_PERM_NONE_EXTERNAL );
			break;

		default:
			return( CRYPT_ERROR_BADDATA );
		}

	/* Read the ECC OID, preceded by a length byte.  For no known reason 
	   (probably a coding error that was made part of the spec), the PGP
	   format omits the first two bytes of the OID, so we have to read the
	   value into an intermediate buffer and recreate the full OID from 
	   it */
	status = length = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_OID_SIZE || length >= MAX_OID_SIZE )
		return( CRYPT_ERROR_BADDATA );
	oidBuffer[ 0 ] = 0x06;		/* OID tag */
	oidBuffer[ 1 ] = intToByte( length );
	status = sread( stream, oidBuffer + 2, length );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &oidStream, oidBuffer, 2 + length );
	status = readECCOID( &oidStream, &curveType );
	sMemDisconnect( &oidStream );
	if( cryptStatusError( status ) )
		return( status );
	eccKey->curveType = curveType;

	ENSURES( sanityCheckPKCInfo( eccKey ) );

	/* We don't do PGP ECC yet since there's barely any use of it to test
	   against */
	return( CRYPT_ERROR_BADDATA );
	}
#endif /* USE_ECDH || USE_ECDSA */
#endif /* USE_PGP */

/****************************************************************************
*																			*
*							Public-Key Read Interface						*
*																			*
****************************************************************************/

/* Umbrella public-key read functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int completePubkeyRead( INOUT CONTEXT_INFO *contextInfoPtr,
							   IN_FLAGS( ACTION_PERM ) const int actionFlags )
	{
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( isFlagRange( actionFlags, ACTION_PERM ) );

	/* If it's statically-initialised context data used in the self-test 
	   then there's no corresponding cryptlib object and we're done */
	if( TEST_FLAG( contextInfoPtr->flags, CONTEXT_FLAG_STATICCONTEXT ) )
		return( CRYPT_OK );

	/* Set the action permissions for the context */
	return( krnlSendMessage( contextInfoPtr->objectHandle, 
							 IMESSAGE_SETATTRIBUTE, 
							 ( MESSAGE_CAST ) &actionFlags, 
							 CRYPT_IATTRIBUTE_ACTIONPERMS ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPublicKeyRsaFunction( INOUT STREAM *stream, 
									 INOUT CONTEXT_INFO *contextInfoPtr,
									 IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									 STDC_UNUSED const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int actionFlags, status;

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
		case KEYFORMAT_CERT:
			status = readRsaSubjectPublicKey( stream, contextInfoPtr, 
											  &actionFlags );
			break;
#endif /* USE_INT_ASN1 */

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			status = readSshRsaPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_SSH */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			status = readPgpRsaPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_PGP */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	return( completePubkeyRead( contextInfoPtr, actionFlags ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPublicKeyDlpFunction( INOUT STREAM *stream, 
									 INOUT CONTEXT_INFO *contextInfoPtr,
									 IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									 STDC_UNUSED const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int actionFlags, status;

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
		case KEYFORMAT_CERT:
			status = readDlpSubjectPublicKey( stream, contextInfoPtr, 
											  &actionFlags );
			break;
#endif /* USE_INT_ASN1 */

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			status = readSshDlpPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_SSH */

#ifdef USE_SSL
		case KEYFORMAT_SSL:
		case KEYFORMAT_SSL_EXT:
			status = readSslDlpPublicKey( stream, contextInfoPtr, 
									&actionFlags,
									( formatType == KEYFORMAT_SSL_EXT ) ? \
									  TRUE : FALSE );
			break;
#endif /* USE_SSL */
		
#ifdef USE_PGP
		case KEYFORMAT_PGP:
			status = readPgpDlpPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_PGP */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	return( completePubkeyRead( contextInfoPtr, actionFlags ) );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPublicKeyEccFunction( INOUT STREAM *stream, 
									 INOUT CONTEXT_INFO *contextInfoPtr,
									 IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType,
									 STDC_UNUSED const BOOLEAN checkRead )
	{
	const CAPABILITY_INFO *capabilityInfoPtr = \
								DATAPTR_GET( contextInfoPtr->capabilityInfo );
	int actionFlags, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( capabilityInfoPtr != NULL );
	REQUIRES( contextInfoPtr->type == CONTEXT_PKC && \
			  ( capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDSA || \
				capabilityInfoPtr->cryptAlgo == CRYPT_ALGO_ECDH ) );
	REQUIRES( formatType == KEYFORMAT_CERT || formatType == KEYFORMAT_SSL || \
			  formatType == KEYFORMAT_SSL_EXT || formatType == KEYFORMAT_SSH || \
			  formatType == KEYFORMAT_PGP );
	REQUIRES( checkRead == TRUE || checkRead == FALSE );

	switch( formatType )
		{
#ifdef USE_INT_ASN1
		case KEYFORMAT_CERT:
			status = readEccSubjectPublicKey( stream, contextInfoPtr, 
											  &actionFlags );
			break;
#endif /* USE_INT_ASN1 */

		/* TLS only uses ECDH (the ECDSA key data is conveyed in a 
		   certificate) so we only enable the TLS format if ECDH is defined 
		   rather than ECDH or ECDSA */
#if defined( USE_SSL ) && defined( USE_ECDH )
		case KEYFORMAT_SSL:
		case KEYFORMAT_SSL_EXT:
			status = readSslEccPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_SSL && USE_ECDH */

#ifdef USE_SSH
		case KEYFORMAT_SSH:
			status = readSshEccPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_SSH */

#ifdef USE_PGP
		case KEYFORMAT_PGP:
			status = readPgpEccPublicKey( stream, contextInfoPtr, 
										  &actionFlags );
			break;
#endif /* USE_SSH */

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	return( completePubkeyRead( contextInfoPtr, actionFlags ) );
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*								Read DL Values								*
*																			*
****************************************************************************/

/* Unlike the simpler RSA PKC, DL-based PKCs produce a pair of values that
   need to be encoded as structured data.  The following two functions 
   decode the encoded forms from various formats.  SSH assumes that DLP 
   values are two fixed-size blocks of 20 bytes so we can't use the normal 
   read/write routines to handle these values */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int decodeDLValuesFunction( IN_BUFFER( bufSize ) const BYTE *buffer, 
								   IN_LENGTH_SHORT_MIN( 32 ) const int bufSize, 
								   INOUT BIGNUM *value1, 
								   INOUT BIGNUM *value2, 
								   const BIGNUM *maxRange,
								   IN_ENUM( CRYPT_FORMAT )  \
										const CRYPT_FORMAT_TYPE formatType )
	{
	STREAM stream;
	int status;

	assert( isReadPtrDynamic( buffer, bufSize ) );
	assert( isWritePtr( value1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( value2, sizeof( BIGNUM ) ) );
	assert( isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( bufSize >= 32 && bufSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isEnumRange( formatType, CRYPT_FORMAT ) );

	/* Clear return values */
	BN_clear( value1 );
	BN_clear( value2 );

	sMemConnect( &stream, buffer, bufSize );

	/* Read the DL components from the buffer and make sure that they're 
	   valid, i.e. that they're in the range [1...maxRange - 1] (the lower
	   bound is actually DLPPARAM_MIN_SIG_x and not 1, which is > 100 bits).  
	   Although nominally intended for DLP algorithms the DLPPARAM_MIN_SIG_x 
	   values also work for ECC ones since they're also in the DLP family */
	switch( formatType )
		{
#ifdef USE_INT_ASN1
		case CRYPT_FORMAT_CRYPTLIB:
			readSequence( &stream, NULL );
			status = readBignum( &stream, value1, DLPPARAM_MIN_SIG_R,
								 CRYPT_MAX_PKCSIZE, maxRange );
			if( cryptStatusError( status ) )
				break;
			status = readBignum( &stream, value2, DLPPARAM_MIN_SIG_S,
								 CRYPT_MAX_PKCSIZE, maxRange );
			break;
#endif /* USE_INT_ASN1 */

#ifdef USE_PGP
		case CRYPT_FORMAT_PGP:
			status = readBignumInteger16Ubits( &stream, value1, 
											   DLPPARAM_MIN_SIG_R,
											   bytesToBits( CRYPT_MAX_PKCSIZE ),
											   maxRange );
			if( cryptStatusError( status ) )
				break;
			status = readBignumInteger16Ubits( &stream, value2, 
											   DLPPARAM_MIN_SIG_S,
											   bytesToBits( CRYPT_MAX_PKCSIZE ),
											   maxRange );
			break;
#endif /* USE_PGP */
	
#ifdef USE_SSH
		case CRYPT_IFORMAT_SSH:
			status = importBignum( value1, buffer, 20, DLPPARAM_MIN_SIG_R, 
								   20, maxRange, KEYSIZE_CHECK_NONE );
			if( cryptStatusError( status ) )
				break;
			status = importBignum( value2, buffer + 20, 20, DLPPARAM_MIN_SIG_S, 
								   20, maxRange, KEYSIZE_CHECK_NONE );
			break;
#endif /* USE_SSH */

		default:
			retIntError();
		}

	/* Clean up */
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckBignum( value1 ) );
	ENSURES( sanityCheckBignum( value2 ) );

	return( CRYPT_OK );
	}

#if defined( USE_ECDH ) || defined( USE_ECDSA )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int decodeECDLValuesFunction( IN_BUFFER( bufSize ) const BYTE *buffer, 
									 IN_LENGTH_SHORT_MIN( 32 ) const int bufSize, 
									 INOUT BIGNUM *value1, 
									 INOUT BIGNUM *value2, 
									 const BIGNUM *maxRange,
									 IN_ENUM( CRYPT_FORMAT )  \
										const CRYPT_FORMAT_TYPE formatType )
	{
	STREAM stream;
	int status;

	assert( isReadPtrDynamic( buffer, bufSize ) );
	assert( isWritePtr( value1, sizeof( BIGNUM ) ) );
	assert( isWritePtr( value2, sizeof( BIGNUM ) ) );
	assert( isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES( bufSize >= 32 && bufSize < MAX_INTLENGTH_SHORT );
	REQUIRES( isEnumRange( formatType, CRYPT_FORMAT ) );

	/* Clear return values */
	BN_clear( value1 );
	BN_clear( value2 );

	/* In most cases the DLP and ECDLP formats are identical and we can just
	   pass the call on to the DLP form, however SSH uses totally different 
	   signature formats depending on whether the signature is DSA or ECDSA, 
	   so we handle the SSH format explicitly here */
	if( formatType != CRYPT_IFORMAT_SSH )
		{
		return( decodeDLValuesFunction( buffer, bufSize, value1, value2, 
										maxRange, formatType ) );
		}
	sMemConnect( &stream, buffer, bufSize );
	status = readBignumInteger32( &stream, value1, ECCPARAM_MIN_SIG_R,
								  CRYPT_MAX_PKCSIZE_ECC, maxRange );
	if( cryptStatusOK( status ) )
		{
		status = readBignumInteger32( &stream, value2, ECCPARAM_MIN_SIG_S,
									  CRYPT_MAX_PKCSIZE_ECC, maxRange );
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckBignum( value1 ) );
	ENSURES( sanityCheckBignum( value2 ) );

	return( CRYPT_OK );
	}
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*							Context Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initPubKeyRead( INOUT CONTEXT_INFO *contextInfoPtr )
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
		FNPTR_SET( pkcInfo->readPublicKeyFunction, readPublicKeyDlpFunction );
		FNPTR_SET( pkcInfo->decodeDLValuesFunction, decodeDLValuesFunction );
		return;
		}
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( isEccAlgo( cryptAlgo ) )
		{
		FNPTR_SET( pkcInfo->readPublicKeyFunction, readPublicKeyEccFunction );
		FNPTR_SET( pkcInfo->decodeDLValuesFunction, decodeECDLValuesFunction );
		return;
		}
#endif /* USE_ECDH || USE_ECDSA */
	FNPTR_SET( pkcInfo->readPublicKeyFunction, readPublicKeyRsaFunction );
	}
#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPublicKeyNullFunction( INOUT STREAM *stream, 
									  INOUT CONTEXT_INFO *contextInfoPtr,
									  IN_ENUM( KEYFORMAT )  \
										const KEYFORMAT_TYPE formatType )
	{
	return( CRYPT_ERROR_NOTAVAIL );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void initPubKeyRead( INOUT CONTEXT_INFO *contextInfoPtr )
	{
	REQUIRES_V( sanityCheckContext( contextInfoPtr ) );

	FNPTR_SET( pkcInfo->readPublicKeyFunction, readPublicKeyNullFunction );
	}
#endif /* USE_PKC */
