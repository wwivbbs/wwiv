/****************************************************************************
*																			*
*					Read CMP (and TSP) Status Information					*
*					  Copyright Peter Gutmann 1999-2009						*
*																			*
****************************************************************************/

#include <stdio.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
  #include "cmp.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
  #include "session/cmp.h"
#endif /* Compiler-specific includes */

/* The following code is shared between CMP and TSP due to TSP's use of
   random elements cut & pasted from CMP without any real understanding of
   their function or semantics */

#if defined( USE_CMP ) || defined( USE_TSP )

/* CMP includes a comical facility for the server to tell the client "You
   asked for food, I've given you a flaming telephone directory on a silver
   platter" (actually what it says is "You asked for food, I've given you
   something that isn't food but I won't tell you what").  Like much of the
   rest of CMP it's unclear what we're supposed to do in this situation, the
   only implementation that's known to use this facility will return 
   something totally different from what was requested so for now we treat
   PKISTATUS_OK_WITHINFO as an error */

#define cmpStatusOK( value )	( value == PKISTATUS_OK )

/* CMP error messages */

typedef struct {
	const int failureCode;			/* CMP failure code */
	const int status;				/* cryptlib error status */
	const char FAR_BSS *string;		/* Descriptive error message */
	const int stringLength;
	} FAILURE_INFO;

static const FAILURE_INFO FAR_BSS failureInfo[] = {
	{ CMPFAILINFO_BADALG, CRYPT_ERROR_NOTAVAIL,
	  "Unrecognized or unsupported Algorithm Identifier", 48 },
	{ CMPFAILINFO_BADMESSAGECHECK, CRYPT_ERROR_SIGNATURE,
	  "The integrity check failed (e.g. signature did not verify)", 58 },
	{ CMPFAILINFO_BADREQUEST, CRYPT_ERROR_PERMISSION,
	  "This transaction is not permitted or supported", 46 },
	{ CMPFAILINFO_BADTIME, CRYPT_ERROR_FAILED,
	  "The messageTime was not sufficiently close to the system time as "
	  "defined by local policy", 88 },
	{ CMPFAILINFO_BADCERTID, CRYPT_ERROR_NOTFOUND,
	  "No certificate could be found matching the provided criteria", 60 },
	{ CMPFAILINFO_BADDATAFORMAT, CRYPT_ERROR_BADDATA,
	  "The data submitted has the wrong format", 39 },
	{ CMPFAILINFO_WRONGAUTHORITY, CRYPT_ERROR_FAILED,
	  "The authority indicated in the request is different from the one "
	  "creating the response token", 92 },
	{ CMPFAILINFO_INCORRECTDATA, CRYPT_ERROR_FAILED,
	  "The requester's data is incorrect (used for notary services)", 60 },
	{ CMPFAILINFO_MISSINGTIMESTAMP, CRYPT_ERROR_FAILED,
	  "Timestamp is missing but should be there (by policy)", 52 },
	{ CMPFAILINFO_BADPOP, CRYPT_ERROR_SIGNATURE,
	  "The proof-of-possession failed", 30 },
	{ CMPFAILINFO_CERTREVOKED, CRYPT_ERROR_FAILED,
	  "The certificate has already been revoked", 40 },
	{ CMPFAILINFO_CERTCONFIRMED, CRYPT_ERROR_FAILED,
	  "The certificate has already been confirmed", 42 },
	{ CMPFAILINFO_WRONGINTEGRITY, CRYPT_ERROR_FAILED,
	  "Invalid integrity, password based instead of signature or vice "
	  "versa", 68 },
	{ CMPFAILINFO_BADRECIPIENTNONCE, CRYPT_ERROR_FAILED,
	  "Invalid recipient nonce, either missing or wrong value", 54 },
	{ CMPFAILINFO_TIMENOTAVAILABLE, CRYPT_ERROR_FAILED,
	  "The TSA's time source is not available", 38 },
	{ CMPFAILINFO_UNACCEPTEDPOLICY, CRYPT_ERROR_INVALID,
	  "The requested TSA policy is not supported by the TSA", 52 },
	{ CMPFAILINFO_UNACCEPTEDEXTENSION, CRYPT_ERROR_INVALID,
	  "The requested extension is not supported by the TSA", 51 },
	{ CMPFAILINFO_ADDINFONOTAVAILABLE, CRYPT_ERROR_FAILED,
	  "The additional information requested could not be understood or "
	  "is not available", 80 },
	{ CMPFAILINFO_BADSENDERNONCE, CRYPT_ERROR_FAILED,
	  "Invalid sender nonce, either missing or wrong size", 50 },
	{ CMPFAILINFO_BADCERTTEMPLATE, CRYPT_ERROR_INVALID,
	  "Invalid certificate template or missing mandatory information", 61 },
	{ CMPFAILINFO_SIGNERNOTTRUSTED, CRYPT_ERROR_WRONGKEY,
	  "Signer of the message unknown or not trusted", 44 },
	{ CMPFAILINFO_TRANSACTIONIDINUSE, CRYPT_ERROR_DUPLICATE,
	  "The transaction identifier is already in use", 44 },
	{ CMPFAILINFO_UNSUPPORTEDVERSION, CRYPT_ERROR_NOTAVAIL,
	  "The version of the message is not supported", 43 },
	{ CMPFAILINFO_NOTAUTHORIZED, CRYPT_ERROR_PERMISSION,
	  "The sender was not authorized to make the preceding request or "
	  "perform the preceding action", 91 },
	{ CMPFAILINFO_SYSTEMUNAVAIL, CRYPT_ERROR_FAILED,
	  "The request cannot be handled due to system unavailability", 58 },
	{ CMPFAILINFO_SYSTEMFAILURE, CRYPT_ERROR_FAILED,
	  "The request cannot be handled due to system failure", 51 },
	{ CMPFAILINFO_DUPLICATECERTREQ, CRYPT_ERROR_DUPLICATE,
	  "Certificate cannot be issued because a duplicate certificate "
	  "already exists", 75 },
	{ CRYPT_ERROR, CRYPT_ERROR, "Unknown PKI failure code", 24 }, 
	{ CRYPT_ERROR, CRYPT_ERROR, "Unknown PKI failure code", 24 }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Map a PKI failure information value to an error string */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int getFailureInfo( OUT_BUFFER_ALLOC_OPT( *stringLength ) \
								const char **stringPtrPtr, 
						   OUT_LENGTH_SHORT_Z int *stringLength,
						   OUT_ERROR int *failureStatus,
						   OUT_INT_SHORT_Z int *failureBitPos,
						   IN_INT_Z const int value )
	{
	const FAILURE_INFO *failureInfoPtr = NULL;
	int bitPos = 0, i;

	assert( isWritePtr( ( char ** ) stringPtrPtr, sizeof( char * ) ) );
	assert( isWritePtr( stringLength, sizeof( int ) ) );
	assert( isWritePtr( failureStatus, sizeof( int ) ) );
	assert( isWritePtr( failureBitPos, sizeof( int ) ) );

	REQUIRES( value >= 0 && value < MAX_INTLENGTH );

	/* Clear return values */
	*stringPtrPtr = NULL;
	*stringLength = *failureBitPos = 0;
	*failureStatus = CRYPT_ERROR_FAILED;

	/* For no known reason the status is encoded as a BIT STRING instead of 
	   an ENUMERATED so to find the appropriate failure string we have to 
	   walk down the bit flags to find the first failure string 
	   corresponding to a bit set in the failure information */
	if( value <= 0 )
		{
		*stringPtrPtr = "Missing PKI failure code";
		*stringLength = 24;

		return( CRYPT_OK );
		}
	for( i = 0; failureInfo[ i ].failureCode != CRYPT_ERROR && \
				i < FAILSAFE_ARRAYSIZE( failureInfo, FAILURE_INFO ); i++ )
		{
		const int failureCode = failureInfo[ i ].failureCode;

		if( ( failureCode & value ) == failureCode )
			{
			failureInfoPtr = &failureInfo[ i ];
			bitPos = i;
			break;
			}
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( failureInfo, FAILURE_INFO ) );
	if( failureInfoPtr == NULL )
		{
		*stringPtrPtr = "Unknown PKI failure code";
		*stringLength = 24;

		return( CRYPT_OK );
		}

	/* We've got information for this failure code, return it to the 
	   caller */
	*stringPtrPtr = failureInfoPtr->string;
	*stringLength = failureInfoPtr->stringLength;
	*failureStatus = failureInfoPtr->status;
	*failureBitPos = bitPos;

	return( CRYPT_OK );
	}

/* Map a cryptlib status value to PKI failure information.  Note that we use 
   a distinct mapping table rather than the general failureInfo table because 
   the mappings are multivalued, so that a single cryptlib status may 
   correspond to multiple CMP failure codes.  The mappings below are the most 
   generic ones */

static const MAP_TABLE pkiStatusMapTbl[] = {
	{ CRYPT_ERROR_NOTAVAIL, CMPFAILINFO_BADALG },
	{ CRYPT_ERROR_SIGNATURE, CMPFAILINFO_BADMESSAGECHECK },
	{ CRYPT_ERROR_PERMISSION, CMPFAILINFO_BADREQUEST },
	{ CRYPT_ERROR_BADDATA, CMPFAILINFO_BADDATAFORMAT },
	{ CRYPT_ERROR_INVALID, CMPFAILINFO_BADCERTTEMPLATE },
	{ CRYPT_ERROR_DUPLICATE, CMPFAILINFO_DUPLICATECERTREQ },
	{ CRYPT_ERROR_WRONGKEY, CMPFAILINFO_SIGNERNOTTRUSTED },
	{ CRYPT_OK, CMPFAILINFO_OK }, { CRYPT_OK, CMPFAILINFO_OK }
	};

static long getFailureBitString( IN_STATUS const int pkiStatus )
	{
	int i;

	REQUIRES_EXT( cryptStatusError( pkiStatus ), 0 );

	/* Try and map the cryptlib status value to a CMP failure information 
	   code.  We can't use mapValue() for this because we're mapping from a 
	   negative value, which is used by mapValue() as the end-of-data 
	   marker */
	for( i = 0; pkiStatusMapTbl[ i ].source != CRYPT_OK && \
				i < FAILSAFE_ARRAYSIZE( pkiStatusMapTbl, MAP_TABLE ); i++ )
		{
		if( pkiStatusMapTbl[ i ].source == pkiStatus )
			return( pkiStatusMapTbl[ i ].destination );
		}
	ENSURES_EXT( i < FAILSAFE_ARRAYSIZE( pkiStatusMapTbl, MAP_TABLE ), 0 );

	/* We couldn't find any appropriate failure information code, don't use
	   one at all */
	return( 0 );
	}

/****************************************************************************
*																			*
*							Read Status Information							*
*																			*
****************************************************************************/

/* Read PKIStatus information:

	PKIStatusInfo ::= SEQUENCE {
		status			INTEGER,
		dummy			SEQUENCE ... OPTIONAL,
		failInfo		BIT STRING OPTIONAL
		} 

   In the usual CMP weirdness the failure information is encoded as a BIT 
   STRING instead of an ENUMERATED value, and comes with a side-order of an 
   arbitrary number of free-format text strings of unknown type or 
   function.  Although we could in theory jump through all sorts of hoops to 
   try and handle the resulting multivalued status code and multivalued 
   string data it doesn't make any sense to do so and just increases our 
   attack surface significantly, so all we do is look for the first (and in 
   all known implementations only) bit set and use that as the error value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readPkiStatusInfo( INOUT STREAM *stream, 
					   const BOOLEAN isServer,
					   INOUT ERROR_INFO *errorInfo )
	{
	const char *failureString;
	long endPos, value;
	int bitString = 0, bitPos, failureStringLength, failureStatus;
	int errorCode, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	/* Clear the return values */
	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	/* Read the outer wrapper and status value */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	endPos = stell( stream ) + length;
	status = readShortInteger( stream, &value );
	if( cryptStatusOK( status ) && \
		( value < 0 || value > MAX_INTLENGTH_SHORT ) )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo,
				  "Invalid PKI status value" ) );
		}
	errorCode = ( int ) value;

	/* Read the failure information, skipping any intervening junk that may 
	   precede it */
	if( stell( stream ) < endPos && peekTag( stream ) == BER_SEQUENCE )
		status = readUniversal( stream );
	if( cryptStatusOK( status ) && stell( stream ) < endPos )
		status = readBitString( stream, &bitString );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo,
				  "Invalid PKI failure information" ) );
		}

	/* If everything's OK, we're done */
	if( cmpStatusOK( errorCode ) )
		return( CRYPT_OK );

	/* Convert the failure code into a message string and report the result 
	   to the caller */
	status = getFailureInfo( &failureString, &failureStringLength,
							 &failureStatus, &bitPos, bitString );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( cryptStatusError( failureStatus ) );
	if( bitString == 0 )
		{
		/* If we haven't been given any specific details for the problem, 
		   there's not much more that we can report.  Note that we need to
		   peform this operation after calling getFailureInfo() because
		   even though there's no returned detailed error information we're
		   still using the failure status value that's returned */
		retExt( failureStatus,
				( failureStatus, errorInfo,
				  "%s returned nonspecific failure code",
				  isServer ? "Client" : "Server" ) );
		}
	retExt( failureStatus,
			( failureStatus, errorInfo,
			  "%s returned error code %X (bit %d): %s",
			  isServer ? "Client" : "Server", bitString, bitPos, 
			  failureString ) );
	}

/****************************************************************************
*																			*
*							Write Status Information						*
*																			*
****************************************************************************/

/* Write PKIStatus information:

	PKIStatusInfo ::= SEQUENCE {
		status			INTEGER,
		failInfo		BIT STRING OPTIONAL
		} */

CHECK_RETVAL \
int sizeofPkiStatusInfo( IN_STATUS const int pkiStatus,
						 IN_ENUM_OPT( CMPFAILINFO ) const long pkiFailureInfo )
	{
	long localPKIFailureInfo;
	
	/* If it's an OK status then there's just a single integer value */
	if( cryptStatusOK( pkiStatus ) )
		return( objSize( sizeofShortInteger( PKISTATUS_OK ) ) );

	/* Return the size of the error status and optional extended error 
	   code */
	localPKIFailureInfo = ( pkiFailureInfo != CMPFAILINFO_OK ) ? \
						  pkiFailureInfo : getFailureBitString( pkiStatus );
	return( objSize( sizeofShortInteger( PKISTATUS_REJECTED ) + \
					 ( ( localPKIFailureInfo != CMPFAILINFO_OK ) ? \
						sizeofBitString( localPKIFailureInfo ) : 0 ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writePkiStatusInfo( INOUT STREAM *stream, 
						IN_STATUS const int pkiStatus,
						IN_ENUM_OPT( CMPFAILINFO ) const long pkiFailureInfo )
	{
	long localPKIFailureInfo;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( cryptStatusOK( pkiStatus ) || cryptStatusError( pkiStatus ) );
	REQUIRES( pkiFailureInfo >= CMPFAILINFO_OK && \
			  pkiFailureInfo < CMPFAILINFO_LAST );
			  /* The failure code is another piece of CMP stupidity, it 
			     looks like an enum but it's actually a bit flag, however we
				 only ever set one bit in it so we treat it as an enum for 
				 checking purposes.  In addition there's a no-error status 
				 CMPFAILINFO_OK that has the same value as CMPFAILINFO_NONE 
				 so we use _OPT and >= 0 for the low range check */

	/* If it's an OK status then there's just a single integer value */
	if( cryptStatusOK( pkiStatus ) )
		{
		writeSequence( stream, sizeofShortInteger( PKISTATUS_OK ) );
		return( writeShortInteger( stream, PKISTATUS_OK, DEFAULT_TAG ) );
		}

	/* Write the error status and optional extended error code */
	localPKIFailureInfo = ( pkiFailureInfo != CMPFAILINFO_OK ) ? \
						  pkiFailureInfo : getFailureBitString( pkiStatus );
	if( localPKIFailureInfo == CMPFAILINFO_OK )
		{
		/* There's no extended error code, just write a basic failure 
		   status */
		writeSequence( stream, sizeofShortInteger( PKISTATUS_REJECTED ) );
		return( writeShortInteger( stream, PKISTATUS_REJECTED, DEFAULT_TAG ) );
		}
	writeSequence( stream, sizeofShortInteger( PKISTATUS_REJECTED ) + \
						   sizeofBitString( localPKIFailureInfo ) );
	writeShortInteger( stream, PKISTATUS_REJECTED, DEFAULT_TAG );
	return( writeBitString( stream, localPKIFailureInfo, DEFAULT_TAG ) );
	}
#endif /* USE_CMP || USE_TSP */
