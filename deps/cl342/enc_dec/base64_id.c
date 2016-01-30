/****************************************************************************
*																			*
*					 cryptlib PKI UserID En/Decoding Routines				*
*						Copyright Peter Gutmann 1998-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/* The number of bits in each code group of 5 characters */

#define BITS_PER_GROUP	( 5 * 5 )	/* 5 chars encoding 5 bits each */

/* En/decode tables for text representations of binary keys.  For the two
   mask tables, only positions 4...7 are used */

static const char codeTable[] = \
			"ABCDEFGHJKLMNPQRSTUVWXYZ23456789____";	/* No O/0, I/1 */
static const int hiMask[] = \
			{ 0x00, 0x00, 0x00, 0x00, 0x0F, 0x07, 0x03, 0x01, 0x00, 0x00 };
static const int loMask[] = \
			{ 0x00, 0x00, 0x00, 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0x00, 0x00 };

/****************************************************************************
*																			*
*						PKI User ID Encoding Functions						*
*																			*
****************************************************************************/

/* Adjust the binary form of a PKI user ID so that it can be encoded into a
   fixed number of text characters.  This function is required because key 
   lookup is performed on the decoded form of the ID that's supplied via PKI 
   user requests, if we used the non-adjusted form for the key lookup then 
   we couldn't locate the stored user info that's indexed from the adjusted 
   form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int adjustPKIUserValue( INOUT_BUFFER( valueMaxLength, *valueLength ) \
									BYTE *value, 
							   IN_LENGTH_SHORT_MIN( 32 ) \
									const int valueMaxLength, 
							   OUT_LENGTH_BOUNDED_Z( valueMaxLength ) \
									int *valueLength,
							   IN_RANGE( 3, 4 ) const int noCodeGroups )
	{
	assert( isWritePtr( value, valueMaxLength ) );
	assert( isWritePtr( valueLength, sizeof( int ) ) );
	
	REQUIRES( valueMaxLength >= 32 && valueMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( noCodeGroups == 3 || noCodeGroups == 4 );

	/* Mask off any bits at the end of the data that can't be encoded using
	   the given number of code groups */
	if( noCodeGroups == 3 )
		{
		/* Length = ( ( roundUp( 3 * BITS_PER_GROUP, 8 ) / 8 ) - 1 )
				  = ( 80 / 8 ) - 1
				  = 9
		   Mask = ( 0xFF << ( 8 - ( ( 3 * BITS_PER_GROUP ) % 8 ) ) ) 
				= ( 0xFF << ( 8 - 3 ) )
				= 0xE0 */
		value[ 8 ] &= 0xE0;
		*valueLength = 9;
		}
	else
		{
		/* Length = ( ( roundUp( 4 * BITS_PER_GROUP, 8 ) / 8 ) - 1 )
				  = ( 104 / 8 ) - 1
				  = 12
		   Mask = ( 0xFF << ( 8 - ( ( 4 * BITS_PER_GROUP ) % 8 ) ) ) 
				= ( 0xFF << ( 8 - 4 ) )
				= 0xF0 */
		value[ 11 ] &= 0xF0;
		*valueLength = 12;
		}

	return( CRYPT_OK );
	}

/* Encode a text representation of a binary key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int encodePKIUserValue( OUT_BUFFER( encValMaxLen, *encValLen ) char *encVal, 
						IN_LENGTH_SHORT_MIN( 10 ) const int encValMaxLen, 
						OUT_LENGTH_BOUNDED_Z( encValMaxLen ) int *encValLen,
						IN_BUFFER( valueLen ) const BYTE *value, 
						IN_LENGTH_SHORT_MIN( 8 ) const int valueLen, 
						IN_RANGE( 3, 4 ) const int noCodeGroups )
	{
	BYTE valBuf[ 128 + 8 ];
	const int dataBytes = ( roundUp( noCodeGroups * BITS_PER_GROUP, 8 ) / 8 );
	int i, byteCount = 0, bitCount = 0, length, status;

	assert( isWritePtr( encVal, encValMaxLen ) );
	assert( isWritePtr( encValLen, sizeof( int ) ) );
	assert( isReadPtr( value, dataBytes ) );

	REQUIRES( encValMaxLen >= 10 && encValMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( valueLen >= 8 && valueLen < MAX_INTLENGTH_SHORT );
	REQUIRES( noCodeGroups == 3 || noCodeGroups == 4 );
	REQUIRES( dataBytes >= 10 && dataBytes < 64 );
	REQUIRES( valueLen >= dataBytes - 1 );
			  /* There must be enough input data present to produce the 
			     required number of output bytes minus one for the checksum
				 at the start */

	/* Clear return values */
	memset( encVal, 0, min( 16, encValMaxLen ) );
	*encValLen = 0;

	/* Copy across the data bytes, leaving a gap at the start for the
	   checksum */
	memcpy( valBuf + 1, value, dataBytes - 1 );
	status = adjustPKIUserValue( valBuf + 1, 128 - 1, &length, 
								 noCodeGroups );
	if( cryptStatusError( status ) )
		return( status );
	length += 1;

	/* Calculate the Fletcher checksum and prepend it to the data bytes
	   This is easier than handling the addition of a non-byte-aligned
	   quantity to the end of the data */
	valBuf[ 0 ] = intToByte( checksumData( valBuf + 1, length - 1 ) & 0xFF );

	/* Encode the binary data as text */
	for( length = 0, i = 1; i <= noCodeGroups * 5; i++ )
		{
		int chunkValue;

		/* Extract the next 5-bit chunk and convert it to text form */
		if( bitCount < 3 )
			{
			/* Everything's present in one byte, shift it down to the LSB */
			chunkValue = ( valBuf[ byteCount ] >> ( 3 - bitCount ) ) & 0x1F;
			}
		else
			{
			if( bitCount == 3 )
				{
				/* It's the 5 LSBs */
				chunkValue = valBuf[ byteCount ] & 0x1F;
				}
			else
				{
				/* The data spans two bytes, shift the bits from the high
				   byte up and the bits from the low byte down */
				chunkValue = ( ( valBuf[ byteCount ] & \
								 hiMask[ bitCount ] ) << ( bitCount - 3 ) ) | \
							 ( ( valBuf[ byteCount + 1 ] & \
								 loMask[ bitCount ] ) >> ( 11 - bitCount ) );
				}
			}
		ENSURES( chunkValue >= 0 && chunkValue <= 0x20 );
		encVal[ length++ ] = codeTable[ chunkValue ];
		if( length < encValMaxLen && ( i % 5 ) == 0 && i < noCodeGroups * 5 )
			encVal[ length++ ] = '-';
		ENSURES( length < encValMaxLen );

		/* Advance by 5 bits */
		bitCount += 5;
		if( bitCount >= 8 )
			{
			bitCount -= 8;
			byteCount++;
			}
		ENSURES( bitCount >= 0 && bitCount < 8 );
		ENSURES( byteCount >= 0 && byteCount < 64 );
		}
	*encValLen = length;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							PKI User ID Decoding Functions					*
*																			*
****************************************************************************/

/* Check whether a text string appears to be an encoded PKI user value */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN isPKIUserValue( IN_BUFFER( encValLength ) const char *encVal, 
						IN_LENGTH_SHORT_MIN( 10 ) const int encValLength )
	{
	int i = 0;

	assert( isReadPtr( encVal, encValLength ) );

	REQUIRES_B( encValLength > 10 && encValLength < MAX_INTLENGTH_SHORT );

	/* Check whether a user value is of the form XXXXX-XXXXX-XXXXX{-XXXXX}.  
	   Although we shouldn't be seeing O/0 or I/1 in the input we don't
	   specifically check for these since they could be present as typos.
	   In other words we're checking for the presence of an input pattern
	   that matches an encoded PKI user value, not for the validity of the
	   value itself, which will be checked by decodePKIUserValue() */
	if( ( encValLength != ( 3 * 5 ) + 2 ) && \
		( encValLength != ( 4 * 5 ) + 3 ) )
		return( FALSE );
	while( i < encValLength )
		{
		int j;

		/* Decode each character group.  We know from the length check above
		   that this won't run off the end of the data, so we don't have to
		   check the index value */
		for( j = 0; j < 5; j++ )
			{
			const int ch = byteToInt( encVal[ i++ ] );

			if( !isAlnum( ch ) )
				return( FALSE );
			}
		if( i < encValLength && encVal[ i++ ] != '-' )
			return( FALSE );
		}
	return( TRUE );
	}

/* Decode a text representation of a binary key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int decodePKIUserValue( OUT_BUFFER( valueMaxLen, *valueLen ) BYTE *value, 
						IN_LENGTH_SHORT_MIN( 10 ) const int valueMaxLen, 
						OUT_LENGTH_BOUNDED_Z( valueMaxLen ) int *valueLen,
						IN_BUFFER( encValLength ) const char *encVal, 
						IN_LENGTH_SHORT const int encValLength )
	{
	BYTE valBuf[ 128 + 8 ];
	char encBuf[ CRYPT_MAX_TEXTSIZE + 8 ];
	int i = 0, byteCount = 0, bitCount = 0, length = 0;

	assert( isWritePtr( value, valueMaxLen ) );
	assert( isWritePtr( valueLen, sizeof( int ) ) );
	assert( isReadPtr( encVal, encValLength ) );

	REQUIRES( valueMaxLen >= 10 && valueMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( encValLength > 0 && encValLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( value, 0, min( 16, valueMaxLen ) );
	*valueLen = 0;

	/* Make sure that the input has a reasonable length (this should have 
	   been checked by the caller using isPKIUserValue(), so we throw an
	   exception if the check fails).  We return CRYPT_ERROR_BADDATA rather 
	   than the more obvious CRYPT_ERROR_OVERFLOW since something returned 
	   from this low a level should be a consistent error code indicating 
	   that there's a problem with the PKI user value as a whole */
	if( encValLength < ( 3 * 5 ) || encValLength > CRYPT_MAX_TEXTSIZE )
		{
		DEBUG_DIAG(( "PKI user value has invalid length" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_BADDATA );
		}

	REQUIRES( isPKIUserValue( encVal, encValLength ) );

	/* Undo the formatting of the encoded value from XXXXX-XXXXX-XXXXX... 
	   to XXXXXXXXXXXXXXX... */
	while( i < encValLength )
		{
		int j;

		for( j = 0; j < 5; j++ )
			{
			const int ch = byteToInt( encVal[ i++ ] );

			/* Note that we've just incremented 'i', so the range check is
			   '>' rather than '>=' */
			if( !isAlnum( ch ) || i > encValLength )
				return( CRYPT_ERROR_BADDATA );
			encBuf[ length++ ] = intToByte( toUpper( ch ) );
			}
		if( i < encValLength && encVal[ i++ ] != '-' )
			return( CRYPT_ERROR_BADDATA );
		}
	if( ( length % 5 ) != 0 || length > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );

	/* Decode the text data into binary */
	memset( valBuf, 0, 128 );
	for( i = 0; i < length; i ++ )
		{
		const int ch = byteToInt( encBuf[ i ] );
		int chunkValue;

		for( chunkValue = 0; chunkValue < 0x20; chunkValue++ )
			{
			if( codeTable[ chunkValue ] == ch )
				break;
			}
		if( chunkValue >= 0x20 )
			return( CRYPT_ERROR_BADDATA );

		/* Extract the next 5-bit chunk and convert it to text form */
		if( bitCount < 3 )
			{
			/* Everything's present in one byte, shift it up into position */
			valBuf[ byteCount ] |= chunkValue << ( 3 - bitCount );
			}
		else
			{
			if( bitCount == 3 )
				{
				/* It's the 5 LSBs */
				valBuf[ byteCount ] |= chunkValue;
				}
			else
				{
				/* The data spans two bytes, shift the bits from the high
				   byte down and the bits from the low byte up */
				valBuf[ byteCount ] |= \
							intToByte( ( chunkValue >> ( bitCount - 3 ) ) & \
										 hiMask[ bitCount ] );
				valBuf[ byteCount + 1 ] = \
							intToByte( ( chunkValue << ( 11 - bitCount ) ) & \
										 loMask[ bitCount ] );
				}
			}

		/* Advance by 5 bits */
		bitCount += 5;
		if( bitCount >= 8 )
			{
			bitCount -= 8;
			byteCount++;
			}
		ENSURES( bitCount >= 0 && bitCount < 8 );
		ENSURES( byteCount >= 0 && byteCount < 64 );
		}

	/* Calculate the Fletcher checksum and make sure that it matches the
	   value at the start of the data bytes */
	if( bitCount > 0 )
		byteCount++;	/* More bits in the last partial byte */
	if( valBuf[ 0 ] != ( checksumData( valBuf + 1, byteCount - 1 ) & 0xFF ) )
		return( CRYPT_ERROR_BADDATA );

	/* Return the decoded value to the caller */
	ENSURES( byteCount >= 2 && byteCount - 1 <= valueMaxLen );
	memcpy( value, valBuf + 1, byteCount - 1 );
	*valueLen = byteCount - 1;

	return( CRYPT_OK );
	}
