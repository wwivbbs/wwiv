/****************************************************************************
*																			*
*						   ASN.1 Checking Routines							*
*						Copyright Peter Gutmann 1992-2009					*
*																			*
****************************************************************************/

#include <ctype.h>
#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/* The maximum nesting level for constructed or encapsulated objects (this
   can get surprisingly high for some of the more complex attributes).  This
   value is chosen to pass all normal certs while avoiding stack overflows
   for artificial bad data */

#define MAX_NESTING_LEVEL	50

/* When we parse a nested data object encapsulated within a larger object,
   the length is initially set to a magic value which is adjusted to the
   actual length once we start parsing the object.  Even CRLs will hopefully 
   never reach 500MB, the current limits seems to be around 150MB */

#define LENGTH_MAGIC		500000000L

/* Current parse state.  This is used to check for potential BIT STRING and
   OCTET STRING targets for OCTET/BIT STRING holes, which are always
   preceded by an AlgorithmIdentifier.  In order to detect these without
   having to know every imaginable AlgorithmIdentifier OID, we check for the
   following sequence of events:

	SEQUENCE {			-- STATE_SEQUENCE
		OID,			-- STATE_HOLE_OID
		NULL			-- STATE_NULL
		},
						-- STATE_CHECK_HOLE_BITSTRING
	BIT STRING | ...

	SEQUENCE {			-- STATE_SEQUENCE
		OID,			-- STATE_HOLE_OID
		[ BOOLEAN OPT,	-- STATE_BOOLEAN ]
						-- STATE_CHECK_HOLE_OCTETSTRING
		OCTET STRING | ...

   Once we reach any of the STATE_CHECK_HOLE_* states, if we hit a BIT STRING 
   or OCTET STRING as the next item then we try and locate encapsulated 
   content within it.
   
   This type of checking is rather awkward in the (otherwise stateless) code 
   but is the only way to be sure that it's safe to try burrowing into an 
   OCTET STRING or BIT STRING to try to find encapsulated data, since 
   otherwise even with relatively strict checking there's still a very small 
   chance that random data will look like a nested object.

   The handling of BIT STRING encapsulation is complicated by the fact that
   for crypto use it really only occurs in one of two cases:

	SEQUENCE {
		OID,
		NULL
		},
	BIT STRING {
		SEQUENCE {
			INTEGER
			...

   for public keys and and:

	SEQUENCE {
		OID,
		NULL
		},
	BIT STRING ...

   for signatures (with an additional complication for DLP keys that the 
   NULL for the public-key parameters is replaced by a SEQUENCE { ... }
   containing the public parameters).  Because of this there's little point
   in trying to track the state because any occurrence of a potential BIT 
   STRING hole has a 50:50 chance of actually being one or not.  Because of
   this we don't bother tracking the state for BIT STRINGs but rely on the
   encapsulation-check to catch them.  This should be fairly safe because we
   require that the value be:

	[ SEQUENCE ][ = outerLength - SEQUENCE-hdrSize ]
		[ INTEGER ][ <= innerlength - INTEGER-hdrSize ]

   which provides at least 26-28 bits of safety */

typedef enum {
	/* Generic non-state */
	STATE_NONE,

	/* States corresponding to ASN.1 primitives */
	STATE_BOOLEAN, STATE_NULL, STATE_OID, STATE_SEQUENCE,

	/* States corresponding to different parts of a SEQUENCE { OID, optional,
	   potential OCTET/BIT STRING } sequence */
	STATE_HOLE_OID, /*STATE_CHECK_HOLE_BITSTRING,*/ STATE_CHECK_HOLE_OCTETSTRING,

	/* Error state */
	STATE_ERROR,
	
	STATE_LAST
	} ASN1_STATE;

/* Structure to hold info on an ASN.1 item */

typedef struct {
	int tag;			/* Tag */
	long length;		/* Data length */
	BOOLEAN indefinite;	/* Item has indefinite length */
	int headerSize;		/* Size of tag+length */
	} ASN1_ITEM;

/* Get an ASN.1 object's tag and length */

CHECK_RETVAL_ENUM( STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getItem( INOUT STREAM *stream, OUT ASN1_ITEM *item )
	{
	const long offset = stell( stream );
	long length;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( item, sizeof( ASN1_ITEM ) ) );

	REQUIRES_EXT( ( offset >= 0 && offset < MAX_INTLENGTH ), STATE_ERROR );

	/* Clear return value */
	memset( item, 0, sizeof( ASN1_ITEM ) );

	/* Read the tag.  We can't use peekTag() for this since we may be 
	   reading EOC octets, which would be rejected by peekTag() */
	status = item->tag = sPeek( stream );
	if( cryptStatusError( status ) )
		return( STATE_ERROR );
	if( item->tag == BER_EOC )
		{
		/* It looks like EOC octets, make sure that they're in order */
		status = checkEOC( stream );
		if( cryptStatusError( status ) )
			return( STATE_ERROR );
		if( status == TRUE )
			{
			item->headerSize = 2;
			return( STATE_NONE );
			}
		}

	/* Make sure that it's at least vaguely valid before we try the
	   readLongGenericHole() */
	if( item->tag <= 0 || item->tag >= MAX_TAG )
		return( STATE_ERROR );

	/* It's not an EOC, read the tag and length as a generic hole */
	status = readLongGenericHole( stream, &length, item->tag );
	if( cryptStatusError( status ) )
		return( STATE_ERROR );
	item->headerSize = stell( stream ) - offset;
	if( length == CRYPT_UNUSED )
		item->indefinite = TRUE;
	else
		item->length = length;
	return( STATE_NONE );
	}

/* Check whether an ASN.1 object is encapsulated inside an OCTET STRING or
   BIT STRING.  After performing the various checks we have to explicitly
   clear the stream error state since the probing for valid data could have
   set the error indicator if nothing valid was found.

   Note that this is a no-biased test since the best that we can do is guess 
   at the presence of encapsulated content and we can't risk rejecting valid
   content based on a false positive.  This means that unfortunately 
   maliciously-encoded nested content with (for example) an incorrect inner 
   length will slip past our checks, and we have to rely on the robustness 
   of the general ASN1-read code to avoid problems with it.  It's not 
   obvious whether this is really a serious problem or not though (apart from
   it being a certificational weakness), a too-short length will result in 
   whatever additional padding is present being skipped by the general ASN1-
   read code, a too-long length will result in an immediate error as the 
   decoder encounters garbage from reading past the TLV that follows */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkEncapsulation( INOUT STREAM *stream, 
								   IN_LENGTH const int length,
								   const BOOLEAN isBitstring,
								   IN_ENUM_OPT( ASN1_STATE ) \
									const ASN1_STATE state )
	{
	BOOLEAN isEncapsulated = TRUE;
	const long streamPos = stell( stream );
	const int tag = peekTag( stream );
	int innerLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_B( length > 0 && length < MAX_INTLENGTH );
	REQUIRES_B( state >= STATE_NONE && state < STATE_ERROR );
	REQUIRES_B( streamPos >= 0 && streamPos < MAX_INTLENGTH );

	/* Make sure that the tag is in order */
	if( cryptStatusError( tag ) )
		{
		sClearError( stream );
		sseek( stream, streamPos );
		return( FALSE );
		}

	/* Make sure that there's an encapsulated object present.  This is a 
	   reasonably effective check, but unfortunately this same effectiveness 
	   means that it'll reject nested objects with incorrect lengths.  It's 
	   not really possible to fix this, either there'll be false positives 
	   due to true OCTET/BIT STRINGs that look like they might contain 
	   nested data or there'll be no false positives but nested content 
	   with slightly incorrect encodings will be missed (see the comment at
	   the start for more on this) */
	status = readGenericHole( stream, &innerLength, 1, DEFAULT_TAG );
	if( cryptStatusError( status ) || \
		( stell( stream ) - streamPos ) + innerLength != length )
		{
		sClearError( stream );
		sseek( stream, streamPos );
		return( FALSE );
		}

	/* A BIT STRING that encapsulates something only ever contains
	   { SEQUENCE { INTEGER, ... } } */
	if( isBitstring )
		{
		/* Make sure that there's a SEQUENCE containing an INTEGER present */
		if( tag != BER_SEQUENCE || peekTag( stream ) != BER_INTEGER || \
			cryptStatusError( readGenericHole( stream, &innerLength, 1,
											   BER_INTEGER ) ) || \
			innerLength > length - 4 )
			isEncapsulated = FALSE;

		sClearError( stream );
		sseek( stream, streamPos );
		return( isEncapsulated );
		}

	/* An OCTET STRING is more complex.  This could encapsulate any of:

		BIT STRING: keyUsage, crlReason, Netscape certType, must be
			<= 16 bits and a valid bitstring.
		GeneralisedTime: invalidityDate: Not possible to check directly
			since the obvious check for a valid length will also fail
			invalid-length encodings, missing the very thing we usually
			want to check for, so all that we can check for is a vaguely 
			valid length.
		IA5String: Netscape extensions, the most that we can do is perform 
			an approximate length range check
		INTEGER: deltaCRLIndicator, crlNumber, must be <= 16 bits.
		OCTET STRING: keyID, again the most that we can do is perform an
			approximate length range check.
		OID: holdInstructionCode, again just an approximate length range 
			check.
		SEQUENCE: most extensions, a bit difficult to check but again we can 
			make sure that the length is right for strict encapsulation.

	   Note that we want these checks to be as liberal as possible since 
	   we're only checking for the *possibility* of encapsulated data at
	   this point (again, see the comment at the start).  Once we're fairly 
	   certain that it's encapsulated data then we recurse down into it with 
	   checkASN1().  If we rejected too many things at this level then it'd 
	   never get checked via checkASN1() */
	switch( tag )
		{
		case BER_BITSTRING:
			if( innerLength < 0 || innerLength > 2 )
				isEncapsulated = FALSE;
			else
				{
				int ch = sgetc( stream );

				if( ch < 0 || ch > 7 )
					isEncapsulated = FALSE;
				}
			break;

		case BER_TIME_GENERALIZED:
			if( innerLength < 10 || innerLength > 20 )
				isEncapsulated = FALSE;
			break;

		case BER_INTEGER:
			if( innerLength < 0 || innerLength > 2 )
				isEncapsulated = FALSE;
			break;

		case BER_STRING_IA5:
		case BER_OCTETSTRING:
			if( innerLength < 2 || innerLength > 256 )
				isEncapsulated = FALSE;
			break;

		case BER_OBJECT_IDENTIFIER:
			if( innerLength < MIN_OID_SIZE - 2 || \
				innerLength > MAX_OID_SIZE )
				isEncapsulated = FALSE;
			break;

		case BER_SEQUENCE:
			break;

		default:
			isEncapsulated = FALSE;
		}
	sClearError( stream );
	sseek( stream, streamPos );
	return( isEncapsulated );
	}

/* Check a primitive ASN.1 object */

CHECK_RETVAL_ENUM( STATE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STATE checkASN1( INOUT STREAM *stream, 
							 IN_LENGTH const long length, 
							 const BOOLEAN isIndefinite, 
							 IN_RANGE( 0, MAX_NESTING_LEVEL ) const int level, 
							 IN_ENUM_OPT( ASN1_STATE ) ASN1_STATE state, 
							 const BOOLEAN checkDataElements );

CHECK_RETVAL_ENUM( STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ASN1_STATE checkPrimitive( INOUT STREAM *stream, const ASN1_ITEM *item,
								  IN_RANGE( 1, MAX_NESTING_LEVEL ) const int level, 
								  IN_ENUM_OPT( ASN1_STATE ) const ASN1_STATE state )
	{
	int length = ( int ) item->length, ch, i;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( item, sizeof( ASN1_ITEM ) ) );
	
	REQUIRES( level > 0 && level <= MAX_NESTING_LEVEL );
	REQUIRES( state >= STATE_NONE && state < STATE_ERROR );
	REQUIRES( length >= 0 && length < MAX_INTLENGTH );

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( STATE_ERROR );

	/* In theory only NULL and EOC elements (BER_RESERVED) are allowed to 
	   have a zero length, but some broken implementations (Netscape, Van 
	   Dyke) encode numeric zero values as a zero-length element so we have 
	   to accept these as well */
	if( length <= 0 && item->tag != BER_NULL && \
					   item->tag != BER_RESERVED && \
					   item->tag != BER_INTEGER )
		return( STATE_ERROR );

	/* Perform a general check that everything is OK.  We don't check for 
	   invalid content except where it would impede decoding of the data in
	   order to avoid failing on all of the broken certs out there */
	switch( item->tag )
		{
		case BER_BOOLEAN:
			return( cryptStatusError( sgetc( stream ) ) ? \
					STATE_ERROR : STATE_BOOLEAN );

		case BER_INTEGER:
		case BER_ENUMERATED:
			if( length > 0 &&	/* May be encoded as a zero-length value */
				cryptStatusError( sSkip( stream, length ) ) )
				return( STATE_ERROR );
			return( STATE_NONE );

		case BER_BITSTRING:
			/* Check the number of unused bits */
			ch = sgetc( stream );
			length--;
			if( length < 0 || length >= MAX_INTLENGTH || \
				ch < 0 || ch > 7 )
				{
				/* Invalid number of unused bits */
				return( STATE_ERROR );
				}

			/* If it's short enough to be a bit flag, it's just a sequence 
			   of bits */
			if( length <= 4 )
				{
				if( length > 0 && \
					cryptStatusError( sSkip( stream, length ) ) )
					return( STATE_ERROR );
				return( STATE_NONE );
				}
			/* Fall through */

		case BER_OCTETSTRING:
			{
			const BOOLEAN isBitstring = ( item->tag == BER_BITSTRING ) ? \
										TRUE : FALSE;

			/* Check to see whether an OCTET STRING or BIT STRING hole is 
			   allowed at this point (a BIT STRING must be preceded by 
			   { SEQ, OID, NULL }, an OCTET STRING must be preceded by 
			   { SEQ, OID, {BOOLEAN} }), and if it's something encapsulated 
			   inside the string, handle it as a constructed item */
#if 0	/* See comment at start */
			if( ( ( isBitstring && state == STATE_CHECK_HOLE_BITSTRING ) || \
				  ( !isBitstring && ( state == STATE_HOLE_OID || \
									  state == STATE_CHECK_HOLE_OCTETSTRING ) ) ) && \
				checkEncapsulation( stream, length, isBitstring, state ) )
#else
			if( ( isBitstring || state == STATE_HOLE_OID || \
								 state == STATE_CHECK_HOLE_OCTETSTRING ) && \
				checkEncapsulation( stream, length, isBitstring, state ) )
#endif /* 0 */
				{
				ASN1_STATE encapsState;

				encapsState = checkASN1( stream, length, item->indefinite,
										 level + 1, STATE_NONE, TRUE );
				return( ( encapsState < STATE_NONE || \
						  encapsState >= STATE_ERROR ) ? \
						STATE_ERROR : STATE_NONE );
				}

			/* Skip the data */
			return( cryptStatusError( sSkip( stream, length ) ) ? \
					STATE_ERROR : STATE_NONE );
			}

		case BER_OBJECT_IDENTIFIER:
			if( length > MAX_OID_SIZE - 2 )
				{
				/* Total OID size (including tag and length, since they're 
				   treated as a blob) should be less than a sane limit */
				return( STATE_ERROR );
				}
			return( cryptStatusError( sSkip( stream, length ) ) ? \
					STATE_ERROR : STATE_OID );

		case BER_RESERVED:
			break;					/* EOC */

		case BER_NULL:
			return( STATE_NULL );

		case BER_STRING_BMP:
		case BER_STRING_GENERAL:	/* Produced by Entrust software */
		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_NUMERIC:
		case BER_STRING_PRINTABLE:
		case BER_STRING_T61:
		case BER_STRING_UTF8:
			return( cryptStatusError( sSkip( stream, length ) ) ? \
					STATE_ERROR : STATE_NONE );

		case BER_TIME_UTC:
		case BER_TIME_GENERALIZED:
			if( item->tag == BER_TIME_GENERALIZED )
				{
				if( length != 15 )
					return( STATE_ERROR );
				}
			else
				{
				if( length != 13 )
					return( STATE_ERROR );
				}
			for( i = 0; i < length - 1; i++ )
				{
				ch = sgetc( stream );
				if( cryptStatusError( ch ) || !isDigit( ch ) )
					return( STATE_ERROR );
				}
			if( sgetc( stream ) != 'Z' )
				return( STATE_ERROR );
			return( STATE_NONE );

		default:
			/* Disallowed or unrecognised primitive */
			return( STATE_ERROR );
		}

	return( STATE_NONE );
	}

/* Check a single ASN.1 object.  checkASN1() and checkASN1Object() are 
   mutually recursive, the ...Object() version only exists to avoid a
   large if... else chain in checkASN1().  A typical checking run is
   as follows:

	30 nn			cASN1 -> cAObj -> cASN1
	   30 nn						  cASN1 -> cAObj -> cASN1
		  04 nn nn										cASN1 -> cPrim

	30 80			cASN1 -> cAObj -> cASN1
	   30 80						  cASN1 -> cAObj -> cASN1
		  04 nn nn										cASN1 -> cPrim
	   00 00						  cASN1 <- cAObj <- cASN1
	00 00			cASN1 <- cAObj <- cASN1

   The use of checkASN1Object() leads to an (apparently) excessively deep 
   call hierarchy, but that's mostly just an artifact of the way that it's 
   diagrammed here */

CHECK_RETVAL_ENUM( STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ASN1_STATE checkASN1Object( INOUT STREAM *stream, const ASN1_ITEM *item,
								   IN_RANGE( 1, MAX_NESTING_LEVEL ) \
									const int level, 
								   IN_ENUM_OPT( ASN1_STATE ) const ASN1_STATE state,
								   const BOOLEAN checkDataElements )
	{
	ASN1_STATE newState;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( item, sizeof( ASN1_ITEM ) ) );

	REQUIRES( level > 0 && level <= MAX_NESTING_LEVEL );
	REQUIRES( state >= STATE_NONE && state < STATE_ERROR );

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( STATE_ERROR );

	/* Check the contents for validity.  A straight data-length check doesn't 
	   check nested elements since all it cares about is finding the overall 
	   length with as little effort as possible */
	if( ( item->tag & BER_CLASS_MASK ) == BER_UNIVERSAL )
		{
		/* If we're not interested in the data elements (i.e. if we're just 
		   doing a length check) and the item has a definite length, just 
		   skip over it and continue */
		if( !checkDataElements && item->length > 0 )
			{
			if( cryptStatusError( sSkip( stream, item->length ) ) )
				return( STATE_ERROR );
			}

		/* If it's constructed, parse the nested object(s) */
		if( ( item->tag & BER_CONSTRUCTED_MASK ) == BER_CONSTRUCTED )
			{
			/* Special-case for zero-length SEQUENCE/SET */
			if( item->length <= 0 && !item->indefinite )
				return( STATE_NONE );

			return( checkASN1( stream, item->length, item->indefinite,
							   level + 1, ( item->tag == BER_SEQUENCE ) ? \
									STATE_SEQUENCE : STATE_NONE,
									checkDataElements ) );
			}

		/* It's primitive, check the primitive element with optional state
		   update: SEQ + OID -> HOLE_OID; OID + { NULL | BOOLEAN } -> 
		   HOLE_BITSTRING/HOLE_OCTETSTRING */
		newState = checkPrimitive( stream, item, level + 1, state );
		if( newState < STATE_NONE || newState >= STATE_ERROR )
			return( STATE_ERROR );
		if( state == STATE_SEQUENCE && newState == STATE_OID )
			return( STATE_HOLE_OID );
		if( state == STATE_HOLE_OID )
			{
#if 0	/* See comment at start */
			if( newState == STATE_NULL )
				return( STATE_CHECK_HOLE_BITSTRING );
#endif /* 0 */
			if( newState == STATE_BOOLEAN )
				return( STATE_CHECK_HOLE_OCTETSTRING );
			}
		return( STATE_NONE );
		}

	/* Zero-length objects are usually an error, however PKCS #10 has an
	   attribute-encoding ambiguity that produces zero-length tagged 
	   extensions and OCSP has its braindamaged context-specific tagged 
	   NULLs so we don't complain about them if they have low-valued 
	   context-specific tags */
	if( item->length <= 0 && !item->indefinite )
		{
		return( ( ( item->tag & BER_CLASS_MASK ) == BER_CONTEXT_SPECIFIC && \
				  EXTRACT_CTAG( item->tag ) <= 3 ) ? \
				STATE_NONE : STATE_ERROR );
		}

	ENSURES( item->length > 0 || item->indefinite );

	/* If it's constructed, parse the nested object(s) */
	if( ( item->tag & BER_CONSTRUCTED_MASK ) == BER_CONSTRUCTED )
		{
		newState = checkASN1( stream, item->length, item->indefinite,
							  level + 1, STATE_NONE, checkDataElements );
		return( ( newState < STATE_NONE || newState >= STATE_ERROR ) ? \
				STATE_ERROR : STATE_NONE );
		}

	/* It's a context-specific tagged item that could contain anything, just 
	   skip it */
	if( ( item->tag & BER_CLASS_MASK ) != BER_CONTEXT_SPECIFIC || \
		item->length <= 0 )
		return( STATE_ERROR );
	return( cryptStatusError( sSkip( stream, item->length ) ) ? \
			STATE_ERROR : STATE_NONE );
	}

/* Check a complex ASN.1 object.  In order to handle huge CRLs with tens or 
   hundreds of thousands of individual entries we can't use a fixed loop 
   failsafe iteration count but have to vary it based on the size of the 
   input data.  Luckily this situation is relatively easy to check for, it 
   only occurs at a nesting level of 6 (when we find the CRL entries) and we 
   only have to enable it when the data length is more than 30K since the 
   default FAILSAFE_ITERATIONS_LARGE will handle anything smaller than that */

CHECK_RETVAL_ENUM( STATE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STATE checkASN1( INOUT STREAM *stream, 
							 IN_LENGTH const long length, 
							 const BOOLEAN isIndefinite, 
							 IN_RANGE( 0, MAX_NESTING_LEVEL ) const int level, 
							 IN_ENUM_OPT( ASN1_STATE ) ASN1_STATE state, 
							 const BOOLEAN checkDataElements )
	{
	ASN1_ITEM item;
	ASN1_STATE newState;
	const long maxIterationCount = ( level == 6 && length > 30000 ) ? \
									length / 25 : FAILSAFE_ITERATIONS_LARGE;
	long localLength = length, lastPos = stell( stream );
	int iterationCount;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( ( level > 0 && level <= MAX_NESTING_LEVEL ) || \
			  ( level == 0 && length == LENGTH_MAGIC ) );
	REQUIRES( state >= STATE_NONE && state < STATE_ERROR );
	REQUIRES( ( isIndefinite && length == 0 ) || \
			  ( !isIndefinite && length >= 0 && length < MAX_INTLENGTH ) );
	REQUIRES( lastPos >= 0 && lastPos < MAX_INTLENGTH );

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( STATE_ERROR );

	for( iterationCount = 0;
		 ( newState = getItem( stream, &item ) ) == STATE_NONE && \
			iterationCount < maxIterationCount;
		 iterationCount++ )
		{
		/* If this is the top level (for which the length isn't known in
		   advance) and the item has a definite length, set the length to 
		   the item's length */
		if( level <= 0 && !item.indefinite )
			localLength = item.headerSize + item.length;

		/* If this is an EOC (tag == BER_RESERVED) for an indefinite item, 
		   we're done */
		if( isIndefinite && item.tag == BER_RESERVED )
			return( STATE_NONE );

		/* Check the object */
		if( !checkDataElements && item.length > 0 )
			{
			/* Shortcut to save a level of recursion, if we're not 
			   interested in the data elements (i.e. if we're just doing a
			   length check) and the item has a definite length, just skip 
			   over it and continue */
			if( cryptStatusError( sSkip( stream, item.length ) ) )
				return( STATE_ERROR );
			}
		else
			{
			newState = checkASN1Object( stream, &item, level + 1, state, 
										checkDataElements );
			if( newState < STATE_NONE || newState >= STATE_ERROR )
				return( STATE_ERROR );
			}

		/* If it's an indefinite-length object, we have to keep going until 
		   we find the EOC octets */
		if( isIndefinite )
			continue;

		/* If the outermost object was of indefinite length and we've come 
		   back to the top level, exit.  The isIndefinite flag won't be set
		   at this point because we can't know the length status before we
		   start, but it's implicitly indicated by finding a length of
		   LENGTH_MAGIC at the topmost level */
		if( level == 0 && length == LENGTH_MAGIC )
			return( STATE_NONE );

		/* Check whether we've reached the end of the current (definite-
		   length) object */
		localLength -= stell( stream ) - lastPos;
		lastPos = stell( stream );
		if( localLength < 0 || localLength >= MAX_INTLENGTH )
			return( STATE_ERROR );
		if( localLength == 0 )
			{
			/* We've reached the end of the object, we're done */
			return( newState );
			}

		/* We're reading more data from the current object, propagate any
		   state updates */
		state = newState;
		}
	ENSURES_S( iterationCount < maxIterationCount );

	return( ( newState == STATE_NONE ) ? STATE_NONE : STATE_ERROR );
	}

/* Check the encoding of a complete object and determine its length (qui 
   omnes insidias timet in nullas incidit - Syrus) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkObjectEncoding( IN_BUFFER( objectLength ) const void *objectPtr, 
						 IN_LENGTH const int objectLength )
	{
	STREAM stream;
	ASN1_STATE state;
	int length = DUMMY_INIT;

	assert( isReadPtr( objectPtr, objectLength ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_INTLENGTH );

	sMemConnect( &stream, objectPtr, objectLength );
	state = checkASN1( &stream, LENGTH_MAGIC, FALSE, 0, STATE_NONE, TRUE );
	if( state >= STATE_NONE && state < STATE_ERROR )
		length = stell( &stream );
	sMemDisconnect( &stream );
	return( ( state < STATE_NONE ) ? CRYPT_ERROR_INTERNAL : \
			( state >= STATE_ERROR ) ? CRYPT_ERROR_BADDATA : length );
	}

/* Recursively dig into an ASN.1 object as far as we need to to determine 
   its length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int findObjectLength( INOUT STREAM *stream, 
							 OUT_LENGTH_Z long *length, 
							 const BOOLEAN isLongObject )
	{
	const long startPos = stell( stream );
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_INTLENGTH );

	/* Clear return value */
	*length = 0;

	/* Try for a definite length (quousque tandem?) */
	status = readGenericObjectHeader( stream, &localLength, isLongObject );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's an indefinite-length object, burrow down into it to find its 
	   actual length */
	if( localLength == CRYPT_UNUSED )
		{
		ASN1_STATE state;

		/* We have to be a bit careful how we handle error reporting for 
		   this since we can run out of input and hit an underflow while 
		   we're in the process of burrowing through the data.  This is 
		   somewhat unfortunate since it leads to non-orthogonal behaviour 
		   because a definite length only requires checking a few bytes at 
		   the start of the data but an indefinite length requires 
		   processing the entire data quantity in order to determine where 
		   it ends */
		sseek( stream, startPos );
		state = checkASN1( stream, LENGTH_MAGIC, FALSE, 0, STATE_NONE, 
						   FALSE );
		if( state < STATE_NONE || state >= STATE_ERROR )
			{
			return( ( state < STATE_NONE ) ? \
						CRYPT_ERROR_INTERNAL : \
					( sGetStatus( stream ) == CRYPT_ERROR_UNDERFLOW ) ? \
						CRYPT_ERROR_UNDERFLOW : \
						CRYPT_ERROR_BADDATA );
			}
		localLength = stell( stream ) - startPos;
		}
	else
		{
		/* It's a definite-length object, add the size of the tag+length */
		localLength += stell( stream ) - startPos;
		}

	/* If it's not a long object, make sure that the length is within bounds.  
	   We have to do this explicitly here because indefinite-length objects
	   can be arbitrarily large so the length isn't checked as it is for
	   readGenericHoleI() */
	if( !isLongObject && localLength > 32767L )
		return( CRYPT_ERROR_OVERFLOW );
	*length = localLength;
	return( sseek( stream, startPos ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getStreamObjectLength( INOUT STREAM *stream, OUT_LENGTH_Z int *length )
	{
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	/* Clear return value */
	*length = 0;

	status = findObjectLength( stream, &localLength, FALSE );
	if( cryptStatusOK( status ) )
		*length = localLength;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getLongStreamObjectLength( INOUT STREAM *stream, 
							   OUT_LENGTH_Z long *length )
	{
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	/* Clear return value */
	*length = 0;

	status = findObjectLength( stream, &localLength, TRUE );
	if( cryptStatusOK( status ) )
		*length = localLength;
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getObjectLength( IN_BUFFER( objectLength ) const void *objectPtr, 
					 IN_LENGTH const int objectLength, 
					 OUT_LENGTH_Z int *length )
	{
	STREAM stream;
	long localLength = DUMMY_INIT;
	int status;

	assert( isReadPtr( objectPtr, objectLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( objectLength > 0 && objectLength < MAX_INTLENGTH );

	/* Clear return value */
	*length = 0;

	sMemConnect( &stream, objectPtr, objectLength );
	status = findObjectLength( &stream, &localLength, FALSE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	*length = localLength;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getLongObjectLength( IN_BUFFER( objectLength ) const void *objectPtr, 
						 IN_LENGTH const long objectLength,
						 OUT_LENGTH_Z long *length )
	{
	STREAM stream;
	long localLength;
	int status;

	assert( isReadPtr( objectPtr, objectLength ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES( objectLength > 0 && objectLength < MAX_INTLENGTH );

	/* Clear return value */
	*length = 0;

	sMemConnect( &stream, objectPtr, objectLength );
	status = findObjectLength( &stream, &localLength, TRUE );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		*length = localLength;
	return( status );
	}
