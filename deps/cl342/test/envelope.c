/****************************************************************************
*																			*
*						cryptlib Enveloping Test Routines					*
*						Copyright Peter Gutmann 1996-2011					*
*																			*
****************************************************************************/

#include <limits.h>		/* To determine max.buffer size we can encrypt */
#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

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

BYTE FAR_BSS globalBuffer[ BUFFER_SIZE ];

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

static int readFileData( const char *fileName, const char *description,
						 BYTE *buffer, const int bufSize )
	{
	FILE *filePtr;
	int count;

	if( ( filePtr = fopen( fileName, "rb" ) ) == NULL )
		{
		printf( "Couldn't find %s file, skipping test of data import...\n",
				description );
		return( 0 );
		}
	printf( "Testing %s import...\n", description );
	count = fread( buffer, 1, bufSize, filePtr );
	fclose( filePtr );
	if( count >= bufSize )
		{
		puts( "The data buffer size is too small for the data.  To fix this, "
			  "either increase\nthe BUFFER_SIZE value in " __FILE__ " and "
			  "recompile the code, or use the\ntest code with dynamically-"
			  "allocated buffers." );
		return( 0 );		/* Skip this test and continue */
		}
	if( count < 16 )
		{
		printf( "Read failed, only read %d bytes.\n", count );
		return( 0 );		/* Skip this test and continue */
		}
	printf( "%s has size %d bytes.\n", description, count );
	return( count );
	}

static int readFileFromTemplate( const C_STR fileTemplate, const int count,
								 const char *description, BYTE *buffer, 
								 const int bufSize )
	{
	BYTE fileName[ BUFFER_SIZE ];

	filenameFromTemplate( fileName, fileTemplate, count );
	return( readFileData( fileName, description, buffer, bufSize ) );
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
		printf( "cryptCreateEnvelope() failed with error code %d, line %d.\n",
				status, __LINE__ );
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
		printf( "cryptCreateEnvelope() failed with error code %d, line %d.\n",
				status, __LINE__ );
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
		printf( "cryptSetAttributeString() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
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
		printf( "cryptSetAttribute() of %d failed with error code %d, "
				"line %d.\n", type, status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

static int processEnvelopeResource( const CRYPT_ENVELOPE envelope, 
									const void *stringEnvInfo,
									const int numericEnvInfo,
									BOOLEAN *isRestartable )
	{
	int cryptEnvInfo, cryptAlgo, keySize = DUMMY_INIT;
	int integrityLevel, status;

	/* Clear return value */
	*isRestartable = FALSE;
	
	/* Add the appropriate enveloping information that we need to 
	   continue */
	status = cryptSetAttribute( envelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
								CRYPT_CURSOR_FIRST );
	if( cryptStatusError( status ) )
		{
		printf( "Attempt to move cursor to start of list failed with error "
				"code %d, line %d.\n", status, __LINE__ );
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
			printf( "Attempt to read current group failed with error code "
					"%d, line %d.\n", status, __LINE__ );
			return( status );
			}

		switch( cryptEnvInfo )
			{
			case CRYPT_ATTRIBUTE_NONE:
				/* The required information was supplied via other means (in 
				   practice this means there's a crypto device available and 
				   that was used for the decrypt), there's nothing left to 
				   do */
				puts( "(Decryption key was recovered using crypto device or "
					  "non-password-protected\n private key)." );
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
						printf( "Attempt to add private key failed with "
								"error code %d, line %d.\n", status, 
								__LINE__ );
						return( status );
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
					printf( "Private key label read failed with error code "
							"%d, line %d.\n", status, __LINE__ );
					return( status );
					}
#ifdef UNICODE_STRINGS
				label[ labelLength / sizeof( wchar_t ) ] = '\0';
				printf( "Need password to decrypt private key '%S'.\n", 
						label );
#else
				label[ labelLength ] = '\0';
				printf( "Need password to decrypt private key '%s'.\n",
						label );
#endif /* UNICODE_STRINGS */
				if( !addEnvInfoString( envelope, CRYPT_ENVINFO_PASSWORD,
								stringEnvInfo, paramStrlen( stringEnvInfo ) ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_PASSWORD:
				puts( "Need user password." );
				if( !addEnvInfoString( envelope, CRYPT_ENVINFO_PASSWORD,
							stringEnvInfo, paramStrlen( stringEnvInfo ) ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_SESSIONKEY:
				puts( "Need session key." );
				if( !addEnvInfoNumeric( envelope, CRYPT_ENVINFO_SESSIONKEY,
										numericEnvInfo ) )
					return( SENTINEL );
				*isRestartable = TRUE;
				break;

			case CRYPT_ENVINFO_KEY:
				puts( "Need conventional encryption key." );
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
				printf( "Need unknown enveloping information type %d.\n",
						cryptEnvInfo );
				return( SENTINEL );
			}
		}
	while( cryptSetAttribute( envelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							  CRYPT_CURSOR_NEXT ) == CRYPT_OK );

	/* Check whether there's any integrity protection present */
	status = cryptGetAttribute( envelope, CRYPT_ENVINFO_INTEGRITY, 
								&integrityLevel );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't query integrity protection used in envelope, "
				"status %d, line %d.\n", status, __LINE__ );
		return( status );
		}
	if( integrityLevel > CRYPT_INTEGRITY_NONE )
		{
		/* Display the integrity level.  For PGP it's not really a MAC but
		   a sort-of-keyed encrypted hash, but we call it a MAC to keep 
		   things simple */
		printf( "Data is integrity-protected using %s authentication.\n",
				( integrityLevel == CRYPT_INTEGRITY_MACONLY ) ? \
					"MAC" : \
				( integrityLevel == CRYPT_INTEGRITY_FULL ) ? \
					"MAC+encrypt" : "unknown" );
		}

	/* If we're not using encryption, we're done */
	if( cryptEnvInfo != CRYPT_ATTRIBUTE_NONE && \
		cryptEnvInfo != CRYPT_ENVINFO_PRIVATEKEY && \
		cryptEnvInfo != CRYPT_ENVINFO_PASSWORD )
		return( CRYPT_OK );
	if( integrityLevel == CRYPT_INTEGRITY_MACONLY )
		return( CRYPT_OK );

	/* If we're using some form of encrypted enveloping, report the 
	   algorithm and keysize used */
	status = cryptGetAttribute( envelope, CRYPT_CTXINFO_ALGO, &cryptAlgo );
	if( cryptStatusOK( status ) )
		status = cryptGetAttribute( envelope, CRYPT_CTXINFO_KEYSIZE, 
									&keySize );
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't query encryption algorithm and keysize used in "
				"envelope, status %d, line %d.\n", status, __LINE__ );
		return( status );
		}
	printf( "Data is protected using algorithm %d with %d bit key.\n", 
			cryptAlgo, keySize * 8 );
	
	return( CRYPT_OK );
	}

static int pushData( const CRYPT_ENVELOPE envelope, const BYTE *buffer,
					 const int length, const void *stringEnvInfo,
					 const int numericEnvInfo )
	{
	int bytesIn, contentType, status;

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
				printf( "cryptPushData() for remaining data failed with "
						"error code %d, line %d.\n", status, __LINE__ );
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
		printf( "(Ran out of input buffer data space, popping %d bytes to "
				"make room).\n", 8192 );
		status = cryptPopData( envelope, tempBuffer, 8192, &bytesOut );
		if( cryptStatusError( status ) )
			{
			printf( "cryptPopData() to make way for remaining data failed "
					"with error code %d, line %d.\n", status, __LINE__ );
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
		printf( "cryptPushData() only copied %d of %d bytes, line %d.\n",
				bytesIn, length, __LINE__ );
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
			puts( "(Data is from a detached signature, couldn't determine "
				  "content type)." );
			return( bytesIn );
			}
		printf( "Couldn't query content type in envelope, status %d, "
				"line %d.\n", status, __LINE__ );
		return( status );
		}
	if( contentType != CRYPT_CONTENT_DATA )
		printf( "Nested content type = %d.\n", contentType );

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
		printf( "cryptDestroyEnvelope() failed with error code %d, line %d.\n",
				status, __LINE__ );
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

static int envelopeData( const char *dumpFileName,
						 const BOOLEAN useDatasize,
						 const int bufferSize,
						 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BYTE *inBufPtr, *outBufPtr = globalBuffer;
	int length, bufSize, count;

	switch( bufferSize )
		{
		case 0:
			printf( "Testing %splain data enveloping%s...\n",
					( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
					( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
					" with datasize hint" : "" );
			length = ENVELOPE_TESTDATA_SIZE;
			inBufPtr = ENVELOPE_TESTDATA;
			break;

		case 1:
			printf( "Testing %splain data enveloping of intermediate-size data...\n",
					( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );
			length = 512;
			inBufPtr = globalBuffer;
			for( count = 0; count < length; count++ )
				inBufPtr[ count ] = count & 0xFF;
			break;

		case 2:
			printf( "Testing %senveloping of large data quantity...\n",
					( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );

			/* Allocate a large buffer and fill it with a known value */
			length = ( INT_MAX <= 32768L ) ? 16384 : 1048576;
			if( ( inBufPtr = malloc( length + 128 ) ) == NULL )
				{
				printf( "Couldn't allocate buffer of %d bytes, skipping large "
						"buffer enveloping test.\n", length );
				return( TRUE );
				}
			outBufPtr = inBufPtr;
			for( count = 0; count < length; count++ )
				inBufPtr[ count ] = count & 0xFF;
			break;

		default:
			return( FALSE );
		}
	bufSize = length + 128;

	/* Create the envelope, push in the data, pop the enveloped result, and
	   destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( useDatasize )
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, length );
	if( bufferSize > 1 )
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
	count = pushData( cryptEnvelope, inBufPtr, length, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( bufferSize == 0 && \
		count != length + ( ( formatType == CRYPT_FORMAT_PGP ) ? 8 : \
							useDatasize ? 17 : 25 ) )
		{
		printf( "Enveloped data length %d, should be %d, line %d.\n",
				count, length + 25, __LINE__ );
		return( FALSE );
		}

	/* Tell them what happened */
	printf( "Enveloped data has size %d bytes.\n", count );
	if( bufferSize < 2 )
		debugDump( dumpFileName, outBufPtr, count );

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( bufferSize > 1 )
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
	count = pushData( cryptEnvelope, outBufPtr, count, NULL, 0 );
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
		puts( "De-enveloped data length != original length." );
		return( FALSE );
		}
	if( bufferSize > 0 )
		{
		int i;

		for( i = 0; i < length; i++ )
			if( outBufPtr[ i ] != ( i & 0xFF ) )
			{
			printf( "De-enveloped data != original data at byte %d, "
					"line %d.\n", i, __LINE__ );
			return( FALSE );
			}
		}
	else
		{
		if( !compareData( ENVELOPE_TESTDATA, length, outBufPtr, length ) )
			return( FALSE );
		}

	/* Clean up */
	if( bufferSize > 1 )
		free( inBufPtr );
	puts( "Enveloping of plain data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopeData( void )
	{
	if( !envelopeData( "env_datn", FALSE, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite-length */
	if( !envelopeData( "env_dat", TRUE, 0, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	if( !envelopeData( "env_dat.pgp", TRUE, 0, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format */
	return( envelopeData( "env_datl.pgp", TRUE, 1, CRYPT_FORMAT_PGP ) );
	}						/* PGP format, longer data */

int testEnvelopeDataLargeBuffer( void )
	{
	if( !envelopeData( NULL, TRUE, 2, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, large buffer */
	return( envelopeData( NULL, TRUE, 2, CRYPT_FORMAT_PGP ) );
	}						/* Large buffer, PGP format */

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
			puts( "Older HPUX compilers break zlib, to remedy this you can "
				  "either get a better\ncompiler/OS or grab a debugger and "
				  "try to figure out what HPUX is doing to\nzlib.  To "
				  "continue the self-tests, comment out the call to\n"
				  "testEnvelopeCompress() and rebuild." );
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
		printf( "Attempt to pop more data after end-of-data had been "
				"reached succeeded, the\nenvelope should have reported 0 "
				"bytes available rather than %d.\n", zeroCount );
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

	printf( "Testing %scompressed data enveloping%s...\n",
			( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
			useDatasize ? " with datasize hint" : ""  );

	/* Since this needs a nontrivial amount of data for the compression, we
	   read it from an external file into dynamically-allocated buffers */
	if( ( ( buffer = malloc( FILEBUFFER_SIZE ) ) == NULL ) || \
		( ( envelopedBuffer = malloc( FILEBUFFER_SIZE ) ) == NULL ) )
		{
		if( buffer != NULL )
			free( buffer );
		puts( "Couldn't allocate test buffers." );
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
		puts( "Couldn't read test file for compression." );
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
		printf( "Attempt to enable compression failed, status = %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( useDatasize )
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, dataCount );
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
		printf( "Compression of data failed, %d bytes in -> %d bytes out, "
				"line %d.\n", dataCount, count, __LINE__ );
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
	printf( "Enveloped data has size %d bytes.\n", count );
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
	puts( "Enveloping of compressed data succeeded.\n" );
	free( buffer );
	free( envelopedBuffer );
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
								 const BOOLEAN useLargeBuffer,
								 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext;
	CRYPT_ALGO_TYPE cryptAlgo = ( formatType == CRYPT_FORMAT_PGP ) ? \
								selectCipher( CRYPT_ALGO_IDEA ) : \
								selectCipher( CRYPT_ALGO_AES );
	BYTE *inBufPtr = ENVELOPE_TESTDATA, *outBufPtr = globalBuffer;
#if defined( __MSDOS16__ ) || defined( __WIN16__ )
	const int length = useLargeBuffer ? 16384 : ENVELOPE_TESTDATA_SIZE;
#else
	const int length = useLargeBuffer ? 1048576L : ENVELOPE_TESTDATA_SIZE;
#endif /* 16- vs.32-bit systems */
	const int bufSize = length + 128;
	int count, status;

	if( useLargeBuffer )
		{
		int i;

		printf( "Testing %sraw-session-key encrypted enveloping of large "
				"data quantity...\n",
				( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "" );

		/* Allocate a large buffer and fill it with a known value */
		if( ( inBufPtr = malloc( bufSize ) ) == NULL )
			{
			printf( "Couldn't allocate buffer of %d bytes, skipping large "
					"buffer enveloping test.\n", length );
			return( TRUE );
			}
		outBufPtr = inBufPtr;
		for( i = 0; i < length; i++ )
			inBufPtr[ i ] = i & 0xFF;
		}
	else
		{
		printf( "Testing %sraw-session-key encrypted enveloping%s...\n",
				( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
				( useStreamCipher ) ? " with stream cipher" : \
				( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
				" with datasize hint" : "" );
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
			puts( "Can't test PGP enveloping because the IDEA algorithm "
				  "isn't available in this\nbuild of cryptlib.\n" );
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
	if( useDatasize && !useLargeBuffer )
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
			printf( "Addition of duplicate session key succeeded when it "
					"should have failed,\nline %d.\n", __LINE__ );
			return( FALSE );
			}
		puts( "  (The above message indicates that the test condition was "
			  "successfully\n   checked)." );
		}
	if( useDatasize )
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE, length );
	if( useLargeBuffer )
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
	count = pushData( cryptEnvelope, inBufPtr, length, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, outBufPtr, bufSize );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	printf( "Enveloped data has size %d bytes.\n", count );
	if( !useLargeBuffer )
		debugDump( dumpFileName, outBufPtr, count );

	/* Create the envelope, push in the data, pop the de-enveloped result,
	   and destroy the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );
	if( useLargeBuffer )
		cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_BUFFERSIZE,
						   length + 1024 );
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
		puts( "De-enveloped data length != original length." );
		return( FALSE );
		}
	if( useLargeBuffer )
		{
		int i;

		for( i = 0; i < length; i++ )
			if( outBufPtr[ i ] != ( i & 0xFF ) )
			{
			printf( "De-enveloped data != original data at byte %d, "
					"line %d.\n", i, __LINE__ );
			return( FALSE );
			}
		}
	else
		{
		if( !compareData( ENVELOPE_TESTDATA, length, outBufPtr, length ) )
			return( FALSE );
		}

	/* Clean up */
	if( useLargeBuffer )
		free( inBufPtr );
	cryptDestroyContext( cryptContext );
	puts( "Enveloping of raw-session-key-encrypted data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopeSessionCrypt( void )
	{
	if( !envelopeSessionCrypt( "env_sesn", FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeSessionCrypt( "env_ses", TRUE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize */
	if( !envelopeSessionCrypt( "env_ses", TRUE, TRUE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, stream cipher */
#if 0
	/* Although in theory PGP supports raw session-key based enveloping, in
	   practice this key is always (implicitly) derived from a user password,
	   so the enveloping code doesn't allow the use of raw session keys */
	return( envelopeSessionCrypt( "env_ses.pgp", TRUE, FALSE, FALSE, CRYPT_FORMAT_PGP ) );
#endif /* 0 */
	return( TRUE );
	}

int testEnvelopeSessionCryptLargeBuffer( void )
	{
	return( envelopeSessionCrypt( "env_ses", TRUE, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB ) );
	}						/* Datasize, large buffer */

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

	printf( "Testing encrypted enveloping%s...\n",
			useDatasize ? " with datasize hint" : "" );

	/* Create the session key context.  We don't check for errors here
	   since this code will already have been tested earlier */
	status = cryptCreateContext( &cryptContext, CRYPT_UNUSED, 
								 CRYPT_ALGO_3DES );
	if( cryptStatusError( status ) )
		return( FALSE );
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_KEY,
							 "0123456789ABCDEF", 16 );

	/* Create the envelope, push in a KEK and the data, pop the enveloped
	   result, and destroy the envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) || \
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEY, cryptContext ) )
		return( FALSE );
	if( useDatasize )
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
	printf( "Enveloped data has size %d bytes.\n", count );
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
	puts( "Enveloping of encrypted data succeeded.\n" );
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

	printf( "Testing %s%spassword-encrypted enveloping%s",
			( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : "",
			multiKeys ? "multiple-" : "",
			( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
			" with datasize hint" : "" );
	if( useAltCipher )
		{
		printf( ( formatType == CRYPT_FORMAT_PGP ) ? \
				" with non-default cipher type" : " and stream cipher" );
		}
	puts( "..." );

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
				puts( "Warning: Couldn't set non-default envelope cipher "
						"RC4, this may be disabled\n         in this build of "
						"cryptlib.\n" );
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
			printf( "Couldn't set non-default envelope cipher, error code "
					"%d, line %d.\n", status, __LINE__ );
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
			return( FALSE );
		}
	if( useDatasize )
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
	printf( "Enveloped data has size %d bytes.\n", count );
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
	puts( "Enveloping of password-encrypted data succeeded.\n" );
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
	const int eocPos = usePassword ? 182 : 34;
	int count, status;

	printf( "Testing %senveloping boundary condition handling, "
			"strategy %d...\n", usePassword ? "encrypted " : "",
			strategy );

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
				encBuffer[ eocPos ] == 0 );
		}
	else
		{
		assert( encBuffer[ eocPos - 2 ] == \
							ENVELOPE_TESTDATA[ ENVELOPE_TESTDATA_SIZE - 2 ] && \
				encBuffer[ eocPos - 1 ] == \
							ENVELOPE_TESTDATA[ ENVELOPE_TESTDATA_SIZE - 1 ] && \
				encBuffer[ eocPos ] == 0 );
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
		printf( "Error popping data before final-EOC push, error code %d, "
				"line %d.\n", status, __LINE__ );
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
		printf( "Error pushing final EOCs, error code %d, line %d.\n", 
				status, __LINE__ );
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
		printf( "Error popping data after final-EOC push, error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( ( !usePassword && count != 0 ) || \
		( usePassword && \
		  ( count != ( ENVELOPE_TESTDATA_SIZE % 8 ) && \
		    count != ENVELOPE_TESTDATA_SIZE - ( ENVELOPE_TESTDATA_SIZE % 8 ) ) ) )
		{
		printf( "Final data count should have been %d, was %d, line %d.\n", 
				usePassword ? ENVELOPE_TESTDATA_SIZE % 8 : 0, count, 
				__LINE__ );
		return( FALSE );
		}
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Clean up */
	puts( "Enveloping boundary-condition handling succeeded.\n" );
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
	CRYPT_KEYSET cryptKeyset;
	const C_STR keysetName;
	const C_STR password;
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
		keysetName = getKeyfileName( keyFileType, TRUE );
		password = getKeyfilePassword( keyFileType );
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, 
								  CRYPT_KEYSET_FILE, keysetName, 
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusError( status ) )
			return( FALSE );
		}
	else
		{
		cryptKeyset = externalCryptKeyset;
		password = NULL;
		}
	status = addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_KEYSET_DECRYPT, 
								cryptKeyset );
	if( keyFileType != KEYFILE_NONE )
		cryptKeysetClose( cryptKeyset );
	if( status <= 0 )
		return( FALSE );

	/* Push in the data */
	count = pushData( cryptEnvelope, buffer, length, password, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, buffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
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
		puts( "Couldn't find key files, skipping test of public-key "
			  "encrypted enveloping..." );
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
	printf( "Testing %spublic-key encrypted enveloping",
			( formatType == CRYPT_FORMAT_PGP ) ? \
				( ( keyFileType == KEYFILE_PGP ) ? "PGP " : "OpenPGP " ) : "" );
	if( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) && \
		!( useRecipient || useMultipleKeyex || useDirectKey ) )
		printf( " with datasize hint" );
	printf( " using " );
	printf( ( keyFileType == KEYFILE_PGP ) ? \
				( ( formatType == CRYPT_FORMAT_PGP ) ? \
					"PGP key" : "raw public key" ) : \
			  "X.509 cert" );
	if( useRecipient && !useAltAlgo )
		printf( " and recipient info" );
	if( useMultipleKeyex )
		printf( " and additional keying info" );
	if( useAltAlgo )
		printf( " and alt.encr.algo" );
	if( useDirectKey )
		printf( " and direct key add" );
	puts( "..." );

	/* Open the keyset and either get the public key explicitly (to make sure
	   that this version works) or leave the keyset open to allow it to be
	   added to the envelope */
	if( keyFileType != KEYFILE_NONE )
		{
		status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, 
								  CRYPT_KEYSET_FILE, keysetName, 
								  CRYPT_KEYOPT_READONLY );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't open keyset '%s', status %d, line %d.\n", 
					keysetName, status, __LINE__ );
			return( FALSE );
			}
		if( !useRecipient )
			{
			status = cryptGetPublicKey( cryptKeyset, &cryptKey, 
										CRYPT_KEYID_NAME, keyID );
			cryptKeysetClose( cryptKeyset );
			if( cryptStatusError( status ) )
				{
				puts( "Read of public key from file keyset failed." );
				return( FALSE );
				}
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
								CRYPT_ALGO_BLOWFISH ) )
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
			return( FALSE );
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
				printf( "Addition of duplicate key succeeded when it "
						"should have failed,\nline %d.\n", __LINE__ );
				return( FALSE );
				}
			puts( "  (The above message indicates that the test condition "
				  "was successfully\n   checked)." );
			}

		if( keyFileType != KEYFILE_NONE )
			cryptDestroyObject( cryptKey );
		}
	if( useMultipleKeyex && \
		!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
						   TEXT( "test" ), paramStrlen( TEXT( "test" ) ) ) )
		return( FALSE );
	if( useDatasize )
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
	printf( "Enveloped data has size %d bytes.\n", count );
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
	puts( "Enveloping of public-key encrypted data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopePKCCrypt( void )
	{
	if( cryptQueryCapability( CRYPT_ALGO_IDEA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		puts( "Skipping raw public-key and PGP enveloping, which requires "
			  "the IDEA cipher to\nbe enabled.\n" );
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
		if( !envelopePKCCrypt( "env_pkca.pgp", TRUE, KEYFILE_PGP, TRUE, FALSE, TRUE, FALSE, CRYPT_FORMAT_PGP, CRYPT_UNUSED, CRYPT_UNUSED ) )
			return( FALSE );	/* PGP format, recipient, nonstandard bulk encr.algo */
		}
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

	printf( "Testing %s public-key encrypted enveloping...\n", algoName );

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
			printf( "Unknown encryption algorithm %d, line %d.\n", 
					cryptAlgo, __LINE__ );
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
	printf( "%s-enveloped data has size %d bytes.\n", algoName, count );
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
	puts( "Enveloping of public-key encrypted data succeeded.\n" );
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
			printf( "Read of required attribute for signature check "
					"returned status %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		if( value != CRYPT_ENVINFO_SIGNATURE )
			{
			printf( "Envelope requires unexpected enveloping information "
					"type %d, line %d.\n", value, __LINE__ );
			return( FALSE );
			}
		if( sigCheckContext != CRYPT_UNUSED )
			{
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
										sigCheckContext );
			if( cryptStatusError( status ) )
				{
				printf( "Attempt to add signature check key returned status "
						"%d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			}
		}
	status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE_RESULT,
								&value );
	if( cryptStatusError( status ) )
		{
		printf( "Signature check returned status %d, line %d.\n", status, 
				__LINE__ );
		return( FALSE );
		}
	switch( value )
		{
		case CRYPT_OK:
			puts( "Signature is valid." );
			return( TRUE );

		case CRYPT_ERROR_NOTFOUND:
			puts( "Cannot find key to check signature." );
			break;

		case CRYPT_ERROR_SIGNATURE:
			puts( "Signature is invalid." );
			break;

		case CRYPT_ERROR_NOTAVAIL:
			puts( "Warning: Couldn't verify signature due to use of a "
				  "deprecated/insecure\n         signature algorithm.\n" );
			return( TRUE );

		default:
			printf( "Signature check failed, result = %d, line %d.\n", 
					value, __LINE__ );
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
			status = addEnvInfoNumeric( cryptEnvelope,
								CRYPT_ENVINFO_KEYSET_SIGCHECK, cryptKeyset );
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
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_HASH,
										hashContext );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't add externally-generated hash value to "
					"envelope, status %d, line %d.\n", status, __LINE__ );
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
				count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
								  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
			}
		else
			count = popData( cryptEnvelope, buffer, length );
		}
	if( cryptStatusError( count ) )
		return( FALSE );

	/* Determine the result of the signature check */
	if( !getSigCheckResult( cryptEnvelope, sigContext, TRUE, FALSE ) )
		return( FALSE );

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
				printf( "Attempt to read signature check key from PGP "
						"envelope succeeded when it\nshould have failed, "
						"line %d.\n", __LINE__ );
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
				printf( "Couldn't retrieve signature check key from "
						"envelope, status %d, line %d.\n", status,
						__LINE__ );
				return( FALSE );
				}

			/* The signing key should be a pure certificate, not the private 
			   key+certificate combination that we pushed in.  Note that the 
			   following will result in an error message being printed in
			   addEnvInfoNumeric() */
			createEnvelope( &testEnvelope, CRYPT_FORMAT_CRYPTLIB );
			if( addEnvInfoNumeric( testEnvelope, CRYPT_ENVINFO_SIGNATURE,
								   sigCheckContext ) )
				{
				printf( "Retrieved signature check key is a private key, not "
						"a certificate, line %d.\n", __LINE__ );
				return( FALSE );
				}
			else
				{
				puts( "  (The above message indicates that the test "
					  "condition was successfully\n   checked)." );
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
						 const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_KEYSET cryptKeyset;
	CRYPT_CONTEXT cryptContext = DUMMY_INIT;
	int count, status;

	if( !keyReadOK )
		{
		puts( "Couldn't find key files, skipping test of signed "
			  "enveloping..." );
		return( TRUE );
		}
	printf( "Testing %ssigned enveloping%s",
			( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : \
			( formatType == CRYPT_FORMAT_SMIME ) ? "S/MIME " : "",
			( useDatasize && ( formatType != CRYPT_FORMAT_PGP ) ) ? \
			" with datasize hint" : "" );
	if( useCustomHash )
		{
		printf( " %s custom hash",
				( formatType == CRYPT_FORMAT_PGP ) ? "with" :"and" );
		}
	printf( " using %s", 
			( keyFileType == KEYFILE_OPENPGP_HASH ) ? "raw DSA key" : \
			( keyFileType == KEYFILE_PGP ) ? "raw public key" : \
			useSuppliedKey ? "supplied X.509 cert" : "X.509 cert" );
	puts( "..." );

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
		status = getPrivateKey( &cryptContext, USER_PRIVKEY_FILE,
								USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		}
	if( cryptStatusError( status ) )
		{
		printf( "Read of private key from key file failed, status %d, "
				"line %d, cannot test enveloping.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Create the envelope, push in the signing key, any extra information,
	   and the data to sign, pop the enveloped result, and destroy the
	   envelope */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
#if 0	/* 8/7/2012 Removed since it conflicts with the functionality for 
					setting hash values for detached signatures.  This
					capability was never documented in the manual so it's
					unlikely that it was ever used */
	if( useCustomHash )
		{
		CRYPT_CONTEXT hashContext;

		/* Add the (nonstandard) hash algorithm information.  We need to do
		   this before we add the signing key since it's automatically
		   associated with the last hash algorithm added */
		status = cryptCreateContext( &hashContext, CRYPT_UNUSED,
									 CRYPT_ALGO_RIPEMD160 );
		qif( cryptStatusError( status ) )
			return( FALSE );
		status = addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_HASH,
									hashContext );
		if( status <= 0 )
			return( FALSE );

		/* Test the ability to reject a duplicate add of the same hash */
		status = addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_HASH,
									hashContext );
		cryptDestroyContext( hashContext );
		if( status > 0 )
			{
			printf( "Addition of duplicate hash succeeded when it should "
					"have failed, line %d.\n", __LINE__ );
			return( FALSE );
			}
		puts( "  (The above message indicates that the test condition was "
			  "successfully\n   checked)." );
		}
#endif /* 0 */
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext ) )
		return( FALSE );
#if 0	/* 8/7/2012 Removed since it conflicts with the functionality for 
					setting hash values for detached signatures.  This
					capability was never documented in the manual so it's
					unlikely that it was ever used */
	if( useDatasize && !useCustomHash && \
		!( keyFileType == KEYFILE_OPENPGP_HASH || keyFileType == KEYFILE_PGP ) && \
		( formatType != CRYPT_FORMAT_PGP ) )
		{
		CRYPT_CONTEXT hashContext;

		/* Make sure that adding a (pseudo-duplicate) hash action that
		   duplicates the one already added implicitly by the addition of
		   the signature key succeeds (internally, nothing is really done
		   since the hash action is already present) */
		status = cryptCreateContext( &hashContext, CRYPT_UNUSED, 
									 CRYPT_ALGO_SHA1 );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_HASH,
									hashContext );
		cryptDestroyContext( hashContext );
		if( status <= 0 )
			return( FALSE );
		}
#endif /* 0 */
	if( cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
						   cryptContext ) != CRYPT_ERROR_INITED )
		{
		puts( "Addition of duplicate key to envelope wasn't detected." );
		return( FALSE );
		}
	if( !useSuppliedKey )
		cryptDestroyContext( cryptContext );
	if( useDatasize )
		cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
						   dataLength );
	count = pushData( cryptEnvelope, data, dataLength, NULL, 0 );
	if( cryptStatusError( count ) )
		return( FALSE );
	count = popData( cryptEnvelope, globalBuffer, BUFFER_SIZE );
	if( cryptStatusError( count ) )
		return( FALSE );
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	printf( "Enveloped data has size %d bytes.\n", count );
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
			printf( "Attempt to destroy externally-added sig.check key "
					"returned %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		}

	/* Clean up */
	puts( "Enveloping of signed data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopeSign( void )
	{
	if( cryptQueryCapability( CRYPT_ALGO_IDEA, NULL ) == CRYPT_ERROR_NOTAVAIL )
		{
		puts( "Skipping raw public-key based signing, which requires the "
			  "IDEA cipher to\nbe enabled.\n" );
		}
	else
		{
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sign", KEYFILE_PGP, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
			return( FALSE );	/* Indefinite length, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sig", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
			return( FALSE );	/* Datasize, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sig.pgp", KEYFILE_PGP, TRUE, FALSE, FALSE, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, raw key */
		if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_sigd.pgp", KEYFILE_OPENPGP_HASH, TRUE, FALSE, FALSE, CRYPT_FORMAT_PGP ) )
			return( FALSE );	/* PGP format, raw DSA key */
		}
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csgn", KEYFILE_X509, FALSE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Indefinite length, certificate */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csgs", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_FORMAT_SMIME ) )
		return( FALSE );	/* Datasize, certificate, S/MIME semantics */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate, sigcheck key supplied */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg.pgp", KEYFILE_X509, TRUE, FALSE, FALSE, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* PGP format, certificate */
#if 0	/* 8/7/2012 Removed since it conflicts with the functionality for 
					setting hash values for detached signatures.  This
					capability was never documented in the manual so it's
					unlikely that it was ever used */
	if( !envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_hsg", KEYFILE_X509, TRUE, TRUE, FALSE, CRYPT_FORMAT_CRYPTLIB ) )
		return( FALSE );	/* Datasize, certificate, externally-suppl.hash */
#endif /* 0 */
	return( envelopeSign( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, "env_csg", KEYFILE_X509, TRUE, FALSE, TRUE, CRYPT_FORMAT_CRYPTLIB ) );
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

	printf( "Testing %s signed enveloping...\n", algoName );

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
			if( !loadECDSAContexts( &sigCheckKey, &sigKey ) )
				return( FALSE );
			break;

		default:
			printf( "Unknown signature algorithm %d, line %d.\n", 
					cryptAlgo, __LINE__ );
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
	printf( "%s-signed data has size %d bytes.\n", algoName, count );
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
	puts( "Enveloping of signed data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopeSignAlgo( void )
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
		puts( "Couldn't find key files, skipping test of signed "
			  "enveloping..." );
		return( TRUE );
		}
	printf( "Testing %ssigned enveloping with forced overflow in %s...\n",
			( formatType == CRYPT_FORMAT_PGP ) ? "PGP " : \
			( formatType == CRYPT_FORMAT_SMIME ) ? "S/MIME " : "",
			description );

	/* Get the private key */
	status = getPrivateKey( &cryptContext, USER_PRIVKEY_FILE,
							USER_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "Read of private key from key file failed, cannot test "
				"enveloping, line %d.\n", __LINE__ );
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
		printf( "Couldn't set envelope parameters to force overflow, line "
				"%d.\n", __LINE__ );
		return( FALSE );
		}

	/* Push in the data to sign.  Since we're forcing an overflow, we can't
	   do this via the usual pushData() but have to do it manually to handle
	   the restart once the overflow occurs */
	status = cryptPushData( cryptEnvelope, data, dataLength, &bytesIn );
	if( cryptStatusError( status ) || bytesIn != dataLength )
		{
		printf( "cryptPushData() failed with status %d, copied %d of %d "
				"bytes, line %d.\n", status, bytesIn, dataLength, __LINE__ );
		return( FALSE );
		}
	status = cryptFlushData( cryptEnvelope );
	if( forceOverflow && status != CRYPT_ERROR_OVERFLOW )
		{
		printf( "cryptFlushData() returned status %d, should have been "
				"CRYPT_ERROR_OVERFLOW,\n  line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptPopData( cryptEnvelope, localBuffer, 8192 + 4096,
						   &bytesOut );
	if( cryptStatusError( status ) )
		{
		printf( "cryptPopData() #1 failed with status %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	localBufPos = bytesOut;
	status = cryptFlushData( cryptEnvelope );
	if( cryptStatusError( status ) )
		{
		printf( "cryptFlushData() failed with error code %d, line %d.\n",
				status, __LINE__ );
		printErrorAttributeInfo( cryptEnvelope );
		return( FALSE );
		}
	status = cryptPopData( cryptEnvelope, localBuffer + localBufPos,
						   8192 + 4096 - localBufPos, &bytesOut );
	if( cryptStatusError( status ) )
		{
		printf( "cryptPopData() #2 failed with status %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	localBufPos += bytesOut;
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );

	/* Tell them what happened */
	printf( "Enveloped data has size %d bytes.\n", localBufPos );
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
	puts( "Enveloping of signed data succeeded.\n" );
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

/* Test authenticated (MACd/authEnc) enveloping */

static int envelopeAuthent( const void *data, const int dataLength,
							const CRYPT_FORMAT_TYPE formatType,
							const BOOLEAN useAuthEnc, 
							const BOOLEAN useDatasize,
							const int corruptDataLocation )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	BOOLEAN corruptionDetected = FALSE;
	int count, integrityLevel;

	printf( "Testing %sauthenticated%s enveloping", 
			( formatType == CRYPT_FORMAT_PGP ) ? "PGP-format " : "",
			useAuthEnc ? "+encrypted" : "" );
	if( useDatasize )
		printf( " with datasize hint" );
	puts( "..." );

	/* Create the envelope and push in the password after telling the
	   enveloping code that we want to MAC rather than encrypt */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_INTEGRITY,
							useAuthEnc ? CRYPT_INTEGRITY_FULL : \
										 CRYPT_INTEGRITY_MACONLY ) || \
		!addEnvInfoString( cryptEnvelope, CRYPT_ENVINFO_PASSWORD,
						   TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) ) )
		return( FALSE );

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
	printf( "Enveloped data has size %d bytes.\n", count );
	debugDump( ( formatType == CRYPT_FORMAT_PGP ) ? "env_mdc.pgp" : \
			   useAuthEnc ? "env_authenc" : \
			   useDatasize ? "env_mac" : "env_macn", globalBuffer, count );

	/* If we're testing sig.verification, corrupt one of the payload bytes.  
	   This is a bit tricky because we have to hardcode the location of the
	   payload byte, if we change the format at (for example by using a 
	   different algorithm somewhere) then this location will change */
	if( corruptDataLocation > 0 )
		globalBuffer[ corruptDataLocation ]++;

	/* Create the envelope */
	if( !createDeenvelope( &cryptEnvelope ) )
		return( FALSE );

	/* Push in the data */
	count = pushData( cryptEnvelope, globalBuffer, count, 
					  TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) );
	if( cryptStatusError( count ) )
		{
		if( corruptDataLocation && count == CRYPT_ERROR_SIGNATURE )
			{
			puts( "  (This is an expected result since this test verifies "
				  "handling of\n   corrupted authenticated data)." );
			corruptionDetected = TRUE;
			}
		else
			return( FALSE );
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
		printf( "Integrity-protected envelope is reported as having no "
				"integrity protection,\n  line %d.\n", __LINE__ );
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
			puts( "Corruption of encrypted data wasn't detected." );
			return( FALSE );
			}
		}

	/* Clean up */
	puts( "Enveloping of authenticated data succeeded.\n" );
	return( TRUE );
	}

int testEnvelopeAuthenticate( void )
	{
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, FALSE, FALSE ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, FALSE ) )
		return( FALSE );	/* Datasize */
	return( envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, FALSE, TRUE, 175 ) );
	}						/* Datasize, corrupt data to check sig.verification */

int testEnvelopeAuthEnc( void )
	{
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, FALSE, FALSE ) )
		return( FALSE );	/* Indefinite length */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, FALSE ) )
		return( FALSE );	/* Datasize */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_PGP, TRUE, TRUE, FALSE ) )
		return( FALSE );	/* PGP format */
	if( !envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, 192 ) )
		return( FALSE );	/* Datasize, corrupt payload data to check sig.verification */
	return( envelopeAuthent( ENVELOPE_TESTDATA, ENVELOPE_TESTDATA_SIZE, CRYPT_FORMAT_CRYPTLIB, TRUE, TRUE, 170 ) );
	}						/* Datasize, corrupt metadata to check sig.verification */

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
		printf( "Couldn't retrieve signer information from CMS signature, "
				"status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( cryptStatusOK( status ) )
		{
		puts( "Signer information is:" );
		if( !printCertInfo( signerInfo ) )
			return( FALSE );
		cryptDestroyCert( signerInfo );
		}
	status = cryptGetAttribute( cryptEnvelope,
							CRYPT_ENVINFO_SIGNATURE_EXTRADATA, &signerInfo );
	if( cryptStatusError( status ) && sigStatus && \
		( mustHaveAttributes || status != CRYPT_ERROR_NOTFOUND ) )
		{
		printf( "Couldn't retrieve signature information from CMS signature, "
				"status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( cryptStatusOK( status ) )
		{
		puts( "Signature information is:" );
		if( !printCertInfo( signerInfo ) )
			return( FALSE );
		cryptDestroyCert( signerInfo );
		}

	return( sigStatus );
	}

static int cmsEnvelopeSigCheck( const void *signedData,
								const int signedDataLength,
								const CRYPT_CONTEXT sigCheckContext,
								const CRYPT_CONTEXT hashContext,
								const BOOLEAN detachedSig,
								const BOOLEAN hasTimestamp,
								const BOOLEAN checkData,
								const BOOLEAN mustHaveAttributes,
								const BOOLEAN checkWrongKey )
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
			status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_HASH,
										hashContext );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't add externally-generated hash value to "
					"envelope, status %d, line %d.\n", status, __LINE__ );
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
		puts( "Warning: The hash/signature algorithm required to verify "
			  "the signed data\n         isn't enabled in this build of "
			  "cryptlib, can't verify the\n         signature." );
		if( !destroyEnvelope( cryptEnvelope ) )
			return( FALSE );
		return( TRUE );
		}
	if( !cryptStatusError( count ) )
		{
		if( detachedSig )
			{
			if( hashContext == CRYPT_UNUSED )
				count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
								  ENVELOPE_TESTDATA_SIZE, NULL, 0 );
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
			printf( "Use of incorrect key wasn't detected, got %d, should "
					"have been %d, line %d.\n", status, 
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
		printf( "Envelope contains a timestamp..." );
		status = cryptGetAttribute( cryptEnvelope, CRYPT_ENVINFO_TIMESTAMP,
									&cryptTimestamp );
		if( cryptStatusError( status ) )
			{
			printf( "\nCouldn't read timestamp from envelope, status %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptGetAttribute( cryptTimestamp,
									CRYPT_ENVINFO_CONTENTTYPE, &contentType );
		if( cryptStatusError( status ) || \
			contentType != CRYPT_CONTENT_TSTINFO )
			{
			printf( "\nTimestamp data envelope doesn't appear to contain a "
					"timestamp, line %d.\n", __LINE__ );
			return( FALSE );
			}
		puts( " timestamp data appears OK." );

		/* Now get the signature info (if the timestamp doesn't contain a 
		   full CMS message then we'd set 'status = TRUE' at this point
		   without performing the signture check) */
		puts( "Timestamp signature information is:" );
		status = displaySigResult( cryptTimestamp, sigCheckContext, 
								   TRUE, TRUE );
		cryptDestroyEnvelope( cryptTimestamp );
		}
	if( status == TRUE && cryptStatusOK( \
			cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_NEXT ) ) )
		{
		puts( "Data has a second signature:" );
		status = displaySigResult( cryptEnvelope, CRYPT_UNUSED, 
								   mustHaveAttributes, FALSE );
		}
	if( status == TRUE && cryptStatusOK( \
			cryptSetAttribute( cryptEnvelope, CRYPT_ATTRIBUTE_CURRENT_GROUP,
							   CRYPT_CURSOR_NEXT ) ) )
		{
		/* We can have two, but not three */
		puts( "Data appears to have (nonexistent) third signature." );
		return( FALSE );
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
				const BOOLEAN dualSig, const CRYPT_CONTEXT externalSignContext,
				const CRYPT_FORMAT_TYPE formatType )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	CRYPT_CONTEXT cryptContext, cryptContext2, hashContext = CRYPT_UNUSED;
	BOOLEAN isPGP = ( formatType == CRYPT_FORMAT_PGP ) ? TRUE : FALSE;
	BOOLEAN isRawCMS = ( formatType == CRYPT_FORMAT_CRYPTLIB ) ? TRUE : FALSE;
	int count, status = CRYPT_OK;

	if( !keyReadOK )
		{
		puts( "Couldn't find key files, skipping test of CMS signed "
			  "enveloping..." );
		return( TRUE );
		}
	printf( "Testing %s %s%s", isPGP ? "PGP" : "CMS",
			useExtAttributes ? "extended " : "",
			detachedSig ? "detached signature" : \
				dualSig ? "dual signature" : "signed enveloping" );
	if( useNonDataContent )
		printf( " of non-data content" );
	if( externalHashLevel )
		{
		printf( ( externalHashLevel == 1 ) ? \
				" with externally-supplied hash for verify" : \
				" with ext-supplied hash for sign and verify" );
		}
	if( !isPGP && !useAttributes )
		printf( " without signing attributes" );
	if( useDatasize && \
		!( useNonDataContent || useAttributes || useExtAttributes || \
		   detachedSig || useTimestamp ) )
		{
		/* Keep the amount of stuff being printed down */
		printf( " with datasize hint" );
		}
	if( useTimestamp )
		printf( " and timestamp" );
	puts( "..." );

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
			puts( "Read of private key from key file failed, cannot test "
				  "CMS enveloping." );
			return( FALSE );
			}
		}
	if( dualSig )
		{
		status = getPrivateKey( &cryptContext2, DUAL_PRIVKEY_FILE,
								DUAL_SIGNKEY_LABEL, TEST_PRIVKEY_PASSWORD );
		if( cryptStatusError( status ) )
			{
			puts( "Read of private key from key file failed, cannot test "
				  "CMS enveloping." );
			return( FALSE );
			}
		}

	/* If we're supplying the hash value for signing externally, calculate 
	   it now */
	if( externalHashLevel > 1 )
		{
		status = cryptCreateContext( &hashContext, CRYPT_UNUSED,
									 CRYPT_ALGO_SHA1 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( hashContext, ENVELOPE_TESTDATA,
								   ENVELOPE_TESTDATA_SIZE );
		if( cryptStatusOK( status ) && formatType == CRYPT_FORMAT_CMS )
			status = cryptEncrypt( hashContext, "", 0 );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't create external hash of data." );
			return( FALSE );
			}
		}

	/* Create the CMS envelope and  push in the signing key(s) and data */
	if( !createEnvelope( &cryptEnvelope, formatType ) )
		return( FALSE );
	if( !addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext ) )
		return( FALSE );
	if( dualSig &&
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_SIGNATURE,
							cryptContext2 ) )
		return( FALSE );
	if( detachedSig && \
		!addEnvInfoNumeric( cryptEnvelope, 
							CRYPT_ENVINFO_DETACHEDSIGNATURE, TRUE ) )
		return( FALSE );
	if( externalHashLevel > 1 && \
		!addEnvInfoNumeric( cryptEnvelope, CRYPT_ENVINFO_HASH,
							hashContext ) )
		return( FALSE );
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
		status = cryptSetAttribute( cryptEnvelope, CRYPT_ENVINFO_DATASIZE,
									ENVELOPE_TESTDATA_SIZE );
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
			status = cryptSetAttribute( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION,
							CRYPT_CLASSIFICATION_SECRET );
		if( cryptStatusOK( status ) )
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE,
							TEXT( "1 3 6 1 4 1 9999 2" ),
							paramStrlen( TEXT( "1 3 6 1 4 1 9999 2" ) ) );
		if( cryptStatusOK( status ) )
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE,
							"\x04\x04\x01\x02\x03\x04", 6 );
		if( cryptStatusOK( status ) )
			status = cryptSetAttributeString( cmsAttributes,
							CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION,
							"This signature isn't worth the paper it's not "
							"printed on", 56 );
		if( cryptStatusOK( status ) )
			status = cryptSetAttribute( cryptEnvelope,
							CRYPT_ENVINFO_SIGNATURE_EXTRADATA, cmsAttributes );
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
			return( attrErrorExit( cryptSession, "cryptSetAttributeString()",
								   status, __LINE__ ) );
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
				printf( "Addition of timestamping attribute to "
						"CRYPT_FORMAT_CRYPTLIB envelope\nsucceeded, should "
						"have failed, line %d.\n", __LINE__ );
				return( FALSE );
				}
			puts( "Enveloping with timestamp using invalid env.type was "
				  "correctly rejected.\n" );
			return( TRUE );
			}
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Push in the data to be signed, unless it's already been processed via
	   an external hash */
	if( !( externalHashLevel > 1 ) )
		{
		status = count = pushData( cryptEnvelope, ENVELOPE_TESTDATA,
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
			puts( "Envelope timestamping failed due to problems talking to "
				  "TSA, this is a non-\ncritical problem.  Continuing...\n" );
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
	printf( "%s %s has size %d bytes.\n", isPGP ? "PGP" : "CMS",
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
		status = cryptCreateContext( &hashContext, CRYPT_UNUSED,
									 CRYPT_ALGO_SHA1 );
		if( cryptStatusOK( status ) )
			status = cryptEncrypt( hashContext, ENVELOPE_TESTDATA,
								   ENVELOPE_TESTDATA_SIZE );
		if( cryptStatusOK( status ) && formatType == CRYPT_FORMAT_CMS )
			status = cryptEncrypt( hashContext, "", 0 );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't create external hash of data." );
			return( FALSE );
			}
		}

	/* Make sure that the signature is valid.  The somewhat redundant mapping
	   of useNonDataContent is because we're performing an additional check
	   of envelope key-handling as part of this particular operation */
	status = cmsEnvelopeSigCheck( globalBuffer, count,
								  isPGP || isRawCMS ? cryptContext : CRYPT_UNUSED,
								  hashContext, detachedSig, useTimestamp, TRUE,
								  useAttributes, useNonDataContent ? \
												 TRUE : FALSE );
	if( externalHashLevel > 0 )
		cryptDestroyContext( hashContext );
	if( isPGP || isRawCMS )
		cryptDestroyContext( cryptContext );
	if( status <= 0 )
		return( FALSE );

	if( detachedSig )
		{
		printf( "Creation of %s %sdetached signature %ssucceeded.\n\n",
				isPGP ? "PGP" : "CMS", useExtAttributes ? "extended " : "",
				( hashContext != CRYPT_UNUSED ) ? \
					"with externally-supplied hash " : "" );
		}
	else
		{
		printf( "Enveloping of CMS %s%ssigned data succeeded.\n\n",
				useExtAttributes ? "extended " : "",
				useTimestamp ? "timestamped " : "" );
		}
	return( TRUE );
	}

int testCMSEnvelopeSign( void )
	{
	if( !cmsEnvelopeSign( FALSE, FALSE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Minimal (no default S/MIME attributes) */
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Standard (default S/MIME signing attributes) */
	if( !cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize and attributes */
	if( !cmsEnvelopeSign( FALSE, TRUE, TRUE, FALSE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Extended signing attributes */
	if( !cmsEnvelopeSign( TRUE, TRUE, TRUE, FALSE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize and extended attributes */
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, 0, FALSE, TRUE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
	}						/* Signing of non-data content */

int testCMSEnvelopeDualSign( void )
	{
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) );
							/* Standard, with two signatures */
	}

int testCMSEnvelopeDetachedSig( void )
	{
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 0, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Detached sig and attributes */
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 1, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Detached sig, attributes, externally-suppl.hash for verify */
	if( !cmsEnvelopeSign( FALSE, TRUE, FALSE, TRUE, 2, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Detached sig, externally-suppl.hash for sign and verify */
	if( !cmsEnvelopeSign( TRUE, FALSE, FALSE, TRUE, 1, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) )
		return( FALSE );	/* Detached sig, data size, externally-suppl.hash for verify, PGP format */
	return( cmsEnvelopeSign( TRUE, FALSE, FALSE, TRUE, 2, FALSE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_PGP ) );
	}						/* Detached sig, data size, externally-suppl.hash for sign and verify, PGP format */

int testCMSEnvelopeSignEx( const CRYPT_CONTEXT signContext )
	{
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, signContext, CRYPT_FORMAT_CMS ) );
	}						/* Datasize, attributes, external signing context */

int testSessionEnvTSP( void )
	{
	/* This is a pseudo-enveloping test that uses the enveloping
	   functionality but is called as part of the session tests since full
	   testing of the TSP handling requires that it be used to timestamp an
	   S/MIME sig */
	if( !cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CMS ) )
		return( FALSE );	/* Datasize, attributes, timestamp */
	return( cmsEnvelopeSign( TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_FORMAT_CRYPTLIB ) );
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
			printf( "Couldn't allocate test buffer of %d bytes, line %d.\n", 
					count, __LINE__ );
			return( FALSE );
			}
		}
	sprintf( msgBuffer, "S/MIME SignedData #%d", fileNo );
	count = readFileData( fileName, msgBuffer, bufPtr, count );
	if( count <= 0 )
		{
		if( bufPtr != globalBuffer )
			free( bufPtr );
		return( count );
		}

	/* Check the signature on the data */
	status = cmsEnvelopeSigCheck( bufPtr, count, CRYPT_UNUSED, CRYPT_UNUSED,
								  FALSE, FALSE, FALSE, 
								  ( fileNo == 1 ) ? FALSE : TRUE, FALSE );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	if( status )
		puts( "S/MIME SignedData import succeeded.\n" );
	else
		{
		/* The AuthentiCode data sig-check fails for some reason */
		if( fileNo == 2 )
			{
			puts( "AuthentiCode SignedData import succeeded but signature "
				  "couldn't be verified\n  due to AuthentiCode special "
				  "processing requirements.\n" );
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
			status = addEnvInfoNumeric( cryptEnvelope,
								CRYPT_ENVINFO_KEYSET_DECRYPT, cryptKeyset );
		cryptKeysetClose( cryptKeyset );
		}
	if( status <= 0 )
		return( FALSE );

	/* Push in the data */
	count = pushData( cryptEnvelope, envelopedData, envelopedDataLength,
					  ( externalPassword == NULL ) ? TEST_PRIVKEY_PASSWORD :
					  externalPassword, 0 );
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
		puts( "Couldn't find key files, skipping test of CMS encrypted "
			  "enveloping..." );
		return( TRUE );
		}
	printf( "Testing CMS public-key encrypted enveloping" );
	if( externalKeyset != CRYPT_UNUSED && recipientName != NULL )
		{
		if( useExternalRecipientKeyset )
			printf( " with recipient keys in device" );
		else
			printf( " with dual encr./signing certs" );
		}
	else
		{
		if( useStreamCipher )
			printf( " with stream cipher" );
		else
			{
			if( useLargeBlockCipher )
				printf( " with large block size cipher" );
			else
				{
				if( useDatasize )
					printf( " with datasize hint" );
				}
			}
		}
	puts( "..." );

	/* Get the public key.  We use assorted variants to make sure that they
	   all work */
	if( externalCryptContext != CRYPT_UNUSED )
		{
		int cryptAlgo;

		status = cryptGetAttribute( externalCryptContext, CRYPT_CTXINFO_ALGO,
									&cryptAlgo );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't determine algorithm for public key, cannot test "
				  "CMS enveloping." );
			return( FALSE );
			}
		cryptKey = externalCryptContext;
		}
	else
		{
		if( recipientName == NULL )
			{
			CRYPT_KEYSET cryptKeyset;

			/* No recipient name, get the public key */
			status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED,
									  CRYPT_KEYSET_FILE, USER_PRIVKEY_FILE,
									  CRYPT_KEYOPT_READONLY );
			if( cryptStatusError( status ) )
				{
				printf( "Recipient keyset open failed with status %d, "
						"line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			status = cryptGetPublicKey( cryptKeyset, &cryptKey, 
										CRYPT_KEYID_NAME,
										USER_PRIVKEY_LABEL );
			cryptKeysetClose( cryptKeyset );
			if( cryptStatusError( status ) )
				{
				printf( "Read of public key from key file failed with "
						"status %d, line %d.\n", status, __LINE__ );
				return( FALSE );
				}
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
				printf( "Couldn't open key database, status %d, line %d.\n",
						status, __LINE__ );
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
	printf( "Enveloped data has size %d bytes.\n", count );
	debugDump( dumpFileName, globalBuffer, count );

	/* Make sure that the enveloped data is valid */
	status = cmsEnvelopeDecrypt( globalBuffer, count, externalKeyset,
								 externalPassword, TRUE );
	if( status <= 0 )	/* Can be FALSE or an error code */
		return( FALSE );

	/* Clean up */
	puts( "Enveloping of CMS public-key encrypted data succeeded.\n" );
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
			printf( "Couldn't allocate test buffer of %d bytes, lin e%d.\n", 
					count, __LINE__ );
			return( FALSE );
			}
		}
	sprintf( msgBuffer, "S/MIME EnvelopedData #%d", fileNo );
	count = readFileData( fileName, msgBuffer, bufPtr, count );
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
	if( !destroyEnvelope( cryptEnvelope ) )
		return( FALSE );
	if( status == CRYPT_ENVELOPE_RESOURCE )
		{
		/* When we get to the CRYPT_ENVELOPE_RESOURCE stage we know that 
		   we've successfully processed all of the recipients, because
		   cryptlib is asking us for a key to continue */
		status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		printf( "Couldn't de-envelope data, status = %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}
	
	puts( "S/MIME EnvelopedData import succeeded.\n" );
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
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, &value );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, CRYPT_ALGO_RC4 );
	status = cmsEnvelopeCrypt( "smi_pkcs", TRUE, TRUE, FALSE, FALSE, CRYPT_UNUSED, CRYPT_UNUSED, NULL, NULL );
	cryptSetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, value );
	if( !status )			/* Datasize and stream cipher */
		return( status );

	/* Test enveloping with a cipher with a larger-than-usual block size */
	cryptGetAttribute( CRYPT_UNUSED, CRYPT_OPTION_ENCR_ALGO, &value );
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
		puts( "  (This is probably because the public key certificate was "
			  "regenerated after\n   the certificate stored with the "
			  "private key was created, so that the\n   private key can't "
			  "be identified any more using the public key that was\n   "
			  "used for encryption.  This can happen when the cryptlib "
			  "self-test is run\n   in separate stages, with one stage "
			  "re-using data that was created\n   earlier during a "
			  "previous stage)." );
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
		puts( "The certificate database wasn't updated with dual encryption/"
			  "signing certs\nduring this test run (either because database "
			  "keysets aren't enabled in this\nbuild of cryptlib or because "
			  "only some portions of the self-tests are being\nrun), "
			  "skipping the test of CMS enveloping with dual certs.\n" );
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
		puts( "Couldn't find keyset with dual encryption/signature certs "
			  "for test of dual certificate\nencryption." );
		return( FALSE );
		}
	status = cmsEnvelopeCrypt( "smi_pkcr", TRUE, FALSE, FALSE, FALSE, 
							   CRYPT_UNUSED, cryptKeyset, 
							   TEST_PRIVKEY_PASSWORD, 
							   TEXT( "dave@wetaburgers.com" ) );
	cryptKeysetClose( cryptKeyset );
	if( status == CRYPT_ERROR_NOTFOUND )
		{					/* Datasize, recipient */
		puts( "  (This is probably because the public key certificate was "
			  "regenerated after\n   the certificate stored with the "
			  "private key was created, so that the\n   private key can't "
			  "be identified any more using the public key that was\n   "
			  "used for encryption.  This can happen when the cryptlib "
			  "self-test is run\n   in separate stages, with one stage "
			  "re-using data that was created\n   earlier during a "
			  "previous stage)." );
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
		puts( "Couldn't find S/MIME SignedData file, skipping test of "
			  "SignedData import..." );
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

	puts( "Import of S/MIME SignedData succeeded.\n" );

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
		puts( "Couldn't find S/MIME EnvelopedData file, skipping test of "
			  "EnvelopedData import..." );
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

	puts( "Import of S/MIME EnvelopedData succeeded.\n" );

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
		puts( "Couldn't find CMS EnvelopedData file, skipping test of "
			  "EnvelopedData/AuthentcatedData import..." );
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

	puts( "Import of CMS password-encrypted/authenticated data "
		  "succeeded.\n" );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated password-encrypted data */

int testPGPEnvelopePasswordCryptImport( void )
	{
	int count;

	/* Process the PGP 2.x data: IDEA with MD5 hash.  Create with:

		pgp -c +compress=off -o conv_enc1.pgp test.txt */
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
	puts( "Import of PGP password-encrypted data succeeded." );

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
	puts( "Import of OpenPGP password-encrypted data succeeded.\n" );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated PKC-encrypted data */

int testPGPEnvelopePKCCryptImport( void )
	{
	int count;

	/* Process the PGP 2.x encrypted data */
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
		puts( "De-enveloped data != original data." );
		return( FALSE );
		}
	puts( "Import of PGP-encrypted data succeeded." );

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
	if( count < 600 || \
		!compareData( "\n\n<sect>Vorwort\n", 16, globalBuffer, 16 ) )
		return( FALSE );
	puts( "Import of OpenPGP-encrypted data succeeded.\n" );

	return( TRUE );
	}

/* Import PGP 2.x and OpenPGP-generated signed data */

int testPGPEnvelopeSignedDataImport( void )
	{
	CRYPT_CONTEXT hashContext;
	int count, status;

	/* Process the PGP 2.x signed data.  Create with:

		pgp -s +secring="secring.pgp" +pubring="pubring.pgp" -u test test.txt */
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
	puts( "Import of PGP-signed data succeeded." );

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
	puts( "Import of PGP 2.x/OpenPGP-hybrid-signed data succeeded." );
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
	puts( "Import of OpenPGP-signed data succeeded." );

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
		puts( "Couldn't create external hash of data." );
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
	puts( "Import of OpenPGP-signed data with externally-supplied hash "
		  "succeeded.\n" );

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
		puts( "Couldn't allocate test buffer." );
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
					  ENVELOPE_COMPRESSEDDATA, ENVELOPE_COMPRESSEDDATA_SIZE ) )
		{
		free( bufPtr );
		return( FALSE );
		}
	puts( "Import of PGP 2.x compressed data succeeded.\n" );

	/* Process the OpenPGP compressed nested data.  Create with:

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
	free( bufPtr );
	count = envelopeSigCheck( globalBuffer, count, CRYPT_UNUSED,
							  CRYPT_UNUSED, KEYFILE_OPENPGP_HASH, FALSE,
							  CRYPT_FORMAT_PGP );
	if( count <= 0 )
		return( FALSE );
	if( !compareData( ENVELOPE_COMPRESSEDDATA, ENVELOPE_COMPRESSEDDATA_SIZE, 
					  globalBuffer, ENVELOPE_COMPRESSEDDATA_SIZE ) )
		return( FALSE );
	puts( "Import of OpenPGP compressed signed data succeeded.\n" );

	return( TRUE );
	}

/* Generic test routines used for debugging.  These are only meant to be
   used interactively, and throw exceptions rather than returning status
   values */

static void dataImport( void *buffer, const int length, 
						const BOOLEAN resultBad )
	{
	CRYPT_ENVELOPE cryptEnvelope;
	int count;

	createDeenvelope( &cryptEnvelope );
	count = pushData( cryptEnvelope, buffer, length, NULL, 0 );
	if( resultBad )
		{
		assert( cryptStatusError( count ) );
		return;
		}
	assert( !cryptStatusError( count ) );
	count = popData( cryptEnvelope, buffer, length );
	assert( !cryptStatusError( count ) );
	destroyEnvelope( cryptEnvelope );
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
	count = readFileData( fileName, "Generic test data", bufPtr, count );
	assert( count > 0 );
	dataImport( bufPtr, count, FALSE );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	}

void xxxSignedDataImport( const char *fileName )
	{
	BYTE *bufPtr = globalBuffer;
	int count, status;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE && \
		( bufPtr = malloc( count ) ) == NULL )
		{
		assert( 0 );
		return;
		}
	count = readFileData( fileName, "S/MIME test data", bufPtr, count );
	assert( count > 0 );
	status = cmsEnvelopeSigCheck( bufPtr, count, CRYPT_UNUSED, CRYPT_UNUSED, 
								  FALSE, FALSE, FALSE, FALSE, FALSE );
	assert( status > 0 );
	if( bufPtr != globalBuffer )
		free( bufPtr );
	}

void xxxEncryptedDataImport( const char *fileName, const char *keyset,
							 const char *password )
	{
	CRYPT_KEYSET cryptKeyset;
	BYTE *buffer = globalBuffer;
	int count, status;

	count = getFileSize( fileName ) + 10;
	if( count >= BUFFER_SIZE )
		{
		buffer = malloc( count );
		assert( buffer != NULL );
		}
	count = readFileData( fileName, "S/MIME test data", buffer,
						  count );
	assert( count > 0 );
	status = cryptKeysetOpen( &cryptKeyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE,
							  keyset, CRYPT_KEYOPT_READONLY );
	assert( cryptStatusOK( status ) );
	status = cmsEnvelopeDecrypt( buffer, count, cryptKeyset, password, FALSE );
	assert( status > 0 );
	cryptKeysetClose( cryptKeyset );
	if( buffer != globalBuffer )
		free( buffer );
	}

void xxxEnvelopeTest( void )
	{
	CRYPT_CERTIFICATE cryptCert;

	importCertFile( &cryptCert, TEXT( "r:/cert.der" ) );

	envelopePKCCrypt( "r:/test.p7m", TRUE, KEYFILE_NONE, FALSE,
					  FALSE, FALSE, TRUE, CRYPT_FORMAT_CMS, cryptCert, 0 );
	}
#endif /* TEST_ENVELOPE || TEST_SESSION */
