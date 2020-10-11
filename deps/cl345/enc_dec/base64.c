/****************************************************************************
*																			*
*							cryptlib Base64 Routines						*
*						Copyright Peter Gutmann 1992-2016					*
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

#ifdef USE_BASE64

/* Base64 encode/decode tables from RFC 1113 */

#define BPAD		'='		/* Padding for odd-sized output */
#define BEOL		0x80	/* CR or LF */
#define BEOF		0x81	/* EOF marker '=' */
#define BERR		0xFF	/* Illegal character marker */

static const char binToAscii[] = \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const BYTE asciiToBin[ 256 ] = { 
	BERR, BERR, BERR, BERR, BERR, BERR, BERR, BERR,	/* 00 */
	BERR, BERR, BEOL, BERR, BERR, BEOL, BERR, BERR,
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

/* base64 data can be preceded by all manner of additional header lines, 
   blank lines, PEM type-and-value pairs, and so on, the following is the
   maximum number of lines that we'll accept before declaring an error */

#define MAX_HEADER_LINES	30

/* The size of the buffer for reading a line of text */

#define LINEBUFFER_SIZE		512

/* Basic single-character en/decode functions.  We mask the value to 6 or 8 
   bits both as a range check and to avoid generating negative array offsets 
   if the sign bit is set, since the strings are passed as 'char *'s */

#define encode( data )	binToAscii[ ( data ) & 0x3F ]
#define decode( data )	asciiToBin[ ( data ) & 0xFF ]

/* The headers and trailers used for base64-encoded certificate objects.  
   These are used for both en- and de-coding, thus the presence of 
   apparently redundant entries such as the duplicated "-----BEGIN NEW 
   CERTIFICATE REQUEST-----" */

typedef struct {
	const CRYPT_CERTTYPE_TYPE type;
	BUFFER_FIXED( headerLen ) \
	const char *header;
	const int headerLen;
	BUFFER_FIXED( trailerLen ) \
	const char *trailer;
	const int trailerLen;
	} HEADER_INFO;
static const HEADER_INFO headerInfo[] = {
	{ CRYPT_CERTTYPE_CERTIFICATE,
	  "-----BEGIN CERTIFICATE-----" EOL, 27 + EOL_LEN,
	  "-----END CERTIFICATE-----" EOL, 25 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTIFICATE,	/* Alternative form */
	  "-----BEGIN X509 CERTIFICATE-----" EOL, 32 + EOL_LEN,
	  "-----END X509 CERTIFICATE-----" EOL, 30 + EOL_LEN },
	{ CRYPT_CERTTYPE_ATTRIBUTE_CERT,
	  "-----BEGIN ATTRIBUTE CERTIFICATE-----" EOL, 37 + EOL_LEN,
	  "-----END ATTRIBUTE CERTIFICATE-----" EOL, 35 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTCHAIN,
	  "-----BEGIN CERTIFICATE CHAIN-----" EOL, 33 + EOL_LEN,
	  "-----END CERTIFICATE CHAIN-----" EOL, 31 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTCHAIN,		/* Alternative form */
	  "-----BEGIN PKCS7-----" EOL, 21 + EOL_LEN,
	  "-----END PKCS7-----" EOL, 29 + EOL_LEN },
#ifdef USE_CERTREQ
	{ CRYPT_CERTTYPE_CERTREQUEST,
	  "-----BEGIN NEW CERTIFICATE REQUEST-----" EOL, 39 + EOL_LEN,
	  "-----END NEW CERTIFICATE REQUEST-----" EOL, 37 + EOL_LEN },
	{ CRYPT_CERTTYPE_CERTREQUEST,	/* Alternative form */
	  "-----BEGIN CERTIFICATE REQUEST-----" EOL, 35 + EOL_LEN,
	  "-----END CERTIFICATE REQUEST-----" EOL, 33 + EOL_LEN },
	{ CRYPT_CERTTYPE_REQUEST_CERT,	/* See note above */
	  "-----BEGIN NEW CERTIFICATE REQUEST-----" EOL, 39 + EOL_LEN,
	  "-----END NEW CERTIFICATE REQUEST-----" EOL, 37 + EOL_LEN },
#endif /* USE_CERTREQ */
#ifdef USE_CERTREV
	{ CRYPT_CERTTYPE_CRL,
	  "-----BEGIN CERTIFICATE REVOCATION LIST-----"  EOL, 43 + EOL_LEN,
	  "-----END CERTIFICATE REVOCATION LIST-----" EOL, 41 + EOL_LEN },
	{ CRYPT_CERTTYPE_CRL,			/* Alternative form */
	  "-----BEGIN X509 CRL-----"  EOL, 24 + EOL_LEN,
	  "-----END X509 CRL-----" EOL, 22 + EOL_LEN },
#endif /* USE_CERTREV */
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

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkBase64( INOUT STREAM *stream, const BOOLEAN isPEM )
	{
	BYTE buffer[ BASE64_MAX_LINESIZE + 8 ];
	BYTE decodeBuffer[ BASE64_MAX_LINESIZE + 8 ];
	int decodedLen, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isPEM == TRUE || isPEM == FALSE );

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
	   filter won't catch this one particular case, however it'll be caught
	   by the next check because '-' and the ':' after the MIME-Version 
	   aren't valid base64 charactes */
	if( memcmp( buffer, "MI", 2 ) && \
		memcmp( buffer, "AA", 2 ) && \
		memcmp( buffer, "mQ", 2 ) )
		return( FALSE );

	/* Check that we have at least one minimal-length line of valid base64-
	   encoded data */
	status = base64decode( decodeBuffer, BASE64_MAX_LINESIZE, &decodedLen,
						   buffer, BASE64_MIN_LINESIZE, CRYPT_CERTFORMAT_NONE );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* If it's supposed to be PEM data, check that it's delimited by 
	   linebreaks, or at least that there's a linebreak visible before 
	   encountering BASE64_MAX_LINESIZE characters */
	if( isPEM )
		{
		BOOLEAN hasLineBreak = FALSE;
		int i, LOOP_ITERATOR;

		status = sread( stream, buffer, 
						( BASE64_MAX_LINESIZE - BASE64_MIN_LINESIZE ) + 1 );
		if( cryptStatusError( status ) )
			return( FALSE );
		LOOP_LARGE( i = 0, i < ( BASE64_MAX_LINESIZE - \
								 BASE64_MIN_LINESIZE ) + 1, i++ )
			{
			const int ch = byteToInt( decode( buffer[ i ] ) );
			if( ch == BEOL || ch == BEOF || ch == BERR )
				{
				hasLineBreak = TRUE;
				break;
				}
			}
		ENSURES_B( LOOP_BOUND_OK );
		if( !hasLineBreak )
			return( FALSE );
		}

	return( TRUE );
	}

/* Check for PEM-encapsulated data.  All that we need to look for is the
   '-----..' header, which is fairly simple although we also need to handle
   SSH's mutant '---- ...' variant (4 dashes and a space) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkPEMHeader( INOUT STREAM *stream, 
						   OUT_DATALENGTH_Z int *headerLength )
	{
	BOOLEAN isSSH = FALSE, isPGP = FALSE;
	char buffer[ LINEBUFFER_SIZE + 8 ], *bufPtr = buffer;
	int length, position DUMMY_INIT, lineCount, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( headerLength, sizeof( int ) ) );

	/* Clear return value */
	*headerLength = 0;

	/* Check for the initial 5 dashes and 'BEGIN ' (unless we're SSH, in
	   which case we use 4 dashes, a space, and 'BEGIN ') */
	status = readTextLine( ( READCHAR_FUNCTION ) sgetc, stream, 
						   buffer, LINEBUFFER_SIZE, &length, NULL, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	if( length < 5 + 6 + 5 + 5 )
		{
		/* We need room for at least the shortest possible string, 
		   '-----' (5) + 'BEGIN ' (6) + 'PKCS7' (5) + '-----' (5) */
		return( CRYPT_ERROR_BADDATA );
		}
	if( memcmp( bufPtr, "-----BEGIN ", 11 ) &&	/* PEM/PGP form */
		memcmp( bufPtr, "---- BEGIN ", 11 ) )	/* SSH form */
		return( CRYPT_ERROR_BADDATA );
	bufPtr += 11;
	length -= 11;

	/* Skip the object name.  We know that we have enough data present for 
	   the compare because we've just checked it above */
	if( !strCompare( bufPtr, "SSH2 ", 5 ) )
		isSSH = TRUE;
	else
		{
		if( !strCompare( bufPtr, "PGP ", 4 ) )
			isPGP = TRUE;
		}
	LOOP_EXT_CHECKINC( length >= 4, length--, LINEBUFFER_SIZE + 1 )
		{
		if( *bufPtr == '-' )
			break;
		bufPtr++;
		}
	ENSURES( LOOP_BOUND_OK );
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
	LOOP_MED( lineCount = 0, lineCount < MAX_HEADER_LINES, lineCount++ )
		{
		position = stell( stream );
		status = readTextLine( ( READCHAR_FUNCTION ) sgetc, stream, 
							   buffer, LINEBUFFER_SIZE, &length, NULL, 
							   FALSE );
		if( cryptStatusError( status ) )
			return( status );
		if( isSSH && strFindCh( buffer, length, ':' ) < 0 )
			break;
		if( isPGP && length <= 0 )
			break;
		}
	ENSURES( LOOP_BOUND_OK );
	if( lineCount >= MAX_HEADER_LINES )
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
   CRYPT_CERTFORMAT_TEXT_CERTIFICATE this will give us '-----END...' (or
   SSH's mutant variant '---- END...') on the next line which is easy to 
   check for, but for CRYPT_ICERTFORMAT_SMIME_CERTIFICATE what we end up 
   with depends on the calling code.  
   
   It could either truncate immediately at the end of the data (which it 
   isn't supposed to) so we get '\0', it could truncate after the EOL (so 
   we get EOL + '\0'), it could continue with a futher content type after a 
   blank line (so we get EOL + EOL), or it could truncate without the '\0' 
   so we get garbage, which is the caller's problem.  
   
   Because of this we look for all of these situations and, if any are 
   found, return an OK_SPECIAL EOL indicator */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int checkEOL( IN_BUFFER( srcLen ) const BYTE *src, 
					 IN_DATALENGTH const int srcLen,
					 OUT_DATALENGTH_Z int *eolSize,
					 IN_ENUM( CRYPT_CERTFORMAT ) \
						const CRYPT_CERTFORMAT_TYPE format )
	{
	int srcIndex = 0;

	assert( isReadPtrDynamic( src, srcLen ) );
	assert( isWritePtr( eolSize, sizeof( int ) ) );

	REQUIRES( srcLen > 0 && srcLen < MAX_BUFFER_SIZE );
	REQUIRES( isEnumRange( format, CRYPT_CERTFORMAT ) );

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
		{
		/* We're at EOF, not just EOL */
		return( OK_SPECIAL );
		}

	/* Check for '\0' or EOL (S/MIME), '----END...' (PEM), or
	   '---- END' (SSH) after EOL */
	if( format == CRYPT_ICERTFORMAT_SMIME_CERTIFICATE )
		{
		if( src[ srcIndex ] == '\0' || src[ srcIndex ] == '\n' || \
			src[ srcIndex ] == '\r' )
			{
			/* We're at EOF, not just EOL */
			return( OK_SPECIAL );
			}
		}
	if( format == CRYPT_CERTFORMAT_TEXT_CERTIFICATE && \
		srcIndex + 9 <= srcLen && \
		src[ srcIndex ] == '-' )
		{
		if( !strCompare( src + srcIndex, "-----END ", 9 ) )
			return( OK_SPECIAL );	/* PEM EOF */
		if( !strCompare( src + srcIndex, "---- END ", 9 ) )
			return( OK_SPECIAL );	/* SSH EOF */
		}

	/* If we were expecting an EOL but didn't find one then there's a 
	   problem with the data */
	if( srcIndex <= 0 )
		return( CRYPT_ERROR_BADDATA );

	ENSURES( srcIndex > 0 && srcIndex < srcLen );
	*eolSize = srcIndex;

	return( CRYPT_OK );
	}

/* Check whether a data item has a header that identifies it as some form of
   encoded object and return the start position of the encoded data.  For
   S/MIME certificate data this can in theory get quite complex because
   there are many possible variations in the headers.  
   
   Some early S/MIME agents used a content type of "application/x-pkcs7-mime",
   "application/x-pkcs7-signature", and "application/x-pkcs10", while newer
   ones use the same without the "x-" at the start.  In addition Netscape
   have their own MIME data types for certificates, "application/x-x509-"
   "{user-cert|ca-cert|email-cert}, and this tradition is perpetuated by the
   mass of further types in the neverending stream of RFCs that PKIX churns 
   out.
   
   There are a whole pile of other possible headers as well, none of them 
   terribly relevant for our purposes, so all that we check for is the base64 
   indicator */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64checkHeader( IN_BUFFER( dataLength ) const BYTE *data, 
					   IN_DATALENGTH_MIN( MIN_CERTSIZE ) \
							const int dataLength,
					   OUT_ENUM_OPT( CRYPT_CERTFORMAT ) \
							CRYPT_CERTFORMAT_TYPE *format,
					   OUT_DATALENGTH_Z int *startPos )
	{
	STREAM stream;
	BOOLEAN seenTransferEncoding = FALSE, isBinaryEncoding = FALSE;
	BOOLEAN seenDash = FALSE, isBase64;
	int position DUMMY_INIT, lineCount, length, status, LOOP_ITERATOR;

	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( format, sizeof( CRYPT_CERTFORMAT_TYPE ) ) );
	assert( isWritePtr( startPos, sizeof( int ) ) );

	REQUIRES( dataLength >= MIN_CERTSIZE && dataLength < MAX_BUFFER_SIZE );

	/* Clear return values */
	*format = CRYPT_CERTFORMAT_NONE;
	*startPos = 0;

	sMemConnect( &stream, data, dataLength );

	/* Perform a quick check to weed out unencoded certificate data, which 
	   is usually the case.  Certificates and related objects are always an 
	   ASN.1 SEQUENCE so if we find data that begins with this value then we
	   perform the check for a certificate object */
	if( sPeek( &stream ) == BER_SEQUENCE )
		{
		/* For very large objects (which can only be CRLs) we can get an 
		   overflow error trying to read a short length so if the length is 
		   suspiciously long we allow a long length.  We don't do this 
		   unconditionally in order to reduce potential false positives */
		if( dataLength < MAX_INTLENGTH_SHORT )
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
	LOOP_MED( ( length = 0, lineCount = 0 ), 
			  length <= 0 && lineCount < MAX_HEADER_LINES, lineCount++ )
		{
		char buffer[ LINEBUFFER_SIZE + 8 ];

		position = stell( &stream );
		status = readTextLine( ( READCHAR_FUNCTION ) sgetc, &stream, 
							   buffer, LINEBUFFER_SIZE, &length, NULL, 
							   FALSE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( buffer[ 0 ] == '-' )
			seenDash = TRUE;
		}
	ENSURES( LOOP_BOUND_OK );
	if( lineCount >= MAX_HEADER_LINES )
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
	LOOP_MED( ( length = 1, lineCount = 0 ),
			  length > 0 && lineCount < MAX_HEADER_LINES, lineCount++ )
		{
		char buffer[ LINEBUFFER_SIZE + 8 ];

		status = readTextLine( ( READCHAR_FUNCTION ) sgetc, &stream, 
							   buffer, LINEBUFFER_SIZE, &length, NULL, 
							   TRUE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( !seenTransferEncoding && length >= ( 26 + 1 + 6 ) && \
			!strCompare( buffer, "Content-Transfer-Encoding:", 26 ) )
			{
			int bufPos = strSkipWhitespace( buffer + 26, length - 26 );

			/* Check for a valid content encoding type.  The first part of
			   the check verifies that there's a space present, which is a 
			   bit of an odd check because the RFCs that cover this (2045, 
			   2822) specify the encoding as "'Content-Transfer-Encoding:' 
			   mechanism" with no space between the colon and the mechanism 
			   string, however all known implementations add a space so we
			   check for this.  
			   
			   The second part of the check verifies that there's enough 
			   data left after the "Content-Transfer-Encoding:" and any
			   skipped whitespace for the encoding type:

										  26  bufPos  min = 6
										   |--->|----->
				+--------------------------+----+---------
				|Content-Transfer-Encoding:|	|xxxxxx...
				+--------------------------+----+--------- */
			if( bufPos < 1 || 26 + bufPos + 6 > length )
				continue;
			bufPos += 26;	/* Skip "Content-Transfer-Encoding:" */
			if( !strCompare( buffer + bufPos, "base64", 6 ) )
				seenTransferEncoding = TRUE;
			else
				{
				if( !strCompare( buffer + bufPos, "binary", 6 ) )
					seenTransferEncoding = isBinaryEncoding = TRUE;
				}
			}
		}
	ENSURES( LOOP_BOUND_OK );
	if( lineCount >= MAX_HEADER_LINES || !seenTransferEncoding )
		{
		sMemDisconnect( &stream );
		return( CRYPT_ERROR_BADDATA );
		}
	position = stell( &stream );

	/* Make sure that the content is some form of encoded certificate using
	   the same check as the one that we used earlier */
	if( isBinaryEncoding )
		{
		/* See the comment earlier in this function for the read strategy 
		   used here */
		if( dataLength < MAX_INTLENGTH_SHORT )
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

/* Decode a base64-encoded binary object.  This has to resort to a somewhat
   unusual decoding strategy because of the fact that some broken 
   implementations don't split lines across base64-encoded block boundaries,
   so that part of a base64-encoded block can start on one line but continue 
   on the next.  To deal with this, we shift the decoded data into an 
   accumulator and write out its contents once it's absorbed a full block.  
   This means that we can skip intervening EOL characters without needing to 
   have a full block on each line */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeAccumulator( INOUT STREAM *stream, 
							 const unsigned long accumulator,
							 IN_RANGE( 2, 4 ) const int byteCount )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( byteCount >= 2 && byteCount <= 4 );

	if( byteCount == 4 )
		{
		/* A full block of four bytes decoded to three */
		sputc( stream, intToByte( accumulator >> 16 ) );
		sputc( stream, intToByte( accumulator >> 8 ) );
		return( sputc( stream, intToByte( accumulator ) ) );
		}
	if( byteCount == 3 )
		{
		/* Three bytes decoded to two */
		sputc( stream, intToByte( accumulator >> 10 ) );
		return( sputc( stream, intToByte( accumulator >> 2 ) ) );
		}
	/* Two bytes decoded to one */
	return( sputc( stream, intToByte( accumulator >> 4 ) ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64decode( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
				  IN_DATALENGTH_MIN( 10 ) const int destMaxLen, 
				  OUT_DATALENGTH_Z int *destLen,
				  IN_BUFFER( srcLen ) const BYTE *src, 
				  IN_DATALENGTH_MIN( 10 ) const int srcLen, 
				  IN_ENUM_OPT( CRYPT_CERTFORMAT ) \
					const CRYPT_CERTFORMAT_TYPE format )
	{
	STREAM stream;
	unsigned long accumulator = 0;
	int srcIndex, byteCount = 0, status DUMMY_INIT, LOOP_ITERATOR;

	assert( destMaxLen > 10 && isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( srcLen >= 10 && isReadPtrDynamic( src, srcLen ) );

	REQUIRES( destMaxLen > 10 && destMaxLen < MAX_BUFFER_SIZE );
	REQUIRES( srcLen >= 10 && srcLen < MAX_BUFFER_SIZE );
	REQUIRES( isEnumRangeOpt( format, CRYPT_CERTFORMAT ) );

	/* Clear return values */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	sMemOpen( &stream, dest, destMaxLen );

	/* Decode the encoded object.  Since we're processing arbitrary-sized 
	   input we can't use the usual FAILSAFE_ITERATIONS_MAX to bound the 
	   loop because the input could be larger than this so we use 
	   MAX_BUFFER_SIZE instead */
	LOOP_EXT( srcIndex = 0,
			  srcIndex < srcLen && srcIndex < MAX_BUFFER_SIZE,
			  srcIndex++, MAX_BUFFER_SIZE )
		{
		const unsigned int value = byteToInt( decode( src[ srcIndex ] ) );

		/* Process special-case characters */
		if( value == BERR )
			{
			/* It's an invalid value */
			sMemDisconnect( &stream );
			return( CRYPT_ERROR_BADDATA );
			}
		if( value == BEOL )
			{
			int eolDataSize;

			/* If it's fixed-format data (so a pure base64 string) then 
			   there can't be an EOL present */
			if( format == CRYPT_CERTFORMAT_NONE )
				{
				sMemDisconnect( &stream );
				return( CRYPT_ERROR_BADDATA );
				}

			/* It's  CR or LF, skip it */
			status = checkEOL( src + srcIndex, srcLen - srcIndex, 
							   &eolDataSize, format );
			if( cryptStatusError( status ) )
				{
				/* If we get an OK_SPECIAL status then it means that we've 
				   reached the EOF */
				if( status == OK_SPECIAL )
					break;

				sMemDisconnect( &stream );
				return( status );
				}
			srcIndex += eolDataSize - 1;	/* Loop increments srcIndex */
			continue;
			}
		if( value == BEOF )
			{
			/* It's a '=' padding character, we've reached the end of the 
			   encoded data */
			break;
			}

		/* We've got another six bits of valid data, shift them into the 
		   accumulator */
		accumulator = ( accumulator << 6 ) | value;
		byteCount++;

		/* If we've filled the accumulator, write it to the output */
		if( byteCount >= 4 )
			{
			status = writeAccumulator( &stream, accumulator, byteCount );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}

			/* Reset the accumulator */
			accumulator = 0;
			byteCount = 0;
			}
		}
	ENSURES( LOOP_BOUND_OK );
	ENSURES( srcIndex < MAX_BUFFER_SIZE );

	/* If there's a single byte left then this is an error since it can't be 
	   decoded into a valid byte as it only represents six bits of data */
	if( byteCount == 1 )
		{
		sMemDisconnect( &stream );
		return( CRYPT_ERROR_BADDATA );
		}

	/* Write any leftover bytes in the accumulator */
	if( byteCount > 0 )
		{
		status = writeAccumulator( &stream, accumulator, byteCount );
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

/* Calculate the size of a quantity of data once it's en/decoded */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int base64decodeLen( IN_BUFFER( dataLength ) const BYTE *data, 
					 IN_DATALENGTH_MIN( 10 ) const int dataLength,
					 OUT_DATALENGTH_Z int *decodedLength )
	{
	STREAM stream;
	int length = 0, i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( data, dataLength ) );
	assert( isWritePtr( decodedLength, sizeof( int ) ) );

	REQUIRES( dataLength >= 10 && dataLength < MAX_BUFFER_SIZE );

	/* Clear return value */
	*decodedLength = 0;

	/* Skip ahead until we find the end of the decodable data.  This ignores 
	   base64 errors on the input stream since at this point all that we're 
	   interested in is how much we can decode from it and not whether it's 
	   valid or not.

	   Since we're processing arbitrary-sized input we can't use the usual
	   FAILSAFE_ITERATIONS_MAX to bound the loop because the input could be
	   larger than this so we use MAX_BUFFER_SIZE instead */
	sMemConnect( &stream, data, dataLength );
	LOOP_EXT( i = 0, i < dataLength, i++, MAX_BUFFER_SIZE + 1 )
		{
		int ch, status;

		status = ch = sgetc( &stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		if( ch == BPAD )
			break;
		if( ch == '\r' || ch == '\n' )
			{
			/* Don't try and decode out-of-band data */
			continue;
			}
		ch = byteToInt( decode( ch ) );
		if( ch == BERR || ch == BEOF )
			break;
		length++;
		}
	ENSURES( LOOP_BOUND_OK );
	sMemDisconnect( &stream );

	static_assert( MAX_BUFFER_SIZE < MAX_INTLENGTH / 3,
				   "Integer overflow check for base64" );
	ENSURES( length < MAX_INTLENGTH / 3 );

	/* Return a rough estimate of how much room the decoded data will occupy.
	   This overestimates by a few bytes, but a strict value isn't necessary 
	   since it's only used for memory buffer allocation */
	*decodedLength = ( length * 3 ) / 4;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Base64 Encoding Functions						*
*																			*
****************************************************************************/

/* Find the header/trailer info for a given export format */

static const HEADER_INFO *getHeaderInfo( IN_ENUM_OPT( CRYPT_CERTTYPE ) \
											const CRYPT_CERTTYPE_TYPE certType )
	{
	int index, LOOP_ITERATOR;

	REQUIRES_N( isEnumRangeOpt( certType, CRYPT_CERTTYPE ) );

	LOOP_MED( index = 0, headerInfo[ index ].type != CRYPT_CERTTYPE_NONE && \
						 index < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO ),
			  index++ )
		{
		if( headerInfo[ index ].type == certType )
			return( &headerInfo[ index ] );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( index < FAILSAFE_ARRAYSIZE( headerInfo, HEADER_INFO ) );

	retIntError_Null();
	}

/* Calculate the size of a quantity of data once it's encoded */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int base64encodeLen( IN_DATALENGTH_MIN( 10 ) const int dataLength,
					 OUT_DATALENGTH_Z int *encodedLength,
					 IN_ENUM_OPT( CRYPT_CERTTYPE ) \
						const CRYPT_CERTTYPE_TYPE certType )
	{
	const HEADER_INFO *headerInfoPtr;
	int length;

	assert( isWritePtr( encodedLength, sizeof( int ) ) );

	REQUIRES( dataLength >= 10 && dataLength < MAX_BUFFER_SIZE );
	REQUIRES( isEnumRangeOpt( certType, CRYPT_CERTTYPE ) );
	
	if( dataLength >= MAX_INTLENGTH / 4 )
		{
		/* Catch overflows in the length calculation, this is a can-never-
		   happen situation so the check is mostly to keep static 
		   analysers happy */
		return( CRYPT_ERROR_OVERFLOW );
		}
	length = roundUp( ( dataLength * 4 ) / 3, 4 );
	ENSURES( length >= 10 && length < MAX_BUFFER_SIZE );

	/* Clear return value */
	*encodedLength = 0;

	/* If we're encoding the data as a raw base64 string then we're done */
	if( certType == CRYPT_CERTTYPE_NONE )
		{
		*encodedLength = length;

		return( CRYPT_OK );
		}

	/* Find the header/trailer info for this format */
	headerInfoPtr = getHeaderInfo( certType );
	ENSURES( headerInfoPtr != NULL );

	/* Calculate the extra length due to EOLs and delimiters */
	length += ( ( roundUp( length, BASE64_LINESIZE ) / BASE64_LINESIZE ) * EOL_LEN );
	length += headerInfoPtr->headerLen + headerInfoPtr->trailerLen;

	ENSURES( length >= 64 && length < MAX_BUFFER_SIZE );

	*encodedLength = length;

	return( CRYPT_OK );
	}

/* Encode a block of binary data into the base64 format */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int base64encode( OUT_BUFFER( destMaxLen, *destLen ) char *dest, 
				  IN_DATALENGTH_MIN( 10 ) const int destMaxLen, 
				  OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen,
				  IN_BUFFER( srcLen ) const void *src, 
				  IN_DATALENGTH_MIN( 10 ) const int srcLen, 
				  IN_ENUM_OPT( CRYPT_CERTTYPE ) \
					const CRYPT_CERTTYPE_TYPE certType )
	{
	const HEADER_INFO *headerInfoPtr DUMMY_INIT_PTR;
	STREAM stream;
	const BYTE *srcPtr = src;
	int srcIndex, lineByteCount, remainder = srcLen % 3;
	int status DUMMY_INIT, LOOP_ITERATOR;

	assert( destMaxLen > 10 && isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( srcLen >= 10 && isReadPtrDynamic( src, srcLen ) );

	REQUIRES( destMaxLen >= 10 && destMaxLen > srcLen && \
			  destMaxLen < MAX_BUFFER_SIZE );
	REQUIRES( srcLen >= 10 && srcLen < MAX_BUFFER_SIZE );
	REQUIRES( isEnumRangeOpt( certType, CRYPT_CERTTYPE ) );

	/* Clear return values */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	sMemOpen( &stream, dest, destMaxLen );

	/* If it's an encoded certificate object rather than raw base64 data, 
	   add the header */
	if( certType != CRYPT_CERTTYPE_NONE )
		{
		headerInfoPtr = getHeaderInfo( certType );
		ENSURES( headerInfoPtr != NULL );
		status = swrite( &stream, headerInfoPtr->header, 
						 headerInfoPtr->headerLen );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}

	/* Encode the data */
	LOOP_EXT( ( srcIndex = 0, lineByteCount = 0 ), srcIndex < srcLen, 
			  lineByteCount += 4, MAX_BUFFER_SIZE / 4 )
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
	ENSURES( LOOP_BOUND_OK );
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
		status = swrite( &stream, headerInfoPtr->trailer, 
						 headerInfoPtr->trailerLen );
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
#endif /* USE_BASE64 */
