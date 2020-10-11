/****************************************************************************
*																			*
*						cryptlib Enveloping Test Routines					*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#include <limits.h>		/* To determine max.buffer size we can encrypt */
#include "cryptlib.h"
#include "test/test.h"

/* Various features can be disabled by configuration options, in order to 
   handle this we need to include the cryptlib config file so that we can 
   selectively disable some tests.
   
   Note that this checking isn't perfect, if cryptlib is built in release
   mode but we include config.h here in debug mode then the defines won't
   match up because the use of debug mode enables extra options that won't
   be enabled in the release-mode cryptlib */
#include "misc/config.h"
#include "misc/consts.h"	/* For DEFAULT_CRYPT_ALGO */

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */
#ifndef NDEBUG
  #include "misc/analyse.h"		/* Needed for fault.h */
  #include "misc/fault.h"
#endif /* !NDEBUG */

/* Test data to use for the self-test.  The PGP test data is slightly
   different since it's not possible to include a null character in data
   generated via the command-line versions of PGP.  On EBCDIC systems we
   have to hardcode in the character codes since the pre-generated data
   came from an ASCII system */

#if defined( __MVS__ ) || defined( __VMCMS__ )
  #define ENVELOPE_TESTDATA		( ( BYTE * ) "\x53\x6F\x6D\x65\x20\x74\x65\x73\x74\x20\x64\x61\x74\x61" )
  #define ENVELOPE_PGP_TESTDATA	( ( BYTE * ) "\x53\x6F\x6D\x65\x20\x74\x65\x73\x74\x20\x64\x61\x74\x61\x2E" )
  #define ENVELOPE_COMPRESSEDDATA	"\x2F\x2A\x20\x54\x68\x69\x73\x20\x69\x73\x20\x61\x20\x6C\x6F\x77\x65\x73\x74\x2D"
#else
  #define ENVELOPE_TESTDATA		( ( BYTE * ) "Some test data" )
  #define ENVELOPE_PGP_TESTDATA	( ( BYTE * ) "Some test data." )
  #define ENVELOPE_COMPRESSEDDATA	"/* This is a lowest-"
#endif /* EBCDIC systems */
#define ENVELOPE_TESTDATA_SIZE			15
#define ENVELOPE_COMPRESSEDDATA_SIZE	20

/* External flags which indicate that the key read/update routines worked OK.
   This is set by earlier self-test code, if it isn't set some of the tests
   are disabled */

extern int keyReadOK, doubleCertOK;

#if defined( TEST_ENVELOPE ) || defined( TEST_SESSION )	/* For TSP enveloping */

/****************************************************************************
*																			*
*								Utility Routines 							*
*																			*
****************************************************************************/

/* The general-purpose buffer used for enveloping.  We use a fixed buffer
   if possible to save having to add huge amounts of allocation/deallocation
   code */

BYTE globalBuffer[ BUFFER_SIZE ];

/* Determine the size of a file.  If there's a problem, we return the
   default buffer size, which will cause a failure further up the chain
   where the error can be reported better */

static int getFileSize( const char *fileName )
	{
	FILE *filePtr;
	long size;

	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		return( BUFFER_SIZE );
	fseek( filePtr, 0L, SEEK_END );
	size = ftell( filePtr );
	fclose( filePtr );
	if( size > INT_MAX )
		return( BUFFER_SIZE );

	return( ( int ) size );
	}

/* Read test data from a file */

static int readFileFromTemplate( const C_STR fileTemplate, const int count,
								 const char *description, BYTE *buffer, 
								 const int bufSize )
	{
	BYTE fileName[ BUFFER_SIZE ];

	filenameFromTemplate( fileName, fileTemplate, count );
	return( readFileData( fileName, description, buffer, bufSize, 32, 
						  FALSE ) );
	}

/* Verify the contents of a data buffer */

static void initBufferContents( BYTE *buffer, const int bufSize ) 
	{
	int i;

	for( i = 0; i < bufSize; i++ )
		buffer[ i ] = i & 0xFF;
	}

static int verifyBufferContents( const BYTE *buffer, const int bufSize,
								 const int startCount )
	{
	int i;

	for( i = 0; i < bufSize; i++ )
		{
		if( buffer[ i ] != ( ( startCount + i ) & 0xFF ) )
			{
			fprintf( outputStream, "De-enveloped data != original data at "
					 "byte %d, line %d.\n", i, __LINE__ );
			return( FALSE );
			}
		}

	return( TRUE );
	}

/* Common routines to create an envelope, add enveloping information, push
   data, pop data, and destroy an envelope */

static int createEnvelope( CRYPT_ENVELOPE *envelope,
						   const CRYPT_FORMAT_TYPE formatType )
	{
	int status;

	/* Create the envelope */
	status = cryptCreateEnvelope( envelope, CRYPT_UNUSED, formatType );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateEnvelope() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int createDeenvelope( CRYPT_ENVELOPE *envelope )
	{
	int status;

	/* Create the envelope */
	status = cryptCreateEnvelope( envelope, CRYPT_UNUSED, CRYPT_FORMAT_AUTO );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptCreateEnvelope() for de-enveloping "
				 "failed with error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int addEnvInfoString( const CRYPT_ENVELOPE envelope,
							 const CRYPT_ATTRIBUTE_TYPE type,
							 const void *envInfo, const int envInfoLen )
	{
	int status;

	status = cryptSetAttributeString( envelope, type, envInfo, envInfoLen );
	if( cryptStatusError( status ) )
		{
		printExtError( envelope, "cryptSetAttributeString()", status, 
					   __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int addEnvInfoNumeric( const CRYPT_ENVELOPE envelope,
							  const CRYPT_ATTRIBUTE_TYPE type,
							  const int envInfo )
	{
	int status;

	status = cryptSetAttribute( envelope, type, envInfo );
	if( cryptStatusError( status ) )
		{
		printExtError( envelope, "cryptSetAttributeString()", status, 
					   __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int processEnvelopeResource( const CRYPT_ENVELOPE envelope, 
									const void *stringEnvInfo,
									const int numericEnvInfo,
									BOOLEAN *isRestartable )
	{
	BOOLEAN isWrongKey = FALSE, exitLoop = FALSE;
	int cryptEnvInfo, cryptAlgo, keySize DUMMY_INIT;
	int integrityLevel, iteration = 0, status;

	/* Args are either NULL, a handle, or { data, length } */
	assert( ( stringEnvInfo == NULL && numericEnvInfo == 0 ) || \
			( stringEnvInfo == NULL && numericEnvInfo != 0 ) || \
			( stringEnvInfo != NULL && numericEnvInfo >= 1 ) );

	/* Clear return value */
	*isRestartable = FALSE;
	
	/* Add the appropriate enveloping information that we need to 
	   continue */
	status = cryptSetAttribute( envelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
								CRYPT_CURSOR_FIRST );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Attempt to move cursor to start of list "
				 "failed with error code %d, line %d.\n", status, 
				 __LINE__ );
		return( status );
		}
	do
		{
		C_CHR label[ CRYPT_MAX_TEXTSIZE + 1 ];
		int labelLength;

		status = cryptGetAttribute( envelope, CRYPT_ATTRIBUTE_CURRENT,
									&cryptEnvInfo );
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Attempt to read current group failed "
					 "with error code %d, line %d.\n", status, __LINE__ );
			return( status );
			}
		iteration++;

		switch( cryptEnvInfo )
			{
			case CRYPT_ATTRIBUTE_NONE:
				/* The required information was supplied via other means (in 
				   practice this means there's a crypto device available and 
				   that was used for the decrypt), there's nothing left to 
				   do */
				fputs( "(Decryption key was recovered using crypto device "
					   "or non-password-protected\n private key).\n", 
					   outputStream );
				break;

			case CRYPT_ENVINFO_PRIVATEKEY:
				/* If there's no decryption password present, the private 
				   key must be passed in directly */
				if( stringEnvInfo == NULL )
					{
					status = cryptSetAttribute( envelope,
												CRYPT_ENVINFO_PRIVATEKEY,
												numericEnvInfo );
					if( cryptStatusError( status ) )
						{
						if( status != CRYPT_ERROR_WRONGKEY )
							{
							fprintf( outputStream, "Attempt to add private "
									 "key failed with error code %d, "
									 "line %d.\n", status, __LINE__ );
							return( status );
							}
						fprintf( outputStream, "Attempt to add private key "
								 "for attribute #%d failed with "
								 "CRYPT_ERROR_WRONGKEY,\n  trying for "
								 "subsequent attributes.\n", iteration );
						isWrongKey = TRUE;
						}
					else
						{
						if( isWrongKey )
							{
							isWrongKey = FALSE;
							fprintf( outputStream, "Decryption succeeded "
									 "for attribute #%d.\n", iteration );
							}
						exitLoop = TRUE;
						}
					*isRestartable = TRUE;
					break;
					}

				/* A private-key keyset is present in the envelope, we need 
				   a password to decrypt the key */
				status = cryptGetAttributeString( envelope,
									CRYPT_ENVINFO_PRIVATEKEY_LABEL,
									label, &labelLength );
				if( cryptStatusError( status ) )
					{
					fprintf( outputStream, "Private key label read failed "
							 "with error code %d, line %d.\n", status, 
							 __LINE__ );
					return( status );
					}
#ifdef UNICODE_STRINGS
				label[ labelLength / sizeof( wchar_t ) ] = '\0';
				fprintf( outputStream, "Need password to decrypt private "
						 "key '%S'.\n", label );
#else
				label[ labelLength ] = '\0';
				fprintf( outputStream, "Need password to decrypt private key "
						 "'%s'.\n", label );
#endif /* UNICODE_STRINGS */
				if( !addEnvInfoString( envelope, CRYPT_ENVINFO_PASSWORD,
									   stringEnvInfo, numericEnvInfo ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_PASSWORD:
				fputs( "Need user password.\n", outputStream );
				assert( stringEnvInfo != NULL );	/* For static analysers */
				status = cryptSetAttributeString( envelope, 
									CRYPT_ENVINFO_PASSWORD, stringEnvInfo, 
									numericEnvInfo );
				if( cryptStatusError( status ) )
					{
					if( status != CRYPT_ERROR_WRONGKEY )
						{
						fprintf( outputStream, "Attempt to add password "
								 "failed with error code %d, line %d.\n", 
								 status, __LINE__ );
						return( status );
						}
					fprintf( outputStream, "Attempt to add password for "
							 "attribute #%d failed with "
							 "CRYPT_ERROR_WRONGKEY, trying for subsequent "
							 "attributes.\n", iteration );
					isWrongKey = TRUE;
					}
				else
					{
					if( isWrongKey )
						{
						isWrongKey = FALSE;
						fprintf( outputStream, "Decryption succeeded for "
								 "attribute #%d.\n", iteration );
						}
					exitLoop = TRUE;
					}
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_SESSIONKEY:
				fputs( "Need session key.\n", outputStream );
				if( !addEnvInfoNumeric( envelope, CRYPT_ENVINFO_SESSIONKEY,
										numericEnvInfo ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_KEY:
				fputs( "Need conventional encryption key.\n", 
					   outputStream );
				if( !addEnvInfoNumeric( envelope, CRYPT_ENVINFO_KEY,
										numericEnvInfo ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_SIGNATURE:
				/* If we've processed the entire data block in one go, we 
				   may end up with only signature information available, in 
				   which case we defer processing them until after we've 
				   finished with the deenveloped data */
				break;

			default:
				fprintf( outputStream, "Need unknown enveloping information "
						 "type %d.\n", cryptEnvInfo );
				return( SENTINEL );
			}
		}
	while( !exitLoop && \
		   cryptSetAttribute( envelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							  CRYPT_CURSOR_NEXT ) == CRYPT_OK );

	/* If we couldn't find a usable decryption key, we can't go any 
	   further */
	if( isWrongKey )
		{
		fprintf( outputStream, "Couldn't find key capable of decrypting "
				 "enveloped data, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Check whether there's any integrity protection present */
	status = cryptGetAttribute( envelope, CRYPT_ENVINFO_INTEGRITY, 
								&integrityLevel );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Couldn't query integrity protection used "
				 "in envelope, status %d, line %d.\n", status, __LINE__ );
		return( status );
		}
	if( integrityLevel > CRYPT_INTEGRITY_NONE )
		{
		/* Display the integrity level.  For PGP it's not really a MAC but
		   a sort-of-keyed encrypted hash, but we call it a MAC to keep 
		   things simple */
		fprintf( outputStream, "Data is integrity-protected using %s "
				 "authentication.\n",
				( integrityLevel == CRYPT_INTEGRITY_MACONLY ) ? \
					"MAC" : \
				( integrityLevel == CRYPT_INTEGRITY_FULL ) ? \
					"MAC+encrypt" : "unknown" );
		}

	/* If we're not using encryption, we're done */
	if( cryptEnvInfo != CRYPT_ATTRIBUTE_NONE && \
		cryptEnvInfo != CRYPT_ENVINFO_PRIVATEKEY && \
		cryptEnvInfo != CRYPT_ENVINFO_PASSWORD )
		{
		return( CRYPT_OK );
		}
	if( integrityLevel == CRYPT_INTEGRITY_MACONLY )
		return( CRYPT_OK );

	/* If we're using some form of encrypted enveloping, report the 
	   algorithm and keysize used */
	status = cryptGetAttribute( envelope, CRYPT_CTXINFO_ALGO, &cryptAlgo );
	if( cryptStatusOK( status ) )
		{
		status = cryptGetAttribute( envelope, CRYPT_CTXINFO_KEYSIZE, 
									&keySize );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Couldn't query encryption algorithm and "
				 "keysize used in envelope, status %d, line %d.\n", status, 
				 __LINE__ );
		return( status );
		}
	fprintf( outputStream, "Data is protected using algorithm %d with %d "
			 "bit key.\n", cryptAlgo, keySize * 8 );
	
	return( CRYPT_OK );
	}

static int pushData( const CRYPT_ENVELOPE envelope, const BYTE *buffer,
					 const int length, const void *stringEnvInfo,
					 const int numericEnvInfo )
	{
	int bytesIn, contentType, status;

	/* Args are either NULL, a handle, or { data, length } */
	assert( ( stringEnvInfo == NULL && numericEnvInfo == 0 ) || \
			( stringEnvInfo == NULL && numericEnvInfo != 0 ) || \
			( stringEnvInfo != NULL && numericEnvInfo >= 1 ) );

	/* Push in the data */
	status = cryptPushData( envelope, buffer, length, &bytesIn );
	if( status == CRYPT_ENVELOPE_RESOURCE )
		{
		BOOLEAN isRestartable = FALSE;

		/* Process the required de-enveloping resource */
		status = processEnvelopeResource( envelope, stringEnvInfo, 
										  numericEnvInfo, &isRestartable );
		if( cryptStatusError( status ) )
			return( status );

		/* If we only got some of the data in due to the envelope stopping to
		   ask us for a decryption resource, push in the rest */
		if( bytesIn < length && isRestartable )
			{
			const int initialBytesIn = bytesIn;

			status = cryptPushData( envelope, buffer + initialBytesIn,
									length - initialBytesIn, &bytesIn );
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "cryptPushData() for remaining data "
						 "failed with error code %d, line %d.\n", status, 
						 __LINE__ );
				return( status );
				}
			bytesIn += initialBytesIn;
			}
		}
	else
		{
		if( cryptStatusError( status ) )
			{
			printExtError( envelope, "cryptPushData()", status, __LINE__ );
			return( status );
			}
		}
	if( bytesIn < length )
		{
		BYTE tempBuffer[ 8192 ];
		int bytesIn2, bytesOut;

		/* In the case of very large data quantities we may run out of 
		   envelope buffer space during processing so we have to push the
		   remainder a second time.  Removing some of the data destroys
		   the ability to compare the popped data with the input data later
		   on, but this is only done for the known self-test data and not 
		   for import of arbitrary external data quantities */
		fprintf( outputStream, "(Ran out of input buffer data space, "
				 "popping %d bytes to make room).\n", 8192 );
		status = cryptPopData( envelope, tempBuffer, 8192, &bytesOut );
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "cryptPopData() to make way for "
					 "remaining data failed with error code %d, line %d.\n", 
					 status, __LINE__ );
			return( status );
			}
		status = cryptPushData( envelope, buffer + bytesIn, 
								length - bytesIn, &bytesIn2 );
		if( cryptStatusError( status ) )
			{
			printExtError( envelope, "cryptPushData() of remaining data", 
						   status, __LINE__ );
			return( status );
			}
		bytesIn += bytesIn2;
		}
	if( bytesIn != length )
		{
		fprintf( outputStream, "cryptPushData() only copied %d of %d "
				 "bytes, line %d.\n", bytesIn, length, __LINE__ );
		return( SENTINEL );
		}

	/* Flush the data */
	status = cryptFlushData( envelope );
	if( cryptStatusError( status ) && status != CRYPT_ERROR_COMPLETE )
		{
		printExtError( envelope, "cryptFlushData()", status, __LINE__ );
		return( status );
		}

	/* Now that we've finished processing the data, report the inner content 
	   type.  We can't do in until this stage because some enveloping format
	   types encrypt the inner content type with the payload, so we can't 
	   tell what it is until we've decrypted the payload */
	status = cryptGetAttribute( envelope, CRYPT_ENVINFO_CONTENTTYPE,
								&contentType );
	if( cryptStatusError( status ) )
		{
		int value;

		/* A detached signature doesn't have any content to an inability to 
		   determine the content type isn't a failure */
		status = cryptGetAttribute( envelope, CRYPT_ENVINFO_DETACHEDSIGNATURE,
									&value );
		if( cryptStatusOK( status ) && value == TRUE )
			{
			fputs( "(Data is from a detached signature, couldn't determine "
				   "content type).\n", outputStream );
			return( bytesIn );
			}
		fprintf( outputStream, "Couldn't query content type in envelope, "
				 "status %d, line %d.\n", status, __LINE__ );
		return( status );
		}
	if( contentType != CRYPT_CONTENT_DATA )
		{
		fprintf( outputStream, "Nested content type = %d.\n", 
				 contentType );
		}

	return( bytesIn );
	}

static int popData( CRYPT_ENVELOPE envelope, BYTE *buffer, int bufferSize )
	{
	int status, bytesOut;

	status = cryptPopData( envelope, buffer, bufferSize, &bytesOut );
	if( cryptStatusError( status ) )
		{
		printExtError( envelope, "cryptPopData()", status, __LINE__ );
		return( status );
		}

	return( bytesOut );
	}

static int destroyEnvelope( CRYPT_ENVELOPE envelope )
	{
	int status;

	/* Destroy the envelope */
	status = cryptDestroyEnvelope( envelope );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptDestroyEnvelope() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Data Enveloping Routines 						*
*																			*
****************************************************************************/

/* Test raw data enveloping */

typedef enum {
	DATA_AMOUNT_SMALL,
	DATA_AMOUNT_MEDIUM,
	DATA_AMOUNT_LARGE,
	DATA_AMOUNT_VARIABLE,
	DATA_AMOUNT_MEDIUM_MULTIPLE
	} DATA_AMOUNT_TYPE;

static int envelopeData( const char *dumpFileName,
						 const BOOLEAN useDatasize,
						 const DATA_AMOUNT_TYPE dataAmountType,
						 const int dataAmount,
						 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BYTE *inBufPtr, *outBufPtr = globalBuffer;
	int length, bufSize, count;

	switch( dataAmountType )
		{
		case DATA_AMOUNT_SMALL:
			fprintf( outputStream, "Testing %splain data enveloping%s...\n",
					 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
					 ( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
					 " with datasize hint" : "" );
			length = ENVELOPE_TESTDATA_SIZE;
			inBufPtr = ENVELOPE_TESTDATA;
			break;

		case DATA_AMOUNT_MEDIUM:
		case DATA_AMOUNT_MEDIUM_MULTIPLE:
			fprintf( outputStream, "Testing %splain data enveloping of "
					 "%sintermediate-size data...\n",
					( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
					( dataAmountType == DATA_AMOUNT_MEDIUM_MULTIPLE ) ? \
					  "multiple blocks of " : "" );
			length = 512;
			inBufPtr = globalBuffer;
			initBufferContents( inBufPtr, length );
			break;

		case DATA_AMOUNT_LARGE:
			fprintf( outputStream, "Testing %senveloping of large data "
					 "quantity...\n",
					 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );

			/* Allocate a large buffer and fill it with a known value */
			length = ( INT_MAX <= 32768L ) ? 16384 : 1048576;
			if( ( inBufPtr = malloc( length + 128 ) ) == NULL )
				{
				fprintf( outputStream, "Couldn't allocate buffer of %d "
						 "bytes, skipping large buffer enveloping test.\n\n", 
						 length );
				return( TRUE );
				}
			outBufPtr = inBufPtr;
			initBufferContents( inBufPtr, length );
			break;

		case DATA_AMOUNT_VARIABLE:
			length = dataAmount;
			inBufPtr = globalBuffer;
			initBufferContents( inBufPtr, length );
			break;

		default:
			return( FALSE );
		}
	bufSize = length + 128;

	/* Create the envelope, push in the data, pop the enveloped result, and
	   destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( useDatasize )
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, length );
	if( dataAmountType == DATA_AMOUNT_LARGE )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
		}
	count = pushData( cryptEnvelope, inBufPtr, length, NULL, 0 );
	if( cryptStatusError( count ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( dataAmountType == DATA_AMOUNT_SMALL && \
		count != length + ( ( formatType == CRYPT_FORMAT_PGP ) ? 8 : \
							useDatasize ? 17 : 25 ) )
		{
		fprintf( outputStream, "Enveloped data length %d, should be %d, "
				 "line %d.\n", count, length + 25, __LINE__ );
		return( FALSE );
		}

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	if( dumpFileName != NULL && dataAmountType != DATA_AMOUNT_LARGE )
		debugDump( dumpFileName, outBufPtr, count );

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( dataAmountType == DATA_AMOUNT_LARGE )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
		}
	count = pushData( cryptEnvelope, outBufPtr, count, NULL, 0 );
	if( cryptStatusError( count ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		{
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}

	/* Make sure that the result matches what we pushed */
	if( count != length )
		{
		fputs( "De-enveloped data length != original length.\n", 
			   outputStream );
		if( dataAmountType == DATA_AMOUNT_LARGE )
			free( inBufPtr );
		return( FALSE );
		}
	if( dataAmountType != DATA_AMOUNT_SMALL )
		{
		if( !verifyBufferContents( outBufPtr, length, 0 ) )
			{
			if( dataAmountType == DATA_AMOUNT_LARGE )
				free( inBufPtr );
			return( FALSE );
			}
		}
	else
		{
		if( !compareData( ENVELOPE_TESTDATA, length, outBufPtr, length ) )
			{
			if( dataAmountType == DATA_AMOUNT_LARGE )
				free( inBufPtr );
			return( FALSE );
			}
		}

	/* Clean up */
	if( dataAmountType == DATA_AMOUNT_LARGE )
		free( inBufPtr );
	fputs( "Enveloping of plain data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeData( void )
	{
	if( !envelopeData( "env_datn", FALSE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite-length */
	if( !envelopeData( "env_dat", TRUE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	if( !envelopeData( "env_dat.pgp", TRUE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format */
	return( envelopeData( "env_datl.pgp", TRUE, DATA_AMOUNT_MEDIUM, 0, CRYPT_FORMAT_PGP ) );
	}						/* PGP format, longer data */

int testEnvelopeDataLargeBuffer( void )
	{
	if( !envelopeData( NULL, TRUE, DATA_AMOUNT_LARGE, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, large buffer */
	return( envelopeData( NULL, TRUE, DATA_AMOUNT_LARGE, 0, CRYPT_FORMAT_PGP ) );
	}						/* Large buffer, PGP format */

int testEnvelopeDataVariable( void )
	{
	FILE *origOutputStream = outputStream;
	const int maxCount = min( BUFFER_SIZE, 8192 );
	int i;				 /* Maximum is 32600, from size of internal envelope buffer */

	fprintf( outputStream, "Testing enveloping quantities from 1 to %d "
			 "bytes to check for\n  boundary errors...\n", maxCount );

	outputStream = fopen( DEVNULL, "w" );
	assert( outputStream != NULL );
	for( i = 1; i < maxCount; i++ )
		{
		fprintf( origOutputStream, "%d.\r", i );
		if( !envelopeData( NULL, FALSE, DATA_AMOUNT_VARIABLE, i, CRYPT_FORMAT_CRYPTLIB ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "Failed for indefinite-length enveloping "
					 "size %d.\n", i );
			return( FALSE );	/* Indefinite-length */
			}
		if( !envelopeData( NULL, TRUE, DATA_AMOUNT_VARIABLE, i, CRYPT_FORMAT_CRYPTLIB ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "Failed for definite-length enveloping "
					 "size %d.\n", i );
			return( FALSE );	/* Datasize */
			}
		if( !envelopeData( NULL, TRUE, DATA_AMOUNT_VARIABLE, i, CRYPT_FORMAT_PGP ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "Failed for PGP-format enveloping "
					 "size %d.\n", i );
			return( FALSE );	/* PGP format */
			}
		}
	outputStream = origOutputStream;

	fprintf( outputStream, "Boundary error test succeeded.\n\n" );
	return( TRUE );
	}

static int envelopeDataMultiple( const int dataSize, 
								 const BOOLEAN useEncryption )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_ENVELOPE cryptEnvelope, decryptEnvelope;
	BYTE inBuffer[ BUFFER_SIZE ], *inBufPtr = inBuffer;
	BYTE *outBufPtr = globalBuffer;
	int count, startCount = 0, i, status;

	/* Create the encryption context that'll be used for en/de-enveloping */
	if( useEncryption )
		{
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
									 CRYPT_ALGO_AES );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
								 "0123456789ABCDEF", 16 );
		}

	initBufferContents( inBufPtr, BUFFER_SIZE );
	if( !createEnvelope( &cryptEnvelope, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );
	if( useEncryption && \
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SESSIONKEY,
							cryptContext ) )
		return( FALSE );
	if( !createDeenvelope( &decryptEnvelope ) )
		return( FALSE );
	for( i = 0; i < BUFFER_SIZE / dataSize; i++ )
		{
		/* Envelope the data */
		status = cryptPushData( cryptEnvelope, inBufPtr, dataSize, &count );
		if( cryptStatusError( status ) )
			return( FALSE );
		inBufPtr += count;
		count = popData( cryptEnvelope, outBufPtr, BUFFER_SIZE );
		if( cryptStatusError( count ) )
			return( FALSE );

		/* If the envelope has absorbed all of the data, continue */
		if( count == 0 )
			continue;

		/* De-envelope the data */
		status = cryptPushData( decryptEnvelope, outBufPtr, count, &count );
		if( status == CRYPT_ENVELOPE_RESOURCE )
			{
			if( !addEnvInfoNumeric( decryptEnvelope, CRYPT_ENVINFO_SESSIONKEY,
									cryptContext ) )
				return( FALSE );
			status = CRYPT_OK;
			}
		if( cryptStatusError( status ) )
			return( FALSE );
		status = cryptPopData( decryptEnvelope, outBufPtr, BUFFER_SIZE, 
							   &count );
		if( cryptStatusError( count ) )
			{
			/* If we're doing encrypted enveloping then there won't be any 
			   output available until we've got a full block of data */
			if( useEncryption && i < 16 )
				continue;
			return( FALSE );
			}

		/* Make sure that it matches what was pushed into the envelope */
		if( !verifyBufferContents( outBufPtr, count, startCount ) )
			return( FALSE );
		startCount = ( startCount + count ) & 0xFF;
		}

	/* Flush through the final trailer bytes, e.g. EOCs */
	status = cryptFlushData( cryptEnvelope );
	if( cryptStatusError( status ) )
		return( FALSE );
	count = popData( cryptEnvelope, outBufPtr, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	status = cryptPushData( decryptEnvelope, outBufPtr, count, &count );
	if( cryptStatusError( status ) )
		return( FALSE );
	count = popData( decryptEnvelope, outBufPtr, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );

	/* Clean up */
	if( useEncryption )
		cryptDestroyContext( cryptContext );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( !destroyEnvelope( decryptEnvelope ) )
		return( FALSE );

	return( TRUE );
	}

int testEnvelopeDataMultiple( void )
	{
	int i;

	fprintf( outputStream, "Testing plain data enveloping of multiple "
			 "blocks of data...\n" );
	for( i = 1; i < 8192; i++ )
		{
		fprintf( outputStream, "%d.\r", i );
		if( !envelopeDataMultiple( i, FALSE ) )
			return( FALSE );
		}
	fprintf( outputStream, "Enveloping of multiple blocks of data "
			 "succeeded.\n\n" );
	return( TRUE );
	}

/* Test compressed enveloping */

static int envelopeDecompress( BYTE *buffer, const int bufSize,
							   const int length )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BYTE smallBuffer[ 128 ];
	int count, zeroCount;

	/* Create the envelope, push in the data, and pop the de-enveloped
	   result */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	count = pushData( cryptEnvelope, buffer, length, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, buffer, bufSize );
	if( cryptStatusError( count ) )
		{
#ifdef __hpux
		if( count == -1 )
			{
			fputs( "Older HPUX compilers break zlib, to remedy this you "
				   "can either get a better\ncompiler/OS or grab a debugger "
				   "and try to figure out what HPUX is doing to\nzlib.  To "
				   "continue the self-tests, comment out the call to\n"
				   "testEnvelopeCompress() and rebuild.\n\n", 
				   outputStream );
			}
#endif /* __hpux */
		return( FALSE );
		}

	/* See what happens when we try and pop out more data.  This test is done
	   because some compressed-data formats don't indicate the end of the
	   data properly, and we need to make sure that the de-enveloping code
	   handles this correctly.  This also tests for correct handling of 
	   oddball OOB data tacked onto the end of the payload, like PGP's 
	   MDCs */
	zeroCount = popData( cryptEnvelope, smallBuffer, 128 );
	if( zeroCount != 0 )
		{
		fprintf( outputStream, "Attempt to pop more data after end-of-data "
				 "had been reached succeeded, the\nenvelope should have "
				 "reported 0 bytes available rather than %d.\n", 
				 zeroCount );
		return( FALSE );
		}

	/* Clean up */
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopeCompress( const char *dumpFileName,
							 const BOOLEAN useDatasize,
							 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	FILE *inFile;
	BYTE *buffer, *envelopedBuffer;
	int dataCount = 0, count, status;

	fprintf( outputStream, "Testing %scompressed data enveloping%s...\n",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
			 useDatasize ? " with datasize hint" : ""  );

	/* Since this needs a nontrivial amount of data for the compression, we
	   read it from an external file into dynamically-allocated buffers */
	if( ( ( buffer = malloc( FILEBUFFER_SIZE ) ) == NULL ) || \
		( ( envelopedBuffer = malloc( FILEBUFFER_SIZE ) ) == NULL ) )
		{
		if( buffer != NULL )
			free( buffer );
		fputs( "Couldn't allocate test buffers.\n\n", outputStream );
		return( FALSE );
		}
	inFile = fopen( convertFileName( COMPRESS_FILE ), "rb" );
	if( inFile != NULL )
		{
		dataCount = fread( buffer, 1, FILEBUFFER_SIZE, inFile );
		fclose( inFile );
		assert( dataCount < FILEBUFFER_SIZE );
		}
	if( dataCount < 1000 || dataCount == FILEBUFFER_SIZE )
		{
		free( buffer );
		free( envelopedBuffer );
		fputs( "Couldn't read test file for compression.\n\n", 
			   outputStream );
		return( FALSE );
		}

	/* Create the envelope, push in the data, pop the enveloped result, and
	   destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		{
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_COMPRESSION,
								CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		free( buffer );
		free( envelopedBuffer );
		fprintf( outputStream, "Attempt to enable compression failed, "
				 "status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, 
						   dataCount );
		}
	count = pushData( cryptEnvelope, buffer, dataCount, NULL, 0 );
	if( cryptStatusError( count ) )
		{
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}
	count = popData( cryptEnvelope, envelopedBuffer, FILEBUFFER_SIZE );
	if( count > dataCount - 1000 )
		{
		fprintf( outputStream, "Compression of data failed, %d bytes in "
				 "-> %d bytes out, line %d.\n", dataCount, count, 
				 __LINE__ );
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}
	if( cryptStatusError( count ) || \
		!destroyEnvelope( cryptEnvelope ) )
		{
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, envelopedBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	count = envelopeDecompress( envelopedBuffer, FILEBUFFER_SIZE, count );
	if( count <= 0 )
		{
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}
	if( !compareData( buffer, dataCount, envelopedBuffer, count ) )
		{
		free( buffer );
		free( envelopedBuffer );
		return( FALSE );
		}

	/* Clean up */
	free( buffer );
	free( envelopedBuffer );
	fputs( "Enveloping of compressed data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeCompress( void )
	{
	/* In practice these two produce identical output since we always have to
	   use the indefinite-length encoding internally because we don't know in
	   advance how large the compressed data will be */
	if( !envelopeCompress( "env_cprn", FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeCompress( "env_cpr", TRUE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	return( envelopeCompress( "env_cpr.pgp", TRUE, CRYPT_FORMAT_PGP ) );
	}						/* PGP format */

/****************************************************************************
*																			*
*						Encrypted Enveloping Routines 						*
*																			*
****************************************************************************/

/* Test encrypted enveloping with a raw session key */

static int envelopeSessionCrypt( const char *dumpFileName,
								 const BOOLEAN useDatasize,
								 const BOOLEAN useStreamCipher,
								 const DATA_AMOUNT_TYPE dataAmountType,
								 const int dataAmount,
								 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext;
	CRYPT_ALGO_TYPE cryptAlgo = ( formatType == CRYPT_FORMAT_PGP ) ? \
								selectCipher( CRYPT_ALGO_IDEA ) : \
								selectCipher( CRYPT_ALGO_AES );
	BYTE *inBufPtr, *outBufPtr;
	int length, bufSize, count, status;

	switch( dataAmountType )
		{
		case DATA_AMOUNT_SMALL:
			fprintf( outputStream, "Testing %sraw-session-key encrypted "
					 "enveloping%s...\n",
					 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
					 ( useStreamCipher ) ? " with stream cipher" : \
					 ( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
					 " with datasize hint" : "" );
			inBufPtr = ENVELOPE_TESTDATA;
			length = ENVELOPE_TESTDATA_SIZE;
			bufSize = length + 128;
			outBufPtr = globalBuffer;
			break;

		case DATA_AMOUNT_LARGE:
			fprintf( outputStream, "Testing %sraw-session-key encrypted "
					 "enveloping of large data quantity...\n",
					 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );
			length = 1048576L;
			bufSize = length + 128;

			/* Allocate a large buffer and fill it with a known value */
			if( ( inBufPtr = malloc( bufSize ) ) == NULL )
				{
				fprintf( outputStream, "Couldn't allocate buffer of %d bytes, "
						 "skipping large buffer enveloping test.\n\n", length );
				return( TRUE );
				}
			outBufPtr = inBufPtr;
			initBufferContents( inBufPtr, length );
			break;

		case DATA_AMOUNT_VARIABLE:
			length = dataAmount;
			bufSize = length + 128;
			inBufPtr = globalBuffer;
			outBufPtr = inBufPtr;
			initBufferContents( inBufPtr, length );
			break;

		default:
			return( FALSE );
		}

	if( formatType != CRYPT_FORMAT_PGP )
		{
		/* Create the session key context.  We don't check for errors here
		   since this code will already have been tested earlier */
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
									 cryptAlgo );
		if( cryptStatusError( status ) )
			return( FALSE );
		}
	else
		{
		/* PGP only allows a limited subset of algorithms and modes, in
		   addition we have to specifically check that IDEA is available
		   since it's possible to build cryptlib without IDEA support */
		if( cryptAlgo != CRYPT_ALGO_IDEA )
			{
			fputs( "Can't test PGP enveloping because the IDEA algorithm "
				   "isn't available in this\nbuild of cryptlib.\n\n", 
				   outputStream );
			return( TRUE );
			}
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
									 cryptAlgo );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptSetAttribute( cryptContext, CRYPT_CTXINFO_MODE, 
						   CRYPT_MODE_CFB );
		}
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
							 "0123456789ABCDEF", 16 );

	/* Create the envelope, push in a password and the data, pop the
	   enveloped result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) || \
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SESSIONKEY,
							cryptContext ) )
		return( FALSE );
	if( useDatasize && dataAmountType != DATA_AMOUNT_LARGE )
		{
		/* Test the ability to destroy the context after it's been added
		   (we replace it with a different context that's used later for
		   de-enveloping) */
		cryptDestroyContext( cryptContext );
		status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
									 cryptAlgo );
		if( cryptStatusError( status ) )
			return( FALSE );
		cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
								 "0123456789ABCDEF", 16 );

		/* As a side-effect, use the new context to test the rejection of 
		   addition of a second session key */
		if( addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SESSIONKEY,
							   cryptContext ) )
			{
			fprintf( outputStream, "Addition of duplicate session key "
					 "succeeded when it should have failed,\nline %d.\n", 
					 __LINE__ );
			return( FALSE );
			}
		fputs( "  (The above message indicates that the test condition was "
			   "successfully\n   checked).\n", outputStream );
		}
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, 
						   length );
		}
	if( dataAmountType == DATA_AMOUNT_LARGE )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
		}
	count = pushData( cryptEnvelope, inBufPtr, length, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	if( dumpFileName != NULL && dataAmountType != DATA_AMOUNT_LARGE )
		debugDump( dumpFileName, outBufPtr, count );

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( dataAmountType == DATA_AMOUNT_LARGE )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
		}
	count = pushData( cryptEnvelope, outBufPtr, count, NULL, cryptContext );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Make sure that the result matches what we pushed */
	if( count != length )
		{
		fputs( "De-enveloped data length != original length.\n", 
			   outputStream );
		return( FALSE );
		}
	if( dataAmountType != DATA_AMOUNT_SMALL )
		{
		if( !verifyBufferContents( outBufPtr, length, 0 ) )
			return( FALSE );
		}
	else
		{
		if( !compareData( ENVELOPE_TESTDATA, length, outBufPtr, length ) )
			return( FALSE );
		}

	/* Clean up */
	if( dataAmountType == DATA_AMOUNT_LARGE )
		free( inBufPtr );
	cryptDestroyContext( cryptContext );
	fputs( "Enveloping of raw-session-key-encrypted data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopeSessionCrypt( void )
	{
	if( !envelopeSessionCrypt( "env_sesn", FALSE, FALSE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeSessionCrypt( "env_ses", TRUE, FALSE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	if( !envelopeSessionCrypt( "env_ses", TRUE, TRUE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, stream cipher */
#if 0
	/* Although in theory PGP supports raw session-key based enveloping, in
	   practice this key is always (implicitly) derived from a user password,
	   so the enveloping code doesn't allow the use of raw session keys */
	return( envelopeSessionCrypt( "env_ses.pgp", TRUE, FALSE, DATA_AMOUNT_SMALL, 0, CRYPT_FORMAT_PGP ) );
#endif /* 0 */
	return( TRUE );
	}

int testEnvelopeSessionCryptLargeBuffer( void )
	{
	return( envelopeSessionCrypt( "env_ses", TRUE, FALSE, DATA_AMOUNT_LARGE, 0, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Datasize, large buffer */

int testEnvelopeSessionCryptVariable( void )
	{
	FILE *origOutputStream = outputStream;
	const int maxCount = min( BUFFER_SIZE, 8192 );
	int i;				 /* Maximum is 32600, from size of internal envelope buffer */

	fprintf( outputStream, "Testing encrypted enveloping quantities from "
			 "1 to %d bytes to check for\n  boundary errors...\n", maxCount );

	outputStream = fopen( DEVNULL, "w" );
	assert( outputStream != NULL );
	for( i = 1; i < maxCount; i++ )
		{
		fprintf( origOutputStream, "%d.\r", i );
		if( !envelopeSessionCrypt( NULL, FALSE, FALSE, DATA_AMOUNT_VARIABLE, i, CRYPT_FORMAT_CRYPTLIB ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "Failed for indefinite-length encrypted enveloping "
					 "size %d.\n", i );
			return( FALSE );	/* Indefinite length */
			}
		if( !envelopeSessionCrypt( NULL, TRUE, FALSE, DATA_AMOUNT_VARIABLE, i, CRYPT_FORMAT_CRYPTLIB ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "Failed for definite-length encrypted enveloping "
					 "size %d.\n", i );
			return( FALSE );	/* Datasize */
			}
		}
	outputStream = origOutputStream;

	fprintf( outputStream, "Boundary error test succeeded.\n\n" );
	return( TRUE );
	}

int testEnvelopeSessionCryptMultiple( void )
	{
	int i;

	fprintf( outputStream, "Testing encrypted data enveloping of multiple "
			 "blocks of data...\n" );
	for( i = 1; i < 8192; i++ )
		{
		fprintf( outputStream, "%d.\r", i );
		if( !envelopeDataMultiple( i, TRUE ) )
			return( FALSE );
		}
	fprintf( outputStream, "Enveloping of multiple blocks of encrypted "
			 "data succeeded.\n\n" );
	return( TRUE );
	}

/* Test encrypted enveloping */

static int envelopeDecrypt( BYTE *buffer, const int length,
							const CRYPT_CONTEXT cryptContext )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count;

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	count = pushData( cryptEnvelope, buffer, length, NULL, cryptContext );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, buffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopeCrypt( const char *dumpFileName,
						  const BOOLEAN useDatasize,
						  const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	fprintf( outputStream, "Testing encrypted enveloping%s...\n",
			 useDatasize ? " with datasize hint" : "" );

	/* Create the session key context.  We don't check for errors here
	   since this code will already have been tested earlier */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 DEFAULT_CRYPT_ALGO );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
							 "0123456789ABCDEF", 16 );

	/* Create the envelope, push in a KEK and the data, pop the enveloped
	   result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) || \
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEY, 
							cryptContext ) )
		{
		return( FALSE );
		}
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   ENVELOPE_TESTDATA_SIZE );
		}
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	count = envelopeDecrypt( globalBuffer, count, cryptContext );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );

	/* Clean up */
	cryptDestroyContext( cryptContext );
	fputs( "Enveloping of encrypted data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeCrypt( void )
	{
	if( !envelopeCrypt( "env_kekn", FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length */
	return( envelopeCrypt( "env_kek", TRUE, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Datasize */

/* Test password-based encrypted enveloping */

static int envelopePasswordDecrypt( BYTE *buffer, const int length )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count;

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	count = pushData( cryptEnvelope, buffer, length, TEST_PASSWORD,
					  paramStrlen( TEST_PASSWORD ) );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, buffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopePasswordCrypt( const char *dumpFileName,
								  const BOOLEAN useDatasize,
								  const BOOLEAN useAltCipher,
								  const BOOLEAN multiKeys,
								  const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count;

	fprintf( outputStream, "Testing %s%spassword-encrypted enveloping%s",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
			 multiKeys ? "multiple-" : "",
			 ( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
			 " with datasize hint" : "" );
	if( useAltCipher )
		{
		fprintf( outputStream, ( formatType == CRYPT_FORMAT_PGP ) ? \
				 " with non-default cipher type" : " and stream cipher" );
		}
	fputs( "...\n", outputStream );

	/* Create the envelope, push in a password and the data, pop the
	   enveloped result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( useAltCipher )
		{
		int status;

		if( formatType == CRYPT_FORMAT_PGP ) 
			{
			status = cryptSetAttribute( cryptEnvelope, 
										CRYPT_OPTION_ENCR_ALGO, 
										CRYPT_ALGO_AES );
			}
		else
			{
			CRYPT_CONTEXT sessionKeyContext;

			/* Test enveloping with an IV-less stream cipher, which tests 
			   the handling of algorithms that can't be used to wrap 
			   themselves in the RecipientInfo */
			status = cryptCreateContext( &sessionKeyContext, CRYPT_UNUSED,
										 CRYPT_ALGO_RC4 );
			if( status == CRYPT_ERROR_NOTAVAIL )
				{
				fputs( "Warning: Couldn't set non-default envelope cipher "
					   "RC4, this may be disabled\n         in this build of "
					   "cryptlib.\n\n", outputStream );
				if( !destroyEnvelope( cryptEnvelope ) )
					return( FALSE );
				return( TRUE );
				}
			if( cryptStatusOK( status ) )
				status = cryptGenerateKey( sessionKeyContext );
			if( cryptStatusOK( status ) )
				{
				status = cryptSetAttribute( cryptEnvelope,
											CRYPT_ENVINFO_SESSIONKEY,
											sessionKeyContext );
				cryptDestroyContext( sessionKeyContext );
				}
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Couldn't set non-default envelope "
					 "cipher, error code %d, line %d.\n", status, 
					 __LINE__ );
			return( FALSE );
			}
		}
	if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
						   TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) ) )
		return( FALSE );
	if( multiKeys )
		{
		if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEXT( "Password1" ),
							   paramStrlen( TEXT( "Password1" ) ) ) || \
			!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEXT( "Password2" ),
							   paramStrlen( TEXT( "Password2" ) ) ) || \
			!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEXT( "Password3" ),
							   paramStrlen( TEXT( "Password3" ) ) ) )
			{
			return( FALSE );
			}
		}
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   ENVELOPE_TESTDATA_SIZE );
		}
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", 
			 count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	count = envelopePasswordDecrypt( globalBuffer, count );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of password-encrypted data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopePasswordCrypt( void )
	{
	if( !envelopePasswordCrypt( "env_pasn", FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopePasswordCrypt( "env_pas", TRUE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	if( !envelopePasswordCrypt( "env_mpas", TRUE, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, multiple keys */
	if( !envelopePasswordCrypt( "env_pas.pgp", TRUE, FALSE, FALSE, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format */
	if( !envelopePasswordCrypt( "env_pasa.pgp", TRUE, TRUE, FALSE, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format */
	return( envelopePasswordCrypt( "env_pasr", TRUE, TRUE, FALSE, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* IV-less cipher */

/* Test boundary conditions for encrypted enveloping.  The strategies are:

	All: Push data up to the EOC, then:

	Strategy 0: Pop exact data length.
	Strategy 1: Pop as much as possible.

   For encrypted data, 8 bytes should be left un-popped since the enveloping
   code hasn't hit the EOCs yet and so can't know how many bytes at the end
   are payload vs. padding */

static int envelopeBoundaryTest( const BOOLEAN usePassword,
								 const int strategy )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BYTE encBuffer[ 1024 ];
	int eocPos = 34;
	int count, status;

	fprintf( outputStream, "Testing %senveloping boundary condition "
			 "handling, strategy %d...\n", 
			 usePassword ? "encrypted " : "", strategy );

	/* If we're using password-based enveloping then the EOC position can
	   vary depending on the hash algorithm used */
	if( usePassword )
		{
		int hashAlgo;

		status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_KEYING_ALGO, 
									&hashAlgo );
		if( cryptStatusError( status ) )
			return( FALSE );
		eocPos = ( hashAlgo == CRYPT_ALGO_SHA1 ) ? 182 : 216;
		}

	/* Create an envelope and envelope some data using indefinite-length
	   encoding */
	if( !createEnvelope( &cryptEnvelope, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );
	if( usePassword )
		{
		if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) ) )
			return( FALSE );
		}
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, encBuffer, 1024 );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( strategy == 0 )	/* Different strategies produce the same output */
		debugDump( usePassword ? "env_ovfe" : "env_ovfd", encBuffer, count );

	/* Make sure the EOCs are at the expected position */
	if( usePassword )
		{
		assert( ( encBuffer[ eocPos - 1 ] != 0 || \
				  encBuffer[ eocPos - 2 ] != 0 || \
				  encBuffer[ eocPos - 3 ] != 0 ) && \
				encBuffer[ eocPos ] == 0 && encBuffer[ eocPos + 1 ] == 0 );
		}
	else
		{
		assert( encBuffer[ eocPos - 2 ] == \
							ENVELOPE_TESTDATA[ ENVELOPE_TESTDATA_SIZE - 2 ] && \
				encBuffer[ eocPos - 1 ] == \
							ENVELOPE_TESTDATA[ ENVELOPE_TESTDATA_SIZE - 1 ] && \
				encBuffer[ eocPos ] == 0 && encBuffer[ eocPos + 1 ] == 0 );
				/* [ -2 ] = last char, [ -1 ] = '\0' */
		}

	/* De-envelope the data.  We can't use envelopePasswordDecrypt() for this
	   because we need to perform custom data handling to test the boundary
	   conditions.  First we push the data up to the EOC */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	status = cryptPushData( cryptEnvelope, encBuffer, eocPos, &count );
	if( cryptStatusError( status ) )
		{
		if( !usePassword || status != CRYPT_ENVELOPE_RESOURCE )
			{
			printExtError( cryptEnvelope, "cryptPushData()", status, __LINE__ );
			return( status );
			}
		if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) ) )
			return( FALSE );
		}

	/* Then we pop data either up to the EOC or as much as we can */
	if( strategy == 0 )
		{
		/* Pop the exact data length */
		status = cryptPopData( cryptEnvelope, globalBuffer, 
							   ENVELOPE_TESTDATA_SIZE, &count );
		}
	else
		{
		/* Pop as much as we can get */
		status = cryptPopData( cryptEnvelope, globalBuffer, 
							   BUFFER_SIZE, &count );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Error popping data before final-EOC push, "
				 "error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( usePassword )
		{
		/* If it's password-encrypted, there are five EOCs at the end of the 
		   data */
		status = cryptPushData( cryptEnvelope, encBuffer + eocPos, 10, 
								&count );
		}
	else
		{
		/* If it's plain data, there are three EOCs at the end of the data */
		status = cryptPushData( cryptEnvelope, encBuffer + eocPos, 6, 
								&count );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Error pushing final EOCs, error code %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Finally we pop the remaining data.  Due to the internal envelope
	   buffering strategy the remaining data can be either the cipher
	   blocksize or ENVELOPE_TESTDATA_SIZE - blocksize if we're working with
	   encrypted data */
	status = cryptPopData( cryptEnvelope, &globalBuffer, BUFFER_SIZE, 
						   &count );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Error popping data after final-EOC push, "
				 "error code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( usePassword )
		{
		int blockSize;

		status = cryptGetAttribute( cryptEnvelope, CRYPT_CTXINFO_BLOCKSIZE, 
									&blockSize );
		if( cryptStatusError( status ) )
			return( FALSE );
		if( count != ( ENVELOPE_TESTDATA_SIZE % blockSize ) && \
		    count != ENVELOPE_TESTDATA_SIZE - \
							( ENVELOPE_TESTDATA_SIZE % blockSize ) )
			{
			fprintf( outputStream, "Final data count should have been %d, "
					 "was %d, line %d.\n", 
					 ENVELOPE_TESTDATA_SIZE % blockSize, count, __LINE__ );
			return( FALSE );
			}
		}
	else
		{
		if( count != 0 )
			{
			fprintf( outputStream, "Final data count should have been 0, "
					 "was %d, line %d.\n", count, __LINE__ );
			return( FALSE );
			}
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping boundary-condition handling succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopePasswordCryptBoundary( void )
	{
	if( !envelopeBoundaryTest( FALSE, 0 ) )
		return( FALSE );
	if( !envelopeBoundaryTest( FALSE, 1 ) )
		return( FALSE );
	if( !envelopeBoundaryTest( TRUE, 0 ) )
		return( FALSE );
	return( envelopeBoundaryTest( TRUE, 1 ) );
	}

/* Test PKC-encrypted enveloping */

static int envelopePKCDecrypt( BYTE *buffer, const int length,
							   const KEYFILE_TYPE keyFileType,
							   const CRYPT_HANDLE externalCryptKeyset )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	assert( ( keyFileType != KEYFILE_NONE && \
			  externalCryptKeyset == CRYPT_UNUSED ) || \
			( keyFileType == KEYFILE_NONE && \
			  externalCryptKeyset != CRYPT_UNUSED ) );

	/* Create the envelope and push in the decryption keyset */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( keyFileType != KEYFILE_NONE )
		{
		CRYPT_KEYSET cryptKeyset;
		const C_STR keysetName = getKeyfileName( keyFileType, TRUE );

		/* Add the decryption keyset */
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, 
								  CRYPT_KEYSET_FILE, keysetName, 
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusError( status ) )
			{
			destroyEnvelope( cryptEnvelope );
			return( FALSE );
			}
		status = addEnvInfoNumeric( cryptEnvelope, 
									CRYPT_ENVINFO_KEYSET_DECRYPT, 
									cryptKeyset );
		cryptKeysetClose( cryptKeyset );
		}
	else
		{
		/* We're using a device to handle decryption keys */
		status = addEnvInfoNumeric( cryptEnvelope, 
									CRYPT_ENVINFO_KEYSET_DECRYPT, 
									externalCryptKeyset );
		}
	if( status <= 0 )
		{
		destroyEnvelope( cryptEnvelope );
		return( FALSE );
		}

	/* Push in the data */
	if( keyFileType != KEYFILE_NONE )
		{
		const C_STR password = getKeyfilePassword( keyFileType );

		count = pushData( cryptEnvelope, buffer, length, password, 
						  paramStrlen( password ) );
		}
	else
		{
		count = pushData( cryptEnvelope, buffer, length, NULL, 0 );
		}
	if( cryptStatusError( count ) )
		{
		destroyEnvelope( cryptEnvelope );
		return( FALSE );
		}
	count = popData( cryptEnvelope, buffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		{
		destroyEnvelope( cryptEnvelope );
		return( FALSE );
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopePKCDecryptDirect( BYTE *buffer, const int length,
									 const KEYFILE_TYPE keyFileType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext;
	int count, status;

	/* Create the envelope and get the decryption key */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	status = getPrivateKey( &cryptContext,
							getKeyfileName( keyFileType, TRUE ),
							getKeyfileUserID( keyFileType, TRUE ),
							getKeyfilePassword( keyFileType ) );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Push in the data */
	count = pushData( cryptEnvelope, buffer, length, NULL, cryptContext );
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, buffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopePKCCrypt( const char *dumpFileName,
							 const BOOLEAN useDatasize,
							 const KEYFILE_TYPE keyFileType,
							 const BOOLEAN useRecipient,
							 const BOOLEAN useMultipleKeyex,
							 const BOOLEAN useAltAlgo,
							 const BOOLEAN useDirectKey,
							 const CRYPT_FORMAT_TYPE formatType,
							 const CRYPT_CONTEXT externalCryptContext,
							 const CRYPT_HANDLE externalCryptKeyset )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_KEYSET cryptKeyset;
	CRYPT_HANDLE cryptKey;
	const C_STR keysetName = TEXT( "<None>" );
	const C_STR keyID = TEXT( "<None>" );
	int count, status;

	assert( ( keyFileType != KEYFILE_NONE && \
			  externalCryptContext == CRYPT_UNUSED && \
			  externalCryptKeyset == CRYPT_UNUSED ) || \
			( keyFileType == KEYFILE_NONE && \
			  externalCryptContext != CRYPT_UNUSED && \
			  externalCryptKeyset != CRYPT_UNUSED ) );

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of public-key "
			   "encrypted enveloping...\n\n", outputStream );
		return( TRUE );
		}
	if( keyFileType != KEYFILE_NONE )
		{
		/* When reading keys we have to explicitly use the first matching
		   key in the PGP 2.x keyring since the remaining keys are (for some
		   reason) stored unencrypted, and the keyring read code will
		   disallow the use of the key if it's stored in this manner */
		keysetName = getKeyfileName( keyFileType, FALSE );
		keyID = ( keyFileType == KEYFILE_PGP ) ? \
				TEXT( "test" ) : getKeyfileUserID( keyFileType, FALSE );
		}
	fprintf( outputStream, "Testing %spublic-key encrypted enveloping",
			 ( formatType == CRYPT_FORMAT_PGP ) ? \
				( ( keyFileType == KEYFILE_PGP ) ? "PGP " : "OpenPGP " ) : "" );
	if( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) && \
		!( useRecipient || useMultipleKeyex || useDirectKey ) )
		{
		fprintf( outputStream, " with datasize hint" );
		}
	fprintf( outputStream, " using " );
	fprintf( outputStream, ( keyFileType == KEYFILE_PGP ) ? \
				( ( formatType == CRYPT_FORMAT_PGP ) ? \
					"PGP key" : "raw public key" ) : \
			  "X.509 cert" );
	if( useRecipient && !useAltAlgo )
		fprintf( outputStream, " and recipient info" );
	if( useMultipleKeyex )
		fprintf( outputStream, " and additional keying info" );
	if( useAltAlgo )
		fprintf( outputStream, " and alt.encr.algo" );
	if( useDirectKey )
		fprintf( outputStream, " and direct key add" );
	fputs( "...\n", outputStream );

	/* Open the keyset and either get the public key explicitly (to make sure
	   that this version works) or leave the keyset open to allow it to be
	   added to the envelope */
	if( keyFileType != KEYFILE_NONE )
		{
		if( useRecipient )
			{
			status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, 
									  CRYPT_KEYSET_FILE, keysetName, 
									  CRYPT_KEYOPT_READONLY );
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "Couldn't open keyset '%s', "
						 "status %d, line %d.\n", keysetName, status, 
						 __LINE__ );
				return( FALSE );
				}
			}
		else
			{
			status = getPublicKey( &cryptKey, keysetName, keyID );
			if( cryptStatusError( status ) )
				return( FALSE );
			}
		}
	else
		{
		cryptKey = externalCryptContext;
		cryptKeyset = externalCryptKeyset;
		}

	/* Create the envelope, push in the recipient info or public key and data,
	   pop the enveloped result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( useAltAlgo )
		{
		/* Specify the use of an alternative (non-default) bulk encryption
		   algorithm */
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_OPTION_ENCR_ALGO,
								CRYPT_ALGO_3DES ) )
			return( FALSE );
		}
	if( useRecipient )
		{
		/* Add recipient information to the envelope.  Since we can't
		   guarantee for enveloping with cryptlib native key types that we
		   have a real public-key keyset available at this time (it's created
		   by a different part of the self-test code that may not have run
		   yet) we're actually reading the public key from the private-key
		   keyset.  Normally we couldn't do this, however since PKCS #15
		   doesn't store email addresses as key ID's (there's no need to),
		   the code will drop back to trying for a match on the key label.
		   Because of this we specify the private key label instead of a real
		   recipient email address.  Note that this trick only works because
		   of a coincidence of two or three factors and wouldn't normally be
		   used, it's only used here because we can't assume that a real
		   public-key keyset is available for use.

		   An additional test that would be useful is the ability to handle
		   multiple key exchange records, however the keyset kludge makes
		   this rather difficult.  Since the functionality is tested by the
		   use of multiple passwords in the conventional-encryption test
		   earlier on this isn't a major issue */
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEYSET_ENCRYPT,
								cryptKeyset ) || \
			!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_RECIPIENT,
							   keyID, paramStrlen( keyID ) ) )
			{
			return( FALSE );
			}
		cryptKeysetClose( cryptKeyset );
		}
	else
		{
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
								cryptKey ) )
			return( FALSE );

		/* Test the ability to detect the addition of a duplicate key */
		if( !( useRecipient || useMultipleKeyex || useDirectKey ) )
			{
			if( addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
								   cryptKey ) )
				{
				fprintf( outputStream, "Addition of duplicate key "
						 "succeeded when it should have failed,\n  "
						 "line %d.\n", __LINE__ );
				return( FALSE );
				}
			fputs( "  (The above message indicates that the test condition "
				   "was successfully\n   checked).\n", outputStream );
			}

		if( keyFileType != KEYFILE_NONE )
			cryptDestroyObject( cryptKey );
		}
	if( useMultipleKeyex && \
		!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
						   TEXT( "test" ), paramStrlen( TEXT( "test" ) ) ) )
		return( FALSE );
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   ENVELOPE_TESTDATA_SIZE );
		}
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	if( useDirectKey )
		count = envelopePKCDecryptDirect( globalBuffer, count, keyFileType );
	else
		{
		count = envelopePKCDecrypt( globalBuffer, count, keyFileType, 
									externalCryptKeyset );
		}
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of public-key encrypted data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopePKCCrypt( void )
	{
#ifndef USE_PGP2
	fputs( "Skipping raw public-key and PGP enveloping, which requires PGP "
		   "2.x support to\n  be enabled.\n\n", outputStream );
#else
	if( cryptQueryCapability( CRYPT_ALGO_IDEA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		fputs( "Skipping raw public-key and PGP enveloping, which requires "
			   "the IDEA cipher to\nbe enabled.\n\n", outputStream );
		}
	else
		{
		if( !envelopePKCCrypt( "env_pkcn", FALSE, KEYFILE_PGP, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* Indefinite length, raw key */
		if( !envelopePKCCrypt( "env_pkc", TRUE, KEYFILE_PGP, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* Datasize, raw key */
		if( !envelopePKCCrypt( "env_pkc.pgp", TRUE, KEYFILE_PGP, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* PGP format */
		if( !envelopePKCCrypt( "env_pkc.pgp", TRUE, KEYFILE_PGP, TRUE, FALSE, FALSE, FALSE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* PGP format, recipient */
		if( !envelopePKCCrypt( "env_pkc.pgp", TRUE, KEYFILE_PGP, FALSE, FALSE, FALSE, TRUE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* PGP format, decrypt key provided directly */
#ifdef USE_3DES
		if( !envelopePKCCrypt( "env_pkca.pgp", TRUE, KEYFILE_PGP, TRUE, FALSE, TRUE, FALSE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* PGP format, recipient, nonstandard bulk encr.algo */
#endif /* USE_3DES */
		}
#endif /* USE_PGP2 */
	if( !envelopePKCCrypt( "env_crt.pgp", TRUE, KEYFILE_X509, TRUE, FALSE, FALSE, FALSE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
		return( FALSE );	/* PGP format, certificate */
	if( !envelopePKCCrypt( "env_crtn", FALSE, KEYFILE_X509, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
		return( FALSE );	/* Indefinite length, certificate */
	if( !envelopePKCCrypt( "env_crt", TRUE, KEYFILE_X509, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
		return( FALSE );	/* Datasize, certificate */
	if( !envelopePKCCrypt( "env_crt", TRUE, KEYFILE_X509, FALSE, FALSE, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
		return( FALSE );	/* Datasize, certificate, decrypt key provided directly */
	if( !envelopePKCCrypt( "env_crt", TRUE, KEYFILE_X509, TRUE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) )
		return( FALSE );	/* Datasize, cerficate, recipient */
	return( envelopePKCCrypt( "env_crtp", TRUE, KEYFILE_X509, FALSE, TRUE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, CRYPT_UNUSED, CRYPT_UNUSED ) );
	}						/* Datasize, cerficate+password */

int testEnvelopePKCCryptEx( const CRYPT_CONTEXT cryptContext, 
							const CRYPT_HANDLE decryptKeyset )
	{
	/* Note that we can't test PGP enveloping with device-based keys since 
	   the PGP keyIDs aren't supported by any known device type */
#if 0
	if( !envelopePKCCrypt( "env_pkc.pgp", TRUE, KEYFILE_NONE, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_PGP, cryptContext, decryptKeyset ) )
		return( FALSE );	/* PGP format */
#endif
	return( envelopePKCCrypt( "env_pkc", TRUE, KEYFILE_NONE, FALSE, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB, cryptContext, decryptKeyset ) );
	}						/* Datasize, raw key */

/* Perform an iterated envelope/deenvelope test to catch errors that only 
   occur in a small subset of runs.  This is used to check for problems in 
   data-dependant operations, e.g. leading-zero truncation which only crops 
   up in 1/256 runs */

#define NO_ITERS	1000

static int envelopePKCIterated( const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_CONTEXT cryptPubKey, cryptPrivKey;
	FILE *origOutputStream = outputStream;
	int i, status;

	fprintf( outputStream, "Testing %d iterations of %s enveloping to "
			 "check for pad/trunc.errors...\n", NO_ITERS,
			( formatType == CRYPT_FORMAT_CRYPTLIB ) ? "cryptlib" : "PGP" );
	status = getPublicKey( &cryptPubKey, 
						   getKeyfileName( KEYFILE_X509, FALSE ), 
						   getKeyfileUserID( KEYFILE_X509, FALSE ) );
	assert( cryptStatusOK( status ) );
	status = getPrivateKey( &cryptPrivKey, 
							getKeyfileName( KEYFILE_X509, FALSE ), 
							getKeyfileUserID( KEYFILE_X509, FALSE ),
							getKeyfilePassword( KEYFILE_X509 ) );
	assert( cryptStatusOK( status ) );
	outputStream = fopen( DEVNULL, "w" );
	assert( outputStream != NULL );
	for( i = 0; i < NO_ITERS; i++ )
		{
		CRYPT_ENVELOPE cryptEnvelope;
		int count;

		fprintf( origOutputStream, "%d.\r", i );

		/* Envelope the data */
		status = createEnvelope( &cryptEnvelope, formatType );
		assert( status == TRUE );
		status = addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY, 
									cryptPubKey );
		assert( status == TRUE );
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   ENVELOPE_TESTDATA_SIZE );
		count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
						  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
		assert( !cryptStatusError( count ) );
		count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
		assert( !cryptStatusError( count ) );
		destroyEnvelope( cryptEnvelope );

		/* De-envelope the data */
		status = createDeenvelope( &cryptEnvelope );
		assert( status == TRUE );
		status = pushData( cryptEnvelope, globalBuffer, count, NULL, 
						   cryptPrivKey );
		if( cryptStatusError( status ) )
			{
			outputStream = origOutputStream;
			fprintf( outputStream, "\nFailed, iteration = %d.\n", i );
			debugDump( "r:/out.der", globalBuffer, count );
			destroyEnvelope( cryptEnvelope );
			return( FALSE );
			}
		count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
		assert( !cryptStatusError( count ) );
		destroyEnvelope( cryptEnvelope );
		}
	outputStream = origOutputStream;
	cryptDestroyContext( cryptPubKey );
	cryptDestroyContext( cryptPrivKey );
	
	fprintf( outputStream, "Tested %d iterations of %s PKC enveloping.\n\n", 
			 NO_ITERS, 
			 ( formatType == CRYPT_FORMAT_CRYPTLIB ) ? "cryptlib" : "PGP" );
	return( TRUE );
	}

int testEnvelopePKCIterated( void )
	{
	if( !envelopePKCIterated( CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );
	return( envelopePKCIterated( CRYPT_FORMAT_PGP ) );
	}

/* Test each encryption algorithm */

static int envelopeAlgoCrypt( const char *dumpFileName,
							  const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_HANDLE encryptKey, decryptKey;
	const char *algoName = \
			( cryptAlgo == CRYPT_ALGO_RSA ) ? "RSA" : \
			( cryptAlgo == CRYPT_ALGO_ELGAMAL ) ? "Elgamal" : "<Unknown>";
	int count;

	fprintf( outputStream, "Testing %s public-key encrypted "
			 "enveloping...\n", algoName );

	/* Create the en/decryption contexts */
	switch( cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
			if( !loadRSAContexts( CRYPT_UNUSED, &encryptKey, &decryptKey ) )
				return( FALSE );
			break;

		case CRYPT_ALGO_ELGAMAL:
			if( !loadElgamalContexts( &encryptKey, &decryptKey ) )
				return( FALSE );
			break;

		default:
			fprintf( outputStream, "Unknown encryption algorithm %d, "
					 "line %d.\n", cryptAlgo, __LINE__ );
			return( FALSE );
		}

	/* Create the envelope, push in the public key and data, pop the 
	   enveloped result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
							encryptKey ) )
		return( FALSE );
	cryptDestroyContext( encryptKey );
	cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
					   ENVELOPE_TESTDATA_SIZE );
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "%s-enveloped data has size %d bytes.\n", 
			 algoName, count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	count = pushData( cryptEnvelope, globalBuffer, count, NULL, 
					  decryptKey );
	cryptDestroyContext( decryptKey );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of public-key encrypted data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopePKCCryptAlgo( void )
	{
	if( !envelopeAlgoCrypt( "env_pkc_rsa", CRYPT_ALGO_RSA ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ELGAMAL, NULL ) ) && \
		!envelopeAlgoCrypt( "env_pkc_elg", CRYPT_ALGO_ELGAMAL ) )
		return( FALSE );
	return( TRUE );
	}

/* Test PKC-encrypted enveloping with multiple recipients */

static int envelopePKCCryptMulti( const char *dumpFileName,
								  const int recipientNo,
								  const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_HANDLE cryptKey1, cryptKey2;
	int count, status;

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of public-key "
			   "encrypted enveloping...\n\n", outputStream );
		return( TRUE );
		}
	fprintf( outputStream, "Testing %spublic-key encrypted enveloping with "
			 "multiple recipients,\n",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );
	fprintf( outputStream, "  recipient %d to decrypt", recipientNo );
	fputs( "...\n", outputStream );

	/* Open the keysets and get the two public keys */
	status = getPublicKey( &cryptKey1, 
						   getKeyfileName( KEYFILE_X509, FALSE ), 
						   getKeyfileUserID( KEYFILE_X509, FALSE ) );
	if( cryptStatusOK( status ) )
		{
		status = getPublicKey( &cryptKey2, 
							   getKeyfileName( KEYFILE_X509_ALT, FALSE ), 
							   getKeyfileUserID( KEYFILE_X509_ALT, FALSE ) );
		}
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Create the envelope, push in the keys and data, pop the enveloped 
	   result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
							cryptKey1 ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
							cryptKey2 ) )
		return( FALSE );
	cryptDestroyObject( cryptKey1 );
	cryptDestroyObject( cryptKey2 );
	cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
					   ENVELOPE_TESTDATA_SIZE );
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	count = envelopePKCDecryptDirect( globalBuffer, count, 
									  ( recipientNo == 1 ) ? KEYFILE_X509 : \
															 KEYFILE_X509_ALT );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of public-key encrypted data with multiple "
		   "recipients succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopePKCCryptMulti( void )
	{
	if( cryptQueryCapability( CRYPT_ALGO_IDEA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		fputs( "Skipping raw public-key and PGP enveloping, which requires "
			   "the IDEA cipher to\nbe enabled.\n\n", outputStream );
		}
	else
		{
		if( !envelopePKCCryptMulti( "env_pkc_mp", 1, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, multiple recip, key #1 decrypts */
		if( !envelopePKCCryptMulti( "env_pkc_mp", 2, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, multiple recip, key #2 decrypts */
		}
	if( !envelopePKCCryptMulti( "env_pkc_m", 1, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Multiple recipients, key #1 decrypts */
	return( envelopePKCCryptMulti( "env_pkc_m", 2, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Multiple recipients, key #2 decrypts */


/****************************************************************************
*																			*
*							Signed Enveloping Routines 						*
*																			*
****************************************************************************/

/* Test signed enveloping */

static int getSigCheckResult( const CRYPT_ENVELOPE cryptEnvelope,
							  const CRYPT_CONTEXT sigCheckContext,
							  const BOOLEAN showAttributes,
							  const BOOLEAN isAuthData )
	{
	int value, status;

	/* Display all of the attributes that we've got */
	if( showAttributes && !displayAttributes( cryptEnvelope ) )
		return( FALSE );

	/* Determine the result of the signature check.  If it's authenticated
	   data there's no need to do anything with keys, if it's signed data
	   we have to do some extra processing */
	if( !isAuthData )
		{
		status = cryptGetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_CURRENT,
									&value );
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Read of required attribute for signature "
					 "check returned status %d, line %d.\n", status, 
					 __LINE__ );
			return( FALSE );
			}
		if( value != CRYPT_ENVINFO_SIGNATURE )
			{
			fprintf( outputStream, "Envelope requires unexpected enveloping "
					 "information type %d, line %d.\n", value, __LINE__ );
			return( FALSE );
			}
		if( sigCheckContext != CRYPT_UNUSED )
			{
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
										sigCheckContext );
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "Attempt to add signature check key "
						 "returned status %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		}
	status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE_RESULT,
								&value );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Signature check returned status %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	switch( value )
		{
		case CRYPT_OK:
			fputs( "Signature is valid.\n", outputStream );
			return( TRUE );

		case CRYPT_ERROR_NOTFOUND:
			fputs( "Cannot find key to check signature.\n", outputStream );
			break;

		case CRYPT_ERROR_SIGNATURE:
			fputs( "Signature is invalid.\n", outputStream );
			break;

		case CRYPT_ERROR_NOTAVAIL:
			fputs( "Warning: Couldn't verify signature due to use of a "
				   "deprecated/insecure\n         signature algorithm.\n", 
				   outputStream );
			return( TRUE );

		default:
			fprintf( outputStream, "Signature check failed, result = %d, "
					 "line %d.\n", value, __LINE__ );
		}

	return( FALSE );
	}

static int envelopeSigCheck( BYTE *buffer, const int length,
							 const CRYPT_CONTEXT hashContext,
							 const CRYPT_CONTEXT sigContext,
							 const KEYFILE_TYPE keyFileType,
							 const BOOLEAN detachedSig,
							 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	/* Create the envelope and push in the sig.check keyset if we're not
	   using a supplied context for the sig.check */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( sigContext == CRYPT_UNUSED )
		{
		const C_STR keysetName = getKeyfileName( keyFileType, FALSE );
		CRYPT_KEYSET cryptKeyset;

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, keysetName,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) )
			{
			status = addEnvInfoNumeric( cryptEnvelope,
										CRYPT_ENVINFO_KEYSET_SIGCHECK, 
										cryptKeyset );
			}
		cryptKeysetClose( cryptKeyset );
		if( status <= 0 )
			return( FALSE );
		}

	/* If the hash value is being supplied externally, add it to the envelope
	   before we add the signature data */
	if( detachedSig && hashContext != CRYPT_UNUSED )
		{
		status = cryptSetAttribute( cryptEnvelope, 
									CRYPT_ENVINFO_DETACHEDSIGNATURE, TRUE );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_HASH,
										hashContext );
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Couldn't add externally-generated hash "
					 "value to envelope, status %d, line %d.\n", status, 
					 __LINE__ );
			return( FALSE );
			}
		}

	/* Push in the data */
	count = pushData( cryptEnvelope, buffer, length, NULL, 0 );
	if( !cryptStatusError( count ) )
		{
		if( detachedSig )
			{
			if( hashContext == CRYPT_UNUSED )
				{
				count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
								  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
				}
			}
		else
			count = popData( cryptEnvelope, buffer, length );
		}
	if( cryptStatusError( count ) )
		{
		cryptDestroyEnvelope( cryptEnvelope );
		return( FALSE );
		}

	/* Determine the result of the signature check */
	if( !getSigCheckResult( cryptEnvelope, sigContext, TRUE, FALSE ) )
		{
		cryptDestroyEnvelope( cryptEnvelope );
		return( FALSE );
		}

	/* If we're testing the automatic upgrade of the envelope hash function
	   to a stronger one, make sure that the hash algorithm was upgraded */
	if( keyFileType == KEYFILE_X509_ALT )
		{
		/* There's no easy way to retrieve the hash algorithm used for 
		   signing from the envelope since it's a low-level internal 
		   attribute, so the only way to verify that the algorithm upgrade 
		   was successful is by running dumpasn1 on the output data file */
		}

	/* If we supplied the sig-checking key, make sure that it's handled
	   correctly by the envelope.  We shouldn't be able to read it back from
	   a PGP envelope, and from a cryptlib/CMS/SMIME envelope we should get
	   back only a certificate, not the full private key that we added */
	if( sigContext != CRYPT_UNUSED )
		{
		CRYPT_CONTEXT sigCheckContext;

		status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
									&sigCheckContext );
		if( formatType == CRYPT_FORMAT_PGP )
			{
			/* If it's a PGP envelope, we can't retrieve the signing key from
			   it */
			if( cryptStatusOK( status ) )
				{
				fprintf( outputStream, "Attempt to read signature check key "
						 "from PGP envelope succeeded when it\ns  hould have "
						 "failed, line %d.\n", __LINE__ );
				return( FALSE );
				}
			}
		else
			{
			CRYPT_ENVELOPE testEnvelope;

			/* If it's a cryptlib/CMS/SMIME envelope then we should be able 
			   to retrieve the signing key from it */
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "Couldn't retrieve signature check "
						 "key from envelope, status %d, line %d.\n", status,
						 __LINE__ );
				return( FALSE );
				}

			/* The signing key should be a pure certificate, not the private 
			   key+certificate combination that we pushed in.  Note that the 
			   following will result in an error message being printed in
			   addEnvInfoNumeric() */
			if( !createEnvelope( &testEnvelope, CRYPT_FORMAT_CRYPTLIB ) )
				return( FALSE );
			if( addEnvInfoNumeric( testEnvelope, CRYPT_ENVINFO_SIGNATURE,
								   sigCheckContext ) )
				{
				fprintf( outputStream, "Retrieved signature check key is a "
						 "private key, not a certificate, line %d.\n", 
						 __LINE__ );
				return( FALSE );
				}
			else
				{
				fputs( "  (The above message indicates that the test "
					   "condition was successfully\n   checked).\n", 
					   outputStream );
				}
			if( !destroyEnvelope( testEnvelope ) )
				return( FALSE );
			cryptDestroyCert( sigCheckContext );
			}
		}

	/* Clean up */
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( count );
	}

static int envelopeSign( const void *data, const int dataLength,
						 const char *dumpFileName, 
						 const KEYFILE_TYPE keyFileType, 
						 const BOOLEAN useDatasize,
						 const BOOLEAN useCustomHash, 
						 const BOOLEAN useSuppliedKey,
						 const CRYPT_CONTENT_TYPE contentType,
						 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CONTEXT cryptContext DUMMY_INIT;
	char filenameBuffer[ FILENAME_BUFFER_SIZE ];
	int count, status;

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of signed "
			   "enveloping...\n\n", outputStream );
		return( TRUE );
		}
	fprintf( outputStream, "Testing %ssigned enveloping%s",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : \
			 ( formatType == CRYPT_FORMAT_SMIME ) ? "S/MIME " : "",
			 ( useDatasize && formatType != CRYPT_FORMAT_PGP && \
			   keyFileType != KEYFILE_X509_ALT ) ? \
			 " with datasize hint" : "" );
	if( useCustomHash )
		{
		fprintf( outputStream, " %s custom hash",
				 ( formatType == CRYPT_FORMAT_PGP ) ? "with" : "and" );
		}
	if( contentType == CRYPT_CONTENT_NONE )
		{
		fprintf( outputStream, " using %s", 
				 ( keyFileType == KEYFILE_OPENPGP_HASH ) ? "raw DSA key" : \
				 ( keyFileType == KEYFILE_PGP ) ? "raw public key" : \
				 useSuppliedKey ? "supplied X.509 cert" : "X.509 cert" );
		}
	if( keyFileType == KEYFILE_X509_ALT )
		fprintf( outputStream, " and automatic hash upgrade" );
	if( contentType != CRYPT_CONTENT_NONE )
		fprintf( outputStream, " and custom content type" );
	fputs( "...\n", outputStream );

	/* Get the private key */
	if( keyFileType == KEYFILE_OPENPGP_HASH || keyFileType == KEYFILE_PGP )
		{
		const C_STR keysetName = getKeyfileName( keyFileType, TRUE );

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, keysetName,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetPrivateKey( cryptKeyset, &cryptContext,
									CRYPT_KEYID_NAME, 
									getKeyfileUserID( KEYFILE_PGP, TRUE ),
									getKeyfilePassword( KEYFILE_PGP ) );
			cryptKeysetClose( cryptKeyset );
			}
		}
	else
		{
		filenameFromTemplate( filenameBuffer, USER_PRIVKEY_FILE_TEMPLATE, 2 );
		status = getPrivateKey( &cryptContext, 
								( keyFileType == KEYFILE_X509 ) ? \
									USER_PRIVKEY_FILE : filenameBuffer,
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Read of private key from key file failed, "
				 "status %d, line %d, cannot test enveloping.\n", status, 
				 __LINE__ );
		return( FALSE );
		}

	/* Create the envelope, push in the signing key, any extra information,
	   and the data to sign, pop the enveloped result, and destroy the
	   envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( contentType != CRYPT_CONTENT_NONE )
		{
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_CONTENTTYPE,
								contentType ) )
			return( FALSE );
		}
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext ) )
		{
		cryptDestroyContext( cryptContext );
		cryptDestroyEnvelope( cryptEnvelope );
		if( formatType == CRYPT_FORMAT_PGP && \
			contentType != CRYPT_CONTENT_NONE )
			{
			fputs( "  (This is an expected result since this test verifies "
				   "rejection of an\n   attempt to set a non-data content "
				   "type with PGP).\n\n", outputStream );
			return( TRUE );
			}
		return( FALSE );
		}
	else
		{
		if( formatType == CRYPT_FORMAT_PGP && \
			contentType != CRYPT_CONTENT_NONE )
			{
			fprintf( outputStream, "Addition of custom content type to PGP "
					 "envelope succeeded when it should\n  have failed, "
					 "line %d.\n", __LINE__ );
			return( FALSE );
			}
		}
	if( cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
						   cryptContext ) != CRYPT_ERROR_INITED )
		{
		fputs( "Addition of duplicate key to envelope wasn't "
			   "detected.\n\n", outputStream );
		return( FALSE );
		}
	if( !useSuppliedKey )
		cryptDestroyContext( cryptContext );
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   dataLength );
		}
	count = pushData( cryptEnvelope, data, dataLength, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  ( useSuppliedKey ) ? cryptContext : CRYPT_UNUSED,
							  keyFileType, FALSE, formatType );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( data, dataLength, globalBuffer, count ) )
		return( FALSE );
	if( useSuppliedKey )
		{
		/* If the following fails, there's a problem with handling reference
		   counting for keys */
		status = cryptDestroyContext( cryptContext );
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Attempt to destroy externally-added "
					 "sig.check key returned %d, line %d.\n", status, 
					 __LINE__ );
			return( FALSE );
			}
		}

	/* Clean up */
	fputs( "Enveloping of signed data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeSign( void )
	{
#ifndef USE_PGP2
	fputs( "Skipping raw public-key and PGP signing, which requires PGP 2.x "
		   "support to\n  be enabled.\n\n", outputStream );
#else
	if( cryptQueryCapability( CRYPT_ALGO_IDEA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		fputs( "Skipping raw public-key based signing, which requires the "
			   "IDEA cipher to\n  be enabled.\n\n", outputStream );
		}
	else
		{
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sign", KEYFILE_PGP, FALSE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
			return( FALSE );	/* Indefinite length, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sig", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
			return( FALSE );	/* Datasize, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sigc", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_CONTENT_COMPRESSEDDATA, CRYPT_FORMAT_CRYPTLIB ) )
			return( FALSE );	/* Datasize, raw key, custom content type */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sig.pgp", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, datasize, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sigc.pgp", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_CONTENT_COMPRESSEDDATA, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, datasize, raw key, custom content type */
#ifdef USE_3DES		/* Uses 3DES keyring */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sigd.pgp", KEYFILE_OPENPGP_HASH, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, datasize, raw DSA key */
#endif /* USE_3DES */
		}
#endif /* USE_PGP2 */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csgn", KEYFILE_X509, FALSE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length, certificate */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csgs", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_SMIME ) )
		return( FALSE );	/* Datasize, certificate, S/MIME semantics */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, TRUE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate, sigcheck key supplied */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg.pgp", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format, certificate */
#if 0	/* 8/7/2012 Removed since it conflicts with the functionality for 
					setting hash values for detached signatures.  This
					capability was never documented in the manual so it's
					unlikely that it was ever used */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_hsg", KEYFILE_X509, TRUE, TRUE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate, externally-suppl.hash */
#endif /* 0 */
	return( envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, TRUE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Externally-supplied key, to test isolation of sig.check key */

static int envelopeAlgoSign( const char *dumpFileName,
							 const CRYPT_ALGO_TYPE cryptAlgo )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT sigKey, sigCheckKey;
	const char *algoName = \
			( cryptAlgo == CRYPT_ALGO_RSA ) ? "RSA" : \
			( cryptAlgo == CRYPT_ALGO_DSA ) ? "DSA" : \
			( cryptAlgo == CRYPT_ALGO_ECDSA ) ? "ECDSA" : "<Unknown>";
	int count;

	fprintf( outputStream, "Testing %s signed enveloping...\n", algoName );

	/* Create the en/decryption contexts */
	switch( cryptAlgo )
		{
		case CRYPT_ALGO_RSA:
			if( !loadRSAContexts( CRYPT_UNUSED, &sigCheckKey, &sigKey ) )
				return( FALSE );
			break;

		case CRYPT_ALGO_DSA:
			if( !loadDSAContexts( CRYPT_UNUSED, &sigCheckKey, &sigKey ) )
				return( FALSE );
			break;

		case CRYPT_ALGO_ECDSA:
			if( !loadECDSAContexts( CRYPT_UNUSED, &sigCheckKey, &sigKey ) )
				return( FALSE );
			break;

		default:
			fprintf( outputStream, "Unknown signature algorithm %d, "
					 "line %d.\n", cryptAlgo, __LINE__ );
			return( FALSE );
		}

	/* Create the envelope, push in the signing key and the data to sign, 
	   pop the enveloped result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							sigKey ) )
		return( FALSE );
	cryptDestroyContext( sigKey );
	cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
					   ENVELOPE_TESTDATA_SIZE );
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA, 
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "%s-signed data has size %d bytes.\n", 
			 algoName, count );
	debugDump( dumpFileName, globalBuffer, count );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	count = pushData( cryptEnvelope, globalBuffer, count, NULL, 0 );
	if( !cryptStatusError( count ) )
		count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !getSigCheckResult( cryptEnvelope, sigCheckKey, TRUE, FALSE ) )
		return( FALSE );
	cryptDestroyContext( sigCheckKey );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of signed data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeSignAlgos( void )
	{
	if( !envelopeAlgoSign( "env_sig_rsa", CRYPT_ALGO_RSA ) )
		return( FALSE );
	if( !envelopeAlgoSign( "env_sig_dsa", CRYPT_ALGO_DSA ) )
		return( FALSE );
	if( cryptStatusOK( cryptQueryCapability( CRYPT_ALGO_ECDSA, NULL ) ) && \
		!envelopeAlgoSign( "env_sig_ecc", CRYPT_ALGO_ECDSA ) )
		return( FALSE );
	return( TRUE );
	}

/* Test envelope signing with automatic upgrade of the signing algorithm to
   SHA256 */

int testEnvelopeSignHashUpgrade( void )
	{
	return( envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csgh", KEYFILE_X509_ALT, TRUE, FALSE, FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_CRYPTLIB ) );
	}

/* Test signed envelope with forced envelope buffer overflow */

static int envelopeSignOverflow( const void *data, const int dataLength,
								 const char *dumpFileName,
								 const CRYPT_FORMAT_TYPE formatType,
								 const char *description )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext;
	BYTE localBuffer[ 8192 + 4096 ];
	const BOOLEAN forceOverflow = ( dataLength <= 8192 ) ? TRUE : FALSE;
	int localBufPos, bytesIn, bytesOut, status;

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of signed "
			   "enveloping...\n\n", outputStream );
		return( TRUE );
		}
	fprintf( outputStream, "Testing %ssigned enveloping with forced "
			 "overflow in %s...\n",
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : \
			 ( formatType == CRYPT_FORMAT_SMIME ) ? "S/MIME " : "",
			 description );

	/* Get the private key */
	status = getPrivateKey( &cryptContext, USER_PRIVKEY_FILE,
							USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Read of private key from key file failed, "
				 "cannot test enveloping, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Create the envelope and push in the signing key and any extra
	   information */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext ) )
		return( FALSE );
	cryptDestroyContext( cryptContext );
	status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
								dataLength );
	if( cryptStatusOK( status ) && forceOverflow )
		{
		/* Set an artificially-small buffer to force an overflow */
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
									8192 );
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "Couldn't set envelope parameters to force "
				 "overflow, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Push in the data to sign.  Since we're forcing an overflow, we can't
	   do this via the usual pushData() but have to do it manually to handle
	   the restart once the overflow occurs */
	status = cryptPushData( cryptEnvelope, data, dataLength, &bytesIn );
	if( cryptStatusError( status ) || bytesIn != dataLength )
		{
		fprintf( outputStream, "cryptPushData() failed with status %d, "
				 "copied %d of %d bytes, line %d.\n", status, bytesIn, 
				 dataLength, __LINE__ );
		return( FALSE );
		}
	status = cryptFlushData( cryptEnvelope );
	if( forceOverflow && status != CRYPT_ERROR_OVERFLOW )
		{
		fprintf( outputStream, "cryptFlushData() returned status %d, "
				 "should have been CRYPT_ERROR_OVERFLOW,\n  line %d.\n", 
				 status, __LINE__ );
		return( FALSE );
		}
	status = cryptPopData( cryptEnvelope, localBuffer, 8192 + 4096,
						   &bytesOut );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptPopData() #1 failed with status %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	localBufPos = bytesOut;
	status = cryptFlushData( cryptEnvelope );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptFlushData() failed with error code %d, "
				 "line %d.\n", status, __LINE__ );
		printErrorAttributeInfo( cryptEnvelope );
		return( FALSE );
		}
	status = cryptPopData( cryptEnvelope, localBuffer + localBufPos,
						   8192 + 4096 - localBufPos, &bytesOut );
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptPopData() #2 failed with status %d, "
				 "line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	localBufPos += bytesOut;
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", 
			 localBufPos );
	debugDump( dumpFileName, localBuffer, localBufPos );

	/* De-envelope the data and make sure that the result matches what we
	   pushed */
	bytesOut = envelopeSigCheck( localBuffer, localBufPos, CRYPT_UNUSED,
								 CRYPT_UNUSED, KEYFILE_X509, FALSE, 
								 formatType );
	if( bytesOut <= 0 )
		return( FALSE );
	if( !compareData( localBuffer, bytesOut, data, dataLength ) )
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of signed data succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testEnvelopeSignOverflow( void )
	{
	BYTE buffer[ 8192 + 1024 ];

	/* Push in just the right amount of data to force an overflow when we
	   generate the signature, to check overflow handling in the enveloping
	   code.

	   For PGP it's almost impossible to invoke overflow handling since the
	   enveloping code is set up to either emit the signature directly into
	   the buffer or, via an over-conservative estimation of buffer space,
	   ensure that the user leaves enough space in the buffer for the entire
	   sig.  For an estimated space requirement of 256 bytes, 8192 - 280
	   will force the sig into the auxBuffer, but since this is an over-
	   conservative estimate it'll then be flushed straight into the
	   envelope buffer.  The only way to actually force overflow handling
	   would be to use the longest possible key size and a certificate with 
	   a large issuerAndSerialNumber.

	   (In addition to the envelope buffer-overflow check, we also try
	   enveloping data with a length at the boundary where PGP switches from
	   2-byte to 4-byte lengths, 8384 bytes, to verify that this works OK).

	   For CMS, we can cause an overflow in one of two locations.  The first,
	   with 8192 - 1152 bytes of data, causes an overflow when emitting the
	   signing certs.  This is fairly straightforward, the enveloping code
	   always requires enough room for the signing certs, so all that happens
	   is that the user pops some data and tries again.

	   The second overflow is with 8192 - 1280 bytes of data, which causes an
	   overflow on signing */
	memset( buffer, '*', 8192 + 1024 );
	if( !envelopeSignOverflow( buffer, 8192 - 280, "env_sigo.pgp", CRYPT_FORMAT_PGP, "signature" ) )
		return( FALSE );	/* PGP format, raw key */
	if( !envelopeSignOverflow( buffer, 8384 - 6, "env_sigo2.pgp", CRYPT_FORMAT_PGP, "seg.boundary" ) )
		return( FALSE );	/* PGP format, raw key */
	if( !envelopeSignOverflow( buffer, 8192 - 1152, "env_csgo1", CRYPT_FORMAT_SMIME, "signing certs" ) )
		return( FALSE );	/* Datasize, certificate, S/MIME semantics */
	return( envelopeSignOverflow( buffer, 8192 - 1280, "env_csgo2", CRYPT_FORMAT_SMIME, "signature" ) );
	}						/* Datasize, certificate, S/MIME semantics */

/* Test the ability to process indefinite-length data with a break at every 
   byte position in the file */

int testEnvelopeSignIndef( void )
	{
	int count, bufPos, status;

	fputs( "Testing ability to process indefinite-length data at any byte "
		   "position...\n", outputStream );

	/* Read the data to be checked */
	count = readFileData( SMIME_SIG_FILE_INDEF, 
						  "Indefinite-length signed data", globalBuffer, 
						  BUFFER_SIZE, 128, FALSE );
	if( count <= 0 )
		return( FALSE );

	/* De-envelope the data, breaking the data quantity at every byte
	   position */
	for( bufPos = 1; bufPos < count; bufPos++ )
		{
		CRYPT_ENVELOPE cryptEnvelope;
		int byteCount;

		if( !createDeenvelope( &cryptEnvelope ) )
			return( FALSE );
		fprintf( outputStream, "Testing byte position %d.\r", bufPos );

		/* Push the first half of the data */
		status = cryptPushData( cryptEnvelope, globalBuffer, bufPos, 
								&byteCount );
		if( cryptStatusError( status ) )
			{
			/* If we get an underflow error within the header or trailer 
			   then everything's OK and we can continue */
			if( status == CRYPT_ERROR_UNDERFLOW && \
				( bufPos < 50 || bufPos > 69 ) )
				{
				/* We're in the middle of the header or trailer, a 
				   CRYPT_ERROR_UNDERFLOW is expected if we break at this 
				   point */
				}
			else
				return( FALSE );
			}
		if( byteCount != bufPos )
			return( FALSE );

		/* Push the second half of the data */
		status = cryptPushData( cryptEnvelope, globalBuffer + bufPos, 
								count - bufPos, &byteCount );
		if( cryptStatusOK( status ) )
			status = cryptFlushData( cryptEnvelope );
		if( cryptStatusError( status ) )
			return( FALSE );
		if( byteCount != count - bufPos )
			return( FALSE );

		/* Destroy the envelope.  Since we haven't processed the signed data
		   we'll get a CRYPT_ERROR_INCOMPLETE, so we ignore any error 
		   result */
		( void ) cryptDestroyEnvelope( cryptEnvelope );
		}

	fputs( "Ability to process indefinite-length data at any byte position "
		   "succeeded.\n\n", outputStream );
	return( TRUE );
	}

/* Test authenticated (MACd/authEnc) enveloping */

static int envelopeAuthent( const void *data, const int dataLength,
							const CRYPT_FORMAT_TYPE formatType,
							const BOOLEAN useAuthEnc, 
							const BOOLEAN useDatasize,
							const BOOLEAN usePKC,
							const int corruptDataLocation )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BOOLEAN corruptionDetected = FALSE;
	int count, integrityLevel, status;

	fprintf( outputStream, "Testing %sauthenticated%s enveloping", 
			 ( formatType == CRYPT_FORMAT_PGP ) ? "PGP-format " : "",
			 useAuthEnc ? "+encrypted" : "" );
	if( corruptDataLocation )
		fprintf( outputStream, " with deliberate corruption of data" );
	else
		{
		if( useDatasize )
			fprintf( outputStream, " with datasize hint" );
		}
	fputs( "...\n", outputStream );

	/* Create the envelope and push in the password after telling the
	   enveloping code that we want to MAC rather than encrypt */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_INTEGRITY,
							useAuthEnc ? CRYPT_INTEGRITY_FULL : \
										 CRYPT_INTEGRITY_MACONLY ) )
		{
		cryptDestroyEnvelope( cryptEnvelope );
		if( formatType == CRYPT_FORMAT_PGP && !useAuthEnc )
			{
			fputs( "  (This is an expected result since this test verifies "
				   "rejection of an\n   attempt to use a MAC with "
				   "PGP).\n\n", outputStream );
			return( TRUE );
			}
		return( FALSE );
		}
	if( usePKC )
		{
		CRYPT_CONTEXT cryptKey;

		status = getPublicKey( &cryptKey, USER_PRIVKEY_FILE, 
							   USER_PRIVKEY_LABEL );
		if( cryptStatusError( status ) )
			return( FALSE );
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
								cryptKey ) )
			{
			cryptDestroyContext( cryptKey );
			cryptDestroyEnvelope( cryptEnvelope );
			if( formatType == CRYPT_FORMAT_CRYPTLIB && !useAuthEnc )
				{
				fputs( "  (This is an expected result since this test "
					   "verifies rejection of an\n   attempt to use a PKC "
					   "with a MAC-only envelope).\n\n", outputStream );
				return( TRUE );
				}
			return( FALSE );
			}
		cryptDestroyContext( cryptKey );
		}
	else
		{
		if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
							   TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) ) )
			return( FALSE );
		}
	if( formatType == CRYPT_FORMAT_PGP && !useAuthEnc )
		{
		fprintf( outputStream, "Invalid attempt to use MAC-only "
				 "authentication for PGP-enveloped data wasn't\n  detected, "
				 "line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* Push in the data, pop the enveloped result, and destroy the
	   envelope */
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   dataLength );
		}
	count = pushData( cryptEnvelope, data, dataLength, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( ( formatType == CRYPT_FORMAT_PGP ) ? "env_mdc.pgp" : \
			   useAuthEnc ? ( usePKC ? "env_authenp" : "env_authenc" ) : \
			   useDatasize ? "env_mac" : usePKC ? "env_macp" : "env_macn", 
			   globalBuffer, count );

	/* If we're testing sig.verification, corrupt one of the payload bytes.  
	   This is a bit tricky because we have to hardcode the location of the
	   payload byte, if we change the format at (for example by using a 
	   different algorithm somewhere) then this location will change */
	if( corruptDataLocation > 0 )
		globalBuffer[ corruptDataLocation ]++;

	/* Create the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( usePKC )
		{
		CRYPT_KEYSET cryptKeyset;

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, 
								  CRYPT_KEYSET_FILE, USER_PRIVKEY_FILE, 
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusError( status ) )
			return( FALSE );
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEYSET_DECRYPT, 
								cryptKeyset ) )
			return( FALSE );
		cryptKeysetClose( cryptKeyset );
		}

	/* Push in the data */
	if( usePKC )
		{
		count = pushData( cryptEnvelope, globalBuffer, count, 
						  TEST_PRIVKEY_PASSWORD, 
						  paramStrlen( TEST_PRIVKEY_PASSWORD ) );
		}
	else
		{
		count = pushData( cryptEnvelope, globalBuffer, count, 
						  TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) );
		}
	if( cryptStatusError( count ) )
		{
		if( corruptDataLocation && count == CRYPT_ERROR_SIGNATURE )
			{
			fputs( "  (This is an expected result since this test verifies "
				   "handling of\n   corrupted authenticated data).\n", 
				   outputStream );
			corruptionDetected = TRUE;
			}
		else
			{
			cryptDestroyEnvelope( cryptEnvelope );
			return( FALSE );
			}
		}
	else
		{
		count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
		if( cryptStatusError( count ) )
			return( FALSE );
		}

	/* Make sure that the envelope is reported as being authenticated */
	if( cryptStatusError( \
			cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_INTEGRITY, 
							   &integrityLevel ) ) || \
		integrityLevel <= CRYPT_INTEGRITY_NONE )
		{
		fprintf( outputStream, "Integrity-protected envelope is reported "
				 "as having no integrity protection,\n  line %d.\n", 
				 __LINE__ );
		return( FALSE );
		}

	/* Determine the result of the MAC check */
	if( !getSigCheckResult( cryptEnvelope, CRYPT_UNUSED, FALSE, TRUE ) && \
		corruptDataLocation == 0 )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Make sure that we got what we were expecting */
	if( corruptDataLocation == 0 )
		{
		if( !compareData( data, dataLength, globalBuffer, count ) )
			return( FALSE );
		}
	else
		{
		if( !corruptionDetected )
			{
			fputs( "Corruption of encrypted data wasn't detected.\n\n", 
				   outputStream );
			return( FALSE );
			}
		}

	/* Clean up */
	fputs( "Enveloping of authenticated data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

int testEnvelopeAuthenticate( void )
	{
	int macAlgo, status;

	/* Find out what the default MAC algorithm is.  This is required because 
	   the data location that we corrupt changes based on the MAC algorithm 
	   used */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_MAC, &macAlgo );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* MAC-only envelopes are currently only supported via password (or at 
	   least shared-key) key management, using a PKC implies encrypt+MAC.
	   It's also somewhat unclear what the benefits of using a PKC for a MAC
	   are, so until there's a demand for this we only allow use with
	   passwords */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, FALSE, FALSE, 0 ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, FALSE, 0 ) )
		return( FALSE );	/* Datasize */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, TRUE, 0 ) )
		return( FALSE );	/* Datasize, PKC to check rejection of this format */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_PGP, FALSE, TRUE, FALSE, 0 ) )
		return( FALSE );	/* PGP format to check rejection of this format */
	return( envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, FALSE, ( macAlgo == CRYPT_ALGO_HMAC_SHA1 ) ? 175 : 208 ) );
	}						/* Datasize, corrupt data to check sig.verification */

int testEnvelopeAuthEnc( void )
	{
	int macAlgo, status;

	/* Find out what the default MAC algorithm is.  This is required because 
	   the data location that we corrupt changes based on the MAC algorithm 
	   used */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_MAC, &macAlgo );
	if( cryptStatusError( status ) )
		return( FALSE );

	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, FALSE, FALSE, 0 ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, FALSE, 0 ) )
		return( FALSE );	/* Datasize */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, TRUE, 0 ) )
		return( FALSE );	/* Datasize, PKC */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_PGP, TRUE, TRUE, FALSE, 0 ) )
		return( FALSE );	/* PGP format */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_PGP, TRUE, TRUE, TRUE, 0 ) )
		return( FALSE );	/* PGP format, PKC */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, FALSE, ( macAlgo == CRYPT_ALGO_HMAC_SHA1 ) ? 192 : 260 ) )
		return( FALSE );	/* Datasize, corrupt payload data to check sig.verification */
	return( envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, FALSE, ( macAlgo == CRYPT_ALGO_HMAC_SHA1 ) ? 170 : 228 ) );
	}						/* Datasize, corrupt metadata to check sig.verification */

/* Test handling of injected faults */

#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )

typedef enum { TEST_SIGN, TEST_AUTH, TEST_AUTHENC } TEST_TYPE;

static int testCMSDebugCheck( const TEST_TYPE testType, const FAULT_TYPE testFaultType )
	{
	int status;

	cryptSetFaultType( testFaultType );
	switch( testType )
		{
		case TEST_SIGN:
			status = envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
								   "fault", KEYFILE_X509, TRUE, FALSE, FALSE, 
								   CRYPT_CONTENT_NONE, CRYPT_FORMAT_SMIME );
			break;

		case TEST_AUTH:
			status = envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
									  CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, 
									  FALSE, 0 );
			break;

		case TEST_AUTHENC:
			status = envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
									  CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, 
									  FALSE, 0 );
			break;

		default:
			assert( 0 );
		}
	if( status )
		{
		cryptSetFaultType( FAULT_NONE );
		fputs( "  (This test should have led to an enveloping failure but "
			   "didn't, test has\n   failed).\n\n", outputStream );
		return( FALSE );
		}

	/* These tests are supposed to fail, so if this happens then the overall 
	   test has succeeded */
	fputs( "  (This test checks error handling, so the failure response is "
		   "correct).\n\n", outputStream );
	return( TRUE );
	}

static int testPGPDebugCheck( const TEST_TYPE testType, const FAULT_TYPE testFaultType )
	{
	int status;

	cryptSetFaultType( testFaultType );
	switch( testType )
		{
		case TEST_SIGN:
			status = envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
								   "fault.pgp", KEYFILE_PGP, TRUE, FALSE, 
								   FALSE, CRYPT_CONTENT_NONE, CRYPT_FORMAT_PGP );
			break;

		case TEST_AUTH:
			status = envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
									  CRYPT_FORMAT_PGP, TRUE, TRUE, FALSE, 0 );
			break;

		default:
			assert( 0 );
		}
	if( status )
		{
		cryptSetFaultType( FAULT_NONE );
		fputs( "  (This test should have led to an enveloping failure but "
			   "didn't, test has\n   failed).\n\n", outputStream );
		return( FALSE );
		}

	/* These tests are supposed to fail, so if this happens then the overall 
	   test has succeeded */
	fputs( "  (This test checks error handling, so the failure response is "
		   "correct).\n\n", outputStream );
	return( TRUE );
	}
#endif /* CONFIG_FAULTS && Debug */

int testEnvelopeCMSDebugCheck( void )
	{
#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )
	cryptSetFaultType( FAULT_NONE );
	if( !testCMSDebugCheck( TEST_SIGN, FAULT_BADSIG_DATA ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_SIGN, FAULT_BADSIG_HASH ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_SIGN, FAULT_ENVELOPE_CORRUPT_AUTHATTR ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_AUTH, FAULT_ENVELOPE_CMS_CORRUPT_AUTH_DATA ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_AUTH, FAULT_ENVELOPE_CMS_CORRUPT_AUTH_MAC ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_AUTHENC, FAULT_ENVELOPE_CMS_CORRUPT_AUTH_DATA ) )
		return( FALSE );
	if( !testCMSDebugCheck( TEST_AUTHENC, FAULT_ENVELOPE_CMS_CORRUPT_AUTH_MAC ) )
		return( FALSE );
	cryptSetFaultType( FAULT_NONE );
#endif /* CONFIG_FAULTS && Debug */
	return( TRUE );
	}

int testEnvelopePGPDebugCheck( void )
	{
#if defined( CONFIG_FAULTS ) && !defined( NDEBUG )
	cryptSetFaultType( FAULT_NONE );
	if( !testPGPDebugCheck( TEST_AUTH, FAULT_BADSIG_DATA ) )
		return( FALSE );
	if( !testPGPDebugCheck( TEST_AUTH, FAULT_BADSIG_HASH ) )
		return( FALSE );
	if( !testPGPDebugCheck( TEST_SIGN, FAULT_ENVELOPE_PGP_CORRUPT_ONEPASS_ID ) )
		return( FALSE );
	if( !testPGPDebugCheck( TEST_SIGN, FAULT_CORRUPT_ID ) )
		return( FALSE );
	if( !testPGPDebugCheck( TEST_SIGN, FAULT_ENVELOPE_CORRUPT_AUTHATTR ) )
		return( FALSE );
	cryptSetFaultType( FAULT_NONE );
#endif /* CONFIG_FAULTS && Debug */
	return( TRUE );
	}

/****************************************************************************
*																			*
*							CMS Enveloping Test Routines 					*
*																			*
****************************************************************************/

/* Test CMS signature generation/checking */

static int displaySigResult( const CRYPT_ENVELOPE cryptEnvelope,
							 const CRYPT_CONTEXT sigCheckContext,
							 const BOOLEAN mustHaveAttributes,
							 const BOOLEAN firstSig )
	{
	CRYPT_CERTIFICATE signerInfo;
	BOOLEAN sigStatus;
	int status;

	/* Determine the result of the signature check.  We only display the
	   attributes for the first sig since this operation walks the attribute
	   list,which moves the attribute cursor */
	sigStatus = getSigCheckResult( cryptEnvelope, sigCheckContext,
								   firstSig, FALSE );
	if( sigCheckContext != CRYPT_UNUSED && !mustHaveAttributes )
		{
		/* If this is a PGP signature (indicated by the fact that the 
		   sig-check key isn't included in the signed data and we're not 
		   checking attributes), there's no signer info or extra data 
		   present */
		return( sigStatus );
		}

	/* Report on the signer and signature info.  We continue even if the sig.
	   status is bad since we can still try and display signing info even if
	   the check fails */
	status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
								&signerInfo );
	if( cryptStatusError( status ) && sigStatus )
		{
		fprintf( outputStream, "Couldn't retrieve signer information from "
				 "CMS signature, status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( cryptStatusOK( status ) )
		{
		fputs( "Signer information is:\n", outputStream );
		if( !printCertInfo( signerInfo ) )
			return( FALSE );
		cryptDestroyCert( signerInfo );
		}
	status = cryptGetAttribute( cryptEnvelope,
							CRYPT_ENVINFO_SIGNATURE_EXTRADATA, &signerInfo );
	if( cryptStatusError( status ) && sigStatus && \
		( mustHaveAttributes || status != CRYPT_ERROR_NOTFOUND ) )
		{
		fprintf( outputStream, "Couldn't retrieve signature information "
				 "from CMS signature, status = %d, line %d.\n", status, 
				 __LINE__ );
		return( FALSE );
		}
	if( cryptStatusOK( status ) )
		{
		fputs( "Signature information is:\n", outputStream );
		if( !printCertInfo( signerInfo ) )
			return( FALSE );
		cryptDestroyCert( signerInfo );
		}

	return( sigStatus );
	}

static int cmsEnvelopeSigCheck( const void *signedData, 
				const int signedDataLength, 
				const CRYPT_CONTEXT sigCheckContext, 
				const CRYPT_CONTEXT hashContext, const BOOLEAN detachedSig,
				const BOOLEAN hasTimestamp, const BOOLEAN checkData,
				const BOOLEAN mustHaveAttributes, const BOOLEAN checkWrongKey,
				const BOOLEAN testRefCount )

	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	/* Create the (de-)envelope and push in the data.  Since this is a CMS
	   signature that carries its certs with it, there's no need to push in
	   a sig.check keyset.  If it has a detached sig, we need to push two
	   lots of data, first the signature to set the envelope state, then the
	   data, however if the hash is being supplied externally we just set the
	   hash attribute.  In addition if it's a detached sig, there's nothing
	   to be unwrapped so we don't pop any data */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( detachedSig && hashContext != CRYPT_UNUSED )
		{
		/* The hash value is being supplied externally, add it to the
		   envelope before we add the signature data */
		status = cryptSetAttribute( cryptEnvelope, 
									CRYPT_ENVINFO_DETACHEDSIGNATURE, TRUE );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_HASH,
										hashContext );
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Couldn't add externally-generated hash "
					 "value to envelope, status %d, line %d.\n", status, 
					 __LINE__ );
			return( FALSE );
			}
		}
	count = pushData( cryptEnvelope, signedData, signedDataLength, NULL, 0 );
	if( count == CRYPT_ERROR_NOTAVAIL && \
		!( detachedSig || hasTimestamp || checkData ) )
		{
		/* Some old signed data uses deprecated or now-broken algorithms
		   which will produce a CRYPT_ERROR_NOTAVAIL if we try and verify
		   the signature, treat this as a special case */
		fputs( "Warning: The hash/signature algorithm required to verify "
			   "the signed data\n         isn't enabled in this build of "
			   "cryptlib, can't verify the\n         signature.\n\n", 
			   outputStream );
		if( !destroyEnvelope( cryptEnvelope ) )
			return( FALSE );
		return( TRUE );
		}
	if( !cryptStatusError( count ) )
		{
		if( detachedSig )
			{
			if( hashContext == CRYPT_UNUSED )
				{
				count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
								  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
				}
			}
		else
			count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
		}
	if( cryptStatusError( count ) )
		return( FALSE );

	/* If we're checking for the ability to reject an incorrect key, fetch
	   a bogus signature key and try and add that */
	if( checkWrongKey )
		{
		CRYPT_CERTIFICATE cryptWrongCert;

		status = getPublicKey( &cryptWrongCert, DUAL_PRIVKEY_FILE, 
							   DUAL_SIGNKEY_LABEL );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
									cryptWrongCert );
		cryptDestroyCert( cryptWrongCert );
		if( status != CRYPT_ERROR_WRONGKEY )
			{
			fprintf( outputStream, "Use of incorrect key wasn't detected, "
					 "got %d, should have been %d, line %d.\n", status, 
					 CRYPT_ERROR_WRONGKEY, __LINE__ );
			return( FALSE );
			}
		}

	/* Display the details of the envelope signature and check whether
	   there's more information such as a timestamp or a second signature
	   present */
	status = displaySigResult( cryptEnvelope, sigCheckContext, 
							   mustHaveAttributes, TRUE );
	if( status == TRUE && hasTimestamp )
		{
		CRYPT_ENVELOPE cryptTimestamp;
		int contentType;

		/* Try and get the timestamp info.  Note that we can't safely use
		   displaySigResult() on every timestamp because many are stripped-
		   down minimal-size CMS messages with no additional sig-checking
		   info present (for those ones we have to stop at reading the CMS 
		   content-type to make sure that everything's OK), but for the
		   cryptlib test TSA there's always a full CMS signed message 
		   returned */
		fprintf( outputStream, "Envelope contains a timestamp..." );
		status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_TIMESTAMP,
									&cryptTimestamp );
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "\nCouldn't read timestamp from "
					 "envelope, status %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptGetAttribute( cryptTimestamp,
									CRYPT_ENVINFO_CONTENTTYPE, &contentType );
		if( cryptStatusError( status ) || \
			contentType != CRYPT_CONTENT_TSTINFO )
			{
			fprintf( outputStream, "\nTimestamp data envelope doesn't "
					 "appear to contain a timestamp, line %d.\n", __LINE__ );
			return( FALSE );
			}
		fputs( " timestamp data appears OK.\n", outputStream );

		/* Now get the signature info (if the timestamp doesn't contain a 
		   full CMS message then we'd set 'status = TRUE' at this point
		   without performing the signture check) */
		fputs( "Timestamp signature information is:\n", outputStream );
		status = displaySigResult( cryptTimestamp, sigCheckContext, 
								   TRUE, TRUE );
		cryptDestroyEnvelope( cryptTimestamp );
		}
	if( status == TRUE && cryptStatusOK( \
			cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_NEXT ) ) )
		{
		fputs( "Data has a second signature:\n", outputStream );
		status = displaySigResult( cryptEnvelope, CRYPT_UNUSED, 
								   mustHaveAttributes, FALSE );
		}
	if( status == TRUE && cryptStatusOK( \
			cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_NEXT ) ) )
		{
		/* We can have two, but not three */
		fprintf( outputStream, "Data appears to have (nonexistent) third "
				 "signature, line %d.\n", __LINE__ );
		return( FALSE );
		}

	/* If we're testing the ability to handle reference-counted objects, 
	   create multiple references to the certificate object in the signed
	   data and make sure they're accessible */
	if( testRefCount )
		{
		CRYPT_CERTIFICATE cryptCert1, cryptCert2, cryptCert3;
		int value;

		/* Create three references to the signing certificate */
		status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE, 
									&cryptCert1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE, 
										&cryptCert2 );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE, 
										&cryptCert3 );
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Couldn't retrieve 3 copies of signing "
					 "certificate from envelope, line %d.\n", __LINE__ );
			return( FALSE );
			}
		if( cryptCert1 != cryptCert2 || cryptCert1 != cryptCert3 )
			{
			fprintf( outputStream, "Multiple-referenced object has "
					 "different object handles, line %d.\n", __LINE__ );
			return( FALSE );
			}

		/* Destroy the second reference and make sure that it's still
		   accessible via both itself and another reference */
		status = cryptDestroyCert( cryptCert2 );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptCert2, CRYPT_CERTINFO_CERTTYPE, 
										&value );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptCert1, CRYPT_CERTINFO_CERTTYPE, 
										&value );
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Removing signing certificate reference "
					 "caused consistency failure, line %d.\n", __LINE__ );
			return( FALSE );
			}

		/* Destroy the original reference and make sure that it's no longer 
		   accessible but other references are */
		status = cryptDestroyCert( cryptCert1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptCert1, CRYPT_CERTINFO_CERTTYPE, 
										&value );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptCert3, CRYPT_CERTINFO_CERTTYPE, 
										&value );
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Removing signing certificate reference "
					 "caused consistency failure, line %d.\n", __LINE__ );
			return( FALSE );
			}

		/* Destroy the last reference and make sure that it's now 
		   inaccessible */
		status = cryptDestroyCert( cryptCert1 );
		if( cryptStatusOK( status ) )
			{
			status = cryptGetAttribute( cryptCert1, CRYPT_CERTINFO_CERTTYPE, 
										&value );
			status = cryptStatusError( status ) ? \
					 CRYPT_OK : CRYPT_ERROR_FAILED;
			}
		if( cryptStatusError( status ) )
			{
			fprintf( outputStream, "Removing signing certificate reference "
					 "caused consistency failure, line %d.\n", __LINE__ );
			return( FALSE );
			}

		/* Remember that everything went OK */
		status = TRUE;
		}

	/* Make sure that the result matches what we pushed */
	if( !detachedSig && checkData && \
		!compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, \
					  globalBuffer, count ) )
		return( FALSE );

	/* Clean up */
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	return( status );
	}

static int cmsEnvelopeSign( const BOOLEAN useDatasize,
				const BOOLEAN useAttributes, const BOOLEAN useExtAttributes,
				const BOOLEAN detachedSig, const int externalHashLevel,
				const BOOLEAN useTimestamp, const BOOLEAN useNonDataContent,
				const BOOLEAN dualSig, const BOOLEAN testRefCount,
				const CRYPT_CONTEXT externalSignContext,
				const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext, cryptContext2, hashContext = CRYPT_UNUSED;
	BOOLEAN isPGP = ( formatType == CRYPT_FORMAT_PGP ) ? TRUE : FALSE;
	BOOLEAN isRawCMS = ( formatType == CRYPT_FORMAT_CRYPTLIB ) ? TRUE : FALSE;
	int count, status = CRYPT_OK;

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of CMS signed "
			   "enveloping...\n\n", outputStream );
		return( TRUE );
		}
	fprintf( outputStream, "Testing %s %s%s", isPGP ? "PGP" : "CMS",
			 useExtAttributes ? "extended " : "",
			 detachedSig ? "detached signature" : \
				dualSig ? "dual signature" : "signed enveloping" );
	if( useNonDataContent )
		fprintf( outputStream, " of non-data content" );
	if( testRefCount )
		fprintf( outputStream, " for reference-count capability check" );
	if( externalHashLevel )
		{
		fprintf( outputStream, ( externalHashLevel == 1 ) ? \
				 " with externally-supplied hash for verify" : \
				 " with ext-supplied hash for sign and verify" );
		}
	if( !isPGP && !( useAttributes || testRefCount ) )
		fprintf( outputStream, " without signing attributes" );
	if( useDatasize && \
		!( useNonDataContent || useAttributes || useExtAttributes || \
		   detachedSig || useTimestamp || testRefCount ) )
		{
		/* Keep the amount of stuff being printed down */
		fprintf( outputStream, " with datasize hint" );
		}
	if( useTimestamp )
		fprintf( outputStream, " and timestamp" );
	fputs( "...\n", outputStream );

	/* Get the private key.  If we're applying two signatures, we also get
	   a second signing key.  Since the dual-key file test has created a
	   second signing key, we use that as the most convenient one */
	if( externalSignContext != CRYPT_UNUSED )
		cryptContext = externalSignContext;
	else
		{
		status = getPrivateKey( &cryptContext, USER_PRIVKEY_FILE,
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusError( status ) )
			{
			fputs( "Read of private key from key file failed, cannot test "
				   "CMS enveloping.\n\n", outputStream );
			return( FALSE );
			}
		}
	if( dualSig )
		{
		status = getPrivateKey( &cryptContext2, DUAL_PRIVKEY_FILE,
								DUAL_SIGNKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusError( status ) )
			{
			fputs( "Read of private key from key file failed, cannot test "
				   "CMS enveloping.\n\n", outputStream );
			return( FALSE );
			}
		}

	/* If we're supplying the hash value for signing externally, calculate 
	   it now */
	if( externalHashLevel > 1 )
		{
		int hashAlgo;

		status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, 
									&hashAlgo );
		if( cryptStatusOK( status ) )
			{
			status = cryptCreateContext( &hashContext, CRYPT_UNUSED,
										 hashAlgo );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptEncrypt( hashContext, ENVELOPE_TESTDATA,
								   ENVELOPE_TESTDATA_SIZE );
			}
		if( cryptStatusOK( status ) && formatType == CRYPT_FORMAT_CMS )
			status = cryptEncrypt( hashContext, "", 0 );
		if( cryptStatusError( status ) )
			{
			fputs( "Couldn't create external hash of data.\n\n", 
				   outputStream );
			return( FALSE );
			}
		}

	/* Create the CMS envelope and  push in the signing key(s) and data */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext ) )
		return( FALSE );
	if( dualSig )
		{
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
								cryptContext2 ) )
			return( FALSE );
		}
	if( detachedSig )
		{
		if( !addEnvInfoNumeric( cryptEnvelope, 
								CRYPT_ENVINFO_DETACHEDSIGNATURE, TRUE ) )
			return( FALSE );
		}
	if( externalHashLevel > 1 )
		{
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_HASH,
								hashContext ) )
			return( FALSE );
		}
	if( ( externalSignContext == CRYPT_UNUSED ) && !isPGP && !isRawCMS )
		cryptDestroyContext( cryptContext );
	if( dualSig )
		cryptDestroyContext( cryptContext2 );
	if( externalHashLevel > 1 )
		{
		cryptDestroyContext( hashContext );
		hashContext = CRYPT_UNUSED;
		}
	if( useNonDataContent )
		{
		/* Test non-data content type w.automatic attribute handling */
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_CONTENTTYPE,
									CRYPT_CONTENT_SIGNEDDATA );
		}
	if( cryptStatusOK( status ) && useDatasize )
		{
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
									ENVELOPE_TESTDATA_SIZE );
		}
	if( cryptStatusOK( status ) && useExtAttributes )
		{
		/* We have to be careful when setting CMS attributes because most 
		   are never used by anything so they're only available of use of 
		   obscure attributes is enabled (unfortunately there aren't
		   actually any attributes that are available for this, but the
		   functionality is tested by the added-by-default attributes such
		   as the signing time) */
#ifdef USE_CMS_OBSCURE
		CRYPT_CERTIFICATE cmsAttributes;

		/* Add an ESS security label and signing description as signing
		   attributes */
		status = cryptCreateCert( &cmsAttributes, CRYPT_UNUSED,
								  CRYPT_CERTTYPE_CMS_ATTRIBUTES );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_POLICY,
							TEXT( "1 3 6 1 4 1 9999 1" ),
							paramStrlen( TEXT( "1 3 6 1 4 1 9999 1" ) ) );
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION,
							CRYPT_CLASSIFICATION_SECRET );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE,
							TEXT( "1 3 6 1 4 1 9999 2" ),
							paramStrlen( TEXT( "1 3 6 1 4 1 9999 2" ) ) );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE,
							"\x04\x04\x01\x02\x03\x04", 6 );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION,
							"This signature isn't worth the paper it's not "
							"printed on", 56 );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptSetAttribute( cryptEnvelope,
							CRYPT_ENVINFO_SIGNATURE_EXTRADATA, cmsAttributes );
			}
		cryptDestroyCert( cmsAttributes );
#endif /* USE_CMS_OBSCURE */
		}
	if( cryptStatusOK( status ) && !isPGP && !useAttributes )
		status = cryptSetAttribute( CRYPT_UNUSED,
							CRYPT_OPTION_CMS_DEFAULTATTRIBUTES, FALSE );
	if( cryptStatusOK( status ) && useTimestamp )
		{
		CRYPT_SESSION cryptSession;

		/* Create the TSP session, add the TSA URL, and add it to the
		   envelope */
		status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
									 CRYPT_SESSION_TSP );
		if( status == CRYPT_ERROR_PARAM3 )	/* TSP session access not available */
			return( CRYPT_ERROR_NOTAVAIL );
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_SERVER_NAME,
										  TSP_DEFAULTSERVER_NAME,
										  paramStrlen( TSP_DEFAULTSERVER_NAME ) );
		if( cryptStatusError( status ) )
			{
			return( attrErrorExit( cryptSession, "cryptSetAttributeString()",
								   status, __LINE__ ) );
			}
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_TIMESTAMP,
									cryptSession );
		cryptDestroySession( cryptSession );
		if( isRawCMS )
			{
			/* Signing attributes aren't allowed for raw CMS data */
			cryptDestroyContext( cryptContext );
			destroyEnvelope( cryptEnvelope );
			if( cryptStatusOK( status ) )
				{
				fprintf( outputStream, "Addition of timestamping attribute "
						 "to CRYPT_FORMAT_CRYPTLIB envelope\n  succeeded, "
						 "should have failed, line %d.\n", __LINE__ );
				return( FALSE );
				}
			fputs( "Enveloping with timestamp using invalid env.type was "
				   "correctly rejected.\n\n", outputStream );
			return( TRUE );
			}
		}
	if( cryptStatusError( status ) )
		{
		fprintf( outputStream, "cryptSetAttribute() failed with error "
				 "code %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Push in the data to be signed, unless it's already been processed via
	   an external hash */
	if( externalHashLevel < 2 )
		{
		status = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
						   ENVELOPE_TESTDATA_SIZE, NULL, 0 );
		}
	if( !isPGP && !useAttributes )
		{
		/* Restore the default attributes setting */
		cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_CMS_DEFAULTATTRIBUTES,
						   TRUE );
		}
	if( cryptStatusError( status ) )
		{
		/* The timestamping can fail for a wide range of (non-fatal) reasons,
		   typically either because this build doesn't have networking
		   enabled or because the TSA can't be contacted, so we don't treat
		   this one as a fatal error */
		if( useTimestamp )
			{
			fputs( "Envelope timestamping failed due to problems talking to "
				   "TSA, this is a non-\ncritical problem.  "
				   "Continuing...\n\n", outputStream );
			cryptDestroyEnvelope( cryptEnvelope );
			return( TRUE );
			}
		return( FALSE );
		}

	/* Pop the resulting signature data and destroy the envelope */
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "%s %s has size %d bytes.\n", 
			 isPGP ? "PGP" : "CMS",
			 ( detachedSig ) ? "detached signature" : "signed data",
			 count );
	debugDump( detachedSig ?
				 ( !isPGP ? \
				   ( useDatasize ? "smi_dsg" : "smi_dsgn" ) : \
				   "pgp_dsg.pgp" ) : \
			   useExtAttributes ? \
				 ( useDatasize ? "smi_esg" : "smi_esgn" ) : \
			   useTimestamp ? \
				 ( useDatasize ? "smi_tsg" : "smi_tsgn" ) : \
			   useNonDataContent ? \
				 ( useDatasize ? "smi_ndc" : "smi_ndcn" ) : \
			   dualSig ? \
				 ( useDatasize ? "smi_2sg" : "smi_n2sg" ) : \
			   useDatasize ? "smi_sig" : "smi_sign", globalBuffer, count );

	/* If we're supplying the hash value for verification externally, 
	   calculate it now */
	if( externalHashLevel > 0 )
		{
		int hashAlgo;

		status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_HASH, 
									&hashAlgo );
		if( cryptStatusOK( status ) )
			{
			status = cryptCreateContext( &hashContext, CRYPT_UNUSED,
										 hashAlgo );
			}
		if( cryptStatusOK( status ) )
			{
			status = cryptEncrypt( hashContext, ENVELOPE_TESTDATA,
								   ENVELOPE_TESTDATA_SIZE );
			}
		if( cryptStatusOK( status ) && formatType == CRYPT_FORMAT_CMS )
			status = cryptEncrypt( hashContext, "", 0 );
		if( cryptStatusError( status ) )
			{
			fputs( "Couldn't create external hash of data.\n\n", 
				   outputStream );
			return( FALSE );
			}
		}

	/* Make sure that the signature is valid.  The somewhat redundant mapping
	   of useNonDataContent is because we're performing an additional check
	   of envelope key-handling as part of this particular operation */
	status = cmsEnvelopeSigCheck( globalBuffer, count,
					isPGP || isRawCMS ? cryptContext : CRYPT_UNUSED, 
					hashContext, detachedSig, useTimestamp, TRUE, 
					useAttributes, useNonDataContent ? TRUE : FALSE, 
					testRefCount );
	if( externalHashLevel > 0 )
		cryptDestroyContext( hashContext );
	if( isPGP || isRawCMS )
		cryptDestroyContext( cryptContext );
	if( status <= 0 )
		return( FALSE );

	if( detachedSig )
		{
		fprintf( outputStream, "Creation of %s %sdetached signature "
				 "%ssucceeded.\n\n",
				 isPGP ? "PGP" : "CMS", useExtAttributes ? "extended " : "",
				 ( hashContext != CRYPT_UNUSED ) ? \
					"with externally-supplied hash " : "" );
		}
	else
		{
		fprintf( outputStream, "Enveloping of CMS %s%ssigned data "
				 "succeeded.\n\n", useExtAttributes ? "extended " : "",
				 useTimestamp ? "timestamped " : "" );
		}
	return( TRUE );
	}

int testCMSEnvelopeSign( void )
	{
	if( !cmsEnvelopeSign( FALSE, FALSE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Minimal (no default S/MIME attributes) */
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Standard (default S/MIME signing attributes) */
	if( !cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize and attributes */
	if( !cmsEnvelopeSign( FALSE, TRUE, TRUE, FALSE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Extended signing attributes */
	if( !cmsEnvelopeSign( TRUE, TRUE, TRUE, FALSE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize and extended attributes */
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, 0, FALSE, TRUE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
	}						/* Signing of non-data content */

int testCMSEnvelopeDualSign( void )
	{
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
							/* Standard, with two signatures */
	}

int testCMSEnvelopeDetachedSig( void )
	{
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 0, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Detached sig and attributes */
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 1, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Detached sig, attributes, externally-suppl.hash for verify */
	return( cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 2, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
	}						/* Detached sig, externally-suppl.hash for sign and verify */

int testCMSEnvelopeSignEx( const CRYPT_CONTEXT signContext )
	{
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, signContext, CRYPT_FORMAT_CMS ) );
	}						/* Datasize, attributes, external signing context */

int testCMSEnvelopeRefCount( void )
	{
	/* This isn't so much a signature test as a test of the reference-
	   counting mechanism in the cryptlib kernel, but it uses a CMS 
	   signature with associated certificate data for the test */
	return( cmsEnvelopeSign( TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
	}

int testPGPEnvelopeDetachedSig( void )
	{
	if( !cmsEnvelopeSign( TRUE, FALSE, FALSE, TRUE, 1, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* Detached sig, data size, externally-suppl.hash for verify, PGP format */
	return( cmsEnvelopeSign( TRUE, FALSE, FALSE, TRUE, 2, FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) );
	}						/* Detached sig, data size, externally-suppl.hash for sign and verify, PGP format */

int testSessionEnvTSP( void )
	{
	/* This is a pseudo-enveloping test that uses the enveloping
	   functionality but is called as part of the session tests since full
	   testing of the TSP handling requires that it be used to timestamp an
	   S/MIME sig */
	if( !cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize, attributes, timestamp */
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Invalid attempt to use attrs.with raw CMS envelope */

static int cmsImportSignedData( const char *fileName, const int fileNo )
	{
	BYTE *bufPtr = globalBuffer;
	char msgBuffer[ 128 ];
	int count, status;

	/* Read the test data */
	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE )
		{
		if( ( bufPtr = malloc( count ) ) == NULL )
			{
			fprintf( outputStream, "Couldn't allocate test buffer of "
					 "%d bytes, line %d.\n", count, __LINE__ );
			return( FALSE );
			}
		}
	sprintf( msgBuffer, "S/MIME SignedData #%d", fileNo );
	count = readFileData( fileName, msgBuffer, bufPtr, count, 2048, FALSE );
	if( count <= 0 )
		{
		if( bufPtr != globalBuffer )
			free( bufPtr );
		return( count );
		}

	/* Check the signature on the data */
	status = cmsEnvelopeSigCheck( bufPtr, count, CRYPT_UNUSED, CRYPT_UNUSED,
								  FALSE, FALSE, FALSE, 
								  ( fileNo == 1 ) ? FALSE : TRUE, FALSE, FALSE );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	if( status )
		fputs( "S/MIME SignedData import succeeded.\n\n", outputStream );
	else
		{
		/* The AuthentiCode data sig-check fails for some reason */
		if( fileNo == 2 )
			{
			fputs( "AuthentiCode SignedData import succeeded but signature "
				   "couldn't be verified\n  due to AuthentiCode special "
				   "processing requirements.\n\n", outputStream );
			}
		}
	return( status );
	}

/* Test CMS enveloping/de-enveloping */

static int cmsEnvelopeDecrypt( const void *envelopedData,
							   const int envelopedDataLength,
							   const CRYPT_HANDLE externalKeyset,
							   const C_STR externalPassword,
							   const BOOLEAN checkDataMatch )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	/* Create the envelope and push in the decryption keyset */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( externalKeyset != CRYPT_UNUSED )
		status = addEnvInfoNumeric( cryptEnvelope,
								CRYPT_ENVINFO_KEYSET_DECRYPT, externalKeyset );
	else
		{
		CRYPT_KEYSET cryptKeyset;

		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
								  CRYPT_KEYSET_FILE, USER_PRIVKEY_FILE,
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusOK( status ) )
			{
			status = addEnvInfoNumeric( cryptEnvelope,
								CRYPT_ENVINFO_KEYSET_DECRYPT, cryptKeyset );
			}
		cryptKeysetClose( cryptKeyset );
		}
	if( status <= 0 )
		return( FALSE );

	/* Push in the data */
	if( externalPassword == NULL )
		externalPassword = TEST_PRIVKEY_PASSWORD;
	count = pushData( cryptEnvelope, envelopedData, envelopedDataLength,
					  externalPassword, paramStrlen( externalPassword ) );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* If we're not checking for a match of the decrypted data (done when 
	   the data is coming from an external source rather than being 
	   something that we generated ourselves), we're done */
	if( !checkDataMatch )
		return( TRUE );

	/* Make sure that the result matches what we pushed */
	if( !compareData( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );

	return( TRUE );
	}

static int cmsEnvelopeCrypt( const char *dumpFileName,
							 const BOOLEAN useDatasize,
							 const BOOLEAN useStreamCipher,
							 const BOOLEAN useLargeBlockCipher,
							 const BOOLEAN useExternalRecipientKeyset,
							 const CRYPT_HANDLE externalCryptContext,
							 const CRYPT_HANDLE externalKeyset,
							 const C_STR externalPassword,
							 const C_STR recipientName )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_HANDLE cryptKey;
	int count, status;

	if( !keyReadOK )
		{
		fputs( "Couldn't find key files, skipping test of CMS encrypted "
			   "enveloping...\n\n", outputStream );
		return( TRUE );
		}
	fprintf( outputStream, "Testing CMS public-key encrypted enveloping" );
	if( externalKeyset != CRYPT_UNUSED && recipientName != NULL )
		{
		if( useExternalRecipientKeyset )
			fprintf( outputStream, " with recipient keys in device" );
		else
			fprintf( outputStream, " with dual encr./signing certs" );
		}
	else
		{
		if( useStreamCipher )
			fprintf( outputStream, " with stream cipher" );
		else
			{
			if( useLargeBlockCipher )
				fprintf( outputStream, " with large block size cipher" );
			else
				{
				if( useDatasize )
					fprintf( outputStream, " with datasize hint" );
				}
			}
		}
	fputs( "...\n", outputStream );

	/* Get the public key.  We use assorted variants to make sure that they
	   all work */
	if( externalCryptContext != CRYPT_UNUSED )
		{
		int cryptAlgo;

		status = cryptGetAttribute( externalCryptContext, CRYPT_CTXINFO_ALGO,
									&cryptAlgo );
		if( cryptStatusError( status ) )
			{
			fputs( "Couldn't determine algorithm for public key, cannot "
				   "test CMS enveloping.\n\n", outputStream );
			return( FALSE );
			}
		cryptKey = externalCryptContext;
		}
	else
		{
		if( recipientName == NULL )
			{
			/* No recipient name, get the public key */
			status = getPublicKey( &cryptKey, USER_PRIVKEY_FILE, 
								   USER_PRIVKEY_LABEL );
			if( cryptStatusError( status ) )
				return( FALSE );
			}
		}

	/* Create the envelope, add the public key and originator key if
	   necessary, push in the data, pop the enveloped result, and destroy
	   the envelope */
	if( !createEnvelope( &cryptEnvelope, CRYPT_FORMAT_CMS ) )
		return( FALSE );
	if( recipientName != NULL )
		{
		CRYPT_KEYSET cryptKeyset = externalKeyset;

		/* We're using a recipient name, add the recipient keyset and
		   recipient name */
		if( !useExternalRecipientKeyset )
			{
			status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
									  DATABASE_KEYSET_TYPE, 
									  DATABASE_KEYSET_NAME,
									  CRYPT_KEYOPT_READONLY );
			if( cryptStatusError( status ) )
				{
				fprintf( outputStream, "Couldn't open key database, "
						 "status %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEYSET_ENCRYPT,
								cryptKeyset ) )
			return( FALSE );
		if( !useExternalRecipientKeyset )
			cryptKeysetClose( cryptKeyset );
		if( !addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_RECIPIENT,
							   recipientName, paramStrlen( recipientName ) ) )
			return( FALSE );
		}
	else
		{
		if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_PUBLICKEY,
								cryptKey ) )
			return( FALSE );
		}
	if( externalCryptContext == CRYPT_UNUSED && recipientName == NULL )
		cryptDestroyObject( cryptKey );
	if( useDatasize )
		{
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   ENVELOPE_TESTDATA_SIZE );
		}
	count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
					  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	fprintf( outputStream, "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* Make sure that the enveloped data is valid */
	status = cmsEnvelopeDecrypt( globalBuffer, count, externalKeyset,
								 externalPassword, TRUE );
	if( status <= 0 )	/* Can be FALSE or an error code */
		return( FALSE );

	/* Clean up */
	fputs( "Enveloping of CMS public-key encrypted data succeeded.\n\n", 
		   outputStream );
	return( TRUE );
	}

static int cmsImportEnvelopedData( const char *fileName, const int fileNo )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BYTE *bufPtr = globalBuffer;
	char msgBuffer[ 128 ];
	int count, bytesIn, byteCount = 0, status;

	/* Read the test data */
	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE )
		{
		if( ( bufPtr = malloc( count ) ) == NULL )
			{
			fprintf( outputStream, "Couldn't allocate test buffer of "
					 "%d bytes, lin e%d.\n", count, __LINE__ );
			return( FALSE );
			}
		}
	sprintf( msgBuffer, "S/MIME EnvelopedData #%d", fileNo );
	count = readFileData( fileName, msgBuffer, bufPtr, count, 128, FALSE );
	if( count <= 0 )
		{
		if( bufPtr != globalBuffer )
			free( bufPtr );
		return( count );
		}

	/* Make sure that we can parse the data */
	if( !createDeenvelope( &cryptEnvelope ) )
		{
		if( bufPtr != globalBuffer )
			free( bufPtr );
		return( FALSE );
		}
	status = cryptPushData( cryptEnvelope, bufPtr, count, &bytesIn );
	while( status == CRYPT_ERROR_UNDERFLOW )
		{
		byteCount += bytesIn;
		status = cryptPushData( cryptEnvelope, bufPtr + byteCount,
								count - byteCount, &bytesIn );
		}
	if( bufPtr != globalBuffer )
		free( bufPtr );
	if( cryptStatusError( status ) && status != CRYPT_ENVELOPE_RESOURCE )
		{
		printExtError( cryptEnvelope, "cryptPushData()", status, 
					   __LINE__ );
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( status == CRYPT_ENVELOPE_RESOURCE )
		{
		/* When we get to the CRYPT_ENVELOPE_RESOURCE stage we know that 
		   we've successfully processed all of the recipients because
		   cryptlib is asking us for a key to continue */
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		return( FALSE );

	fputs( "S/MIME EnvelopedData import succeeded.\n\n", outputStream );
	return( TRUE );
	}

int testCMSEnvelopePKCCrypt( void )
	{
	int value, status;

	if( !cmsEnvelopeCrypt( "smi_pkcn", FALSE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_UNUSED, NULL, NULL ) )
		return( FALSE );	/* Standard */
	if( !cmsEnvelopeCrypt( "smi_pkc", TRUE, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_UNUSED, NULL, NULL ) )
		return( FALSE );	/* Datasize hint */

	/* Test enveloping with an IV-less stream cipher, which bypasses the usual
	   CBC-mode block cipher handling.  The alternative way of doing this is
	   to manually add a CRYPT_CTXINFO_SESSIONKEY object, doing it this way is
	   less work */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, &value );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, CRYPT_ALGO_RC4 );
	status = cmsEnvelopeCrypt( "smi_pkcs", TRUE, TRUE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_UNUSED, NULL, NULL );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, value );
	if( !status )			/* Datasize and stream cipher */
		return( status );

	/* Test enveloping with a cipher with a larger-than-usual block size */
	status = cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, &value );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, CRYPT_ALGO_AES );
	status = cmsEnvelopeCrypt( "smi_pkcb", TRUE, FALSE, TRUE, FALSE, CRYPT_UNUSED, CRYPT_UNUSED, NULL, NULL );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, value );
	return( status );		/* Datasize and large blocksize cipher */
	}

int testCMSEnvelopePKCCryptEx( const CRYPT_HANDLE encryptContext,
							   const CRYPT_HANDLE decryptKeyset,
							   const C_STR password, const C_STR recipient )
	{
	int status;

	assert( ( encryptContext != CRYPT_UNUSED && recipient == NULL ) || \
			( encryptContext == CRYPT_UNUSED && recipient != NULL ) );

	/* We can either provide the recipient's key directly as a pubkey
	   context or indirectly as a recipient name.  This is used to test
	   a device's ability to act as a recipient key store */
	status = cmsEnvelopeCrypt( "smi_pkcd", TRUE, FALSE, FALSE, \
							   ( recipient != NULL ) ? TRUE : FALSE, \
							   encryptContext, decryptKeyset, password, \
							   recipient );
	if( status == CRYPT_ERROR_NOTFOUND )
		{					/* Datasize, keys in crypto device */
		fputs( "  (This is probably because the public key certificate was "
			   "regenerated after\n   the certificate stored with the "
			   "private key was created, so that the\n   private key can't "
			   "be identified any more using the public key that was\n   "
			   "used for encryption.  This can happen when the cryptlib "
			   "self-test is run\n   in separate stages, with one stage "
			   "re-using data that was created\n   earlier during a "
			   "previous stage).\n\n", outputStream );
		return( FALSE );
		}
	return( status );
	}

int testCMSEnvelopePKCCryptDoubleCert( void )
	{
	CRYPT_KEYSET cryptKeyset;
	int status;

	/* The dual-certificate test uses cryptlib's internal key management to 
	   read the appropriate certificate from a database keyset, if this 
	   hasn't been set up then the test will fail so we try and detect the 
	   presence of the database keyset here.  This isn't perfect since it 
	   requires that the database keyset be updated with the certificates in 
	   the same run as this test, but it's the best we can do */
	if( !doubleCertOK )
		{
		fputs( "The certificate database wasn't updated with dual encryption/"
			   "signing certs\nduring this test run (either because database "
			   "keysets aren't enabled in this\nbuild of cryptlib or because "
			   "only some portions of the self-tests are being\nrun), "
			   "skipping the test of CMS enveloping with dual certs.\n\n", 
			   outputStream );
		return( TRUE );
		}

	/* Since we're using certs with the same DN and email address present
	   in multiple certs, we can't use the generic user keyset but have to
	   use one that has been set up to have multiple certs that differ
	   only in keyUsage */
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  DUAL_PRIVKEY_FILE, CRYPT_KEYOPT_READONLY );
	if( cryptStatusError( status ) )
		{
		fputs( "Couldn't find keyset with dual encryption/signature certs "
			   "for test of dual certificate\nencryption.\n\n", 
			   outputStream );
		return( FALSE );
		}
	status = cmsEnvelopeCrypt( "smi_pkcr", TRUE, FALSE, FALSE, FALSE, 
							   CRYPT_UNUSED, cryptKeyset, 
							   TEST_PRIVKEY_PASSWORD, 
							   TEXT( "dave@wetaburgers.com" ) );
	cryptKeysetClose( cryptKeyset );
	if( status == CRYPT_ERROR_NOTFOUND )
		{					/* Datasize, recipient */
		fputs( "  (This is probably because the public key certificate was "
			   "regenerated after\n   the certificate stored with the "
			   "private key was created, so that the\n   private key can't "
			   "be identified any more using the public key that was\n   "
			   "used for encryption.  This can happen when the cryptlib "
			   "self-test is run\n   in separate stages, with one stage "
			   "re-using data that was created\n   earlier during a "
			   "previous stage).\n\n", outputStream );
		return( FALSE );
		}
	return( status );
	}

/****************************************************************************
*																			*
*							Test Data Import Routines 						*
*																			*
****************************************************************************/

/* Import S/MIME signed data */

int testCMSEnvelopeSignedDataImport( void )
	{
	FILE *filePtr;
	BYTE fileName[ BUFFER_SIZE ];
	int i;

	/* Make sure that the test data is present so that we can return a
	   useful error message */
	filenameFromTemplate( fileName, SMIME_SIG_FILE_TEMPLATE, 1 );
	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		{
		fputs( "Couldn't find S/MIME SignedData file, skipping test of "
			   "SignedData import...\n\n", outputStream );
		return( TRUE );
		}
	fclose( filePtr );

	/* There are many encoding variations possible for signed data so we try
	   a representative sample to make sure that the code works in all
	   cases */
	for( i = 1; i <= 3; i++ )
		{
		filenameFromTemplate( fileName, SMIME_SIG_FILE_TEMPLATE, i );
		if( !cmsImportSignedData( fileName, i ) && i != 2 )
			{
			/* AuthentiCode sig check fails for some reason */
			return( FALSE );
			}
		}

	fputs( "Import of S/MIME SignedData succeeded.\n\n", outputStream );

	return( TRUE );
	}

/* Import S/MIME encrypted data */

int testCMSEnvelopePKCCryptImport( void )
	{
	FILE *filePtr;
	BYTE fileName[ BUFFER_SIZE ];
	int i;

	/* Make sure that the test data is present so that we can return a
	   useful error message */
	filenameFromTemplate( fileName, SMIME_ENV_FILE_TEMPLATE, 1 );
	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		{
		fputs( "Couldn't find S/MIME EnvelopedData file, skipping test of "
			   "EnvelopedData import...\n\n", outputStream );
		return( TRUE );
		}
	fclose( filePtr );

	/* Enveloped data requires decryption keys that aren't generally
	   available, so all that we do in this case is make sure that we can
	   parse the data */
	for( i = 1; i <= 1; i++ )
		{
		filenameFromTemplate( fileName, SMIME_ENV_FILE_TEMPLATE, i );
		if( !cmsImportEnvelopedData( fileName, i ) )
			return( FALSE );
		}

	fputs( "Import of S/MIME EnvelopedData succeeded.\n\n", outputStream );

	return( TRUE );
	}

/* Import CMS password-encrypted/authenticated data */

int testEnvelopePasswordCryptImport( void )
	{
	FILE *filePtr;
	BYTE fileName[ BUFFER_SIZE ];
	int i;

	/* Make sure that the test data is present so that we can return a
	   useful error message */
	filenameFromTemplate( fileName, CMS_ENC_FILE_TEMPLATE, 1 );
	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		{
		fputs( "Couldn't find CMS EnvelopedData file, skipping test of "
			   "EnvelopedData/AuthentcatedData import...\n\n", 
			   outputStream );
		return( TRUE );
		}
	fclose( filePtr );

	/* Process the password-protected data.  The files are:

		File 1: CMS Authenticated data, AES-256 key wrap with HMAC-SHA1.

		File 2: CMS Enveloped data, AES-128 key wrap with AES-128 */
	for( i = 1; i <= 2; i++ )
		{
		int count;

		count = readFileFromTemplate( CMS_ENC_FILE_TEMPLATE, i, 
									  "CMS password-encrypted/authd data",
									  globalBuffer, BUFFER_SIZE );
		if( count <= 0 )
			return( FALSE );
		if( !cmsEnvelopeDecrypt( globalBuffer, count, CRYPT_UNUSED, 
								 TEST_PASSWORD, FALSE ) )
			return( FALSE );
		}

	fputs( "Import of CMS password-encrypted/authenticated data "
		   "succeeded.\n\n", outputStream );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated password-encrypted data */

int testPGPEnvelopePasswordCryptImport( void )
	{
	int count;

	/* Process the PGP 2.x data: IDEA with MD5 hash.  Create with:

		pgp -c +compress=off -o conv_enc1.pgp test.txt */
#ifdef USE_IDEA
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 1, 
								  "PGP password-encrypted data",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePasswordDecrypt( globalBuffer, count );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	fputs( "Import of PGP password-encrypted data succeeded.\n", 
		   outputStream );
#endif /* USE_IDEA */

	/* Process the OpenPGP data, all with password "password".  The files are:
	
		File 2: 3DES with SHA1 hash.  Create with:

			pgp65 -c +compress=off -o conv_enc2.pgp test.txt 
	
		File 3: 3DES with SHA1 non-iterated hash.  Create with:

			gpg -c -z 0 --cipher-algo 3des --s2k-mode 1 -o conv_enc3.pgp test.txt 
			
		File 4: AES without MDC.  Create with

			gpg -c -z 0 --disable-mdc --cipher-algo aes -o conv_enc4.pgp test.txt 

		File 5: AES with MDC.  Create with:

			gpg -c -z 0 --cipher-algo aes -o conv_enc5.pgp test.txt

		File 6: AES with MDC and indefinite inner length.  Create with:

			gpg -c --cipher-algo aes -o conv_enc5.pgp test.txt

		File 7: 3DES with partial lengths.  Create with:

			cat test.c | gpg -c -z 0 --cipher-algo 3des -o conv_enc6.pgp
			
			Note that this requires using a file with a minimum length of a 
			few hundred bytes, since anything shorter is encoded into a 
			fixed-length packet.
			
		Since the inner content for files 6 and 7 are indefinite-length, we 
		need to process that in a separate pass, which we do by handing it to 
		the compressed-data de-enveloping code, this is really a general 
		"process non-encrypted content" function so we get back the inner 
		content */
#ifdef USE_3DES
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 2, 
								  "OpenPGP password-encrypted data (3DES)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	if( !envelopePasswordDecrypt( globalBuffer, count ) )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 3, 
								  "OpenPGP password-encrypted data (3DES+non-iterated hash)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	if( !envelopePasswordDecrypt( globalBuffer, count ) )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
#endif /* USE_3DES */
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 4, 
								  "OpenPGP password-encrypted data (AES)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	if( !envelopePasswordDecrypt( globalBuffer, count ) )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 5, 
								  "OpenPGP password-encrypted data (AES+MDC)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	if( !envelopePasswordDecrypt( globalBuffer, count ) )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 6, 
								  "OpenPGP password-encrypted data (AES+MDC, indef-len inner)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePasswordDecrypt( globalBuffer, count );
	if( count <= 0 )
		return( FALSE );
	count = envelopeDecompress( globalBuffer, BUFFER_SIZE, count );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
#ifdef USE_3DES
	count = readFileFromTemplate( PGP_ENC_FILE_TEMPLATE, 7, 
								  "OpenPGP password-encrypted data (partial lengths)",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePasswordDecrypt( globalBuffer, count );
	if( count <= 0 )
		return( FALSE );
	count = envelopeDecompress( globalBuffer, BUFFER_SIZE, count );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_COMPRESSEDDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, ENVELOPE_TESTDATA_SIZE ) )
		return( FALSE );
#endif /* USE_3DES */
	fputs( "Import of OpenPGP password-encrypted data succeeded.\n\n", 
		   outputStream );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated PKC-encrypted data */

int testPGPEnvelopePKCCryptImport( void )
	{
#if defined( USE_PGP2 ) || defined( USE_3DES ) || defined( USE_BLOWFISH )
	int count;
#endif /* USE_PGP2 || USE_3DES || USE_BLOWFISH */

	/* Process the PGP 2.x encrypted data */
#ifdef USE_PGP2
	count = readFileFromTemplate( PGP_PKE_FILE_TEMPLATE, 1, 
								  "PGP-encrypted data", globalBuffer,
								  BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_PGP, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	count = readFileFromTemplate( PGP_PKE_FILE_TEMPLATE, 2, 
								  "PGP (NAI)-encrypted data", globalBuffer,
								  BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_NAIPGP, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( globalBuffer[ 0 ] != 0xA3 || globalBuffer[ 1 ] != 0x01 || \
		globalBuffer[ 2 ] != 0x5B || globalBuffer[ 3 ] != 0x53 )
		{
		fputs( "De-enveloped data != original data.\n\n", outputStream );
		return( FALSE );
		}
	fputs( "Import of PGP-encrypted data succeeded.\n\n", outputStream );
#endif /* USE_PGP2 */

	/* Process the OpenPGP encrypted data.  The files are:

		File 1: RSA with 3DES.  Create with:

			pgpo -e +pubring=".\pubring.pgp" +compress=off -o gpg_enc1.pgp -r test test.txt

			(In theory it could be created with
			gpg -e -z 0 --homedir . --always-trust -r test test.txt
			and pubring.pgp renamed to pubring.gpg, however GPG won't allow 
			the use of the RSA key in pubring.pgp no matter what command-
			line options you specify because it's not self-signed).

		File 2: Elgamal and AES without MDC.  Create with:

			cp sec_hash.gpg secring.gpg
			cp pub_hash.gpg pubring.gpg
			gpg -e --disable-mdc -z 0 --homedir . -o gpg_enc2.gpg -r test1 test.txt
			rm secring.gpg pubring.gpg

		File 3: Elgamal and AES with MDC.  Create with:

			cp sec_hash.gpg secring.gpg
			cp pub_hash.gpg pubring.gpg
			gpg -e -z 0 --homedir . -o gpg_enc3.gpg -r test1 test.txt
			rm secring.gpg pubring.gpg

		File 4: Elgamal and Blowfish with MDC.  Create with:

			cp sec_hash.gpg secring.gpg
			cp pub_hash.gpg pubring.gpg
			gpg -e -z 0 --homedir . --cipher-algo blowfish -o gpg_enc4.gpg -r test1 test.txt
			rm secring.gpg pubring.gpg
		
		File 5: Elgamal and AES with MDC and partial lengths (not sure how 
				this was created) */
#if defined( USE_PGP2 ) && defined( USE_3DES )	/* Uses PGP 2.x private keyring */
	count = readFileFromTemplate( OPENPGP_PKE_FILE_TEMPLATE, 1, 
								  "OpenPGP (GPG)-encrypted data",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_PGP, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
#endif /* USE_PGP2 && USE_3DES */
#ifdef USE_3DES		/* Uses 3DES-encrypted keyring */
	count = readFileFromTemplate( OPENPGP_PKE_FILE_TEMPLATE, 2, 
								  "OpenPGP (GPG)-encrypted data (AES)", 
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_OPENPGP_HASH, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	count = readFileFromTemplate( OPENPGP_PKE_FILE_TEMPLATE, 3, 
								  "OpenPGP (GPG)-encrypted data (AES+MDC)", 
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_OPENPGP_HASH, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
#endif /* USE_3DES */
#ifdef USE_BLOWFISH		/* Uses Blowfish for encryption */
	count = readFileFromTemplate( OPENPGP_PKE_FILE_TEMPLATE, 4, 
								  "OpenPGP (GPG)-encrypted data (Blowfish+MDC)", 
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_OPENPGP_HASH, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
#endif /* USE_BLOWFISH */
#ifdef USE_3DES		/* Uses 3DES-encrypted keyring */
	count = readFileFromTemplate( OPENPGP_PKE_FILE_TEMPLATE, 5, 
								  "OpenPGP (GPG)-encrypted data (partial lengths)", 
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopePKCDecrypt( globalBuffer, count, KEYFILE_OPENPGP_PARTIAL, 
								CRYPT_UNUSED );
	if( count <= 0 )
		return( FALSE );
	count = envelopeDecompress( globalBuffer, BUFFER_SIZE, count );
	if( count <= 0 )
		return( FALSE );
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( resume )
#endif /* IBM big iron */
	if( count < 600 || \
		!compareData( "\n\n<sect>Vorwort\n", 16, globalBuffer, 16 ) )
		return( FALSE );
#if defined( __MVS__ ) || defined( __VMCMS__ )
  #pragma convlit( suspend )
#endif /* IBM big iron */
#endif /* USE_3DES */
	fputs( "Import of OpenPGP-encrypted data succeeded.\n\n", 
		   outputStream );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated signed data */

int testPGPEnvelopeSignedDataImport( void )
	{
	CRYPT_CONTEXT hashContext;
	int count, status;

	/* Process the PGP 2.x signed data.  Create with:

		pgp -s +secring="secring.pgp" +pubring="pubring.pgp" -u test test.txt */
#ifdef USE_PGP2
	count = readFileFromTemplate( PGP_SIG_FILE_TEMPLATE, 1, 
								  "PGP-signed data", globalBuffer,
								  BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  CRYPT_UNUSED, KEYFILE_PGP, FALSE, 
							  CRYPT_FORMAT_PGP );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	fputs( "Import of PGP-signed data succeeded.\n\n", outputStream );
#endif /* USE_PGP2 */

#if 0	/* Disabled because it uses a 512-bit sig and there doesn't appear to
		   be any way to create a new file in this format */
	/* Process the OpenPGP (actually a weird 2.x/OpenPGP hybrid produced by
	   PGP 5.0) signed data.  In theory this could be created with:

		pgpo -s +secring="secring.pgp" +pubring="pubring.pgp" +compress=off -o signed2.pgp -u test test.txt
	
	   but the exact details of how to create this packet are a mystery, it 
	   starts with a marker packet and then a one-pass sig, which isn't 
	   generated by default by any of the PGP 5.x or 6.x versions */
	count = readFileFromTemplate( PGP_SIG_FILE_TEMPLATE, 2, 
								  "PGP 2.x/OpenPGP-hybrid-signed data",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  CRYPT_UNUSED, TRUE, FALSE, FALSE,
							  CRYPT_FORMAT_PGP );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	fputs( "Import of PGP 2.x/OpenPGP-hybrid-signed data succeeded.\n\n", 
		   outputStream );
#endif /* 0 */

	/* Process the OpenPGP signed data: DSA with SHA1.  Create with:

		cp sec_hash.gpg secring.gpg
		cp pub_hash.gpg pubring.gpg
		gpg -s -z 0 --homedir . -o signed3.pgp test.txt 
		rm secring.gpg pubring.gpg */
	count = readFileFromTemplate( PGP_SIG_FILE_TEMPLATE, 3, 
								  "OpenPGP-signed data",
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  CRYPT_UNUSED, KEYFILE_OPENPGP_HASH, FALSE, 
							  CRYPT_FORMAT_PGP );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_PGP_TESTDATA, ENVELOPE_TESTDATA_SIZE, 
					  globalBuffer, count ) )
		return( FALSE );
	fputs( "Import of OpenPGP-signed data succeeded.\n\n", outputStream );

	/* Process the OpenPGP detached signed data.  Create with:

		cp sec_hash.gpg secring.gpg
		cp pub_hash.gpg pubring.gpg
		gpg -b -z 0 --openpgp --homedir . -o signed4.pgp test.txt
		rm secring.gpg pubring.gpg 

	   (the --openpgp is for another GPG bug, without it it'll generate
	   v3 sigs as if --force-v3-sigs had been specified).  The data is 
	   provided externally so we have to hash it ourselves.  Since PGP 
	   hashes further data after hashing the content, we can't complete 
	   the hashing but have to use the partially-completed hash */
	status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_SHA1 );
	if( cryptStatusOK( status ) )
		status = cryptEncrypt( hashContext, ENVELOPE_PGP_TESTDATA,
							   ENVELOPE_TESTDATA_SIZE );
	if( cryptStatusError( status ) )
		{
		fputs( "Couldn't create external hash of data.\n\n", outputStream );
		return( FALSE );
		}
	count = readFileFromTemplate( PGP_SIG_FILE_TEMPLATE, 4, 
								  "OpenPGP-signed data with externally-supplied hash", 
								  globalBuffer, BUFFER_SIZE );
	if( count <= 0 )
		return( FALSE );
	count = envelopeSigCheck( globalBuffer, count, hashContext,
							  CRYPT_UNUSED, KEYFILE_OPENPGP_HASH, TRUE,
							  CRYPT_FORMAT_PGP );
	cryptDestroyContext( hashContext );
	if( count <= 0 )
		return( FALSE );
	fputs( "Import of OpenPGP-signed data with externally-supplied hash "
		   "succeeded.\n\n", outputStream );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated compressed data */

int testPGPEnvelopeCompressedDataImport( void )
	{
	BYTE *bufPtr;
	int count;

	/* Since this needs a nontrivial amount of data for the compression, we
	   use a dynamically-allocated buffer */
	if( ( bufPtr = malloc( FILEBUFFER_SIZE ) ) == NULL )
		{
		fputs( "Couldn't allocate test buffer.\n\n", outputStream );
		return( FALSE );
		}

	/* Process the PGP 2.x compressed data */
	count = readFileFromTemplate( PGP_COPR_FILE_TEMPLATE, 1, 
								  "PGP 2.x compressed data",
								  bufPtr, FILEBUFFER_SIZE );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	count = envelopeDecompress( bufPtr, FILEBUFFER_SIZE, count );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	if( !compareData( bufPtr, ENVELOPE_COMPRESSEDDATA_SIZE, 
					  ENVELOPE_COMPRESSEDDATA, 
					  ENVELOPE_COMPRESSEDDATA_SIZE ) )
		{
		free( bufPtr );
		return( FALSE );
		}
	fputs( "Import of PGP 2.x compressed data succeeded.\n\n", 
		   outputStream );

	/* Process the OpenPGP compressed nested data.  GPG is weird in that 
	   instead of signing compressed data it first creates the signature
	   and then compresses that, so that the result isn't { one-pass sig,
	   copr.data, signature } but { copr.data { one-pass sig, data, 
	   signature } }.  Create with:

		cp sec_hash.gpg secring.gpg
		cp pub_hash.gpg pubring.gpg
		gpg -s --openpgp --homedir . -o copr2.pgp test.c
		rm secring.gpg pubring.gpg
	
	   with the same note as before for GPG bugs and --openpgp */
	count = readFileFromTemplate( PGP_COPR_FILE_TEMPLATE, 2, 
								  "OpenPGP compressed signed data",
								  bufPtr, FILEBUFFER_SIZE );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	count = envelopeDecompress( bufPtr, FILEBUFFER_SIZE, count );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	if( !compareData( "\x90\x0D\x03\x00", 4, bufPtr, 4 ) )
		{
		free( bufPtr );
		return( FALSE );
		}
	memcpy( globalBuffer, bufPtr, count );
	debugDump( "decopr_sig.pgp", globalBuffer, count );
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  CRYPT_UNUSED, KEYFILE_OPENPGP_HASH, FALSE,
							  CRYPT_FORMAT_PGP );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	if( !compareData( ENVELOPE_COMPRESSEDDATA, ENVELOPE_COMPRESSEDDATA_SIZE, 
					  globalBuffer, ENVELOPE_COMPRESSEDDATA_SIZE ) )
		{
		free( bufPtr );
		return( FALSE );
		}
	fputs( "Import of OpenPGP compressed signed data succeeded.\n\n", 
		   outputStream );

	/* Process booby-traped compressed data import.  This is a PGP quine 
	   which decompresses to itself, which crashes some PGP implementations, 
	   see https://nvd.nist.gov/vuln/detail/CVE-2013-4402 for a discussion 
	   and http://mumble.net/%7Ecampbell/misc/pgp-quine/ for the data.
	   This doesn't affect cryptlib since it only removes one layer at a 
	   time, but we test it anyway just to show it's not affected.
	   
	   Since this is custom-generated malformed data, there's a spurious
	   byte at the end so we end up with a CRYPT_ERROR_INCOMPLETE when we 
	   try and destroy the envelope, this is expected behaviour */
	count = readFileFromTemplate( PGP_COPR_FILE_TEMPLATE, 3, 
								  "PGP quine compressed data", 
								  bufPtr, FILEBUFFER_SIZE );
	if( count <= 0 )
		{
		free( bufPtr );
		return( FALSE );
		}
	count = envelopeDecompress( bufPtr, FILEBUFFER_SIZE, count );
	if( count > 0 )
		{
		free( bufPtr );
		fputs( "Import of PGP quine succeeded, should have failed.\n\n", 
			   outputStream );
		return( FALSE );
		}
	free( bufPtr );
	fputs( "  (This is an expected result from importing invalid data).\n", 
		   outputStream );
	fputs( "Import of PGP quine compressed data succeeded.\n\n", 
		   outputStream );

	return( TRUE );
	}

/****************************************************************************
*																			*
*						Debugging Data Import Routines 						*
*																			*
****************************************************************************/

/* Generic test routines used for debugging.  These are only meant to be
   used interactively, and throw exceptions rather than returning status
   values */

static void dataImport( void *buffer, const int length, 
						const BOOLEAN resultBad )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count, status;

	status = createDeenvelope( &cryptEnvelope );
	assert( status == TRUE );
	count = pushData( cryptEnvelope, buffer, length, NULL, 0 );
	if( resultBad )
		{
		assert( cryptStatusError( count ) );
		return;
		}
	assert( !cryptStatusError( count ) );
	count = popData( cryptEnvelope, buffer, length );
	assert( !cryptStatusError( count ) );
	status = destroyEnvelope( cryptEnvelope );
	assert( status == TRUE );
	}

void xxxDataImport( const char *fileName )
	{
	BYTE *bufPtr = globalBuffer;
	int count;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE && \
		( bufPtr = malloc( count ) ) == NULL )
		{
		assert( 0 );
		return;
		}
	count = readFileData( fileName, "Generic test data", bufPtr, count, 32,
						  FALSE );
	assert( count > 0 );
	dataImport( bufPtr, count, FALSE );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	}

static void signedDataImport( const BYTE *data, const int length )
	{
	int status;

	status = cmsEnvelopeSigCheck( data, length, CRYPT_UNUSED, CRYPT_UNUSED, 
								  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE );
	assert( status > 0 );
	}

void xxxSignedDataImport( const char *fileName )
	{
	BYTE *bufPtr = globalBuffer;
	int count;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE && \
		( bufPtr = malloc( count ) ) == NULL )
		{
		assert( 0 );
		return;
		}
	count = readFileData( fileName, "S/MIME test data", bufPtr, count, 64,
						  FALSE );
	assert( count > 0 );
	signedDataImport( bufPtr, count );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	}

static void encryptedDataImport( const BYTE *data, const int length,
								 const char *keyset, const char *password )
	{
	CRYPT_KEYSET cryptKeyset;
	int status;

	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  keyset, CRYPT_KEYOPT_READONLY );
	assert( cryptStatusOK( status ) );
	status = cmsEnvelopeDecrypt( data, length, cryptKeyset, password, FALSE );
	assert( status > 0 );
	cryptKeysetClose( cryptKeyset );
	}

void xxxEncryptedDataImport( const char *fileName, const char *keyset,
							 const char *password )
	{
	BYTE *buffer = globalBuffer;
	int count;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE )
		{
		buffer = malloc( count );
		assert( buffer != NULL );
		}
	count = readFileData( fileName, "S/MIME test data", buffer, 32,
						  count, FALSE );
	assert( count > 0 );
	encryptedDataImport( buffer, count, keyset, password );
	if( buffer != globalBuffer )
		free( buffer );
	}

void xxxSignedEncryptedDataImport( const char *fileName, const char *keyset,
								   const char *password )
	{
	BYTE *bufPtr = globalBuffer;
	int count;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE && \
		( bufPtr = malloc( count ) ) == NULL )
		{
		assert( 0 );
		return;
		}
	count = readFileData( fileName, "S/MIME test data", bufPtr, count, 64,
						  FALSE );
	assert( count > 0 );
	signedDataImport( bufPtr, count );
	encryptedDataImport( bufPtr, count, keyset, password );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	}

void xxxEnvelopeTest( const char *certFileName, const char *outFile )
	{
	CRYPT_CERTIFICATE cryptCert;
	int status;

	status = importCertFile( &cryptCert, certFileName );
	assert( cryptStatusOK( status ) );
	status = envelopePKCCrypt( outFile, TRUE, KEYFILE_NONE, FALSE,
							   FALSE, FALSE, TRUE, CRYPT_FORMAT_CMS, 
							   cryptCert, 0 );
	assert( status > 0 );
	}
#endif /* TEST_ENVELOPE || TEST_SESSION */
