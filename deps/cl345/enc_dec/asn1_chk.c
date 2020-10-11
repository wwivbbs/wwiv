/****************************************************************************
*																			*
*						   ASN.1 Checking Routines							*
*						Copyright Peter Gutmann 1992-2017					*
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
   actual length once we start parsing the object.  The MAX_BUFFER_SIZE - 1
   value means that it's as large as possible while still passing the 
   parameter sanity checks */

#define LENGTH_MAGIC		( MAX_BUFFER_SIZE - 1 )

/* Current parse state.  This is used to check for potential BIT STRING and
   OCTET STRING targets for OCTET/BIT STRING holes, which are always
   preceded by an AlgorithmIdentifier.  In order to detect these without
   having to know every imaginable AlgorithmIdentifier OID, we check for the
   following sequence of events:

	SEQUENCE {			-- ASN1_STATE_SEQUENCE
		OID,			-- ASN1_STATE_HOLE_OID
		NULL			-- ASN1_STATE_NULL
		},
						-- ASN1_STATE_CHECK_HOLE_BITSTRING
	BIT STRING | ...

	SEQUENCE {			-- ASN1_STATE_SEQUENCE
		OID,			-- ASN1_STATE_HOLE_OID
		[ BOOLEAN OPT,	-- ASN1_STATE_BOOLEAN ]
						-- ASN1_STATE_CHECK_HOLE_OCTETSTRING
		OCTET STRING | ...

   Once we reach any of the ASN1_STATE_CHECK_HOLE_* states, if we hit a 
   BIT STRING or OCTET STRING as the next item then we try and locate 
   encapsulated content within it.
   
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

   for public keys and:

	SEQUENCE {
		OID,
		NULL
		},
	BIT STRING ...

   for signatures (with an additional complication for DLP keys that the 
   NULL for the public-key parameters is replaced by a SEQUENCE { ... }
   containing the public parameters).  This means that there's little point 
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
	ASN1_STATE_NONE,

	/* States corresponding to ASN.1 primitives */
	ASN1_STATE_BOOLEAN, ASN1_STATE_NULL, ASN1_STATE_OID, ASN1_STATE_SEQUENCE,

	/* States corresponding to different parts of a SEQUENCE { OID, optional,
	   potential OCTET/BIT STRING } sequence */
	ASN1_STATE_HOLE_OID, /*ASN1_STATE_CHECK_HOLE_BITSTRING,*/ 
	ASN1_STATE_CHECK_HOLE_OCTETSTRING,

	/* Error state */
	ASN1_STATE_ERROR,
	
	ASN1_STATE_LAST
	} ASN1_STATE;

/* Structure to hold info on an ASN.1 item */

typedef struct {
	int tag;				/* Tag */
	long length;			/* Data length */
	int headerSize;			/* Size of tag+length */
	BOOLEAN isIndefinite;	/* Item has indefinite length */
	} ASN1_ITEM;

#ifdef USE_INT_ASN1

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get an ASN.1 object's tag and length */

CHECK_RETVAL_ENUM( ASN1_STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getItem( INOUT STREAM *stream, 
					OUT ASN1_ITEM *item )
	{
	const long offset = stell( stream );
	long length;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( item, sizeof( ASN1_ITEM ) ) );

	REQUIRES_EXT( isIntegerRange( offset ), ASN1_STATE_ERROR );

	/* Clear return value */
	memset( item, 0, sizeof( ASN1_ITEM ) );

	/* Read the tag.  We can't use peekTag() for this since we may be 
	   reading EOC octets, which would be rejected by peekTag() */
	status = item->tag = sPeek( stream );
	if( cryptStatusError( status ) )
		return( ASN1_STATE_ERROR );
	if( item->tag == BER_EOC )
		{
		/* It looks like EOC octets, make sure that they're in order */
		status = checkEOC( stream );
		if( cryptStatusError( status ) )
			return( ASN1_STATE_ERROR );
		if( status == TRUE )
			{
			item->headerSize = 2;
			return( ASN1_STATE_NONE );
			}
		}

	/* If it's a NULL, special-case the read since the length will be 
	   zero */
	if( item->tag == BER_NULL )
		{
		int ch;

		( void ) sgetc( stream );	/* Skip tag */
		status = ch = sgetc( stream );
		if( cryptStatusError( status ) || ( ch != 0 ) )
			return( ASN1_STATE_ERROR );
		item->headerSize = 2;

		return( ASN1_STATE_NONE );
		}

	/* Make sure that the tag is at least vaguely valid before we try the 
	   readLongGenericHole() */
	if( item->tag <= 0 || item->tag >= MAX_TAG )
		return( ASN1_STATE_ERROR );

	/* It's not an EOC, read the tag and length as a generic hole */
	status = readLongGenericHoleZ( stream, &length, item->tag );
	if( cryptStatusError( status ) )
		return( ASN1_STATE_ERROR );
	item->headerSize = stell( stream ) - offset;
	if( length == CRYPT_UNUSED )
		item->isIndefinite = TRUE;
	else
		{
		/* If the length that we've just read is larger than the object 
		   itself, it's an error */
		if( length < 0 || length >= MAX_BUFFER_SIZE - 1 ) 
			return( ASN1_STATE_ERROR );

		item->length = length;
		}
	return( ASN1_STATE_NONE );
	}

/* Check whether an ASN.1 object is encapsulated inside an OCTET STRING or
   BIT STRING.  After performing the various checks we have to explicitly
   clear the stream error state since the probing for valid data could have
   set the error indicator if nothing valid was found.

   Note that this is a no-biased test since the best that we can do is guess 
   at the presence of encapsulated content and we can't risk rejecting valid
   content based on false positives.  This means that unfortunately 
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
	int tag, innerLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_B( length > 0 && length < MAX_BUFFER_SIZE );
	REQUIRES_B( isBitstring == TRUE || isBitstring == FALSE );
	REQUIRES_B( state >= ASN1_STATE_NONE && state < ASN1_STATE_ERROR );
	REQUIRES_B( isIntegerRange( streamPos ) );

	/* Make sure that the tag is in order */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
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
			{
			isEncapsulated = FALSE;
			}

		sClearError( stream );
		sseek( stream, streamPos );
		return( isEncapsulated );
		}

	/* An OCTET STRING is more complex.  This could encapsulate any of:

		BIT STRING: keyUsage, crlReason, Netscape certType, must be
			<= 16 bits and a valid bitstring.
		GeneralisedTime: invalidityDate: Valid length for a GeneralisedTime,
			followed by two numeric digits.
		IA5String: Netscape extensions, perform an approximate length range 
			check and verify that the first two characters are IA5.
		INTEGER: deltaCRLIndicator, crlNumber, must be <= 16 bits and a 
			positive integer.
		OCTET STRING: keyID, again the most that we can do is perform an
			approximate length range check.
		OID: holdInstructionCode, again just an approximate length range 
			check.
		SEQUENCE: most extensions, a bit difficult to check but again we can 
			make sure that the length is right for strict encapsulation (via
			the check above) and that the inner content is a valid ASN.1
			object */
	switch( tag )
		{
		case BER_BITSTRING:
			if( innerLength < 0 || innerLength > 2 )
				isEncapsulated = FALSE;
			else
				{
				const int ch = sgetc( stream );

				if( ch < 0 || ch > 7 )
					isEncapsulated = FALSE;
				}
			break;

		case BER_TIME_GENERALIZED:
			if( innerLength != 15 )
				isEncapsulated = FALSE;
			else
				{
				const int ch1 = sgetc( stream );
				const int ch2 = sgetc( stream );

				if( !isDigit( ch1 ) || !isDigit( ch2 ) )
					isEncapsulated = FALSE;
				}
			break;

		case BER_INTEGER:
			if( innerLength < 0 || innerLength > 2 )
				isEncapsulated = FALSE;
			else
				{
				const int ch = sgetc( stream );

				if( ch < 0 )
					isEncapsulated = FALSE;
				}
			break;

		case BER_STRING_IA5:
			if( innerLength < 2 || innerLength > 256 )
				isEncapsulated = FALSE;
			else
				{
				const int ch1 = sgetc( stream );
				const int ch2 = sgetc( stream );

				if( !isPrint( ch1 ) || !isPrint( ch2 ) )
					isEncapsulated = FALSE;
				}
			break;

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
			status = tag = peekTag( stream );
			if( cryptStatusError( status ) )
				isEncapsulated = FALSE;
			else
				{
				status = readGenericHole( stream, &innerLength, 1, 
										  DEFAULT_TAG );
				if( cryptStatusError( status ) )
					isEncapsulated = FALSE;
				}
			break;

		default:
			isEncapsulated = FALSE;
		}
	sClearError( stream );
	sseek( stream, streamPos );
	return( isEncapsulated );
	}

/****************************************************************************
*																			*
*							Check Primitive ASN.1 Objects					*
*																			*
****************************************************************************/

/* Check a primitive ASN.1 object */

CHECK_RETVAL_ENUM( ASN1_STATE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STATE checkASN1( INOUT STREAM *stream,
							 IN_DATALENGTH_Z const long length,
							 const BOOLEAN isIndefinite,
							 IN_RANGE( 0, MAX_NESTING_LEVEL ) const int level,
							 IN_ENUM_OPT( ASN1_STATE ) ASN1_STATE state,
							 IN_ENUM_OPT( CHECK_ENCODING ) \
								const CHECK_ENCODING_TYPE checkType,
							 const BOOLEAN checkDataElements );

CHECK_RETVAL_ENUM( ASN1_STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ASN1_STATE checkPrimitive( INOUT STREAM *stream, const ASN1_ITEM *item,
								  IN_RANGE( 1, MAX_NESTING_LEVEL ) const int level, 
								  IN_ENUM_OPT( ASN1_STATE ) const ASN1_STATE state,
								  IN_ENUM_OPT( CHECK_ENCODING ) \
										const CHECK_ENCODING_TYPE checkType )
	{
	int length = ( int ) item->length, ch, i, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( item, sizeof( ASN1_ITEM ) ) );
	
	REQUIRES_EXT( level > 0 && level <= MAX_NESTING_LEVEL, ASN1_STATE_ERROR );
	REQUIRES_EXT( state >= ASN1_STATE_NONE && state < ASN1_STATE_ERROR, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( isEnumRangeOpt( checkType, CHECK_ENCODING ), 
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( isIntegerRange( item->length ), ASN1_STATE_ERROR );
	REQUIRES_EXT( length >= 0 && length < MAX_BUFFER_SIZE, ASN1_STATE_ERROR );

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( ASN1_STATE_ERROR );

	/* Check for a zero-length item.  In theory only NULL and EOC elements 
	   (BER_RESERVED) are allowed to have a zero length */
	if( length <= 0 && item->tag != BER_NULL && item->tag != BER_RESERVED )
		{
		/* There are other cases where zero lengths can occur:

			OCTET STRING: When using PBKDF2 as a general-purpose KDF (for
				CMS authEnc encryption), the salt is omitted, which means 
				that it's encoded as a zero-length value.  It would be 
				better to perform context checking to see whether it's in 
				the right location but the most information that we have at 
				this point is the nesting level, we allow a zero-length 
				OCTET STRING at a nesting level between 16 and 30, which 
				occurs for CMS authEnveloped data */
		if( !( item->tag == BER_OCTETSTRING && level >= 16 && level <= 30 ) )
			return( ASN1_STATE_ERROR );

		/* Since this is a zero-length element, there's nothing further to
		   check */
		return( ASN1_STATE_NONE );
		}

	/* Perform a general check that everything is OK.  We don't check for 
	   invalid content except where it would impede decoding of the data in
	   order to avoid failing on all of the broken certificates out there */
	switch( item->tag )
		{
		case BER_BOOLEAN:
			if( length != 1 )
				return( ASN1_STATE_ERROR );
			return( cryptStatusError( sgetc( stream ) ) ? \
					ASN1_STATE_ERROR : ASN1_STATE_BOOLEAN );

		case BER_INTEGER:
		case BER_ENUMERATED:
			if( !isShortIntegerRangeNZ( length ) || \
				cryptStatusError( \
						sSkip( stream, length, MAX_INTLENGTH_SHORT ) ) )
				{
				return( ASN1_STATE_ERROR );
				}
			return( ASN1_STATE_NONE );

		case BER_BITSTRING:
			if( length < 2 )
				return( ASN1_STATE_ERROR );

			/* Check the number of unused bits */
			ch = sgetc( stream );
			length--;
			if( !isShortIntegerRange( length ) || ch < 0 || ch > 7 )
				{
				/* Invalid number of unused bits */
				return( ASN1_STATE_ERROR );
				}

			/* If it's short enough to be a bit flag, it's just a sequence 
			   of bits */
			if( length <= 4 )
				{
				if( length > 0 && \
					cryptStatusError( \
						sSkip( stream, length, MAX_INTLENGTH_SHORT ) ) )
					{
					return( ASN1_STATE_ERROR );
					}
				return( ASN1_STATE_NONE );
				}
			STDC_FALLTHROUGH;

		case BER_OCTETSTRING:
			{
			const BOOLEAN isBitstring = ( item->tag == BER_BITSTRING ) ? \
										TRUE : FALSE;
			BOOLEAN checkEncaps = ( checkType == CHECK_ENCODING_ENCAPS ) ? \
								  TRUE : FALSE;

			/* If we're checking for the presence of hole encodings at a 
			   certain level, things get a bit more tricky because we want
			   to try and avoid false positives as much as possible.  The
			   following somewhat ugly heuristics enable hole-encoding 
			   checking under conditions where they occur in data that we 
			   can encounter */
			if( checkType == CHECK_ENCODING_SEMIENCAPS )
				{
				if( isBitstring )
					{
					/* SCEP, CMP, and RTCS have BIT STRINGs in certificate
					   signatures 7 levels down */
					if( level >= 7 )
						checkEncaps = TRUE;
					}
				else
					{
					/* CMS (and by extension SCEP) have OCTET STRING 
					   encrypted content 5 levels down */
					if( level >= 5 && length > 256 )
						checkEncaps = TRUE;
					else
						{
						/* SCEP and CMP have OCTET STRINGs in certificate
						   attributes 9 and 12 levels down */
						if( level >= 9 )
							checkEncaps = TRUE;
						}
					}
				}

			/* Check to see whether an OCTET STRING or BIT STRING hole is 
			   allowed at this point.  An OCTET STRING must be preceded by 
			   { SEQ, OID, {BOOLEAN} }), a BIT STRING hole can't easily be 
			   checked based on what precedes it (see the comment for 
			   ASN1_STATE) but is checked based on the contents of the hole */
			if( checkEncaps && \
				( isBitstring || state == ASN1_STATE_HOLE_OID || \
								 state == ASN1_STATE_CHECK_HOLE_OCTETSTRING ) && \
				checkEncapsulation( stream, length, isBitstring, state ) )
				{
				ASN1_STATE encapsState;

				/* It looks like it's a hole encoding, handle it as a 
				   constructed item */
				encapsState = checkASN1( stream, length, item->isIndefinite,
										 level + 1, ASN1_STATE_NONE, checkType, 
										 TRUE );
				return( ( encapsState < ASN1_STATE_NONE || \
						  encapsState >= ASN1_STATE_ERROR ) ? \
						ASN1_STATE_ERROR : ASN1_STATE_NONE );
				}

			/* Skip the data */
			if( length <= 0 )
				return( ASN1_STATE_ERROR );
			return( cryptStatusError( \
						sSkip( stream, length, SSKIP_MAX ) ) ? \
					ASN1_STATE_ERROR : ASN1_STATE_NONE );
			}

		case BER_OBJECT_IDENTIFIER:
			if( length < 3 )
				return( ASN1_STATE_ERROR );
			if( length > MAX_OID_SIZE - 2 )
				{
				/* Microsoft invented their own gibberish OIDs off the arc 
				   1 3 6 1 4 1 311 21 8, beyond which the contents are just 
				   noise.  These OIDs are technically valid (in the usual 
				   sense that an MPEG of a cat encoded in there is valid) 
				   but look like garbage, so they'd normally get rejected by 
				   the sanity-checking code.  In order to be able to process 
				   Microsoft-generated certificates we kludge around them by 
				   performing an explicit check for this particular arc and 
				   not rejecting the input if it's one of these */
				if( state == ASN1_STATE_SEQUENCE && length < 48 )
					{
					BYTE oidBuffer[ 48 + 8 ];
					int status;

					status = sread( stream, oidBuffer, length );
					if( cryptStatusError( status ) )
						return( ASN1_STATE_ERROR );
					if( memcmp( oidBuffer, 
								"\x2B\x06\x01\x04\x01\x82\x37\x15\x08", 9 ) )
						return( ASN1_STATE_ERROR );

					/* It's a Microsoft gibberish OID, report it as a 
					   standard OID */
					return( ASN1_STATE_OID );
					}

				/* Total OID size (including tag and length, since they're 
				   treated as a blob) should be less than a sane limit */
				return( ASN1_STATE_ERROR );
				}
			if( !isShortIntegerRangeNZ( length ) )
				return( ASN1_STATE_ERROR );
			return( cryptStatusError( \
						sSkip( stream, length, MAX_INTLENGTH_SHORT ) ) ? \
					ASN1_STATE_ERROR : ASN1_STATE_OID );

		case BER_RESERVED:			/* EOC */
			return( ASN1_STATE_NONE );

		case BER_NULL:
			return( ASN1_STATE_NULL );

		case BER_STRING_BMP:
		case BER_STRING_GENERAL:	/* Produced by Entrust software */
		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_NUMERIC:
		case BER_STRING_PRINTABLE:
		case BER_STRING_T61:
		case BER_STRING_UTF8:
			if( length <= 0 )
				return( ASN1_STATE_ERROR );
			return( cryptStatusError( \
						sSkip( stream, length, SSKIP_MAX ) ) ? \
					ASN1_STATE_ERROR : ASN1_STATE_NONE );

		case BER_TIME_UTC:
		case BER_TIME_GENERALIZED:
			if( item->tag == BER_TIME_GENERALIZED )
				{
				if( length != 15 )
					return( ASN1_STATE_ERROR );
				}
			else
				{
				if( length != 13 )
					return( ASN1_STATE_ERROR );
				}
			LOOP_MED( i = 0, i < length - 1, i++ )
				{
				ch = sgetc( stream );
				if( cryptStatusError( ch ) || !isDigit( ch ) )
					return( ASN1_STATE_ERROR );
				}
			ENSURES_EXT( LOOP_BOUND_OK, ASN1_STATE_ERROR );
			if( sgetc( stream ) != 'Z' )
				return( ASN1_STATE_ERROR );
			return( ASN1_STATE_NONE );

		default:
			/* Disallowed or unrecognised primitive */
			return( ASN1_STATE_ERROR );
		}

	retIntError_Ext( ASN1_STATE_ERROR );
	}

/****************************************************************************
*																			*
*							Check Complex ASN.1 Objects						*
*																			*
****************************************************************************/

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
   call hierarchy, but that's mostly just an artefact of the way that it's 
   diagrammed here */

CHECK_RETVAL_ENUM( ASN1_STATE ) STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ASN1_STATE checkASN1Object( INOUT STREAM *stream, 
								   const ASN1_ITEM *item,
								   IN_RANGE( 1, MAX_NESTING_LEVEL ) \
										const int level, 
								   IN_ENUM_OPT( ASN1_STATE ) \
										const ASN1_STATE state,
								   IN_ENUM_OPT( CHECK_ENCODING ) \
										const CHECK_ENCODING_TYPE checkType,
								   const BOOLEAN checkDataElements )
	{
	ASN1_STATE newState;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( item, sizeof( ASN1_ITEM ) ) );

	REQUIRES_EXT( level > 0 && level <= MAX_NESTING_LEVEL, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( state >= ASN1_STATE_NONE && state < ASN1_STATE_ERROR, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( isEnumRangeOpt( checkType, CHECK_ENCODING ), 
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( ( checkDataElements == TRUE && item->headerSize ) || \
				  checkDataElements == FALSE, \
				  ASN1_STATE_ERROR );
				  /* Definite-length items are handled by the caller for a 
				     length-only check */

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( ASN1_STATE_ERROR );

	/* Check the contents for validity.  A straight data-length check doesn't 
	   check nested elements since all it cares about is finding the overall 
	   length with as little effort as possible */
	if( ( item->tag & BER_CLASS_MASK ) == BER_UNIVERSAL )
		{
		/* If it's constructed, parse the nested object(s) */
		if( ( item->tag & BER_CONSTRUCTED_MASK ) == BER_CONSTRUCTED )
			{
			/* Special-case for zero-length SEQUENCE/SET */
			if( item->length <= 0 && !item->isIndefinite )
				return( ASN1_STATE_NONE );

			return( checkASN1( stream, item->length, item->isIndefinite,
							   level + 1, ( item->tag == BER_SEQUENCE ) ? \
									ASN1_STATE_SEQUENCE : ASN1_STATE_NONE,
							   checkType, checkDataElements ) );
			}

		/* It's primitive, check the primitive element with optional state
		   update: SEQ + OID -> HOLE_OID; OID + { NULL | BOOLEAN } -> 
		   HOLE_BITSTRING/HOLE_OCTETSTRING */
		newState = checkPrimitive( stream, item, level + 1, state, checkType );
		if( newState < ASN1_STATE_NONE || newState >= ASN1_STATE_ERROR )
			return( ASN1_STATE_ERROR );
		if( state == ASN1_STATE_SEQUENCE && newState == ASN1_STATE_OID )
			return( ASN1_STATE_HOLE_OID );
		if( state == ASN1_STATE_HOLE_OID )
			{
#if 0	/* See comment at start */
			if( newState == ASN1_STATE_NULL )
				return( ASN1_STATE_CHECK_HOLE_BITSTRING );
#endif /* 0 */
			if( newState == ASN1_STATE_BOOLEAN )
				return( ASN1_STATE_CHECK_HOLE_OCTETSTRING );
			}
		return( ASN1_STATE_NONE );
		}

	/* Zero-length objects are usually an error, however PKCS #10 has an
	   attribute-encoding ambiguity that produces zero-length tagged 
	   extensions and OCSP has its braindamaged context-specific tagged 
	   NULLs so we don't complain about them if they have low-valued 
	   context-specific tags */
	if( item->length <= 0 && !item->isIndefinite )
		{
		return( ( ( item->tag & BER_CLASS_MASK ) == BER_CONTEXT_SPECIFIC && \
				  EXTRACT_CTAG( item->tag ) <= 3 ) ? \
				ASN1_STATE_NONE : ASN1_STATE_ERROR );
		}

	ENSURES_EXT( item->length > 0 || item->isIndefinite, ASN1_STATE_ERROR );

	/* If it's constructed, parse the nested object(s) */
	if( ( item->tag & BER_CONSTRUCTED_MASK ) == BER_CONSTRUCTED )
		{
		newState = checkASN1( stream, item->length, item->isIndefinite,
							  level + 1, ASN1_STATE_NONE, checkType,
							  checkDataElements );
		return( ( newState < ASN1_STATE_NONE || newState >= ASN1_STATE_ERROR ) ? \
				ASN1_STATE_ERROR : ASN1_STATE_NONE );
		}

	/* It's a context-specific tagged item that could contain anything, just 
	   skip it */
	if( ( item->length <= 0 ) || \
		( ( item->tag & BER_CLASS_MASK ) != BER_CONTEXT_SPECIFIC ) )
		{
		return( ASN1_STATE_ERROR );
		}
	return( cryptStatusError( \
				sSkip( stream, item->length, SSKIP_MAX ) ) ? \
			ASN1_STATE_ERROR : ASN1_STATE_NONE );
	}

/* Check a complex ASN.1 object.  In order to handle huge CRLs with tens or 
   hundreds of thousands of individual entries we can't use a fixed loop 
   failsafe iteration count but have to vary it based on the size of the 
   input data.  Luckily this situation is relatively easy to check for, it 
   only occurs at a nesting level of 6 (when we find the CRL entries) and we 
   only have to enable it when the data length is more than 30K since the 
   default FAILSAFE_ITERATIONS_LARGE will handle anything smaller than that */

CHECK_RETVAL_ENUM( ASN1_STATE ) STDC_NONNULL_ARG( ( 1 ) ) \
static ASN1_STATE checkASN1( INOUT STREAM *stream, 
							 IN_DATALENGTH_Z const long length, 
							 const BOOLEAN isIndefinite, 
							 IN_RANGE( 0, MAX_NESTING_LEVEL ) const int level, 
							 IN_ENUM_OPT( ASN1_STATE ) ASN1_STATE state, 
							 IN_ENUM_OPT( CHECK_ENCODING ) \
								const CHECK_ENCODING_TYPE checkType,
							 const BOOLEAN checkDataElements )
	{
	ASN1_ITEM item;
	ASN1_STATE newState DUMMY_INIT;
	const long maxIterationCount = ( level == 6 && length > 30000 ) ? \
									 length / 25 : FAILSAFE_ITERATIONS_LARGE - 1;
	long localLength = length, lastPos = stell( stream );
	int iterationCount, LOOP_ITERATOR;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES_EXT( ( level > 0 && level <= MAX_NESTING_LEVEL ) || \
				  ( level == 0 && length == LENGTH_MAGIC ), \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( isIndefinite == TRUE || isIndefinite == FALSE, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( ( isIndefinite && length == 0 ) || \
				  ( !isIndefinite && length >= 0 && length < MAX_BUFFER_SIZE ),
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( state >= ASN1_STATE_NONE && state < ASN1_STATE_ERROR, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( isEnumRangeOpt( checkType, CHECK_ENCODING ), 
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( checkDataElements == TRUE || checkDataElements == FALSE, \
				  ASN1_STATE_ERROR );
	REQUIRES_EXT( lastPos >= 0 && lastPos < MAX_BUFFER_SIZE, \
				  ASN1_STATE_ERROR );

	/* Make sure that we're not processing suspiciosly deeply nested data */
	if( level >= MAX_NESTING_LEVEL )
		return( ASN1_STATE_ERROR );

	LOOP_MAX( iterationCount = 0,
			  ( newState = getItem( stream, &item ) ) == ASN1_STATE_NONE && \
				iterationCount < maxIterationCount,
			  iterationCount++ )
		{
		/* If this is the top level (for which the length isn't known in
		   advance) and the item has a definite length, set the length to 
		   the item's length */
		if( level <= 0 && !item.isIndefinite )
			localLength = item.headerSize + item.length;

		/* If this is an EOC (tag == BER_RESERVED) for an indefinite item, 
		   we're done */
		if( isIndefinite && item.tag == BER_RESERVED )
			return( ASN1_STATE_NONE );

		/* Check the object */
		if( !checkDataElements && !item.isIndefinite )
			{
			/* Shortcut to save a level of recursion, if we're not 
			   interested in the data elements (i.e. if we're just doing a
			   length check) and the item has a definite length, just skip 
			   over it and continue */
			if( item.length > 0 && \
				cryptStatusError( sSkip( stream, item.length, SSKIP_MAX ) ) )
				{
				return( ASN1_STATE_ERROR );
				}
			}
		else
			{
			newState = checkASN1Object( stream, &item, level + 1, state, 
										checkType, checkDataElements );
			if( newState < ASN1_STATE_NONE || newState >= ASN1_STATE_ERROR )
				return( ASN1_STATE_ERROR );
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
			return( ASN1_STATE_NONE );

		/* Check whether we've reached the end of the current (definite-
		   length) object */
		localLength -= stell( stream ) - lastPos;
		lastPos = stell( stream );
		if( localLength < 0 || localLength >= MAX_BUFFER_SIZE )
			return( ASN1_STATE_ERROR );
		if( localLength == 0 )
			{
			/* We've reached the end of the object, we're done */
			return( newState );
			}

		/* We're reading more data from the current object, propagate any
		   state updates */
		state = newState;
		}
	ENSURES_EXT( LOOP_BOUND_OK, ASN1_STATE_ERROR );
	if( iterationCount >= maxIterationCount )
		{
		assert_nofuzz( DEBUG_WARN );
		return( ASN1_STATE_ERROR );
		}

	return( ( newState == ASN1_STATE_NONE ) ? \
			ASN1_STATE_NONE : ASN1_STATE_ERROR );
	}

/****************************************************************************
*																			*
*								ASN.1 Check Interface						*
*																			*
****************************************************************************/

/* Check the encoding of a complete object and determine its length (qui 
   omnes insidias timet in nullas incidit - Pubilius Syrus) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkEncoding( IN_BUFFER( objectLength ) const void *objectPtr, 
						  IN_DATALENGTH const int objectLength,
						  OUT_OPT_DATALENGTH_Z int *objectActualSize,
						  IN_ENUM_OPT( CHECK_ENCODING ) \
								const CHECK_ENCODING_TYPE checkType )
	{
	STREAM stream;
	ASN1_STATE state;

	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	assert( objectActualSize == NULL || \
			isWritePtr( objectActualSize, sizeof( int ) ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );
	REQUIRES( isEnumRangeOpt( checkType, CHECK_ENCODING ) );

	/* Clear return value */
	if( objectActualSize != NULL )
		*objectActualSize = 0;

	sMemConnect( &stream, objectPtr, objectLength );
	state = checkASN1( &stream, LENGTH_MAGIC, FALSE, 0, ASN1_STATE_NONE, 
					   checkType, TRUE );
	if( state >= ASN1_STATE_NONE && state < ASN1_STATE_ERROR )
		{
		/* We've processed the object, return its length if required */
		if( objectActualSize != NULL )
			*objectActualSize = stell( &stream );
		}
	sMemDisconnect( &stream );
	return( ( state < ASN1_STATE_NONE ) ? CRYPT_ERROR_INTERNAL : \
			( state >= ASN1_STATE_ERROR ) ? CRYPT_ERROR_BADDATA : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkObjectEncoding( IN_BUFFER( objectLength ) const void *objectPtr, 
						 IN_DATALENGTH const int objectLength )
	{
	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

	/* Check an ASN.1 object in the absence of hole encodings.  This is 
	   possible because non-certificate objects use the CMS form for 
	   attributes, SEQUENCE { OID, SET {} } rather than the certificate 
	   form, SEQUENCE { OID, OCTET STRING {} } */
	return( checkEncoding( objectPtr, objectLength, NULL, 
						   CHECK_ENCODING_NONE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int checkObjectEncodingLength( IN_BUFFER( objectLength ) \
									const void *objectPtr, 
							   IN_DATALENGTH const int objectLength,
							   OUT_DATALENGTH_Z int *objectActualSize )
	{
	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	assert( isWritePtr( objectActualSize, sizeof( int ) ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

	/* Clear return value */
	*objectActualSize = 0;

	/* Check an ASN.1 object in the absence of hole encodings in most cases.  
	   This is an awkward intersection between CHECK_ENCODING_NONE (for CMS 
	   objects) and CHECK_ENCODING_ENCAPS (for certificate objects) that 
	   occurs in things like PKI messaging, where a more general CMS object 
	   may have a certificate attached */
	return( checkEncoding( objectPtr, objectLength, objectActualSize, 
						   CHECK_ENCODING_SEMIENCAPS ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertObjectEncoding( IN_BUFFER( objectLength ) const void *objectPtr, 
							 IN_DATALENGTH const int objectLength )
	{
	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

	/* Check an ASN.1 object in the presence of OCTET STRING and BIT STRING 
	   holes */
	return( checkEncoding( objectPtr, objectLength, NULL, 
						   CHECK_ENCODING_ENCAPS ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int checkCertObjectEncodingLength( IN_BUFFER( objectLength ) \
										const void *objectPtr, 
								   IN_DATALENGTH const int objectLength,
								   OUT_DATALENGTH_Z int *objectActualSize )
	{
	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	assert( isWritePtr( objectActualSize, sizeof( int ) ) );
	
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

	/* Clear return value */
	*objectActualSize = 0;

	/* Check an ASN.1 object in the presence of OCTET STRING and BIT STRING 
	   holes */
	return( checkEncoding( objectPtr, objectLength, objectActualSize, 
						   CHECK_ENCODING_ENCAPS ) );
	}

/* Recursively dig into an ASN.1 object as far as we need to to determine 
   its length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int findObjectLength( INOUT STREAM *stream, 
							 OUT_DATALENGTH_Z long *length, 
							 const BOOLEAN isLongObject )
	{
	const long startPos = stell( stream );
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES( startPos >= 0 && startPos < MAX_BUFFER_SIZE );
	REQUIRES( isLongObject == TRUE || isLongObject == FALSE );

	/* Clear return value */
	*length = 0;

	/* Try for a definite length (quo usque tandem?) */
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
		state = checkASN1( stream, LENGTH_MAGIC, FALSE, 0, ASN1_STATE_NONE,
						   CHECK_ENCODING_NONE, FALSE );
		if( state < ASN1_STATE_NONE || state >= ASN1_STATE_ERROR )
			{
			return( ( state < ASN1_STATE_NONE ) ? \
						CRYPT_ERROR_INTERNAL : \
					( sGetStatus( stream ) == CRYPT_ERROR_UNDERFLOW ) ? \
						CRYPT_ERROR_UNDERFLOW : \
						CRYPT_ERROR_BADDATA );
			}
		localLength = stell( stream ) - startPos;
		}
	else
		{
		/* We've read the length information directly from the object rather
		   than calculating it ourselves, make sure that the it's within 
		   bounds */
		if( localLength > sMemDataLeft( stream ) )
			return( CRYPT_ERROR_UNDERFLOW );

		/* It's a definite-length object, add the size of the tag+length */
		localLength += stell( stream ) - startPos;
		}

	/* An object needs to consist of at least a tag and a length */
	if( !isIntegerRange( localLength ) || localLength <= 1 + 1 )
		return( CRYPT_ERROR_BADDATA );

	/* If it's not a long object, make sure that the length is within bounds.  
	   We have to do this explicitly here because indefinite-length objects
	   can be arbitrarily large so the length isn't checked as it is for
	   readGenericHoleI() */
	if( !isLongObject && localLength > MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_OVERFLOW );
	*length = localLength;
	return( sseek( stream, startPos ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getStreamObjectLength( INOUT STREAM *stream, 
						   OUT_DATALENGTH_Z int *length )
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
							   OUT_DATALENGTH_Z long *length )
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
					 IN_DATALENGTH const int objectLength, 
					 OUT_DATALENGTH_Z int *length )
	{
	STREAM stream;
	long localLength DUMMY_INIT;
	int status;

	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

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
						 IN_DATALENGTH const long objectLength,
						 OUT_DATALENGTH_Z long *length )
	{
	STREAM stream;
	long localLength;
	int status;

	assert( isReadPtrDynamic( objectPtr, objectLength ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );

	/* Clear return value */
	*length = 0;

	sMemConnect( &stream, objectPtr, objectLength );
	status = findObjectLength( &stream, &localLength, TRUE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	*length = localLength;
	return( CRYPT_OK );
	}
#endif /* USE_INT_ASN1 */
