/****************************************************************************
*																			*
*								Mechanism ACLs								*
*						Copyright Peter Gutmann 1997-2004					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

/****************************************************************************
*																			*
*								Mechanism ACLs								*
*																			*
****************************************************************************/

/* The ACL tables for each mechanism class */

static const MECHANISM_ACL FAR_BSS mechanismWrapACL[] = {
	/* PKCS #1 encrypt */
	{ MECHANISM_ENC_PKCS1,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 MAX_PKCENCRYPTED_SIZE ),
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_HIGH_STATE ),		/* Ctx containing key */
		MKACP_O( ST_CTX_PKC,				/* Wrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #1 encrypt using PGP formatting */
#ifdef USE_PGP
	{ MECHANISM_ENC_PKCS1_PGP,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 MAX_PKCENCRYPTED_SIZE ),
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV,				/* Ctx containing key */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CTX_PKC,				/* Wrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PGP */

	/* PKCS #1 encrypt of raw data */
	{ MECHANISM_ENC_PKCS1_RAW,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped raw data */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_S( MIN_KEYSIZE,				/* Raw data */
				 CRYPT_MAX_KEYSIZE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CTX_PKC,				/* Wrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* OAEP encrypt */
#ifdef USE_OAEP
	{ MECHANISM_ENC_OAEP,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_HIGH_STATE ),		/* Ctx containing key */
		MKACP_O( ST_CTX_PKC,				/* Wrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N( CRYPT_ALGO_SHA1, CRYPT_ALGO_SHAng ) } },
			/* The algoID CRYPT_ALGO_SHA2 + 1 (= CRYPT_ALGO_SHAng) is a 
			   special-case placeholder for SHA2-512 until its fate/
			   potential future usage becomes a bit clearer */
#endif /* USE_OAEP */

	/* CMS key wrap */
	{ MECHANISM_ENC_CMS,
	  { MKACP_S_OPT( 8 + 8, CRYPT_MAX_KEYSIZE + 16 ),/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_HIGH_STATE ),		/* Ctx containing key */
		MKACP_O( ST_CTX_CONV,				/* Wrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #15 private key wrap */
	{ MECHANISM_PRIVATEKEYWRAP,
	  { MKACP_S_OPT( MIN_PRIVATE_KEYSIZE, \
					 MAX_PRIVATE_KEYSIZE ),	/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx containing private key */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_O( ST_CTX_CONV,				/* Wrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #8 private key wrap */
#ifdef USE_PKCS12
	{ MECHANISM_PRIVATEKEYWRAP_PKCS8,
	  { MKACP_S_OPT( MIN_PRIVATE_KEYSIZE, \
					 MAX_PRIVATE_KEYSIZE ),	/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx containing private key */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_O( ST_CTX_CONV,				/* Wrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PKCS12 */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

static const MECHANISM_ACL FAR_BSS mechanismUnwrapACL[] = {
	/* PKCS #1 decrypt */
	{ MECHANISM_ENC_PKCS1,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 MAX_PKCENCRYPTED_SIZE ),
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_LOW_STATE ),		/* Ctx to contain key */
		MKACP_O( ST_CTX_PKC,				/* Unwrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #1 decrypt using PGP formatting */
#ifdef USE_PGP
	{ MECHANISM_ENC_PKCS1_PGP,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 MAX_PKCENCRYPTED_SIZE ),
		MKACP_S_NONE(),
		MKACP_N_FIXED( CRYPT_UNUSED ),		/* Placeholder for ctx to contain key */
		MKACP_O( ST_CTX_PKC,				/* Unwrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PGP */

	/* PKCS #1 decrypt of raw data */
	{ MECHANISM_ENC_PKCS1_RAW,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped raw data */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_S( MIN_KEYSIZE,				/* Raw data */
				 CRYPT_MAX_PKCSIZE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CTX_PKC,				/* Unwrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* OAEP decrypt */
#ifdef USE_OAEP
	{ MECHANISM_ENC_OAEP,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Wrapped key */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_LOW_STATE ),		/* Ctx to contain key */
		MKACP_O( ST_CTX_PKC,				/* Unwrap PKC context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N( CRYPT_ALGO_SHA1, CRYPT_ALGO_SHAng ) } },
			/* The algoID CRYPT_ALGO_SHA2 + 1 (= CRYPT_ALGO_SHAng) is a 
			   special-case placeholder for SHA2-512 until its fate/
			   potential future usage becomes a bit clearer */
#endif /* USE_OAEP */

	/* CMS key unwrap */
	{ MECHANISM_ENC_CMS,
	  { MKACP_S( 8 + 8, CRYPT_MAX_KEYSIZE + 16 ),/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_CONV | ST_CTX_MAC | ST_CTX_GENERIC,
				 ACL_FLAG_LOW_STATE ),		/* Ctx to contain key */
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #15 private key unwrap */
	{ MECHANISM_PRIVATEKEYWRAP,
	  { MKACP_S( MIN_PRIVATE_KEYSIZE, \
				 MAX_PRIVATE_KEYSIZE ),		/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx to contain private key */
				 ACL_FLAG_LOW_STATE ),
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* PKCS #8 private key unwrap */
#ifdef USE_PKCS12
	{ MECHANISM_PRIVATEKEYWRAP_PKCS8,
	  { MKACP_S( MIN_PRIVATE_KEYSIZE, \
				 MAX_PRIVATE_KEYSIZE ),		/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx to contain private key */
				 ACL_FLAG_LOW_STATE ),
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PKCS12 */

	/* PGP 2.x private key unwrap */
#ifdef USE_PGPKEYS
	{ MECHANISM_PRIVATEKEYWRAP_PGP2,
	  { MKACP_S( MIN_PRIVATE_KEYSIZE, \
				 MAX_PRIVATE_KEYSIZE ),		/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx to contain private key */
				 ACL_FLAG_LOW_STATE ),
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PGPKEYS */

	/* PGP 5.x private key unwrap */
#ifdef USE_PGPKEYS
	{ MECHANISM_PRIVATEKEYWRAP_OPENPGP_OLD,
	  { MKACP_S( MIN_PRIVATE_KEYSIZE, \
				 MAX_PRIVATE_KEYSIZE ),		/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx to contain private key */
				 ACL_FLAG_LOW_STATE ),
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PGPKEYS */

	/* OpenPGP private key unwrap */
#ifdef USE_PGPKEYS
	{ MECHANISM_PRIVATEKEYWRAP_OPENPGP,
	  { MKACP_S( MIN_PRIVATE_KEYSIZE, \
				 MAX_PRIVATE_KEYSIZE ),		/* Wrapped key */
		MKACP_S_NONE(),
		MKACP_O( ST_CTX_PKC,				/* Ctx to contain private key */
				 ACL_FLAG_LOW_STATE ),
		MKACP_O( ST_CTX_CONV,				/* Unwrap context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },
#endif /* USE_PGPKEYS */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

static const MECHANISM_ACL FAR_BSS mechanismSignACL[] = {
	/* PKCS #1 sign */
	{ MECHANISM_SIG_PKCS1,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Signature */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_O( ST_CTX_HASH,				/* Hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),		/* Secondary hash context */
		MKACP_O( ST_CTX_PKC,				/* Signing context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ) } },

	/* SSL sign with dual hashes */
#ifdef USE_SSL
	{ MECHANISM_SIG_SSL,
	  { MKACP_S_OPT( MIN_PKCSIZE,			/* Signature */
					 CRYPT_MAX_PKCSIZE ),
		MKACP_O( ST_CTX_HASH,				/* Hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CTX_HASH,				/* Secondary hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CTX_PKC,				/* Signing context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ) } },
#endif /* USE_SSL */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

static const MECHANISM_ACL FAR_BSS mechanismSigCheckACL[] = {
	/* PKCS #1 sig check */
	{ MECHANISM_SIG_PKCS1,
	  { MKACP_S( MIN_PKCSIZE,				/* Signature */
				 CRYPT_MAX_PKCSIZE ),
		MKACP_O( ST_CTX_HASH,				/* Hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ),		/* Secondary hash context */
		MKACP_O( ST_CTX_PKC,				/* Sig.check context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ) } },

	/* SSL sign with dual hashes */
#ifdef USE_SSL
	{ MECHANISM_SIG_SSL,
	  { MKACP_S( MIN_PKCSIZE,				/* Signature */
				 CRYPT_MAX_PKCSIZE ),
		MKACP_O( ST_CTX_HASH,				/* Hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CTX_HASH,				/* Secondary hash context */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CTX_PKC,				/* Sig.check context */
				 ACL_FLAG_HIGH_STATE | ACL_FLAG_ROUTE_TO_CTX ) } },
#endif /* USE_SSL */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

static const MECHANISM_ACL FAR_BSS mechanismDeriveACL[] = {
	/* PKCS #5 derive */
	{ MECHANISM_DERIVE_PKCS5,
	  { MKACP_S( 1, CRYPT_MAX_KEYSIZE ),	/* Key data */
		MKACP_S( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE ),/* Keying material */
		MKACP_N( CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_HMAC_SHAng ),/* Hash algo */
		MKACP_N( 0, CRYPT_MAX_HASHSIZE ),	/* Hash parameters */
		MKACP_S( 4, 512 ),					/* Salt */
		MKACP_N( 1, MAX_KEYSETUP_ITERATIONS ) } },	/* Iterations */

	/* SSL derive */
#ifdef USE_SSL
	{ MECHANISM_DERIVE_SSL,
	  { MKACP_S( 48, 512 ),					/* Master secret/key data */
		MKACP_S( 48, CRYPT_MAX_PKCSIZE ),	/* Premaster secret/master secret */
		MKACP_N_FIXED( CRYPT_USE_DEFAULT ),	/* SSL uses dual hash */
		MKACP_N( 0, 0 ),					/* Hash parameters */
		MKACP_S( 64, 64 ),					/* Salt */
		MKACP_N( 1, 1 ) } },				/* Iterations */
#endif /* USE_SSL */

	/* TLS/TLS 1.2 derive.  The odd lower bounds on the output and salt are 
	   needed when generating the TLS hashed MAC and (for the salt and 
	   output) and when generating a master secret from a fixed shared key 
	   (for the input) */
#ifdef USE_SSL
	{ MECHANISM_DERIVE_TLS,
	  { MKACP_S( 12, 512 ),					/* Master secret/key data (usually 48) */
		MKACP_S( 6, CRYPT_MAX_PKCSIZE ),	/* Premaster secret/master secret (us'ly 48) */
		MKACP_N_FIXED( CRYPT_USE_DEFAULT ),	/* TLS uses dual hash */
		MKACP_N( 0, 0 ),					/* Hash parameters */
		MKACP_S( 13, 512 ),					/* Salt (usually 64) */
		MKACP_N( 1, 1 ) } },				/* Iterations */
	{ MECHANISM_DERIVE_TLS12,
	  { MKACP_S( 12, 512 ),					/* Master secret/key data (usually 48) */
		MKACP_S( 6, CRYPT_MAX_PKCSIZE ),	/* Premaster secret/master secret (us'ly 48) */
		MKACP_N( CRYPT_ALGO_SHA2, CRYPT_ALGO_SHAng ),/* Hash algo */
		MKACP_N( 0, CRYPT_MAX_HASHSIZE ),	/* Hash parameters */
		MKACP_S( 13, 512 ),					/* Salt (usually 64) */
		MKACP_N( 1, 1 ) } },				/* Iterations */
#endif /* USE_SSL */

	/* CMP/Entrust derive */
#ifdef USE_CMP
	{ MECHANISM_DERIVE_CMP,
	  { MKACP_S( 20, 20 ),					/* HMAC-SHA key */
		MKACP_S( 1, 512 ),					/* Key data */
		MKACP_N( CRYPT_ALGO_SHA1, CRYPT_ALGO_SHA1 ),/* Hash algo */
		MKACP_N( 0, 0 ),					/* Hash parameters */
		MKACP_S( 1, 512 ),					/* Salt */
		MKACP_N( 1, MAX_KEYSETUP_ITERATIONS ) } },	/* Iterations */
#endif /* USE_CMP */

	/* OpenPGP S2K derive.  The MAX_KEYSETUP_HASHSPECIFIER bound on the 
	   iterations instead of the more usual MAX_KEYSETUP_ITERATIONS is 
	   because of PGP's strange handling of this value by counting bytes to 
	   process through the PRF rather than actual PRF iterations */
#if defined( USE_PGP ) || defined( USE_PGPKEYS )
	{ MECHANISM_DERIVE_PGP,
	  { MKACP_S( 16, CRYPT_MAX_KEYSIZE ),	/* Key data */
		MKACP_S( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE ),/* Keying material */
		MKACP_N( CRYPT_ALGO_MD5, CRYPT_ALGO_SHA256 ),/* Hash algo */
		MKACP_N( 0, 0 ),					/* Hash parameters */
		MKACP_S( 8, 8 ),					/* Salt */
		MKACP_N( 0, MAX_KEYSETUP_HASHSPECIFIER ) } }, /* Iterations (0 = don't iterate) */
#endif /* USE_PGP || USE_PGPKEYS */

	/* PKCS #12 derive */
#ifdef USE_PKCS12
	{ MECHANISM_DERIVE_PKCS12,
	  { MKACP_S( 5, CRYPT_MAX_KEYSIZE ),	/* Key data (5 for RC2-40) */
		MKACP_S( MIN_NAME_LENGTH, CRYPT_MAX_TEXTSIZE ),/* Keying material */
		MKACP_N( CRYPT_ALGO_SHA1, CRYPT_ALGO_SHA1 ),/* Hash algo */
		MKACP_N( 0, 0 ),					/* Hash parameters */
		MKACP_S( 9, 512 ),					/* Salt (+ ID byte) */
		MKACP_N( 1, MAX_KEYSETUP_ITERATIONS ) } },	/* Iterations */
#endif /* USE_PKCS12 */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

static const MECHANISM_ACL FAR_BSS mechanismKDFACL[] = {
	/* PKCS #5 KDF */
	{ MECHANISM_DERIVE_PKCS5,
	  { MKACP_O( ST_CTX_CONV | ST_CTX_MAC, 
				 ACL_FLAG_LOW_STATE ),		/* Key data */
		MKACP_O( ST_CTX_GENERIC,
				 ACL_FLAG_HIGH_STATE ),		/* Keying material */
		MKACP_N( CRYPT_ALGO_HMAC_SHA1, CRYPT_ALGO_HMAC_SHAng ),/* Hash algo */
		MKACP_N( 0, CRYPT_MAX_HASHSIZE ),	/* Hash parameters */
		MKACP_S( 8, CRYPT_MAX_TEXTSIZE ) } },	/* Salt */

	/* End-of-ACL marker */
	{ MECHANISM_NONE,
	  { MKACP_END() } },
	{ MECHANISM_NONE,
	  { MKACP_END() } }
	};

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

/* Ensure that a mechanism ACL is consistent */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN mechanismAclConsistent( IN_ARRAY( mechanismAclSize ) \
										const MECHANISM_ACL *mechanismACLPtr,
									   IN_RANGE( 1, 16 ) \
										const int mechanismAclSize )
	{
	int i;

	assert( isReadPtr( mechanismACLPtr, \
					   sizeof( MECHANISM_ACL ) * mechanismAclSize ) );

	REQUIRES_B( mechanismAclSize > 0 && mechanismAclSize <= 16 );

	for( i = 0; mechanismACLPtr[ i ].type != MECHANISM_NONE && \
				i < mechanismAclSize; i++ )
		{
		const MECHANISM_ACL *mechanismACL = &mechanismACLPtr[ i ];
		const PARAM_ACL *paramACL = getParamACL( mechanismACL );
		const int paramACLSize = getParamACLSize( mechanismACL );
		BOOLEAN paramACLEmpty = FALSE;
		int j;

		/* Make sure that the mechanism Acl entries are consistent */
		if( mechanismACL->type <= MECHANISM_NONE || \
			mechanismACL->type >= MECHANISM_LAST )
			return( FALSE );

		/* Check the paramter ACLs within the mechanism ACL */
		for( j = 0; j < paramACLSize && j < FAILSAFE_ITERATIONS_SMALL; j++ )
			{
			if( !paramAclConsistent( &paramACL[ j ], paramACLEmpty ) )
				return( FALSE );
			if( paramACL[ j ].valueType == PARAM_VALUE_NONE )
				paramACLEmpty = TRUE;
			}
		ENSURES_B( j < FAILSAFE_ITERATIONS_SMALL );
		}
	ENSURES_B( i < mechanismAclSize );

	return( TRUE );
	}

/* Initialise and check the mechanism ACLs */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initMechanismACL( INOUT KERNEL_DATA *krnlDataPtr )
	{
	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* If we're running a fuzzing build, skip the lengthy self-checks */
#ifdef CONFIG_FUZZ
	krnlData = krnlDataPtr;
	return( CRYPT_OK );
#endif /* CONFIG_FUZZ */

	/* Perform a consistency check on the various message ACLs */
	if( !mechanismAclConsistent( mechanismWrapACL, 
				FAILSAFE_ARRAYSIZE( mechanismWrapACL, MECHANISM_ACL ) ) )
		return( FALSE );
	if( !mechanismAclConsistent( mechanismUnwrapACL, 
				FAILSAFE_ARRAYSIZE( mechanismUnwrapACL, MECHANISM_ACL ) ) )
		return( FALSE );
	if( !mechanismAclConsistent( mechanismSignACL, 
				FAILSAFE_ARRAYSIZE( mechanismSignACL, MECHANISM_ACL ) ) )
		return( FALSE );
	if( !mechanismAclConsistent( mechanismSigCheckACL, 
				FAILSAFE_ARRAYSIZE( mechanismSigCheckACL, MECHANISM_ACL ) ) )
		return( FALSE );
	if( !mechanismAclConsistent( mechanismDeriveACL, 
				FAILSAFE_ARRAYSIZE( mechanismDeriveACL, MECHANISM_ACL ) ) )
		return( FALSE );
	if( !mechanismAclConsistent( mechanismKDFACL, 
				FAILSAFE_ARRAYSIZE( mechanismKDFACL, MECHANISM_ACL ) ) )
		return( FALSE );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	return( CRYPT_OK );
	}

void endMechanismACL( void )
	{
	krnlData = NULL;
	}

/****************************************************************************
*																			*
*							Mechanism ACL Check Functions					*
*																			*
****************************************************************************/

/* Functions to implement the checks in the mechanism ACL tables */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismWrapAccess( IN_HANDLE const int objectHandle,
										 IN_MESSAGE const MESSAGE_TYPE message,
										 IN_BUFFER_C( sizeof( MECHANISM_WRAP_INFO ) ) \
											TYPECAST( MECHANISM_WRAP_INFO * ) \
											const void *messageDataPtr,
										 IN_ENUM( MECHANISM ) const int messageValue,
										 STDC_UNUSED const void *dummy )
	{
	const MECHANISM_WRAP_INFO *mechanismInfo = \
				( MECHANISM_WRAP_INFO * ) messageDataPtr;
	const MECHANISM_ACL *mechanismACL = \
				( ( message & MESSAGE_MASK ) == MESSAGE_DEV_EXPORT ) ? \
				mechanismWrapACL : mechanismUnwrapACL;
	const OBJECT_INFO *objectTable = krnlData->objectTable;
	const int mechanismAclSize = \
				( ( message & MESSAGE_MASK ) == MESSAGE_DEV_EXPORT ) ? \
				FAILSAFE_ARRAYSIZE( mechanismWrapACL, MECHANISM_ACL ) : \
				FAILSAFE_ARRAYSIZE( mechanismUnwrapACL, MECHANISM_ACL );
	BOOLEAN isRawMechanism;
	int contextHandle, i, status;

	assert( isReadPtr( messageDataPtr, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Precondition */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( message == MESSAGE_DEV_EXPORT || \
			  message == IMESSAGE_DEV_EXPORT || \
			  message == MESSAGE_DEV_IMPORT || \
			  message == IMESSAGE_DEV_IMPORT );
	REQUIRES( messageValue == MECHANISM_ENC_PKCS1 || \
			  messageValue == MECHANISM_ENC_PKCS1_PGP || \
			  messageValue == MECHANISM_ENC_PKCS1_RAW || \
			  messageValue == MECHANISM_ENC_OAEP || \
			  messageValue == MECHANISM_ENC_CMS || \
			  messageValue == MECHANISM_PRIVATEKEYWRAP || \
			  messageValue == MECHANISM_PRIVATEKEYWRAP_PKCS8 || \
			  messageValue == MECHANISM_PRIVATEKEYWRAP_PGP2 || \
			  messageValue == MECHANISM_PRIVATEKEYWRAP_OPENPGP_OLD || \
			  messageValue == MECHANISM_PRIVATEKEYWRAP_OPENPGP );

	/* Find the appropriate ACL for this mechanism */
	for( i = 0; i < mechanismAclSize && \
				mechanismACL[ i ].type != messageValue && \
				mechanismACL[ i ].type != MECHANISM_NONE; i++ );
	ENSURES( i < mechanismAclSize );
	ENSURES( mechanismACL[ i ].type != MECHANISM_NONE );
	mechanismACL = &mechanismACL[ i ];
	isRawMechanism = \
		( paramInfo( mechanismACL, 2 ).valueType == PARAM_VALUE_NUMERIC && \
		  paramInfo( mechanismACL, 2 ).lowRange == CRYPT_UNUSED ) ? \
		TRUE : FALSE;

	/* Inner precondition: We have an ACL for this mechanism, and the non-
	   user-supplied parameters (the ones supplied by cryptlib that must
	   be OK) are in order */
	REQUIRES( mechanismACL->type != MECHANISM_NONE );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 0 ),
								mechanismInfo->wrappedData,
								mechanismInfo->wrappedDataLength ) );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 1 ),
							    mechanismInfo->keyData,
							    mechanismInfo->keyDataLength ) );
	REQUIRES( checkParamObject( paramInfo( mechanismACL, 4 ),
							    mechanismInfo->auxContext ) );

	/* Make sure that the user-supplied parameters are in order, part 1: The
	   session key is a valid object of the correct type, and there's a key
	   loaded/not loaded as appropriate */
	if( !isRawMechanism )
		{
		if( !fullObjectCheck( mechanismInfo->keyContext, message ) )
			return( CRYPT_ARGERROR_NUM1 );
		if( paramInfo( mechanismACL, 2 ).flags & ACL_FLAG_ROUTE_TO_CTX )
			{
			/* The key being wrapped may be accessed via an object such as a
			   certificate that isn't the required object type, in order to
			   perform the following check on it we have to first find the
			   ultimate target object */
			status = findTargetType( mechanismInfo->keyContext,	
									 &contextHandle, OBJECT_TYPE_CONTEXT );
			if( cryptStatusError( status ) )
				return( CRYPT_ARGERROR_NUM1 );
			}
		else
			contextHandle = mechanismInfo->keyContext;
		if( !checkParamObject( paramInfo( mechanismACL, 2 ), contextHandle ) )
			return( CRYPT_ARGERROR_NUM1 );
		}
	else
		{
		/* For raw wrap/unwrap mechanisms the data is supplied as string
		   data.  In theory this would be somewhat risky since it allows
		   bypassing of object ownership checks, however these mechanisms
		   are only accessed from deep within cryptlib (e.g. by the SSH and
		   SSL/TLS session code, which needs to handle protocol-specific
		   secret data in special ways) so there's no chance for problems
		   since the contexts it ends up in are cryptlib-internal,
		   automatically-created ones belonging to the owner of the session
		   object */
		REQUIRES( checkParamObject( paramInfo( mechanismACL, 2 ),
									mechanismInfo->keyContext ) );
		}

	/* Make sure that the user-supplied parameters are in order, part 2: The
	   wrapping key is a valid object of the correct type with a key loaded */
	if( !fullObjectCheck( mechanismInfo->wrapContext, message ) )
		return( CRYPT_ARGERROR_NUM2 );
	if( paramInfo( mechanismACL, 3 ).flags & ACL_FLAG_ROUTE_TO_CTX )
		{
		/* The wrapping key may be accessed via an object such as a
		   certificate that isn't the required object type, in order to
		   perform the following check on it we have to first find the
		   ultimate target object */
		status = findTargetType( mechanismInfo->wrapContext,
								 &contextHandle, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_NUM2 );
		}
	else
		contextHandle = mechanismInfo->wrapContext;
	if( !checkParamObject( paramInfo( mechanismACL, 3 ), contextHandle ) )
		return( CRYPT_ARGERROR_NUM2 );

	/* Make sure that the user-supplied parameters are in order, part 3: Any
	   auxiliary info needed for the wrapping/unwrapping is OK.  Reporting 
	   the specific problem with these checks is a bit tricky because they
	   apply to parameters coming from deep within cryptlib-internal 
	   functions that will never been seen by the user, so it doesn't really
	   make sense to report a parameter error for a parameter that the user
	   doesn't know exists.  The best that we can do is return a bad-data 
	   error (sol lucet omnibus), since the auxInfo value has been read from 
	   externally-supplied encoded data */
	if( !checkParamNumeric( paramInfo( mechanismACL, 5 ),
							mechanismInfo->auxInfo ) )
		return( CRYPT_ERROR_BADDATA );

	/* Postcondition: The wrapping key and session key are of the appropriate
	   type, there are keys loaded/not loaded as appropriate, and the access
	   is valid.  We don't explicitly state this since it's just
	   regurgitating the checks already performed above */

	/* Make sure that all of the objects have the same owner */
	if( isRawMechanism )
		{
		if( !isSameOwningObject( objectHandle, mechanismInfo->wrapContext ) )
			return( CRYPT_ARGERROR_NUM2 );
		}
	else
		{
		if( !isSameOwningObject( objectHandle, mechanismInfo->keyContext ) )
			return( CRYPT_ARGERROR_NUM1 );
		if( !isSameOwningObject( mechanismInfo->keyContext,
								 mechanismInfo->wrapContext ) )
			return( CRYPT_ARGERROR_NUM2 );
		}

	/* Postcondition: All the objects have the same owner */
#ifndef __WINCE__	/* String too long for compiler */
	ENSURES( ( isRawMechanism && \
			   isSameOwningObject( objectHandle, mechanismInfo->wrapContext ) ) || \
			 ( !isRawMechanism && \
			   isSameOwningObject( objectHandle, mechanismInfo->keyContext ) && \
			   isSameOwningObject( mechanismInfo->keyContext, \
								   mechanismInfo->wrapContext ) ) );
#endif /* !__WINCE__ */

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismSignAccess( IN_HANDLE const int objectHandle,
										 IN_MESSAGE const MESSAGE_TYPE message,
										 IN_BUFFER_C( sizeof( MECHANISM_SIGN_INFO ) ) \
											TYPECAST( MECHANISM_SIGN_INFO * ) \
											const void *messageDataPtr,
										 IN_ENUM( MECHANISM ) const int messageValue,
										 STDC_UNUSED const void *dummy )
	{
	const MECHANISM_SIGN_INFO *mechanismInfo = \
				( MECHANISM_SIGN_INFO * ) messageDataPtr;
	const MECHANISM_ACL *mechanismACL = \
				( ( message & MESSAGE_MASK ) == MESSAGE_DEV_SIGN ) ? \
				mechanismSignACL : mechanismSigCheckACL;
	const OBJECT_INFO *objectTable = krnlData->objectTable;
	const int mechanismAclSize = \
				( ( message & MESSAGE_MASK ) == MESSAGE_DEV_SIGN ) ? \
				FAILSAFE_ARRAYSIZE( mechanismSignACL, MECHANISM_ACL ) : \
				FAILSAFE_ARRAYSIZE( mechanismSigCheckACL, MECHANISM_ACL );
	int contextHandle, i, status;

	assert( isReadPtr( messageDataPtr, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Precondition */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( message == MESSAGE_DEV_SIGN || \
			  message == IMESSAGE_DEV_SIGN || \
			  message == MESSAGE_DEV_SIGCHECK || \
			  message == IMESSAGE_DEV_SIGCHECK );
	REQUIRES( messageValue == MECHANISM_SIG_PKCS1 || \
			  messageValue == MECHANISM_SIG_SSL );

	/* Find the appropriate ACL for this mechanism */
	for( i = 0; i < mechanismAclSize && \
				mechanismACL[ i ].type != messageValue && \
				mechanismACL[ i ].type != MECHANISM_NONE; i++ );
	ENSURES( i < mechanismAclSize );
	ENSURES( mechanismACL[ i ].type != MECHANISM_NONE );
	mechanismACL = &mechanismACL[ i ];

	/* Inner precondition: We have an ACL for this mechanism, and the non-
	   user-supplied parameters (the ones supplied by cryptlib that must
	   be OK) are in order */
	REQUIRES( mechanismACL->type != MECHANISM_NONE );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 0 ),
								mechanismInfo->signature,
								mechanismInfo->signatureLength ) );

	/* Make sure that the user-supplied parameters are in order, part 1: The
	   hash contexts are valid objects of the correct type.  If there's a
	   secondary hash context present we report problems with it as a problem
	   with the (logical) single hash context */
	if( !fullObjectCheck( mechanismInfo->hashContext, message ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( !checkParamObject( paramInfo( mechanismACL, 1 ),
						   mechanismInfo->hashContext ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( !( paramInfo( mechanismACL, 2 ).valueType == PARAM_VALUE_NUMERIC && \
		   paramInfo( mechanismACL, 2 ).lowRange == CRYPT_UNUSED ) && \
		!fullObjectCheck( mechanismInfo->hashContext2, message ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( !checkParamObject( paramInfo( mechanismACL, 2 ),
						   mechanismInfo->hashContext2 ) )
		return( CRYPT_ARGERROR_NUM1 );

	/* Make sure that the user-supplied parameters are in order, part 2: The
	   sig/sig check context is a valid object of the correct type, and
	   there's a key loaded */
	if( !fullObjectCheck( mechanismInfo->signContext, message ) )
		return( CRYPT_ARGERROR_NUM2 );
	if( paramInfo( mechanismACL, 3 ).flags & ACL_FLAG_ROUTE_TO_CTX )
		{
		/* The sig.check key may be accessed via an object such as a
		   certificate that isn't the required object type, in order to
		   perform the following check on it we have to first find the
		   ultimate target object */
		status = findTargetType( mechanismInfo->signContext,
								 &contextHandle, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_NUM2 );
		}
	else
		contextHandle = mechanismInfo->signContext;
	if( !checkParamObject( paramInfo( mechanismACL, 3 ), contextHandle ) )
		return( CRYPT_ARGERROR_NUM2 );

	/* Postcondition: The hash and sig/sig check contexts are of the
	   appropriate type, there's a key loaded in the sig/sig check context,
	   and the access is valid.  We don't explicitly state this since it's
	   just regurgitating the checks already performed above */

	/* Make sure that all of the objects have the same owner */
	if( !isSameOwningObject( objectHandle, mechanismInfo->hashContext ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( !isSameOwningObject( mechanismInfo->hashContext, \
							 mechanismInfo->signContext ) )
		return( CRYPT_ARGERROR_NUM2 );
	if( !( paramInfo( mechanismACL, 2 ).valueType == PARAM_VALUE_NUMERIC && \
		   paramInfo( mechanismACL, 2 ).lowRange == CRYPT_UNUSED ) )
		{
		if( !isSameOwningObject( objectHandle, mechanismInfo->hashContext2 ) )
			return( CRYPT_ARGERROR_NUM1 );
		if( !isSameOwningObject( mechanismInfo->hashContext, \
								 mechanismInfo->signContext ) )
			return( CRYPT_ARGERROR_NUM2 );
		}

	/* Postcondition: All of the objects have the same owner */
	ENSURES( isSameOwningObject( objectHandle, mechanismInfo->hashContext ) && \
			 isSameOwningObject( mechanismInfo->hashContext, \
								 mechanismInfo->signContext ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismDeriveAccess( IN_HANDLE const int objectHandle,
										   IN_MESSAGE const MESSAGE_TYPE message,
										   IN_BUFFER_C( sizeof( MECHANISM_DERIVE_INFO ) ) \
												TYPECAST( MECHANISM_DERIVE_INFO * ) \
												const void *messageDataPtr,
										   IN_ENUM( MECHANISM ) const int messageValue,
										   STDC_UNUSED const void *dummy )
	{
	const MECHANISM_DERIVE_INFO *mechanismInfo = \
				( MECHANISM_DERIVE_INFO * ) messageDataPtr;
	const MECHANISM_ACL *mechanismACL = mechanismDeriveACL;
	int i;

	assert( isReadPtr( messageDataPtr, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Precondition */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( message == MESSAGE_DEV_DERIVE || \
			  message == IMESSAGE_DEV_DERIVE );
	REQUIRES( messageValue == MECHANISM_DERIVE_PKCS5 || \
			  messageValue == MECHANISM_DERIVE_PKCS12 || \
			  messageValue == MECHANISM_DERIVE_SSL || \
			  messageValue == MECHANISM_DERIVE_TLS || \
			  messageValue == MECHANISM_DERIVE_TLS12 || \
			  messageValue == MECHANISM_DERIVE_CMP || \
			  messageValue == MECHANISM_DERIVE_PGP );

	/* Find the appropriate ACL for this mechanism */
	for( i = 0; mechanismACL[ i ].type != messageValue && \
				mechanismACL[ i ].type != MECHANISM_NONE && \
				i < FAILSAFE_ARRAYSIZE( mechanismDeriveACL, MECHANISM_ACL ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( mechanismDeriveACL, MECHANISM_ACL ) );
	ENSURES( mechanismACL[ i ].type != MECHANISM_NONE );
	mechanismACL = &mechanismACL[ i ];

	/* Inner precondition: We have an ACL for this mechanism, and the non-
	   user-supplied parameters (the ones supplied by cryptlib that must
	   be OK) are in order */
	REQUIRES( mechanismACL->type != MECHANISM_NONE );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 0 ),
							    mechanismInfo->dataOut,
							    mechanismInfo->dataOutLength ) );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 1 ),
							    mechanismInfo->dataIn,
							    mechanismInfo->dataInLength ) );
	REQUIRES( checkParamNumeric( paramInfo( mechanismACL, 2 ),
								 mechanismInfo->hashAlgo ) );
	REQUIRES( checkParamNumeric( paramInfo( mechanismACL, 3 ),
								 mechanismInfo->hashParam ) );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 4 ),
							    mechanismInfo->salt,
							    mechanismInfo->saltLength ) );
	REQUIRES( checkParamNumeric( paramInfo( mechanismACL, 5 ),
								 mechanismInfo->iterations ) );

	/* This is a pure data-transformation mechanism, there are no objects
	   used so there are no further checks to perform */

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckMechanismKDFAccess( IN_HANDLE const int objectHandle,
										IN_MESSAGE const MESSAGE_TYPE message,
										IN_BUFFER_C( sizeof( MECHANISM_KDF_INFO ) ) \
											TYPECAST( MECHANISM_KDF_INFO * ) \
											const void *messageDataPtr,
										IN_ENUM( MECHANISM ) const int messageValue,
										STDC_UNUSED const void *dummy )
	{
	const MECHANISM_KDF_INFO *mechanismInfo = \
				( MECHANISM_KDF_INFO * ) messageDataPtr;
	const MECHANISM_ACL *mechanismACL = mechanismKDFACL;
	const OBJECT_INFO *objectTable = krnlData->objectTable;
	int i;

	assert( isReadPtr( messageDataPtr, sizeof( MECHANISM_WRAP_INFO ) ) );

	/* Precondition */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( message == MESSAGE_DEV_KDF || message == IMESSAGE_DEV_KDF );
	REQUIRES( messageValue == MECHANISM_DERIVE_PKCS5  );

	/* Find the appropriate ACL for this mechanism */
	for( i = 0; mechanismACL[ i ].type != messageValue && \
				mechanismACL[ i ].type != MECHANISM_NONE && \
				i < FAILSAFE_ARRAYSIZE( mechanismKDFACL, MECHANISM_ACL ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( mechanismKDFACL, MECHANISM_ACL ) );
	ENSURES( mechanismACL[ i ].type != MECHANISM_NONE );
	mechanismACL = &mechanismACL[ i ];

	/* Inner precondition: We have an ACL for this mechanism, and the non-
	   user-supplied parameters (the ones supplied by cryptlib that must
	   be OK) are in order */
	REQUIRES( mechanismACL->type != MECHANISM_NONE );
	REQUIRES( fullObjectCheck( mechanismInfo->keyContext, message ) );
	REQUIRES( checkParamObject( paramInfo( mechanismACL, 0 ),
								mechanismInfo->keyContext ) );
	REQUIRES( fullObjectCheck( mechanismInfo->masterKeyContext, message ) );
	REQUIRES( checkParamObject( paramInfo( mechanismACL, 1 ),
								mechanismInfo->masterKeyContext ) );
	REQUIRES( checkParamNumeric( paramInfo( mechanismACL, 2 ),
								 mechanismInfo->hashAlgo ) );
	REQUIRES( checkParamNumeric( paramInfo( mechanismACL, 3 ),
								 mechanismInfo->hashParam ) );
	REQUIRES( checkParamString( paramInfo( mechanismACL, 4 ),
								mechanismInfo->salt,
								mechanismInfo->saltLength ) );

	/* This is a pure data-transformation mechanism, there are no objects
	   used so there are no further checks to perform */

	return( CRYPT_OK );
	}
