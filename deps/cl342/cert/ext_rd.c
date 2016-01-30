/****************************************************************************
*																			*
*						Certificate Attribute Read Routines					*
*						 Copyright Peter Gutmann 1996-2015					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* Define the following to print a trace of the certificate fields being 
   parsed, useful for debugging broken certificates */

#if !defined( NDEBUG ) && 0
  #if defined( _MSC_VER )
	#pragma warning( disable: 4127 )	/* 'stackPos' may be hardcoded */
  #endif /* VC++ */
  #define TRACE_FIELDTYPE( attributeInfoPtr, stackPos ) \
		  { \
		  int i; \
		  \
		  DEBUG_PRINT(( "%4d:", stell( stream ) )); \
		  for( i = 0; i < stackPos; i++ ) \
			  DEBUG_PRINT(( "  " )); \
		  if( ( attributeInfoPtr ) != NULL && \
			  ( attributeInfoPtr )->description != NULL ) \
			  { \
			  DEBUG_PRINT(( ( attributeInfoPtr )->description )); \
			  DEBUG_PRINT(( "\n" )); \
			  } \
		  else \
			  { \
			  DEBUG_PRINT(( "<Unknown field>\n" )); \
			  } \
		  }
  #define TRACE_DEBUG( message ) \
		  DEBUG_PRINT( message ); \
		  DEBUG_PRINT(( "\n" ));
#else
  #define TRACE_FIELDTYPE( attributeInfoPtr, stackPos )
  #define TRACE_DEBUG( message )
#endif /* NDEBUG */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get the tag for a field from the attribute field definition */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getFieldTag( INOUT STREAM *stream, 
						const ATTRIBUTE_INFO *attributeInfoPtr,
						int *tag )
	{
	int status, value;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( tag, sizeof( int ) ) );

	/* Clear return value.  This is actually a bit difficult to do because 
	   the output can have both positive values (tags) and negative values 
	   (field codes), setting the output to -1000 is invalid for both 
	   types */
	*tag = -1000;

	/* Check whether the field is tagged */
	status = value = getFieldEncodedTag( attributeInfoPtr );
	if( cryptStatusError( status ) )
		{
		/* If there's no tagging (i.e. the tag is the same as the field 
		   type) we'll get an OK_SPECIAL return value, this isn't an 
		   error */
		if( status != OK_SPECIAL )
			return( status );
		}
	else
		{
		/* It's a tagged field, return the encoded form */
		*tag = value;

		return( CRYPT_OK );
		}
	ENSURES( status == OK_SPECIAL );

	/* It's a non-tagged field, the tag is the same as the field type */
	value = attributeInfoPtr->fieldType;
	if( value == FIELDTYPE_TEXTSTRING )
		{
		static const int allowedStringTypes[] = {
			BER_STRING_BMP, BER_STRING_IA5, BER_STRING_ISO646, 
			BER_STRING_PRINTABLE, BER_STRING_T61, BER_STRING_UTF8,
			CRYPT_ERROR, CRYPT_ERROR 
			};
		int i;

		/* This is a variable-tag field that can have one of a number of 
		   tags.  To handle this we peek ahead into the stream to see if an 
		   acceptable tag is present and if not set the value to a non-
		   matching tag value */
		status = value = peekTag( stream );
		if( cryptStatusError( status ) )
			return( status );
		for( i = 0; allowedStringTypes[ i ] != value && \
					allowedStringTypes[ i ] != CRYPT_ERROR && \
					i < FAILSAFE_ARRAYSIZE( allowedStringTypes, int ); i++ );
		ENSURES( i < FAILSAFE_ARRAYSIZE( allowedStringTypes, int ) );
		if( allowedStringTypes[ i ] == CRYPT_ERROR )
			{
			/* We've reached the end of the list of allowed types without 
			   finding a match, change the tag value from what we've read 
			   from the stream to make sure that it results in a non-match 
			   when the caller uses it */
			value = BER_ID_RESERVED;
			}
		}
	if( value == FIELDTYPE_BLOB_BITSTRING || \
		value == FIELDTYPE_BLOB_SEQUENCE )
		{
		/* This is a typed blob that's read as a blob but still has a type
		   for type-checking purposes */
		value = ( value == FIELDTYPE_BLOB_BITSTRING ) ? \
				BER_BITSTRING : BER_SEQUENCE;
		}

	ENSURES( ( ( value == FIELDTYPE_BLOB_ANY || value == FIELDTYPE_DN ) && \
			   !( attributeInfoPtr->encodingFlags & FL_OPTIONAL ) ) || \
			 ( value >= BER_ID_RESERVED && value <= MAX_TAG ) );
			 /* A FIELDTYPE_BLOB_ANY or FIELDTYPE_DN can't be optional 
			    fields because with no type information for them available 
				there's no way to check whether we've encountered them or 
				not, has been verified during the startup check.  
				BER_ID_RESERVED is used to indicate that an invalid tag was 
				read so it's valid at this point */
	*tag = value;

	return( CRYPT_OK );
	}

/* Read an explicit tag that wraps the actual item that we're after */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readExplicitTag( INOUT STREAM *stream, 
							const ATTRIBUTE_INFO *attributeInfoPtr, 
							OUT_TAG_Z int *tag )
	{
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( tag, sizeof( int ) ) );

	REQUIRES( attributeInfoPtr->encodingFlags & FL_EXPLICIT );
	REQUIRES( attributeInfoPtr->fieldEncodedType >= 0 && \
			  attributeInfoPtr->fieldEncodedType < MAX_TAG );

	/* Clear return value */
	*tag = 0;

	/* Read the explicit wrapper */
	status = readConstructed( stream, NULL, 
							  attributeInfoPtr->fieldEncodedType );
	if( cryptStatusError( status ) )
		return( status );

	/* We've processed the explicit wrappper, we're now on the actual tag */
	*tag = attributeInfoPtr->fieldType;

	return( CRYPT_OK );
	}

/* Find the end of an item (either primitive or constructed) in the attribute
   table.  Sometimes we may have already entered a constructed object (for
   example when an attribute has a version number so we don't know until we've
   started processing it that we can't do anything with it), if this is the
   case then the depth parameter indicates how many nesting levels we have to 
   undo */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findItemEnd( const ATTRIBUTE_INFO **attributeInfoPtrPtr,
						IN_RANGE( 0, 2 ) const int depth )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	int currentDepth = depth, iterationCount;

	assert( isReadPtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isReadPtr( *attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( depth >= 0 && depth <= 2 );

	/* Skip to the end of the (potentially) constructed item by recording the
	   nesting level and continuing until either it reaches zero or we reach
	   the end of the item */
	for( attributeInfoPtr = *attributeInfoPtrPtr, iterationCount = 0; 
		 !( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 attributeInfoPtr++, iterationCount++ )
		{
		/* If it's a SEQUENCE/SET, increment the depth; if it's an end-of-
		   constructed-item marker, decrement it by the appropriate amount */
		if( attributeInfoPtr->fieldType == BER_SEQUENCE || \
			attributeInfoPtr->fieldType == BER_SET )
			currentDepth++;
		currentDepth -= decodeNestingLevel( attributeInfoPtr->encodingFlags );
		if( currentDepth <= 0 )
			break;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* We return the next-to-last entry by stopping when we find the 
	   FL_ATTR_ATTREND flag since we're going to move on to the next entry 
	   once we return */
	*attributeInfoPtrPtr = attributeInfoPtr;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							SET/SEQUENCE Management Routines				*
*																			*
****************************************************************************/

/* When we're processing SETs/SEQUENCEs (generically referred to as a SET
   OF) we need to maintain a stack of state information to handle a nested 
   SET OF.  The following code implements the state stack, with the zero-th
   entry being a dummy entry and the first user-set entry being at position
   1 */

#define SETOF_STATE_STACKSIZE	16

#define SETOF_FLAG_NONE			0x00	/* No flag value */
#define SETOF_FLAG_SUBTYPED		0x01	/* SET ends on a subtyped value */
#define SETOF_FLAG_RESTARTPOINT	0x02	/* SET OF rather than SET */
#define SETOF_FLAG_ISEMPTY		0x04	/* Cleared if SET OF contains at least one entry */

typedef struct {
	/* SET OF state information */
	const ATTRIBUTE_INFO *infoStart;	/* Start of SET OF attribute information */
	int startPos, endPos;	/* Start and end position of SET OF */
	int flags;				/* SET OF flags */

	/* Subtype information */
	CRYPT_ATTRIBUTE_TYPE subtypeParent;	/* Parent type if this is subtyped */
	int inheritedFlags;		/* Flags inherited from parent if subtyped */
	} SETOF_STATE_INFO;

typedef struct {
	ARRAY( SETOF_STATE_STACKSIZE, stackPos ) \
	SETOF_STATE_INFO stateInfo[ SETOF_STATE_STACKSIZE + 8 ];
	int stackPos;			/* Current position in stack */
	} SETOF_STACK;

STDC_NONNULL_ARG( ( 1 ) ) \
static void setofStackInit( OUT SETOF_STACK *setofStack )
	{
	SETOF_STATE_INFO *setofInfoPtr;

	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );

	memset( setofStack, 0, sizeof( SETOF_STACK ) );

	/* Set up the dummy entry at position zero, making it of (effectively)
	   infinite length to ensure that the encapsulation check for subsequent
	   entries always succeeds */
	setofInfoPtr = &setofStack->stateInfo[ 0 ];
	setofInfoPtr->endPos = MAX_INTLENGTH_SHORT;
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN setofStackPush( INOUT SETOF_STACK *setofStack )
	{
	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );

	/* Increment the stack pointer.  Note that the precondition is explicitly
	   checked for since it can occur normally as a result of a corrupted
	   certificate while the postcondition can only occur as an internal 
	   error */
	if( setofStack->stackPos < 0 || \
		setofStack->stackPos >= SETOF_STATE_STACKSIZE - 1 )
		return( FALSE );
	setofStack->stackPos++;
	ENSURES_B( setofStack->stackPos >= 1 && \
			   setofStack->stackPos < SETOF_STATE_STACKSIZE );

	/* Initialise the new entry */
	memset( &setofStack->stateInfo[ setofStack->stackPos ], 0, 
			sizeof( SETOF_STATE_INFO ) );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN setofStackPop( INOUT SETOF_STACK *setofStack )
	{
	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );

	/* Decrement the stack pointer.  Note that the precondition is explicitly
	   checked for since it can occur normally as a result of a corrupted
	   certificate while the postcondition can only occur as an internal 
	   error */
	if( setofStack->stackPos <= 0 || \
		setofStack->stackPos >= SETOF_STATE_STACKSIZE )
		return( FALSE );
	setofStack->stackPos--;
	ENSURES_B( setofStack->stackPos >= 0 && \
			   setofStack->stackPos < SETOF_STATE_STACKSIZE - 1 );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN setofStackIsEmpty( INOUT SETOF_STACK *setofStack )
	{
	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );

	REQUIRES_B( setofStack->stackPos >= 0 && \
				setofStack->stackPos < SETOF_STATE_STACKSIZE );

	return( ( setofStack->stackPos <= 0 ) ? TRUE : FALSE );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
static SETOF_STATE_INFO *setofTOS( const SETOF_STACK *setofStack )
	{
	assert( isReadPtr( setofStack, sizeof( SETOF_STACK ) ) );

	REQUIRES_N( setofStack->stackPos >= 0 && \
				setofStack->stackPos < SETOF_STATE_STACKSIZE );

	return( ( SETOF_STATE_INFO * ) \
			&setofStack->stateInfo[ setofStack->stackPos ] );
	}

/* Process the start of a SET/SET OF/SEQUENCE/SEQUENCE OF */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int beginSetof( INOUT STREAM *stream, 
					   INOUT SETOF_STACK *setofStack,
					   const ATTRIBUTE_INFO *attributeInfoPtr )
	{
	SETOF_STATE_INFO *setofInfoPtr, *parentSetofInfoPtr;
	int setofLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	/* Determine the length and start position of the SET OF items.  If the
	   tag is an explicit tag then we don't have to process it since it's
	   already been handled by the caller */
	if( attributeInfoPtr->fieldEncodedType >= 0 && \
		!( attributeInfoPtr->encodingFlags & FL_EXPLICIT ) )
		{
		status = readConstructed( stream, &setofLength,
								  attributeInfoPtr->fieldEncodedType );
		}
	else
		{
		if( attributeInfoPtr->fieldType == BER_SET )
			status = readSet( stream, &setofLength );
		else
			status = readSequence( stream, &setofLength );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* When processing a SET/SEQUENCE with default values for the elements 
	   the result may be a zero-length object, in which case we don't take 
	   any action.  Other too-short objects are an error */
	if( setofLength <= 2 )
		{
		if( setofLength == 0 && \
			( attributeInfoPtr->encodingFlags & FL_EMPTYOK ) )
			return( CRYPT_OK );

		return( CRYPT_ERROR_BADDATA );
		}

	/* Remember assorted information such as where the SET/SEQUENCE ends.  
	   In addition if this is a SET OF/SEQUENCE OF, remember this as a 
	   restart point for when we're parsing the next item in the 
	   SET/SEQUENCE OF */
	parentSetofInfoPtr = setofTOS( setofStack );
	ENSURES( parentSetofInfoPtr != NULL );
	if( !setofStackPush( setofStack ) )
		{
		/* Stack overflow, there's a problem with the certificate */
		return( CRYPT_ERROR_BADDATA );
		}
	setofInfoPtr = setofTOS( setofStack );
	ENSURES( setofInfoPtr != NULL );
	setofInfoPtr->infoStart = attributeInfoPtr;
	if( attributeInfoPtr->encodingFlags & FL_SETOF )
		setofInfoPtr->flags |= SETOF_FLAG_RESTARTPOINT;
	if( !( attributeInfoPtr->encodingFlags & FL_EMPTYOK ) )
		setofInfoPtr->flags |= SETOF_FLAG_ISEMPTY;
	setofInfoPtr->subtypeParent = parentSetofInfoPtr->subtypeParent;
	setofInfoPtr->inheritedFlags = parentSetofInfoPtr->inheritedFlags;
	setofInfoPtr->startPos = stell( stream );
	setofInfoPtr->endPos = setofInfoPtr->startPos + setofLength;

	/* Perform a basic validity check that the current SET OF is contained
	   within its parent */
	if( setofInfoPtr->endPos > parentSetofInfoPtr->endPos )
		return( CRYPT_ERROR_BADDATA );

	return( CRYPT_OK );
	}

/* Check whether we've reached the end of a SET/SEQUENCE.  Returns OK_SPECIAL
   if the end has been reached */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int checkSetofEnd( const STREAM *stream, 
						  INOUT SETOF_STACK *setofStack,
						  const ATTRIBUTE_INFO **attributeInfoPtrPtr )
	{
	const SETOF_STATE_INFO *setofInfoPtr = setofTOS( setofStack );
	const ATTRIBUTE_INFO *oldAttributeInfoPtr = *attributeInfoPtrPtr;
	const ATTRIBUTE_INFO *attributeInfoPtr = *attributeInfoPtrPtr;
	const int currentPos = stell( stream );
	int iterationCount;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( setofStack, sizeof( SETOF_STACK ) ) );
	assert( isReadPtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isReadPtr( *attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( setofInfoPtr != NULL );
	REQUIRES( currentPos > 0 && currentPos < MAX_INTLENGTH_SHORT );

	/* If we're still within the SET/SEQUENCE, we're done */
	if( currentPos < setofInfoPtr->endPos )
		return( CRYPT_OK );

	/* If we've read past the end of the SET/SEQUENCE then there's a problem
	   with the data.  Usually this will be caught by the encoding-validity
	   check, but if it's been disabled due to an oblivious-mode read then
	   we can end up catching the problem here */
	if( currentPos > setofInfoPtr->endPos )
		return( CRYPT_ERROR_BADDATA );

	/* We've reached the end of one or more layers of SET/SEQUENCE, keep 
	   popping SET/SEQUENCE state information until we can continue */
	for( iterationCount = 0;
		 !setofStackIsEmpty( setofStack ) && \
			currentPos >= setofInfoPtr->endPos && \
			iterationCount < SETOF_STATE_STACKSIZE;
		 iterationCount++ )
		{
		const int flags = setofInfoPtr->flags;

		/* Pop one level of parse state.  Alongside the standard stack-
		   underflow check we also check whether the stack is empty after 
		   the pop.  This condition should never occur because for any
		   (non-primitive) attribute data inside the { OID, OCTET STRING }
		   wrapper the encapsulation will always be a SEQUENCE (or perhaps 
		   SET):

			OID attributeType,
			OCTET STRING encapsulates {
				SEQUENCE {
					... ,
					... ,
					...
					}
				}

		   which means that it can never be popped until the end of the 
		   attribute is reached.  Since we exit before this, emptying the
		   stack indicates that there's spurious data at the end of the
		   attribute */
		if( !setofStackPop( setofStack ) )
			return( CRYPT_ERROR_BADDATA );
		if( setofStackIsEmpty( setofStack ) )
			return( CRYPT_ERROR_BADDATA );
		setofInfoPtr = setofTOS( setofStack );
		ENSURES( setofInfoPtr != NULL && setofInfoPtr->endPos > 0 && \
				 setofInfoPtr->endPos <= MAX_INTLENGTH_SHORT );
		attributeInfoPtr = setofInfoPtr->infoStart;

		/* If it's a pure SET/SEQUENCE (not a SET OF/SEQUENCE OF) and there 
		   are no more elements present, go to the end of the SET/SEQUENCE 
		   information in the decoding table */
		if( !( flags & SETOF_FLAG_RESTARTPOINT ) && \
			currentPos >= setofInfoPtr->endPos )
			{
			int status;

			status = findItemEnd( &attributeInfoPtr, 0 );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	ENSURES( iterationCount < SETOF_STATE_STACKSIZE );

	*attributeInfoPtrPtr = attributeInfoPtr;
	return( ( attributeInfoPtr != oldAttributeInfoPtr ) ? \
			OK_SPECIAL : CRYPT_OK );
	}

/* Set full/emptiness handling: Check whether the current set is empty, and 
   mark the current set as non-empty */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN setIsEmpty( SETOF_STATE_INFO *setofInfoPtr )
	{
	assert( isWritePtr( setofInfoPtr, sizeof( SETOF_STATE_INFO ) ) );

	return( ( setofInfoPtr->flags & SETOF_FLAG_ISEMPTY ) ? TRUE : FALSE );
	}

STDC_NONNULL_ARG( ( 1 ) ) \
static void setSetNonempty( SETOF_STATE_INFO *setofInfoPtr )
	{
	assert( isWritePtr( setofInfoPtr, sizeof( SETOF_STATE_INFO ) ) );

	setofInfoPtr ->flags &= ~SETOF_FLAG_ISEMPTY;
	}

/****************************************************************************
*																			*
*						Identified Item Management Routines					*
*																			*
****************************************************************************/

/* Given a pointer to a set of SEQUENCE { type, value } entries, return a
   pointer to the { value } entry appropriate for the data in the stream.  
   If the entry contains user data in the { value } portion then the 
   returned pointer points to this, if it contains a fixed value or isn't 
   present at all then the returned pointer points to the { type } portion */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static const ATTRIBUTE_INFO *findIdentifiedItem( INOUT STREAM *stream,
									const ATTRIBUTE_INFO *attributeInfoPtr )
	{
	BYTE oid[ MAX_OID_SIZE + 8 ];
	int oidLength, sequenceLength, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES_N( attributeInfoPtr->encodingFlags & FL_IDENTIFIER );

	/* Skip the header and read the OID */
	readSequence( stream, &sequenceLength );
	status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
							 BER_OBJECT_IDENTIFIER );
	if( cryptStatusError( status ) )
		return( NULL );
	sequenceLength -= oidLength;
	if( sequenceLength < 0 || sequenceLength >= MAX_INTLENGTH_SHORT )
		return( NULL );

	/* Walk down the list of entries trying to match the read OID to an 
	   allowed value.  Unfortunately we can't use the attributeInfoSize 
	   bounds check limit here because we don't know how far through the 
	   attribute table we already are, so we have to use a generic large 
	   value */
	for( iterationCount = 0;
		 ( attributeInfoPtr->encodingFlags & FL_IDENTIFIER ) && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const BYTE *oidPtr;

		/* Skip the SEQUENCE and OID.  The fact that an OID follows the
		   entry with the FL_IDENTIFIER field in the encoding table unless 
		   it's the final catch-all blob entry has been verified during the 
		   startup check */
		attributeInfoPtr++;
		oidPtr = attributeInfoPtr->oid;

		/* If this is a non-encoded blob field then we've hit a don't-care 
		   value (usually the last in a series of type-and-value pairs) 
		   which ensures that { type }s added after the encoding table was 
		   defined don't get processed as errors, skip the field and 
		   continue */
		if( attributeInfoPtr->fieldType == FIELDTYPE_BLOB_ANY && \
			( attributeInfoPtr->encodingFlags & FL_NONENCODING ) )
			{
			/* If there's a { value } attached to the type, skip it */
			if( sequenceLength > 0 )
				{
				status = sSkip( stream, sequenceLength, 
								MAX_INTLENGTH_SHORT );
				if( cryptStatusError( status ) )
					return( NULL );
				}
			return( attributeInfoPtr );
			}
		ENSURES_N( oidPtr != NULL );

		/* Skip to the payload data unless this is a field like a version 
		   number which isn't used to encode user-supplied information */
		if( !( attributeInfoPtr->encodingFlags & FL_NONENCODING ) )
			attributeInfoPtr++;

		/* If the OID matches, return a pointer to the value entry */
		if( oidLength == sizeofOID( oidPtr ) && \
			!memcmp( oidPtr, oid, sizeofOID( oidPtr ) ) )
			{
			/* If this is a fixed field and there's a value attached, skip
			   it */
			if( ( attributeInfoPtr->encodingFlags & FL_NONENCODING ) && \
				sequenceLength > 0 )
				{
				status = sSkip( stream, sequenceLength, 
								MAX_INTLENGTH_SHORT );
				if( cryptStatusError( status ) )
					return( NULL );
				}

			return( attributeInfoPtr );
			}

		/* The OID doesn't match, skip the { value } entry and continue.  We 
		   set the current nesting depth parameter to 1 since we've already
		   entered the SEQUENCE above */
		status = findItemEnd( &attributeInfoPtr, 1 );
		if( cryptStatusError( status ) )
			return( NULL );
		attributeInfoPtr++;		/* Move to start of next item */
		}
	ENSURES_N( iterationCount < FAILSAFE_ITERATIONS_LARGE );

	/* We've reached the end of the set of entries without matching the OID 
	   (including a possible catch-all value at the end), this is an error */
	return( NULL );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5, 6, 7 ) ) \
static int processIdentifiedItem( INOUT STREAM *stream, 
								  INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
								  IN_FLAGS( ATTR ) const int flags, 
								  const SETOF_STACK *setofStack,
								  const ATTRIBUTE_INFO **attributeInfoPtrPtr,
								  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	const SETOF_STATE_INFO *setofInfoPtr = setofTOS( setofStack );
	const ATTRIBUTE_INFO *attributeInfoPtr;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( setofStack, sizeof( SETOF_STACK ) ) );
	assert( isReadPtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isReadPtr( *attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

assert( ( flags & ~( ATTR_FLAG_CRITICAL ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );
	REQUIRES( setofInfoPtr != NULL );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Search for the identified item from the start of the set of items.  
	   The 0-th value is the SET OF/SEQUENCE OF so we start the search at 
	   the next entry, which is the first FL_IDENTIFIER */
	ENSURES( setofInfoPtr->infoStart->encodingFlags & FL_SETOF );
	attributeInfoPtr = findIdentifiedItem( stream, 
										   setofInfoPtr->infoStart + 1 );
	if( attributeInfoPtr == NULL )
		return( CRYPT_ERROR_BADDATA );
	*attributeInfoPtrPtr = attributeInfoPtr;

	/* If it's a subtyped field, tell the caller to restart the decoding
	   using the new attribute information.  This is typically used where
	   the { type, value } combinations that we're processing are
	   { OID, GeneralName }, so the process consists of locating the entry 
	   that corresponds to the OID and then continuing the decoding with
	   the the subtyped attribute information entry that points to the
	   GeneralName decoding table */
	if( attributeInfoPtr->fieldType == FIELDTYPE_SUBTYPED )
		return( OK_SPECIAL );

	/* If it's not a special-case, non-encoding field, we're done */
	if( !( attributeInfoPtr->encodingFlags & FL_NONENCODING ) )
		return( CRYPT_OK );

	/* If the { type, value } pair has a fixed value then the information 
	   being conveyed is its presence, not its contents, so we add an 
	   attribute corresponding to its ID and continue.  The addition of the 
	   attribute is a bit tricky, some of the fixed type-and-value pairs can 
	   have multiple entries denoting things like { algorithm, weak key }, 
	   { algorithm, average key }, { algorithm, strong key }, however all 
	   that we're interested in is the strong key so we ignore the value and 
	   only use the type (in his ordo est ordinem non servare).  Since the 
	   same type can be present multiple times (with different { value }s) 
	   we ignore data duplicate errors and continue.  If we're processing a 
	   blob field type then we've ended up at a generic catch-any value and 
	   can't do much with it */
	if( attributeInfoPtr->fieldType != FIELDTYPE_BLOB_ANY )
		{
		int status;

		/* Add the field type, discarding warnings about duplicates */
		TRACE_FIELDTYPE( attributeInfoPtr, 0 );
		status = addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
									attributeInfoPtr->fieldID, 
									CRYPT_ATTRIBUTE_NONE, CRYPT_UNUSED, 
									flags, errorLocus, errorType );
		if( cryptStatusError( status ) && status != CRYPT_ERROR_INITED )
			return( CRYPT_ERROR_BADDATA );
		}

	/* Reset the attribute information position in preparation for 
	   processing the next value and tell the caller to continue using the 
	   reset attribute information */
	*attributeInfoPtrPtr = setofInfoPtr->infoStart + 1;
	return( OK_SPECIAL );
	}

/* Read a sequence of identifier fields of the form { oid, value OPTIONAL }.
   This is used to read both SEQUENCE OF (via FIELDTYPE_IDENTIFIER) and 
   CHOICE (via FIELDTYPE_CHOICE), with SEQUENCE OF allowing multiple entries 
   and CHOICE allowing only a single entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6, 7 ) ) \
static int readIdentifierFields( INOUT STREAM *stream, 
								 INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
								 const ATTRIBUTE_INFO **attributeInfoPtrPtr, 
								 IN_FLAGS( ATTR ) const int flags,
								 IN_ATTRIBUTE_OPT const CRYPT_ATTRIBUTE_TYPE fieldID, 
								 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
								 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_INFO *attributeInfoPtr;
	const BOOLEAN isChoice = ( fieldID != CRYPT_ATTRIBUTE_NONE );
	int count = 0, tag, iterationCount, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO * ) ) );
	assert( isReadPtr( *attributeInfoPtrPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

assert( ( flags == ATTR_FLAG_NONE ) || ( flags == ATTR_FLAG_CRITICAL ) );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );
	REQUIRES( ( fieldID == CRYPT_ATTRIBUTE_NONE ) || \
			  ( fieldID >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				fieldID <= CRYPT_CERTINFO_LAST_EXTENSION ) || \
			  ( fieldID >= CRYPT_CERTINFO_FIRST_CMS && \
				fieldID <= CRYPT_CERTINFO_LAST_CMS ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	for( iterationCount = 0;
		 checkStatusPeekTag( stream, status, tag ) && \
			tag == BER_OBJECT_IDENTIFIER && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		BYTE oid[ MAX_OID_SIZE + 8 ];
		BOOLEAN addField = TRUE;
		int oidLength, innerIterationCount;

		/* The fact that the FIELDTYPE_IDENTIFIER field is present and 
		   associated with an OID in the encoding table has been verified 
		   during the startup check */
		attributeInfoPtr = *attributeInfoPtrPtr;
		ENSURES( attributeInfoPtr != NULL && \
				 attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER && \
				 attributeInfoPtr->oid != NULL );

		/* Read the OID and walk down the list of possible OIDs up to the end
		   of the group of alternatives trying to match it to an allowed
		   value */
		status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
								 BER_OBJECT_IDENTIFIER );
		if( cryptStatusError( status ) )
			return( status );
		for( innerIterationCount = 0;
			 ( oidLength != sizeofOID( attributeInfoPtr->oid ) || \
			   memcmp( attributeInfoPtr->oid, oid, oidLength ) ) && \
			   innerIterationCount < FAILSAFE_ITERATIONS_MED;
			 innerIterationCount++ )
			{
			/* If we've reached the end of the list and the OID wasn't
			   matched, exit */
			if( ( attributeInfoPtr->encodingFlags & FL_SEQEND_MASK ) || \
				( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
				return( CRYPT_ERROR_BADDATA );

			attributeInfoPtr++;

			/* If this is a blob field then we've hit a don't-care value 
			   which ensures that { type }s added after the encoding table 
			   was defined don't get processed as errors, skip the field and 
			   continue */
			if( attributeInfoPtr->fieldType == FIELDTYPE_BLOB_ANY )
				{
				addField = FALSE;
				break;
				}

			/* The fact that the FIELDTYPE_IDENTIFIER field is present and 
			   associated with an OID in the encoding table has been verified 
			   during the startup check */
			ENSURES( attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER && \
					 attributeInfoPtr->oid != NULL );
			}
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_MED );
		TRACE_FIELDTYPE( attributeInfoPtr, 0 );
		if( addField )
			{
			/* The OID matches, add this field as an identifier field.  This
			   will catch duplicate OIDs since we can't add the same 
			   identifier field twice */
			if( isChoice )
				{
				/* If there's a field value present then this is a CHOICE of
				   attributes whose value is the field value so we add it with
				   this value */
				status = addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
											fieldID, CRYPT_ATTRIBUTE_NONE,
											attributeInfoPtr->fieldID,  
											flags, errorLocus, errorType );
				}
			else
				{
				/* It's a standard field */
				status = addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr,
											attributeInfoPtr->fieldID, 
											CRYPT_ATTRIBUTE_NONE, 
											CRYPT_UNUSED, flags, 
											errorLocus, errorType );
				}
			if( cryptStatusError( status ) )
				return( status );
			}
		count++;

		/* If there's more than one OID present in a CHOICE, it's an error */
		if( isChoice && count > 1 )
			{
			*errorLocus = attributeInfoPtr->fieldID,
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_BADDATA );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	if( cryptStatusError( status ) )
		return( status );	/* Residual error from peekTag() */

	/* If we haven't seen any fields, this is an error */
	if( count <= 0 )
		return( CRYPT_ERROR_BADDATA );

	/* We've processed the non-data field(s), move on to the next field.
	   We move to the last valid non-data field rather than the start of the
	   field following it since the caller needs to be able to check whether
	   there are more fields to follow using the current field's flags.
	   Unfortunately we can't use the attributeInfoSize bounds check limit 
	   here because we don't know how far through the attribute table we 
	   already are, so we have to use a generic value */
	for( attributeInfoPtr = *attributeInfoPtrPtr, iterationCount = 0;
		 !( attributeInfoPtr->encodingFlags & FL_SEQEND_MASK ) && \
			!( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED;
		 attributeInfoPtr++, iterationCount++ );
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	*attributeInfoPtrPtr = attributeInfoPtr;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Attribute/Attribute Field Read Routines				*
*																			*
****************************************************************************/

/* Generic error-handler that sets extended error codes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int fieldErrorReturn( OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType, 
							 const ATTRIBUTE_INFO *attributeInfoPtr,
							 IN_ERROR const int status )
	{
	CRYPT_ATTRIBUTE_TYPE fieldID = attributeInfoPtr->fieldID;

	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );

	REQUIRES( cryptStatusError( status ) );

	/* For some attributes the field ID is in the following entry, so we 
	   skip to this if required */
	if( fieldID == FIELDID_FOLLOWS )
		{
		fieldID = attributeInfoPtr[ 1 ].fieldID;

		/* Verified during the startup check */
		ENSURES( fieldID > CRYPT_CERTINFO_FIRST && \
				 fieldID < CRYPT_CERTINFO_LAST );
		}
	else
		{
		/* Since some fields are internal-use only (e.g. meaningless blob 
		   data, version numbers, and other paraphernalia) we only set the 
		   locus if it has a meaningful value */
		if( fieldID <= CRYPT_CERTINFO_FIRST || \
			fieldID >= CRYPT_CERTINFO_LAST )
			fieldID = CRYPT_ATTRIBUTE_NONE;
		}
	*errorLocus = fieldID;
	*errorType = CRYPT_ERRTYPE_ATTR_VALUE;

	return( status );
	}

/* Switch from the main encoding table to a subtype encoding table */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
static ATTRIBUTE_INFO *switchToSubtype( const ATTRIBUTE_INFO *attributeInfoPtr,
										INOUT SETOF_STATE_INFO *setofInfoPtr )
	{
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isReadPtr( attributeInfoPtr->extraData, 
					   sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( setofInfoPtr, sizeof( SETOF_STATE_INFO ) ) );

	/* This has already been verified during the startup check */
	REQUIRES_N( attributeInfoPtr != NULL && \
				attributeInfoPtr->extraData != NULL );

	/* Record the subtype parent information */
	setofInfoPtr->subtypeParent = attributeInfoPtr->fieldID;
	setofInfoPtr->inheritedFlags = \
					( attributeInfoPtr->encodingFlags & FL_MULTIVALUED ) ? \
					  ATTR_FLAG_MULTIVALUED : ATTR_FLAG_NONE;

	/* If the subtype is being used to process a list of { ... OPTIONAL, 
	   ... OPTIONAL } and at least one entry must be present, remember 
	   that we haven't seen any entries yet */
	if( !( attributeInfoPtr->encodingFlags & FL_EMPTYOK ) )
		setofInfoPtr->flags |= SETOF_FLAG_ISEMPTY;

	/* If the subtype ends once the current SET/SEQUENCE ends, remember this 
	   so that we return to the main type when appropriate */
	if( ( attributeInfoPtr->encodingFlags & FL_SEQEND_MASK ) || \
		( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) )
		setofInfoPtr->flags |= SETOF_FLAG_SUBTYPED;

	/* Switch to the subtype encoding table */
	return( ( ATTRIBUTE_INFO * ) attributeInfoPtr->extraData );
	}

/* Read the contents of an attribute field.  This uses the readXXXData() 
   variants of the read functions because the field that we're reading may 
   be tagged so we process the tag at a higher level and only read the 
   contents here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6, 7 ) ) \
static int readAttributeField( INOUT STREAM *stream, 
							   INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
							   const ATTRIBUTE_INFO *attributeInfoPtr,
							   IN_ATTRIBUTE_OPT \
									const CRYPT_ATTRIBUTE_TYPE subtypeParent, 
							   IN_FLAGS( ATTR ) const int flags, 
							   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
							   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	CRYPT_ATTRIBUTE_TYPE fieldID, subFieldID;
	const int fieldType = attributeInfoPtr->fieldType;
	int length, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( ( subtypeParent == CRYPT_ATTRIBUTE_NONE ) || \
			  ( subtypeParent > CRYPT_CERTINFO_FIRST && \
				subtypeParent < CRYPT_CERTINFO_LAST ) );
assert( ( flags & ~( ATTR_FLAG_NONE | ATTR_FLAG_CRITICAL | ATTR_FLAG_MULTIVALUED ) ) == 0 );
	REQUIRES( flags >= ATTR_FLAG_NONE && flags <= ATTR_FLAG_MAX );

	/* Set up the field identifiers depending on whether it's a normal field
	   or a subfield of a parent field */
	if( subtypeParent == CRYPT_ATTRIBUTE_NONE )
		{
		fieldID = attributeInfoPtr->fieldID;
		subFieldID = CRYPT_ATTRIBUTE_NONE;
		}
	else
		{
		fieldID = subtypeParent;
		subFieldID = attributeInfoPtr->fieldID;
		}

	/* Read the field as appropriate */
	switch( fieldType )
		{
		case BER_INTEGER:
		case BER_ENUMERATED:
		case BER_BITSTRING:
		case BER_BOOLEAN:
		case BER_NULL:
			{
			int value;

			/* Read the data as appropriate */
			switch( fieldType )
				{
				case BER_BITSTRING:
					status = readBitStringData( stream, &value );
					break;

				case BER_BOOLEAN:
					{
					BOOLEAN boolean;

					status = readBooleanData( stream, &boolean );
					value = boolean;
					break;
					}

				case BER_ENUMERATED:
					status = readEnumeratedData( stream, &value );
					break;

				case BER_INTEGER:
					{
					long longValue;

					status = readShortIntegerData( stream, &longValue );
					value = ( int ) longValue;
					break;
					}

				case BER_NULL:
					/* NULL values have no associated data so we explicitly 
					   set the value to CRYPT_UNUSED to ensure that this is 
					   returned on any attempt to read the attribute data */
					value = CRYPT_UNUSED;
					break;

				default:
					retIntError();
				}
			if( cryptStatusError( status ) )
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );

			/* Add the data for this attribute field */
			return( addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
									   fieldID, subFieldID, value, flags, 
									   errorLocus, errorType ) );
			}

		case BER_TIME_GENERALIZED:
		case BER_TIME_UTC:
			{
			time_t timeVal;

			if( fieldType == BER_TIME_GENERALIZED )
				status = readGeneralizedTimeData( stream, &timeVal );
			else
				status = readUTCTimeData( stream, &timeVal );
			if( cryptStatusError( status ) )
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );

			/* Add the data for this attribute field */
			return( addAttributeFieldString( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
											 fieldID, subFieldID, &timeVal, 
											 sizeof( time_t ), flags, 
											 errorLocus, errorType ) );
			}

		case BER_STRING_BMP:
		case BER_STRING_IA5:
		case BER_STRING_ISO646:
		case BER_STRING_NUMERIC:
		case BER_STRING_PRINTABLE:
		case BER_STRING_T61:
		case BER_STRING_UTF8:
		case BER_OCTETSTRING:
		case FIELDTYPE_BLOB_ANY:
		case FIELDTYPE_BLOB_BITSTRING:
		case FIELDTYPE_BLOB_SEQUENCE:
		case FIELDTYPE_TEXTSTRING:
			{
			/* If it's a string type or a blob read it in as a blob (the 
			   only difference being that for a true blob we read the tag + 
			   length as well) */
			BYTE buffer[ 256 + 8 ];

			/* Read in the string to a maximum length of 256 bytes */
			if( isBlobField( fieldType ) )
				{
				int tag;
				
				/* Reading in blob fields is somewhat difficult since these
				   are typically used to read SET/SEQUENCE OF kitchen-sink
				   values and so won't have a consistent tag that we can pass
				   to readRawObject().  To get around this we peek ahead into
				   the stream to get the tag and then pass that down to 
				   readRawObject().  Note that this requires that the blob 
				   have an internal structure (with its own { tag, length }
				   data) since the caller has already stripped off the 
				   encapsulating tag and length */
				status = tag = peekTag( stream );
				if( cryptStatusError( status ) )
					{
					return( fieldErrorReturn( errorLocus, errorType, 
											  attributeInfoPtr, status ) );
					}
				status = readRawObject( stream, buffer, 256, &length, tag );
				}
			else
				{
				status = readOctetStringData( stream, buffer, &length, \
											  1, 256 );
				}
			if( cryptStatusError( status ) )
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );

			/* There are enough broken certificates out there with 
			   enormously long disclaimers in the certificate policy 
			   explicit text field that we have to specifically check for 
			   them here and truncate the text at a valid length in order to 
			   get it past the attribute validity checking code */
			if( fieldID == CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT && \
				length > 200 )
				length = 200;

			/* Add the data for this attribute field, setting the payload-
			   blob flag to disable type-checking of the payload data so 
			   that users can cram any old rubbish into the strings */
			return( addAttributeFieldString( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
											 fieldID, subFieldID, buffer, length, 
											 flags | ATTR_FLAG_BLOB_PAYLOAD,
											 errorLocus, errorType ) );
			}

		case BER_OBJECT_IDENTIFIER:
			{
			BYTE oid[ MAX_OID_SIZE + 8 ];

			/* If it's an OID then we need to reassemble the entire OID 
			   since this is the form expected by addAttributeFieldString() */
			oid[ 0 ] = BER_OBJECT_IDENTIFIER;	/* Add skipped tag */
			status = readEncodedOID( stream, oid + 1, MAX_OID_SIZE - 1, 
									 &length, NO_TAG );
			if( cryptStatusError( status ) )
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );
			return( addAttributeFieldString( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
											 fieldID, subFieldID, oid, length + 1, 
											 flags, errorLocus, errorType ) );
			}

		case FIELDTYPE_DN:
			{
			DN_PTR *dnPtr;

			/* Read the DN */
			status = readDN( stream, &dnPtr );
			if( cryptStatusError( status ) )
				{
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );
				}

			/* Some buggy certificates can include zero-length DNs, which we 
			   skip */
			if( dnPtr == NULL )
				return( CRYPT_OK );

			/* We're being asked to instantiate the field containing the DN,
			   create the attribute field and fill in the DN value.  Since 
			   the value that we're passing in is actually a DN_PTR rather 
			   than a standard string value we set the size field to the 
			   pseudo-length of a DN_PTR_STORAGE value to keep the static/
			   runtime code checks happy */
			status = addAttributeFieldString( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr, 
											  fieldID, subFieldID, dnPtr, 
											  sizeof( DN_PTR_STORAGE ), 
											  flags, errorLocus, errorType );
			if( cryptStatusError( status ) )
				deleteDN( &dnPtr );
			return( status );
			}
		}

	retIntError();
	}

/* Read an attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6, 7 ) ) \
static int readAttribute( INOUT STREAM *stream, 
						  INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
						  const ATTRIBUTE_INFO *attributeInfoPtr, 
						  IN_LENGTH_SHORT_Z const int attributeLength, 
						  const BOOLEAN criticalFlag, 
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	CRYPT_ATTRIBUTE_TYPE fieldID = attributeInfoPtr->fieldID;
	SETOF_STACK setofStack;
	SETOF_STATE_INFO *setofInfoPtr;
	const int endPos = stell( stream ) + attributeLength;
	BOOLEAN attributeContinues = TRUE;
	int flags = criticalFlag ? ATTR_FLAG_CRITICAL : ATTR_FLAG_NONE;
	int attributeFieldsProcessed, maxAttributeFields, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isReadPtr( attributeInfoPtr, sizeof( ATTRIBUTE_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );
	
	REQUIRES( attributeLength >= 0 && attributeLength < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos > 0 && endPos < MAX_INTLENGTH );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Set an upper limit for how many attribute fields we should be seeing 
	   before we report a problem.  We have to apply special-case handling
	   for a few problem attributes that get used as general-purpose lists
	   fields with arbitrary numbers of entries that exceed normal sanity-
	   check limits */
	if( fieldID == FIELDID_FOLLOWS )
		fieldID = attributeInfoPtr[ 1 ].fieldID; 
	switch( fieldID )
		{
		/* CRYPT_CERTINFO_IPADDRESSBLOCKS and 
		   CRYPT_CERTINFO_AUTONOMOUSSYSIDS are an end-run around the 
		   nonexistence of attribute certificates that turn standard 
		   certificates into arbitrary-length capability lists, with each
		   entry being as small as three bytes.  To deal with this we have
		   to apply a special scaling factor to the upper-bound check */
		case CRYPT_CERTINFO_AUTONOMOUSSYSIDS:
		case CRYPT_CERTINFO_IPADDRESSBLOCKS:
			maxAttributeFields = 5 + ( attributeLength / 3 );
			break;

		case CRYPT_CERTINFO_SUBJECTALTNAME:
			/* altNames are used by CDNs to contain one of every site the
			   CDN supports or one of every domain the parent company owns,
			   an upper limit of 512 names hasn't been exceeded by any
			   currently-seen certificate, although Google has come close */
			maxAttributeFields = min( 5 + ( attributeLength / MIN_DNS_SIZE ), 512 );
			break;

		default:
			maxAttributeFields = min( 5 + ( attributeLength / 3 ), 256 );
		}

	/* Initialise the SET OF state stack */
	setofStackInit( &setofStack );
	setofInfoPtr = setofTOS( &setofStack );
	ENSURES( setofInfoPtr != NULL );

	/* Process each field in the attribute.  This is a simple (well, 
	   conceptually simple but practically complex since it has to deal with 
	   the product of PKI standards committees) FSM driven by the encoding 
	   table and the data that we encounter.  The various states and 
	   associated actions are indicated by the comment tags */
	for( attributeFieldsProcessed = 0;
		 ( attributeContinues || !setofStackIsEmpty( &setofStack ) ) && \
			stell( stream ) < endPos && \
			attributeFieldsProcessed < maxAttributeFields;
		 attributeFieldsProcessed++ )
		{
		int tag, peekedTag, value;

		/* Inside a SET/SET OF/SEQUENCE/SEQUENCE OF: Check for the end of the
		   item/collection of items.  This must be the first action taken
		   since reaching the end of a SET/SEQUENCE pre-empts all other
		   parsing actions */
		if( !setofStackIsEmpty( &setofStack ) )
			{
			/* If we've reached the end of the collection of items, exit */
			status = checkSetofEnd( stream, &setofStack, &attributeInfoPtr );
			if( cryptStatusError( status ) && status != OK_SPECIAL )
				return( status );
			setofInfoPtr = setofTOS( &setofStack );
			ENSURES( setofInfoPtr != NULL );
			if( status == OK_SPECIAL )
				goto continueDecoding;

			/* If we're looking for a new item, find the table entry that it
			   corresponds to.  This takes a pointer to the start of a set of
			   SEQUENCE { type, value } entries and returns a pointer to the
			   appropriate value entry.

			   The test for the start of a new item is a bit complex since we
			   could be at the end of the previous item (i.e. on the next item
			   flagged as an identifier) or at the end of the attribute (i.e.
			   on the start of the next attribute) */
			if( ( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTRSTART ) || \
				( attributeInfoPtr->encodingFlags & FL_IDENTIFIER ) )
				{
				status = processIdentifiedItem( stream, attributeListPtrPtr,
												flags, &setofStack, 
												&attributeInfoPtr, 
												errorLocus, errorType );
				if( cryptStatusError( status ) )
					{
					if( status == OK_SPECIAL )
						{
						/* We've switched to a new encoding table, continue
						   from there */
						status = CRYPT_OK;
						continue;
						}
					return( fieldErrorReturn( errorLocus, errorType, 
											  attributeInfoPtr,
											  CRYPT_ERROR_BADDATA ) );
					}
				}
			}

		/* Subtyped field: Switch to the new encoding table */
		if( attributeInfoPtr->fieldType == FIELDTYPE_SUBTYPED )
			{
			attributeInfoPtr = switchToSubtype( attributeInfoPtr, 
												setofInfoPtr );
			ENSURES( attributeInfoPtr != NULL );
			}

		/* CHOICE (of object identifiers): Read a single OID from a
		   selection defined in a subtable.
		   Identifier field: Read a sequence of one or more { oid, value }
		   fields and continue */
		if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE || \
			attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER )
			{
			if( attributeInfoPtr->fieldType == FIELDTYPE_CHOICE )
				{
				const ATTRIBUTE_INFO *extraDataPtr = \
											attributeInfoPtr->extraData;
						/* Needed because ->extraData is read-only */

				/* This has already been verified during the startup check */
				ENSURES( extraDataPtr != NULL );
				
				status = readIdentifierFields( stream, attributeListPtrPtr,
							&extraDataPtr, flags, attributeInfoPtr->fieldID,
							errorLocus, errorType );
				}
			else
				{
				status = readIdentifierFields( stream, attributeListPtrPtr,
							&attributeInfoPtr, flags, CRYPT_ATTRIBUTE_NONE, 
							errorLocus, errorType );
				}
			if( cryptStatusError( status ) )
				{
				return( fieldErrorReturn( errorLocus, errorType,
										  attributeInfoPtr, 
										  CRYPT_ERROR_BADDATA ) );
				}
			if( !setofStackIsEmpty( &setofStack ) )
				setSetNonempty( setofInfoPtr );	/* We've seen a set entry */
			goto continueDecoding;
			}

		/* Non-encoding field: Skip it and continue */
		if( attributeInfoPtr->encodingFlags & FL_NONENCODING )
			{
			/* Read the data and continue.  We don't check its value or set
			   specific error information for the reasons given under the SET 
			   OF handling code above (value check) and optional field code 
			   below (error locus set) */
			TRACE_FIELDTYPE( attributeInfoPtr, setofStack.stackPos );
			status = readUniversal( stream );
			if( cryptStatusError( status ) )
				return( status );
			if( !setofStackIsEmpty( &setofStack ) )
				setSetNonempty( setofInfoPtr );	/* We've seen a set entry */
			goto continueDecoding;
			}

		/* Get the tag for the field */
		status = getFieldTag( stream, attributeInfoPtr, &tag );
		if( cryptStatusError( status ) )
			return( status );

		/* Optional field: Check whether it's present and if it isn't, move
		   on to the next field */
		if( ( attributeInfoPtr->encodingFlags & FL_OPTIONAL ) && \
			checkStatusPeekTag( stream, status, peekedTag ) && \
			peekedTag != tag )
			{
			/* If it's a field with a default value, add that value.  This
			   isn't needed for cryptlib's own use since it knows the default
			   values for fields but can cause confusion for the caller if 
			   all fields in an attribute have default values because the
			   attribute will appear to disappear when it's read in as no
			   fields are ever added */
			if( attributeInfoPtr->encodingFlags & FL_DEFAULT )
				{
				CRYPT_ATTRIBUTE_TYPE dummy1;
				CRYPT_ERRTYPE_TYPE dummy2;

				status = addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr,
											attributeInfoPtr->fieldID, 
											CRYPT_ATTRIBUTE_NONE,
											attributeInfoPtr->defaultValue, 
											flags, &dummy1, &dummy2 );
				if( cryptStatusError( status ) )
					{
					/* This is a field contributed from internal data so we
					   don't get an error locus or value for it since this 
					   would only confuse the caller */
					return( CRYPT_ERROR_BADDATA );
					}
				}

			/* Skip to the end of the item and continue */
			status = findItemEnd( &attributeInfoPtr, 0 );
			if( cryptStatusError( status ) )
				return( status );
			
			/* Since this was a (non-present) optional attribute it 
			   shouldn't be counted in the total when we continue decoding,
			   so we adjust the fields-processed value to account for this */
			if( attributeFieldsProcessed > 0 )
				attributeFieldsProcessed--;

			goto continueDecoding;
			}
		if( cryptStatusError( status ) )
			return( status );	/* Residual error from peekTag() */

		/* Print a trace of what we're processing.  Everything before this
		   point does its own special-case tracing if required so we don't
		   trace before we get here to avoid displaying duplicate/
		   misleading information */
		TRACE_FIELDTYPE( attributeInfoPtr, setofStack.stackPos );

		/* Explicitly tagged field: Read the explicit wrapper and make sure
		   that it matches what we're expecting (the read is done here, the
		   match is done further down) */
		if( attributeInfoPtr->encodingFlags & FL_EXPLICIT )
			{
			REQUIRES( tag == MAKE_CTAG( attributeInfoPtr->fieldEncodedType ) );
					  /* Always constructed */

			status = readExplicitTag( stream, attributeInfoPtr, &tag );
			if( cryptStatusError( status ) )
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );
			}

		/* Blob field or DN: We don't try and interpret blobs in any way, 
		   and DNs are a composite structure read as a complete unit by the 
		   lower-level code */
		if( isBlobField( attributeInfoPtr->fieldType ) || \
			attributeInfoPtr->fieldType == FIELDTYPE_DN )
			{
			status = readAttributeField( stream, attributeListPtrPtr,
										 attributeInfoPtr,
										 setofInfoPtr->subtypeParent,
										 flags | setofInfoPtr->inheritedFlags,
										 errorLocus, errorType );
			if( cryptStatusError( status ) )
				{
				/* Adding complex attributes such as DNs can return detailed
				   error codes that report the exact parameter that was wrong,
				   we don't need this much detail so we convert a parameter
				   error into a more general bad data status */
				return( fieldErrorReturn( errorLocus, errorType,
										  attributeInfoPtr,
										  cryptArgError( status ) ? \
											CRYPT_ERROR_BADDATA : status ) );
				}
			if( !setofStackIsEmpty( &setofStack ) )
				setSetNonempty( setofInfoPtr );	/* We've seen a set entry */
			goto continueDecoding;
			}

		/* Standard field: Read the tag for the field and make sure that it
		   matches what we're expecting */
		status = value = peekTag( stream );
		if( cryptStatusError( status ) || value != tag )
			{
			return( fieldErrorReturn( errorLocus, errorType,
									  attributeInfoPtr,
									  CRYPT_ERROR_BADDATA ) );
			}
		if( !setofStackIsEmpty( &setofStack ) )
			setSetNonempty( setofInfoPtr );		/* We've seen a set entry */

		/* SET/SET OF/SEQUENCE/SEQUENCE OF start: Record its end position,
		   stack the current processing state, and continue */
		if( attributeInfoPtr->fieldType == BER_SEQUENCE || \
			attributeInfoPtr->fieldType == BER_SET )
			{
			status = beginSetof( stream, &setofStack, attributeInfoPtr );
			if( cryptStatusError( status ) )
				{
				return( fieldErrorReturn( errorLocus, errorType, 
										  attributeInfoPtr, status ) );
				}
			setofInfoPtr = setofTOS( &setofStack );
			ENSURES( setofInfoPtr != NULL );
			goto continueDecoding;
			}
		ENSURES( !( attributeInfoPtr->encodingFlags & FL_SETOF ) );

		/* We've checked the tag, skip it.  We do this at this level rather
		   than in readAttributeField() because it doesn't know about 
		   context-specific tagging requirements */
		status = readTag( stream );
		if( cryptStatusError( status ) )
			return( status );

		/* Standard field, read the field data */
		status = readAttributeField( stream, attributeListPtrPtr,
									 attributeInfoPtr,
									 setofInfoPtr->subtypeParent,
									 flags | setofInfoPtr->inheritedFlags,
									 errorLocus, errorType );
		if( cryptStatusError( status ) )
			{
			/* Adding invalid attribute data can return detailed error codes
			   that report the exact parameter that was wrong, we don't
			   need this much detail so we convert a parameter error into a
			   more general bad data status */
			return( cryptArgError( status ) ? CRYPT_ERROR_BADDATA : status );
			}

		/* Move on to the next field */
continueDecoding:
		attributeContinues = \
					( attributeInfoPtr->typeInfoFlags & FL_ATTR_ATTREND ) ? \
					FALSE : TRUE;
		attributeInfoPtr++;

		/* If this is the end of the attribute encoding information but we're
		   inside a SET OF/SEQUENCE OF and there's more attribute data 
		   present, go back to the restart point and try again */
		if( !attributeContinues && !setofStackIsEmpty( &setofStack ) && \
			stell( stream ) < setofInfoPtr->endPos )
			{
			/* If we require at least one entry in the SET OF/SEQUENCE OF 
			   but we haven't found one, this is an error */
			if( setIsEmpty( setofInfoPtr ) )
				return( CRYPT_ERROR_BADDATA );
			ENSURES( setofInfoPtr->infoStart != NULL );

			/* If this isn't a SET OF/SEQUENCE OF and we haven't seen an 
			   element, we've run into an error */
			if( !( setofInfoPtr->infoStart->encodingFlags & FL_SETOF ) )
				return( CRYPT_ERROR_BADDATA );

			/* If we haven't made any progress in processing the SET OF/
			   SEQUENCE OF then further iterations through the loop won't
			   make any difference, there's a bug in the decoder */
			ENSURES( stell( stream ) > setofInfoPtr->startPos );

			/* Retry from the restart point */
			attributeInfoPtr = setofInfoPtr->infoStart + 1;
			ENSURES( ( setofInfoPtr->flags & SETOF_FLAG_RESTARTPOINT ) || \
					 attributeInfoPtr->fieldType == FIELDTYPE_IDENTIFIER );
			attributeContinues = TRUE;
			}
		}

	/* If we got stuck in a loop trying to decode an attribute, complain and 
	   exit.  At this point we could have encountered either a certificate-
	   parsing error or a CRYPT_ERROR_INTERNAL internal error, since we 
	   can't tell without human intervention we treat it as a certificate 
	   error rather than throwing a retIntError() exception */
	if( attributeFieldsProcessed >= maxAttributeFields )
		{
		DEBUG_DIAG(( "Processed more than %d fields in attribute, decoder "
					 "may be stuck", maxAttributeFields ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_BADDATA );
		}

	/* Handle the special case of (a) the encoded data ending but fields with
	   default values being present or (b) the encoded data continuing but no 
	   more decoding information being present */
	if( attributeContinues )
		{
		/* If there are default fields to follow, add the default value, see
		   the comment on the handling of default fields above for more 
		   details.  For now we only add the first field since the only 
		   attributes where this case can occur have a single default value 
		   as the next possible entry, burrowing down further causes 
		   complications due to default values present in optional sequences.  
		   As usual we don't set any specific error information for the 
		   default fields */
		if( attributeInfoPtr->encodingFlags & FL_DEFAULT )
			{
			CRYPT_ATTRIBUTE_TYPE dummy1;
			CRYPT_ERRTYPE_TYPE dummy2;

			status = addAttributeField( ( ATTRIBUTE_PTR ** ) attributeListPtrPtr,
										attributeInfoPtr->fieldID, 
										CRYPT_ATTRIBUTE_NONE,
										attributeInfoPtr->defaultValue, 
										flags, &dummy1, &dummy2 );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	else
		{
		int extIterations;
		
		/* Some attributes have a SEQUENCE OF fields of no great use (e.g.
		   Microsoft's extensive crlDistributionPoints lists providing
		   redundant pointers to the same inaccessible site-internal
		   servers, although these are already handled above), if there's
		   any extraneous data left then we just skip it */
		for( extIterations = 0; 
			 cryptStatusOK( status ) && stell( stream ) < endPos && \
				extIterations < 5; extIterations++ )
			{
			DEBUG_DIAG(( "Skipping extraneous data at end of attribute" ));
			assert_nofuzz( DEBUG_WARN );
			status = readUniversal( stream );
			}
		if( extIterations > 5 )
			{
			/* There's a suspiciously large amount of extra data, treat it 
			   as an error */
			status = CRYPT_ERROR_BADDATA;
			}
		}

	return( status );
	}

/****************************************************************************
*																			*
*						Attribute Collection Read Routines					*
*																			*
****************************************************************************/

/* Read the certificate object-specific wrapper for a set of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readAttributeWrapper( INOUT STREAM *stream, 
								 OUT_LENGTH_Z int *lengthPtr, 
								 IN_ENUM_OPT( CRYPT_CERTTYPE ) \
									const CRYPT_CERTTYPE_TYPE type,
								 IN_LENGTH_SHORT const int attributeLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( lengthPtr, sizeof( int ) ) );

	REQUIRES( type >= CRYPT_CERTTYPE_NONE && type < CRYPT_CERTTYPE_LAST );
			  /* Single CRL entries have the special-case type 
			     CRYPT_CERTTYPE_NONE */
	REQUIRES( attributeLength >= 0 && attributeLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*lengthPtr = 0;

	/* Read the appropriate wrapper for the certificate object type and 
	   determine how far we can read.  CRLs and OCSP requests/responses have 
	   two attribute types that have different tagging, per-entry attributes 
	   and entire-CRL/request attributes.  To differentiate between the two 
	   we read per-entry attributes with a type of CRYPT_CERTTYPE_NONE */
	switch( type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
			readConstructed( stream, NULL, CTAG_CE_EXTENSIONS );
			return( readSequence( stream, lengthPtr ) );

		case CRYPT_CERTTYPE_CRL:
			readConstructed( stream, NULL, CTAG_CL_EXTENSIONS );
			return( readSequence( stream, lengthPtr ) );

		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_PKIUSER:
		case CRYPT_CERTTYPE_NONE:
			/* Any outer wrapper for per-entry CRL/OCSP attributes has
			   already been read by the caller so there's only the inner
			   SEQUENCE left to read */
			return( readSequence( stream, lengthPtr ) );

		case CRYPT_CERTTYPE_CMS_ATTRIBUTES:
			return( readConstructed( stream, lengthPtr,
									 CTAG_SI_AUTHENTICATEDATTRIBUTES ) );

		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* CRMF/CMP attributes don't contain any wrapper so there's
			   nothing to read */
			*lengthPtr = attributeLength;
			return( CRYPT_OK );

		case CRYPT_CERTTYPE_RTCS_REQUEST:
			return( readSet( stream, lengthPtr ) );

		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			return( readConstructed( stream, lengthPtr, CTAG_RP_EXTENSIONS ) );

		case CRYPT_CERTTYPE_OCSP_REQUEST:
			readConstructed( stream, NULL, CTAG_OR_EXTENSIONS );
			return( readSequence( stream, lengthPtr ) );

		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			readConstructed( stream, NULL, CTAG_OP_EXTENSIONS );
			return( readSequence( stream, lengthPtr ) );
		}

	retIntError();
	}

#ifdef USE_CERTREQ 

/* Read a PKCS #10 certificate request wrapper for a set of attributes.  
   This isn't as simple as it should be because there are two approaches to 
   adding attributes to a request, the PKCS #10 approach which puts them all 
   inside a PKCS #9 extensionRequest attribute and the SET approach which 
   lists them all individually (CRMF is a separate case handled by 
   readAttributeWrapper() above).  Complicating this even further is the 
   SCEP approach, which puts attributes intended to be put into the final 
   certificate inside a PKCS #9 extensionRequest but other attributes used 
   for certificate issuance control, for example a challengePassword, 
   individually as SET does.  So the result can be something like:

	[0] SEQUENCE {
		SEQUENCE { challengePassword ... }
		SEQUENCE { extensionRequest 
			SEQUENCE basicConstraints 
			SEQUENCE keyUsage 
			}
		}

   In addition Microsoft invented their own incompatible version of the PKCS 
   #9 extensionRequest which is exactly the same as the PKCS #9 one but with 
   a MS OID, however they also invented other values to add containing God 
   knows what sort of data (long Unicode strings describing the Windows 
   module that created it (as if you'd need that to know where it came 
   from), the scripts from "Gilligan's Island", every "Brady Bunch" episode 
   ever made, dust from under somebody's bed from the 1930s, etc).
   
   Because of these problems, the code does the following:

	- If it's a standalone attribute, it processes it.
	- If it's a PKCS #9 extensionRequest, it reads the wrapper and returns.
	- If it's unknown garbage, it skips it.
   
   This leads to two follow-on issues.  Firstly, since all attributes may be 
   either skipped or processed at this stage we include provisions for 
   bailing out if we exhaust the available attributes.  Secondly, as soon as 
   we encounter a PKCS #9 extensionRequest we exit back to readAttributes() 
   for handling the individual attributes within the extensionRequest.  This 
   means that in order to handle any additional attributes present after the 
   ones encapsulated in the PKCS #9 extensionRequest we have to make a 
   second call to here after the main attribute-processing loop in 
   readAttributes() has finished reading the encapsulated attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5, 6 ) ) \
static int readCertReqWrapper( INOUT STREAM *stream, 
							   INOUT ATTRIBUTE_LIST **attributeListPtrPtr,
							   OUT_DATALENGTH_Z int *lengthPtr, 
							   IN_LENGTH_SHORT const int attributeLength,
							   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus,
							   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	const int endPos = stell( stream ) + attributeLength;
	int attributesProcessed, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( lengthPtr, sizeof( int ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( attributeLength > 0 && attributeLength < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos > 0 && endPos < MAX_BUFFER_SIZE );

	/* Clear return values */
	*lengthPtr = 0;
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Make sure that the length that we've been given makes sense */
	if( attributeLength < MIN_ATTRIBUTE_SIZE || \
		attributeLength >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );

	for( attributesProcessed = 0; attributesProcessed < 16; \
		 attributesProcessed++ )
		{
#if defined( USE_CERT_OBSOLETE ) || defined( USE_SCEP )
		const ATTRIBUTE_INFO *attributeInfoPtr;
#endif /* USE_CERT_OBSOLETE || USE_SCEP */
		BYTE oid[ MAX_OID_SIZE + 8 ];
		int oidLength;

		/* If we've run out of attributes, exit */
		if( stell( stream ) >= endPos )
			return( OK_SPECIAL );

		/* Read the wrapper SEQUENCE and OID */
		readSequence( stream, NULL );
		status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
								 BER_OBJECT_IDENTIFIER );
		if( cryptStatusError( status ) )
			return( status );

#if defined( USE_CERT_OBSOLETE ) || defined( USE_SCEP )
		/* Check for a known attribute, which can happen with SET and SCEP 
		   certificate requests.  If it's a known attribute, process it */
		attributeInfoPtr = oidToAttribute( ATTRIBUTE_CERTIFICATE, 
										   oid, oidLength );
		if( attributeInfoPtr != NULL )
			{
			int length;

			status = readSet( stream, &length );
			if( cryptStatusOK( status ) )
				{
				status = readAttribute( stream, attributeListPtrPtr,
										attributeInfoPtr, length, FALSE, 
										errorLocus, errorType );
				}
			if( cryptStatusError( status ) )
				return( status );

			continue;
			}
#endif /* USE_CERT_OBSOLETE || USE_SCEP */

		/* It's not a known attribute, check whether it's a CRMF or MS 
		   wrapper attribute */
		if( ( oidLength == sizeofOID( OID_PKCS9_EXTREQ ) && \
			  !memcmp( oid, OID_PKCS9_EXTREQ, oidLength ) ) || \
			( oidLength == sizeofOID( OID_MS_EXTREQ ) && \
			  !memcmp( oid, OID_MS_EXTREQ, oidLength ) ) )
			{
			/* Skip the wrapper to reveal the encapsulated attributes */
			readSet( stream, NULL );
			return( readSequence( stream, lengthPtr ) );
			}

		/* It's unknown MS garbage, skip it */
		status = readUniversal( stream );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* As with the check in readAttribute() above, getting to this point 
	   could be either a certificate-parsing error or a CRYPT_ERROR_INTERNAL 
	   internal error, since we can't tell without human intervention we 
	   treat it as a certificate error rather than throwing a retIntError() 
	   exception */
	DEBUG_DIAG(( "Unknown certificate request wrapper type encountered" ));
	assert_nofuzz( DEBUG_WARN );
	return( CRYPT_ERROR_BADDATA );
	}
#endif /* USE_CERTREQ */

/* Read a set of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5, 6 ) ) \
int readAttributes( INOUT STREAM *stream, 
					INOUT ATTRIBUTE_PTR **attributeListPtrPtr,
					IN_ENUM_OPT( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type, 
					IN_LENGTH_Z const int attributeLength,
					OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	const ATTRIBUTE_TYPE attributeType = ( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES || \
										   type == CRYPT_CERTTYPE_RTCS_REQUEST || \
										   type == CRYPT_CERTTYPE_RTCS_RESPONSE ) ? \
										 ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE;
	const BOOLEAN wrapperTagSet = ( attributeType == ATTRIBUTE_CMS ) ? \
								  TRUE : FALSE;
#ifdef USE_CERTREQ 
	const int attributeEndPos = stell( stream ) + attributeLength;
#endif /* USE_CERTREQ */
	int length, endPos, complianceLevel, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeListPtrPtr, sizeof( ATTRIBUTE_LIST * ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( type >= CRYPT_CERTTYPE_NONE && type < CRYPT_CERTTYPE_LAST );
			  /* Single CRL entries have the special-case type 
			     CRYPT_CERTTYPE_NONE */
	REQUIRES( ( type == CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
				attributeLength == 0 ) || \
			  ( type != CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
				attributeLength > 0 && attributeLength < MAX_INTLENGTH ) );
			  /* CMS attributes are pure attribute data with no 
			     encapsulation to indicate the length so the length is 
				 implicitly "everything that's present".
				 See the comment below for why we check for MAX_INTLENGTH
				 rather than MAX_INTLENGTH_SHORT */

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE, &complianceLevel,
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );

	/* If we've been given an excessively long length, don't try and go any 
	   further.  Generally the higher-level code will have enforced this
	   check by way of the length being passed to us being what's left of
	   the data in the certificate object, but if the length has come from
	   a read of any level of wrapper around the attributes then this 
	   implicit enforcement won't have taken place so we perform an explicit 
	   check here.  This means that from now on we can guarantee that the 
	   length is no greater than MAX_INTLENGTH_SHORT rather than the more 
	   generic MAX_INTLENGTH that's checked for in the precondition check
	   above */
	if( ( type != CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
		  attributeLength < MIN_ATTRIBUTE_SIZE ) || \
		attributeLength >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );

	/* Read the wrapper for the certificate object's attributes and 
	   determine how far we can read */
#ifdef USE_CERTREQ 
	if( type == CRYPT_CERTTYPE_CERTREQUEST )
		{
		status = readCertReqWrapper( stream, 
									 ( ATTRIBUTE_LIST ** ) attributeListPtrPtr, 
									 &length, attributeLength, errorLocus, 
									 errorType );
		if( status == OK_SPECIAL )
			{
			/* We processed all attributes that were present as part of the
			   wrapper read (see the explanation in readCertReqWrapper() for
			   details), we're done */
			return( CRYPT_OK );
			}
		}
	else
#endif /* USE_CERTREQ */
		status = readAttributeWrapper( stream, &length, type, attributeLength );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_ATTRIBUTE_SIZE || length >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );
	endPos = stell( stream ) + length;

	/* Read the collection of attributes */
	TRACE_DEBUG(( "\nReading attributes for certificate object starting at "
				  "offset %d.", stell( stream ) ));
	for( iterationCount = 0;
		 stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		const ATTRIBUTE_INFO *attributeInfoPtr;
		BYTE oid[ MAX_OID_SIZE + 8 ];
		BOOLEAN criticalFlag = FALSE, ignoreAttribute = FALSE;
		void *attributeDataPtr;
		int tag, oidLength, attributeDataLength;

		/* Read the outer wrapper and determine the attribute type based on
		   the OID */
		readSequence( stream, NULL );
		status = readEncodedOID( stream, oid, MAX_OID_SIZE, &oidLength, 
								 BER_OBJECT_IDENTIFIER );
		if( cryptStatusError( status ) )
			{
			TRACE_DEBUG(( "Couldn't read attribute OID, status %d.", 
						  status ));
			return( status );
			}
		attributeInfoPtr = oidToAttribute( attributeType, oid, oidLength );
		if( attributeInfoPtr != NULL && \
			complianceLevel < decodeComplianceLevel( attributeInfoPtr->typeInfoFlags ) )
			{
			/* If we're running at a lower compliance level than that
			   required for the attribute, ignore it by treating it as a
			   blob-type attribute */
			TRACE_DEBUG(( "Reading %s (%d) as a blob.", 
						  attributeInfoPtr->description, 
						  attributeInfoPtr->fieldID ));
			attributeInfoPtr = NULL;
			ignoreAttribute = TRUE;
			}

		/* Read the optional critical flag if it's a certificate object.  If 
		   the attribute is marked critical and we don't recognise it then 
		   we don't reject it at this point because that'd make it 
		   impossible to examine the contents of the certificate or display 
		   it to the user.  Instead we reject the certificate when we try 
		   and check it with cryptCheckCert()/checkCertValidity() */
		if( attributeType != ATTRIBUTE_CMS && \
			checkStatusPeekTag( stream, status, tag ) && \
			tag == BER_BOOLEAN )
			{
			status = readBoolean( stream, &criticalFlag );
			if( cryptStatusError( status ) )
				{
				*errorLocus = ( attributeInfoPtr != NULL ) ? \
							  attributeInfoPtr->fieldID : CRYPT_ATTRIBUTE_NONE;
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( status );
				}
			}
		if( cryptStatusError( status ) )
			return( status );	/* Residual error from peekTag() */

		/* Read the wrapper around the attribute payload.  We allow a length 
		   down to zero since it could be an attribute with all-default 
		   fields */
		if( wrapperTagSet )
			status = readSet( stream, &attributeDataLength );
		else
			status = readOctetStringHole( stream, &attributeDataLength, 2,
										  DEFAULT_TAG );
		if( cryptStatusOK( status ) && \
			( attributeDataLength < 0 || \
			  attributeDataLength >= MAX_INTLENGTH_SHORT ) )
			status = CRYPT_ERROR_BADDATA;
		if( cryptStatusError( status ) )
			{
			*errorLocus = ( attributeInfoPtr != NULL ) ? \
						  attributeInfoPtr->fieldID : CRYPT_ATTRIBUTE_NONE;
			*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
			TRACE_DEBUG(( "Couldn't read attribute payload wrapper for %s, "
						  "status %d.", 
						  ( attributeInfoPtr != NULL && \
						    attributeInfoPtr->description != NULL ) ? \
							attributeInfoPtr->description : \
							"(unrecognised blob attribute)", status ));
			return( status );
			}

		/* If it's a known attribute, parse the payload */
		if( attributeInfoPtr != NULL )
			{
			status = readAttribute( stream, 
									( ATTRIBUTE_LIST ** ) attributeListPtrPtr,
									attributeInfoPtr, attributeDataLength,
									criticalFlag, errorLocus, errorType );
			if( cryptStatusError( status ) )
				{
				TRACE_DEBUG(( "Error %d reading %s attribute.", status,
							  ( attributeInfoPtr->description != NULL ) ? \
								attributeInfoPtr->description : \
								"(unrecognised blob attribute)" ));
				return( status );
				}
			continue;
			}

		/* If it's a zero-length unrecognised attribute, don't add anything.
		   A zero length indicates that the attribute contains all default
		   values, however since we don't recognise the attribute we can't
		   fill these in so the attribute is in effect not present */
		if( attributeDataLength <= 0 )
			continue;

		/* It's an unrecognised or ignored attribute type, add the raw data
		   to the list of attributes */
		status = sMemGetDataBlock( stream, &attributeDataPtr, 
								   attributeDataLength );
		if( cryptStatusOK( status ) )
			{
			ANALYSER_HINT( attributeDataPtr != NULL );
			status = addAttribute( attributeType, attributeListPtrPtr, 
								   oid, oidLength, criticalFlag, 
								   attributeDataPtr, attributeDataLength, 
								   ignoreAttribute ? \
										ATTR_FLAG_BLOB | ATTR_FLAG_IGNORED : \
										ATTR_FLAG_BLOB );
			}
		if( cryptStatusError( status ) )
			{
			if( status == CRYPT_ERROR_INITED )
				{
				/* If there's a duplicate attribute present, set error
				   information for it and flag it as a bad data error.  We
				   can't set an error locus since it's an unknown blob */
				*errorLocus = CRYPT_ATTRIBUTE_NONE;
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				status = CRYPT_ERROR_BADDATA;
				}
			TRACE_DEBUG(( "Error %d adding unrecognised blob attribute data.", 
						  status ));
			return( status );
			}

		/* Skip the attribute data */
		sSkip( stream, attributeDataLength, MAX_INTLENGTH_SHORT );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	TRACE_DEBUG(( "Finished reading attributes for certificate object ending at "
				  "offset %d.\n", stell( stream ) ));

	/* Certificate requests can contain unencapsulated attributes as well as
	   attributes encapsulated inside PKCS #9 extensionRequests.  
	   readCertReqWrapper() processes any unencapsulated attributes that 
	   precede a PKCS #9 extensionRequest (if present) and the attribute-
	   read loop above reads the encapsulated attributes.  If there are 
	   further unencapsulated attributes present following the PKCS #9 
	   extensionRequest-encapsulated ones then we read them now */
#ifdef USE_CERTREQ 
	if( type == CRYPT_CERTTYPE_CERTREQUEST && \
		stell( stream ) < attributeEndPos )
		{
		status = readCertReqWrapper( stream, 
						( ATTRIBUTE_LIST ** ) attributeListPtrPtr, &length, 
						attributeEndPos - stell( stream ), errorLocus, 
						errorType );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			{
			/* If all remaining attributes were processed by 
			   readCertReqWrapper() then it returns OK_SPECIAL, this is used 
			   to indicate that there are no PKCS #9 extensionRequest-
			   encapsulated attributes present.  Since this condition is
			   always met at this point because we're calling it after
			   processing encapsulated attributes, we have to filter it
			   out */
			return( status );
			}
		}
#endif /* USE_CERTREQ */

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
