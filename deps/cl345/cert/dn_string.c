/****************************************************************************
*																			*
*							Certificate String Routines						*
*						Copyright Peter Gutmann 1996-2013					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "cert.h"
  #include "misc_rw.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/misc_rw.h"
#endif /* Compiler-specific includes */

/* The character set (or at least ASN.1 string type) for a string.  Although 
   IA5String and VisibleString/ISO646String are technically different the 
   only real difference is that IA5String allows the full range of control 
   characters, which isn't notably useful.  For this reason we treat both as 
   ISO646String.  Sometimes we can be fed Unicode strings that are just 
   bloated versions of another string type so we need to account for these 
   as well.

   UTF-8 strings are a pain because they're only intermittently supported as 
   a native format.  For this reason we convert them to a more useful local
   character set (ASCII, 8859-1, or Unicode as appropriate) when we read 
   them to make them usable.  Although their use was required after the 
   cutover date of December 2003, by unspoken unanimous consensus of 
   implementers everywhere implementations stuck with the existing DN 
   encoding to avoid breaking things.  Several years after the cutoff date 
   vendors slowly started introducing UTF-8 support to certificate-using
   applications, although on an ad-hoc basis and sometimes only as a user-
   configurable option so it's still too risky to rely on this being
   supported */

typedef enum {
	STRINGTYPE_NONE,					/* No string type */

	/* 8-bit character types */
	STRINGTYPE_PRINTABLE,				/* PrintableString */
	STRINGTYPE_IA5,						/* IA5String */
	STRINGTYPE_T61,						/* T61 (8859-1) string */

	/* 8-bit types masquerading as Unicode */
	STRINGTYPE_UNICODE_PRINTABLE,		/* PrintableString as Unicode */
	STRINGTYPE_UNICODE_IA5,				/* IA5String as Unicode */
	STRINGTYPE_UNICODE_T61,				/* 8859-1 as Unicode */

	/* 8-bit native character string (e.g. eastern European latin-2 or 
	   Windows CP 1251) that needs to be converted to Unicode (only valid 
	   for strings originating from the local system) */
	STRINGTYPE_TO_UNICODE,				/* 8-bit string needing conversion */

	/* Unicode/UTF-8 */
	STRINGTYPE_UNICODE,					/* Unicode string */
	STRINGTYPE_UTF8,					/* UTF-8 string */

	/* Special-case error string types.  STRINGTYPE_ERROR_TRY_UNICODE tells
	   the caller to try converting from a system-native 8-bit character set
	   type to Unicode before reporting an error in case this is some 
	   oddball 8-bit character set that the local system understands but that
	   isn't equivalent to any ASN.1 string type.  If the string type can be
	   converted to Unicode it's then reported as STRINGTYPE_TO_UNICODE */
	STRINGTYPE_ERROR,					/* Error occurred during processing */
	STRINGTYPE_ERROR_TRY_UNICODE,		/* Try converting to Unicode */

	STRINGTYPE_LAST						/* Last possible string type */
	} ASN1_STRINGTYPE;

#define isErrorStringType( stringType ) \
		( ( stringType ) == STRINGTYPE_ERROR_TRY_UNICODE || \
		  ( stringType ) == STRINGTYPE_ERROR )

/* Since wchar_t can be anything from 8 bits (Borland C++ under DOS) to 32 
   bits (some RISC Unixen) we define a bmpchar_t for Unicode/BMPString 
   characters which is always 16 bits as required for BMPStrings, to match 
   the naming convention of wchar_t.  The conversion to and from a BMPString 
   and wchar_t may require narrowing or widening of characters and possibly 
   endianness conversion as well */

typedef unsigned short int bmpchar_t;	/* Unicode/BMPString data type */
#define UCSIZE	2

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*						Character Set Management Functions					*
*																			*
****************************************************************************/

/* Because of the bizarre (and mostly useless) collection of ASN.1 character
   types we need to be very careful about what we allow in a string.  The
   following table is used to determine whether a character is valid within 
   a given string type.

   Although IA5String and VisibleString/ISO646String are technically
   different the only real difference is that IA5String allows the full 
   range of control characters, which isn't notably useful.  For this reason 
   we treat both as ISO646String */

#define P	1						/* PrintableString */
#define I	2						/* IA5String/VisibleString/ISO646String */
#define PI	( P | I )				/* PrintableString and IA5String */

static const int asn1CharFlags[] = {
	/* 00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F */
		0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	/* 10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F */
		0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	/*		!	"	#	$	%	&	'	(	)	*	+	,	-	.	/ */
	   PI,	I,	I,	I,	I,	I,	I, PI, PI, PI,	I, PI, PI, PI, PI, PI,
	/*	0	1	2	3	4	5	6	7	8	9	:	;	<	=	>	? */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I, PI,	I, PI,
	/*	@	A	B	C	D	E	F	G	H	I	J	K	L	M	N	O */
		I, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,
	/*	P	Q	R	S	T	U	V	W	X	Y	Z	[	\	]	^	_ */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I,	I,	I,	I,
	/*	`	a	b	c	d	e	f	g	h	i	j	k	l	m	n	o */
		I, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,
	/*	p	q	r	s	t	u	v	w	x	y	z	{	|	}	~  DL */
	   PI, PI, PI, PI, PI, PI, PI, PI, PI, PI, PI,	I,	I,	I,	I,	0,
		0, 0	/* Catch overflows */
	};

#define nativeCharFlags	asn1CharFlags

/* Extract a widechar or bmpchar from an (arbitrarily-aligned) string.  Note
   that if WCSIZE == sizeof( wchar_t ) is larger than 16 bits and we're fed 
   malformed data then the return value can become larger than what's 
   indicated in the CHECK_RETVAL_RANGE() statement, but the calling code 
   checks for this so it's mostly a slightly misleading static-analysis
   annotation */

CHECK_RETVAL_RANGE_NOERROR( 0, 0xFFFF ) STDC_NONNULL_ARG( ( 1 ) ) \
static wchar_t getNativeWidechar( IN_BUFFER_C( 2 ) const BYTE *string )
	{
	wchar_t ch = 0;
#ifdef DATA_LITTLEENDIAN
	int shiftAmt = 0;
#endif /* DATA_LITTLEENDIAN */
	int i, LOOP_ITERATOR;

	assert( isReadPtr( string, 2 ) );
	
	/* Since we're reading wchar_t-sized values from a char-aligned source, 
	   we have to assemble the data a byte at a time to handle systems where 
	   non-char values can only be accessed on word-aligned boundaries */
	LOOP_SMALL( i = 0, i < WCSIZE, i++ )
		{
#ifdef DATA_LITTLEENDIAN
		ch |= string[ i ] << shiftAmt;
		shiftAmt += 8;
#else
		ch = ( ch << 8 ) | string[ i ];
#endif /* DATA_LITTLEENDIAN */
		}
	ENSURES_EXT( LOOP_BOUND_OK, 0 );

	return( ch );
	}

/****************************************************************************
*																			*
*								UTF-8 Functions								*
*																			*
****************************************************************************/

/* Parse one UTF-8 encoded character from a string, enforcing the UTF-8 
   canonical-encoding rules:

	  00 -  7F = 0xxxxxxx, mask = 0x80
	 80 -  7FF = 110xxxxx 10xxxxxx, mask = 0xE0
	800 - FFFF = 1110xxxx 10xxxxxx 10xxxxxx, mask = 0xF0

   Note that some of the checks applied below are redundant but due to the 
   powerful nature of UTF-8 string formatting attacks we apply them anyway 
   to make the different checking actions explicit and to reduce the chances 
   of a coding error that allows something dangerous to slip through */

STDC_NONNULL_ARG( ( 1 ) ) \
static long getUTF8Char( INOUT STREAM *stream )
	{
	long largeCh;
	int firstChar, secondChar, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Process the first character */
	status = firstChar = sgetc( stream );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );
	if( !( firstChar & 0x80 ) )
		{
		/* 1-byte character, straight ASCII */
		return( firstChar & 0x7F );
		}
	if( ( firstChar & 0xC0 ) == 0x80 )		/* 11xxxxxx -> 10xxxxxx */
		return( CRYPT_ERROR_BADDATA );

	/* Process the second character */
	status = secondChar = sgetc( stream );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );
	if( ( firstChar & 0xE0 ) == 0xC0 )		/* 111xxxxx -> 110xxxxx */
		{
		/* 2-byte character in the range 0x80...0x7FF */
		if( ( secondChar & 0xC0 ) != 0x80 )
			return( CRYPT_ERROR_BADDATA );
		largeCh = ( ( firstChar & 0x1F ) << 6 ) | \
					( secondChar & 0x3F );
		if( largeCh < 0x80 || largeCh > 0x7FF )
			return( CRYPT_ERROR_BADDATA );
		return( largeCh & 0x7FF );
		}

	/* Process any further characters */
	if( ( firstChar & 0xF0 ) == 0xE0 )		/* 1111xxxx -> 1110xxxx */
		{
		int thirdChar;
		
		status = thirdChar = sgetc( stream );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_BADDATA );

		/* 3-byte character in the range 0x800...0xFFFF */
		if( ( secondChar & 0xC0 ) != 0x80 || \
			( thirdChar & 0xC0 ) != 0x80 )
			return( CRYPT_ERROR_BADDATA );
		largeCh = ( ( firstChar & 0x1F ) << 12 ) | \
				  ( ( secondChar & 0x3F ) << 6 ) | \
					( thirdChar & 0x3F );
		if( largeCh < 0x800 || largeCh > 0xFFFF )
			return( CRYPT_ERROR_BADDATA );
		return( largeCh & 0xFFFF );
		}

	/* In theory we can also get 4- and 5-byte encodings but this is far 
	   more likely to be invalid data than a genuine attempt to represent 
	   something in Tsolyani */
	return( CRYPT_ERROR_BADDATA );
	}

#if 0	/* Not currently used, see note at start */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int putUtf8Char( INOUT STREAM *stream, 
						IN_RANGE( 0, 0xFFFF ) const long largeCh )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_S( largeCh >= 0 && largeCh <= 0xFFFF );

	if( largeCh < 0x80 )
		return( sputc( stream, ( BYTE ) largeCh ) );
	if( largeCh < 0x0800 )
		{
		sputc( stream, ( BYTE )( 0xC0 | ( ( largeCh >> 6 ) & 0x1F ) ) );
		return( sputc( stream, ( BYTE )( 0x80 | ( largeCh & 0x3F ) ) ) );
		}
	sputc( stream, ( BYTE )( 0xE0 | ( ( largeCh >> 12 ) & 0x0F ) ) );
	sputc( stream, ( BYTE )( 0x80 | ( ( largeCh >> 6 ) & 0x3F ) ) );
	return( sputc( stream, ( BYTE )( 0x80 | largeCh & 0x3F ) ) );
	}

/* Determine the length of a string once it's encoded as UTF-8 */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int utf8TargetStringLen( IN_BUFFER( stringLen ) const void *string, 
								IN_LENGTH_SHORT const int stringLen,
								OUT_LENGTH_SHORT_Z int *targetStringLength,
								const BOOLEAN isWideChar )
	{
	STREAM stream;
	int i, status = CRYPT_OK, LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );
	assert( isWritePtr( targetStringLength, sizeof( int ) ) );

	REQUIRES( isShortIntegerRangeNZ( stringLen ) );
	REQUIRES( isWideChar == TRUE || isWideChar == FALSE );

	/* Clear return value */
	*targetStringLength = 0;

	sMemNullOpen( &stream );
	if( isWideChar )
		{
		const wchar_t *wcStrPtr = ( wchar_t * ) string;
		const int wcStrLen = stringLen / WCSIZE;

		REQUIRES( stringLen % WCSIZE == 0 );

		LOOP_LARGE( i = 0, i < wcStrLen, i++ )
			{
			status = putUtf8Char( &stream, wcStrPtr[ i ] );
			if( cryptStatusError( status ) )
				break;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	else
		{
		const BYTE *strPtr = string;

		LOOP_LARGE( i = 0, i < stringLen, i++ )
			{
			status = putUtf8Char( &stream, strPtr[ i ] );
			if( cryptStatusError( status ) )
				break;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	if( cryptStatusOK( status ) )
		*targetStringLength = stell( &stream );
	sMemClose( &stream );

	return( status );
	}
#endif /* 0 */

/* Check that a UTF-8 string has a valid encoding */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkUtf8String( IN_BUFFER( stringLen ) const void *string, 
							IN_LENGTH_SHORT const int stringLen )
	{
	STREAM stream;
	int LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES( isShortIntegerRangeNZ( stringLen ) );

	/* Scan the string making sure that there are no malformed characters */
	sMemConnect( &stream, string, stringLen );
	LOOP_LARGE_CHECK( stell( &stream ) < stringLen )
		{
		const long longStatus = getUTF8Char( &stream );
		if( cryptStatusError( longStatus ) )
			{
			sMemDisconnect( &stream );
			return( CRYPT_ERROR_BADDATA );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

/* Convert a UTF-8 string to ASCII, 8859-1, or Unicode, and vice versa */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
static int copyFromUtf8String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
							   IN_LENGTH_SHORT const int destMaxLen, 
							   OUT_LENGTH_BOUNDED_Z( destMaxLen ) \
									int *destLen, 
							   OUT_RANGE( 0, 20 ) int *destStringType,
							   IN_BUFFER( sourceLen ) const void *source, 
							   IN_LENGTH_SHORT const int sourceLen )
	{
	STREAM stream;
	ASN1_STRINGTYPE stringType = STRINGTYPE_PRINTABLE;
	wchar_t *wcDestPtr = dest;
	BYTE *destPtr = dest;
	int noChars = 0, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( isWritePtr( destStringType, sizeof( int ) ) );
	assert( isReadPtrDynamic( source, sourceLen ) );

	REQUIRES( isShortIntegerRangeNZ( destMaxLen ) );
	REQUIRES( isShortIntegerRangeNZ( sourceLen ) );

	/* Clear return value */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;
	*destStringType = STRINGTYPE_NONE;

	/* Scan the string to determine its length and the widest character type 
	   in it.  We have to process the entire string even if we've already 
	   identified it as containing the widest string type (Unicode) in order 
	   to check for malformed characters.  In addition we can't combine this 
	   with the copy loop that follows because until we get to the end we 
	   don't know whether we're copying 8-bit or wide characters */
	sMemConnect( &stream, source, sourceLen );
	LOOP_LARGE_CHECK( stell( &stream ) < sourceLen )
		{
		const long largeCh = getUTF8Char( &stream );
		if( cryptStatusError( largeCh ) )
			{
			sMemDisconnect( &stream );
			return( ( int ) largeCh );
			}
		noChars++;

		/* If we've already identified the widest character type then 
		   there's nothing more to do */
		if( stringType == STRINGTYPE_UNICODE )
			continue;

		/* If it's not an 8-bit value then it can only be Unicode */
		if( largeCh > 0xFF )
			{
			stringType = STRINGTYPE_UNICODE;
			continue;
			}

		/* If it's not a PrintableString character mark it as T61 if it's 
		   within range, otherwise it's Unicode */
		if( largeCh >= 128 )
			{
			stringType = ( asn1CharFlags[ largeCh & 0x7F ] & P ) ? \
						 STRINGTYPE_T61 : STRINGTYPE_UNICODE;
			}
		}
	ENSURES( LOOP_BOUND_OK );
	sMemDisconnect( &stream );

	/* Make sure that the translated string will fit into the destination 
	   buffer */
	*destLen = noChars * ( ( stringType == STRINGTYPE_UNICODE ) ? \
						   WCSIZE : 1 );
	if( *destLen < 0 || *destLen > destMaxLen )
		return( CRYPT_ERROR_OVERFLOW );
	*destStringType = stringType;

	/* Perform a second pass copying the string over */
	sMemConnect( &stream, source, sourceLen );
	LOOP_LARGE_CHECK( stell( &stream ) < sourceLen )
		{
		const long largeCh = getUTF8Char( &stream );

		ENSURES( !cryptStatusError( largeCh ) );

		/* Copy the result as a Unicode or ASCII/8859-1 character */
		if( stringType == STRINGTYPE_UNICODE )
			*wcDestPtr++ = ( wchar_t ) largeCh;
		else
			*destPtr++ = ( BYTE ) largeCh;
		}
	ENSURES( LOOP_BOUND_OK );
	sMemDisconnect( &stream );

	return( CRYPT_OK );
	}

#if 0	/* Currently unused, see note at start */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int copyToUtf8String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
							 IN_LENGTH_SHORT const int destMaxLen, 
							 OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen,
							 IN_BUFFER( sourceLen ) const void *source, 
							 IN_LENGTH_SHORT const int sourceLen,
							 const BOOLEAN isWideChar )
	{
	STREAM stream;
	int i, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( isReadPtrDynamic( source, sourceLen ) );

	REQUIRES( isShortIntegerRangeNZ( destMaxLen ) );
	REQUIRES( isShortIntegerRangeNZ( sourceLen ) );
	REQUIRES( isWideChar == TRUE || isWideChar == FALSE );

	/* Clear return value */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	sMemOpen( &stream, dest, destMaxLen );
	if( isWideChar )
		{
		const wchar_t *wcStrPtr = ( wchar_t * ) string;
		const int wcStrLen = stringLen / WCSIZE;

		REQUIRES( stringLen % WCSIZE == 0 );

		LOOP_LARGE( i = 0, i < wcStrLen, i++ )
			{
			if( wcStrPtr[ i ] < 0 || wcStrPtr[ i ] > 0xFFFF )
				{
				/* Safety check for WCSIZE > 2 */
				return( CRYPT_ERROR_BADDATA );
				}
			status = putUtf8Char( &stream, wcStrPtr[ i ] );
			if( cryptStatusError( status ) )
				break;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	else
		{
		LOOP_LARGE( i = 0, i < strinLen, i++ )
			{
			status = putUtf8Char( &stream, string[ i ] );
			if( cryptStatusError( status ) )
				break;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	if( cryptStatusOK( status ) )
		*destLen = stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*					String-type Identification Functions					*
*																			*
****************************************************************************/

/* Try and guess whether a native string is a widechar string */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isNativeWidecharString( IN_BUFFER( stringLen ) const BYTE *string, 
									   IN_LENGTH_SHORT_MIN( 2 ) const int stringLen )
	{
	wchar_t wCh;
#if WCHAR_MAX <= 0xFFFF
	int hiByte = 0, i, LOOP_ITERATOR;
#endif /* 16-bit wchar_t */

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES_B( stringLen >= WCSIZE && stringLen < MAX_INTLENGTH_SHORT );
	REQUIRES_B( !( stringLen % WCSIZE ) );

	/* Look at the first character in the string */
	wCh = getNativeWidechar( string );

	/* If wchar_t is > 16 bits and the bits above 16 are set or all zero,
	   it's either definitely not Unicode or Unicode.  Note that some
	   compilers will complain of unreachable code here, unfortunately we
	   can't easily fix this since WCSIZE is usually an expression involving
	   sizeof() which can't be handled via the preprocessor so we have to
	   guard it with a preprocessor check */
#if WCHAR_MAX > 0xFFFF
	return( ( wCh > 0xFFFF ) ? FALSE : TRUE );
#else
	/* wchar_t is 16 bits, make sure that we don't get false positives with 
	   short strings.  Two-character strings are more likely to be ASCII 
	   than a single widechar, and repeated alternate characters (e.g. 
	   "tanaka") in an ASCII string appear to be widechars for the general-
	   purpose check below so we check for these in strings of 2-3 wide 
	   characters before we perform the general-purpose check */
	if( stringLen <= ( WCSIZE * 3 ) && wCh > 0xFF )
		{
		if( stringLen == WCSIZE )
			{
			const int ch1 = string[ 0 ];
			const int ch2 = string[ 1 ];

			/* Check for a two-character ASCII string, usually a country 
			   name */
			if( isValidTextChar( ch1 ) && isValidTextChar( ch2 ) )
				return( FALSE );
			}
		else
			{
			int hi1, hi2, hi3;

			ENSURES_B( stringLen == ( WCSIZE * 2 ) || \
					   stringLen == ( WCSIZE * 3 ) );

			/* Check for alternate characters being ASCII */
			hi1 = hi3 = byteToInt( wCh >> 8 );
			hi2 = byteToInt( getNativeWidechar( string + WCSIZE ) >> 8 );
			if( stringLen > WCSIZE * 2 )
				hi3 = byteToInt( getNativeWidechar( string + ( WCSIZE * 2 ) ) >> 8 );
			if( isAlnum( hi1 ) && isAlnum( hi2 ) && isAlnum( hi3 ) && \
				hi1 == hi2 && hi2 == hi3 )
				return( FALSE );
			}
		}

	/* wchar_t is 16 bits, check whether it's in the form { 00 xx }* or
	   { AA|00 xx }*, either ASCII-as-Unicode or Unicode.  The code used 
	   below is safe because to get to this point the string has to be some 
	   multiple of 2 bytes long.  Note that if someone passes in a 1-byte 
	   string and mistakenly includes the terminator in the length it'll be 
	   identified as a 16-bit widechar string but this doesn't really matter 
	   since it'll get "converted" into a non-widechar string later */
	LOOP_LARGE( i = 0, i < stringLen, i += WCSIZE )
		{
		wCh = getNativeWidechar( &string[ i ] );
		if( wCh > 0xFF )
			{
			const int wChHi = byteToInt( wCh >> 8 );

			ENSURES_B( wChHi );

			/* If we haven't already seen a high byte, remember it */
			if( hiByte == 0 )
				hiByte = wChHi;
			else
				{
				/* If the current high byte doesn't match the previous one,
				   it's probably 8-bit characters */
				if( wChHi != hiByte )
					return( FALSE );
				}
			}
		}
	ENSURES_B( LOOP_BOUND_OK );

	return( TRUE );				/* Probably 16-bit characters */
#endif /* 32- vs 16-bit wchar_t */ 
	}

/* Try and figure out the true string type for an ASN.1-encoded or native 
   string.  This detects (or at least tries to detect) not only the basic 
   string type but also basic string types encoded as widechar strings and 
   widechar strings encoded as basic string types.

   All of these functions work by checking for the most restrictive ASN.1
   string subset that'll still contain the string, progressively widening
   from PrintableString -> IA5String -> T61(8859-1)String -> 
   BMP(Unicode)String, reporting an error if even a Unicode string can't
   contain the value.  Note that we continue processing the string even
   when we've reached the least-constraining value (Unicode) because we
   still need to check whether all of the characters in the string are
   actually valid before we exit */

CHECK_RETVAL_ENUM( STRINGTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STRINGTYPE get8bitStringType( IN_BUFFER( stringLen ) \
												const BYTE *string,
										  IN_LENGTH_SHORT const int stringLen,
										  IN_TAG_ENCODED const int stringTag )
	{
	BOOLEAN isIA5 = FALSE, isT61 = FALSE;
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES_EXT( isShortIntegerRangeNZ( stringLen ), \
				  STRINGTYPE_ERROR );
	REQUIRES_EXT( ( stringTag >= BER_STRING_UTF8 && \
					stringTag < BER_STRING_BMP ), 
				  STRINGTYPE_ERROR );

	/* Walk down the string checking each character */
	LOOP_LARGE( i = 0, i < stringLen, i++ )
		{
		const BYTE ch = string[ i ];

		/* If the high bit is set then it's not an ASCII subset */
		if( ch >= 128 )
			{
			isT61 = TRUE;
			if( asn1CharFlags[ ch & 0x7F ] )
				continue;
			}
		else
			{
			/* Check whether it's a PrintableString */
			if( !( asn1CharFlags[ ch ] & P ) )
				isIA5 = TRUE;

			/* Check whether it's something peculiar */
			if( asn1CharFlags[ ch ] )
				continue;
			}

		/* It's not 8859-1 either, check whether it's UTF-8 if this is 
		   permitted.  We can safely do an early-out in this case because 
		   checkUtf8String() checks the entire string */
		if( stringTag == BER_STRING_UTF8 && \
			cryptStatusOK( checkUtf8String( string, stringLen ) ) )
			return( STRINGTYPE_UTF8 );

		/* This may be some native 8-bit string type that can be converted 
		   to Unicode, tell the caller to try this before reporting an 
		   error */
		return( STRINGTYPE_ERROR_TRY_UNICODE );
		}
	ENSURES_EXT( LOOP_BOUND_OK, STRINGTYPE_ERROR );

	return( isT61 ? STRINGTYPE_T61 : \
			isIA5 ? STRINGTYPE_IA5 : \
					STRINGTYPE_PRINTABLE );
	}

CHECK_RETVAL_ENUM( STRINGTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STRINGTYPE getAsn1StringType( IN_BUFFER( stringLen ) \
											const BYTE *string, 
										  IN_LENGTH_SHORT const int stringLen, 
										  IN_TAG_ENCODED const int stringTag )
	{
	STREAM stream;
	BOOLEAN isIA5 = FALSE, isT61 = FALSE, isUnicode = FALSE;
	int length, status, LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES_EXT( isShortIntegerRangeNZ( stringLen ), \
				  STRINGTYPE_ERROR );
	REQUIRES_EXT( ( stringTag >= BER_STRING_UTF8 && \
					stringTag <= BER_STRING_BMP ), 
				  STRINGTYPE_ERROR );

	/* If it's not a Unicode string, determine the 8-bit string type */
	if( stringTag != BER_STRING_BMP )
		{
		const ASN1_STRINGTYPE stringType = \
						get8bitStringType( string, stringLen, stringTag );

		return( isErrorStringType( stringType ) ? \
				STRINGTYPE_ERROR : stringType );
		}

	/* If it's not a multiple of bmpchar_t in size then it can't really be 
	   Unicode either */
	if( stringLen % UCSIZE )
		return( STRINGTYPE_ERROR );

	/* If the first character isn't a null then it's definitely a Unicode
	   string.  These strings are always big-endian, even coming from 
	   Microsoft software, so we don't have to check for a null as the 
	   second character */
	if( string[ 0 ] != '\0' )
		return( STRINGTYPE_UNICODE );

	/* Check whether it's an 8-bit string encoded as a BMPString */
	sMemConnect( &stream, string, stringLen );
	LOOP_LARGE( length = 0, length < stringLen, length += UCSIZE )
		{
		int ch;
		
		status = ch = readUint16( &stream );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( STRINGTYPE_ERROR );
			}

		/* If it's not an 8-bit value then it's a pure Unicode string */
		if( ch > 0xFF )
			{
			isUnicode = TRUE;
			continue;
			}

		/* If the high bit is set then it's not an ASCII subset */
		if( ch >= 0x80 )
			{
			isT61 = TRUE;
			continue;
			}

		/* Check whether it's a PrintableString */
		if( !( asn1CharFlags[ ch ] & P ) )
			isIA5 = TRUE;
		}
	ENSURES_EXT( LOOP_BOUND_OK, STRINGTYPE_ERROR );
	sMemDisconnect( &stream );

	return( isUnicode ? STRINGTYPE_UNICODE : \
			isT61 ? STRINGTYPE_UNICODE_T61 : \
			isIA5 ? STRINGTYPE_UNICODE_IA5 : \
					STRINGTYPE_UNICODE_PRINTABLE );
	}

CHECK_RETVAL_ENUM( STRINGTYPE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STRINGTYPE getNativeStringType( IN_BUFFER( stringLen ) \
												const BYTE *string, 
											IN_LENGTH_SHORT const int stringLen )
	{
	BOOLEAN isIA5 = FALSE, isT61 = FALSE, isUnicode = FALSE;
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES_EXT( isShortIntegerRangeNZ( stringLen ), \
				  STRINGTYPE_ERROR );

	/* If it's not a widechar string, handle it as a basic 8-bit string 
	   type.  By default we pass in 8859-1 as the most permissible allowed 
	   string type, making it UTF-8 is rather risky because we don't know 
	   whether the environment really does support this natively or whether 
	   we've been passed garbage, so we only enable it for Unix systems, 
	   which are likely to be using UTF8 */
	if( stringLen < WCSIZE || ( stringLen % WCSIZE ) != 0 || \
		!isNativeWidecharString( string, stringLen ) )
		{
#if defined( __UNIX__ )
		return( get8bitStringType( string, stringLen, BER_STRING_UTF8 ) );
#else
		return( get8bitStringType( string, stringLen, BER_STRING_T61 ) );
#endif /* __UNIX__ */
		}

	/* It's a widechar string, although it may actually be something else 
	   that's been bloated out into widechars so we check for this as well */
	LOOP_LARGE( i = 0, i < stringLen, i += WCSIZE )
		{
		const wchar_t wCh = getNativeWidechar( &string[ i ] );

		/* Make sure that we've got a character from a Unicode (BMP) string.  
		   This check can be triggered if WCSIZE > 2 and the caller feeds us 
		   a binary garbage string so that the resulting decoded widechar 
		   value is larger than 16 bits */
		if( wCh < 0 || ( wCh & 0xFFFF8000L ) )
			return( STRINGTYPE_ERROR );

		/* If the high bit is set then it's not an ASCII subset */
		if( wCh >= 0x80 )
			{
			/* If it's larger than 8 bits then it's definitely Unicode */
			if( wCh >= 0xFF )
				{
				isUnicode = TRUE;
				continue;
				}

			/* Check which (if any) 8-bit type it could be */
			isT61 = TRUE;
			if( nativeCharFlags[ wCh & 0x7F ] )
				continue;

			/* It's not 8859-1 either, or more specifically it's something 
			   that ends up looking like a control character.  What to do at 
			   this point is a bit uncertain but making it a generic Unicode 
			   string which'll be handled via somewhat more robust 
			   mechanisms than something presumed to be ASCII or close to it 
			   is probably safer than storing it as a what's probably a 
			   control character */
			isUnicode = TRUE;

			continue;
			}

		/* Check whether it's a PrintableString */
		if( !( nativeCharFlags[ wCh ] & P ) )
			isIA5 = TRUE;
		}
	ENSURES_EXT( LOOP_BOUND_OK, STRINGTYPE_ERROR );

	return( isUnicode ? STRINGTYPE_UNICODE : \
			isT61 ? STRINGTYPE_UNICODE_T61 : \
			isIA5 ? STRINGTYPE_UNICODE_IA5 : \
					STRINGTYPE_UNICODE_PRINTABLE );
	}

/****************************************************************************
*																			*
*						ASN.1 String Conversion Functions					*
*																			*
****************************************************************************/

/* Check that a text string contains valid characters for its string type.
   This is used in non-DN strings where we can't vary the string type based 
   on the characters being used */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkTextStringData( IN_BUFFER( stringLen ) const char *string, 
							 IN_LENGTH_SHORT const int stringLen,
							 const BOOLEAN isPrintableString )
	{
	const int charTypeMask = isPrintableString ? P : I;
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( string, stringLen ) );

	REQUIRES_B( isShortIntegerRangeNZ( stringLen ) );
	REQUIRES( isPrintableString == TRUE || isPrintableString == FALSE );

	LOOP_LARGE( i = 0, i < stringLen, i++ )
		{
		const int ch = byteToInt( string[ i ] );

		if( !isValidTextChar( ch ) )
			return( FALSE );
		if( !( nativeCharFlags[ ch ] & charTypeMask ) )
			return( FALSE );
		}
	ENSURES_B( LOOP_BOUND_OK );

	return( TRUE );
	}

/* Convert a character string from the format used in certificates into the 
   native format */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
int copyFromAsn1String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
						IN_LENGTH_SHORT const int destMaxLen, 
						OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen, 
						OUT_RANGE( 0, 20 ) int *destStringType,
						IN_BUFFER( sourceLen ) const void *source, 
						IN_LENGTH_SHORT const int sourceLen,
						IN_TAG_ENCODED const int stringTag )
	{
	STREAM stream;
	const ASN1_STRINGTYPE stringType = \
					getAsn1StringType( source, sourceLen, stringTag );
	int status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( isWritePtr( destStringType, sizeof( int ) ) );
	assert( isReadPtrDynamic( source, sourceLen ) );

	REQUIRES( isShortIntegerRangeNZ( destMaxLen ) );
	REQUIRES( isShortIntegerRangeNZ( sourceLen ) );
	REQUIRES( stringTag >= BER_STRING_UTF8 && stringTag <= BER_STRING_BMP );

	/* Clear return values */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;
	*destStringType = STRINGTYPE_NONE;

	/* Make sure that the string type can be determined */
	if( stringType <= STRINGTYPE_NONE || stringType >= STRINGTYPE_ERROR )
		return( CRYPT_ERROR_BADDATA );

	/* If it's a BMP or UTF-8 string convert it to the native format */
	if( stringType == STRINGTYPE_UNICODE )
		{
		wchar_t *wcDestPtr = ( wchar_t * ) dest;
		const int newLen = ( sourceLen / UCSIZE ) * WCSIZE;

		if( newLen <= 0 || newLen > destMaxLen )
			return( CRYPT_ERROR_OVERFLOW );
		
		sMemConnect( &stream, source, sourceLen );
		LOOP_LARGE_CHECK( stell( &stream ) < sourceLen )
			{
			int wCh;
			
			status = wCh = readUint16( &stream );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			*wcDestPtr++ = ( wchar_t ) wCh;
			}
		ENSURES( LOOP_BOUND_OK );
		sMemDisconnect( &stream );
		*destLen = newLen;
		*destStringType = STRINGTYPE_UNICODE;

		return( CRYPT_OK );
		}
	if( stringTag == BER_STRING_UTF8 )
		{
		return( copyFromUtf8String( dest, destMaxLen, destLen, 
									destStringType, source, sourceLen ) );
		}

	/* If it's something masquerading as Unicode, convert it to the narrower 
	   format */
	if( stringType == STRINGTYPE_UNICODE_PRINTABLE || \
		stringType == STRINGTYPE_UNICODE_IA5 || \
		stringType == STRINGTYPE_UNICODE_T61 )
		{
		BYTE *destPtr = dest;
		const int newLen = sourceLen / UCSIZE;

		if( newLen <= 0 || newLen > destMaxLen )
			return( CRYPT_ERROR_OVERFLOW );

		sMemConnect( &stream, source, sourceLen );
		LOOP_LARGE_CHECK( stell( &stream ) < sourceLen )
			{
			int wCh;
			
			status = wCh = readUint16( &stream );
			if( !cryptStatusError( status ) && ( wCh > 0xFF ) )
				{
				/* Should never happen because of the pre-scan that we've 
				   done earlier */
				status = CRYPT_ERROR_BADDATA;
				}
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( status );
				}
			*destPtr++ = intToByte( wCh );
			}
		ENSURES( LOOP_BOUND_OK );
		sMemDisconnect( &stream );
		*destLen = newLen;
		*destStringType = ( stringType == STRINGTYPE_UNICODE_PRINTABLE ) ? \
							STRINGTYPE_PRINTABLE : \
						  ( stringType == STRINGTYPE_UNICODE_IA5 ) ? \
							STRINGTYPE_IA5 : STRINGTYPE_T61;

		return( CRYPT_OK );
		}

	/* It's an 8-bit character set, just copy it across */
	if( sourceLen <= 0 || sourceLen > destMaxLen )
		return( CRYPT_ERROR_OVERFLOW );
	REQUIRES( rangeCheck( sourceLen, 1, destMaxLen ) );
	memcpy( dest, source, sourceLen );
	*destLen = sourceLen;
	*destStringType = stringType;

	return( CRYPT_OK );
	}

/* Convert a character string from the native format to the format used in 
   certificates.  The 'stringType' value doesn't have any meaning outside
   this module, it's merely used as a cookie to pass back to other functions 
   here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
int getAsn1StringInfo( IN_BUFFER( stringLen ) const void *string, 
					   IN_LENGTH_SHORT const int stringLen,
					   OUT_RANGE( 0, 20 ) int *stringType, 
					   OUT_TAG_ENCODED_Z int *asn1StringType,
					   OUT_LENGTH_SHORT_Z int *asn1StringLen,
					   const BOOLEAN isNativeString )
	{
	const ASN1_STRINGTYPE localStringType = \
							getNativeStringType( string, stringLen );

	assert( isReadPtrDynamic( string, stringLen ) );
	assert( isWritePtr( stringType, sizeof( int ) ) );
	assert( isWritePtr( asn1StringType, sizeof( int ) ) );
	assert( isWritePtr( asn1StringLen, sizeof( int ) ) );

	REQUIRES( isShortIntegerRangeNZ( stringLen ) );
	REQUIRES( isNativeString == TRUE || isNativeString == FALSE );

	/* Clear return value */
	*asn1StringLen = 0;
	*asn1StringType = 0;

	/* Make sure that the string type can be determined */
	if( localStringType <= STRINGTYPE_NONE || \
		localStringType >= STRINGTYPE_ERROR )
		{
		size_t mbStringLen;
		int result;

		/* If it's not a native string or we're not being told to try re-
		   encoding as Unicode, report an error */
		if( !isNativeString || \
			localStringType != STRINGTYPE_ERROR_TRY_UNICODE )
			return( CRYPT_ERROR_BADDATA );
		
		/* This may be some oddball 8-bit local character type that can be 
		   converted into Unicode, check whether this is the case */
		result = mbstowcs_s( &mbStringLen, NULL, 0, string, stringLen );
		if( result != 0 || mbStringLen <= 0 || mbStringLen > stringLen )
			return( CRYPT_ERROR_BADDATA );

		/* It's a valid native 8-bit character string, report it as 
		   something that needs to be converted to Unicode */
		*stringType = STRINGTYPE_TO_UNICODE;
		*asn1StringLen = mbStringLen * UCSIZE;
		*asn1StringType = BER_STRING_BMP;

		return( CRYPT_OK );
		}

	*stringType = localStringType;
	switch( localStringType )
		{
		case STRINGTYPE_UNICODE:
			/* It's a native widechar string, the output is Unicode 
			   (BMPString) */
			*asn1StringLen = ( stringLen / WCSIZE ) * UCSIZE;
			*asn1StringType = BER_STRING_BMP;
			return( CRYPT_OK );

		case STRINGTYPE_UNICODE_PRINTABLE:
		case STRINGTYPE_UNICODE_IA5:
		case STRINGTYPE_UNICODE_T61:
			/* It's an ASCII string masquerading as a native widechar 
			   string, output is an 8-bit string type */
			*asn1StringLen = stringLen / WCSIZE;
			*asn1StringType = \
					( localStringType == STRINGTYPE_UNICODE_PRINTABLE ) ? \
						BER_STRING_PRINTABLE : \
					( localStringType == STRINGTYPE_UNICODE_IA5 ) ? \
						BER_STRING_IA5 : BER_STRING_T61;
			return( CRYPT_OK );

		case STRINGTYPE_UTF8:
#if 0
			/* It's a native widechar string encoded as UTF-8, the output is 
			   a variable-length UTF-8 string */
			status = utf8TargetStringLen( string, stringLen, asn1StringLen,
						( localStringType == STRINGTYPE_UNICODE || \
						  localStringType == STRINGTYPE_UNICODE_PRINTABLE || \
						  localStringType == STRINGTYPE_UNICODE_IA5 || \
						  localStringType == STRINGTYPE_UNICODE_T61 ) ? \
						TRUE : FALSE );
			if( cryptStatusError( status ) )
				return( status );
#else
			/* It's an already-encoded UTF8 string (getNativeStringType() 
			   has already checked its validity) */
			*asn1StringLen = stringLen;
#endif /* 0 */
			*asn1StringType = BER_STRING_UTF8;
			return( CRYPT_OK );
		}

	/* If nothing specialised matches then the default string type is an 
	   ASCII or T61/latin1 string */
	*asn1StringLen = stringLen;
	*asn1StringType = ( localStringType == STRINGTYPE_PRINTABLE ) ? \
						BER_STRING_PRINTABLE : \
					  ( localStringType == STRINGTYPE_IA5 ) ? \
						BER_STRING_IA5 : BER_STRING_T61;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int copyToAsn1String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
					  IN_LENGTH_SHORT const int destMaxLen, 
					  OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen, 
					  IN_BUFFER( sourceLen ) const void *source, 
					  IN_LENGTH_SHORT const int sourceLen,
					  IN_RANGE( 1, 20 ) const int stringType )
	{
	STREAM stream;
	const BYTE *srcPtr = source;
	const BOOLEAN unicodeTarget = ( stringType == STRINGTYPE_UNICODE ) ? \
								  TRUE : FALSE;
	int i, status, LOOP_ITERATOR;

	assert( isWritePtrDynamic( dest, destMaxLen ) );
	assert( isWritePtr( destLen, sizeof( int ) ) );
	assert( isReadPtrDynamic( source, sourceLen ) );

	REQUIRES( isShortIntegerRangeNZ( destMaxLen ) );
	REQUIRES( isShortIntegerRangeNZ( sourceLen ) );
	REQUIRES( isEnumRange( stringType, STRINGTYPE ) && \
			  stringType != STRINGTYPE_ERROR );

	/* Clear return value */
	memset( dest, 0, min( 16, destMaxLen ) );
	*destLen = 0;

	/* If it's a non-widechar string then we can just copy it across 
	   directly */
	if( stringType != STRINGTYPE_UNICODE && \
		stringType != STRINGTYPE_UNICODE_PRINTABLE && \
		stringType != STRINGTYPE_UNICODE_IA5 && \
		stringType != STRINGTYPE_UNICODE_T61 && \
		stringType != STRINGTYPE_TO_UNICODE )
		{
		if( sourceLen > destMaxLen )
			return( CRYPT_ERROR_OVERFLOW );
		REQUIRES( rangeCheck( sourceLen, 1, destMaxLen ) );
		memcpy( dest, source, sourceLen );
		*destLen = sourceLen;

		return( CRYPT_OK );
		}

	/* If it's some oddball native 8-bit string type, convert it to 
	   Unicode */
	if( stringType == STRINGTYPE_TO_UNICODE )
		{
		mbstate_t mbState;
		int count = 0;

		memset( &mbState, 0, sizeof( mbstate_t  ) );
  
		/* Copy the characters across, converting to bmpchar_t as we go.
		   In commemoration of classic non-thread-safe functions like
		   strtok(), the C99 standards committee also made the standard 
		   mbtowc() non-thread-safe by allowing it to be called with a 
		   null second argument to initialise the internal shift state for 
		   state-dependent encodings, we use mbrtowc() to deal with this */
		sMemOpen( &stream, dest, destMaxLen );
		LOOP_LARGE( ( status = CRYPT_OK, i = 0 ),
					cryptStatusOK( status ) && i < sourceLen,
					i += count )
			{
			wchar_t wCh;

			count = mbrtowc( &wCh, srcPtr + i, sourceLen - i, &mbState );
			if( count <= 0 )
				status = CRYPT_ERROR_BADDATA;
			else
				status = writeUint16( &stream, wCh );
			}
		ENSURES( LOOP_BOUND_OK );
		if( cryptStatusOK( status ) )
			*destLen = stell( &stream );
		sMemDisconnect( &stream );

		return( status );
		}

	/* It's a native widechar string, copy it across converting from wchar_t 
	   to bmpchar_t/char as required */
	sMemOpen( &stream, dest, destMaxLen );
	LOOP_LARGE( ( status = CRYPT_OK, i = 0 ),
				cryptStatusOK( status ) && i < sourceLen,
				i += WCSIZE )
		{
		const wchar_t wCh = getNativeWidechar( &srcPtr[ i ] );
		if( unicodeTarget )
			status = writeUint16( &stream, wCh );
		else
			status = sputc( &stream, intToByte( wCh ) );
		}
	ENSURES( LOOP_BOUND_OK );
	if( cryptStatusOK( status ) )
		*destLen = stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}
#endif /* USE_CERTIFICATES */
