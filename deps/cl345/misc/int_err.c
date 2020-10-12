/****************************************************************************
*																			*
*					cryptlib Internal Error Reporting API					*
*						Copyright Peter Gutmann 1992-2016					*
*																			*
****************************************************************************/

#include <stdarg.h>
#include <stdio.h>	/* Needed on some systems for macro-mapped *printf()'s */
#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

#ifdef USE_ERRMSGS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check the error status and, if it's a leaked status code from a lower-
   level call, convert it to a generic CRYPT_ERROR_FAILED.  These status 
   codes should only ever occur in 'can't-occur' error situations so we only 
   warn in debug mode */

CHECK_RETVAL_ERROR \
static int convertErrorStatus( IN_ERROR const int status )
	{
	REQUIRES( cryptStatusError( status ) );

	if( cryptArgError( status ) )
		{
		DEBUG_DIAG(( "Error exit was passed argError status %d", status ));
		DEBUG_DUMP_STACKTRACE();
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_FAILED );
		}

	return( status );
	}

/* Format a printf-style error string.  The ERROR_INFO is annotated as
   OUT_ALWAYS because it's initalised unconditionally, the return status 
   exists only to signal to the caller that, in the case where further 
   information is added to the error information, that it's OK to add this
   further information.

   In the following we can't make the third arg a NONNULL_ARG because in the 
   Arm ABI it's a scalar value */

RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN formatErrorString( OUT_ALWAYS ERROR_INFO *errorInfoPtr, 
								  IN_STRING const char *format, 
								  IN va_list argPtr )
	{
	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	ANALYSER_HINT_STRING( format );
	ANALYSER_HINT_FORMAT_STRING( format );

	REQUIRES_B( verifyVAList( argPtr ) );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	/* This function is a bit tricky to deal with because of the 
	   braindamaged behaviour of some of the underlying functions that it 
	   may be mapped to.  Specifically, (v)snprintf() returns the number of 
	   bytes it *could* have written had it felt like it rather than how 
	   many it actually wrote on non-Windows systems and an error indicator 
	   with no guarantee of null-termination on Windows systems.  The latter 
	   isn't a problem because we both catch the error and don't require 
	   null termination, the former is more problematic because it can lead 
	   to a length indication that's larger than the actual buffer.  To 
	   handle this we explicitly check for an overflow as well as an 
	   error/underflow */
	errorInfoPtr->errorStringLength = \
				vsprintf_s( errorInfoPtr->errorString, MAX_ERRMSG_SIZE, 
							format, argPtr ); 
	if( errorInfoPtr->errorStringLength <= 0 || \
		errorInfoPtr->errorStringLength > MAX_ERRMSG_SIZE )
		{
		DEBUG_DIAG(( "Invalid error string data" ));
		assert( DEBUG_WARN );
		setErrorString( errorInfoPtr, 
						"(Couldn't record error information)", 35 );

		return( FALSE );
		}

	return( TRUE );
	}

/* Append a second error string containing further explanatory information 
   to an existing one.  There's no failure/success return value for this 
   function since there's not much that we can do in the case of a failure, 
   we rely on the existing primary error string to convey as much 
   information as possible */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void appendErrorString( INOUT ERROR_INFO *errorInfoPtr, 
							   IN_BUFFER( extErrorStringLength ) \
									const char *extErrorString, 
							   IN_LENGTH_ERRORMESSAGE \
									const int extErrorStringLength )
	{
	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtrDynamic( extErrorString, extErrorStringLength ) );

	REQUIRES_V( errorInfoPtr->errorStringLength > 0 && \
				errorInfoPtr->errorStringLength <= MAX_ERRMSG_SIZE );
	REQUIRES_V( extErrorStringLength > 0 && \
				extErrorStringLength <= MAX_ERRMSG_SIZE );

	/* If there's no room to store the combined strings, don't try and 
	   append the second string */
	if( errorInfoPtr->errorStringLength + \
							extErrorStringLength >= MAX_ERRMSG_SIZE - 8 )
		return;

	REQUIRES_V( boundsCheck( errorInfoPtr->errorStringLength,
							 extErrorStringLength, MAX_ERRMSG_SIZE ) );
	memcpy( errorInfoPtr->errorString + errorInfoPtr->errorStringLength,
			extErrorString, extErrorStringLength );
	errorInfoPtr->errorStringLength += extErrorStringLength;
	}

/****************************************************************************
*																			*
*							Clear/Set/Copy Error Strings					*
*																			*
****************************************************************************/

/* Set a fixed string as the error message.  This is used to set a 
   predefined error string from something like a table of error messages */

STDC_NONNULL_ARG( ( 1 ) ) \
void clearErrorString( OUT ERROR_INFO *errorInfoPtr )
	{
	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );

	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void setErrorString( OUT ERROR_INFO *errorInfoPtr, 
					 IN_BUFFER( stringLength ) const char *string, 
					 IN_LENGTH_ERRORMESSAGE const int stringLength )
	{
	int length = stringLength;

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtrDynamic( string, stringLength ) );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	/* Since we're already in an error-handling function we don't use the 
	   REQUIRES() predicate (which would result in an infinite page fault)
	   but make the sanity-checking of parameters explicit */
	if( stringLength <= 0 || stringLength > MAX_ERRMSG_SIZE )
		{
		DEBUG_DIAG(( "Invalid error string data" ));
		assert( DEBUG_WARN );
		string = "(Couldn't record error information)";
		length = 35;
		}
	
	REQUIRES_V( rangeCheck( length, 1, MAX_ERRMSG_SIZE ) );
	memcpy( errorInfoPtr->errorString, string, length );
	errorInfoPtr->errorStringLength = length;
	}

/* Copy error information from a low-level state structure (for example a 
   stream) to a high-level one (for example a session or envelope) */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void copyErrorInfo( OUT ERROR_INFO *destErrorInfoPtr, 
					const ERROR_INFO *srcErrorInfoPtr )
	{
	assert( isWritePtr( destErrorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( srcErrorInfoPtr, sizeof( ERROR_INFO ) ) );

	memset( destErrorInfoPtr, 0, sizeof( ERROR_INFO ) );
	if( srcErrorInfoPtr->errorStringLength > 0 )
		{
		setErrorString( destErrorInfoPtr, srcErrorInfoPtr->errorString, 
						srcErrorInfoPtr->errorStringLength );
		}
	}

/* Read error information from an object into an error-info structure */

STDC_NONNULL_ARG( ( 1 ) ) \
int readErrorInfo( OUT ERROR_INFO *errorInfo, 
				   IN_HANDLE const CRYPT_HANDLE objectHandle )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( objectHandle == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( objectHandle ) );

	/* Clear return value */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Read any additional error information that may be available */
	setMessageData( &msgData, errorInfo->errorString, MAX_ERRMSG_SIZE );
	status = krnlSendMessage( objectHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_ATTRIBUTE_ERRORMESSAGE );
	if( cryptStatusError( status ) )
		return( status );
	errorInfo->errorStringLength = msgData.length;
	ENSURES( errorInfo->errorStringLength > 0 && \
			 errorInfo->errorStringLength < MAX_ERRMSG_SIZE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Return Extended Error Information					*
*																			*
****************************************************************************/

/* Exit after recording a detailed error message.  This is used by lower-
   level code to provide more information to the caller than a basic error 
   code.  Felix qui potuit rerum cognoscere causas.

   Since we're already in an error-handling function when we call these 
   functions we don't use the REQUIRES() predicate (which would result in an 
   infinite page fault) but make the sanity-checking of parameters 
   explicit */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) STDC_PRINTF_FN( 3, 4 ) \
int retExtFn( IN_ERROR const int status, 
			  OUT ERROR_INFO *errorInfoPtr, 
			  FORMAT_STRING const char *format, ... )
	{
	va_list argPtr;
	const int localStatus = convertErrorStatus( status );

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	REQUIRES( cryptStatusError( status ) );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	va_start( argPtr, format );
	formatErrorString( errorInfoPtr, format, argPtr );
	va_end( argPtr );

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( localStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) STDC_PRINTF_FN( 3, 4 ) \
int retExtArgFn( IN_ERROR const int status, 
				 OUT ERROR_INFO *errorInfoPtr, 
				 FORMAT_STRING const char *format, ... )
	{
	va_list argPtr;

	/* This function is identical to retExtFn() except that it doesn't trap
	   CRYPT_ARGERROR_xxx values, since they're valid return values in some
	   cases */

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	REQUIRES( cryptStatusError( status ) );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	va_start( argPtr, format );
	formatErrorString( errorInfoPtr, format, argPtr );
	va_end( argPtr );

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 4 ) ) STDC_PRINTF_FN( 4, 5 ) \
int retExtObjFn( IN_ERROR const int status, 
				 OUT ERROR_INFO *errorInfoPtr, 
				 IN_HANDLE const CRYPT_HANDLE extErrorObject, 
				 FORMAT_STRING const char *format, ... )
	{
	ERROR_INFO extErrorInfo;
	va_list argPtr;
	const int localStatus = convertErrorStatus( status );
	BOOLEAN errorStringOK;
	int errorStringLength, extErrorStatus;

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	REQUIRES( cryptStatusError( status ) );
	REQUIRES( extErrorObject == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( extErrorObject ) );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	/* Format the basic error string */
	va_start( argPtr, format );
	errorStringOK = formatErrorString( errorInfoPtr, format, argPtr );
	va_end( argPtr );
	if( !errorStringOK )
		{
		/* If we couldn't format the basic error string then there's no 
		   point in continuing.  formatErrorString() throws a debug 
		   exception if there's a problem so there's no need to throw one 
		   here */
		return( localStatus );
		}
	errorStringLength = errorInfoPtr->errorStringLength;
	ENSURES( errorStringLength > 0 && errorStringLength < MAX_ERRMSG_SIZE );

	/* Check whether there's any additional error information available */
	extErrorStatus = readErrorInfo( &extErrorInfo, extErrorObject );
	if( cryptStatusError( extErrorStatus ) )
		{
		/* Nothing further to report, exit */
		return( localStatus );
		}

	/* There's additional information present via the additional object, 
	   fetch it and append it to the higher-level error message */
	if( errorStringLength + \
			extErrorInfo.errorStringLength < MAX_ERRMSG_SIZE - 32 )
		{
		REQUIRES( boundsCheck( errorStringLength + 26, 
							   extErrorInfo.errorStringLength,
							   MAX_ERRMSG_SIZE ) );
		appendErrorString( errorInfoPtr, ". Additional information: ", 26 );
		appendErrorString( errorInfoPtr, extErrorInfo.errorString, 
						   extErrorInfo.errorStringLength );
		}

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( localStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 5 ) ) STDC_PRINTF_FN( 5, 6 ) \
int retExtStrFn( IN_ERROR const int status, 
				 OUT ERROR_INFO *errorInfoPtr, 
				 IN_BUFFER( extErrorStringLength ) const char *extErrorString, 
				 IN_LENGTH_ERRORMESSAGE const int extErrorStringLength,
				 FORMAT_STRING const char *format, ... )
	{
	va_list argPtr;
	const int localStatus = convertErrorStatus( status );
	BOOLEAN errorStringOK;

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtrDynamic( extErrorString, extErrorStringLength ) );
	assert( isReadPtr( format, 4 ) );

	REQUIRES( cryptStatusError( status ) );
	REQUIRES( extErrorStringLength > 0 && \
			  extErrorStringLength < MAX_ERRMSG_SIZE );

	/* Clear return value */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );

	/* Format the basic error string */
	va_start( argPtr, format );
	errorStringOK = formatErrorString( errorInfoPtr, format, argPtr );
	va_end( argPtr );
	if( !errorStringOK )
		{
		/* If we couldn't format the basic error string then there's no 
		   point in continuing.  formatErrorString() throws a debug 
		   exception if there's a problem so there's no need to throw one 
		   here */
		return( localStatus );
		}

	/* Append the additional status string */
	appendErrorString( errorInfoPtr, extErrorString, extErrorStringLength );

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( localStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) STDC_PRINTF_FN( 4, 5 ) \
int retExtErrFn( IN_ERROR const int status, 
				 OUT ERROR_INFO *errorInfoPtr, 
				 const ERROR_INFO *existingErrorInfoPtr, 
				 FORMAT_STRING const char *format, ... )
	{
	va_list argPtr;
	const int localStatus = convertErrorStatus( status );
	char extErrorString[ MAX_ERRMSG_SIZE + 8 ];
	BOOLEAN errorStringOK;
	int extErrorStringLength;

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( existingErrorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	/* We can't clear the return value at this point because errorInfoPtr
	   could be the same as existingErrorInfoPtr */

	/* This function is typically used when the caller wants to convert 
	   something like "Low-level error string" into "High-level error 
	   string: Low-level error string".  Since the low-level error string 
	   may already be held in the errorInfo buffer where the high-level 
	   error string needs to go, we copy the string into a temporary buffer 
	   from where it can be appended back onto the string in the errorInfo 
	   buffer */
	if( existingErrorInfoPtr->errorStringLength > 0 && \
		existingErrorInfoPtr->errorStringLength <= MAX_ERRMSG_SIZE )
		{
		REQUIRES( rangeCheck( existingErrorInfoPtr->errorStringLength, 1, 
							  MAX_ERRMSG_SIZE ) );
		memcpy( extErrorString, existingErrorInfoPtr->errorString,
				existingErrorInfoPtr->errorStringLength );
		extErrorStringLength = existingErrorInfoPtr->errorStringLength;
		}
	else
		{
		memcpy( extErrorString, "(No additional information)", 27 );
		extErrorStringLength = 27;
		}
	ENSURES( extErrorStringLength > 0 && \
			 extErrorStringLength <= MAX_ERRMSG_SIZE );

	/* Format the basic error string */
	memset( errorInfoPtr, 0, sizeof( ERROR_INFO ) );
	va_start( argPtr, format );
	errorStringOK = formatErrorString( errorInfoPtr, format, argPtr );
	va_end( argPtr );
	if( !errorStringOK )
		{
		/* If we couldn't format the basic error string then there's no 
		   point in continuing.  formatErrorString() throws a debug 
		   exception if there's a problem so there's no need to throw one 
		   here */
		return( localStatus );
		}

	/* Append the additional status string */
	appendErrorString( errorInfoPtr, extErrorString, extErrorStringLength );

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( localStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) STDC_PRINTF_FN( 3, 4 ) \
int retExtErrAltFn( IN_ERROR const int status, 
					INOUT ERROR_INFO *errorInfoPtr, 
					FORMAT_STRING const char *format, ... )
	{
	va_list argPtr;
	const int localStatus = convertErrorStatus( status );
	char extErrorString[ MAX_ERRMSG_SIZE + 8 ];
	int extErrorStringLength;

	assert( isWritePtr( errorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( format, 4 ) );

	/* This function is typically used when the caller wants to convert 
	   something like "Low-level error string" into "Low-level error string,
	   additional comments".  
	   
	   First we need to check that there's actually a base error string 
	   present.  This situation can occur when a low-level function returns 
	   a cryptlib-level error status without getting/setting any extended 
	   error information, which then percolates up to a point where it's 
	   returned via a retExt() */
	if( errorInfoPtr->errorStringLength <= 0 || \
		errorInfoPtr->errorStringLength > MAX_ERRMSG_SIZE )
		{
		DEBUG_DIAG(( "Invalid error string data" ));
		assert( DEBUG_WARN );
		setErrorString( errorInfoPtr, 
						"(Couldn't record error information)", 35 );
		}

	/* Format the additional-comments string prior appending it to the base
	   error string */
	va_start( argPtr, format );
	extErrorStringLength = vsprintf_s( extErrorString, MAX_ERRMSG_SIZE, 
									   format, argPtr ); 
	va_end( argPtr );
	if( extErrorStringLength <= 0 || extErrorStringLength > MAX_ERRMSG_SIZE )
		{
		DEBUG_DIAG(( "Invalid error string data" ));
		assert( DEBUG_WARN );
		setErrorString( errorInfoPtr, 
						"(Couldn't record error information)", 35 );
		return( localStatus );
		}

	/* Append the additional status string */
	appendErrorString( errorInfoPtr, extErrorString, extErrorStringLength );

	DEBUG_PUTS( errorInfoPtr->errorString );
	return( localStatus );
	}

/****************************************************************************
*																			*
*								Helper Functions							*
*																			*
****************************************************************************/

/* Additional helper functions used to provide extended information for
   error messages.  The first set of functions provides names for error
   status values, algorithms, and key IDs */

typedef struct {
	const int value;
	const char *description;
	} DESCRIPTION_MAP;

CHECK_RETVAL \
const char *getStatusName( IN_STATUS const int errorStatus )
	{
	static const DESCRIPTION_MAP descriptionMap[] = {
		{ CRYPT_ERROR_PARAM1, "CRYPT_ERROR_PARAM1" },
		{ CRYPT_ERROR_PARAM2, "CRYPT_ERROR_PARAM2" },
		{ CRYPT_ERROR_PARAM3, "CRYPT_ERROR_PARAM3" },
		{ CRYPT_ERROR_PARAM4, "CRYPT_ERROR_PARAM4" },
		{ CRYPT_ERROR_PARAM5, "CRYPT_ERROR_PARAM5" },
		{ CRYPT_ERROR_PARAM6, "CRYPT_ERROR_PARAM6" },
		{ CRYPT_ERROR_PARAM7, "CRYPT_ERROR_PARAM7" },
		{ CRYPT_ERROR_MEMORY, "CRYPT_ERROR_MEMORY" },
		{ CRYPT_ERROR_NOTINITED, "CRYPT_ERROR_NOTINITED" },
		{ CRYPT_ERROR_INITED, "CRYPT_ERROR_INITED" },
		{ CRYPT_ERROR_NOSECURE, "CRYPT_ERROR_NOSECURE" },
		{ CRYPT_ERROR_RANDOM, "CRYPT_ERROR_RANDOM" },
		{ CRYPT_ERROR_FAILED, "CRYPT_ERROR_FAILED" },
		{ CRYPT_ERROR_INTERNAL, "CRYPT_ERROR_INTERNAL" },
		{ CRYPT_ERROR_NOTAVAIL, "CRYPT_ERROR_NOTAVAIL" },
		{ CRYPT_ERROR_PERMISSION, "CRYPT_ERROR_PERMISSION" },
		{ CRYPT_ERROR_WRONGKEY, "CRYPT_ERROR_WRONGKEY" },
		{ CRYPT_ERROR_INCOMPLETE, "CRYPT_ERROR_INCOMPLETE" },
		{ CRYPT_ERROR_COMPLETE, "CRYPT_ERROR_COMPLETE" },
		{ CRYPT_ERROR_TIMEOUT, "CRYPT_ERROR_TIMEOUT" },
		{ CRYPT_ERROR_INVALID, "CRYPT_ERROR_INVALID" },
		{ CRYPT_ERROR_SIGNALLED, "CRYPT_ERROR_SIGNALLED" },
		{ CRYPT_ERROR_OVERFLOW, "CRYPT_ERROR_OVERFLOW" },
		{ CRYPT_ERROR_UNDERFLOW, "CRYPT_ERROR_UNDERFLOW" },
		{ CRYPT_ERROR_BADDATA, "CRYPT_ERROR_BADDATA" },
		{ CRYPT_ERROR_SIGNATURE, "CRYPT_ERROR_SIGNATURE" },
		{ CRYPT_ERROR_OPEN, "CRYPT_ERROR_OPEN" },
		{ CRYPT_ERROR_READ, "CRYPT_ERROR_READ" },
		{ CRYPT_ERROR_WRITE, "CRYPT_ERROR_WRITE" },
		{ CRYPT_ERROR_NOTFOUND, "CRYPT_ERROR_NOTFOUND" },
		{ CRYPT_ERROR_DUPLICATE, "CRYPT_ERROR_DUPLICATE" },
		{ CRYPT_ENVELOPE_RESOURCE, "CRYPT_ENVELOPE_RESOURCE" },
		{ CRYPT_OK, "<<<Unknown>>>" }, { CRYPT_OK, "<<<Unknown>>>" },
		};
	int i, LOOP_ITERATOR;

	REQUIRES_EXT( cryptStatusError( errorStatus ), "<<<Unknown>>>" );

	LOOP_MED( i = 0, descriptionMap[ i ].value != CRYPT_OK && \
					 i < FAILSAFE_ARRAYSIZE( descriptionMap, \
											 DESCRIPTION_MAP ), i++ )
		{
		if( descriptionMap[ i ].value == errorStatus )
			return( descriptionMap[ i ].description );
		}
	ENSURES_EXT( LOOP_BOUND_OK, "<<<Unknown>>>" );
	ENSURES_EXT( i < FAILSAFE_ARRAYSIZE( descriptionMap, DESCRIPTION_MAP ),
				 "<<<Unknown>>>" );

	retIntError_Ext( "<<<Unknown>>>" );
	}

CHECK_RETVAL \
const char *getAlgoName( IN_ALGO const CRYPT_ALGO_TYPE cryptAlgo )
	{
	static const DESCRIPTION_MAP descriptionMap[] = {
		/* Conventional algorithms */
		{ CRYPT_ALGO_DES, "DES" },
		{ CRYPT_ALGO_3DES, "3DES" },
		{ CRYPT_ALGO_IDEA, "IDEA" },
		{ CRYPT_ALGO_CAST, "CAST-128" },
		{ CRYPT_ALGO_RC2, "RC2" },
		{ CRYPT_ALGO_RC4, "RC4" },
		{ CRYPT_ALGO_AES, "AES" },
		/* PKC algorithms */
		{ CRYPT_ALGO_DH, "DH" },
		{ CRYPT_ALGO_RSA, "RSA" },
		{ CRYPT_ALGO_DSA, "DSA" },
		{ CRYPT_ALGO_ELGAMAL, "ElGamal" },
		{ CRYPT_ALGO_ECDSA,	"ECDSA" },
		{ CRYPT_ALGO_ECDH, "ECDH" },
		/* Hash algorithms */
		{ CRYPT_ALGO_MD5, "MD5" },
		{ CRYPT_ALGO_SHA1, "SHA1" },
		{ CRYPT_ALGO_SHA2, "SHA2" },
		{ CRYPT_ALGO_SHAng,	"SHAng" },
		/* Generic secret algorithm.  This is a bit of an odd one because 
		   it's only visible internally, but we need to have some 
		   description available for it in case an attempt to create a 
		   context with it fails */
		{ CRYPT_IALGO_GENERIC_SECRET, "Generic Secret" },
		{ CRYPT_ALGO_NONE, "NONE" }, { CRYPT_ALGO_NONE, "NONE" },
		};
	int i, LOOP_ITERATOR;

	REQUIRES_EXT( isConvAlgo( cryptAlgo ) || isPkcAlgo( cryptAlgo ) || \
				  isHashAlgo( cryptAlgo ) || isSpecialAlgo( cryptAlgo ), 
				  "<<<Unknown>>>" );

	LOOP_MED( i = 0, descriptionMap[ i ].value != CRYPT_ALGO_NONE && \
					 i < FAILSAFE_ARRAYSIZE( descriptionMap, \
											 DESCRIPTION_MAP ), i++ )
		{
		if( descriptionMap[ i ].value == cryptAlgo )
			return( descriptionMap[ i ].description );
		}
	ENSURES_EXT( LOOP_BOUND_OK, "<<<Unknown>>>" );
	ENSURES_EXT( i < FAILSAFE_ARRAYSIZE( descriptionMap, DESCRIPTION_MAP ),
				 "<<<Unknown>>>" );

	retIntError_Ext( "<<<Unknown>>>" );
	}

CHECK_RETVAL \
const char *getKeyIDName( IN_KEYID const CRYPT_KEYID_TYPE keyIDtype )
	{
	static const DESCRIPTION_MAP descriptionMap[] = {
		{ CRYPT_KEYID_NAME, "CRYPT_KEYID_NAME" },
		{ CRYPT_KEYID_URI, "CRYPT_KEYID_URI" },
		{ CRYPT_IKEYID_KEYID, "CRYPT_IKEYID_KEYID" },
		{ CRYPT_IKEYID_PGPKEYID, "CRYPT_IKEYID_PGPKEYID" },
		{ CRYPT_IKEYID_CERTID, "CRYPT_IKEYID_CERTID" },
		{ CRYPT_IKEYID_SUBJECTID, "CRYPT_IKEYID_SUBJECTID" },
		{ CRYPT_IKEYID_ISSUERID, "CRYPT_IKEYID_ISSUERID" },
		{ CRYPT_IKEYID_ISSUERANDSERIALNUMBER, "CRYPT_IKEYID_ISSUERANDSERIALNUMBER" },
		{ CRYPT_KEYID_NONE, "NONE" }, { CRYPT_KEYID_NONE, "NONE" },
		};
	int i, LOOP_ITERATOR;

	REQUIRES_EXT( isEnumRange( keyIDtype, CRYPT_KEYID ), "<<<Unknown>>>" );

	LOOP_MED( i = 0, descriptionMap[ i ].value != CRYPT_KEYID_NONE && \
					 i < FAILSAFE_ARRAYSIZE( descriptionMap, \
											 DESCRIPTION_MAP ), i++ )
		{
		if( descriptionMap[ i ].value == keyIDtype )
			return( descriptionMap[ i ].description );
		}
	ENSURES_EXT( LOOP_BOUND_OK, "<<<Unknown>>>" );
	ENSURES_EXT( i < FAILSAFE_ARRAYSIZE( descriptionMap, DESCRIPTION_MAP ),
				 "<<<Unknown>>>" );

	retIntError_Ext( "<<<Unknown>>>" );
	}

/* Format a fingerprint (hash value) for display in an error message */

STDC_NONNULL_ARG( ( 1, 3 ) ) \
void formatFingerprint( OUT_BUFFER_FIXED( fingerprintTextMaxLen ) \
							char *fingerprintText, 
						IN_LENGTH_SHORT_MIN( 32 ) const int fingerprintTextMaxLen,
						IN_BUFFER( fingerprintLen ) const BYTE *fingerprint,
						IN_LENGTH_SHORT_MIN( 8 ) const int fingerprintLen )
	{
	assert( isWritePtr( fingerprintText, fingerprintTextMaxLen ) );
	assert( isReadPtr( fingerprint, fingerprintLen ) );

	REQUIRES_V( fingerprintTextMaxLen >= 32 && \
				fingerprintTextMaxLen <= MAX_INTLENGTH_SHORT );
	REQUIRES_V( fingerprintLen >= 8 && \
				fingerprintLen <= MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( fingerprintText, 0, fingerprintTextMaxLen );

	/* Format the fingerprint as ACII hex data.  The potential expansion 
	   factor is ( fingerprintLen * 3 ) + 1 (+3 for the ellipses, but -2 for 
	   the absent spaces at the start and end of the string).  Since we 
	   currently limit the output to 8 bytes we never output more than 25 
	   characters of text which is well under the 32-byte minimum buffer 
	   size */
	sprintf_s( fingerprintText, fingerprintTextMaxLen, 
			   "%02X %02X %02X %02X...%02X %02X %02X %02X",
			   fingerprint[ 0 ], fingerprint[ 1 ], 
			   fingerprint[ 2 ], fingerprint[ 3 ],
			   fingerprint[ fingerprintLen - 4 ], 
			   fingerprint[ fingerprintLen - 3 ],
			   fingerprint[ fingerprintLen - 2 ], 
			   fingerprint[ fingerprintLen - 1 ] );
	}
#else

/****************************************************************************
*																			*
*						Minimal Error Reporting Functions					*
*																			*
****************************************************************************/

/* Even if we're not using extended error reporting there is one minimal 
   facility that we still need to support, which is the copying of an integer 
   error code from source to destination */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
void copyErrorInfo( OUT ERROR_INFO *destErrorInfoPtr, 
					const ERROR_INFO *srcErrorInfoPtr )
	{
	assert( isWritePtr( destErrorInfoPtr, sizeof( ERROR_INFO ) ) );
	assert( isReadPtr( srcErrorInfoPtr, sizeof( ERROR_INFO ) ) );

	memset( destErrorInfoPtr, 0, sizeof( ERROR_INFO ) );
	destErrorInfoPtr->errorCode = srcErrorInfoPtr->errorCode;
	}
#endif /* USE_ERRMSGS */
