/****************************************************************************
*																			*
*					cryptlib Generic-Secret Object Routines					*
*						Copyright Peter Gutmann 1995-2009					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

/* This is a special-case context type that implements no actual 
   functionality but instead serves as a generic-secret container for use 
   with crypto mechanisms that employ intermediate cryptovariables, for 
   example ones that derive encryption keys, MAC keys, and IVs from a master
   secret value */

/****************************************************************************
*																			*
*								Self-test Routines							*
*																			*
****************************************************************************/

#ifndef CONFIG_NO_SELFTEST

CHECK_RETVAL \
static int selfTest( void )
	{
	return( CRYPT_OK );
	}
#else
	#define selfTest	NULL
#endif /* !CONFIG_NO_SELFTEST */

/****************************************************************************
*																			*
*								Key Management Routines						*
*																			*
****************************************************************************/

/* Since this is a pure data-storage object, this is the only routine that
   does anything */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initKey( CONTEXT_INFO *contextInfoPtr, const void *key, 
					const int keyLength )
	{
	GENERIC_INFO *genericInfo = contextInfoPtr->ctxGeneric;

	assert( isWritePtr( contextInfoPtr, sizeof( CONTEXT_INFO ) ) );
	assert( isReadPtrDynamic( key, keyLength ) );

	REQUIRES( sanityCheckContext( contextInfoPtr ) );
	REQUIRES( keyLength >= MIN_KEYSIZE && keyLength <= CRYPT_MAX_KEYSIZE );

	/* Copy the key to internal storage */
	if( genericInfo->genericSecret != key )
		{
		REQUIRES( rangeCheck( keyLength, 1, CRYPT_MAX_KEYSIZE ) );
		memcpy( genericInfo->genericSecret, key, keyLength );
		genericInfo->genericSecretLength = keyLength;
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Capability Access Routines							*
*																			*
****************************************************************************/

static const CAPABILITY_INFO capabilityInfo = {
	CRYPT_IALGO_GENERIC_SECRET, 0, "Generic Secret", 14,
	bitsToBytes( 128 ), bitsToBytes( 128 ), bitsToBytes( 256 ),
	selfTest, getDefaultInfo, NULL, initGenericParams, initKey, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 
	NULL, NULL
	};

CHECK_RETVAL_PTR_NONNULL \
const CAPABILITY_INFO *getGenericSecretCapability( void )
	{
	return( &capabilityInfo );
	}
