/****************************************************************************
*																			*
*				Miscellaneous (Non-ASN.1) Read/Write Routines				*
*						Copyright Peter Gutmann 1992-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "bn.h"
  #include "misc_rw.h"
#else
  #include "crypt.h"
  #include "bn/bn.h"
  #include "enc_dec/misc_rw.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Read large integer data */

typedef enum {
	LENGTH_NONE,		/* No length type */
	LENGTH_16U,			/* Unsigned int, 16-bit length */
	LENGTH_16U_BITS,	/* Unsigned int, 16-bit length, length in bits */
	LENGTH_32,			/* Signed int, 32-bit length */
	LENGTH_LAST			/* Last possible length type */
	} LENGTH_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int readInteger( INOUT STREAM *stream, 
						OUT_BUFFER_OPT( maxLength, \
										*integerLength ) void *integer, 
						OUT_LENGTH_BOUNDED_Z( maxLength ) int *integerLength,
						IN_LENGTH_PKC const int minLength, 
						IN_LENGTH_PKC const int maxLength,
						IN_ENUM( LENGTH ) const LENGTH_TYPE lengthType,
						IN_ENUM_OPT( KEYSIZE_CHECK ) \
							const KEYSIZE_CHECK_TYPE checkType )
	{
	int length, status, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( integer == NULL || isWritePtrDynamic( integer, maxLength ) );
	assert( isWritePtr( integerLength, sizeof( int ) ) );

	REQUIRES_S( minLength > 0 && minLength < maxLength && \
				maxLength <= CRYPT_MAX_PKCSIZE );
	REQUIRES_S( isEnumRange( lengthType, LENGTH ) );
	REQUIRES_S( isEnumRangeOpt( checkType, KEYSIZE_CHECK ) );

	/* Clear return values */
	if( integer != NULL )
		memset( integer, 0, min( 16, maxLength ) );
	*integerLength = 0;

	/* Read the length and make sure that it's within range, with a 2-byte 
	   allowance for extra zero-padding (the exact length will be checked 
	   later after the padding is stripped) */
	if( lengthType == LENGTH_16U || lengthType == LENGTH_16U_BITS )
		status = length = readUint16( stream );
	else
		status = length = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( lengthType == LENGTH_16U_BITS )
		length = bitsToBytes( length );
	if( checkType != KEYSIZE_CHECK_NONE )
		{
		REQUIRES( ( checkType == KEYSIZE_CHECK_ECC && \
					minLength >= MIN_PKCSIZE_ECCPOINT ) || \
				  ( checkType != KEYSIZE_CHECK_ECC && \
					minLength >= MIN_PKCSIZE_THRESHOLD ) );

		/* If the length is below the minimum allowed but still looks at 
		   least vaguely valid, report it as a too-short key rather than a
		   bad data error */
		if( checkType == KEYSIZE_CHECK_ECC )
			{
			if( isShortECCKey( length ) )
				return( CRYPT_ERROR_NOSECURE );
			}
		else
			{
			if( isShortPKCKey( length ) )
				return( CRYPT_ERROR_NOSECURE );
			}
		}
	if( length < minLength || length > maxLength + 2 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* If we're reading a signed integer then the sign bit can't be set 
	   since this would produce a negative value.  This differs from the 
	   ASN.1 code, where the incorrect setting of the sign bit is so common 
	   that we always treat integers as unsigned */
	if( lengthType == LENGTH_32 && ( sPeek( stream ) & 0x80 ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Skip possible leading-zero padding and repeat the length check once
	   the zero-padding has been adjusted */
	LOOP_MAX_CHECKINC( length > 0 && sPeek( stream ) == 0, length-- )
		{
		status = sgetc( stream );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( LOOP_BOUND_OK );
	if( checkType != KEYSIZE_CHECK_NONE )
		{
		/* Repeat the earlier check on the adjusted value */
		if( checkType == KEYSIZE_CHECK_ECC )
			{
			if( isShortECCKey( length ) )
				return( CRYPT_ERROR_NOSECURE );
			}
		else
			{
			if( isShortPKCKey( length ) )
				return( CRYPT_ERROR_NOSECURE );
			}
		}
	if( length < minLength || length > maxLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* Read the value */
	*integerLength = length;
	if( integer == NULL )
		return( sSkip( stream, length, MAX_INTLENGTH_SHORT ) );
	return( sread( stream, integer, length ) );
	}

/****************************************************************************
*																			*
*								Data Read Routines							*
*																			*
****************************************************************************/

/* Read 16- and 32-bit integer values */

RETVAL_RANGE( 0, 0xFFFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int readUint16( INOUT STREAM *stream )
	{
	BYTE buffer[ UINT16_SIZE + 8 ];
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = sread( stream, buffer, UINT16_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	value = ( ( long ) buffer[ 0 ] << 8 ) | buffer[ 1 ];
	if( !isIntegerRange( value ) || \
		value > 0xFFFFL || value >= INT_MAX )
		{
		/* On 16-bit systems, INT_MAX may be less than 0xFFFFL */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	return( value );
	}

RETVAL_RANGE( 0, INT_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
int readUint32( INOUT STREAM *stream )
	{
	BYTE buffer[ UINT32_SIZE + 8 ];
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	status = sread( stream, buffer, UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( buffer[ 0 ] & 0x80 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	value = ( ( long ) buffer[ 0 ] << 24 ) | \
			( ( long ) buffer[ 1 ] << 16 ) | \
			( ( long ) buffer[ 2 ] << 8 ) | \
					   buffer[ 3 ];
	if( !isIntegerRange( value ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( value );
	}

/* Read 32-bit time values */

RETVAL_RANGE( 0, INT_MAX ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUint32Time( INOUT STREAM *stream, OUT time_t *timeVal )
	{
	BYTE buffer[ UINT32_SIZE + 8 ];
	long value;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( timeVal, sizeof( time_t ) ) );

	/* Clear return value */
	*timeVal = 0;

	status = sread( stream, buffer, UINT32_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	if( buffer[ 0 ] & 0x80 )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	value = ( ( long ) buffer[ 0 ] << 24 ) | \
			( ( long ) buffer[ 1 ] << 16 ) | \
			( ( long ) buffer[ 2 ] << 8 ) | \
					   buffer[ 3 ];
	if( value < MIN_STORED_TIME_VALUE || value >= MAX_TIME_VALUE )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	*timeVal = ( time_t ) value;
	return( CRYPT_OK );
	}

/* Read 64-bit integer values */

#ifdef USE_WEBSOCKETS

RETVAL_RANGE( 0, INT_MAX ) STDC_NONNULL_ARG( ( 1 ) ) \
int readUint64( INOUT STREAM *stream )
	{
	BYTE buffer[ ( UINT64_SIZE / 2 ) + 8 ];
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* This is never a 64-bit value but always an overprovisioned int/long, 
	   so we verify that the top four bytes are zero and then read it as
	   a Uint32 */
	status = sread( stream, buffer, UINT64_SIZE / 2 );
	if( cryptStatusError( status ) )
		return( status );
	if( memcmp( buffer, "\x00\x00\x00\x00", UINT64_SIZE / 2 ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	return( readUint32( stream ) );
	}
#endif /* USE_WEBSOCKETS */

/* Read a string preceded by a 32-bit length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readData32( INOUT STREAM *stream, 
					   OUT_BUFFER( dataMaxLength, *dataLength ) void *data, 
					   IN_LENGTH_SHORT const int dataMaxLength, 
					   OUT_LENGTH_BOUNDED_Z( dataMaxLength ) int *dataLength,
					   const BOOLEAN includeLengthField,
					   const BOOLEAN zeroLengthOK )
	{
	BYTE *dataPtr = data;
	const int headerSize = includeLengthField ? UINT32_SIZE : 0;
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( data, dataMaxLength ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES_S( isShortIntegerRangeNZ( dataMaxLength ) );
	REQUIRES_S( includeLengthField == TRUE || includeLengthField == FALSE );
	REQUIRES_S( zeroLengthOK == TRUE || zeroLengthOK == FALSE );

	/* Clear return values */
	memset( data, 0, min( 16, dataMaxLength ) );
	*dataLength = 0;

	status = length = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		{
		/* Zero length value */
		if( !zeroLengthOK )
			return( CRYPT_ERROR_BADDATA );
		if( includeLengthField )
			{
			memset( data, 0, UINT32_SIZE );
			*dataLength = UINT32_SIZE;
			}
		return( CRYPT_OK );
		}
	if( !isShortIntegerRangeNZ( length ) )
		{
		/* Avoid integer-overflow warning in the following check */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	if( headerSize + length > dataMaxLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	if( includeLengthField )
		{
		dataPtr[ 0 ] = intToByte( ( length >> 24 ) & 0xFF );
		dataPtr[ 1 ] = intToByte( ( length >> 16 ) & 0xFF );
		dataPtr[ 2 ] = intToByte( ( length >> 8 ) & 0xFF );
		dataPtr[ 3 ] = intToByte( length & 0xFF );
		}
	*dataLength = headerSize + length;
	return( sread( stream, dataPtr + headerSize, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readString32( INOUT STREAM *stream, 
				  OUT_BUFFER( stringMaxLength, \
							  *stringLength ) void *string, 
				  IN_LENGTH_SHORT const int stringMaxLength, 
				  OUT_LENGTH_BOUNDED_Z( stringMaxLength ) int *stringLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( string, stringMaxLength ) );
	assert( isWritePtr( stringLength, sizeof( int ) ) );

	REQUIRES_S( isShortIntegerRangeNZ( stringMaxLength ) );

	/* Read the string, limiting the size to the maximum buffer size */
	return( readData32( stream, string, stringMaxLength, stringLength, 
						FALSE, FALSE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readString32Opt( INOUT STREAM *stream, 
					 OUT_BUFFER( stringMaxLength, \
								 *stringLength ) void *string, 
					 IN_LENGTH_SHORT const int stringMaxLength, 
					 OUT_LENGTH_BOUNDED_Z( stringMaxLength ) \
						int *stringLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( string, stringMaxLength ) );
	assert( isWritePtr( stringLength, sizeof( int ) ) );

	REQUIRES_S( isShortIntegerRangeNZ( stringMaxLength ) );

	/* Read the string, limiting the size to the maximum buffer size, and 
	   with zero-length strings allowed */
	return( readData32( stream, string, stringMaxLength, stringLength, 
						FALSE, TRUE ) );
	}

/* Read a raw object preceded by a 32-bit length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readRawObject32( INOUT STREAM *stream, 
					 OUT_BUFFER( bufferMaxLength, *bufferLength ) \
						void *buffer, 
					 IN_LENGTH_SHORT_MIN( UINT32_SIZE + 1 ) \
						const int bufferMaxLength, 
					 OUT_LENGTH_BOUNDED_Z( bufferMaxLength ) \
						int *bufferLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtrDynamic( buffer, bufferMaxLength ) );
	assert( isWritePtr( bufferLength, sizeof( int ) ) );

	REQUIRES_S( bufferMaxLength >= UINT32_SIZE + 1 && \
				bufferMaxLength < MAX_INTLENGTH_SHORT );

	/* Read the string, limiting the size to the maximum buffer size */
	return( readData32( stream, buffer, bufferMaxLength, bufferLength, 
						TRUE, FALSE ) );
	}

/* Read a universal type and discard it, used to skip unknown or unwanted
   types.  Since it's only used to skip short no-op fields, we limit the
   maximum length to MAX_INTLENGTH_SHORT */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readUniversal( INOUT STREAM *stream, 
						  IN_ENUM( LENGTH ) const LENGTH_TYPE lengthType )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( lengthType == LENGTH_16U || lengthType == LENGTH_32 );

	/* Read the length and skip the data */
	if( lengthType == LENGTH_16U )
		status = length = readUint16( stream );
	else
		status = length = readUint32( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 )
		return( CRYPT_OK );		/* Zero-length data */
	if( !isShortIntegerRangeNZ( length ) )
		return( CRYPT_ERROR_BADDATA );
	return( sSkip( stream, length, MAX_INTLENGTH_SHORT ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal16( INOUT STREAM *stream )
	{
	return( readUniversal( stream, LENGTH_16U ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal32( INOUT STREAM *stream )
	{
	return( readUniversal( stream, LENGTH_32 ) );
	}

/* Read (large) integers in various formats */

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16U( INOUT STREAM *stream, 
					OUT_BUFFER_OPT( maxLength, \
									*integerLength ) void *integer, 
					OUT_LENGTH_BOUNDED_Z( maxLength ) \
						int *integerLength, 
					IN_LENGTH_PKC const int minLength, 
					IN_LENGTH_PKC const int maxLength )
	{
	return( readInteger( stream, integer, integerLength, minLength,
						 maxLength, LENGTH_16U, KEYSIZE_CHECK_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16Ubits( INOUT STREAM *stream, 
						OUT_BUFFER_OPT( maxLength, \
										*integerLength ) void *integer, 
						OUT_LENGTH_BOUNDED_Z( maxLength ) \
							int *integerLength, 
						IN_LENGTH_PKC const int minLength, 
						IN_LENGTH_PKC const int maxLength )
	{
	return( readInteger( stream, integer, integerLength, minLength,
						 maxLength, LENGTH_16U_BITS, KEYSIZE_CHECK_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger32( INOUT STREAM *stream, 
				   OUT_BUFFER_OPT( maxLength, \
								   *integerLength ) void *integer, 
				   OUT_LENGTH_BOUNDED_Z( maxLength  ) int *integerLength, 
				   IN_LENGTH_PKC const int minLength, 
				   IN_LENGTH_PKC const int maxLength )
	{
	return( readInteger( stream, integer, integerLength, minLength,
						 maxLength, LENGTH_32, KEYSIZE_CHECK_NONE ) );
	}

/* Special-case large integer read routines that explicitly check for a too-
   short key and return CRYPT_ERROR_NOSECURE rather than the 
   CRYPT_ERROR_BADDATA that'd otherwise be returned */

#ifdef USE_SSL

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16UChecked( INOUT STREAM *stream, 
						   OUT_BUFFER_OPT( maxLength, *integerLength ) \
								void *integer, 
						   OUT_LENGTH_BOUNDED_Z( maxLength ) \
								int *integerLength, 
						   IN_LENGTH_PKC const int minLength, 
						   IN_LENGTH_PKC const int maxLength )
	{
	return( readInteger( stream, integer, integerLength, minLength,
						 maxLength, LENGTH_16U, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_SSL */

#ifdef USE_SSH

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger32Checked( INOUT STREAM *stream, 
						  OUT_BUFFER_OPT( maxLength, *integerLength ) \
								void *integer, 
						  OUT_LENGTH_BOUNDED_Z( maxLength ) \
								int *integerLength, 
						  IN_LENGTH_PKC const int minLength, 
						  IN_LENGTH_PKC const int maxLength )
	{
	/* Perform the appropriate length check for the algorithm type.  The
	   way that this is currently done is a bit of a nasty hack, but it
	   saves having to create a special-case ECC-only function that's only
	   used in one place */
	if( minLength == MIN_PKCSIZE_ECCPOINT && \
		maxLength == MAX_PKCSIZE_ECCPOINT )
		{
		return( readInteger( stream, integer, integerLength, minLength,
							 maxLength, LENGTH_32, KEYSIZE_CHECK_ECC ) );
		}
	return( readInteger( stream, integer, integerLength, minLength,
						 maxLength, LENGTH_32, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_SSH */

#ifdef USE_PKC

/* Read integers as bignums in various formats */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readBignumInteger( INOUT STREAM *stream, 
							  INOUT TYPECAST( BIGNUM * ) void *bignum,
							  IN_LENGTH_PKC const int minLength, 
							  IN_LENGTH_PKC const int maxLength,
							  IN_OPT TYPECAST( BIGNUM * ) const void *maxRange, 
							  IN_ENUM( LENGTH ) const LENGTH_TYPE lengthType,
							  IN_ENUM_OPT( KEYSIZE_CHECK ) \
									const KEYSIZE_CHECK_TYPE checkType )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( bignum, sizeof( BIGNUM ) ) );
	assert( maxRange == NULL || isReadPtr( maxRange, sizeof( BIGNUM ) ) );

	REQUIRES_S( minLength > 0 && minLength < maxLength && \
				maxLength <= CRYPT_MAX_PKCSIZE );
	REQUIRES_S( isEnumRange( lengthType, LENGTH ) );
	REQUIRES_S( isEnumRangeOpt( checkType, KEYSIZE_CHECK ) );

	/* Read the integer data */
	status = readInteger( stream, buffer, &length, minLength, maxLength, 
						  lengthType, checkType );
	if( cryptStatusError( status ) )
		return( status );

	/* Convert the value to a bignum.  Note that we use the KEYSIZE_CHECK
	   parameter for both readInteger() and importBignum(), since the 
	   former merely checks the byte count while the latter actually parses 
	   and processes the bignum */
	status = importBignum( bignum, buffer, length, minLength, maxLength, 
						   maxRange, checkType );
	if( cryptStatusError( status ) )
		status = sSetError( stream, status );
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	return( status );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16U( INOUT STREAM *stream, 
						  INOUT TYPECAST( BIGNUM * ) void *bignum, 
						  IN_LENGTH_PKC const int minLength, 
						  IN_LENGTH_PKC const int maxLength, 
						  IN_OPT TYPECAST( BIGNUM * ) const void *maxRange )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength,
							   maxRange, LENGTH_16U, KEYSIZE_CHECK_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16Ubits( INOUT STREAM *stream, 
							  INOUT TYPECAST( BIGNUM * ) void *bignum, 
							  IN_LENGTH_PKC_BITS const int minBits,
							  IN_LENGTH_PKC_BITS const int maxBits,
							  IN_OPT TYPECAST( BIGNUM * ) const void *maxRange )
	{
	return( readBignumInteger( stream, bignum, bitsToBytes( minBits ),
							   bitsToBytes( maxBits ), maxRange, 
							   LENGTH_16U_BITS, KEYSIZE_CHECK_NONE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger32( INOUT STREAM *stream, 
						 INOUT TYPECAST( BIGNUM * ) void *bignum, 
						 IN_LENGTH_PKC const int minLength, 
						 IN_LENGTH_PKC const int maxLength, 
						 IN_OPT TYPECAST( BIGNUM * ) const void *maxRange )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength, 
							   maxRange, LENGTH_32, KEYSIZE_CHECK_NONE ) );
	}

/* Special-case bignum read routines that explicitly check for a too-short 
   key and return CRYPT_ERROR_NOSECURE rather than the CRYPT_ERROR_BADDATA
   that'd otherwise be returned */

#ifdef USE_SSL

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16UChecked( INOUT STREAM *stream, 
								 INOUT TYPECAST( BIGNUM * ) void *bignum, 
								 IN_LENGTH_PKC const int minLength, 
								 IN_LENGTH_PKC const int maxLength )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength, NULL, 
							   LENGTH_16U, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_SSL */

#ifdef USE_PGP

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16UbitsChecked( INOUT STREAM *stream, 
									 INOUT TYPECAST( BIGNUM * ) void *bignum, 
									 IN_LENGTH_PKC_BITS const int minBits,
									 IN_LENGTH_PKC_BITS const int maxBits )
	{
	return( readBignumInteger( stream, bignum, bitsToBytes( minBits ),
							   bitsToBytes( maxBits ), NULL, 
							   LENGTH_16U_BITS, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_PGP */

#ifdef USE_SSH

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger32Checked( INOUT STREAM *stream, 
								INOUT TYPECAST( BIGNUM * ) void *bignum, 
								IN_LENGTH_PKC const int minLength, 
								IN_LENGTH_PKC const int maxLength )
	{
	return( readBignumInteger( stream, bignum, minLength, maxLength, 
							   NULL, LENGTH_32, KEYSIZE_CHECK_PKC ) );
	}
#endif /* USE_SSH */
#endif /* USE_PKC */

/****************************************************************************
*																			*
*								Data Write Routines							*
*																			*
****************************************************************************/

/* Write 16-, 32- and 64-bit integer values */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint16( INOUT STREAM *stream, 
				 IN_RANGE( 0, 0xFFFF ) const int value )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( isIntegerRange( value ) && value <= 0xFFFFL );

	sputc( stream, ( value >> 8 ) & 0xFF );
	return( sputc( stream, value & 0xFF ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint32( INOUT STREAM *stream, IN_INT_Z const long value )
	{
	BYTE buffer[ UINT32_SIZE + 8 ];

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( isIntegerRange( value ) );

	buffer[ 0 ] = intToByte( ( value >> 24 ) & 0xFF );
	buffer[ 1 ] = intToByte( ( value >> 16 ) & 0xFF );
	buffer[ 2 ] = intToByte( ( value >> 8 ) & 0xFF );
	buffer[ 3 ] = intToByte( value & 0xFF );
	return( swrite( stream, buffer, UINT32_SIZE ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint64( INOUT STREAM *stream, IN_INT_Z const long value )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( isIntegerRange( value ) );

	swrite( stream, "\x00\x00\x00\x00", UINT64_SIZE / 2 );
	return( writeUint32( stream, value ) );
	}

/* Write 32-bit time values */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint32Time( INOUT STREAM *stream, const time_t timeVal )
	{
	REQUIRES_S( timeVal >= MIN_TIME_VALUE );

	return( writeUint32( stream, ( int ) timeVal ) );
	}

/* Write a string preceded by a 32-bit length */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeString32( INOUT STREAM *stream, 
				   IN_BUFFER( stringLength ) const void *string, 
				   IN_LENGTH_SHORT const int stringLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( string, stringLength ) );

	REQUIRES_S( isIntegerRangeNZ( stringLength ) );

	writeUint32( stream, stringLength );
	return( swrite( stream, string, stringLength ) );
	}

/* Write large integers in various formats */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeInteger( INOUT STREAM *stream, 
						 IN_BUFFER( integerLength ) const void *integer, 
						 IN_LENGTH_PKC const int integerLength,
						 IN_ENUM( LENGTH ) const LENGTH_TYPE lengthType )
	{
	const BYTE *intPtr = integer;
	int length = integerLength, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( integer, integerLength ) );

	REQUIRES_S( integerLength > 0 && integerLength <= CRYPT_MAX_PKCSIZE );
	REQUIRES_S( isEnumRange( lengthType, LENGTH ) );

	/* Integers may be passed to us from higher-level code with leading 
	   zeroes as part of the encoding.  Before we write them out we strip
	   out any superfluous leading zeroes that may be present */
	LOOP_LARGE_CHECKINC( length > 0 && *intPtr == 0, ( length--, intPtr++ ) )
		{
		/* In theory this could be a problem since quietly changing the 
		   length of a low-level internal value before writing it will cause 
		   problems with higher-level code that doesn't expect to have the 
		   data length of internal components changed, however in practice it
		   only occurs with PGP signatures which are just a raw integer
		   value, so we don't invoke any kind of warning */
		/* assert( DEBUG_WARN ); */
		}
	ENSURES_S( LOOP_BOUND_OK );
	ENSURES_S( length > 0 );

	switch( lengthType )
		{
		case LENGTH_16U:
			writeUint16( stream, length );
			break;

		case LENGTH_16U_BITS:
			writeUint16( stream, bytesToBits( length ) );
			break;

		case LENGTH_32:
			{
			const int leadingOneBit = ( *intPtr & 0x80 ) ? 1 : 0;

			writeUint32( stream, length + leadingOneBit );
			if( leadingOneBit )
				sputc( stream, 0 );	/* MPIs are signed values */
			break;
			}

		default:
			retIntError_Stream( stream );
		}
	return( swrite( stream, intPtr, length ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger16U( INOUT STREAM *stream, 
					 IN_BUFFER( integerLength ) const void *integer, 
					 IN_LENGTH_PKC const int integerLength )
	{
	return( writeInteger( stream, integer, integerLength, LENGTH_16U ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger16Ubits( INOUT STREAM *stream, 
						 IN_BUFFER( integerLength ) const void *integer, 
						 IN_LENGTH_PKC const int integerLength )
	{
	return( writeInteger( stream, integer, integerLength, LENGTH_16U_BITS ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger32( INOUT STREAM *stream, 
					IN_BUFFER( integerLength ) const void *integer, 
					IN_LENGTH_PKC const int integerLength )
	{
	return( writeInteger( stream, integer, integerLength, LENGTH_32 ) );
	}

#ifdef USE_PKC

/* Write integers from bignums in various formats */

CHECK_RETVAL_RANGE( UINT32_SIZE, MAX_INTLENGTH_SHORT ) STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofBignumInteger32( const void *bignum )
	{
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	return( UINT32_SIZE + BN_high_bit( ( BIGNUM * ) bignum ) + \
						  BN_num_bytes( bignum ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeBignumInteger( INOUT STREAM *stream, 
							   TYPECAST( BIGNUM * ) const void *bignum,
							   IN_ENUM( LENGTH ) const LENGTH_TYPE lengthType )
	{
	BYTE buffer[ CRYPT_MAX_PKCSIZE + 8 ];
	int bnLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( bignum, sizeof( BIGNUM ) ) );

	REQUIRES_S( isEnumRange( lengthType, LENGTH ) );

	status = exportBignum( buffer, CRYPT_MAX_PKCSIZE, &bnLength, bignum );
	ENSURES_S( cryptStatusOK( status ) );
	ENSURES_S( bnLength > 0 && bnLength <= CRYPT_MAX_PKCSIZE );
	if( lengthType == LENGTH_16U_BITS )
		{
		int bitCount;

		/* We can't call down to writeInteger() from here because we need to 
		   write a precise length in bits rather than a value reconstructed 
		   from the byte count.  This also means that we can't easily 
		   perform the leading-zero truncation that writeInteger() does 
		   without a lot of low-level fiddling that duplicates code in
		   writeInteger() */
		status = bitCount = BN_num_bits( bignum );
		if( cryptStatusError( status ) )
			return( status );
		writeUint16( stream, bitCount );
		status = swrite( stream, buffer, bnLength );
		}
	else
		status = writeInteger( stream, buffer, bnLength, lengthType );
	zeroise( buffer, CRYPT_MAX_PKCSIZE );
	return( status );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumInteger16U( INOUT STREAM *stream, 
						   TYPECAST( BIGNUM * ) const void *bignum )
	{
	return( writeBignumInteger( stream, bignum, LENGTH_16U ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumInteger16Ubits( INOUT STREAM *stream, 
							   TYPECAST( BIGNUM * ) const void *bignum )
	{
	return( writeBignumInteger( stream, bignum, LENGTH_16U_BITS ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBignumInteger32( INOUT STREAM *stream, 
						  TYPECAST( BIGNUM * ) const void *bignum )
	{
	return( writeBignumInteger( stream, bignum, LENGTH_32 ) );
	}
#endif /* USE_PKC */
