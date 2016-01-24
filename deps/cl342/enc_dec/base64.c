/****************************************************************************
*																			*
*							cryptlib Base64 Routines						*
*						Copyright Peter Gutmann 1992-2011					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "stream.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "io/stream.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/* Base64 encode/decode tables from RFC 1113 */

#define BPAD		'='		/* Padding for odd-sized output */
#define BERR		0xFF	/* Illegal character marker */
#define BEOF		0x7F	/* EOF marker (padding character or EOL) */

static const char FAR_BSS binToAscii[ 64 ] = { 
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 
	'w', 'x', 'y', 'z', '0', '1', '2', '3', 
	'4', '5', '6', '7', '8', '9', '+', '/' 
	};

static const BYTE FAR_BSS asciiToBin[ 256 ] = { 
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 00 */
	BERR, BERR, BEOF, BERR, BERR, BEOF, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 10 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 20 */
	BERR, BERR, BERR, 0x3E, BERR, BERR, BERR, 0x3F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,	/* 30 */
	0x3C, 0x3D, BERR, BERR, BERR, BEOF, BERR, BERR,
	BERR, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,	/* 40 */
	0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,	/* 50 */
	0x17, 0x18, 0x19, BERR, BERR, BERR, BERR, BERR,
	BERR, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,	/* 60 */
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,	/* 70 */
	0x31, 0x32, 0x33, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 80 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 90 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* A0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* B0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* C0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* D0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* E0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* F0 */
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR
	};

/* The size of lines for base64-encoded data.  This is only used for 
   encoding, for decoding we adjust to whatever size the sender has used, 
   however we require at least some minimum line size as well as a maximum 
   line size when we check for the validity of base64-encoded data */

#define BASE64_LINESIZE		64
#define BASE64_MIN_LINESIZE	56
#define BASE64_MAX_LINESIZE	128

/* Basic single-character en/decode functions.  We mask the value to 6 or 8 
   bits both as a range check and to avoid generating negative array offsets 
   if the sign bit is set, since the strings are passed as 'char *'s */

#define encode( data )	binToAscii[ ( data ) & 0x3F ]
#define decode( data )	asciiToBin[ ( data ) & 0xFF ]

/* The headers and trailers used for base64-encoded certificate objects */

typedef struct {
	const CRYPT_CERTTYPE_TYPE type;
	BUFFER_FIXED( headerLen ) \
	const char FAR_BSS *header;
	const int headerLen;
	BUFFER_FIXED( trailerLen ) \
	const char FAR_BSS *trailer;
	const int trailerLen;
	} HEADER_INFO;
static const HEADER_INFO FAR_BSS headerInfo[] = {
	{ CRYPT_CERTTYPE_CERTIFICATE,
	  "-----BEGIN CERTIFICATE-----" EOL, 27 + EOL_LEN,
	  "-----END CERTIFICATE-----" EOL, 25 + EOL_LEN },
	{ CRYPT_CERTTYPE_ATTRIBUTE_CERT,
	  "-----BEGIN ATTRIBUTE CERTIFICATE-----" EOL, 37 + EOL_LEN,
	  "-----END ATTRIBUTE CERTIFICATE-----" EOL, 35 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTCHAIN,
	  "-----BEGIN CERTIFICATE CHAIN-----" EOL, 33 + EOL_LEN,
	  "-----END CERTIFICATE CHAIN-----" EOL, 31 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTREQUEST,
	  "-----BEGIN NEW CERTIFICATE REQUEST-----" EOL, 39 + EOL_LEN,
	  "-----END NEW CERTIFICATE REQUEST-----" EOL, 37 + EOL_LEN },
	{ CRYPT_CERTTYPE_REQUEST_CERT,
	  "-----BEGIN NEW CERTIFICATE REQUEST-----" EOL, 39 + EOL_LEN,
	  "-----END NEW CERTIFICATE REQUEST-----" EOL, 37 + EOL_LEN },
	{ CRYPT_CERTTYPE_CRL,
	  "-----BEGIN CERTIFICATE REVOCATION LIST-----"  EOL, 43 + EOL_LEN,
	  "-----END CERTIFICATE REVOCATION LIST-----" EOL, 41 + EOL_LEN },
	{ CRYPT_CERTTYPE_NONE,			/* Universal catch-all */
	  "-----BEGIN CERTIFICATE OBJECT-----"  EOL, 34 + EOL_LEN,
	  "-----END CERTIFICATE OBJECT-----" EOL, 32 + EOL_LEN },
	{ CRYPT_CERTTYPE_NONE,			/* Universal catch-all */
	  "-----BEGIN CERTIFICATE OBJECT-----"  EOL, 34 + EOL_LEN,
	  "-----END CERTIFICATE OBJECT-----" EOL, 32 + EOL_LEN }
	};

/****************************************************************************
*																			*
*						Decode Format-checking Functions					*
*																			*
****************************************************************************/

/* Check for raw base64 data.  There isn't a 100% reliable check that we can 
   apply for this but if the first BASE64_MIN_LINESIZE characters are all 
   valid base64 data and the first characters match the encoded form of data 
   handled by cryptlib then it's reasonably certain that it's base64 data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int fixedBase64decode( STREAM *stream,
							  IN_BUFFER( srcLen ) const char *src, 
							  IN_LENGTH_MIN( 10 ) const int srcLen );

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkBase64( INOUT STREAM *stream, const BOOLEAN isPEM )
	{
	STREAM nullStream;
	BYTE buffer[ BASE64_MAX_LINESIZE + 8 ];
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Make sure that there's enough data present to perform a reliable
	   check */
	status = sread( stream, buffer, BASE64_MIN_LINESIZE );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Make sure that the content is some form of encoded key or certificate 
	   data. For certificate data that begins with 30 8x the corresponding 
	   base64 values are MI...; for an SSH public key that begins 00 00 it's 
	   AA...; for a PGP public key that begins 99 0x it's mQ... 

	   Unfortunately in the case of MIME-encoded data with a MIME header, 
	   the header is likely to begin with "MIME-Version", which happens to 
	   match the base64-encoded form of 30 8x = "MI", so this quick-reject 
	   filter won't catch this one particular case */
	if( memcmp( buffer, "MI", 2 ) && \
		memcmp( buffer, "AA", 2 ) && \
		memcmp( buffer, "mQ", 2 ) )
		return( FALSE );

	/* Check that we have at least one minimal-length line of base64-encoded 
	   data */
	sMemNullOpen( &nullStream );
	status = fixedBase64decode( &nullStream, buffer, BASE64_MIN_LINESIZE );
	sMemDisconnect( &nullStream );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* If it's supposed to be PEM data, check that it's delimited by 
	   linebreaks (or at least that there's a linebreak visible before 
	   encountering BASE64_MAX_LINESIZE characters) */
	if( isPEM )
		{
		BOOLEAN hasLineBreak = FALSE;
		int i;

		status = sread( stream, buffer, 
						( BASE64_MAX_LINESIZE - BASE64_MIN_LINESIZE ) + 1 );
		if( cryptStatusError( status ) )
			return( FALSE );
		for( i = 0; i < ( BASE64_MAX_LINESIZE - \
						  BASE64_MIN_LINESIZE ) + 1; i++ )
			{
			const int ch = byteToInt( decode( buffer[ i ] ) );
			if( ch == BERR || ch == BEOF )
				{
				hasLineBreak = TRUE;
				break;
				}
			}
		if( !hasLineBreak )
			return( FALSE );
		}

	return( TRUE );
	}

/* Check for PEM-encapsulated data.  All that we need to look for is the
   '-----..' header, which is fairly simple although we also need to handle
   the SSH '---- ...' variant (4 dashes and a space) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkPEMHeader( INOUT STREAM *stream, 
						   OUT_LENGTH_Z int *headerLength )
	{
	BOOLEAN isSSH = FALSE, isPGP = FALSE;
	char buffer[ 1024 + 8 ], *bufPtr = buffer;
	int length, position = DUMMY_INIT, lineCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( headerLength, sizeof( int ) ) );

	/* Clear return value */
	*headerLength = 0;

	/* Check for the initial 5 dashes and 'BEGIN ' (unless we're SSH, in
	   which case we use 4 dashes, a space, and 'BEGIN ') */
	status = readTextLine( ( READCHARFUNCTION ) sgetc, stream, 
						   buffer, 1024, &length, NULL );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 5 + 6 + 7 + 5 )
		{
		/* We need room for at least '-----' (5) + 'BEGIN ' (6) + 
		   'ABC XYZ' (7) + '-----' (5) */
		return( CRYPT_ERROR_BADDATA );
		}
	if( memcmp( bufPtr, "-----BEGIN ", 11 ) &&	/* PEM/PGP form */
		memcmp( bufPtr, "---- BEGIN ", 11 ) )	/* SSH form */
		return( CRYPT_ERROR_BADDATA );
	bufPtr += 11;
	length -= 11;

	/* Skip the object name */
	if( !strCompare( bufPtr, "SSH2 ", 5 ) )
		isSSH = TRUE;
	else
		{
		if( !strCompare( bufPtr, "PGP ", 4 ) )
			isPGP = TRUE;
		}
	while( length >= 4 )
		{
		if( *bufPtr == '-' )
			break;
		bufPtr++;
		length--;
		}
	if( length != 5 && length != 4 )
		return( CRYPT_ERROR_BADDATA );

	/* Check the the trailing 5 (4 for SSH) dashes */
	if( strCompare( bufPtr, "-----", length ) )
		return( CRYPT_ERROR_BADDATA );

	/* If it's not SSH or PGP data, we're done */
	if( !isSSH && !isPGP )
		{
		*headerLength = stell( stream );

		return( CRYPT_OK );
		}

	/* At this point SSH and PGP can continue with an arbitrary number of
	   type : value pairs that we have to strip before we get to the
	   payload.  SSH runs the header straight into the body so the only way 
	   to tell whether we've hit the body is to check for the absence of the 
	   ':' separator, while PGP uses a conventional header format with a 
	   blank line as the delimiter so all that we have to do is look for a 
	   zero-length line */
	for( lineCount = 0; lineCount < FAILSAFE_ITERATIONS_MED; lineCount++ )
		{
		position = stell( stream );
		status = readTextLine( ( READCHARFUNCTION ) sgetc, stream, 
							   buffer, 1024, &length, NULL );
		if( cryptStatusError( status ) )
			return( status );
		if( isSSH && strFindCh( buffer, length, ':' ) < 0 )
			break;
		if( isPGP && length <= 0 )
			break;
		}
	if( lineCount >= FAILSAFE_ITERATIONS_MED )
		return( CRYPT_ERROR_BADDATA );
	if( isSSH )
		{
		/* Return to the point before the line without the ':' */
		sseek( stream, position );
		}
	*headerLength = stell( stream );

	return( CRYPT_OK );
	}

/* Look for the EOL marker at the end of a line of text.  There's one
   problematic special case here in which, if the encoding has produced
   bricktext, the end of the data will coincide with the EOL.  For
   CRYPT_CERTFORMAT_TEXT_CERTIFICATE this will give us '-----END...' on
   the next line which is easy to check for, but for
   CRYPT_ICERTFORMAT_SMIME_CERTIFICATE what we end up with depends on the
   calling code.  It could either truncate immediately at the end of the 
   data (which it isn't supposed to) so we get '\0', it could truncate after 
   the EOL (so we get EOL + '\0'), it could continue with a futher content 
   type after a blank line (so we get EOL + EOL), or it could truncate 
   without the '\0' so we get garbage, which is the caller's problem.  
   Because of this we look for all of these situations and, if any are 
   found, return a 0-count EOL indicator */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int checkEOL( IN_BUFFER( srcLen ) const char *src, 
					 IN_LENGTH const int srcLen,
					 OUT_LENGTH_Z int *eolSize,
					 IN_ENUM( CRYPT_CERTFORMAT ) \
						const CRYPT_CERTFORMAT_TYPE format )
	{
	int srcIndex = 0;

	assert( isReadPtr( src, srcLen ) );
	assert( isWritePtr( eolSize, sizeof( int ) ) );

	REQUIRES( srcLen > 0 && srcLen < MAX_INTLENGTH );
	REQUIRES( format > CRYPT_CERTFORMAT_NONE && \
			  format < CRYPT_CERTFORMAT_LAST );

	/* Clear return value */
	*eolSize = 0;

	/* Check for a '\0' at the end of the data */
	if( format == CRYPT_ICERTFORMAT_SMIME_CERTIFICATE && src[ 0 ] == '\0' )
		return( OK_SPECIAL );	/* We're at EOF, not just EOL */

	/* Check for EOL */
	if( *src == '\n' )
		srcIndex++;
	else
		{
		if( *src == '\r' )
			{
			srcIndex++;

			/* Some broken implementations emit two CRs before the LF.
			   Stripping these extra CRs clashes with other broken
			   implementations that emit only CRs, which means that we'll
			   be stripping the EOT blank line in MIME encapsulation,
			   however the two-CR bug (usually from older versions of
			   Netscape) appears to be more prevalent than the CR-only
			   bug (old Mac software) */
			if( ( srcIndex < srcLen ) && src[ srcIndex ] == '\r' )
				srcIndex++;
			if( ( srcIndex < srcLen ) && src[ srcIndex ] == '\n' )
				srcIndex++;
			}
		}
	if( srcIndex >= srcLen )
		return( OK_SPECIAL );	/* We're at EOF, not just EOL */

	/* Check for '\0' or EOL (S/MIME) or '----END...' (PEM) after EOL */
	if( format == CRYPT_ICERTFORMAT_SMIME_CERTIFICATE )
		{
		if( src[ srcIndex ] == '\0' || src[ srcIndex ] == '\n' || \
			src[ srcIndex ] == '\r' )
			return( OK_SPECIAL );	/* We're at EOF, not just EOL */
		}
	if( format == CRYPT_CERTFORMAT_TEXT_CERTIFICATE )
		{
		if( srcIndex + 9 <= srcLen && \
			!strCompare( src + srcIndex, "-----END ", 9 ) )
			return( OK_SPECIAL );	/* We're at EOF, not just EOL */
		}

	/* If we were expecting an EOL but didn't find one there's a problem 
	   with the data */
	if( srcIndex <= 0 )
		return( CRYPT_ERROR_BADDATA );

	ENSURES( srcIndex > 0 && srcIndex < srcLen );
	*eolSize = srcIndex;

	return( CRYPT_OK );
	}

/* Check whether a data item has a header that identifies it as some form of
   encoded object and return the start position of the encoded data.  For
   S/MIME certificate data this can in theory get quite complex because
   there are many possible variations in the headers.  Some early S/MIME
   agents used a content type of "application/x-pkcs7-mime",
   "application/x-pkcs7-signature", and "application/x-pkcs10", while newer
   ones use the same without the "x-" at the start.  In addition Netscape
   have their own MIME data types for certificates, "application/x-x509-"
   "{user-cert|ca-cert|email-cert}, and this tradition is perpetuated by the
   mass of further types in the neverending stream of RFCs that PKIX churns 
   out.  There are a whole pile of other possible headers as well, none of 
   them terribly relevant for our purposes, so all that we check for is the 
   base64 indicator */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64checkHeader( IN_BUFFER( dataLength ) const char *data, 
					   IN_LENGTH const int dataLength,
					   OUT_ENUM_OPT( CRYPT_CERTFORMAT ) \
						CRYPT_CERTFORMAT_TYPE *format,
					   OUT_LENGTH_Z int *startPos )
	{
	STREAM stream;
	BOOLEAN seenTransferEncoding = FALSE, isBinaryEncoding = FALSE;
	BOOLEAN seenDash = FALSE, isBase64;
	int position = DUMMY_INIT, lineCount, length, status;

	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( format, sizeof( CRYPT_CERTFORMAT_TYPE ) ) );
	assert( isWritePtr( startPos, sizeof( int ) ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );

	/* Clear return values */
	*format = CRYPT_CERTFORMAT_NONE;
	*startPos = 0;

	/* If the item is too small to contain any useful data we don't even try 
	   and examine it.  We don't treat this as a data or underflow error 
	   since it may be a short but valid data object like an empty CRL */
	if( dataLength < 64 )
		return( CRYPT_OK );

	sMemConnect( &stream, data, dataLength );

	/* Perform a quick check to weed out unencoded certificate data, which 
	   is usually the case.  Certificates and related objects are always an 
	   ASN.1 SEQUENCE so if we find data that begins with this value then we
	   perform the check for a certificate object.  For very large objects 
	   (which can only be CRLs) we can get an overflow error trying to read 
	   a short length so if the length is suspiciously long we allow a long 
	   length.  We don't do this unconditionally in order to reduce 
	   potential false positives */
	if( sPeek( &stream ) == BER_SEQUENCE )
		{
		if( dataLength < 32000L )
			status = readSequenceI( &stream, NULL );
		else
			status = readLongSequence( &stream, NULL );
		if( cryptStatusOK( status ) )
			{
			sMemDisconnect( &stream );

			return( CRYPT_OK );
			}
		sClearError( &stream );
		sseek( &stream, 0 );
		}

	/* Sometimes the object can be preceded by a few blank lines, which we
	   ignore */
	for( length = 0, lineCount = 0; 
		 length <= 0 && lineCount < FAILSAFE_ITERATIONS_MED; 
		 lineCount++ )
		{
		char buffer[ 1024 + 8 ];

		position = stell( &stream );
		status = readTextLine( ( READCHARFUNCTION ) sgetc, &stream, 
							   buffer, 1024, &length, NULL );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( buffer[ 0 ] == '-' )
			seenDash = TRUE;
		}
	if( lineCount >= FAILSAFE_ITERATIONS_MED )
		{
		sMemDisconnect( &stream );
		return( CRYPT_ERROR_BADDATA );
		}
	sseek( &stream, position );

	/* If the data starts with a dash check for PEM header encapsulation 
	   followed by a base64-encoded body */
	if( seenDash )
		{
		status = checkPEMHeader( &stream, &position );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( !checkBase64( &stream, TRUE ) )
			{
			sMemDisconnect( &stream );
			return( CRYPT_ERROR_BADDATA );
			}
		sMemDisconnect( &stream );
		*format = CRYPT_CERTFORMAT_TEXT_CERTIFICATE;
		*startPos = position;

		return( CRYPT_OK );
		}

	/* Check for non-encapsulated base64 data */
	if( checkBase64( &stream, FALSE ) )
		{
		sMemDisconnect( &stream );
		*format = CRYPT_CERTFORMAT_TEXT_CERTIFICATE;
		*startPos = position;

		return( CRYPT_OK );
		}
	sseek( &stream, position );

	/* It doesn't look like base64 encoded data, check for an S/MIME header */
	for( length = 1, lineCount = 0;
		 length > 0 && lineCount < FAILSAFE_ITERATIONS_MED;
		 lineCount++ )
		{
		char buffer[ 1024 + 8 ];

		status = readTextLine( ( READCHARFUNCTION ) sgetc, &stream, 
							   buffer, 1024, &length, NULL );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( !seenTransferEncoding && length >= 33 && \
			!strCompare( buffer, "Content-Transfer-Encoding:", 26 ) )
			{
			int index = strSkipWhitespace( buffer + 26, length - 26 );

			/* Check for a valid content encoding type */
			if( index < 0 || index > 1024 - 26 )
				continue;
			index += 26;	/* Skip "Content-Transfer-Encoding:" */
			if( length - index < 6 )
				{
				/* It's too short to be a valid encoding type, skip it */
				continue;
				}
			if( !strCompare( buffer + index, "base64", 6 ) )
				seenTransferEncoding = TRUE;
			else
				{
				if( !strCompare( buffer + index, "binary", 6 ) )
					seenTransferEncoding = isBinaryEncoding = TRUE;
				}
			}
		}
	if( lineCount >= FAILSAFE_ITERATIONS_MED || !seenTransferEncoding )
		{
		sMemDisconnect( &stream );
		return( CRYPT_ERROR_BADDATA );
		}
	position = stell( &stream );

	/* Make sure that the content is some form of encoded certificate using
	   the same check as the one that we used earlier */
	if( isBinaryEncoding )
		{
		if( dataLength < 32000L )
			status = readSequenceI( &stream, NULL );
		else
			status = readLongSequence( &stream, NULL );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_BADDATA );
		*startPos = position;
		*format = CRYPT_CERTFORMAT_CERTIFICATE;

		return( CRYPT_OK );
		}
	isBase64 = checkBase64( &stream, FALSE );
	sMemDisconnect( &stream );
	if( !isBase64 )
		return( CRYPT_ERROR_BADDATA );
	*startPos = position;
	*format = CRYPT_ICERTFORMAT_SMIME_CERTIFICATE;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Base64 Decoding Functions						*
*																			*
****************************************************************************/

/* Decode a chunk of four base64 characters into three binary characters */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int decodeBase64chunk( INOUT STREAM *stream,
							  IN_BUFFER( srcLeft ) const char *src, 
							  IN_LENGTH const int srcLeft,
							  const BOOLEAN fixedLenData )
	{
	int c0, c1, c2 = 0, c3 = 0, cx;
	int srcIndex = 0, outByteCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( src, srcLeft ) );

	REQUIRES( srcLeft > 0 && srcLeft < MAX_INTLENGTH );

	/* Make sure that there's sufficient input left to decode.  We need at
	   least two more characters to produce one byte of output */
	if( srcLeft < 2 )
		return( CRYPT_ERROR_UNDERFLOW );

	/* Decode a block of data from the input buffer */
	c0 = byteToInt( decode( src[ srcIndex++ ] ) );
	c1 = byteToInt( decode( src[ srcIndex++ ] ) );
	if( srcLeft > 2 )
		{
		c2 = byteToInt( decode( src[ srcIndex++ ] ) );
		if( srcLeft > 3 )
			c3 = byteToInt( decode( src[ srcIndex++ ] ) );
		}
	cx = c0 | c1 | c2 | c3;
	if( cx == BERR || cx == BEOF )
		{
		/* If we're decoding fixed-length data and the decoding produces
		   an invalid character or an EOF, there's a problem with the
		   input */
		if( fixedLenData )
			return( CRYPT_ERROR_BADDATA );

		/* We're decoding indefinite-length data for which EOFs are valid
		   characters.  We have to be a bit careful with the order of
		   checking since hitting an EOF at an earlier character may cause
		   later input data to be decoded as BERR */
		if( c0 == BEOF )
			{
			/* No more input, we're done */
			return( OK_SPECIAL );
			}
		if( c0 == BERR || c1 == BEOF || c1 == BERR )
			{
			/* We can't produce output with only one character of input 
			   data, there's a problem with the input */
			return( CRYPT_ERROR_BADDATA );
			}
		if( c2 == BEOF )
			{
			/* Two characters of input, then EOF, resulting in one character 
			   of output */
			outByteCount = 1;
			}
		else
			{
			if( c2 == BERR || c3 == BERR )
				return( CRYPT_ERROR_BADDATA );
			ENSURES( c3 == BEOF );
			outByteCount = 2;
			}
		}
	else
		{
		/* All decoded characters are valid */
		outByteCount = ( srcLeft > 4 ) ? 3 : srcLeft - 1;
		}
	ENSURES( outByteCount > 0 && outByteCount < 4 );

	/* Write the decoded data to the output buffer */
	status = sputc( stream, ( ( c0 << 2 ) | ( c1 >> 4 ) ) & 0xFF );
	if( outByteCount > 1 )
		{
		status = sputc( stream, ( ( c1 << 4 ) | ( c2 >> 2 ) ) & 0xFF );
		if( outByteCount > 2 )
			status = sputc( stream, ( ( c2 << 6 ) | c3 ) & 0xFF );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* If we've reached the end of the input, let the caller know */
	return( ( outByteCount < 3 ) ? OK_SPECIAL : CRYPT_OK );
	}

/* Decode a block of binary data from the base64 format, returning the total
   number of decoded bytes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int fixedBase64decode( STREAM *stream,
							  IN_BUFFER( srcLen ) const char *src, 
							  IN_LENGTH_MIN( 10 ) const int srcLen )
	{
	int srcIndex;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( src, srcLen ) );

	REQUIRES( srcLen >= 10 && srcLen < MAX_INTLENGTH );

	/* Decode the base64 string as a fixed-length continuous string without
	   padding or newlines.  Since we're processing arbitrary-sized input we 
	   can't use the usual FAILSAFE_ITERATIONS_MAX to bound the loop because 
	   the input could be larger than this so we use MAX_INTLENGTH instead */
	for( srcIndex = 0; srcIndex < srcLen && \
					   srcIndex < MAX_INTLENGTH; srcIndex += 4 )
		{
		int status;

		status = decodeBase64chunk( stream, src + srcIndex, 
									srcLen - srcIndex, TRUE );
		if( cryptStatusError( status ) )
			{
			/* If we've reached the end of the input, we're done */
			if( status == OK_SPECIAL )
				break;

			return( status );
			}
		}
	ENSURES( srcIndex < MAX_INTLENGTH );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64decode( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
				  IN_LENGTH_MIN( 10 ) const int destMaxLen, 
				  OUT_LENGTH_Z int *destLen,
				  IN_BUFFER( srcLen ) const char *src, 
				  IN_LENGTH_MIN( 10 ) const int srcLen, 
				  IN_ENUM_OPT( CRYPT_CERTFORMAT ) \
					const CRYPT_CERTFORMAT_TYPE format )
	{
	STREAM stream;
	int srcIndex, lineByteCount, lineSize = 0, status = DUMMY_INIT;

	assert( destMaxLen > 10 && isWritePtr( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( srcLen > 10 && isReadPtr( src, srcLen ) );

	REQUIRES( destMaxLen > 10 && destMaxLen < MAX_INTLENGTH );
	REQUIRES( srcLen > 10 && srcLen < MAX_INTLENGTH );
	REQUIRES( format >= CRYPT_CERTFORMAT_NONE && \
			  format < CRYPT_CERTFORMAT_LAST );

	/* Clear return values */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	sMemOpen( &stream, dest, destMaxLen );

	/* If it's not a certificate, it's a straight base64 string and we can
	   use the simplified decoding routines */
	if( format == CRYPT_CERTFORMAT_NONE )
		{
		status = fixedBase64decode( &stream, src, srcLen );
		if( cryptStatusOK( status ) )
			*destLen = stell( &stream );
		sMemDisconnect( &stream );
		return( status );
		}

	/* Decode the encoded object.  Since we're processing arbitrary-sized 
	   input we can't use the usual FAILSAFE_ITERATIONS_MAX to bound the 
	   loop because the input could be larger than this so we use 
	   MAX_INTLENGTH instead */
	for( srcIndex = 0, lineByteCount = 0;
		 srcIndex < srcLen && srcIndex < MAX_INTLENGTH;
		 srcIndex += 4, lineByteCount += 4 )
		{
		/* Depending on implementations, the length of the base64-encoded
		   line can vary from BASE64_MIN_LINESIZE to 72 characters.  We 
		   adjust for this by checking for the first EOL and setting the 
		   line length to the size of the first line of base64 text */
		if( lineSize <= 0 && \
			( src[ srcIndex ] == '\r' || src[ srcIndex ] == '\n' ) )
			{
			if( lineByteCount < BASE64_MIN_LINESIZE || lineByteCount > 128 )
				{
				/* Suspiciously short or long text line */
				sMemDisconnect( &stream );
				return( CRYPT_ERROR_BADDATA );
				}
			lineSize = lineByteCount;
			}

		/* If we've reached the end of a line of text, look for the EOL
		   marker */
		if( lineSize > 0 && lineByteCount >= lineSize )
			{
			int eolDataSize;

			status = checkEOL( src + srcIndex, srcLen - srcIndex, 
							   &eolDataSize, format );
			if( cryptStatusError( status ) )
				{
				/* If we get an OK_SPECIAL status it means that we've 
				   reached the EOF rather than just an EOL */
				if( status == OK_SPECIAL )
					{
					status = CRYPT_OK;
					break;
					}

				sMemDisconnect( &stream );
				return( status );
				}
			srcIndex += eolDataSize;
			lineByteCount = 0;
			}

		/* Decode a chunk of data from the input buffer */
		status = decodeBase64chunk( &stream, src + srcIndex, 
									srcLen - srcIndex, FALSE );
		if( cryptStatusError( status ) )
			{
			/* If we've reached the end of the input, we're done.  Note that 
			   we can't just wait for srcIndex to pass srcLen as for the 
			   fixed-length decode because there could be extra trailer data 
			   following the base64 data.

			   In theory we could call checkEOL() here to make sure that the
			   trailer is well-formed but if the data is truncated right on
			   the base64 end marker then this would produce an error so we
			   just stop decoding as soon as we find the end marker */
			if( status == OK_SPECIAL )
				{
				status = CRYPT_OK;
				break;
				}

			sMemDisconnect( &stream );
			return( status );
			}
		}
	ENSURES( srcIndex < MAX_INTLENGTH );
	if( cryptStatusOK( status ) )
		*destLen = stell( &stream );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

/* Calculate the size of a quantity of data once it's en/decoded */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int base64decodeLen( IN_BUFFER( dataLength ) const char *data, 
					 IN_LENGTH_MIN( 10 ) const int dataLength,
					 OUT_LENGTH_Z int *decodedLength )
	{
	STREAM stream;
	int ch, length = DUMMY_INIT, iterationCount;

	assert( isReadPtr( data, dataLength ) );
	assert( isWritePtr( decodedLength, sizeof( int ) ) );

	REQUIRES( dataLength >= 10 && dataLength < MAX_INTLENGTH );

	/* Clear return value */
	*decodedLength = 0;

	/* Skip ahead until we find the end of the decodable data.  This ignores 
	   errors on the input stream since at this point all that we're 
	   interested in is how much we can decode from it and not whether it's 
	   valid or not.  Handling this gets a bit complicated since once the 
	   stream enters an error state stell() doesn't try and return a stream 
	   position any more because the stream is in the error state, so we have 
	   to check the position before every read.
	   
	   Since we're processing arbitrary-sized input we can't use the usual
	   FAILSAFE_ITERATIONS_MAX to bound the loop because the input could be
	   larger than this so we use MAX_INTLENGTH instead */
	sMemConnect( &stream, data, dataLength );
	for( iterationCount = 0; iterationCount < MAX_INTLENGTH; iterationCount++ )
		{
		length = stell( &stream );
		ch = sgetc( &stream );
		if( cryptStatusError( ch ) || ch == BPAD )
			break;
		if( ch == '\r' || ch == '\n' )
			{
			/* Don't try and decode out-of-band data */
			continue;
			}
		ch = byteToInt( decode( ch ) );
		if( ch == BERR || ch == BEOF )
			break;
		}
	ENSURES( iterationCount < MAX_INTLENGTH );
	sMemDisconnect( &stream );

	/* Return a rough estimate of how much room the decoded data will occupy.
	   This ignores the EOL size so it always overestimates, but a strict
	   value isn't necessary since it's only used for memory buffer
	   allocation */
	*decodedLength = ( length * 3 ) / 4;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Base64 Encoding Functions						*
*																			*
****************************************************************************/

/* Calculate the size of a quantity of data once it's encoded */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int base64encodeLen( IN_LENGTH_MIN( 10 ) const int dataLength,
					 OUT_LENGTH_Z int *encodedLength,
					 IN_ENUM_OPT( CRYPT_CERTTYPE ) \
						const CRYPT_CERTTYPE_TYPE certType )
	{
	int length = roundUp( ( dataLength * 4 ) / 3, 4 ), headerInfoIndex;

	assert( isWritePtr( encodedLength, sizeof( int ) ) );

	REQUIRES( dataLength >= 10 && dataLength < MAX_INTLENGTH );
	REQUIRES( certType >= CRYPT_CERTTYPE_NONE && \
			  certType < CRYPT_CERTTYPE_LAST );
	
	ENSURES( length >= 10 && length < MAX_INTLENGTH );

	/* Clear return value */
	*encodedLength = 0;

	/* If we're encoding the data as a raw base64 string then we're done */
	if( certType == CRYPT_CERTTYPE_NONE )
		{
		*encodedLength = length;

		return( CRYPT_OK );
		}

	/* Find the header/trailer info for this format */
	for( headerInfoIndex = 0;
		 headerInfo[ headerInfoIndex ].type != certType && \
			headerInfo[ headerInfoIndex ].type != CRYPT_CERTTYPE_NONE && \
			headerInfoIndex < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO ); 
		 headerInfoIndex++ );
	ENSURES( headerInfoIndex < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO ) );
	ENSURES( headerInfo[ headerInfoIndex ].type != CRYPT_CERTTYPE_NONE );

	/* Calculate the extra length due to EOLs and delimiters */
	length += ( ( roundUp( length, BASE64_LINESIZE ) / BASE64_LINESIZE ) * EOL_LEN );
	length = headerInfo[ headerInfoIndex ].headerLen + length + \
			 headerInfo[ headerInfoIndex ].trailerLen;

	ENSURES( length > 10 && length < MAX_INTLENGTH );

	*encodedLength = length;

	return( CRYPT_OK );
	}

/* Encode a block of binary data into the base64 format */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64encode( OUT_BUFFER( destMaxLen, *destLen ) char *dest, 
				  IN_LENGTH_MIN( 10 ) const int destMaxLen, 
				  OUT_LENGTH_Z int *destLen,
				  IN_BUFFER( srcLen ) const void *src, 
				  IN_LENGTH_MIN( 10 ) const int srcLen, 
				  IN_ENUM_OPT( CRYPT_CERTTYPE ) \
					const CRYPT_CERTTYPE_TYPE certType )
	{
	STREAM stream;
	const BYTE *srcPtr = src;
	int srcIndex, lineByteCount, remainder = srcLen % 3;
	int headerInfoIndex = DUMMY_INIT, status = DUMMY_INIT;

	assert( destMaxLen > 10 && isWritePtr( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( srcLen >= 10 && isReadPtr( src, srcLen ) );

	REQUIRES( destMaxLen >= 10 && destMaxLen > srcLen && \
			  destMaxLen < MAX_INTLENGTH );
	REQUIRES( srcLen >= 10 && srcLen < MAX_INTLENGTH );
	REQUIRES( certType >= CRYPT_CERTTYPE_NONE && \
			  certType < CRYPT_CERTTYPE_LAST );

	/* Clear return values */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	sMemOpen( &stream, dest, destMaxLen );

	/* If it's an encoded certificate object rather than raw base64 data, 
	   add the header */
	if( certType != CRYPT_CERTTYPE_NONE )
		{
		for( headerInfoIndex = 0;
			 headerInfo[ headerInfoIndex ].type != certType && \
				headerInfo[ headerInfoIndex ].type != CRYPT_CERTTYPE_NONE && \
				headerInfoIndex < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO );
			 headerInfoIndex++ );
		ENSURES( headerInfoIndex < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO ) );
		ENSURES( headerInfo[ headerInfoIndex ].type != CRYPT_CERTTYPE_NONE );
		status = swrite( &stream, headerInfo[ headerInfoIndex ].header, 
						 headerInfo[ headerInfoIndex ].headerLen );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}

	/* Encode the data */
	for( srcIndex = 0, lineByteCount = 0; 
		 srcIndex < srcLen; 
		 lineByteCount += 4 )
		{
		const int srcLeft = srcLen - srcIndex;

		/* If we've reached the end of a line of binary data and it's a
		   certificate object rather than a raw binary blob, add the EOL 
		   marker */
		if( certType != CRYPT_CERTTYPE_NONE && \
			lineByteCount >= BASE64_LINESIZE )
			{
			status = swrite( &stream, EOL, EOL_LEN );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			lineByteCount = 0;
			}

		/* Encode a block of data from the input buffer */
		sputc( &stream, encode( ( srcPtr[ srcIndex ] >> 2 ) & 0x3F ) );
		if( srcLeft < 2 )
			{
			REQUIRES( remainder == 1 );
			status = sputc( &stream, encode( ( srcPtr[ srcIndex ] << 4 ) & 0x30 ) );
			break;
			}
		sputc( &stream, encode( ( ( srcPtr[ srcIndex ] << 4 ) & 0x30 ) | \
								( ( srcPtr[ srcIndex + 1 ] >> 4 ) & 0x0F ) ) );
		srcIndex++;
		if( srcLeft < 3 )
			{
			REQUIRES( remainder == 2 );
			status = sputc( &stream, encode( ( srcPtr[ srcIndex ] << 2 ) & 0x3C ) );
			break;
			}
		sputc( &stream, encode( ( ( srcPtr[ srcIndex ] << 2 ) & 0x3C ) | \
								( ( srcPtr[ srcIndex + 1 ] >> 6 ) & 0x03 ) ) );
		srcIndex++;
		status = sputc( &stream, encode( srcPtr[ srcIndex++ ] & 0x3F ) );
		if( cryptStatusError( status ) )
			break;
		}
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* If it's a certificate object, add any required padding and the 
	   trailer */
	if( certType != CRYPT_CERTTYPE_NONE )
		{
		/* Add any necessary padding.  For 0 bytes of remainder there's no 
		   padding (the data fits exactly), for 1 byte of remainder there's 
		   2 bytes of padding ("X=="), and for 2 bytes of remainder there's 
		   1 byte of padding ("XX=") */
		if( remainder > 0 )
			{
			status = sputc( &stream, BPAD );
			if( remainder == 1 )
				status = sputc( &stream, BPAD );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			}

		/* Add the trailer */
		swrite( &stream, EOL, EOL_LEN );
		status = swrite( &stream, headerInfo[ headerInfoIndex ].trailer, 
						 headerInfo[ headerInfoIndex ].trailerLen );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}
	*destLen = stell( &stream );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}
