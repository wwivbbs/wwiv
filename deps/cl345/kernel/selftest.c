/****************************************************************************
*																			*
*								Kernel Self-Test							*
*						Copyright Peter Gutmann 1997-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "capabil.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "device/capabil.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						Correctness/Safety Self-test Functions				*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Self-test code for the correctness/safety mechanisms.  Note that we have 
   to use a buffer of at least 256 bytes since that's the smallest that 
   safeBufferInit() will allow us to specify */

#define SAFE_BUFFER_SIZE	256

static BOOLEAN testCFITrue( const ACCESS_TOKEN accessToken )
	{
	return( CHECK_TOKEN( "testCFITrue" ) ? TRUE : FALSE );
	}

static BOOLEAN testCFIFalse( const ACCESS_TOKEN accessToken )
	{
	return( CHECK_TOKEN( "testCFIFalse" ) ? FALSE : TRUE );
	}

static BOOLEAN testSafetyMechanisms( void )
	{
	DATAPTR testPtr;
	SAFE_FLAGS flags;
	SAFE_BOOLEAN testBool;
	BYTE buffer[ SAFE_BUFFER_SIZE + 16 + 8 ], *bufPtr, savedValue;
	int i, LOOP_ITERATOR, LOOP_ITERATOR_ALT;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;

	/* Test the error-detecting BOOLEAN variable mechanisms */
	BOOL_SET( &testBool );
	if( !BOOL_ISSET( &testBool ) || BOOL_ISCLEAR( &testBool ) )
		return( FALSE );
	BOOL_CLEAR( &testBool );
	if( BOOL_ISSET( &testBool ) || !BOOL_ISCLEAR( &testBool ) )
		return( FALSE );
	BOOL_SET( &testBool );
	testBool.value1 ^= 0x0800;
	if( BOOL_ISVALID( &testBool ) )
		return( FALSE );
	BOOL_SET( &testBool );
	testBool.value2 ^= 0x0800;
	if( BOOL_ISVALID( &testBool ) )
		return( FALSE );
	BOOL_CLEAR( &testBool );
	testBool.value1 ^= 0x0800;
	if( BOOL_ISVALID( &testBool ) )
		return( FALSE );
	BOOL_CLEAR( &testBool );
	testBool.value2 ^= 0x0800;
	if( BOOL_ISVALID( &testBool ) )
		return( FALSE );
	CFI_CHECK_UPDATE( "BOOLEAN" ); 

	/* Test the error-detecting bitflags variable mechanisms */
	INIT_FLAGS( flags, 0 );
	if( !CHECK_FLAGS( flags, 0x00, 0x80 ) )
		return( FALSE );
	SET_FLAG( flags, 0x80 );
	if( !CHECK_FLAGS( flags, 0x00, 0x80 ) )
		return( FALSE );
	if( !TEST_FLAG( flags, 0x80 ) )
		return( FALSE );
	CLEAR_FLAG( flags, 0x80 );
	if( !CHECK_FLAGS( flags, 0x00, 0x80 ) )
		return( FALSE );
	if( TEST_FLAG( flags, 0x80 ) )
		return( FALSE );
	INIT_FLAGS( flags, 0 );
	SET_FLAG( flags, 0x100 );
	if( CHECK_FLAGS( flags, 0x00, 0x80 ) )
		return( FALSE );
	INIT_FLAGS( flags, 0 );
	flags.flagValue |= 0x01;
	if( CHECK_FLAGS( flags, 0x00, 0x80 ) )
		return( FALSE );
	CFI_CHECK_UPDATE( "bitflags" ); 

	/* Test the error-detecting pointer mechanisms.  We only check data
	   pointers since function pointers are implemented identically */
	DATAPTR_SET( testPtr, &testBool );
	if( DATAPTR_GET( testPtr ) != &testBool )
		return( FALSE );
	testPtr.dataCheck ^= 0x0800;		/* Hidden variable used to check testPtr */
	if( DATAPTR_ISVALID( testPtr ) || DATAPTR_ISSET( testPtr ) || \
		DATAPTR_ISNULL( testPtr ) || DATAPTR_GET( testPtr ) != NULL )
		return( FALSE );
	CFI_CHECK_UPDATE( "pointers" ); 

	/* Test the function-call CFI protection mechanism */
	if( !testCFITrue( MK_TOKEN( "testCFITrue" ) ) || \
		!testCFIFalse( MK_TOKEN( "testCFITrue" ) ) ) 
		return( FALSE );
	CFI_CHECK_UPDATE( "CFI" ); 

	/* Test the bounded-loop mechanisms.  There are many variants of this, 
	   we only check the basic forms */
	LOOP_SMALL( i = 0, i < 5, i++ );
	if( !LOOP_BOUND_OK )
		return( FALSE );
	LOOP_SMALL( i = 0, i < 20, i++ );
	if( LOOP_BOUND_OK )
		return( FALSE );
	LOOP_SMALL_ALT( i = 0, i < 5, i++ );
	if( !LOOP_BOUND_OK_ALT )
		return( FALSE );
	LOOP_SMALL_ALT( i = 0, i < 20, i++ );
	if( LOOP_BOUND_OK_ALT )
		return( FALSE );
/*	LOOP_SMALL_ALT2( i = 0, i < 20, i++ );
	if( LOOP_BOUND_OK_ALT2 )
		return( FALSE ); - No LOOP_SMALL_ALT2 */
	CFI_CHECK_UPDATE( "loops" ); 

	/* Test the buffer overrun protection */
	memset( buffer, 0, SAFE_BUFFER_SIZE + 16 );
	bufPtr = SAFEBUFFER_PTR( buffer );
	safeBufferInit( bufPtr, SAFE_BUFFER_SIZE );
	if( !safeBufferCheck( bufPtr, SAFE_BUFFER_SIZE ) )
		return( FALSE );	/* Basic check */
	savedValue = bufPtr[ -1 ];
	bufPtr[ -1 ]++;
	if( safeBufferCheck( bufPtr, SAFE_BUFFER_SIZE ) )
		return( FALSE );	/* Corrupt buffer start */
	bufPtr[ -1 ] = savedValue;
	if( !safeBufferCheck( bufPtr, SAFE_BUFFER_SIZE ) )
		return( FALSE );	/* Redo basic check */
	bufPtr[ SAFE_BUFFER_SIZE + 2 ]++;
	if( safeBufferCheck( bufPtr, SAFE_BUFFER_SIZE ) )
		return( FALSE );	/* Corrupt buffer end */
	CFI_CHECK_UPDATE( "buffers" ); 

	/* Test the sequence-point CFI mechanism */
	if( !CFI_CHECK_SEQUENCE_6( "BOOLEAN", "bitflags", "pointers", "CFI", 
							   "loops", "buffers" ) )
		return( FALSE );
	if( CFI_CHECK_SEQUENCE_6( "BOOLEAN", "pointers", "bitflags", "CFI", 
							  "loops", "buffers" ) )
		return( FALSE );
	if( CFI_CHECK_SEQUENCE_6( "BOOLEAN", "bitflags", "loops", "buffers", 
							  "pointers", "CFI" ) )
		return( FALSE );

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

/****************************************************************************
*																			*
*							Algorithm Self-test Functions					*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Self-test code for several general crypto algorithms that are used
   internally all over cryptlib: MD5, SHA-1, SHA-2, 3DES, and AES */

CHECK_RETVAL_BOOL \
static BOOLEAN testGeneralAlgorithms( void )
	{
#ifndef CONFIG_NO_SELFTEST
	const CAPABILITY_INFO *capabilityInfo;
	int status;

#ifdef USE_MD5
	/* Test the MD5 functionality */
	capabilityInfo = getMD5Capability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "MD5 self-test failed" ));
		return( FALSE );
		}
#endif /* USE_MD5 */

	/* Test the SHA-1 functionality */
	capabilityInfo = getSHA1Capability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "SHA-1 self-test failed" ));
		return( FALSE );
		}

	/* Test the SHA-2 functionality */
	capabilityInfo = getSHA2Capability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "SHA-2 self-test failed" ));
		return( FALSE );
		}

	/* Test the 3DES functionality */
#ifdef USE_3DES
	capabilityInfo = get3DESCapability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "3DES self-test failed" ));
		return( FALSE );
		}
#endif /* USE_3DES */

	/* Test the AES functionality */
#ifdef USE_AES
	capabilityInfo = getAESCapability();
	status = capabilityInfo->selfTestFunction();
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "AES self-test failed" ));
		return( FALSE );
		}
#endif /* USE_AES */
#endif /* CONFIG_NO_SELFTEST */

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

/****************************************************************************
*																			*
*							Kernel Self-test Functions						*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Test the kernel mechanisms to make sure that everything's working as
   expected.  This performs the following tests:

   General:

	Object creation.

   Access checks:

	Inability to access internal object or attribute via external message.
	Inability to perform an internal-only action externally, ability to
		perform an internal-only action internally

   Attribute checks:

	Attribute range checking for numeric, string, boolean, and time
		attributes.
	Inability to write a read-only attribute, read a write-only attribute,
		or delete a non-deletable attribute.

   Object state checks:

	Ability to perform standard operation on object, ability to transition a
		state = low object to state = high.
	Inability to perform state = high operation on state = low object,
		inability to perform state = low operation on state = high object.

   Object property checks:

	Ability to use an object with a finite usage count, inability to
		increment the count, ability to decrement the count, inability to
		exceed the usage count.
	Ability to lock an object, inability to change security parameters once
		it's locked */

CHECK_RETVAL_BOOL \
static BOOLEAN checkNumericRange( IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptContext ) );

	/* Verify kernel range checking of numeric values */
	value = -10;		/* Below (negative) */
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = 0;			/* Lower bound fencepost error */
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = 1;			/* Lower bound */
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_OK )
		return( FALSE );
	value = MAX_KEYSETUP_ITERATIONS / 2;	/* Mid-range */
	krnlSendMessage( iCryptContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_OK )
		return( FALSE );
	value = MAX_KEYSETUP_ITERATIONS;		/* Upper bound */
	krnlSendMessage( iCryptContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_KEYING_ITERATIONS );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_OK )
		return( FALSE );
	value = MAX_KEYSETUP_ITERATIONS + 1;	/* Upper bound fencepost error */
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = MAX_KEYSETUP_ITERATIONS * 50;	/* High */
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_KEYING_ITERATIONS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkStringRange( IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ CRYPT_MAX_HASHSIZE + 1 + 8 ];

	REQUIRES_B( isHandleRangeValid( iCryptContext ) );

	/* Verify kernel range checking of string values.  We have to disable 
	   the more outrageous out-of-bounds values in the debug version since 
	   they'll cause the debug kernel to throw an exception if it sees 
	   them.  In addition for internal messages this results in an internal-
	   error status rather than a argument-error status since the values are
	   so far out of their allowed range that it's reported as a problem 
	   with the caller rather than with the parameters used */
	memset( buffer, '*', CRYPT_MAX_HASHSIZE + 1 );
#ifdef NDEBUG
	/* Below (negative) */
	setMessageData( &msgData, buffer, -10 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_ERROR_INTERNAL )
		return( FALSE );
#endif /* NDEBUG */
	/* Lower bound fencepost error */
	setMessageData( &msgData, buffer, 7 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	/* Lower bound */
	setMessageData( &msgData, buffer, 8 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_OK )
		return( FALSE );
	/* Mid-range */
	setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE / 2 );
	krnlSendMessage( iCryptContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_KEYING_SALT );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_OK )
		return( FALSE );
	/* Upper bound */
	setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE );
	krnlSendMessage( iCryptContext, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CTXINFO_KEYING_SALT );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_OK )
		return( FALSE );
	/* Upper bound fencepost error */
	setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE + 1 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
#ifdef NDEBUG
	/* High */
	setMessageData( &msgData, buffer, 32767 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEYING_SALT ) != CRYPT_ERROR_INTERNAL )
		return( FALSE );
#endif /* NDEBUG */

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkUsageCount( IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	BYTE buffer[ 16 + 8 ];
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptContext ) );

	/* Verify the ability to use an object with a finite usage count, the
	   inability to increment the count, the ability to decrement the count,
	   and the inability to exceed the usage count */
	value = 10;
	memset( buffer, 0, 16 );
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_PROPERTY_USAGECOUNT ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
						 buffer, 16 ) != CRYPT_OK )
		return( FALSE );
	value = 20;
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_PROPERTY_USAGECOUNT ) != CRYPT_ERROR_PERMISSION )
		return( FALSE );
	value = 1;
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_PROPERTY_USAGECOUNT ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
						 buffer, 16 ) != CRYPT_OK || \
		krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
						 buffer, 16 ) != CRYPT_ERROR_PERMISSION )
		return( FALSE );

	/* Note that at this point the usage count is zero so no further 
	   operations with the object are possible */

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkLocking( IN_HANDLE const CRYPT_CONTEXT iCryptContext )
	{
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptContext ) );

	/* Verify the ability to lock an object and the inability to change
	   security parameters once it's locked */
	value = 5;
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_PROPERTY_FORWARDCOUNT ) != CRYPT_OK || \
		krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
						 MESSAGE_VALUE_TRUE,
						 CRYPT_PROPERTY_HIGHSECURITY ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_PROPERTY_LOCKED ) != CRYPT_OK || \
		value != TRUE || \
		krnlSendMessage( iCryptContext, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_PROPERTY_FORWARDCOUNT ) != CRYPT_ERROR_PERMISSION )
		{
		/* Object should be locked, forwardcount should be inaccessible */
		return( FALSE );
		}
	value = 1;
	if( krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_PROPERTY_FORWARDCOUNT ) != CRYPT_ERROR_PERMISSION )
		{
		/* Security parameters shouldn't be writeable */
		return( FALSE );
		}

	return( TRUE );
	}

#ifdef USE_CERTIFICATES

CHECK_RETVAL_BOOL \
static BOOLEAN checkBooleanRange( IN_HANDLE const CRYPT_CONTEXT iCryptCertificate )
	{
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptCertificate ) );

	/* Verify functioning of the kernel range checking, phase 3: Boolean
	   values.  Any value should be OK, with conversion to TRUE/FALSE */
	value = 0;		/* FALSE */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK || \
		value != FALSE )
		return( FALSE );
	value = 1;		/* TRUE */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK || \
		value != TRUE )
		return( FALSE );
	value = 10000;	/* Positive true-equivalent */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK || \
		value != TRUE )
		return( FALSE );
	value = -1;		/* Negative true-equivalent */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK )
		return( FALSE );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CERTINFO_SELFSIGNED ) != CRYPT_OK || \
		value != TRUE )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkTimeRange( IN_HANDLE const CRYPT_CONTEXT iCryptCertificate )
	{
	MESSAGE_DATA msgData;
	time_t timeVal;

	REQUIRES_B( isHandleRangeValid( iCryptCertificate ) );

	/* Verify kernel range checking of time values.  Any value above the 
	   initial cutoff date should be OK */
	setMessageData( &msgData, &timeVal, sizeof( time_t ) );
	timeVal = 10;					/* Below */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_ARGERROR_STR1 )
		return( FALSE );
	timeVal = MIN_TIME_VALUE;		/* Lower bound fencepost error */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_ARGERROR_STR1 )
		return( FALSE );
	timeVal = MIN_TIME_VALUE + 1;	/* Lower bound */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_OK )
		return( FALSE );
	timeVal = roundUp( timeVal, 0x10000000L );	/* Mid-range */
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_VALIDFROM );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_OK )
		return( FALSE );
	timeVal = MAX_TIME_VALUE - 1;	/* Upper bound */
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_VALIDFROM );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_OK )
		return( FALSE );
	timeVal = MAX_TIME_VALUE;		/* Upper bound fencepost error */
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_VALIDFROM );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_ARGERROR_STR1 )
		return( FALSE );
	timeVal = MAX_TIME_VALUE + ( 365 * 86400 );	/* Above */
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_VALIDFROM );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_VALIDFROM ) != CRYPT_ARGERROR_STR1 )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkAllowedValuesRange( IN_HANDLE const CRYPT_CONTEXT iCryptCertificate )
	{
	MESSAGE_DATA msgData;
	BYTE buffer[ 16 + 8 ];
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptCertificate ) );

	/* Verify kernel range checking of special-case allowed values.  Valid 
	   values are either a 4-byte IPv4 address or a 16-byte IPv6 address */
	value = CRYPT_CERTINFO_SUBJECTALTNAME;
	memset( buffer, 0, 16 );
	setMessageData( &msgData, buffer, 3 );	/* Below, allowed value 1 */
	krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
					 CRYPT_ATTRIBUTE_CURRENT );
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	setMessageData( &msgData, buffer, 4 );	/* Equal, allowed value 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_OK )
		return( FALSE );
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_IPADDRESS );
	krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
					 CRYPT_ATTRIBUTE_CURRENT );
	setMessageData( &msgData, buffer, 5 );	/* Above, allowed value 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	setMessageData( &msgData, buffer, 15 );	/* Below, allowed value 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	setMessageData( &msgData, buffer, 16 );	/* Equal, allowed value 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_OK )
		return( FALSE );
	krnlSendMessage( iCryptCertificate, IMESSAGE_DELETEATTRIBUTE, NULL,
					 CRYPT_CERTINFO_IPADDRESS );
	krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
					 CRYPT_ATTRIBUTE_CURRENT );
	setMessageData( &msgData, buffer, 17 );	/* Above, allowed value 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CERTINFO_IPADDRESS ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN checkSubrangeRange( IN_HANDLE const CRYPT_CONTEXT iCryptCertificate )
	{
	int value;

	REQUIRES_B( isHandleRangeValid( iCryptCertificate ) );

	/* Verify kernel range checking of special-case subranges.  Valid values 
	   are either CRYPT_CURSOR_FIRST ... CRYPT_CURSOR_LAST or an extension 
	   ID.  Since the cursor movement codes are negative values an out-of-
	   bounds value is MIN + 1 or MAX - 1, not the other way round */
	value = CRYPT_CURSOR_FIRST + 1;			/* Below, subrange 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = CRYPT_CURSOR_FIRST;				/* Low bound, subrange 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	value = CRYPT_CURSOR_LAST;				/* High bound, subrange 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	value = CRYPT_CURSOR_LAST - 1;			/* Above, subrange 1 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = CRYPT_CERTINFO_FIRST_EXTENSION - 1;	/* Below, subrange 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );
	value = CRYPT_CERTINFO_FIRST_EXTENSION;		/* Low bound, subrange 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	value = CRYPT_CERTINFO_LAST_EXTENSION;		/* High bound, subrange 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ERROR_NOTFOUND )
		return( FALSE );
	value = CRYPT_CERTINFO_LAST_EXTENSION + 1;	/* Above, subrange 2 */
	if( krnlSendMessage( iCryptCertificate, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_ATTRIBUTE_CURRENT_GROUP ) != CRYPT_ARGERROR_NUM1 )
		return( FALSE );

	return( TRUE );
	}
#endif /* USE_CERTIFICATES */

CHECK_RETVAL \
static BOOLEAN testKernelMechanisms( void )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	CRYPT_CONTEXT iCryptHandle;
#ifdef DEFAULT_ALGO_AES
	static const BYTE key[] = \
		{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
	/* AES FIPS test keys */
	static const BYTE iv[] = \
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static const int blockSize = 16, keySize = 16;
#else
	static const BYTE key[] = \
		{ 0x10, 0x46, 0x91, 0x34, 0x89, 0x98, 0x01, 0x31,
		  0x51, 0x07, 0xB0, 0x15, 0x19, 0x58, 0x01, 0x01,
		  0x19, 0x07, 0x92, 0x10, 0x98, 0x1A, 0x01, 0x01 };
	/* Single-DES keys pulled from NBS 500-20 Data Permutation
	   test vectors */
	static const BYTE iv[] = \
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static const int blockSize = 8, keySize = 24;
#endif /* DEFAULT_ALGO_AES */
	BYTE buffer[ 128 + 8 ];
	int value, status;

	/* CRYPT_ALGO_3DES is an enum and so isn't visible to the preprocessor,
	   so we have to hardcode in its actual value above, which we check
	   here */
	static_assert( CRYPT_ALGO_3DES  == 2, "3DES algorithm ID" );

	/* Verify object creation */
	setMessageCreateObjectInfo( &createInfo, DEFAULT_CRYPT_ALGO );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Object creation self-test failed" ));
		return( FALSE );
		}
	iCryptHandle = createInfo.cryptHandle;

	/* Verify the inability to access an internal object or attribute using
	   an external message.  The attribute access will be stopped by the
	   object access check before it even gets to the attribute access check,
	   so we also re-do the check further on when the object is made
	   externally visible to verify the attribute-level checks as well */
	if( krnlSendMessage( iCryptHandle, MESSAGE_GETATTRIBUTE, &value,
						 CRYPT_CTXINFO_ALGO ) != CRYPT_ARGERROR_OBJECT || \
		krnlSendMessage( iCryptHandle, MESSAGE_GETATTRIBUTE, &value,
						 CRYPT_IATTRIBUTE_TYPE ) != CRYPT_ARGERROR_VALUE )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object internal/external self-test failed" ));
		return( FALSE );
		}

	/* Verify the ability to perform standard operations and the inability
	   to perform a state = high operation on a state = low object */
	memset( buffer, 0, 16 );
	setMessageData( &msgData, ( MESSAGE_CAST ) iv, blockSize );
	if( krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_IV ) != CRYPT_OK || \
		krnlSendMessage( iCryptHandle, IMESSAGE_CTX_ENCRYPT,
						 buffer, blockSize ) != CRYPT_ERROR_NOTINITED )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object low/high state self-test failed" ));
		return( FALSE );
		}

	/* Verify the functioning of kernel range checking, phase 1: Numeric
	   values */
	if( !checkNumericRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel numeric range checking self-test failed" ));
		return( FALSE );
		}

	/* Verify the functioning of kernel range checking, phase 2: String
	   values */
	if( !checkStringRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel string range checking self-test failed" ));
		return( FALSE );
		}

	/* Verify the ability to transition a state = low object to state =
	   high */
	setMessageData( &msgData, ( MESSAGE_CAST ) key, keySize );
	if( krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEY ) != CRYPT_OK )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object state transition self-test failed" ));
		return( FALSE );
		}

	/* Verify the inability to write a read-only attribute, read a write-
	   only attribute, or delete a non-deletable attribute */
	value = CRYPT_MODE_CBC;
	setMessageData( &msgData, NULL, 0 );
	if( krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE, &value,
						 CRYPT_CTXINFO_BLOCKSIZE ) != CRYPT_ERROR_PERMISSION || \
		krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEY ) != CRYPT_ERROR_PERMISSION || \
		krnlSendMessage( iCryptHandle, IMESSAGE_DELETEATTRIBUTE, NULL,
						 CRYPT_CTXINFO_MODE ) != CRYPT_ERROR_PERMISSION )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Attribute read/write self-test failed" ));
		return( FALSE );
		}

	/* Verify the inability to perform state = low operations on a state =
	   high object */
	setMessageData( &msgData, ( MESSAGE_CAST ) key, keySize );
	if( krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE_S, &msgData,
						 CRYPT_CTXINFO_KEY ) != CRYPT_ERROR_PERMISSION || \
		krnlSendNotifier( iCryptHandle, 
						  IMESSAGE_CTX_GENKEY ) != CRYPT_ERROR_PERMISSION )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object state access self-test failed" ));
		return( FALSE );
		}

	/* Verify the inability to perform an internal-only action externally
	   but still perform it internally.  We also repeat the internal-only
	   attribute test from earlier on, this access is now stopped at the
	   attribute check level rather than the object-check level.

	   The object will become very briefly visible externally at this point,
	   but there's nothing that can be done with it because of the
	   permission settings, and it's destroyed as soon as the internal/
	   external tests are completed */
	value = \
		MK_ACTION_PERM( MESSAGE_CTX_ENCRYPT, ACTION_PERM_NONE_EXTERNAL ) | \
		MK_ACTION_PERM( MESSAGE_CTX_DECRYPT, ACTION_PERM_NONE_EXTERNAL );
	memset( buffer, 0, 16 );
	krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE, &value,
					 CRYPT_IATTRIBUTE_ACTIONPERMS );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, 
							  CRYPT_IATTRIBUTE_INTERNAL );
	if( cryptStatusError( status ) )
		{
		/* We couldn't make the object external (which can only really 
		   happen due to an internal error), explicitly destroy it since 
		   it's uncertain whether we should decrement an internal or 
		   external reference */
		krnlSendNotifier( iCryptHandle, IMESSAGE_DESTROY );
		DEBUG_DIAG(( "Object internal/external action self-test failed" ));
		return( FALSE );
		}
	if( krnlSendMessage( iCryptHandle, MESSAGE_CTX_ENCRYPT,
						 buffer, blockSize ) != CRYPT_ERROR_PERMISSION || \
		krnlSendMessage( iCryptHandle, IMESSAGE_CTX_ENCRYPT,
						 buffer, blockSize ) != CRYPT_OK )
		{
		krnlSendNotifier( iCryptHandle, MESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object internal/external action self-test failed" ));
		return( FALSE );
		}
	if( krnlSendMessage( iCryptHandle, MESSAGE_GETATTRIBUTE, &value,
						 CRYPT_IATTRIBUTE_TYPE ) != CRYPT_ARGERROR_VALUE )
		{
		krnlSendNotifier( iCryptHandle, MESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object internal/external action self-test failed" ));
		return( FALSE );
		}

	/* The following checks modify object properties and, as a side-effect,
	   render them less and less usable as the checks progress.  For this
	   reason they have to be executed in a set order, the usage-count check
	   (which disables the ability to perform operations with the object)
	   as the second-to-last check, and the locking check, which disables
	   the ability to change object properties, as the last check.  Once 
	   this check has completed, the object is locked down to a level where
	   it can't be used any more */

	/* Verify the ability to use an object with a finite usage count, the
	   inability to increment the count, the ability to decrement the count,
	   and the inability to exceed the usage count.
	   
	   After this check the usage count has dropped to zero so the object is
	   no longer usable for standard operations */
	if( !checkUsageCount( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, MESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object usage-count self-test failed" ));
		return( FALSE );
		}

	/* Verify the ability to lock an object and the inability to change
	   security parameters once it's locked.
	   
	   After this check any further changes on the security-relevant object 
	   properties are no longer possible */
	if( !checkLocking( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, MESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Object locking self-test failed" ));
		return( FALSE );
		}

	/* We've finished with the object (and in any case it's locked down to 
	   the point where it's no longer usable), destroy it */
	status = krnlSendNotifier( iCryptHandle, MESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Object destroy failed" ));
		return( FALSE );
		}

	/* The following checks require that use of certificates be enabled in
	   order to perform them.  This is because these attribute types are
	   only valid for certificates (or, by extension, certificate-using
	   object types like envelopes and sessions).  So although these
	   attribute ACLs won't be tested if certificates aren't enabled, they
	   also won't be used if certificates aren't enabled */
#ifdef USE_CERTIFICATES

	/* Create a cert object for the remaining kernel range checks */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		DEBUG_DIAG(( "Certificate object creation self-test failed" ));
		return( FALSE );
		}
	iCryptHandle = createInfo.cryptHandle;

	/* Verify functioning of the kernel range checking, phase 3: Boolean
	   values */
	if( !checkBooleanRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel boolean range checking self-test failed" ));
		return( FALSE );
		}

	/* Verify functioning of the kernel range checking, phase 4: Time
	   values */
	if( !checkTimeRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel time range checking self-test failed" ));
		return( FALSE );
		}

	/* Verify functioning of kernel range-checking, phase 6: Special-case
	   checks, allowed values */
	if( !checkAllowedValuesRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel allowed-values range checking self-test failed" ));
		return( FALSE );
		}

	/* Verify functioning of kernel range-checking, phase 6: Special-case
	   checks, subranges */
	if( !checkSubrangeRange( iCryptHandle ) )
		{
		krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
		DEBUG_DIAG(( "Kernel subranges range checking self-test failed" ));
		return( FALSE );
		}
	krnlSendNotifier( iCryptHandle, IMESSAGE_DECREFCOUNT );
#endif /* USE_CERTIFICATES */

	return( TRUE );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */

/****************************************************************************
*																			*
*							General Self-test Functions						*
*																			*
****************************************************************************/

#ifndef CONFIG_CONSERVE_MEMORY_EXTRA

/* Run internal-function self-tests.  There are two general tests that are 
   run, testFunctionality() which is an overall sanity-check of general
   functionality before we try anything else, and testKernel(), which runs
   much later and tests the kernel mechanisms */

CHECK_RETVAL_BOOL \
BOOLEAN testIntAPI( void );
CHECK_RETVAL_BOOL \
BOOLEAN testIntString( void );
CHECK_RETVAL_BOOL \
BOOLEAN testIntTime( void );
CHECK_RETVAL_BOOL \
BOOLEAN testIntBN( void );

CHECK_RETVAL_BOOL \
static BOOLEAN testInternalFunctions( void )
	{
	if( !testIntAPI() )
		{
		DEBUG_DIAG(( "int_api.c self-test failed" ));
		return( FALSE );
		}
	if( !testIntString() )
		{
		DEBUG_DIAG(( "int_string.c self-test failed" ));
		return( FALSE );
		}
	if( !testIntTime() )
		{
		DEBUG_DIAG(( "int_time.c self-test failed" ));
		return( FALSE );
		}
	if( !testIntBN() )
		{
		DEBUG_DIAG(( "ctx_bn.c self-test failed" ));
		return( FALSE );
		}

	return( TRUE );
	}

CHECK_RETVAL \
int testFunctionality( void )
	{
	ENSURES_NOFAULT( testSafetyMechanisms() );
	ENSURES_NOFAULT( testGeneralAlgorithms() );
	ENSURES_NOFAULT( testInternalFunctions() );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
int testKernel( void )
	{
	ENSURES_NOFAULT( testKernelMechanisms() );

	return( CRYPT_OK );
	}
#endif /* !CONFIG_CONSERVE_MEMORY_EXTRA */
