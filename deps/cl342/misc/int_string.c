/****************************************************************************
*																			*
*						  cryptlib Internal String API						*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
#else
  #include "crypt.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						General-purpose String Functions					*
*																			*
****************************************************************************/

/* Perform various string-processing operations */

CHECK_RETVAL_STRINGOP STDC_NONNULL_ARG( ( 1 ) ) \
int strFindCh( IN_BUFFER( strLen ) const char *str, 
			   IN_LENGTH_SHORT const int strLen, 
			   IN_CHAR const int findCh )
	{
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );
	REQUIRES( findCh >= 0 && findCh <= 0x7F );

	for( i = 0; i < strLen; i++ )
		{
		if( str[ i ] == findCh )
			return( i );
		}

	return( -1 );
	}

CHECK_RETVAL_STRINGOP STDC_NONNULL_ARG( ( 1, 3 ) ) \
int strFindStr( IN_BUFFER( strLen ) const char *str, 
				IN_LENGTH_SHORT const int strLen, 
				IN_BUFFER( findStrLen ) const char *findStr, 
				IN_LENGTH_SHORT const int findStrLen )
	{
	const int findCh = toUpper( findStr[ 0 ] );
	int i;

	assert( isReadPtr( str, strLen ) );
	assert( isReadPtr( findStr, findStrLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );
	REQUIRES( findStrLen > 0 && findStrLen < MAX_INTLENGTH_SHORT );
	REQUIRES( findCh >= 0 && findCh <= 0x7F );

	for( i = 0; i <= strLen - findStrLen; i++ )
		{
		if( toUpper( str[ i ] ) == findCh && \
			!strCompare( str + i, findStr, findStrLen ) )
			return( i );
		}

	return( -1 );
	}

CHECK_RETVAL_STRINGOP STDC_NONNULL_ARG( ( 1 ) ) \
int strSkipWhitespace( IN_BUFFER( strLen ) const char *str, 
					   IN_LENGTH_SHORT const int strLen )
	{
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	for( i = 0; i < strLen && ( str[ i ] == ' ' || str[ i ] == '\t' ); i++ );
	return( ( i < strLen ) ? i : -1 );
	}

CHECK_RETVAL_STRINGOP STDC_NONNULL_ARG( ( 1 ) ) \
int strSkipNonWhitespace( IN_BUFFER( strLen ) const char *str, 
						  IN_LENGTH_SHORT const int strLen )
	{
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	/* This differs slightly from strSkipWhitespace() in that EOL is also 
	   counted as whitespace so there's never an error condition unless
	   we don't find anything at all */
	for( i = 0; i < strLen && str[ i ] != ' ' && str[ i ] != '\t'; i++ );
	return( i > 0 ? i : -1 );
	}

CHECK_RETVAL_STRINGOP STDC_NONNULL_ARG( ( 1, 2 ) ) \
int strStripWhitespace( OUT_PTR_COND const char **newStringPtr, 
						IN_BUFFER( strLen ) const char *string, 
						IN_LENGTH_SHORT const int strLen )
	{
	int startPos, endPos;

	assert( isReadPtr( newStringPtr, sizeof( char * ) ) );
	assert( isReadPtr( string, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*newStringPtr = NULL;

	/* Skip leading and trailing whitespace */
	for( startPos = 0;
		 startPos < strLen && \
			( string[ startPos ] == ' ' || string[ startPos ] == '\t' );
		 startPos++ );
	if( startPos >= strLen )
		return( -1 );
	*newStringPtr = string + startPos;
	for( endPos = strLen;
		 endPos > startPos && \
			( string[ endPos - 1 ] == ' ' || string[ endPos - 1 ] == '\t' );
		 endPos-- );
	ENSURES( endPos - startPos > 0 );
	return( endPos - startPos );
	}

/****************************************************************************
*																			*
*						Special-purpose String Functions					*
*																			*
****************************************************************************/

/* Extract a substring from a string.  This converts:

	 string				startOffset							 strLen
		|					|									|
		v					v									v
		+-------------------+---------------+-------------------+
		|	Processed data	|	Whitespace	|	Remaining data	|
		+-------------------+---------------+-------------------+

   into:

	 newStr				 length
		|					|
		v					v
		+-------------------+
		|	Remaining data	|
		+-------------------+ 

   The order of the parameters is a bit unusual, normally we'd use 
   { str, strLen } but this makes things a bit confusing for the caller, for
   whom it's more logical to group the parameters based on the overall
   operation being performed, which to extract a substring beginning at
   startOffset is { str, startOffset, strLen } */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int strExtract( OUT_PTR_COND const char **newStringPtr, 
				IN_BUFFER( strLen ) const char *string,
				IN_LENGTH_SHORT_Z const int startOffset,
				IN_LENGTH_SHORT const int strLen )
	{
	const int newLen = strLen - startOffset;

	assert( isReadPtr( newStringPtr, sizeof( char * ) ) );
	assert( isReadPtr( string, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );
	REQUIRES( startOffset >= 0 && startOffset <= strLen && \
			  startOffset < MAX_INTLENGTH_SHORT );
			  /* May be zero if we're extracting from the start of the 
			     string; may be equal to strLen if it's the entire
				 remaining string */

	/* Clear return value */
	*newStringPtr = NULL;

	if( newLen < 1 || newLen > strLen || newLen >= MAX_INTLENGTH_SHORT )
		return( -1 );
	return( strStripWhitespace( newStringPtr, string + startOffset, newLen ) );
	}

/* Parse a numeric or hex string into an integer value.  Safe conversion of a 
   numeric string gets a bit problematic because atoi() can't really 
   indicate an error except by returning 0, which is indistinguishable from 
   a zero numeric value.  To handle this we have to perform the conversion 
   ourselves */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int strGetNumeric( IN_BUFFER( strLen ) const char *str, 
				   IN_LENGTH_SHORT const int strLen, 
				   OUT_INT_Z int *numericValue, 
				   IN_RANGE( 0, 100 ) const int minValue, 
				   IN_RANGE( minValue, MAX_INTLENGTH ) const int maxValue )
	{
	int i, value;

	assert( isReadPtr( str, strLen ) );
	assert( isWritePtr( numericValue, sizeof( int ) ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );
	REQUIRES( minValue >= 0 && minValue < maxValue && \
			  maxValue <= MAX_INTLENGTH );

	/* Clear return value */
	*numericValue = 0;

	/* Make sure that the value is within the range 'n' ... 'nnnnnnn' */
	if( strLen < 1 || strLen > 7 )
		return( CRYPT_ERROR_BADDATA );

	/* Process the numeric string.  Note that the redundant 'value < 0' 
	   check is necessary in order to prevent gcc from detecting what it 
	   thinks is Undefined Behaviour (UB) and removing further checks from 
	   the code.  In addition the second check for MAX_INTLENGTH - ch isn't
	   really necessary because we know that value < MAX_INTLENGTH / 10,
	   which means that value <= MAX_INTLENGTH / 10 - 1, so 
	   value * 10 <= MAX_INTLENGTH - 10, therefore 
	   value * 10 < MAX_INTLENGTH - 9, so value * 10 < MAX_INTLENGTH - ch,
	   however we leave it in to make the condition explicit */
	for( value = 0, i = 0; i < strLen; i++ )
		{
		const int ch = byteToInt( str[ i ] ) - '0';

		if( ch < 0 || ch > 9 )
			return( CRYPT_ERROR_BADDATA );
		if( value < 0 || value >= MAX_INTLENGTH / 10 )
			return( CRYPT_ERROR_BADDATA );
		value *= 10;
		if( value >= MAX_INTLENGTH - ch )
			return( CRYPT_ERROR_BADDATA );
		value += ch;
		ENSURES( value >= 0 && value < MAX_INTLENGTH );
		}

	/* Make sure that the final value is within the specified range */
	if( value < minValue || value > maxValue )
		return( CRYPT_ERROR_BADDATA );

	*numericValue = value;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int strGetHex( IN_BUFFER( strLen ) const char *str, 
			   IN_LENGTH_SHORT const int strLen, 
			   OUT_INT_Z int *numericValue, 
			   IN_RANGE( 0, 100 ) const int minValue, 
			   IN_RANGE( minValue, MAX_INTLENGTH ) const int maxValue )
	{
#ifdef SYSTEM_16BIT
	const int strMaxLen = ( maxValue > 0xFFF ) ? 4 : \
						  ( maxValue > 0xFF ) ? 3 : \
						  ( maxValue > 0xF ) ? 2 : 1;
#else
	const int strMaxLen = ( maxValue > 0xFFFF ) ? 5 : \
						  ( maxValue > 0xFFF ) ? 4 : \
						  ( maxValue > 0xFF ) ? 3 : \
						  ( maxValue > 0xF ) ? 2 : 1;
#endif /* SYSTEM_16BIT */
	int i, value = 0;

	assert( isReadPtr( str, strLen ) );
	assert( isWritePtr( numericValue, sizeof( int ) ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );
	REQUIRES( minValue >= 0 && minValue < maxValue && \
			  maxValue <= MAX_INTLENGTH );

	/* Clear return value */
	*numericValue = 0;

	/* Make sure that the length is sensible for the value that we're 
	   reading */
	if( strLen < 1 || strLen > strMaxLen )
		return( CRYPT_ERROR_BADDATA );

	/* Process the numeric string.  We don't have to perform the same level 
	   of overflow checking as we do in strGetNumeric() because the maximum
	   value is capped to fit into an int */
	for( i = 0; i < strLen; i++ )
		{
		const int ch = toLower( str[ i ] );
	
		if( !isXDigit( ch ) )
			return( CRYPT_ERROR_BADDATA );
		value = ( value << 4 ) | \
				( ( ch <= '9' ) ? ch - '0' : ch - ( 'a' - 10 ) );
		}
	if( value < minValue || value > maxValue )
		return( CRYPT_ERROR_BADDATA );

	*numericValue = value;

	return( CRYPT_OK );
	}

/* Determine whether a string is printable or not, used when checking whether
   it should be displayed to the caller as a text string or a hex dump */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN strIsPrintable( IN_BUFFER( strLen ) const void *str, 
						IN_LENGTH_SHORT const int strLen )
	{
	const BYTE *strPtr = str;
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	for( i = 0; i < strLen; i++ )
		{
		const int ch = byteToInt( strPtr[ i ] );

		if( !isValidTextChar( ch ) )
			return( FALSE );
		}

	return( TRUE );
	}

/* Sanitise a string before passing it back to the user.  This is used to
   clear potential problem characters (for example control characters)
   from strings passed back from untrusted sources (nec verbum verbo 
   curabis reddere fidus interpres - Horace).
   
   This function assumes that the string is in ASCII form rather than some
   exotic character set, since this is used for processing text error
   messages sent to us by remote systems we can reasonably assume that 
   they should be be, well, text error messages so that we're within our
   rights to filter out non-text data.
   
   The function returns a pointer to the string to allow it to be used in 
   the form printf( "..%s..", sanitiseString( string, strLen ) ).  In 
   addition it formats the data to fit a fixed-length buffer, if the string 
   is longer than the indicated buffer size then it appends a '[...]' at the 
   end of the buffer to indicate that further data was truncated.   The 
   transformation applied is as follows:

	  buffer					strMaxLen
		|							|
		v							v
		+---------------------------+		.								.
		|							|  ==>	.								.
		+---------------------------+		.								.
		.							.		.								.
		|---------------|			.		|---------------|\0|			.
		.				^			.		.								.
		|--------------------------------|	|-----------------------|[...]\0|
						|				 ^
						+---- strLen ----+

   so "Error string of arbitrary length..." with a buffer size of 20 would 
   become "Error string [...]" */

STDC_NONNULL_ARG( ( 1 ) ) \
char *sanitiseString( INOUT_BUFFER( strMaxLen, strLen ) void *string, 
					  IN_LENGTH_SHORT const int strMaxLen, 
					  IN_LENGTH_SHORT const int strLen )
	{
	BYTE *strPtr = string;	/* See comment below */
	const int strDataLen = min( strLen, strMaxLen );
	int i;

	assert( isWritePtr( string, strMaxLen ) );

	REQUIRES_EXT( ( strLen > 0 && strLen < MAX_INTLENGTH_SHORT ), \
				  "(Internal error)" );
	REQUIRES_EXT( ( strMaxLen > 0 && strMaxLen < MAX_INTLENGTH_SHORT ), \
				  "(Internal error)" );

	/* Remove any potentially unsafe characters from the string, effectively
	   converting it from a 'BYTE *' to a 'char *'.  This is also the reason
	   why the function prototype declares it as a 'void *', if it's declared
	   as a 'BYTE *' then the conversion process gives compilers and static
	   analysers headaches */
	for( i = 0; i < strDataLen; i++ )
		{
		const int ch = byteToInt( strPtr[ i ] );

		if( !isValidTextChar( ch ) )
			strPtr[ i ] = '.';
		}

	/* If there was more input than we could fit into the buffer and 
	   there's room for a continuation indicator, add this to the output 
	   string */
	if( ( strLen >= strMaxLen ) && ( strMaxLen > 8 ) )
		memcpy( strPtr + strMaxLen - 6, "[...]", 5 );	/* Extra -1 for '\0' */

	/* Terminate the string to allow it to be used in printf()-style
	   functions */
	if( strLen < strMaxLen )
		strPtr[ strLen ] = '\0';
	else
		strPtr[ strMaxLen - 1 ] = '\0';

	/* We've converted the string from BYTE * to char * so it can be 
	   returned as a standard text string */
	return( ( char * ) strPtr );
	}

/****************************************************************************
*																			*
*						TR 24731 Safe stdlib Extensions						*
*																			*
****************************************************************************/

#ifndef __STDC_LIB_EXT1__

/* Minimal wrappers for the TR 24731 functions to map them to older stdlib 
   equivalents.  Because of potential issues when comparing a (signed)
   literal value -1 to the unsigned size_t we explicitly check for both
   '( size_t ) -1' as well as a general check for a negative return value */

RETVAL_RANGE( -1, 0 ) \
int mbstowcs_s( OUT size_t *retval, 
				OUT_BUFFER_FIXED( dstmax ) wchar_t *dst, 
				IN_LENGTH_SHORT size_t dstmax, 
				IN_BUFFER( len ) const char *src, 
				IN_LENGTH_SHORT size_t len )
	{
	size_t bytesCopied;

	assert( isWritePtr( retval, sizeof( size_t ) ) );
	assert( isWritePtr( dst, dstmax ) );
	assert( isReadPtr( src, len ) );

	REQUIRES_EXT( ( dstmax > 0 && dstmax < MAX_INTLENGTH_SHORT ), -1 );
	REQUIRES_EXT( ( len > 0 && len <= dstmax && \
					len < MAX_INTLENGTH_SHORT ), -1 );

	/* Clear return value */
	*retval = 0;

	bytesCopied = mbstowcs( dst, src, len );
	if( ( bytesCopied == ( size_t ) -1 ) || ( bytesCopied <= 0 ) )
		return( -1 );
	*retval = bytesCopied;
	return( 0 );
	}

RETVAL_RANGE( -1, 0 ) \
int wcstombs_s( OUT size_t *retval, 
				OUT_BUFFER_FIXED( dstmax ) char *dst, 
				IN_LENGTH_SHORT size_t dstmax, 
				IN_BUFFER( len) const wchar_t *src, 
				IN_LENGTH_SHORT size_t len )
	{
	size_t bytesCopied;

	assert( isWritePtr( retval, sizeof( size_t ) ) );
	assert( isWritePtr( dst, dstmax ) );
	assert( isReadPtr( src, len ) );

	REQUIRES_EXT( ( dstmax > 0 && dstmax < MAX_INTLENGTH_SHORT ), -1 );
	REQUIRES_EXT( ( len > 0 && len <= dstmax && \
					len < MAX_INTLENGTH_SHORT ), -1 );

	/* Clear return value */
	*retval = 0;

	bytesCopied = wcstombs( dst, src, len );
	if( ( bytesCopied == ( size_t ) -1 ) || ( bytesCopied <= 0 ) )
		return( -1 );
	*retval = bytesCopied;
	return( 0 );
	}
#endif /* !__STDC_LIB_EXT1__ */

/****************************************************************************
*																			*
*								Self-test Functions							*
*																			*
****************************************************************************/

/* Test code for the above functions */

#ifndef NDEBUG

CHECK_RETVAL_BOOL \
BOOLEAN testIntString( void )
	{
	BYTE buffer[ 16 + 8 ];
	const char *stringPtr;
	int stringLen, value;

	/* Test strFindCh() */
	if( strFindCh( "abcdefgh", 8, 'a' ) != 0 || \
		strFindCh( "abcdefgh", 8, 'd' ) != 3 || \
		strFindCh( "abcdefgh", 8, 'h' ) != 7 || \
		strFindCh( "abcdefgh", 8, 'x' ) != -1 )
		return( FALSE );

	/* Test strFindStr() */
	if( strFindStr( "abcdefgh", 8, "abc", 3 ) != 0 || \
		strFindStr( "abcdefgh", 8, "fgh", 3 ) != 5 || \
		strFindStr( "abcdefgh", 8, "ghi", 3 ) != -1 || \
		strFindStr( "abcdefgh", 8, "abcdefghi", 9 ) != -1 )
		return( FALSE );

	/* Test strSkipWhitespace() */
	if( strSkipWhitespace( "abcdefgh", 8 ) != 0 || \
		strSkipWhitespace( " abcdefgh", 9 ) != 1 || \
		strSkipWhitespace( " \t abcdefgh", 11 ) != 3 || \
		strSkipWhitespace( " x abcdefgh", 11 ) != 1 || \
		strSkipWhitespace( "  \t ", 4 ) != -1 )
		return( FALSE );

	/* Test strSkipNonWhitespace() */
	if( strSkipNonWhitespace( "abcdefgh", 8 ) != 8 || \
		strSkipNonWhitespace( " abcdefgh", 9 ) != -1 || \
		strSkipNonWhitespace( "abcdefgh ", 9 ) != 8 || \
		strSkipNonWhitespace( "abcdefgh x ", 11 ) != 8 )
		return( FALSE );

	/* Test strStripWhitespace() */
	stringLen = strStripWhitespace( &stringPtr, "abcdefgh", 8 );
	if( stringLen != 8 || memcmp( stringPtr, "abcdefgh", 8 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, " abcdefgh", 9 );
	if( stringLen != 8 || memcmp( stringPtr, "abcdefgh", 8 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, "abcdefgh ", 9 );
	if( stringLen != 8 || memcmp( stringPtr, "abcdefgh", 8 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, " abcdefgh ", 10 );
	if( stringLen != 8 || memcmp( stringPtr, "abcdefgh", 8 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, " x abcdefgh ", 12 );
	if( stringLen != 10 || memcmp( stringPtr, "x abcdefgh", 10 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, " abcdefgh x ", 12 );
	if( stringLen != 10 || memcmp( stringPtr, "abcdefgh x", 10 ) )
		return( FALSE );
	stringLen = strStripWhitespace( &stringPtr, "  \t ", 4 );
	if( stringLen != -1 || stringPtr != NULL )
		return( FALSE );

	/* Test strExtract() */
	stringLen = strExtract( &stringPtr, "abcdefgh", 4, 8 );
	if( stringLen != 4 || memcmp( stringPtr, "efgh", 4 ) )
		return( FALSE );
	stringLen = strExtract( &stringPtr, "abcd  efgh", 4, 10 );
	if( stringLen != 4 || memcmp( stringPtr, "efgh", 4 ) )
		return( FALSE );
	stringLen = strExtract( &stringPtr, "abcd  efgh  ", 4, 12 );
	if( stringLen != 4 || memcmp( stringPtr, "efgh", 4 ) )
		return( FALSE );
	stringLen = strExtract( &stringPtr, "abcd  efgh  ij  ", 4, 16 );
	if( stringLen != 8 || memcmp( stringPtr, "efgh  ij", 8 ) )
		return( FALSE );


	/* Test strGetNumeric() */
	if( strGetNumeric( "0", 1, &value, 0, 10 ) != CRYPT_OK || value != 0 || \
		strGetNumeric( "00", 2, &value, 0, 10 ) != CRYPT_OK || value != 0 || \
		strGetNumeric( "1234", 4, &value, 0, 2000 ) != CRYPT_OK || value != 1234 || \
		strGetNumeric( "1234x", 5, &value, 0, 2000 ) != CRYPT_ERROR_BADDATA || value != 0 || \
		strGetNumeric( "x1234", 5, &value, 0, 2000 ) != CRYPT_ERROR_BADDATA || value != 0 || \
		strGetNumeric( "1000", 4, &value, 0, 1000 ) != CRYPT_OK || value != 1000 || \
		strGetNumeric( "1001", 4, &value, 0, 1000 ) != CRYPT_ERROR_BADDATA || value != 0 )
		return( FALSE );

	/* Test strGetHex() */
	if( strGetHex( "0", 1, &value, 0, 1000 ) != CRYPT_OK || value != 0 || \
		strGetHex( "1234", 4, &value, 0, 0x2000 ) != CRYPT_OK || value != 0x1234 || \
		strGetHex( "1234x", 5, &value, 0, 0x2000 ) != CRYPT_ERROR_BADDATA || value != 0 || \
		strGetHex( "x1234", 5, &value, 0, 0x2000 ) != CRYPT_ERROR_BADDATA || value != 0 || \
		strGetHex( "12EE", 4, &value, 0, 0x12EE ) != CRYPT_OK || value != 0x12EE || \
		strGetHex( "12EF", 4, &value, 0, 0x12EE ) != CRYPT_ERROR_BADDATA || value != 0 )
		return( FALSE );

	/* Test sanitiseString() */
	memcpy( buffer, "abcdefgh", 8 );
	stringPtr = sanitiseString( buffer, 16, 8 );
	if( memcmp( stringPtr, "abcdefgh", 9 ) )
		return( FALSE );
	memcpy( buffer, "abc\x12" "efgh", 8 );
	stringPtr = sanitiseString( buffer, 16, 8 );
	if( memcmp( stringPtr, "abc.efgh", 9 ) )
		return( FALSE );
	memcpy( buffer, "abcdefgh", 8 );
	stringPtr = sanitiseString( buffer, 7, 8 );
	if( memcmp( stringPtr, "abcdef", 7 ) )
		return( FALSE );
	memcpy( buffer, "abcdefgh", 8 );
	stringPtr = sanitiseString( buffer, 8, 8 );
	if( memcmp( stringPtr, "abcdefg", 8 ) )
		return( FALSE );
	memcpy( buffer, "abcdefghij", 10 );
	stringPtr = sanitiseString( buffer, 9, 10 );
	if( memcmp( stringPtr, "abc[...]", 9 ) )
		return( FALSE );
	memcpy( buffer, "abcdefghij", 10 );
	stringPtr = sanitiseString( buffer, 10, 10 );
	if( memcmp( stringPtr, "abcd[...]", 10 ) )
		return( FALSE );
	memcpy( buffer, "abcdefghij", 10 );
	stringPtr = sanitiseString( buffer, 11, 10 );
	if( memcmp( stringPtr, "abcdefghij", 11 ) )
		return( FALSE );

	return( TRUE );
	}
#endif /* !NDEBUG */
