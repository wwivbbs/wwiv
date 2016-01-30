/****************************************************************************
*																			*
*								ASN.1 Write Routines						*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "bn.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "bn/bn.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

#ifdef USE_INT_ASN1

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Calculate the size of the encoded length octets */

CHECK_RETVAL_RANGE( 1, 5 ) \
static int calculateLengthSize( IN_LENGTH_Z const long length )
	{
	REQUIRES( length >= 0 && length < MAX_INTLENGTH );

	/* Use the short form of the length octets if possible */
	if( length <= 0x7F )
		return( 1 );

	/* Use the long form of the length octets, a length-of-length followed 
	   by an 8, 16, 24, or 32-bit length.  We order the comparisons by 
	   likelihood of occurrence, shorter lengths are far more common than 
	   longer ones */
	if( length <= 0xFF )
		return( 1 + 1 );
	if( length <= 0xFFFFL )
		return( 1 + 2 );
	return( 1 + ( ( length > 0xFFFFFFL ) ? 4 : 3 ) );
	}

/* Write the length octets for an ASN.1 item */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeLength( INOUT STREAM *stream, IN_LENGTH_Z const long length )
	{
	BYTE buffer[ 8 + 8 ];
	const int noLengthOctets = ( length <= 0xFF ) ? 1 : \
							   ( length <= 0xFFFFL ) ? 2 : \
							   ( length <= 0xFFFFFFL ) ? 3 : 4;
	int bufPos = 1;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH );

	/* Use the short form of the length octets if possible */
	if( length <= 0x7F )
		return( sputc( stream, length & 0xFF ) );

	/* Encode the number of length octets followed by the octets themselves */
	buffer[ 0 ] = 0x80 | intToByte( noLengthOctets );
	if( noLengthOctets > 3 )
		buffer[ bufPos++ ] = ( length >> 24 ) & 0xFF;
	if( noLengthOctets > 2 )
		buffer[ bufPos++ ] = ( length >> 16 ) & 0xFF;
	if( noLengthOctets > 1 )
		buffer[ bufPos++ ] = ( length >> 8 ) & 0xFF;
	buffer[ bufPos++ ] = length & 0xFF;
	return( swrite( stream, buffer, bufPos ) );
	}

/* Write a (non-bignum) numeric value, used by several routines.  The 
   easiest way to do this is to encode the bytes starting from the LSB
   and then output them in reverse order to get a big-endian encoding */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeNumeric( INOUT STREAM *stream, IN_INT const long integer )
	{
	BYTE buffer[ 16 + 8 ];
	long intValue = integer;
	int length = 0, i, iterationCount;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( integer >= 0 && integer < MAX_INTLENGTH );

	/* The value 0 is handled specially */
	if( intValue == 0 )
		return( swrite( stream, "\x01\x00", 2 ) );

	/* Assemble the encoded value in little-endian order */
	if( intValue > 0 )
		{
		for( iterationCount = 0;
			 intValue > 0 && iterationCount < FAILSAFE_ITERATIONS_SMALL;
			 iterationCount++ )
			{
			buffer[ length++ ] = intValue & 0xFF;
			intValue >>= 8;
			}
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_SMALL );

		/* Make sure that we don't inadvertently set the sign bit if the 
		   high bit of the value is set */
		if( buffer[ length - 1 ] & 0x80 )
			buffer[ length++ ] = 0x00;
		}
	else
		{
		/* Write a negative integer values.  This code is never executed 
		   (and is actually checked for by the precondition at the start of
		   this function), it's present only in case it's ever needed in the 
		   future */
		iterationCount = 0;
		do
			{
			buffer[ length++ ] = intValue & 0xFF;
			intValue >>= 8;
			}
		while( intValue != -1 && length < sizeof( int ) && \
			   iterationCount++ < FAILSAFE_ITERATIONS_SMALL );
		ENSURES( iterationCount < FAILSAFE_ITERATIONS_SMALL );

		/* Make sure that we don't inadvertently clear the sign bit if the 
		   high bit of the value is clear */
		if( !( buffer[ length - 1 ] & 0x80 ) )
			buffer[ length++ ] = 0xFF;
		}
	ENSURES( length > 0 && length <= 8 );

	/* Output the value in reverse (big-endian) order */
	sputc( stream, length );
	for( i = length - 1; i > 0; i-- )
		sputc( stream, buffer[ i ] );
	return( sputc( stream, buffer[ 0 ] ) );
	}

/****************************************************************************
*																			*
*								Sizeof Routines								*
*																			*
****************************************************************************/

/* Determine the encoded size of an object given only a length.  This
   function is a bit problematic because it's frequently called as part 
   of a complex expression, where in theory it should never be passed a 
   negative value but due to some sort of exceptional circumstances may
   end up being passed one.  Since this is a can't-occur condition, we 
   don't want to go overboard with checking for it (it would require having 
   to check the return value of every single use of sizeofObject() within a
   complex expression), but also need some means of being able to cope with
   it.  To deal with this we always return a safe length of zero on error */

RETVAL_LENGTH_NOERROR \
long sizeofObject( IN_LENGTH_Z const long length )
	{
	/* If we've been passed an error code as input or we're about to exceed 
	   the maximum safe length range, don't try and go any further */
	if( length < 0 || length > MAX_INTLENGTH - 16 )
		{
		DEBUG_DIAG( ( "Invalid value passed to sizeofObject()" ) );
		assert( DEBUG_WARN );
		return( 0 );
		}

	return( 1 + calculateLengthSize( length ) + length );
	}

#ifdef USE_PKC

/* Determine the size of a bignum.  When we're writing these we can't use 
   sizeofObject() directly because the internal representation is unsigned 
   whereas the encoded form is signed */

RETVAL_RANGE_NOERROR( 0, MAX_INTLENGTH_SHORT ) STDC_NONNULL_ARG( ( 1 ) ) \
int signedBignumSize( IN TYPECAST( BIGNUM * ) const void *bignum )
	{
	const int length = BN_num_bytes( bignum );

	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	/* The output from this function is typically used in calculations
	   involving multiple bignums, for which it doesn't make much sense to
	   individually check the return value of each function call for a
	   condition that can only be caused by an internal error, so we throw
	   an exception in debug mode but otherwise convert the condition to
	   a no-op length value */
	if( cryptStatusError( length ) )
		retIntError_Ext( 0 );

	/* Return the bignum length plus a leading zero byte if the high bit is 
	   set */
	return( length + ( ( BN_high_bit( ( BIGNUM * ) bignum ) ) ? 1 : 0 ) );
	}
#endif /* USE_PKC */

/****************************************************************************
*																			*
*					Write Routines for Primitive Objects					*
*																			*
****************************************************************************/

/* Write a short/large/bignum integer value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeShortInteger( INOUT STREAM *stream, 
					   IN_INT_Z const long integer, 
					   IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( integer >= 0 && integer < MAX_INTLENGTH );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_INTEGER : MAKE_CTAG_PRIMITIVE( tag ) );
	return( writeNumeric( stream, integer ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger( INOUT STREAM *stream, 
				  IN_BUFFER( integerLength ) const BYTE *integer, 
				  IN_LENGTH_SHORT const int integerLength, 
				  IN_TAG const int tag )
	{
	const BOOLEAN leadingZero = ( integerLength > 0 && \
								  ( *integer & 0x80 ) ) ? 1 : 0;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( integer, integerLength ) );

	REQUIRES_S( integerLength >= 0 && integerLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_INTEGER : MAKE_CTAG_PRIMITIVE( tag ) );
	writeLength( stream, integerLength + leadingZero );
	if( leadingZero )
		sputc( stream, 0 );
	return( swrite( stream, integer, integerLength ) );
	}

#ifdef USE_PKC

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumTag( INOUT STREAM *stream, 
					IN TYPECAST( BIGNUM * ) const void *bignum, 
					IN_TAG const int tag )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_S( !BN_is_zero( ( BIGNUM * ) bignum ) );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* If it's a dummy write, don't go through the full encoding process.
	   This optimisation both speeds things up and reduces unnecessary
	   writing of key data to memory */
	if( sIsNullStream( stream ) )
		{
		return( sSkip( stream, sizeofBignum( bignum ), 
					   MAX_INTLENGTH_SHORT ) );
		}

	status = exportBignum( buffer, CRYPT_MAX_PKCSIZE, &length, bignum );
	if( cryptStatusError( status ) )
		retIntError_Stream( stream );
	status = writeInteger( stream, buffer, length, tag );
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	return( status );
	}
#endif /* USE_PKC */

/* Write an enumerated value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeEnumerated( INOUT STREAM *stream, 
					 IN_RANGE( 0, 999 ) const int enumerated, 
					 IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( enumerated >= 0 && enumerated < 1000 );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_ENUMERATED : MAKE_CTAG_PRIMITIVE( tag ) );
	return( writeNumeric( stream, ( long ) enumerated ) );
	}

/* Write a null value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeNull( INOUT STREAM *stream, IN_TAG const int tag )
	{
	BYTE buffer[ 8 + 8 ];

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	buffer[ 0 ] = ( tag == DEFAULT_TAG ) ? \
				  BER_NULL : intToByte( MAKE_CTAG_PRIMITIVE( tag ) );
	buffer[ 1 ] = 0;
	return( swrite( stream, buffer, 2 ) );
	}

/* Write a boolean value */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBoolean( INOUT STREAM *stream, const BOOLEAN boolean, 
				  IN_TAG const int tag )
	{
	BYTE buffer[ 8 + 8 ];

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	buffer[ 0 ] = ( tag == DEFAULT_TAG ) ? \
				  BER_BOOLEAN : intToByte( MAKE_CTAG_PRIMITIVE( tag ) );
	buffer[ 1 ] = 1;
	buffer[ 2 ] = boolean ? 0xFF : 0;
	return( swrite( stream, buffer, 3 ) );
	}

/* Write an octet string */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOctetString( INOUT STREAM *stream, 
					  IN_BUFFER( length ) const BYTE *string, 
					  IN_LENGTH_SHORT const int length, 
					  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( string, length ) );

	REQUIRES_S( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_OCTETSTRING : MAKE_CTAG_PRIMITIVE( tag ) );
	writeLength( stream, length );
	return( swrite( stream, string, length ) );
	}

/* Write a character string.  This handles any of the myriad ASN.1 character
   string types.  The handling of the tag works somewhat differently here to
   the usual manner in that since the function is polymorphic, the tag
   defines the character string type and is always used (there's no
   DEFAULT_TAG like the other functions use) */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCharacterString( INOUT STREAM *stream, 
						  IN_BUFFER( length ) const void *string, 
						  IN_LENGTH_SHORT const int length, 
						  IN_TAG_ENCODED const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( string, length ) );

	REQUIRES_S( length > 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES_S( ( tag >= BER_STRING_UTF8 && tag <= BER_STRING_BMP ) || \
				( tag >= MAKE_CTAG_PRIMITIVE( 0 ) && \
				  tag <= MAKE_CTAG_PRIMITIVE( MAX_CTAG_VALUE ) ) );

	writeTag( stream, tag );
	writeLength( stream, length );
	return( swrite( stream, string, length ) );
	}

/* Write a bit string */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBitString( INOUT STREAM *stream, IN_INT_Z const int bitString, 
					IN_TAG const int tag )
	{
	BYTE buffer[ 16 + 8 ];
#if UINT_MAX > 0xFFFF
	const int maxIterations = 32;
#else
	const int maxIterations = 16;
#endif /* 16 vs.32-bit systems */
	unsigned int value = 0;
	int data = bitString, noBits = 0, i;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( bitString >= 0 && bitString < INT_MAX );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* ASN.1 bitstrings start at bit 0, so we need to reverse the order of
	  the bits before we write them out */
	for( i = 0; i < maxIterations; i++ )
		{
		/* Update the number of significant bits */
		if( data > 0 )
			noBits++;

		/* Reverse the bits */
		value <<= 1;
		if( data & 1 )
			value |= 1;
		data >>= 1;
		}

	/* Write the data as an ASN.1 BITSTRING.  This has the potential to lose
	   some bits on 16-bit systems, but the only place where bit strings 
	   longer than one or two bytes are used is with CMP's bizarre encoding 
	   of error subcodes that just provide further information above and 
	   beyond the main error code and text message, and it's unlikely that 
	   too many people will be running a CMP server on a DOS box */
	buffer[ 0 ] = ( tag == DEFAULT_TAG ) ? \
				  BER_BITSTRING : intToByte( MAKE_CTAG_PRIMITIVE( tag ) );
	buffer[ 1 ] = 1 + intToByte( ( ( noBits + 7 ) >> 3 ) );
	buffer[ 2 ] = ~( ( noBits - 1 ) & 7 ) & 7;
#if UINT_MAX > 0xFFFF
	buffer[ 3 ] = ( value >> 24 ) & 0xFF;
	buffer[ 4 ] = ( value >> 16 ) & 0xFF;
	buffer[ 5 ] = ( value >> 8 ) & 0xFF;
	buffer[ 6 ] = value & 0xFF;
#else
	buffer[ 3 ] = ( value >> 8 ) & 0xFF;
	buffer[ 4 ] = value & 0xFF;
#endif /* 16 vs.32-bit systems */
	return( swrite( stream, buffer, 3 + ( ( noBits + 7 ) >> 3 ) ) );
	}

/* Write a canonical UTCTime and GeneralizedTime value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writeTime( INOUT STREAM *stream, const time_t timeVal, 
					  IN_TAG const int tag, const BOOLEAN isUTCTime )
	{
	struct tm timeInfo, *timeInfoPtr = &timeInfo;
	char buffer[ 20 + 8 ];
	const int length = isUTCTime ? 13 : 15;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( timeVal >= MIN_STORED_TIME_VALUE );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	timeInfoPtr = gmTime_s( &timeVal, timeInfoPtr );
	ENSURES_S( timeInfoPtr != NULL && timeInfoPtr->tm_year > 90 );
	buffer[ 0 ] = ( tag != DEFAULT_TAG ) ? \
					intToByte( MAKE_CTAG_PRIMITIVE( tag ) ) : \
				  isUTCTime ? BER_TIME_UTC : BER_TIME_GENERALIZED;
	buffer[ 1 ] = intToByte( length );
	if( isUTCTime )
		{
		sprintf_s( buffer + 2, 16, "%02d%02d%02d%02d%02d%02dZ", 
				   timeInfoPtr->tm_year % 100, timeInfoPtr->tm_mon + 1, 
				   timeInfoPtr->tm_mday, timeInfoPtr->tm_hour, 
				   timeInfoPtr->tm_min, timeInfoPtr->tm_sec );
		}
	else
		{
		sprintf_s( buffer + 2, 16, "%04d%02d%02d%02d%02d%02dZ", 
				   timeInfoPtr->tm_year + 1900, timeInfoPtr->tm_mon + 1, 
				   timeInfoPtr->tm_mday, timeInfoPtr->tm_hour, 
				   timeInfoPtr->tm_min, timeInfoPtr->tm_sec );
		}
	return( swrite( stream, buffer, length + 2 ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUTCTime( INOUT STREAM *stream, const time_t timeVal, 
				  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( timeVal >= MIN_STORED_TIME_VALUE );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( writeTime( stream, timeVal, tag, TRUE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeGeneralizedTime( INOUT STREAM *stream, const time_t timeVal, 
						  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( timeVal >= MIN_STORED_TIME_VALUE );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	return( writeTime( stream, timeVal, tag, FALSE) );
	}

/****************************************************************************
*																			*
*					Write Routines for Constructed Objects					*
*																			*
****************************************************************************/

/* Write the start of an encapsulating SEQUENCE, SET, or generic tagged
   constructed object.  The difference between writeOctet/BitStringHole() and
   writeGenericHole() is that the octet/bit-string versions create a normal
   or context-specific-tagged primitive string while the generic version 
   creates a pure hole with no processing of tags */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeSequence( INOUT STREAM *stream, 
				   IN_LENGTH_Z const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH );

	writeTag( stream, BER_SEQUENCE );
	return( writeLength( stream, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeSet( INOUT STREAM *stream, 
			  IN_LENGTH_SHORT_Z const int length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH_SHORT );

	writeTag( stream, BER_SET );
	return( writeLength( stream, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeConstructed( INOUT STREAM *stream, 
					  IN_LENGTH_Z const int length,
					  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_SEQUENCE : MAKE_CTAG( tag ) );
	return( writeLength( stream, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeOctetStringHole( INOUT STREAM *stream, 
						  IN_LENGTH_Z const int length,
						  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_OCTETSTRING : MAKE_CTAG_PRIMITIVE( tag ) );
	return( writeLength( stream, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBitStringHole( INOUT STREAM *stream, 
						IN_LENGTH_SHORT_Z const int length,
						IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH_SHORT );
	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	writeTag( stream, ( tag == DEFAULT_TAG ) ? \
			  BER_BITSTRING : MAKE_CTAG_PRIMITIVE( tag ) );
	writeLength( stream, length + 1 );	/* +1 for bit count */
	return( sputc( stream, 0 ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeGenericHole( INOUT STREAM *stream, 
					  IN_LENGTH_Z const int length,
					  IN_TAG const int tag )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( length >= 0 && length < MAX_INTLENGTH );
	REQUIRES_S( tag >= 0 && tag < MAX_TAG_VALUE );

	writeTag( stream, tag );
	return( writeLength( stream, length ) );
	}
#endif /* USE_INT_ASN1 */
