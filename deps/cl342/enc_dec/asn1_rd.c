/****************************************************************************
*																			*
*								ASN.1 Read Routines							*
*						Copyright Peter Gutmann 1992-2008					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "bn.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "bn/bn.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* When specifying a tag we can use either the default tag for the object
   (indicated with the value DEFAULT_TAG) or a special-case tag.  The 
   following macro selects the correct value.  Since these are all primitive 
   objects we force the tag type to a primitive tag */

#define selectTag( tag, defaultTag )	\
		( ( ( tag ) == DEFAULT_TAG ) ? ( defaultTag ) : \
									   ( MAKE_CTAG_PRIMITIVE( tag ) ) )

/* Read the length octets for an ASN.1 data type with special-case handling
   for long and short lengths and indefinite-length encodings.  The short-
   length read is limited to 32K, which is a sane limit for most PKI data 
   and one that doesn't cause type conversion problems on systems where 
   sizeof( int ) != sizeof( long ).  If the caller indicates that indefinite 
   lengths are OK for short lengths we return OK_SPECIAL if we encounter one.  
   Long length reads always allow indefinite lengths since these are quite 
   likely for large objects */

typedef enum {
	READLENGTH_NONE,		/* No length read behaviour */
	READLENGTH_SHORT,		/* Short length, no indef.allowed */
	READLENGTH_SHORT_INDEF,	/* Short length, indef.to OK_SPECIAL */
	READLENGTH_LONG_INDEF,	/* Long length, indef.to OK_SPECIAL */
	READLENGTH_LAST			/* Last possible read type */
	} READLENGTH_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readLengthValue( INOUT STREAM *stream, 
							OUT_LENGTH_Z long *length,
							IN_ENUM( READLENGTH ) const READLENGTH_TYPE readType )
	{
	BYTE buffer[ 8 + 8 ], *bufPtr = buffer;
	BOOLEAN shortLen = ( readType == READLENGTH_SHORT || \
						 readType == READLENGTH_SHORT_INDEF );
	long dataLength;
	int noLengthOctets, i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES_S( readType > READLENGTH_NONE && readType < READLENGTH_LAST );

	/* Clear return value */
	*length = 0;

	/* Read the first byte of length data.  If it's a short length, we're
	   done */
	status = dataLength = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( !( dataLength & 0x80 ) )
		{
		*length = dataLength;
		return( CRYPT_OK );
		}

	/* Read the actual length octets */
	noLengthOctets = dataLength & 0x7F;
	if( noLengthOctets <= 0 )
		{
		/* If indefinite lengths aren't allowed, signal an error */
		if( readType != READLENGTH_SHORT_INDEF && \
			readType != READLENGTH_LONG_INDEF )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

		/* It's an indefinite length encoding, we're done */
		*length = CRYPT_UNUSED;

		return( CRYPT_OK );
		}
	if( noLengthOctets > 8 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	status = sread( stream, buffer, noLengthOctets );
	if( cryptStatusError( status ) )
		return( status );

	/* Handle leading zero octets.  Since BER lengths can be encoded in 
	   peculiar ways (at least one text uses a big-endian 32-bit encoding 
	   for everything) we allow up to 8 bytes of non-DER length data, but 
	   only the last 2 or 4 of these (for short or long lengths 
	   respectively) can be nonzero */
	if( buffer[ 0 ] == 0 )
		{
		/* Oddball length encoding with leading zero(es) */
		for( i = 0; i < noLengthOctets && buffer[ i ] == 0; i++ );
		noLengthOctets -= i;
		if( noLengthOctets <= 0 )
			return( 0 );		/* Very broken encoding of a zero length */
		bufPtr += i;			/* Skip leading zero(es) */
		}

	/* Make sure that the length size is reasonable */
	if( noLengthOctets < 0 || noLengthOctets > ( shortLen ? 2 : 4 ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Read and check the length value */
	for( dataLength = 0, i = 0; i < noLengthOctets; i++ )
		{
		const long dataLenTmp = dataLength << 8;
		const int data = byteToInt( bufPtr[ i ] );

		if( dataLength >= ( MAX_INTLENGTH >> 8 ) || \
			dataLenTmp >= MAX_INTLENGTH - data )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		dataLength = dataLenTmp | data;
		if( dataLength <= 0 || dataLength >= MAX_INTLENGTH )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}
	if( shortLen )
		{
		if( dataLength & 0xFFFF8000UL || dataLength > 32767L )
			{
			/* Length must be < 32K for short lengths */
			return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
			}
		}
	else
		{
		if( ( dataLength & 0x80000000UL ) || dataLength >= MAX_INTLENGTH )
			{
			/* Length must be < MAX_INTLENGTH for standard data */
			return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
			}
		}
	if( dataLength <= 0 )
		{
		/* Shouldn't happen since the overflow check above catches it, but we 
		   check again just to be safe */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	ENSURES_S( dataLength > 0 && dataLength < MAX_INTLENGTH );
	*length = dataLength;

	return( CRYPT_OK );
	}

/* Read the header for a (signed) integer value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readIntegerHeader( INOUT STREAM *stream, 
							  IN_TAG_EXT const int tag )
	{
	long length;
	int noLeadingZeroes, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Read the identifier field if necessary and the length */
	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_INTEGER ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	status = readLengthValue( stream, &length, READLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		return( 0 );		/* Zero-length data */

	/* ASN.1 encoded values are signed while the internal representation is
	   unsigned so we skip any leading zero bytes needed to encode a value
	   that has the high bit set.  If we get a value with the (supposed) 
	   sign bit set we treat it as an unsigned value since a number of 
	   implementations get this wrong.  As with length encodings, we allow
	   up to 8 bytes of non-DER leading zeroes */
	for( noLeadingZeroes = 0; 
		 noLeadingZeroes < length && noLeadingZeroes < 8 && \
			sPeek( stream ) == 0;
		 noLeadingZeroes++ )
		{
		status = sgetc( stream );
		if( cryptStatusError( status ) )
			return( status );
		ENSURES_S( status == 0 );
		}
	if( noLeadingZeroes >= 8 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	return( length - noLeadingZeroes );
	}

/* Read a (non-bignum) numeric value, used by several routines */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readNumeric( INOUT STREAM *stream, OUT_OPT_LENGTH_Z long *value )
	{
	BYTE buffer[ 4 + 8 ];
	long localValue;
	int length, i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( value == NULL || isWritePtr( value, sizeof( long ) ) );

	/* Clear return value */
	if( value != NULL )
		*value = 0L;

	/* Read the length field and make sure that it's a non-bignum value */
	status = length = readIntegerHeader( stream, NO_TAG );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		return( 0 );	/* Zero-length data */
	if( length > 4 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Read the data */
	status = sread( stream, buffer, length );
	if( cryptStatusError( status ) || value == NULL )
		return( status );
	for( localValue = 0, i = 0; i < length; i++ )
		{
		const long localValTmp = localValue << 8;
		const int data = byteToInt( buffer[ i ] );

		if( localValue >= ( MAX_INTLENGTH >> 8 ) || \
			localValTmp >= MAX_INTLENGTH - data )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		localValue = localValTmp | data;
		if( localValue < 0 || localValue >= MAX_INTLENGTH )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}
	if( value != NULL )
		*value = localValue;
	return( CRYPT_OK );
	}

/* Read a constrained-length data value, used by several routines */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int readConstrainedData( INOUT STREAM *stream, 
								OUT_BUFFER_OPT( bufferMaxLength, \
												*bufferLength ) BYTE *buffer, 
								IN_LENGTH_SHORT const int bufferMaxLength,
								OUT_LENGTH_SHORT_Z int *bufferLength, 
								IN_LENGTH const int length )
	{
	int dataLength = length, remainder = 0, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( buffer == NULL || isWritePtr( buffer, length ) );
	assert( isWritePtr( bufferLength, sizeof( int ) ) );

	REQUIRES_S( bufferMaxLength > 0 && \
				bufferMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( length > 0 && length < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	if( buffer != NULL )
		memset( buffer, 0, min( 16, bufferMaxLength ) );
	*bufferLength = dataLength;

	/* If we don't care about the return value, skip it and exit */
	if( buffer == NULL )
		return( sSkip( stream, dataLength ) );

	/* Read the object, limiting the size to the maximum buffer size */
	if( dataLength > bufferMaxLength )
		{
		remainder = dataLength - bufferMaxLength;
		dataLength = bufferMaxLength;
		*bufferLength = dataLength;
		}
	status = sread( stream, buffer, dataLength );
	if( cryptStatusError( status ) )
		return( status );

	/* If there's no data remaining, we're done */
	if( remainder <= 0 )
		return( CRYPT_OK );

	/* Skip any remaining data */
	return( sSkip( stream, remainder ) );
	}

/****************************************************************************
*																			*
*						Read Routines for Primitive Objects					*
*																			*
****************************************************************************/

/* Read a tag and make sure that it's (approximately) valid */

CHECK_RETVAL_BOOL \
static BOOLEAN checkTag( IN_TAG_ENCODED const int tag )
	{
	/* Make sure that it's (approximately) valid: Not an EOC, and within the
	   allowed range */
	if( tag <= 0 || tag > MAX_TAG )
		return( FALSE );
	
	/* Make sure that it's not an application-specific or private tag */
	if( ( tag & BER_CLASS_MASK ) == BER_APPLICATION ||
		( tag & BER_CLASS_MASK ) == BER_PRIVATE )
		return( FALSE );

	/* If its's a context-specific tag make sure that the tag value is 
	   within the allowed range */
	if( ( tag & BER_CLASS_MASK ) == BER_CONTEXT_SPECIFIC && \
		( tag & BER_SHORT_ID_MASK ) > MAX_CTAG_VALUE )
		return( FALSE );

	return( TRUE );
	}

RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int readTag( INOUT STREAM *stream )
	{
	int tag;

	/* Read the tag */
	tag = sgetc( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	if( !checkTag( tag ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( tag );
	}

RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int peekTag( INOUT STREAM *stream )
	{
	int tag;

	/* Peek at the tag value */
	tag = sPeek( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	if( !checkTag( tag ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( tag );
	}

/* Check for constructed data end-of-contents octets.  Note that this 
   is a standard integer-return function that can return either a boolean 
   TRUE/FALSE or alternatively a stream error code if there's a problem, so 
   it's not a purely boolean function */

RETVAL_RANGE( MAX_ERROR, TRUE ) STDC_NONNULL_ARG( ( 1 ) ) \
int checkEOC( INOUT STREAM *stream )
	{
	BYTE eocBuffer[ 2 + 8 ];
	int tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Read the tag and check for an EOC octet pair.  Note that we can't use
	   peekTag()/readTag() for this because an EOC isn't a valid tag */
	tag = sPeek( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	if( tag != BER_EOC )
		return( FALSE );
	status = sread( stream, eocBuffer, 2 );
	if( cryptStatusError( status ) )
		return( status );
	if( memcmp( eocBuffer, "\x00\x00", 2 ) )
		{
		/* After finding an EOC tag we need to have a length of zero */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}

	return( TRUE );
	}

/* Read a short (<= 256 bytes) raw object without decoding it.  This is used
   to read short data blocks like object identifiers, which are only ever
   handled in encoded form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readRawObject( INOUT STREAM *stream, 
				   OUT_BUFFER( bufferMaxLength, *bufferLength ) BYTE *buffer, 
				   IN_LENGTH_SHORT_MIN( 3 ) const int bufferMaxLength, 
				   OUT_LENGTH_SHORT_Z int *bufferLength, 
				   IN_TAG_ENCODED const int tag )
	{
	int length, offset = 0;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( buffer, bufferMaxLength ) );
	assert( isWritePtr( bufferLength, sizeof( int ) ) );

	REQUIRES_S( bufferMaxLength >= 3 && \
				bufferMaxLength < MAX_INTLENGTH_SHORT );
				/* Need to be able to write at least the tag, length, and 
				   one byte of content */
	REQUIRES_S( ( tag == NO_TAG ) || ( tag >= 1 && tag <= MAX_TAG ) );
				/* Note tag != 0 */

	/* Clear return values */
	memset( buffer, 0, min( 16, bufferMaxLength ) );
	*bufferLength = 0;

	/* Read the identifier field and length.  We need to remember each byte 
	   as it's read so we can't just call readLengthValue() for the length, 
	   but since we only need to handle lengths that can be encoded in one 
	   or two bytes this isn't a problem.  Since this function reads a 
	   complete encoded object, the tag (if known) must be specified so 
	   there's no capability to use DEFAULT_TAG */
	if( tag != NO_TAG )
		{
		const int objectTag = readTag( stream );
		if( cryptStatusError( objectTag ) )
			return( objectTag );
		if( tag != objectTag )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		buffer[ offset++ ] = objectTag;
		}
	length = sgetc( stream );
	if( cryptStatusError( length ) )
		return( length );
	buffer[ offset++ ] = length;
	if( length & 0x80 )
		{
		/* If the object is indefinite-length or longer than 256 bytes (i.e. 
		   the length-of-length is anything other than 1) we don't want to 
		   handle it */
		if( length != 0x81 )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		length = sgetc( stream );
		if( cryptStatusError( length ) )
			return( length );
		buffer[ offset++ ] = length;
		}
	if( length <= 0 || length > 0xFF )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( offset + length > bufferMaxLength )
		{
		/* We treat this as a stream error even though technically it's an
		   insufficient-buffer-space error because the data object has 
		   violated the implicit format constraint of being larger than the
		   maximum size specified by the caller */
		return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
		}

	/* Read in the rest of the data */
	*bufferLength = offset + length;
	return( sread( stream, buffer + offset, length ) );
	}

/* Read a large integer value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readIntegerTag( INOUT STREAM *stream, 
					OUT_BUFFER_OPT( integerMaxLength, \
									*integerLength ) BYTE *integer, 
					IN_LENGTH_SHORT const int integerMaxLength, 
					OUT_OPT_LENGTH_SHORT_Z int *integerLength, 
					IN_TAG_EXT const int tag )
	{
	int localIntegerLength, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( integer == NULL || isWritePtr( integer, integerMaxLength ) );
	assert( integerLength == NULL || \
			isWritePtr( integerLength, sizeof( int ) ) );

	REQUIRES_S( integerMaxLength > 0 && \
				integerMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return values */
	if( integer != NULL )
		memset( integer, 0, min( 16, integerMaxLength ) );
	if( integerLength != NULL )
		*integerLength = 0;

	/* Read the integer header info */
	status = length = readIntegerHeader( stream, tag );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		return( 0 );	/* Zero-length data */

	/* Read in the numeric value, limiting the size to the maximum buffer 
	   size.  This is safe because the only situation where this can occur 
	   is when we're reading some blob (whose value we don't care about) 
	   dressed up as an integer rather than for any real integer */
	status = readConstrainedData( stream, integer, integerMaxLength,
								  &localIntegerLength, length );
	if( cryptStatusOK( status ) && integerLength != NULL )
		*integerLength = localIntegerLength;
	return( status );
	}

#ifdef USE_PKC

/* Read a bignum integer value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readBignumInteger( INOUT STREAM *stream, 
							  INOUT TYPECAST( BIGNUM * ) void *bignum, 
							  IN_LENGTH_PKC const int minLength, 
							  IN_LENGTH_PKC const int maxLength, 
							  IN_OPT TYPECAST( BIGNUM * ) const void *maxRange, 
							  IN_TAG_EXT const int tag,
							  IN_ENUM_OPT( KEYSIZE_CHECK ) \
								const KEYSIZE_CHECK_TYPE checkType )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES_S( minLength > 0 && minLength <= maxLength && \
				maxLength <= CRYPT_MAX_PKCSIZE );
	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );
	REQUIRES_S( checkType >= KEYSIZE_CHECK_NONE && \
				checkType < KEYSIZE_CHECK_LAST );

	/* Read the integer header info */
	status = length = readIntegerHeader( stream, tag );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		{
		/* It's a read of a zero value, make it explicit */
		BN_zero( bignum );

		return( CRYPT_OK );
		}

	/* Read the value into a fixed buffer */
	if( length > CRYPT_MAX_PKCSIZE )
		return( sSetError( stream, CRYPT_ERROR_OVERFLOW ) );
	status = sread( stream, buffer, length );
	if( cryptStatusError( status ) )
		return( status );
	status = importBignum( bignum, buffer, length, minLength, maxLength, 
						   maxRange, checkType );
	if( cryptStatusError( status ) )
		status = sSetError( stream, status );
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumTag( INOUT STREAM *stream, 
				   INOUT TYPECAST( BIGNUM * ) void *bignum, 
				   IN_LENGTH_PKC const int minLength, 
				   IN_LENGTH_PKC const int maxLength, 
				   IN_OPT TYPECAST( BIGNUM * ) const void *maxRange, 
				   IN_TAG_EXT const int tag )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength, 
							   maxRange, tag, KEYSIZE_CHECK_NONE ) );
	}

/* Special-case bignum read routine that explicitly checks for a too-short 
   key and returns CRYPT_ERROR_NOSECURE rather than the CRYPT_ERROR_BADDATA 
   that'd otherwise be returned */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumChecked( INOUT STREAM *stream, 
					   INOUT TYPECAST( BIGNUM * ) void *bignum, 
					   IN_LENGTH_PKC const int minLength, 
					   IN_LENGTH_PKC const int maxLength, 
					   IN_OPT TYPECAST( BIGNUM * ) const void *maxRange )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength, 
							   maxRange, DEFAULT_TAG, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_PKC */

/* Read a universal type and discard it (used to skip unknown or unwanted
   types) */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversalData( INOUT STREAM *stream )
	{
	long length;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = readLengthValue( stream, &length, READLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		return( CRYPT_OK );	/* Zero-length data */
	return( sSkip( stream, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal( INOUT STREAM *stream )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	readTag( stream );
	return( readUniversalData( stream ) );
	}

/* Read a short integer value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readShortIntegerTag( INOUT STREAM *stream, 
						 OUT_OPT_INT_Z long *value, 
						 IN_TAG_EXT const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( value == NULL || isWritePtr( value, sizeof( long ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return value */
	if( value != NULL )
		*value = 0L;

	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_INTEGER ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( readNumeric( stream, value ) );
	}

/* Read an enumerated value.  This is encoded like an ASN.1 integer so we
   just read it as such */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readEnumeratedTag( INOUT STREAM *stream, 
					   OUT_OPT_INT_Z int *enumeration, 
					   IN_TAG_EXT const int tag )
	{
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( enumeration == NULL || \
			isWritePtr( enumeration, sizeof( int ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return value */
	if( enumeration != NULL )
		*enumeration = 0;

	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_ENUMERATED ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	status = readNumeric( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	if( value < 0 || value > 1000 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( enumeration != NULL )
		*enumeration = ( int ) value;

	return( CRYPT_OK );
	}

/* Read a null value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readNullTag( INOUT STREAM *stream, IN_TAG_EXT const int tag )
	{
	int value;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Read the identifier if necessary */
	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_NULL ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	value = sgetc( stream );
	if( cryptStatusError( value ) )
		return( value );
	if( value != 0 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( CRYPT_OK );
	}

/* Read a boolean value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBooleanTag( INOUT STREAM *stream, 
					OUT_OPT_BOOL BOOLEAN *boolean, 
					IN_TAG_EXT const int tag )
	{
	BYTE buffer[ 2 + 8 ];
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( boolean == NULL || \
			isWritePtr( boolean, sizeof( BOOLEAN ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return value */
	if( boolean != NULL )
		*boolean = FALSE;

	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_BOOLEAN ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	status = sread( stream, buffer, 2 );
	if( cryptStatusError( status ) )
		return( status );
	if( buffer[ 0 ] != 1 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( boolean != NULL )
		*boolean = ( buffer[ 1 ] != 0 ) ? TRUE : FALSE;
	return( CRYPT_OK );
	}

/* Read an OID and check it against a permitted value or a selection of 
   permitted values */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readOIDEx( INOUT STREAM *stream, 
			   IN_ARRAY( noOidSelectionEntries ) const OID_INFO *oidSelection, 
			   IN_RANGE( 1, 50 ) const int noOidSelectionEntries,
			   OUT_OPT_PTR_OPT const OID_INFO **oidSelectionValue )
	{
	static const OID_INFO nullOidSelection = { NULL, CRYPT_ERROR, NULL };
	BYTE buffer[ MAX_OID_SIZE + 8 ];
	int length, oidEntry, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oidSelection, \
					   sizeof( OID_INFO ) * noOidSelectionEntries ) );
	assert( oidSelectionValue == NULL || \
			isReadPtr( oidSelectionValue, sizeof( OID_INFO * ) ) );

	REQUIRES_S( noOidSelectionEntries > 0 && noOidSelectionEntries <= 50 );

	/* Clear return value */
	if( oidSelectionValue != NULL )
		*oidSelectionValue = &nullOidSelection;

	/* Read the OID data */
	status = readRawObject( stream, buffer, MAX_OID_SIZE, &length, 
							BER_OBJECT_IDENTIFIER );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES_S( length == sizeofOID( buffer ) );

	/* Try and find the entry for the OID.  Since related groups of OIDs 
	   typically have identical lengths, we use the last byte of the OID
	   as a quick-reject check to avoid performing a full OID comparison
	   for each entry */
	for( oidEntry = 0, iterationCount = 0; 
		 oidEntry < noOidSelectionEntries && \
			oidSelection[ oidEntry ].oid != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 
		 oidEntry++, iterationCount++ )
		{
		const BYTE *oidPtr = oidSelection[ oidEntry ].oid;
		const int oidLength = sizeofOID( oidPtr );

		/* Check for a match-any wildcard OID */
		if( oidLength == WILDCARD_OID_SIZE && \
			!memcmp( oidPtr, WILDCARD_OID, WILDCARD_OID_SIZE ) )
			{
			/* The wildcard must be the last entry in the list */
			ENSURES_S( oidEntry + 1 < noOidSelectionEntries && \
					   oidSelection[ oidEntry + 1 ].oid == NULL );
			break;
			}

		/* Check for a standard OID match */
		if( length == oidLength && \
			buffer[ length - 1 ] == oidPtr[ length - 1 ] && \
			!memcmp( buffer, oidPtr, length ) )
			break;
		}
	ENSURES_S( iterationCount < FAILSAFE_ITERATIONS_MED );
	if( oidEntry >= noOidSelectionEntries || \
		oidSelection[ oidEntry ].oid == NULL )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	if( oidSelectionValue != NULL )
		*oidSelectionValue = &oidSelection[ oidEntry ];
	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readOID( INOUT STREAM *stream, 
			 IN_ARRAY( noOidSelectionEntries ) const OID_INFO *oidSelection, 
			 IN_RANGE( 1, 50 ) const int noOidSelectionEntries,
			 OUT_RANGE( CRYPT_ERROR, \
						noOidSelectionEntries ) int *selectionID )
	{
	const OID_INFO *oidSelectionInfo;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oidSelection, \
					   sizeof( OID_INFO ) * noOidSelectionEntries ) );
	assert( isWritePtr( selectionID, sizeof( int ) ) );

	REQUIRES_S( noOidSelectionEntries > 0 && noOidSelectionEntries <= 50 );

	/* Clear return value */
	*selectionID = CRYPT_ERROR;

	status = readOIDEx( stream, oidSelection, noOidSelectionEntries, 
						&oidSelectionInfo );
	if( cryptStatusOK( status ) )
		*selectionID = oidSelectionInfo->selectionID;
	return( status );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedOID( INOUT STREAM *stream, 
				  IN_BUFFER( oidLength ) const BYTE *oid, 
				  IN_LENGTH_OID const int oidLength )
	{
	CONST_INIT_STRUCT_A2( OID_INFO oidInfo[ 3 ], oid, NULL );

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( oid, oidLength ) && \
			oidLength == sizeofOID( oid ) && \
			oid[ 0 ] == BER_OBJECT_IDENTIFIER );

	REQUIRES_S( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE );
				/* Must be first for static analysis tools */
	REQUIRES_S( oidLength == sizeofOID( oid ) && \
				oid[ 0 ] == BER_OBJECT_IDENTIFIER );

	/* Set up a one-entry OID_INFO list to pass down to readOID() */
	CONST_SET_STRUCT_A( memset( oidInfo, 0, sizeof( OID_INFO ) * 3 ); \
						oidInfo[ 0 ].oid = oid );
	return( readOIDEx( stream, oidInfo, 3, NULL ) );
	}

/* Read a raw OID in encoded form */

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readEncodedOID( INOUT STREAM *stream, 
					OUT_BUFFER( oidMaxLength, *oidLength ) BYTE *oid, 
					IN_LENGTH_SHORT_MIN( 5 ) const int oidMaxLength, 
					OUT_LENGTH_SHORT_Z int *oidLength, 
					IN_TAG_ENCODED const int tag )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( oid, oidMaxLength ) );
	assert( isWritePtr( oidLength, sizeof( int ) ) );

	REQUIRES_S( oidMaxLength >= MIN_OID_SIZE && \
				oidMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == NO_TAG || tag == BER_OBJECT_IDENTIFIER );

	/* Clear return values */
	memset( oid, 0, min( 16, oidMaxLength ) );
	*oidLength = 0;

	/* Read the encoded OID and make sure that it's the right size for a
	   minimal-length OID: tag (optional) + length + minimal-length OID 
	   data */
	status = readRawObject( stream, oid, oidMaxLength, &length, tag );
	if( cryptStatusError( status ) )
		return( status );
	if( length < ( tag == NO_TAG ? 0 : 1 ) + 1 + 3 || length > oidMaxLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	*oidLength = length;
	return( CRYPT_OK );
	}

/* Read an octet string value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int readString( INOUT STREAM *stream, 
					   OUT_BUFFER_OPT( maxLength, *stringLength ) BYTE *string, 
					   OUT_LENGTH_SHORT_Z int *stringLength,
					   IN_LENGTH_SHORT const int minLength, 
					   IN_LENGTH_SHORT const int maxLength, 
					   IN_TAG_EXT const int tag, 
					   const BOOLEAN isOctetString )
	{
	long length;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( string == NULL || isWritePtr( string, maxLength ) );
	assert( isWritePtr( stringLength, sizeof( int ) ) );

	REQUIRES_S( minLength > 0 && minLength <= maxLength && \
				maxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( ( isOctetString && \
				  ( tag == NO_TAG || tag == DEFAULT_TAG ) ) || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return values */
	if( string != NULL )
		memset( string, 0, min( 16, maxLength ) );
	*stringLength = 0;

	/* Read the string, limiting the size to the maximum buffer size.  If 
	   it's an octet string we make this a hard limit, however if it's a 
	   text string we simply read as much as will fit in the buffer and 
	   discard the rest.  This is required to handle the widespread ignoring
	   of string length limits in certificates and other PKI-related data */
	if( isOctetString )
		{
		if( tag != NO_TAG && \
			readTag( stream ) != selectTag( tag, BER_OCTETSTRING ) )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	else
		{
		if( readTag( stream ) != tag )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	status = readLengthValue( stream, &length, READLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );
	if( length < minLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( isOctetString && length > maxLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( length <= 0 )
		return( CRYPT_OK );		/* Zero-length string */
	return( readConstrainedData( stream, string, maxLength, stringLength, 
								 length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOctetStringTag( INOUT STREAM *stream, 
						OUT_BUFFER( maxLength, *stringLength ) BYTE *string, 
						OUT_LENGTH_SHORT_Z int *stringLength, 
						IN_LENGTH_SHORT const int minLength, 
						IN_LENGTH_SHORT const int maxLength, 
						IN_TAG_EXT const int tag )
	{

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( string == NULL || isWritePtr( string, maxLength ) );
	assert( stringLength == NULL || \
			isWritePtr( stringLength, sizeof( int ) ) );

	REQUIRES_S( minLength > 0 && minLength <= maxLength && \
				maxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readString( stream, string, stringLength, minLength, maxLength, 
						tag, TRUE ) );
	}

/* Read a character string.  This handles any of the myriad ASN.1 character
   string types.  The handling of the tag works somewhat differently here to
   the usual manner in that since the function is polymorphic, the tag
   defines the character string type and is always used (there's no
   NO_TAG or DEFAULT_TAG option like the other functions use).  This works 
   because the plethora of string types means that the higher-level routines 
   that read them invariably have to sort out the valid tag types 
   themselves */

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readCharacterString( INOUT STREAM *stream, 
						 OUT_BUFFER( stringMaxLength, *stringLength ) void *string, 
						 IN_LENGTH_SHORT const int stringMaxLength, 
						 OUT_LENGTH_SHORT_Z int *stringLength, 
						 IN_TAG_EXT const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( string == NULL || isWritePtr( string, stringMaxLength ) );
	assert( stringLength == NULL || \
			isWritePtr( stringLength, sizeof( int ) ) );

	REQUIRES_S( stringMaxLength > 0 && \
				stringMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag >= 0 && tag < MAX_TAG_VALUE );

	return( readString( stream, string, stringLength, 1, stringMaxLength, 
						tag, FALSE ) );
	}

/* Read a bit string */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBitStringTag( INOUT STREAM *stream, 
					  OUT_OPT_INT_Z int *bitString, 
					  IN_TAG_EXT const int tag )
	{
	int length, data, mask, flag, value = 0, noBits, i;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( bitString == NULL || isWritePtr( bitString, sizeof( int ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );
	
	/* Clear return value */
	if( bitString != NULL )
		*bitString = 0;

	/* Make sure that we have a bitstring with between 0 and sizeof( int ) 
	   bits.  This isn't as machine-dependant as it seems, the only place 
	   where bit strings longer than one or two bytes are used is with CMP's 
	   bizarre encoding of error subcodes that just provide further 
	   information above and beyond the main error code and text message, 
	   and CMP is highly unlikely to be used on a 16-bit machine */
	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_BITSTRING ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	length = sgetc( stream );
	if( cryptStatusError( length ) )
		return( length );
	length--;	/* Adjust for bit count */
	if( length < 0 || length > 4 || length > sizeof( int ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	noBits = sgetc( stream );
	if( cryptStatusError( noBits ) )
		return( noBits );
	if( noBits < 0 || noBits > 7 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( length <= 0 )
		return( CRYPT_OK );		/* Zero value */
	ENSURES_S( length >= 1 && length <= min( 4, sizeof( int ) ) );
	ENSURES_S( noBits >= 0 && noBits <= 7 );

	/* Convert the bit count from the unused-remainder bit count into the 
	   total bit count */
	noBits = ( length * 8 ) - noBits;
	ENSURES_S( noBits >= 0 && noBits <= 32 );

	/* ASN.1 bitstrings start at bit 0 so we need to reverse the order of 
	   the bits before we return the value.  This uses a straightforward way
	   of doing it rather than the more efficient but hard-to-follow:

		data = ( data & 0x55555555 ) << 1 | ( data >> 1 ) & 0x55555555;
		data = ( data & 0x33333333 ) << 2 | ( data >> 2 ) & 0x33333333;
		data = ( data & 0x0F0F0F0F ) << 4 | ( data >> 4 ) & 0x0F0F0F0F;
		data = ( data << 24 ) | ( ( data & 0xFF00 ) << 8 ) | \
			   ( ( data >> 8 ) & 0xFF00 || ( data >> 24 );

	  which swaps adjacent bits, then 2-bit fields, then 4-bit fields, and 
	  so on */
	data = sgetc( stream );
	if( cryptStatusError( data ) )
		return( data );
	for( mask = 0x80, i = 1; i < length; mask <<= 8, i++ )
		{
		const long dataValTmp = data << 8;
		const int dataTmp = sgetc( stream );

		if( cryptStatusError( dataTmp ) )
			return( dataTmp );
		if( data >= ( MAX_INTLENGTH >> 8 ) || \
			dataValTmp >= MAX_INTLENGTH - data )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		data = dataValTmp | dataTmp;
		if( data < 0 || data >= MAX_INTLENGTH )
			{
			/* Integer overflow */
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		}
	for( flag = 1, i = 0; i < noBits; flag <<= 1, i++ )
		{
		if( data & mask )
			value |= flag;
		data <<= 1;
		}
	if( bitString != NULL )
		{
		if( value < 0 || value >= MAX_INTLENGTH )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		*bitString = value;
		}

	return( CRYPT_OK );
	}

/* Read a UTCTime and GeneralizedTime value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readTime( INOUT STREAM *stream, OUT time_t *timePtr, 
					 const BOOLEAN isUTCTime )
	{
	BYTE buffer[ 16 + 8 ], *bufPtr = buffer;
	struct tm theTime,  gmTimeInfo, *gmTimeInfoPtr = &gmTimeInfo;
	time_t utcTime, gmTime;
	int value = 0, length, i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( timePtr, sizeof( time_t ) ) );

	/* Clear return value */
	*timePtr = 0;

	/* Read the length field and make sure that it's of the correct size.  
	   There's only one encoding allowed although in theory the encoded 
	   value could range in length from 11 to 17 bytes for UTCTime and 13 to 
	   19 bytes for GeneralizedTime.  We formerly also allowed 11-byte 
	   UTCTimes because an obsolete encoding rule allowed the time to be 
	   encoded without seconds and Sweden Post hadn't realised that this had 
	   changed yet, but these certs have now expired */
	length = sgetc( stream );
	if( cryptStatusError( length ) )
		return( length );
	if( ( isUTCTime && length != 13 ) || ( !isUTCTime && length != 15 ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	ENSURES_S( length == 13 || length == 15 );

	/* Read the encoded time data and make sure that the contents are 
	   valid */
	memset( buffer, 0, 16 );
	status = sread( stream, buffer, length );
	if( cryptStatusError( status ) )
		return( status );
	for( i = 0; i < length - 1; i++ )
		{
		if( !isDigit( buffer[ i ] ) )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	if( buffer[ length - 1 ] != 'Z' )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Decode the time fields */
	memset( &theTime, 0, sizeof( struct tm ) );
	theTime.tm_isdst = -1;		/* Get the system to adjust for DST */
	if( !isUTCTime )
		{
		status = strGetNumeric( bufPtr, 2, &value, 19, 20 );
		if( cryptStatusError( status ) )
			return( status );
		value = ( value - 19 ) * 100;	/* Adjust for the century */
		bufPtr += 2;
		}
	status = strGetNumeric( bufPtr, 2, &theTime.tm_year, 0, 99 );
	if( cryptStatusOK( status ) )
		{
		theTime.tm_year += value;
		status = strGetNumeric( bufPtr + 2, 2, &theTime.tm_mon, 1, 12 );
		}
	if( cryptStatusOK( status ) )
		{
		theTime.tm_mon--;				/* Months are zero-based */
		status = strGetNumeric( bufPtr + 4, 2, &theTime.tm_mday, 1, 31 );
		}
	if( cryptStatusOK( status ) )
		status = strGetNumeric( bufPtr + 6, 2, &theTime.tm_hour, 0, 23 );
	if( cryptStatusOK( status ) )
		status = strGetNumeric( bufPtr + 8, 2, &theTime.tm_min, 0, 59 );
	if( cryptStatusOK( status ) )
		status = strGetNumeric( bufPtr + 10, 2, &theTime.tm_sec, 0, 59 );
	if( cryptStatusError( status ) )
		return( sSetError( stream, status ) );

	/* Finally, convert the decoded value to the local time.  Since the 
	   UTCTime format doesn't take centuries into account (and you'd think 
	   that when the ISO came up with the world's least efficient time 
	   encoding format they could have spared another two bytes to fully 
	   specify the year), we have to adjust by one century for years < 50 if 
	   the format is UTCTime.  Note that there are some implementations that 
	   currently roll over a century from 1970 (the Unix/Posix epoch and 
	   sort-of ISO/ANSI C epoch although they never come out and say it) 
	   but hopefully these will be fixed by 2050 when it would become an
	   issue.

	   In theory we could also check for an at least vaguely sane input 
	   value range on the grounds that (a) some systems' mktime()s may be 
	   broken and (b) some mktime()s may allow (and return) outrageous date 
	   values that others don't, however it's probably better to simply be 
	   consistent with what the system does rather than to try and 
	   second-guess the intent of the mktime() authors.

		"The time is out of joint; o cursed spite,
		 That ever I was born to set it right"	- Shakespeare, "Hamlet" */
	if( isUTCTime && theTime.tm_year < 50 )
		theTime.tm_year += 100;
	utcTime = mktime( &theTime );
	if( utcTime < 0 )
		{
		/* Some Java-based apps with 64-bit times use ridiculous validity
		   dates (yes, we're going to be keeping the same key in active use
		   for *forty years*) that postdate the time_t range when time_t is 
		   a signed 32-bit value.  If we can't convert the time, we check 
		   for a year after the time_t overflow (2038) and try again.  In
		   theory we should just reject objects with such broken dates but
		   since we otherwise accept all sorts of rubbish we at least try 
		   and accept these as well */
		if( theTime.tm_year >= 138 && theTime.tm_year < 180 )
			{
			theTime.tm_year = 136;		/* 2036 */
			utcTime = mktime( &theTime );
			}

		/* Some broken apps set dates to 1/1/1970, handling times this close 
		   to the epoch is problematic because once any possible DST 
		   adjustment is taken into account it's no longer possible to
		   represent the converted time as a time_t unless the system allows
		   it to be negative (Windows doesn't, many Unixen do, but having
		   cryptlib return a negative time value is probably a bad thing).  
		   To handle this, if we find a date set anywhere during January 1970 
		   we manually set the time to zero (the epoch) */
		if( theTime.tm_year == 70 && theTime.tm_mon == 0 )
			{
			*timePtr = 0;
			return( CRYPT_OK );
			}
		}
	if( utcTime < MIN_STORED_TIME_VALUE )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Convert the UTC time to local time.  This is complicated by the fact 
	   that although the C standard library can convert from local time -> 
	   UTC it can't convert the time back, so we treat the UTC time as 
	   local time (gmtime_s() always assumes that the input is local time) 
	   and covert to GMT and back, which should give the offset from GMT.  
	   Since we can't assume that time_t is signed we have to treat a 
	   negative and positive offset separately.
	   
	   An extra complication is added by daylight savings time adjustment, 
	   some (hopefully most by now) systems adjust for DST by default, some 
	   don't, and some allow it to be configured by the user so that it can 
	   vary from machine to machine so we have to make it explicit as part 
	   of the conversion process.
	   
	   Even this still isn't perfect because it displays the time adjusted 
	   for DST now rather than DST when the time value was created.  This 
	   will occur when the code is run within about 12-13 hours either way 
	   of the time at which the DST switchover occurs at that particular 
	   locality and doesn't necessarily have to be during the switchover 
	   hour since being in a different time zone to GMT can change the time 
	   by many hours, flipping it across the switchover hour.
	   
	   This problem is more or less undecidable, the code used here has the 
	   property that the values for Windows agree with those for Unix and 
	   other systems, but it can't handle the DST flip because there's no
	   way to find out whether it's happened to us or not */
	gmTimeInfoPtr = gmTime_s( &utcTime, gmTimeInfoPtr );
	if( gmTimeInfoPtr == NULL )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	gmTimeInfoPtr->tm_isdst = -1;		/* Force correct DST adjustment */
	gmTime = mktime( gmTimeInfoPtr );
	if( gmTime < MIN_STORED_TIME_VALUE )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( utcTime < gmTime )
		*timePtr = utcTime - ( gmTime - utcTime );
	else
		*timePtr = utcTime + ( utcTime - gmTime );

	/* This still isn't quite perfect since it can't handle the time at a 
	   DST changeover.  This is really a user problem ("Don't do that, 
	   then") but if necessary can be corrected by converting back to GMT as 
	   a sanity check and applying a +/- 1 hour correction if there's a 
	   mismatch */
#if 0
	gmTimeInfoPtr = gmTime_s( timePtr );
	gmTimeInfoPtr->tm_isdst = -1;
	gmTime = mktime( gmTimeInfoPtr );
	if( gmTime != utcTime )
		{
		*timePtr += 3600;		/* Try +1 first */
		gmTimeInfoPtr = gmTime_s( timePtr, gmTimeInfoPtr );
		gmTimeInfoPtr->tm_isdst = -1;
		gmTime = mktime( gmTimeInfoPtr );
		if( gmTime != utcTime )
			*timePtr -= 7200;	/* Nope, use -1 instead */
		}
#endif /* 0 */

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUTCTimeTag( INOUT STREAM *stream, OUT time_t *timeVal, 
					IN_TAG_EXT const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( timeVal, sizeof( time_t ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return value */
	*timeVal = 0;
	
	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_TIME_UTC ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( readTime( stream, timeVal, TRUE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGeneralizedTimeTag( INOUT STREAM *stream, OUT time_t *timeVal, 
							IN_TAG_EXT const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( timeVal, sizeof( time_t ) ) );

	REQUIRES_S( tag == NO_TAG || tag == DEFAULT_TAG || \
				( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Clear return value */
	*timeVal = 0;
	
	if( tag != NO_TAG && readTag( stream ) != selectTag( tag, BER_TIME_GENERALIZED ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( readTime( stream, timeVal, FALSE ) );
	}

/****************************************************************************
*																			*
*						Read Routines for Constructed Objects				*
*																			*
****************************************************************************/

/* Read the header for a constructed object.  This performs a more strict 
   check than the checkTag() function used when reading the tag because
   we know that we're reading a constructed object or hole and can restrict
   the permitted values accordingly.  The behaviour of the read is 
   controlled by the following flags:

	FLAG_BITSTRING: The object being read is a BIT STRING with an extra 
		unused-bits count at the start.  This explicit indication is 
		required because implicit tagging can obscure the fact that what's
		being read is a BIT STRING.

	FLAG_INDEFOK: Indefinite-length objects are permitted for short-object
		reads.

	FLAG_UNIVERSAL: Normally we perform a sanity check to make sure that 
		what we're reading has a valid tag for a constructed object or a 
		hole, however if we're reading the object as a blob then we're more 
		liberal in what we allow, although we still perform some minimal 
		checking */

#define READOBJ_FLAG_NONE		0x00	/* No flag */
#define READOBJ_FLAG_BITSTRING	0x01	/* Object is BIT STRING */
#define READOBJ_FLAG_INDEFOK	0x02	/* Indefinite lengths allowed */
#define READOBJ_FLAG_UNIVERSAL	0x04	/* Relax type-checking requirements */
#define READOBJ_FLAG_MAX		0x0F	/* Maximum possible flag value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkReadTag( INOUT STREAM *stream, 
						 IN_TAG_ENCODED_EXT const int tag,
						 const BOOLEAN allowRelaxedMatch )
	{
	int tagValue;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( ( tag == ANY_TAG ) || ( tag >= 1 && tag < MAX_TAG ) );
				/* Note tag != 0 */

	/* Read the identifier field */
	tagValue = readTag( stream );
	if( cryptStatusError( tagValue ) )
		return( tagValue );
	if( tag != ANY_TAG )
		{
		/* If we have to get an exact match, make sure that the tag matches 
		   what we're expecting */
		if( tagValue != tag )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

		return( CRYPT_OK );
		}

	/* Even if we're prepared to accept (almost) any tag we still have to 
	   check for valid universal tags: BIT STRING, primitive or constructed 
	   OCTET STRING, SEQUENCE, or SET */
	if( tagValue == BER_BITSTRING || tagValue == BER_OCTETSTRING || \
		tagValue == ( BER_OCTETSTRING | BER_CONSTRUCTED ) || \
		tagValue == BER_SEQUENCE || tagValue == BER_SET )
		return( CRYPT_OK );

	/* In addition we can accept context-specific tagged items up to [10] */
	if( ( tagValue & BER_CLASS_MASK ) == BER_CONTEXT_SPECIFIC && \
		( tagValue & BER_SHORT_ID_MASK ) <= MAX_CTAG_VALUE )
		return( CRYPT_OK );

	/* If we're reading an object as a genuine blob rather than a 
	   constructed object or hole we allow a wider range of tags that 
	   wouldn't normally be permitted as holes.  Currently only INTEGERs
	   are read in this manner (from their use as generic blobs in 
	   certificate serial numbers and the like) */
	if( allowRelaxedMatch && tagValue == BER_INTEGER )
		return( CRYPT_OK );

	/* Anything else is invalid */
	return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readObjectHeader( INOUT STREAM *stream, 
							 OUT_OPT_LENGTH_Z int *length, 
							 IN_LENGTH_SHORT const int minLength, 
							 IN_TAG_ENCODED_EXT const int tag, 
							 IN_FLAGS_Z( READOBJ ) const int flags )
	{
	long dataLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( minLength >= 0 && minLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( ( tag == ANY_TAG ) || ( tag >= 1 && tag < MAX_TAG ) );
				/* Note tag != 0 */
	REQUIRES( flags >= READOBJ_FLAG_NONE && flags <= READOBJ_FLAG_MAX );

	/* Clear return value */
	if( length != NULL )
		*length = 0;

	/* Read the identifier field and length.  If the indefiniteOK flag is 
	   set or the length is being ignored by the caller then we allow 
	   indefinite lengths.  The latter is because it makes handling of 
	   infinitely-nested SEQUENCEs and whatnot easier if we don't have to 
	   worry about definite vs. indefinite-length encodings (ex duobus malis 
	   minimum eligendum est), and if indefinite lengths really aren't OK 
	   then they'll be picked up when the caller runs into the EOC at the 
	   end of the object */
	status = checkReadTag( stream, tag, 
						   ( flags & READOBJ_FLAG_UNIVERSAL ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );
	status = readLengthValue( stream, &dataLength,
							  ( ( flags & READOBJ_FLAG_INDEFOK ) || \
								length == NULL ) ? \
								READLENGTH_SHORT_INDEF : READLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a bit string there's an extra unused-bits count.  Since this 
	   is a hole encoding we don't bother about the actual value except to
	   check that it has a sensible value */
	if( flags & READOBJ_FLAG_BITSTRING )
		{
		int value;

		if( dataLength != CRYPT_UNUSED )
			{
			dataLength--;
			if( dataLength < 0 || dataLength >= MAX_INTLENGTH )
				return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
			}
		value = sgetc( stream );
		if( cryptStatusError( value ) )
			return( value );
		if( value < 0 || value > 7 )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}

	/* Make sure that the length is in order and return it to the caller if 
	   necessary */
	if( ( dataLength != CRYPT_UNUSED ) && \
		( dataLength < minLength || dataLength > 32767 ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( length != NULL )
		*length = dataLength;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readLongObjectHeader( INOUT STREAM *stream, 
								 OUT_OPT_LENGTH_Z long *length, 
								 IN_TAG_ENCODED_EXT const int tag, 
								 IN_FLAGS_Z( READOBJ ) const int flags )
	{
	long dataLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );

	REQUIRES_S( ( tag == ANY_TAG ) || ( tag >= 1 && tag < MAX_TAG ) );
				/* Note tag != 0 */
	REQUIRES( flags == READOBJ_FLAG_NONE || \
			  flags == READOBJ_FLAG_UNIVERSAL );

	/* Clear return value */
	if( length != NULL )
		*length = 0L;

	/* Read the identifier field and length */
	status = checkReadTag( stream, tag,
						   ( flags & READOBJ_FLAG_UNIVERSAL ) ? TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );
	status = readLengthValue( stream, &dataLength, READLENGTH_LONG_INDEF );
	if( cryptStatusError( status ) )
		return( status );
	if( length != NULL )
		*length = dataLength;

	return( CRYPT_OK );
	}

/* Read an encapsulating SEQUENCE or SET or BIT STRING/OCTET STRING hole */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSequence( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	return( readObjectHeader( stream, length, 0, BER_SEQUENCE, 
							  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSequenceI( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF int *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	return( readObjectHeader( stream, length, 0, BER_SEQUENCE, 
							  READOBJ_FLAG_INDEFOK ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSet( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	return( readObjectHeader( stream, length, 0, BER_SET, 
							  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSetI( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF int *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	return( readObjectHeader( stream, length, 0, BER_SET, 
							  READOBJ_FLAG_INDEFOK ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readConstructed( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( ( tag == DEFAULT_TAG ) || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readObjectHeader( stream, length, 0, ( tag == DEFAULT_TAG ) ? \
							  BER_SEQUENCE : MAKE_CTAG( tag ), 
							  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readConstructedI( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF int *length, 
					  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( ( tag == DEFAULT_TAG ) || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readObjectHeader( stream, length, 0, ( tag == DEFAULT_TAG ) ? \
							  BER_SEQUENCE : MAKE_CTAG( tag ), 
							  READOBJ_FLAG_INDEFOK ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readOctetStringHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
						 IN_LENGTH_SHORT const int minLength, 
						 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( ( tag == DEFAULT_TAG ) || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readObjectHeader( stream, length, minLength, 
							  ( tag == DEFAULT_TAG ) ? \
								BER_OCTETSTRING : MAKE_CTAG_PRIMITIVE( tag ),
							  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBitStringHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					   IN_LENGTH_SHORT const int minLength, 
					   IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	REQUIRES_S( ( tag == DEFAULT_TAG ) || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readObjectHeader( stream, length, minLength, 
							  ( tag == DEFAULT_TAG ) ? \
								BER_BITSTRING : MAKE_CTAG_PRIMITIVE( tag ),
							  READOBJ_FLAG_BITSTRING ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readGenericHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					 IN_LENGTH_SHORT const int minLength, 
					 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	ENSURES_S( ( tag == DEFAULT_TAG ) || ( tag > 0 && tag < MAX_TAG ) );
			   /* In theory we should use MAX_TAG_VALUE but this function is 
			      frequently used as part of the sequence 'read tag; 
				  save tag; readGenericXYZ( tag );' so we have to allow all
				  tag values */

	return( readObjectHeader( stream, length, minLength, 
							  ( tag == DEFAULT_TAG ) ? ANY_TAG : tag, 
							  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readGenericHoleI( INOUT STREAM *stream, 
					  OUT_OPT_LENGTH_INDEF int *length, 
					  IN_LENGTH_SHORT const int minLength, 
					  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( int ) ) );

	ENSURES_S( ( tag == DEFAULT_TAG ) || ( tag > 0 && tag < MAX_TAG ) );
			   /* See comment for readGenericHole() */

	return( readObjectHeader( stream, length, minLength, 
							  ( tag == DEFAULT_TAG ) ? ANY_TAG : tag, 
							  READOBJ_FLAG_INDEFOK ) );
	}

/* Read an abnormally-long encapsulating SEQUENCE or OCTET STRING hole.  
   This is used in place of the usual read in situations where potentially 
   huge data quantities would fail the sanity check enforced by the 
   standard read.  This form always allows indefinite lengths, which are 
   likely for large objects */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongSequence( INOUT STREAM *stream, 
					  OUT_OPT_LENGTH_INDEF long *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );

	return( readLongObjectHeader( stream, length, BER_SEQUENCE,
								  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongSet( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF long *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );

	return( readLongObjectHeader( stream, length, BER_SET, 
								  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongConstructed( INOUT STREAM *stream, 
						 OUT_OPT_LENGTH_INDEF long *length, 
						 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );

	REQUIRES_S( ( tag == DEFAULT_TAG ) || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( readLongObjectHeader( stream, length, ( tag == DEFAULT_TAG ) ? \
								  BER_SEQUENCE : MAKE_CTAG( tag ), 
								  READOBJ_FLAG_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongGenericHole( INOUT STREAM *stream, 
						 OUT_OPT_LENGTH_INDEF long *length, 
						 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );

	ENSURES_S( ( tag == DEFAULT_TAG ) || ( tag > 0 && tag < MAX_TAG ) );
			   /* See comment for readGenericHole() */

	return( readLongObjectHeader( stream, length, 							  
								  ( tag == DEFAULT_TAG ) ? ANY_TAG : tag,
								  READOBJ_FLAG_NONE ) );
	}

/* Read a generic object header, used to find the length of an object being
   read as a blob */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGenericObjectHeader( INOUT STREAM *stream, 
							 OUT_LENGTH_INDEF long *length, 
							 const BOOLEAN isLongObject )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	/* Clear return value */
	*length = 0L;

	if( !isLongObject )
		{
		int localLength, status;

		status = readObjectHeader( stream, &localLength, 0, ANY_TAG, 
								   READOBJ_FLAG_INDEFOK | \
								   READOBJ_FLAG_UNIVERSAL );
		if( cryptStatusOK( status ) )
			*length = localLength;
		return( status );
		}

	return( readLongObjectHeader( stream, length, ANY_TAG, 
								  READOBJ_FLAG_UNIVERSAL ) );
	}

/* Read an arbitrary-length constructed object's data into a memory buffer.  
   This is the arbitrary-length form of readRawObject() */

#define OBJECT_HEADER_DATA_SIZE		16

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readRawObjectAlloc( INOUT STREAM *stream, 
						OUT_BUFFER_ALLOC_OPT( *length ) void **objectPtrPtr, 
						OUT_LENGTH_Z int *objectLengthPtr,
						IN_LENGTH_SHORT_MIN( OBJECT_HEADER_DATA_SIZE ) \
							const int minLength, 
						IN_LENGTH_SHORT const int maxLength )
	{
	STREAM headerStream;
	BYTE buffer[ OBJECT_HEADER_DATA_SIZE + 8 ];
	void *objectData;
	int objectLength, headerSize = DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( objectPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( objectLengthPtr, sizeof( int ) ) );

	REQUIRES_S( minLength >= OBJECT_HEADER_DATA_SIZE && \
				minLength < maxLength && \
				maxLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*objectPtrPtr = NULL;
	*objectLengthPtr = 0;

	/* Find out how much data we need to read.  This may be a non-seekable
	   stream so we have to grab the first OBJECT_HEADER_DATA_SIZE bytes 
	   from the stream and decode them to see what's next */
	status = sread( stream, buffer, OBJECT_HEADER_DATA_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &headerStream, buffer, OBJECT_HEADER_DATA_SIZE );
	status = readGenericHole( &headerStream, &objectLength, 
							  OBJECT_HEADER_DATA_SIZE, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		headerSize = stell( &headerStream );
	sMemDisconnect( &headerStream );
	if( cryptStatusError( status ) )
		{
		sSetError( stream, status );
		return( status );
		}

	/* Make sure that the object has a sensible length */
	if( objectLength < minLength || objectLength > maxLength )
		{
		sSetError( stream, CRYPT_ERROR_BADDATA );
		return( status );
		}

	/* Allocate storage for the object data and copy the already-read 
	   portion to the start of the storage */
	objectLength += headerSize;
	if( ( objectData = clAlloc( "readObjectData", objectLength ) ) == NULL )
		{
		/* This isn't technically a stream error, but all ASN.1 stream 
		   functions need to set the stream error status */
		sSetError( stream, CRYPT_ERROR_MEMORY );
		return( CRYPT_ERROR_MEMORY );
		}
	memcpy( objectData, buffer, OBJECT_HEADER_DATA_SIZE );

	/* Read the remainder of the object data into the memory buffer and 
	   check that the overall object is valid */
	status = sread( stream, ( BYTE * ) objectData + OBJECT_HEADER_DATA_SIZE,
					objectLength - OBJECT_HEADER_DATA_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	status = checkObjectEncoding( objectData, objectLength );
	if( cryptStatusError( status ) )
		{
		sSetError( stream, CRYPT_ERROR_BADDATA );
		return( status );
		}

	*objectPtrPtr = objectData;
	*objectLengthPtr = objectLength;

	return( CRYPT_OK );
	}
