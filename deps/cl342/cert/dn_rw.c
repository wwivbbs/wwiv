/****************************************************************************
*																			*
*						Certificate DN Read/Write Routines					*
*						Copyright Peter Gutmann 1996-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "dn.h"
#else
  #include "cert/cert.h"
  #include "cert/dn.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*									Read a DN								*
*																			*
****************************************************************************/

/* Parse an AVA.  This determines the AVA type and leaves the stream pointer
   at the start of the data value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readAVABitstring( INOUT STREAM *stream, 
							 OUT_LENGTH_SHORT_Z int *length, 
							 OUT_TAG_ENCODED_Z int *stringTag )
	{
	long streamPos;
	int bitStringLength, innerTag, innerLength DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( stringTag, sizeof( int ) ) );

	/* Bitstrings are used for uniqueIdentifiers, however these usually 
	   encapsulate something else:

		BIT STRING {
			IA5String 'xxxxx'
			}

	   so we try and dig one level deeper to find the encapsulated string if 
	   there is one.  This gets a bit complicated because we have to 
	   speculatively try and decode the inner content and if that fails 
	   assume that it's raw bitstring data.  First we read the bitstring 
	   wrapper and remember where the bitstring data starts */
	status = readBitStringHole( stream, &bitStringLength, 2, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	streamPos = stell( stream );

	/* Then we try and read any inner content */
	status = innerTag = peekTag( stream );
	if( !cryptStatusError( status ) )
		status = readGenericHole( stream, &innerLength, 1, innerTag );
	if( !cryptStatusError( status ) && \
		bitStringLength == sizeofObject( innerLength ) )
		{
		/* There was inner content present, treat it as the actual type and 
		   value of the bitstring.  This assumes that the inner content is
		   a string data type, which always seems to be the case (in any 
		   event it's not certain what we should be returning to the user if
		   we find, for example, a SEQUENCE with further encapsulated 
		   content at this point) */
		*stringTag = innerTag;
		*length = innerLength;

		return( CRYPT_OK );
		}

	/* We couldn't identify any (obvious) inner content, it must be raw
	   bitstring data.  Unfortunately we have no idea what format this is
	   in, it could in fact really be raw binary data but never actually 
	   seems to be this, it's usually ASCII text so we mark it as such and 
	   let the string-read routines sort it out */
	sClearError( stream );
	sseek( stream, streamPos );
	*stringTag = BER_STRING_IA5;
	*length = bitStringLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
static int readAVA( INOUT STREAM *stream, 
					OUT_INT_Z int *type, 
					OUT_LENGTH_SHORT_Z int *length, 
					OUT_TAG_ENCODED_Z int *stringTag )
	{
	const DN_COMPONENT_INFO *dnComponentInfo;
	BYTE oid[ MAX_OID_SIZE + 8 ];
	int oidLength, tag, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( type, sizeof( int ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );
	assert( isWritePtr( stringTag, sizeof( int ) ) );

	/* Clear return values */
	*type = 0;
	*length = 0;
	*stringTag = 0;

	/* Read the start of the AVA and determine the type from the 
	   AttributeType field.  If we find something that we don't recognise we 
	   indicate it as a non-component type that can be read or written but 
	   not directly accessed by the user (although it can still be accessed 
	   using the cursor functions) */
	readSequence( stream, NULL );
	status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
							 BER_OBJECT_IDENTIFIER );
	if( cryptStatusError( status ) )
		return( status );
	dnComponentInfo = findDNInfoByOID( oid, oidLength );
	if( dnComponentInfo == NULL )
		{
		/* If we don't recognise the component type at all, skip it */
		status = readUniversal( stream );
		return( cryptStatusError( status ) ? status : OK_SPECIAL );
		}
	*type = dnComponentInfo->type;

	/* We've reached the data value, make sure that it's in order.  When we
	   read the wrapper around the string type with readGenericHole() we have 
	   to allow a minimum length of zero instead of one because of broken 
	   AVAs with zero-length strings */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( tag == BER_BITSTRING )
		return( readAVABitstring( stream, length, stringTag ) );
	*stringTag = tag;
	return( readGenericHole( stream, length, 0, tag ) );
	}

/* Read an RDN/DN component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readRDNcomponent( INOUT STREAM *stream, 
							 INOUT DN_COMPONENT **dnComponentListPtrPtr,
							 IN_LENGTH_SHORT const int rdnDataLeft )
	{	
	CRYPT_ERRTYPE_TYPE dummy;
	BYTE stringBuffer[ MAX_ATTRIBUTE_SIZE + 8 ];
	void *value;
	const int rdnStart = stell( stream );
	int type, valueLength, valueStringType, stringTag;
	int flags = DN_FLAG_NOCHECK, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( dnComponentListPtrPtr, sizeof( DN_COMPONENT * ) ) );

	REQUIRES( rdnDataLeft > 0 && rdnDataLeft < MAX_INTLENGTH_SHORT );
	REQUIRES( rdnStart > 0 && rdnStart < MAX_INTLENGTH_SHORT );

	/* Read the type information for this AVA */
	status = readAVA( stream, &type, &valueLength, &stringTag );
	if( cryptStatusError( status ) )
		{
		/* If this is an unrecognised AVA, don't try and process it (the
		   content will already have been skipped in readAVA()) */
		if( status == OK_SPECIAL )
			return( CRYPT_OK );

		return( status );
		}

	/* Make sure that the string is a valid type for a DirectoryString.  We 
	   don't allow Universal strings since no-one in their right mind uses
	   those.
	   
	   Alongside DirectoryString values we also allow IA5Strings, from the
	   practice of stuffing email addresses into DNs */
	if( stringTag != BER_STRING_PRINTABLE && stringTag != BER_STRING_T61 && \
		stringTag != BER_STRING_BMP && stringTag != BER_STRING_UTF8 && \
		stringTag != BER_STRING_IA5 )
		return( CRYPT_ERROR_BADDATA );

	/* Skip broken AVAs with zero-length strings */
	if( valueLength <= 0 )
		return( CRYPT_OK );

	/* Record the string contents, avoiding moving it into a buffer */
	status = sMemGetDataBlock( stream, &value, valueLength );
	if( cryptStatusOK( status ) )
		status = sSkip( stream, valueLength, MAX_INTLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( value != NULL );

	/* If there's room for another AVA, mark this one as being continued.  The
	   +10 value is the minimum length for an AVA: SEQUENCE { OID, value } 
	   (2-bytes SEQUENCE + 5 bytes OID + 2 bytes (tag + length) + 1 byte min-
	   length data).  We don't do a simple =/!= check to get around incorrectly 
	   encoded lengths */
	if( rdnDataLeft >= ( stell( stream ) - rdnStart ) + 10 )
		flags |= DN_FLAG_CONTINUED;

	/* Convert the string into the local character set */
	status = copyFromAsn1String( stringBuffer, MAX_ATTRIBUTE_SIZE, 
								 &valueLength, &valueStringType, value, 
								 valueLength, stringTag );
	if( cryptStatusError( status ) )
		return( status );

	/* Add the DN component to the DN.  If we hit a non-memory related error
	   we turn it into a generic CRYPT_ERROR_BADDATA error since the other
	   codes are somewhat too specific for this case, something like 
	   CRYPT_ERROR_INITED or an arg error isn't too useful for the caller */
	status = insertDNstring( dnComponentListPtrPtr, type, stringBuffer, 
							 valueLength, valueStringType, flags, &dummy );
	return( ( cryptStatusError( status ) && status != CRYPT_ERROR_MEMORY ) ? \
			CRYPT_ERROR_BADDATA : status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readDNComponent( INOUT STREAM *stream, 
							INOUT DN_COMPONENT **dnComponentListPtrPtr )
	{
	int rdnLength, iterationCount, status;

	/* Read the start of the RDN */
	status = readSet( stream, &rdnLength );
	if( cryptStatusError( status ) )
		return( status );

	/* Read each RDN component */
	for( iterationCount = 0;
		 rdnLength > 0 && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		const int rdnStart = stell( stream );

		REQUIRES( rdnStart > 0 && rdnStart < MAX_INTLENGTH_SHORT );

		status = readRDNcomponent( stream, dnComponentListPtrPtr, 
								   rdnLength );
		if( cryptStatusError( status ) )
			return( status );

		rdnLength -= stell( stream ) - rdnStart;
		}
	if( rdnLength < 0 || iterationCount >= FAILSAFE_ITERATIONS_MED )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_OK );
	}

/* Read a DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readDN( INOUT STREAM *stream, 
			OUT_PTR_COND DN_PTR **dnComponentListPtrPtr )
	{
	DN_COMPONENT *dnComponentListPtr = NULL;
	int length, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( dnComponentListPtrPtr, sizeof( DN_COMPONENT * ) ) );

	/* Clear return value */
	*dnComponentListPtrPtr = NULL;

	/* Read the encoded DN into the local copy of the DN (in other words 
	   into the dnComponentListPtr, not the externally-visible
	   dnComponentListPtrPtr) */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	for( iterationCount = 0;
		 length > 0 && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		const int startPos = stell( stream );

		REQUIRES( startPos > 0 && startPos < MAX_INTLENGTH_SHORT );

		status = readDNComponent( stream, &dnComponentListPtr );
		if( cryptStatusError( status ) )
			break;
		length -= stell( stream ) - startPos;
		}
	if( cryptStatusError( status ) || \
		length < 0 || iterationCount >= FAILSAFE_ITERATIONS_MED )
		{
		/* Delete the local copy of the DN read so far if necessary */
		if( dnComponentListPtr != NULL )
			deleteDN( ( DN_PTR ** ) &dnComponentListPtr );
		return( cryptStatusError( status ) ? status : CRYPT_ERROR_BADDATA );
		}

	/* Copy the local copy of the DN back to the caller */
	*dnComponentListPtrPtr = dnComponentListPtr;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Write a DN								*
*																			*
****************************************************************************/

/* Perform the pre-encoding processing for a DN.  Note that sizeofDN() takes
   a slightly anomalous non-const parameter because the pre-encoding process
   required to determine the DN's size modifies portions of the DN component 
   values related to the encoding process */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int preEncodeDN( INOUT DN_COMPONENT *dnComponentPtr, 
						OUT_LENGTH_SHORT_Z int *length )
	{
	int size = 0, iterationCount;

	assert( isWritePtr( dnComponentPtr, sizeof( DN_COMPONENT ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	/* Clear return value */
	*length = 0;

	assert( isReadPtr( dnComponentPtr, sizeof( DN_COMPONENT ) ) );

	ENSURES( dnComponentPtr->prev == NULL );

	/* Walk down the DN pre-encoding each AVA */
	for( iterationCount = 0;
		 dnComponentPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED; 
		 dnComponentPtr = dnComponentPtr->next, iterationCount++ )
		{
		DN_COMPONENT *rdnStartPtr = dnComponentPtr;
		int innerIterationCount;

		/* Calculate the size of every AVA in this RDN */
		for( innerIterationCount = 0;
			 dnComponentPtr != NULL && \
				innerIterationCount < FAILSAFE_ITERATIONS_MED;
			 dnComponentPtr = dnComponentPtr->next, innerIterationCount++ )
			{
			const DN_COMPONENT_INFO *dnComponentInfo = dnComponentPtr->typeInfo;
			int dnStringLength, status;

			status = getAsn1StringInfo( dnComponentPtr->value, 
										dnComponentPtr->valueLength,
										&dnComponentPtr->valueStringType, 
										&dnComponentPtr->asn1EncodedStringType,
										&dnStringLength, TRUE );
			if( cryptStatusError( status ) )
				return( status );
			dnComponentPtr->encodedAVAdataSize = ( int ) \
										sizeofOID( dnComponentInfo->oid ) + \
										sizeofObject( dnStringLength );
			dnComponentPtr->encodedRDNdataSize = 0;
			rdnStartPtr->encodedRDNdataSize += ( int ) \
						sizeofObject( dnComponentPtr->encodedAVAdataSize );
			if( !( dnComponentPtr->flags & DN_FLAG_CONTINUED ) )
				break;
			}
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_MED );

		/* Calculate the overall size of the RDN */
		size += ( int ) sizeofObject( rdnStartPtr->encodedRDNdataSize );

		/* If the inner loop terminated because it reached the end of the DN 
		   then we need to explicitly exit the outer loop as well before it
		   tries to follow the 'next' link in the dnComponentPtr */
		if( dnComponentPtr == NULL )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	*length = size;

	return( CRYPT_OK );
	}

CHECK_RETVAL_LENGTH \
int sizeofDN( INOUT_OPT DN_PTR *dnComponentList )
	{
	int length, status;

	assert( dnComponentList == NULL || \
			isWritePtr( dnComponentList, sizeof( DN_COMPONENT ) ) );

	/* Null DNs produce a zero-length SEQUENCE */
	if( dnComponentList == NULL )
		return( sizeofObject( 0 ) );

	status = preEncodeDN( dnComponentList, &length );
	if( cryptStatusError( status ) )
		return( status );
	return( sizeofObject( length ) );
	}

/* Write a DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeDN( INOUT STREAM *stream, 
			 IN_OPT const DN_PTR *dnComponentList,
			 IN_TAG const int tag )
	{
	DN_COMPONENT *dnComponentPtr;
	int size, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( dnComponentList == NULL || \
			isReadPtr( dnComponentList, sizeof( DN_COMPONENT ) ) );

	REQUIRES_S( tag == DEFAULT_TAG || ( tag >= 0 && tag < MAX_TAG_VALUE ) );

	/* Special case for empty DNs */
	if( dnComponentList == NULL )
		return( writeConstructed( stream, 0, tag ) );

	status = preEncodeDN( ( DN_COMPONENT * ) dnComponentList, &size );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the DN */
	writeConstructed( stream, size, tag );
	for( dnComponentPtr = ( DN_COMPONENT * ) dnComponentList, \
			iterationCount = 0;
		 dnComponentPtr != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 dnComponentPtr = dnComponentPtr->next, iterationCount++ )
		{
		const DN_COMPONENT_INFO *dnComponentInfo = dnComponentPtr->typeInfo;
		BYTE dnString[ MAX_ATTRIBUTE_SIZE + 8 ];
		int dnStringLength;

		/* Write the RDN wrapper */
		if( dnComponentPtr->encodedRDNdataSize > 0 )
			{
			/* If it's the start of an RDN, write the RDN header */
			writeSet( stream, dnComponentPtr->encodedRDNdataSize );
			}
		writeSequence( stream, dnComponentPtr->encodedAVAdataSize );
		status = swrite( stream, dnComponentInfo->oid, 
						 sizeofOID( dnComponentInfo->oid ) );
		if( cryptStatusError( status ) )
			return( status );

		/* Convert the string to an ASN.1-compatible format and write it
		   out */
		status = copyToAsn1String( dnString, MAX_ATTRIBUTE_SIZE, 
								   &dnStringLength, dnComponentPtr->value,
								   dnComponentPtr->valueLength,
								   dnComponentPtr->valueStringType );
		if( cryptStatusError( status ) )
			return( status );
		if( dnComponentPtr->asn1EncodedStringType == BER_STRING_IA5 && \
			!dnComponentInfo->ia5OK )
			{
			/* If an IA5String isn't allowed in this instance, use a
			   T61String instead */
			dnComponentPtr->asn1EncodedStringType = BER_STRING_T61;
			}
		status = writeCharacterString( stream, dnString, dnStringLength,
									   dnComponentPtr->asn1EncodedStringType );
		if( cryptStatusError( status ) )
			return( status );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								DN String Routines							*
*																			*
****************************************************************************/

/* The ability to specify free-form DNs means that users can create 
   arbitrarily garbled and broken DNs (the creation of weird nonstandard DNs 
   is pretty much the main reason why the DN-string capability exists).  
   This includes DNs that can't be easily handled through normal cryptlib 
   facilities, for example ones where the CN component consists of illegal 
   characters or is in a form that isn't usable as a search key for 
   functions like cryptGetPublicKey().  Because of these problems this 
   functionality is disabled by default, if users want to use this oddball-
   DN facility it's up to them to make sure that the resulting DN 
   information works with whatever environment they're intending to use it 
   in */

#ifdef USE_CERT_DNSTRING 

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #pragma message( "  Building with string-form DNs enabled." )
#endif /* Warn about special features enabled */

/* Check whether a string can be represented as a textual DN string */

static BOOLEAN isTextString( IN_BUFFER( stringLength ) const BYTE *string, 
							 IN_LENGTH_ATTRIBUTE const int stringLength )
	{
	int i;

	assert( isReadPtr( string, stringLength ) );

	REQUIRES( stringLength > 0 && stringLength <= MAX_ATTRIBUTE_SIZE );

	/* Make sure that there are no control characters in the string.  We 
	   allow high-bit-set characters in order to support latin-1 strings,
	   see also the comment at the start of this section about the general
	   caveat-emptor philosophy for this interface */
	for( i = 0; i < stringLength; i++ )
		{
		if( ( string[ i ] & 0x7F ) < ' ' )
			return( FALSE );
		}

	return( TRUE );
	}

/* Read a DN in string form */

typedef struct {
	BUFFER_FIXED( labelLen ) \
	const BYTE *label;
	BUFFER_FIXED( textLen ) \
	const BYTE *text;
	int labelLen, textLen;	/* DN component label and value */
	BOOLEAN isContinued;	/* Whether there are further AVAs in this RDN */
	} DN_STRING_INFO;

#define MAX_DNSTRING_COMPONENTS 32

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN parseDNString( OUT_ARRAY( MAX_DNSTRING_COMPONENTS ) \
									DN_STRING_INFO *dnStringInfo,
							  OUT_RANGE( 0, MAX_DNSTRING_COMPONENTS ) \
									int *dnStringInfoIndex,
							  IN_BUFFER( stringLength ) const BYTE *string, 
							  IN_LENGTH_ATTRIBUTE const int stringLength )
	{
	int stringPos, stringInfoIndex;

	assert( isWritePtr( dnStringInfo, sizeof( DN_STRING_INFO ) * \
									  MAX_DNSTRING_COMPONENTS ) );
	assert( isReadPtr( string, stringLength ) );

	REQUIRES( stringLength > 0 && stringLength <= MAX_ATTRIBUTE_SIZE );

	/* Clear return values */
	memset( dnStringInfo, 0,
			sizeof( DN_STRING_INFO ) * MAX_DNSTRING_COMPONENTS );
	*dnStringInfoIndex = 0;

	/* Make sure that the string is decodable as a text string */
	if( !isTextString( string, stringLength ) )
		return( FALSE );

	/* Verify that a DN string is of the form:

		dnString ::= assignment '\0' | assignment ',' assignment
		assignment ::= label '=' text */
	for( stringPos = 0, stringInfoIndex = 0;
		 stringPos < stringLength && \
			stringInfoIndex < MAX_DNSTRING_COMPONENTS;
		 stringInfoIndex++ )
		{
		DN_STRING_INFO *dnStringInfoPtr = &dnStringInfo[ stringInfoIndex ];
		int i;

		/* Check for label '=' ... */
		for( i = stringPos; i < stringLength; i++ )
			{
			const int ch = byteToInt( string[ i ] );

			if( ch == '\\' || ch == '+' || ch == ',' )
				{
				/* The label component can't have special characters in it */
				return( FALSE );
				}
			if( ch == '=' )
				break;
			}
		if( i <= stringPos || i >= stringLength - 1 )	/* -1 for '=' */
			{
			/* The label component is empty */
			return( FALSE );
			}
		ENSURES( rangeCheckZ( stringPos, i - stringPos, stringLength ) );
		dnStringInfoPtr->label = string + stringPos;
		dnStringInfoPtr->labelLen = i - stringPos;
		stringPos = i + 1;		/* Skip text + '=' */

		/* Check for ... text { EOT | ',' ... | '+' ... }.  Note that we 
		   make the loop variable increment part of the loop code rather 
		   than having it in the for() statement to avoid a series of messy 
		   fencepost-error adjustments in the checks that follow */
		for( i = stringPos; i < stringLength && \
							i < MAX_ATTRIBUTE_SIZE; i++ )
			{
			const int ch = byteToInt( string[ i ] );

			/* Check for invalid data */
			if( ch == '=' )
				{
				/* Spurious '=' */
				return( FALSE );
				}
			if( ch == '\\' )
				{
				if( i >= stringLength - 1 )
					{
					/* Missing escaped character */
					return( FALSE );
					}

				/* It's an escaped character that isn't subject to the usual
				   checks, skip it and continue */
				i++;
				continue;
				}

			/* If this isn't a continuation symbol, continue */
			if( ch != ',' && ch != '+' )
				continue;

			/* We've reached a continuation symbol, make sure that there's 
			   room for at least another 'x=y' after this point */
			if( i >= stringLength - 3 )
				return( FALSE );

			break;
			}
		ENSURES( i < MAX_ATTRIBUTE_SIZE );
		ENSURES( rangeCheck( stringPos, i - stringPos, stringLength ) );
		dnStringInfoPtr->text = string + stringPos;
		dnStringInfoPtr->textLen = i - stringPos;
		if( string[ i ] == ',' || string[ i ] == '+' )
			{
			/* Skip the final ',' or '+' and remember whether this is a 
			   continued RDN */
			if( string[ i ] == '+' )
				dnStringInfoPtr->isContinued = TRUE;
			i++;
			}
		stringPos = i;			/* Skip text + optional ','/'+' */

		/* Strip leading and trailing whitespace on the label and text */
		dnStringInfoPtr->labelLen = \
				strStripWhitespace( ( const char ** ) &dnStringInfoPtr->label,
									dnStringInfoPtr->label, 
									dnStringInfoPtr->labelLen );
		dnStringInfoPtr->textLen = \
				strStripWhitespace( ( const char ** ) &dnStringInfoPtr->text,
									dnStringInfoPtr->text, 
									dnStringInfoPtr->textLen );
		if( dnStringInfoPtr->labelLen < 1 || \
			dnStringInfoPtr->labelLen > MAX_ATTRIBUTE_SIZE || \
			dnStringInfoPtr->textLen < 1 || \
			dnStringInfoPtr->textLen > MAX_ATTRIBUTE_SIZE )
			return( FALSE );
		}
	if( stringInfoIndex <= 0 || stringInfoIndex >= MAX_DNSTRING_COMPONENTS )
		return( FALSE );
	*dnStringInfoIndex = stringInfoIndex;

	return( TRUE );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readDNstring( INOUT_PTR DN_PTR **dnComponentListPtrPtr,
				  IN_BUFFER( stringLength ) const BYTE *string, 
				  IN_LENGTH_ATTRIBUTE const int stringLength )
	{
	DN_STRING_INFO dnStringInfo[ MAX_DNSTRING_COMPONENTS + 8 ];
	DN_COMPONENT *dnComponentPtr = NULL;
	int stringInfoIndex, iterationCount;

	assert( isWritePtr( dnComponentListPtrPtr, sizeof( DN_COMPONENT * ) ) );
	assert( isReadPtr( string, stringLength ) );

	REQUIRES( stringLength > 0 && stringLength <= MAX_ATTRIBUTE_SIZE );

	/* Clear return value */
	*dnComponentListPtrPtr = NULL;

	/* We have to perform the text string to DN translation in two stages
	   thanks to the backwards encoding required by RFC 1779.  First we 
	   parse it forwards to separate out the RDN components, then we move 
	   through the parsed information backwards adding it to the RDN (with 
	   special handling for multi-AVA RDNs as for writeDNstring()).  Overall 
	   this isn't so bad because it means that we can perform a general 
	   firewall check to make sure that the DN string is well-formed and 
	   then leave the encoding as a separate pass */
	if( !parseDNString( dnStringInfo, &stringInfoIndex, string, 
						stringLength ) )
		return( CRYPT_ARGERROR_STR1 );

	/* parseDNString() returns the number of entries parsed, since we're
	   using zero-based indexing we have to decrement the value returned to
	   provide the actual index into the dnStringInfo[] array */
	stringInfoIndex--;

	for( iterationCount = 0; 
		 stringInfoIndex >= 0 && iterationCount < FAILSAFE_ITERATIONS_MED;
		 stringInfoIndex--, iterationCount++ )
		{
		const DN_STRING_INFO *dnStringInfoPtr;
		BOOLEAN isContinued;
		int innerIterationCount;

		/* Find the start of the RDN */
		while( stringInfoIndex > 0 && \
			   dnStringInfo[ stringInfoIndex - 1 ].isContinued )
			stringInfoIndex--;
		dnStringInfoPtr = &dnStringInfo[ stringInfoIndex ];

		for( isContinued = TRUE, innerIterationCount = 0;
			 isContinued && innerIterationCount < FAILSAFE_ITERATIONS_MED;
			 innerIterationCount++ )
			{
			CRYPT_ERRTYPE_TYPE dummy;
			const DN_COMPONENT_INFO *dnComponentInfo;
			BYTE textBuffer[ MAX_ATTRIBUTE_SIZE + 8 ];
			CRYPT_ATTRIBUTE_TYPE type;
			int i, textIndex = 0, valueStringType, dummy1, dummy2, status;

			/* Look up the DN component information */
			dnComponentInfo = findDNInfoByLabel( dnStringInfoPtr->label, 
												 dnStringInfoPtr->labelLen );
			if( dnComponentInfo == NULL )
				{
				if( dnComponentPtr != NULL )
					deleteDN( ( DN_PTR ** ) &dnComponentPtr );
				return( CRYPT_ARGERROR_STR1 );
				}
			type = dnComponentInfo->type;

			/* Convert the text to canonical form, removing any escapes for
			   special characters */
			for( i = 0; i < dnStringInfoPtr->textLen && \
						i < MAX_ATTRIBUTE_SIZE; i++ )
				{
				int ch = byteToInt( dnStringInfoPtr->text[ i ] );

				if( ch == '\\' )
					{
					/* Skip '\\' */
					i++;
					if( i >= dnStringInfoPtr->textLen )
						{
						if( dnComponentPtr != NULL )
							deleteDN( ( DN_PTR ** ) &dnComponentPtr );
						return( CRYPT_ARGERROR_STR1 );
						}
					ch = byteToInt( dnStringInfoPtr->text[ i ] );
					}
				textBuffer[ textIndex++ ] = intToByte( ch );
				}
			ENSURES( i < MAX_ATTRIBUTE_SIZE );
			ENSURES( textIndex > 0 && textIndex < MAX_INTLENGTH_SHORT );

			/* The value is coming from an external source, make sure that 
			   it's representable as a certificate string type.  All that 
			   we care about here is the validity so we ignore the returned 
			   encoding information */
			status = getAsn1StringInfo( textBuffer, textIndex, 
										&valueStringType, &dummy1, &dummy2, 
										FALSE );
			if( cryptStatusError( status ) )
				{
				if( dnComponentPtr != NULL )
					deleteDN( ( DN_PTR ** ) &dnComponentPtr );
				return( CRYPT_ARGERROR_STR1 );
				}

			/* Add the AVA to the DN */
			if( type == CRYPT_CERTINFO_COUNTRYNAME )
				{
				/* If it's a country code force it to uppercase as per ISO 3166 */
				if( textIndex != 2 )
					{
					if( dnComponentPtr != NULL )
						deleteDN( ( DN_PTR ** ) &dnComponentPtr );
					return( CRYPT_ARGERROR_STR1 );
					}
				textBuffer[ 0 ] = intToByte( toUpper( textBuffer[ 0 ] ) );
				textBuffer[ 1 ] = intToByte( toUpper( textBuffer[ 1 ] ) );
				}
			status = insertDNstring( &dnComponentPtr, type, 
									 textBuffer, textIndex, valueStringType,
									 ( dnStringInfoPtr->isContinued ) ? \
										DN_FLAG_CONTINUED | DN_FLAG_NOCHECK :
										DN_FLAG_NOCHECK, &dummy );
			if( cryptStatusError( status ) )
				{
				if( dnComponentPtr != NULL )
					deleteDN( ( DN_PTR ** ) &dnComponentPtr );
				return( status );
				}

			/* Move on to the next AVA */
			isContinued = dnStringInfoPtr->isContinued;
			dnStringInfoPtr++;
			}
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_MED );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* We're done, lock the DN against further updates */
	*dnComponentListPtrPtr = dnComponentPtr;
	for( iterationCount = 0;
		 dnComponentPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 dnComponentPtr = dnComponentPtr->next, iterationCount++ )
		dnComponentPtr->flags |= DN_FLAG_LOCKED;
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	return( CRYPT_OK );
	}

/* Write a DN in string form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeAVAString( INOUT STREAM *stream,
						   const DN_COMPONENT *dnComponentPtr )
	{
	const DN_COMPONENT_INFO *componentInfoPtr = dnComponentPtr->typeInfo;
	int i, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( dnComponentPtr, sizeof( DN_COMPONENT ) ) );

	/* Print the current AVA */
	swrite( stream, componentInfoPtr->name, componentInfoPtr->nameLen );
	status = sputc( stream, '=' );
	if( cryptStatusError( status ) )
		return( status );
	for( i = 0; i < dnComponentPtr->valueLength; i++ )
		{
		const int ch = byteToInt( ( ( BYTE * ) dnComponentPtr->value )[ i ] );

		if( ch == ',' || ch == '=' || ch == '+' || ch == ';' || \
			ch == '\\' || ch == '"' )
			sputc( stream, '\\' );
		status = sputc( stream, ch );
		if( cryptStatusError( status ) )
			return( status );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeDNstring( INOUT STREAM *stream, 
				   IN_OPT const DN_PTR *dnComponentList )
	{
	const DN_COMPONENT *dnComponentPtr = dnComponentList;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( dnComponentList == NULL || \
			isReadPtr( dnComponentList, sizeof( DN_COMPONENT ) ) );

	/* If it's an empty DN there's nothing to write */
	if( dnComponentPtr == NULL )
		return( CRYPT_OK );

	/* Find the end of the DN string.  We have to print the RDNs backwards
	   because of ISODE's JANET memorial backwards encoding.  This also 
	   provides a convenient location to check that the DN data is actually
	   representable as a text string.

	   Some string types can't be represented as text strings which means 
	   that we can't send them directly to the output.  The details of what 
	   to do here are a bit complex, in theory we could send them out as 
	   UTF-8 but few environments are going to expect this as a returned 
	   type, particularly when the existing expectation is for oddball 
	   characters in text strings to be latin-1.  The least ugly solution 
	   seems to be to just return an indicator that this string can't be 
	   represented */
	for( iterationCount = 0;
		 dnComponentPtr->next != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 dnComponentPtr = dnComponentPtr->next, iterationCount++ )
		{
		/* Make sure that the DN component is representable as a text 
		   string.  Exactly what we should return if this check fails is a
		   bit uncertain since there's no error code that it really
		   corresponds to, CRYPT_ERROR_NOTAVAIL appears to be the least
		   inappropriate one to use.
		   
		   An alternative is to return a special-case string like "(DN 
		   can't be represented in string form)" but this then looks (from 
		   the return status) as if it was actually representable, requiring 
		   special-case checks for valid-but-not-valid returned data, so the 
		   error status is probably the best option */
		if( !isTextString( dnComponentPtr->value, 
						   dnComponentPtr->valueLength ) )
			return( CRYPT_ERROR_NOTAVAIL );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	for( iterationCount = 0;
		 dnComponentPtr != NULL && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		const DN_COMPONENT *dnComponentCursor;
		int innerIterationCount;

		/* Find the start of the RDN */
		for( innerIterationCount = 0;
			 dnComponentPtr->prev != NULL && \
				( dnComponentPtr->prev->flags & DN_FLAG_CONTINUED ) && \
				innerIterationCount < FAILSAFE_ITERATIONS_MED;
			 dnComponentPtr = dnComponentPtr->prev, innerIterationCount++ );
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_MED );
		dnComponentCursor = dnComponentPtr;
		dnComponentPtr = dnComponentPtr->prev;

		/* Print the current RDN */
		for( status = CRYPT_OK, innerIterationCount = 0;
			 cryptStatusOK( status ) && \
				innerIterationCount < FAILSAFE_ITERATIONS_MED;
			 innerIterationCount++ )
			{
			status = writeAVAString( stream, dnComponentCursor );
			if( cryptStatusError( status ) )
				return( status );

			/* If this is the last AVA in the RDN, we're done */
			if( !( dnComponentCursor->flags & DN_FLAG_CONTINUED ) )
				break;

			/* There are more AVAs in this RDN, print a continuation 
			   indicator and move on to the next AVA */
			status = swrite( stream, " + ", 3 );
			if( cryptStatusError( status ) )
				return( status );
			dnComponentCursor = dnComponentCursor->next;
			ENSURES( dnComponentCursor != NULL );
			}
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_MED );

		/* If there are more components to come print an RDN separator */
		if( dnComponentPtr != NULL )
			{
			status = swrite( stream, ", ", 2 );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}
#endif /* USE_CERT_DNSTRING */
#endif /* USE_CERTIFICATES */
