/****************************************************************************
*																			*
*						  cryptlib Internal String API						*
*						Copyright Peter Gutmann 1992-2007					*
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

CHECK_RETVAL_STRINGOP( strLen ) STDC_NONNULL_ARG( ( 1 ) ) \
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

CHECK_RETVAL_STRINGOP( strLen ) STDC_NONNULL_ARG( ( 1, 3 ) ) \
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

CHECK_RETVAL_STRINGOP( strLen ) STDC_NONNULL_ARG( ( 1 ) ) \
int strSkipWhitespace( IN_BUFFER( strLen ) const char *str, 
					   IN_LENGTH_SHORT const int strLen )
	{
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	for( i = 0; i < strLen && ( str[ i ] == ' ' || str[ i ] == '\t' ); i++ );
	return( ( i < strLen ) ? i : -1 );
	}

CHECK_RETVAL_STRINGOP( strLen ) STDC_NONNULL_ARG( ( 1 ) ) \
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

CHECK_RETVAL_STRINGOP( strLen ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
int strStripWhitespace( OUT_OPT_PTR const char **newStringPtr, 
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
   operation beingn performed, which to extract a substring beginning at
   startOffset is { str, startOffset, strLen } */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int strExtract( OUT_OPT_PTR const char **newStringPtr, 
				IN_BUFFER( strLen ) const char *string,
				IN_LENGTH_SHORT const int startOffset,
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

	/* Process the numeric string */
	for( value = 0, i = 0; i < strLen; i++ )
		{
		const int valTmp = value * 10;
		const int ch = byteToInt( str[ i ] ) - '0';

		if( ch < 0 || ch > 9 )
			return( CRYPT_ERROR_BADDATA );
		if( value >= ( MAX_INTLENGTH / 10 ) || \
			valTmp >= MAX_INTLENGTH - ch )
			return( CRYPT_ERROR_BADDATA );
		value = valTmp + ch;
		if( value < 0 || value > MAX_INTLENGTH )
			return( CRYPT_ERROR_BADDATA );
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
	const int strMaxLen = ( maxValue > 0xFFFF ) ? 5 : \
						  ( maxValue > 0xFFF ) ? 4 : \
						  ( maxValue > 0xFF ) ? 3 : \
						  ( maxValue > 0xF ) ? 2 : 1;
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

	/* Process the numeric string */
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

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN strIsPrintable( IN_BUFFER( strLen ) const void *str, 
						IN_LENGTH_SHORT const int strLen )
	{
	const BYTE *strPtr = str;
	int i;

	assert( isReadPtr( str, strLen ) );

	REQUIRES( strLen > 0 && strLen < MAX_INTLENGTH_SHORT );

	for( i = 0; i < strLen; i++ )
		{
		if( !isPrint( strPtr[ i ] ) )
			return( FALSE );
		}

	return( TRUE );
	}

/* Sanitise a string before passing it back to the user.  This is used to
   clear potential problem characters (for example control characters)
   from strings passed back from untrusted sources (nec verbum verbo 
   curabis reddere fidus interpres - Horace).  The function returns a 
   pointer to the string to allow it to be used in the form 
   printf( "..%s..", sanitiseString( string, strLen ) ).  In addition it
   formats the data to fit a fixed-length buffer, if the string is longer 
   than the indicated buffer size then it appends a '[...]' at the end of 
   the buffer to indicate that further data was truncated.   The 
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
char *sanitiseString( INOUT_BUFFER( strMaxLen, strLen ) BYTE *string, 
					  IN_LENGTH_SHORT const int strMaxLen, 
					  IN_LENGTH_SHORT const int strLen )
	{
	const int strDataLen = min( strLen, strMaxLen );
	int i;

	assert( isWritePtr( string, strMaxLen ) );

	REQUIRES_EXT( ( strLen > 0 && strLen < MAX_INTLENGTH_SHORT ), \
				  "(Internal error)" );
	REQUIRES_EXT( ( strMaxLen > 0 && strMaxLen < MAX_INTLENGTH_SHORT ), \
				  "(Internal error)" );

	/* Remove any potentially unsafe characters from the string, effectively
	   converting it from a 'BYTE *' to a 'char *' */
	for( i = 0; i < strDataLen; i++ )
		{
		const int ch = byteToInt( string[ i ] );

		if( ch <= 0 || ch > 0x7F || !isPrint( ch ) )
			string[ i ] = '.';
		}

	/* If there was more input than we could fit into the buffer and 
	   there's room for a continuation indicator, add this to the output 
	   string */
	if( ( strLen > strMaxLen ) && ( strMaxLen > 8 ) )
		memcpy( string + strMaxLen - 6, "[...]", 5 );	/* Extra -1 for '\0' */

	/* Terminate the string to allow it to be used in printf()-style
	   functions */
	if( strLen < strMaxLen )
		string[ strLen ] = '\0';
	else
		string[ strMaxLen - 1 ] = '\0';

	/* We've converted the string from BYTE * to char * so it can be 
	   returned as a standard text string */
	return( ( char * ) string );
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
