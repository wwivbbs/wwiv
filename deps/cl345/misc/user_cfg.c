/****************************************************************************
*																			*
*						 cryptlib Configuration Routines					*
*						Copyright Peter Gutmann 1994-2017					*
*																			*
****************************************************************************/

#include "crypt.h"
#ifdef INC_ALL
  #include "user_int.h"
  #include "user.h"
#else
  #include "misc/user_int.h"
  #include "misc/user.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*							Configuration Options							*
*																			*
****************************************************************************/

/* Built-in default configuration options */

#define MK_OPTION( option, value, index ) \
	{ option, OPTION_NUMERIC, index, NULL, value }
#define MK_OPTION_B( option, value, index ) \
	{ option, OPTION_BOOLEAN, index, NULL, value }
#define MK_OPTION_S( option, value, length, index ) \
	{ option, OPTION_STRING, index, value, length }
#define MK_OPTION_NONE() \
	{ CRYPT_ATTRIBUTE_NONE, OPTION_NONE, CRYPT_UNUSED, NULL, 0 }

static const BUILTIN_OPTION_INFO builtinOptionInfo[] = {
	/* cryptlib information (read-only) */
#ifdef NDEBUG
	MK_OPTION_S( CRYPT_OPTION_INFO_DESCRIPTION, "cryptlib security toolkit", 25, CRYPT_UNUSED ),
#else
	MK_OPTION_S( CRYPT_OPTION_INFO_DESCRIPTION, "cryptlib security toolkit (debug build)", 39, CRYPT_UNUSED ),
#endif /* NDEBUG */
	MK_OPTION_S( CRYPT_OPTION_INFO_COPYRIGHT, "Copyright Peter Gutmann, Eric Young, OpenSSL, 1994-2019", 55, CRYPT_UNUSED ),
	MK_OPTION( CRYPT_OPTION_INFO_MAJORVERSION, 3, CRYPT_UNUSED ),
	MK_OPTION( CRYPT_OPTION_INFO_MINORVERSION, 4, CRYPT_UNUSED ),
	MK_OPTION( CRYPT_OPTION_INFO_STEPPING, 5, CRYPT_UNUSED ),

	/* Context options, base = 0 */
	/* Algorithm = Conventional encryption/hash/MAC options */
	MK_OPTION( CRYPT_OPTION_ENCR_ALGO, CRYPT_ALGO_AES, 0 ),
	MK_OPTION( CRYPT_OPTION_ENCR_HASH, CRYPT_ALGO_SHA2, 1 ),
	MK_OPTION( CRYPT_OPTION_ENCR_MAC, CRYPT_ALGO_HMAC_SHA2, 2 ),

	/* Algorithm = PKC options */
	MK_OPTION( CRYPT_OPTION_PKC_ALGO, CRYPT_ALGO_RSA, 3 ),
	MK_OPTION( CRYPT_OPTION_PKC_KEYSIZE, bitsToBytes( 1536 ), 4 ),

	/* Placeholder for obsolete options */
	MK_OPTION( CRYPT_OPTION_DUMMY1, 0, 5 ),
	MK_OPTION( CRYPT_OPTION_DUMMY2, 0, 6 ),

	/* Algorithm = Key derivation options.  On a slower CPU we use a 
	   lower number of iterations.  Conversely, on a fast CPU we use
	   a larger number */
	MK_OPTION( CRYPT_OPTION_KEYING_ALGO, CRYPT_ALGO_HMAC_SHA2, 7 ),
#if defined( CONFIG_SLOW_CPU )
	MK_OPTION( CRYPT_OPTION_KEYING_ITERATIONS, 1000, 8 ),
#elif defined( CONFIG_FAST_CPU )
	MK_OPTION( CRYPT_OPTION_KEYING_ITERATIONS, min( 50000, MAX_KEYSETUP_ITERATIONS ), 8 ),
#else
	MK_OPTION( CRYPT_OPTION_KEYING_ITERATIONS, 10000, 8 ),
#endif /* Options based on CPU speed */

	/* Certificate options, base = 100 */
	MK_OPTION_B( CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES, FALSE, 100 ),
	MK_OPTION( CRYPT_OPTION_CERT_VALIDITY, 365, 101 ),
	MK_OPTION( CRYPT_OPTION_CERT_UPDATEINTERVAL, 90, 102 ),
	MK_OPTION( CRYPT_OPTION_CERT_COMPLIANCELEVEL, CRYPT_COMPLIANCELEVEL_STANDARD, 103 ),
	MK_OPTION_B( CRYPT_OPTION_CERT_REQUIREPOLICY, TRUE, 104 ),

	/* CMS options */
	MK_OPTION_B( CRYPT_OPTION_CMS_DEFAULTATTRIBUTES, TRUE, 105 ),

	/* Keyset options, base = 200 */
	/* Keyset = LDAP options */
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS, "inetOrgPerson", 13, 200 ),
	MK_OPTION( CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE, CRYPT_CERTTYPE_NONE, 201 ),
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_FILTER, "(objectclass=*)", 15, 202 ),
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_CACERTNAME, "cACertificate;binary", 20, 203 ),
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_CERTNAME, "userCertificate;binary", 32, 204 ),
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_CRLNAME, "certificateRevocationList;binary", 22, 205 ),
	MK_OPTION_S( CRYPT_OPTION_KEYS_LDAP_EMAILNAME, "mail", 4, 206 ),

	/* Device options, base = 300 */
	/* Device = PKCS #11 token options */
	MK_OPTION_S( CRYPT_OPTION_DEVICE_PKCS11_DVR01, NULL, 0, 300 ),
	MK_OPTION_S( CRYPT_OPTION_DEVICE_PKCS11_DVR02, NULL, 0, 301 ),
	MK_OPTION_S( CRYPT_OPTION_DEVICE_PKCS11_DVR03, NULL, 0, 302 ),
	MK_OPTION_S( CRYPT_OPTION_DEVICE_PKCS11_DVR04, NULL, 0, 303 ),
	MK_OPTION_S( CRYPT_OPTION_DEVICE_PKCS11_DVR05, NULL, 0, 304 ),
	MK_OPTION_B( CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY, FALSE, 305 ),

	/* Session options, base = 400 */

	/* Miscellaneous options, base = 500.  The network options are mostly 
	   used by sessions but also apply to other object types like network 
	   keysets so they're classed as miscellaneous options. 
	   
	   Side-channel protection is usually disabled by default because a
	   survey of users found that a total of 0% indicated that they wanted
	   side-channel protection in exchange for a performance drop 
	   (particularly in embedded systems where PKC ops already consume about 
	   150% of the available CPU budget), however on fast systems we enable 
	   it by default because they should be fast enough that no-one will 
	   notice */
	MK_OPTION_S( CRYPT_OPTION_NET_SOCKS_SERVER, NULL, 0, 500 ),
	MK_OPTION_S( CRYPT_OPTION_NET_SOCKS_USERNAME, NULL, 0, 501 ),
	MK_OPTION_S( CRYPT_OPTION_NET_HTTP_PROXY, NULL, 0, 502 ),
	MK_OPTION( CRYPT_OPTION_NET_CONNECTTIMEOUT, NET_TIMEOUT_CONNECT, 503 ),
	MK_OPTION( CRYPT_OPTION_NET_READTIMEOUT, NET_TIMEOUT_READ, 504 ),
	MK_OPTION( CRYPT_OPTION_NET_WRITETIMEOUT, NET_TIMEOUT_WRITE, 505 ),
	MK_OPTION_B( CRYPT_OPTION_MISC_ASYNCINIT, TRUE, 506 ),
#if defined( CONFIG_FAST_CPU )
	MK_OPTION( CRYPT_OPTION_MISC_SIDECHANNELPROTECTION, 1, 507 ),
#else
	MK_OPTION( CRYPT_OPTION_MISC_SIDECHANNELPROTECTION, 0, 507 ),
#endif /* Options based on CPU speed */

	/* All options beyond this point are ephemeral and aren't stored to disk. 
	   Remember to update the LAST_STORED_OPTION define in user_int.h when 
	   adding new options here */

	/* cryptlib state information.  These are special-case options that
	   record state information rather than a static configuration value.  
	   The configuration-option-changed status value is updated dynamically, 
	   being set to TRUE if any configuration option is changed.  Writing it 
	   to FALSE commits the changes to disk.  The self-test status value is 
	   initially set to FALSE, writing it to TRUE triggers a self-test for 
	   which the value remains at TRUE if the test succeeds */
	MK_OPTION_B( CRYPT_OPTION_CONFIGCHANGED, FALSE, CRYPT_UNUSED ),
	MK_OPTION( CRYPT_OPTION_SELFTESTOK, FALSE, CRYPT_UNUSED ),

	/* End-of-list marker */
	MK_OPTION_NONE(), MK_OPTION_NONE()
	};

/****************************************************************************
*																			*
*							Utility Functions								*
*																			*
****************************************************************************/

/* Locate an entry in the built-in configuration options by looking up the 
   option code that identifies it when it's written to disk */

CHECK_RETVAL_PTR \
const BUILTIN_OPTION_INFO *getBuiltinOptionInfoByCode( IN_RANGE( 0, LAST_OPTION_INDEX ) \
														const int optionCode )
	{
	int i, LOOP_ITERATOR;

	REQUIRES_N( optionCode >= 0 && optionCode <= LAST_OPTION_INDEX );

	LOOP_LARGE( i = 0, 
				builtinOptionInfo[ i ].option <= LAST_STORED_OPTION && \
				i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, \
										BUILTIN_OPTION_INFO ), 
				i++ )
		{
		if( builtinOptionInfo[ i ].index == optionCode )
			return( &builtinOptionInfo[ i ] );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, BUILTIN_OPTION_INFO ) );

	/* Failing to find a match isn't an internal error since we're looking 
	   up the options based on configuration data stored in an external 
	   file */
	return( NULL );
	}

/* Locate an entry in the current configuration options */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static const OPTION_INFO *getOptionInfo( IN_ARRAY( configOptionsCount ) \
											const OPTION_INFO *optionList,
										 IN_INT_SHORT const int configOptionsCount, 
										 IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE option )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( optionList, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES_N( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES_N( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );

	LOOP_LARGE( i = 0, 
				i < configOptionsCount && \
					optionList[ i ].builtinOptionInfo != NULL && \
					optionList[ i ].builtinOptionInfo->option != CRYPT_ATTRIBUTE_NONE, 
				i++ )
		{
		if( optionList[ i ].builtinOptionInfo->option == option )
			return( &optionList[ i ] );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < configOptionsCount );

	retIntError_Null();
	}

/* Record the fact that one of the configuration options has been changed */

static void setConfigChanged( INOUT_ARRAY( configOptionsCount ) \
									OPTION_INFO *optionList,
							  IN_INT_SHORT const int configOptionsCount )
	{
	OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( optionList, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES_V( isShortIntegerRangeNZ( configOptionsCount ) );

	optionInfoPtr = ( OPTION_INFO * ) \
					getOptionInfo( optionList, configOptionsCount,
								   CRYPT_OPTION_CONFIGCHANGED );
	ENSURES_V( optionInfoPtr != NULL );
	optionInfoPtr->intValue = TRUE;
	}

/* Check whether a configuration option has been changed */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkConfigChanged( IN_ARRAY( configOptionsCount ) \
								const OPTION_INFO *optionList,
							IN_INT_SHORT const int configOptionsCount )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( optionList, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES_B( isShortIntegerRangeNZ( configOptionsCount ) );

	LOOP_LARGE( i = 0, 
				i < configOptionsCount && \
					optionList[ i ].builtinOptionInfo != NULL && \
					optionList[ i ].builtinOptionInfo->option <= LAST_STORED_OPTION, 
				i++ )
		{
		if( optionList[ i ].dirty )
			return( TRUE );
		}
	ENSURES_B( LOOP_BOUND_OK );
	ENSURES_B( i < configOptionsCount );

	return( FALSE );
	}

/****************************************************************************
*																			*
*							Get Configuration Options						*
*																			*
****************************************************************************/

/* Query the value of a numeric or string option */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int getOption( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					const void *configOptions, 
			   IN_INT_SHORT const int configOptionsCount, 
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
			   OUT_INT_Z int *value )
	{
	const OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( isWritePtr( value, sizeof( int ) ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );

	/* Clear return value */
	*value = 0;

	optionInfoPtr = getOptionInfo( configOptions, configOptionsCount, 
								   option );
	ENSURES( optionInfoPtr != NULL && \
			 ( optionInfoPtr->builtinOptionInfo->type == OPTION_NUMERIC || \
			   optionInfoPtr->builtinOptionInfo->type == OPTION_BOOLEAN ) );
	*value = optionInfoPtr->intValue;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
int getOptionString( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						const void *configOptions,
					 IN_INT_SHORT const int configOptionsCount, 
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
					 OUT_PTR_COND const void **strPtrPtr, 
					 OUT_LENGTH_SHORT_Z int *strLen )
	{
	const OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( isReadPtr( strPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( strLen, sizeof( int ) ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );

	/* Clear return values */
	*strPtrPtr = NULL;
	*strLen = 0;

	optionInfoPtr = getOptionInfo( configOptions, configOptionsCount, 
								   option );
	ENSURES( optionInfoPtr != NULL && \
			 optionInfoPtr->builtinOptionInfo->type == OPTION_STRING );
	if( optionInfoPtr->intValue <= 0 )
		return( CRYPT_ERROR_NOTFOUND );
	*strPtrPtr = optionInfoPtr->strValue;
	*strLen = optionInfoPtr->intValue;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Set Configuration Options						*
*																			*
****************************************************************************/

/* Set the value of a numeric or string option */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setOption( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					void *configOptions, 
			   IN_INT_SHORT const int configOptionsCount, 
			   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
			   IN_INT const int value )
	{
	const BUILTIN_OPTION_INFO *builtinOptionInfoPtr;
	OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );
	REQUIRES( isIntegerRange( value ) );

	/* Get a pointer to the option information and make sure that everything
	   is OK */
	optionInfoPtr = ( OPTION_INFO * ) \
					getOptionInfo( configOptions, configOptionsCount, 
								   option );
	ENSURES( optionInfoPtr != NULL );
	builtinOptionInfoPtr = optionInfoPtr->builtinOptionInfo;
	ENSURES( builtinOptionInfoPtr != NULL && \
			 ( builtinOptionInfoPtr->type == OPTION_NUMERIC || \
			   builtinOptionInfoPtr->type == OPTION_BOOLEAN ) );

	/* If the stored value is the same as the new one, there's nothing to 
	   do */
	if( optionInfoPtr->intValue == value )
		return( CRYPT_OK );

	/* If we're forcing a commit by returning the configuration-changed flag 
	   to its ground state, write any changed options to backing store */
	if( option == CRYPT_OPTION_CONFIGCHANGED )
		{
		/* When a non-configuration option (for example a certificate trust 
		   option) is changed then we need to write the updated 
		   configuration data to backing store, but there's no way to tell 
		   that this is required because the configuration options are 
		   unchanged.  To allow the caller to signal this change they can 
		   explicitly set the configuration-changed setting to TRUE 
		   (normally this is done implicitly by when another configuration 
		   setting is changed).  This explicit setting can only be done by 
		   the higher-level configuration-update code because the kernel 
		   blocks any attempts to set it to a value other than FALSE */
		if( value )
			{
			optionInfoPtr->intValue = TRUE;

			return( CRYPT_OK );
			}

		/* Make sure that there's something to write.  We do this to avoid
		   problems with programs that always try to update the 
		   configuration whether it's necessary or not, which can cause 
		   problems with media with limited writeability */
		if( !optionInfoPtr->intValue )
			return( CRYPT_OK );

		/* We don't do anything to write the configuration data at this 
		   level since we currently have the user object locked and don't 
		   want to stall all operations that depend on it while we're 
		   updating the data, so all that we do is signal the user object 
		   to perform the necessary operations */
		return( OK_SPECIAL );
		}

	/* If we're forcing a self-test by changing the value of the self-test
	   status, perform an algorithm test */
	if( option == CRYPT_OPTION_SELFTESTOK )
		{
		/* The self-test can take some time to complete.  While it's running
		   we don't want to leave the user object locked since this will
		   block most other threads, which all eventually read some sort of
		   configuration option.  To get around this problem we set the 
		   result value to an undefined status and unlock the user object 
		   around the call, then re-lock it and set its actual value via 
		   setOptionSpecial() once the self-test is done */
		if( optionInfoPtr->intValue == CRYPT_ERROR )
			return( CRYPT_ERROR_TIMEOUT );
		optionInfoPtr->intValue = CRYPT_ERROR;

		return( OK_SPECIAL );
		}

	/* Set the value and remember that the configuration options have been 
	   changed */
	if( builtinOptionInfoPtr->type == OPTION_BOOLEAN )
		{
		/* Turn a generic zero/nonzero boolean into TRUE or FALSE */
		optionInfoPtr->intValue = ( value ) ? TRUE : FALSE;
		}
	else
		optionInfoPtr->intValue = value;
	optionInfoPtr->dirty = TRUE;
	setConfigChanged( configOptions, configOptionsCount );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setOptionSpecial( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
							void *configOptions, 
					  IN_INT_SHORT const int configOptionsCount, 
					  IN_RANGE_FIXED( CRYPT_OPTION_SELFTESTOK ) \
							const CRYPT_ATTRIBUTE_TYPE option,
					  IN_INT_Z const int value )
	{
	OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );

	/* The update of the self-test status is performed in two phases.  When 
	   we begin the self-test, triggered by the user setting the 
	   CRYPT_OPTION_SELFTESTOK, it's set to an undefined value via 
	   setOption().  In other words the user can only ever set this to the 
	   self-test-in-progress state.  Once the self-test completes it's set 
	   to the test result value via setOptionSpecial(), which can only be 
	   accessed from inside the user object */
	REQUIRES( option == CRYPT_OPTION_SELFTESTOK );

	/* Get a pointer to the option information and make sure that everything
	   is OK */
	optionInfoPtr = ( OPTION_INFO * ) \
					getOptionInfo( configOptions, configOptionsCount, 
								   option );
	ENSURES( optionInfoPtr != NULL && \
			 optionInfoPtr->intValue == CRYPT_ERROR );

	optionInfoPtr->intValue = value;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int setOptionString( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						void *configOptions, 
					 IN_INT_SHORT const int configOptionsCount, 
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option,
					 IN_BUFFER( valueLength ) const char *value, 
					 IN_LENGTH_SHORT const int valueLength )
	{
	const BUILTIN_OPTION_INFO *builtinOptionInfoPtr;
	OPTION_INFO *optionInfoPtr;
	char *valuePtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );
	assert( isReadPtrDynamic( value, valueLength ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );
	REQUIRES( isShortIntegerRangeNZ( valueLength ) );

	/* Get a pointer to the option information and make sure that everything
	   is OK */
	optionInfoPtr = ( OPTION_INFO * ) \
					getOptionInfo( configOptions, configOptionsCount, 
								   option );
	ENSURES( optionInfoPtr != NULL );
	builtinOptionInfoPtr = optionInfoPtr->builtinOptionInfo;
	ENSURES( builtinOptionInfoPtr != NULL && \
			 builtinOptionInfoPtr->type == OPTION_STRING );

	/* If the value is the same as the current one, there's nothing to do */
	if( optionInfoPtr->strValue != NULL && \
		optionInfoPtr->intValue == valueLength && \
		!memcmp( optionInfoPtr->strValue, value, valueLength ) )
		return( CRYPT_OK );

	/* If we're resetting a value to its default setting, just reset the
	   string pointers rather than storing the value */
	if( builtinOptionInfoPtr->strDefault != NULL && \
		builtinOptionInfoPtr->intDefault == valueLength && \
		!memcmp( builtinOptionInfoPtr->strDefault, value, valueLength ) )
		{
		if( optionInfoPtr->strValue != NULL && \
			optionInfoPtr->strValue != builtinOptionInfoPtr->strDefault )
			{
			zeroise( optionInfoPtr->strValue, optionInfoPtr->intValue );
			clFree( "setOptionString", optionInfoPtr->strValue );
			}
		optionInfoPtr->strValue = ( char * ) builtinOptionInfoPtr->strDefault;
		optionInfoPtr->dirty = TRUE;
		setConfigChanged( configOptions, configOptionsCount );
		return( CRYPT_OK );
		}

	/* Try and allocate room for the new option */
	REQUIRES( rangeCheck( valueLength, 1, MAX_INTLENGTH_SHORT ) );
	if( ( valuePtr = clAlloc( "setOptionString", valueLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memcpy( valuePtr, value, valueLength );

	/* If the string value that's currently set isn't the default setting,
	   clear and free it */
	if( optionInfoPtr->strValue != NULL && \
		optionInfoPtr->strValue != builtinOptionInfoPtr->strDefault )
		{
		zeroise( optionInfoPtr->strValue, optionInfoPtr->intValue );
		clFree( "setOptionString", optionInfoPtr->strValue );
		}

	/* Set the value and remember that the configuration options have been 
	   changed */
	optionInfoPtr->strValue = valuePtr;
	optionInfoPtr->intValue = valueLength;
	optionInfoPtr->dirty = TRUE;
	setConfigChanged( configOptions, configOptionsCount );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Misc.Configuration Options Routines					*
*																			*
****************************************************************************/

/* Delete an option */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteOption( INOUT_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
						void *configOptions, 
				  IN_INT_SHORT const int configOptionsCount, 
				  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE option )
	{
	const BUILTIN_OPTION_INFO *builtinOptionInfoPtr;
	OPTION_INFO *optionInfoPtr;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES( isShortIntegerRangeNZ( configOptionsCount ) );
	REQUIRES( option > CRYPT_OPTION_FIRST && option < CRYPT_OPTION_LAST );

	/* Get a pointer to the option information and make sure that everything
	   is OK */
	optionInfoPtr = ( OPTION_INFO * ) \
					getOptionInfo( configOptions, configOptionsCount, option );
	ENSURES( optionInfoPtr != NULL );
	builtinOptionInfoPtr = optionInfoPtr->builtinOptionInfo;
	ENSURES( builtinOptionInfoPtr != NULL && \
			 builtinOptionInfoPtr->type == OPTION_STRING );

	/* If we're deleting an option it can only be a string option without a
	   built-in default to fall back on (enforced by the kernel).  Since 
	   these options don't have default values we check for a setting of 
	   NULL rather than equivalence to a default string value.  In addition
	   since they contain a mixture of fixed and user-definable fields we
	   have to clear the fields explicitly rather than using a memset() */
	ENSURES( builtinOptionInfoPtr->strDefault == NULL );
	if( optionInfoPtr->strValue == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	zeroise( optionInfoPtr->strValue, optionInfoPtr->intValue );
	clFree( "setOptionString", optionInfoPtr->strValue );
	optionInfoPtr->strValue = NULL;
	optionInfoPtr->intValue = 0;
	optionInfoPtr->dirty = TRUE;
	setConfigChanged( configOptions, configOptionsCount );
	return( CRYPT_OK );
	}

/* Initialise/shut down the configuration option handling */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int initOptions( OUT_PTR_COND void **configOptionsPtr, 
				 OUT_INT_SHORT_Z int *configOptionsCount )
	{
	OPTION_INFO *optionList;
	int i, LOOP_ITERATOR;

	/* Clear return values */
	*configOptionsPtr = NULL;
	*configOptionsCount = 0;

	/* Perform a consistency check on the options */
#ifndef CONFIG_FUZZ
	FORALL( i, 0, CRYPT_OPTION_LAST - CRYPT_OPTION_FIRST - 1,
			builtinOptionInfo[ i ].option == i + CRYPT_OPTION_FIRST + 1 );
#endif /* !CONFIG_FUZZ */

	/* Allocate storage for the variable configuration data */
	optionList = getOptionInfoStorage();
	memset( optionList, 0, OPTION_INFO_SIZE );

	/* Walk through the configuration option list setting up each option to 
	   contain its default value */
	LOOP_LARGE( i = 0,
				builtinOptionInfo[ i ].option != CRYPT_ATTRIBUTE_NONE && \
					i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, \
											BUILTIN_OPTION_INFO ), 
				i++ )
		{
		const BUILTIN_OPTION_INFO *builtinOptionInfoPtr = &builtinOptionInfo[ i ];
		OPTION_INFO *optionInfoPtr = &optionList[ i ];

		if( builtinOptionInfoPtr->type == OPTION_STRING )
			optionInfoPtr->strValue = ( char * ) builtinOptionInfoPtr->strDefault;
		optionInfoPtr->intValue = builtinOptionInfoPtr->intDefault;
		optionInfoPtr->builtinOptionInfo = builtinOptionInfoPtr;
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, BUILTIN_OPTION_INFO ) );
	*configOptionsPtr = optionList;
	*configOptionsCount = FAILSAFE_ARRAYSIZE( builtinOptionInfo, \
											  BUILTIN_OPTION_INFO );

	return( CRYPT_OK );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
void endOptions( IN_ARRAY( configOptionsCount ) TYPECAST( OPTION_INFO * ) \
					void *configOptions, 
				 IN_INT_SHORT const int configOptionsCount )
	{
	OPTION_INFO *optionList = configOptions;
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( configOptions, 
							  sizeof( OPTION_INFO ) * configOptionsCount ) );

	REQUIRES_V( isShortIntegerRangeNZ( configOptionsCount ) );

	/* Walk through the configuration option list clearing and freeing each 
	   option */
	LOOP_LARGE( i = 0, 
				builtinOptionInfo[ i ].option != CRYPT_ATTRIBUTE_NONE && \
					i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, \
											BUILTIN_OPTION_INFO ), 
				i++ )
		{
		const BUILTIN_OPTION_INFO *builtinOptionInfoPtr = &builtinOptionInfo[ i ];
		OPTION_INFO *optionInfoPtr = &optionList[ i ];

		if( builtinOptionInfoPtr->type == OPTION_STRING )
			{
			/* If the string value that's currently set isn't the default
			   setting, clear and free it */
			if( optionInfoPtr->strValue != builtinOptionInfoPtr->strDefault )
				{
				zeroise( optionInfoPtr->strValue, optionInfoPtr->intValue );
				clFree( "endOptions", optionInfoPtr->strValue );
				}
			}
		}
	ENSURES_V( LOOP_BOUND_OK );
	ENSURES_V( i < FAILSAFE_ARRAYSIZE( builtinOptionInfo, BUILTIN_OPTION_INFO ) );
	ENSURES_V( i == configOptionsCount - 1 );

	/* Clear and free the configuration option list */
	memset( optionList, 0, sizeof( OPTION_INFO ) * configOptionsCount );
	}
