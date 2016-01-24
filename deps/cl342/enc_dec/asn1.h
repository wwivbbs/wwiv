/****************************************************************************
*																			*
*						  ASN.1 Constants and Structures					*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#ifndef _ASN1_DEFINED

#define _ASN1_DEFINED

#include <time.h>
#if defined( INC_ALL )
  #include "stream.h"
#else
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*						BER/DER Constants and Macros						*
*																			*
****************************************************************************/

/* Definitions for the ISO 8825:1990 Basic Encoding Rules */

/* Tag class */

#define BER_UNIVERSAL			0x00
#define BER_APPLICATION			0x40
#define BER_CONTEXT_SPECIFIC	0x80
#define BER_PRIVATE				0xC0

/* Whether the encoding is constructed or primitive */

#define BER_CONSTRUCTED			0x20
#define BER_PRIMITIVE			0x00

/* The ID's for universal tag numbers 0-31.  Tag number 0 is reserved for
   encoding the end-of-contents value when an indefinite-length encoding
   is used */

enum { BER_ID_RESERVED, BER_ID_BOOLEAN, BER_ID_INTEGER, BER_ID_BITSTRING,
	   BER_ID_OCTETSTRING, BER_ID_NULL, BER_ID_OBJECT_IDENTIFIER,
	   BER_ID_OBJECT_DESCRIPTOR, BER_ID_EXTERNAL, BER_ID_REAL,
	   BER_ID_ENUMERATED, BER_ID_EMBEDDED_PDV, BER_ID_STRING_UTF8, BER_ID_13,
	   BER_ID_14, BER_ID_15, BER_ID_SEQUENCE, BER_ID_SET,
	   BER_ID_STRING_NUMERIC, BER_ID_STRING_PRINTABLE, BER_ID_STRING_T61,
	   BER_ID_STRING_VIDEOTEX, BER_ID_STRING_IA5, BER_ID_TIME_UTC,
	   BER_ID_TIME_GENERALIZED, BER_ID_STRING_GRAPHIC, BER_ID_STRING_ISO646,
	   BER_ID_STRING_GENERAL, BER_ID_STRING_UNIVERSAL, BER_ID_29,
	   BER_ID_STRING_BMP, BER_ID_LAST };

/* The encodings for the universal types */

#define BER_EOC					0	/* Pseudo-type for first EOC octet */
#define BER_RESERVED			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_RESERVED )
#define BER_BOOLEAN				( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_BOOLEAN )
#define BER_INTEGER				( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_INTEGER )
#define BER_BITSTRING			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_BITSTRING )
#define BER_OCTETSTRING			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_OCTETSTRING )
#define BER_NULL				( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_NULL )
#define BER_OBJECT_IDENTIFIER	( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_OBJECT_IDENTIFIER )
#define BER_OBJECT_DESCRIPTOR	( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_OBJECT_DESCRIPTOR )
#define BER_EXTERNAL			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_EXTERNAL )
#define BER_REAL				( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_REAL )
#define BER_ENUMERATED			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_ENUMERATED )
#define BER_EMBEDDED_PDV		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_EMBEDDED_PDV )
#define BER_STRING_UTF8			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_UTF8 )
#define BER_13					( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_13 )
#define BER_14					( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_14 )
#define BER_15					( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_15 )
#define BER_SEQUENCE			( BER_UNIVERSAL | BER_CONSTRUCTED | BER_ID_SEQUENCE )
#define BER_SET					( BER_UNIVERSAL | BER_CONSTRUCTED | BER_ID_SET )
#define BER_STRING_NUMERIC		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_NUMERIC )
#define BER_STRING_PRINTABLE	( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_PRINTABLE )
#define BER_STRING_T61			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_T61 )
#define BER_STRING_VIDEOTEX		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_VIDEOTEX )
#define BER_STRING_IA5			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_IA5 )
#define BER_TIME_UTC			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_TIME_UTC )
#define BER_TIME_GENERALIZED	( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_TIME_GENERALIZED )
#define BER_STRING_GRAPHIC		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_GRAPHIC )
#define BER_STRING_ISO646		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_ISO646 )
#define BER_STRING_GENERAL		( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_GENERAL )
#define BER_STRING_UNIVERSAL	( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_UNIVERSAL )
#define BER_29					( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_BER29 )
#define BER_STRING_BMP			( BER_UNIVERSAL | BER_PRIMITIVE | BER_ID_STRING_BMP )

/* The encodings for constructed, indefinite-length tags and lengths */

#define BER_OCTETSTRING_INDEF	MKDATA( "\x24\x80" )
#define BER_SEQUENCE_INDEF		MKDATA( "\x30\x80" )
#define BER_SET_INDEF			MKDATA( "\x31\x80" )
#define BER_CTAG0_INDEF			MKDATA( "\xA0\x80" )
#define BER_END_INDEF			MKDATA( "\x00\x00" )

/* Masks to extract information from a tag number */

#define BER_CLASS_MASK			0xC0
#define BER_CONSTRUCTED_MASK	0x20
#define BER_SHORT_ID_MASK		0x1F

/* The maximum value for the short tag encoding, and the magic value which 
   indicates that a long encoding of the number is being used */

#define MAX_SHORT_BER_ID		( BER_STRING_BMP + 1 )
#define LONG_BER_ID				0x1F

/* Turn an identifier into a context-specific tag, and extract the value from
   a tag.  Normally these are constructed, but in a few special cases they
   are primitive */

#define MAKE_CTAG( identifier ) \
		( BER_CONTEXT_SPECIFIC | BER_CONSTRUCTED | ( identifier ) )
#define MAKE_CTAG_PRIMITIVE( identifier ) \
		( BER_CONTEXT_SPECIFIC | ( identifier ) )
#define EXTRACT_CTAG( tag ) \
		( ( tag ) & ~( BER_CONTEXT_SPECIFIC | BER_CONSTRUCTED ) )

/****************************************************************************
*																			*
*							ASN.1 Constants and Macros						*
*																			*
****************************************************************************/

/* Special-case tags.  If DEFAULT_TAG is given the basic type (e.g. INTEGER,
   ENUMERATED) is used, otherwise the value is used as a context-specific
   tag.  If NO_TAG is given, processing of the tag is skipped.  If ANY_TAG
   is given, the tag is ignored.  The parentheses are to catch potential
   erroneous use in an expression */

#define DEFAULT_TAG			( -1 )
#define NO_TAG				( -2 )
#define ANY_TAG				( -3 )

/* The highest encoded tag value */

#define MAX_TAG				( BER_CONTEXT_SPECIFIC | BER_CONSTRUCTED | \
							  MAX_SHORT_BER_ID )

/* The highest allowed raw tag value before encoding as a primitive or
   constructed tag or before encoding as a content-specific tag.  In 
   addition to the standard MAX_TAG_VALUE we also have a value for universal
   tags whose basic form is constructed (SETs and SEQUENCES), which would 
   fall outside the normal MAX_TAG_VALUE range.

   Due to CMP's braindamaged use of tag values to communicate message type 
   information we have to be fairly permissive with the context-specific 
   tag range because CMP burns up tag values up to the mid-20s, however we 
   can restrict the range if CMP isn't being used */

#define MAX_TAG_VALUE		MAX_SHORT_BER_ID
#define MAX_CONSTR_TAG_VALUE BER_SET
#ifdef USE_CMP
  #define MAX_CTAG_VALUE	30
#else
  #define MAX_CTAG_VALUE	10
#endif /* USE_CMP */

/* The minimum and maximum allowed size for an (encoded) object identifier */

#define MIN_OID_SIZE		5
#define MAX_OID_SIZE		32

/* When reading an OID selection with readOID() we sometimes need to allow
   a catch-all default value that's used when nothing else matches.  This is
   typically used for type-and-value data where we want to ignore anything
   that we don't recognise.  The following value is used as a match-all
   wildcard.  It's longer than any normal OID to make it possible to do a
   quick-reject match based only on the length.  The second byte is set to
   0x0E (= 14) to make the standard sizeofOID() macro work, since this 
   examines the length field of the encoded OID */

#define WILDCARD_OID		( const BYTE * ) \
							"\xFF\x0E\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00\xFF\x00"
#define WILDCARD_OID_SIZE	16

/* A macro to make make declaring OIDs simpler */

#define MKOID( value )		( ( const BYTE * ) value )

/* Macros and functions to work with indefinite-length tags.  The only ones
   used are SEQUENCE and [0] (for the outer encapsulation) and OCTET STRING
   (for the data itself) */

#define writeOctetStringIndef( stream )	swrite( stream, BER_OCTETSTRING_INDEF, 2 )
#define writeSequenceIndef( stream )	swrite( stream, BER_SEQUENCE_INDEF, 2 )
#define writeSetIndef( stream )			swrite( stream, BER_SET_INDEF, 2 )
#define writeCtag0Indef( stream )		swrite( stream, BER_CTAG0_INDEF, 2 )
#define writeEndIndef( stream )			swrite( stream, BER_END_INDEF, 2 )

#define sizeofEOC()						2
RETVAL_RANGE( MAX_ERROR, TRUE ) STDC_NONNULL_ARG( ( 1 ) ) \
int checkEOC( INOUT STREAM *stream );

/****************************************************************************
*																			*
*							ASN.1 Function Prototypes						*
*																			*
****************************************************************************/

/* Read/peek at a tag and make sure that it's (approximately) valid, and 
   write a tag.  The latter translates directly to sputc(), but we use a
   macro to make explicit what's going on */

RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int readTag( INOUT STREAM *stream );
RETVAL_RANGE( MAX_ERROR, 0xFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int peekTag( INOUT STREAM *stream );
#define writeTag( stream, tag )	sputc( stream, tag )

/* Determine the size of an object once it's wrapped up with a tag and
   length */

RETVAL_RANGE( MAX_ERROR, MAX_INTLENGTH ) \
long sizeofObject( IN_LENGTH const long length );

/* Generalized ASN.1 type manipulation routines.  readRawObject() reads a
   complete object (including tag and length data) while readUniversal()
   just skips it.  Since readRawObject() always requires a tag, we don't
   have the xxx/xxxData() variants that exist for other functions */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversalData( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal( INOUT STREAM *stream );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readRawObject( INOUT STREAM *stream, 
				   OUT_BUFFER( bufferMaxLength, *bufferLength ) BYTE *buffer, 
				   IN_LENGTH_SHORT_MIN( 3 ) const int bufferMaxLength, 
				   OUT_LENGTH_SHORT_Z int *bufferLength, 
				   IN_TAG_ENCODED const int tag );

#define writeRawObject( stream, object, size ) \
		swrite( stream, object, size )

/* Routines for handling OBJECT IDENTIFIERS.  The sizeof() macro determines
   the length of an encoded object identifier as tag + length + value.
   Write OID routines equivalent to the ones for other ASN.1 types don't
   exist since OIDs are always read and written as a blob with sread()/
   swrite().  OIDs are never tagged so we don't need any special-case
   handling for tags.

   When there's a choice of possible OIDs, the list of OID values and
   corresponding selection IDs is provided in an OID_INFO structure (we also
   provide a shortcut readFixedOID() function when there's only a single OID
   that's valid at that point).  The read OID value is checked against each
   OID in the OID_INFO list, if a match is found the selectionID is returned.

   The OID_INFO includes a pointer to further user-supplied information
   related to this OID that may be used by the user, set when the OID list
   is initialised.  For example it could point to OID-specific handlers for
   the data.  When the caller needs to work with the extraInfo field, it's
   necessary to return the complete OID_INFO entry rather than just the
   selection ID, which is done by the ..Ex() form of the function */

typedef struct {
	const BYTE FAR_BSS *oid;/* OID */
	const int selectionID;	/* Value to return for this OID */
	const void *extraInfo;	/* Additional info for this selection */
	} OID_INFO;

#define sizeofOID( oid )	( 1 + 1 + ( int ) oid[ 1 ] )
RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readOID( INOUT STREAM *stream, 
			 IN_ARRAY( noOidSelectionEntries ) \
			 const OID_INFO *oidSelection, 
			 IN_RANGE( 1, 50 ) const int noOidSelectionEntries,
			 OUT_RANGE( CRYPT_ERROR, noOidSelectionEntries ) \
			 int *selectionID );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readOIDEx( INOUT STREAM *stream, 
			   IN_ARRAY( noOidSelectionEntries ) \
			   const OID_INFO *oidSelection, 
			   IN_RANGE( 1, 50 ) const int noOidSelectionEntries,
			   OUT_OPT_PTR_OPT const OID_INFO **oidSelectionValue );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readFixedOID( INOUT STREAM *stream, 
				  IN_BUFFER( oidLength ) \
				  const BYTE *oid, IN_LENGTH_OID const int oidLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readEncodedOID( INOUT STREAM *stream, 
					OUT_BUFFER( oidMaxLength, *oidLength ) BYTE *oid, 
					IN_LENGTH_SHORT_MIN( 5 ) const int oidMaxLength, 
					OUT_LENGTH_SHORT_Z int *oidLength, 
					IN_TAG_ENCODED const int tag );
#define writeOID( stream, oid ) \
				  swrite( ( stream ), ( oid ), sizeofOID( oid ) )

/* Routines for handling large integers.  When we're writing these we can't
   use sizeofObject() directly because the internal representation is
   unsigned whereas the encoded form is signed.  The following macro performs
   the appropriate conversion on the data length before passing it on to
   sizeofObject() */

#define sizeofInteger( value, valueLength ) \
		( int ) sizeofObject( ( valueLength ) + \
							  ( ( *( BYTE * )( value ) & 0x80 ) ? 1 : 0 ) )

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readIntegerTag( INOUT STREAM *stream, 
					OUT_BUFFER_OPT( integerMaxLength, \
									*integerLength ) BYTE *integer, 
					IN_LENGTH_SHORT const int integerMaxLength, 
					OUT_OPT_LENGTH_SHORT_Z int *integerLength, 
					IN_TAG_EXT const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger( INOUT STREAM *stream, 
				  IN_BUFFER( integerLength ) const BYTE *integer, 
				  IN_LENGTH_SHORT const int integerLength, 
				  IN_TAG const int tag );

#define readIntegerData( stream, integer, integerLength, maxLength )	\
		readIntegerTag( stream, integer, integerLength, maxLength, NO_TAG )
#define readInteger( stream, integer, integerLength, maxLength )	\
		readIntegerTag( stream, integer, integerLength, maxLength, DEFAULT_TAG )

/* Routines for handling bignums.  We use void * rather than BIGNUM * to save
   having to include the bignum header everywhere where ASN.1 is used */

#define sizeofBignum( bignum ) \
		( ( int ) sizeofObject( signedBignumSize( bignum ) ) )

RETVAL_RANGE( MAX_ERROR, MAX_INTLENGTH_SHORT ) STDC_NONNULL_ARG( ( 1 ) ) \
int signedBignumSize( IN TYPECAST( BIGNUM * ) const void *bignum );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumTag( INOUT STREAM *stream, INOUT void *bignum, 
				   IN_LENGTH_PKC const int minLength, 
				   IN_LENGTH_PKC const int maxLength, 
				   IN_OPT const void *maxRange, 
				   IN_TAG_EXT const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumTag( INOUT STREAM *stream, 
					IN const void *bignum, 
					IN_TAG const int tag );

#define readBignum( stream, bignum, minLen, maxLen, maxRange ) \
		readBignumTag( stream, bignum, minLen, maxLen, maxRange, DEFAULT_TAG )
#define writeBignum( stream, bignum ) \
		writeBignumTag( stream, bignum, DEFAULT_TAG )

/* Special-case bignum read routine that explicitly checks for a too-short 
   key and returns CRYPT_ERROR_NOSECURE rather than the CRYPT_ERROR_BADDATA 
   that'd otherwise be returned */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumChecked( INOUT STREAM *stream, 
					   INOUT TYPECAST( BIGNUM * ) void *bignum, 
					   IN_LENGTH_PKC const int minLength, 
					   IN_LENGTH_PKC const int maxLength, 
					   IN_OPT const void *maxRange );

/* Generally most integers will be non-bignum values, so we also define
   routines to handle values that will fit into a machine word */

#define sizeofShortInteger( value )	\
	( ( ( value ) < 0x80 ) ? 3 : \
	  ( ( ( long ) value ) < 0x8000L ) ? 4 : \
	  ( ( ( long ) value ) < 0x800000L ) ? 5 : \
	  ( ( ( long ) value ) < 0x80000000UL ) ? 6 : 7 )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeShortInteger( INOUT STREAM *stream, 
					   IN_INT const long integer, 
					   IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readShortIntegerTag( INOUT STREAM *stream, 
						 OUT_OPT_INT_Z long *value, 
						 IN_TAG_EXT const int tag );

#define readShortIntegerData( stream, integer )	\
		readShortIntegerTag( stream, integer, NO_TAG )
#define readShortInteger( stream, integer )	\
		readShortIntegerTag( stream, integer, DEFAULT_TAG )

/* Routines for handling enumerations */

#define sizeofEnumerated( value )	( ( ( value ) < 128 ) ? 3 : 4 )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeEnumerated( INOUT STREAM *stream, 
					 IN_RANGE( 0, 999 ) const int enumerated, 
					 IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readEnumeratedTag( INOUT STREAM *stream, 
					   OUT_OPT_INT_Z int *enumeration, 
					   IN_TAG_EXT const int tag );

#define readEnumeratedData( stream, enumeration ) \
		readEnumeratedTag( stream, enumeration, NO_TAG )
#define readEnumerated( stream, enumeration ) \
		readEnumeratedTag( stream, enumeration, DEFAULT_TAG )

/* Routines for handling booleans */

#define sizeofBoolean()	( sizeof( BYTE ) + sizeof( BYTE ) + sizeof( BYTE ) )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBoolean( INOUT STREAM *stream, const BOOLEAN boolean, 
				  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBooleanTag( INOUT STREAM *stream, 
					OUT_OPT_BOOL BOOLEAN *boolean, 
					IN_TAG_EXT const int tag );

#define readBooleanData( stream, boolean ) \
		readBooleanTag( stream, boolean, NO_TAG )
#define readBoolean( stream, boolean ) \
		readBooleanTag( stream, boolean, DEFAULT_TAG )

/* Routines for handling null values */

#define sizeofNull()	( sizeof( BYTE ) + sizeof( BYTE ) )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeNull( INOUT STREAM *stream, IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readNullTag( INOUT STREAM *stream, IN_TAG_EXT const int tag );

#define readNullData( stream )	readNullTag( stream, NO_TAG )
#define readNull( stream )		readNullTag( stream, DEFAULT_TAG )

/* Routines for handling octet strings */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOctetString( INOUT STREAM *stream, 
					  IN_BUFFER( length ) \
					  const BYTE *string, 
					  IN_LENGTH_SHORT const int length, 
					  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOctetStringTag( INOUT STREAM *stream, 
						OUT_BUFFER( maxLength, *stringLength ) \
						BYTE *string, OUT_LENGTH_SHORT_Z int *stringLength, 
						IN_LENGTH_SHORT const int minLength, 
						IN_LENGTH_SHORT const int maxLength, 
						IN_TAG_EXT const int tag );

#define readOctetStringData( stream, string, stringLength, minLength, maxLength ) \
		readOctetStringTag( stream, string, stringLength, minLength, maxLength, NO_TAG )
#define readOctetString( stream, string, stringLength, minLength, maxLength ) \
		readOctetStringTag( stream, string, stringLength, minLength, maxLength, DEFAULT_TAG )

/* Routines for handling character strings.  There are a number of oddball
   character string types that are all handled through the same functions -
   it's not worth having a seperate function to handle each of the half-dozen
   types */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCharacterString( INOUT STREAM *stream, 
						  IN_BUFFER( length ) const void *string, 
						  IN_LENGTH_SHORT const int length, 
						  IN_TAG_ENCODED const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readCharacterString( INOUT STREAM *stream, 
						 OUT_BUFFER( stringMaxLength, *stringLength ) void *string, 
						 IN_LENGTH_SHORT const int stringMaxLength, 
						 OUT_LENGTH_SHORT_Z int *stringLength, 
						 IN_TAG_EXT const int tag );

/* Routines for handling bit strings.  The sizeof() values are 3 bytes for
   the tag, length, and surplus-bits value, and the data itself */

#define sizeofBitString( value )	\
	( 3 + ( ( ( ( long ) value ) > 0xFFFFFFL ) ? 4 : \
			( ( ( long ) value ) > 0xFFFFL ) ? 3 : \
			( ( value ) > 0xFF ) ? 2 : ( value ) ? 1 : 0 ) )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBitString( INOUT STREAM *stream, 
					IN_INT const int bitString, 
					IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBitStringTag( INOUT STREAM *stream, 
					  OUT_OPT_INT_Z int *bitString, 
					  IN_TAG_EXT const int tag );

#define readBitStringData( stream, bitString ) \
		readBitStringTag( stream, bitString, NO_TAG )
#define readBitString( stream, bitString ) \
		readBitStringTag( stream, bitString, DEFAULT_TAG )

/* Routines for handling UTC and Generalized time */

#define sizeofUTCTime()			( 1 + 1 + 13 )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUTCTime( INOUT STREAM *stream, const time_t timeVal, 
				  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUTCTimeTag( INOUT STREAM *stream, OUT time_t *timeVal, 
					IN_TAG_EXT const int tag );

#define readUTCTimeData( stream, time )	readUTCTimeTag( stream, time, NO_TAG )
#define readUTCTime( stream, time )		readUTCTimeTag( stream, time, DEFAULT_TAG )

#define sizeofGeneralizedTime()	( 1 + 1 + 15 )
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeGeneralizedTime( INOUT STREAM *stream, const time_t timeVal, 
						  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGeneralizedTimeTag( INOUT STREAM *stream, OUT time_t *timeVal, 
							IN_TAG_EXT const int tag );

#define readGeneralizedTimeData( stream, time )	\
		readGeneralizedTimeTag( stream, time, NO_TAG )
#define readGeneralizedTime( stream, time )	\
		readGeneralizedTimeTag( stream, time, DEFAULT_TAG )

/* Utilitity routines for reading and writing constructed objects and
   equivalent holes.  The difference between writeOctet/BitStringHole() and
   writeGenericHole() is that the octet/bit-string versions create a normal
   or context-specific-tagged string while the generic version creates a
   pure hole with no processing of tags */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSequence( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSet( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readConstructed( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					 IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readOctetStringHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
						 IN_LENGTH_SHORT const int minLength, 
						 IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readBitStringHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					   IN_LENGTH_SHORT const int minLength, 
					   IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readGenericHole( INOUT STREAM *stream, OUT_OPT_LENGTH_Z int *length, 
					 IN_LENGTH_SHORT const int minLength, 
					 IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeSequence( INOUT STREAM *stream, IN_LENGTH_Z const int length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeSet( INOUT STREAM *stream, IN_LENGTH_Z const int length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeConstructed( INOUT STREAM *stream, IN_LENGTH_Z const int length, 
					  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeOctetStringHole( INOUT STREAM *stream, 
						  IN_LENGTH_Z const int length, 
						  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBitStringHole( INOUT STREAM *stream, IN_LENGTH_Z const int length, 
						IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeGenericHole( INOUT STREAM *stream, IN_LENGTH_Z const int length, 
					  IN_TAG const int tag );

/* Alternative versions of the above that allow indefinite lengths.  This
   (non-DER) behaviour is the exception rather than the rule, so we have to
   enable it explicitly */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSequenceI( INOUT STREAM *stream, 
				   OUT_OPT_LENGTH_INDEF int *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readSetI( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF int *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readConstructedI( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF int *length, 
					  IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readGenericHoleI( INOUT STREAM *stream, 
					  OUT_OPT_LENGTH_INDEF int *length, 
					  IN_LENGTH_SHORT const int minLength, 
					  IN_TAG const int tag );

/* Read a generic object header, used to find the length of an object being
   read as a blob */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readGenericObjectHeader( INOUT STREAM *stream, 
							 OUT_LENGTH_INDEF long *length, 
							 const BOOLEAN isLongObject );

/* Read an arbitrary-length constructed object's data into a memory buffer.  
   This is the arbitrary-length form of readRawObject() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readRawObjectAlloc( INOUT STREAM *stream, 
						OUT_BUFFER_ALLOC_OPT( *length ) void **objectPtrPtr, 
						OUT_LENGTH_Z int *objectLengthPtr,
						IN_LENGTH_SHORT_MIN( 16 ) const int minLength, 
						IN_LENGTH_SHORT const int maxLength );

/* Determine the length of an ASN.1-encoded object (this just reads the
   outer length if present, but will burrow down into the object if necessary
   if the length is indefinite) and check that an object has valid encoding */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getStreamObjectLength( INOUT STREAM *stream, OUT_LENGTH_Z int *length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getObjectLength( IN_BUFFER( objectLength ) \
					 const void *objectPtr, 
					 IN_LENGTH const int objectLength, 
					 OUT_LENGTH_Z int *length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkObjectEncoding( IN_BUFFER( objectLength ) const void *objectPtr, 
						 IN_LENGTH const int objectLength );

/* Full-length equivalents of length/encapsulating-object read routines.
   These are used explicitly in the rare situations where long lengths are
   valid, all other ASN.1 code only works with short lengths.  Because these
   can be quite long, they allow definite or indefinite lengths */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongSequence( INOUT STREAM *stream, 
					  OUT_OPT_LENGTH_INDEF long *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongSet( INOUT STREAM *stream, OUT_OPT_LENGTH_INDEF long *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongConstructed( INOUT STREAM *stream, 
						 OUT_OPT_LENGTH_INDEF long *length, 
						 IN_TAG const int tag );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readLongGenericHole( INOUT STREAM *stream, 
						 OUT_OPT_LENGTH_INDEF long *length, 
						 IN_TAG const int tag );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getLongStreamObjectLength( INOUT STREAM *stream, 
							   OUT_LENGTH_Z long *length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getLongObjectLength( IN_BUFFER( objectLength ) const void *objectPtr, 
						 IN_LENGTH const long objectLength,
						 OUT_LENGTH_Z long *length );
#endif /* !_ASN1_DEFINED */
