/****************************************************************************
*																			*
*					Certificate Attribute Routines Header File 				*
*						Copyright Peter Gutmann 1997-2011					*
*																			*
****************************************************************************/

#ifndef _CERTATTR_DEFINED

#define _CERTATTR_DEFINED

/****************************************************************************
*																			*
*								Type Information Flags						*
*																			*
****************************************************************************/

/* The attribute type information.  This is used to both check the validity
   of encoded attribute data and to describe the structure of an attribute
   when encoding it.
   
   The flags are broken down into the following groups:

	Attribute-specific flags that apply to an individual field or an
	overall attribute.

	SET/SEQUENCE control flags that indicate the end of a SET/SEQUENCE or
	nested SET/SEQUENCE.  These are only used for encoding, for decoding the 
	decoder maintains a parse state stack driven by the encoded data 
	(actually that's not quite correct, when skipping to the end of some 
	SEQUENCEs containing type-and-value pairs we also use the flags to locate 
	the end of the SEQUENCE encoding/start of the next type-and-value entry).  
	The use of SEQEND gets extremely complicated in the presence of optional
	nested SEQUENCEs because it's not certain how many levels we need to 
	undo.  Consider for example name constraints:

		SEQUENCE {
			permittedSubtrees	[ 0 ] SEQUENCE OF {
				SEQUENCE { GeneralName }
				} OPTIONAL,
			excludedSubtrees	[ 1 ] SEQUENCE OF {
				SEQUENCE { GeneralName }
				} OPTIONAL,
			}

	Is the value at the end FL_SEQEND or FL_SEQEND_3?  If excludedSubtrees 
	are absent then it's FL_SEQEND but if we're encoding the excluded 
	subtree then it's FL_SEQEND_3.  Because of this ambiguity the current 
	encoding routines simply assume that once they reach the end of an 
	extension there's an implicit FL_SEQEND_whatever there.  Luckily all of 
	the ambiguous decoding-level points occur at the end of extensions so 
	this is a workable way to handle things.

	Decoding level flags that indicate the compliance level at which this
	attribute is decoded.

	The object subtypes for which an attribute is valid.  CRLs actually 
	contain two sets of extensions, one for the entire CRL (crlExtensions) 
	and the other for each entry in the CRL (crlEntryExtension).  Sorting 
	out whether we're adding a CRL extension or per-entry extension is 
	handled by the higher-level code which references the CRL attribute list 
	or per-entry attribute list as appropriate.

   The total-attribute flags are:

	FL_ATTR_ATTRSTART/FL_ATTR_ATTREND: Marks the start and end of an 
		attribute.  This has to be done explicitly because there's no other 
		field or flag that we can guarantee will always be set for the first 
		or last field in an attribute and never for any other field.

	FL_ATTR_NOCOPY: The attribute is regarded as sensitive and therefore 
		shouldn't be copied from source to destination (e.g. from a 
		certificate request into a certificate) when the other attributes 
		are copied.
    
	FL_ATTR_CRITICAL: The overall extension is marked critical when encoding.

   The encoding flags are:

	FL_DEFAULT: The field has a default value that's set if no field data
		is present.

	FL_EXPLICIT: The field is explicitly tagged so instead of being en/
		decoded using the tag for the field it's given a second level of 
		tagging that encapsulates the field's actual tag type.

	FL_IDENTIFIER: Used to mark the encapsulating SEQUENCE in fields of the 
		type:

		SEQUENCE {
			identifier	OBJECT IDENTIFIER
			data		ANY DEFINED BY identifier
			}

		for which the field identified by a CRYPT_CERTINFO_xxx is the 'data'
		field and the whole is only encoded if the data field is present.

	FL_MULTIVALUED: If a cryptlib-level attribute is part of a SET OF x/
		SEQUENCE OF x then this flag is set to indicate that more than one 
		instance can exist at the same time.  If this flag isn't set then 
		cryptlib will detect that an attribute of that type already exists 
		and refuse to allow a second instance to be added.

	FL_EMPTYOK: Used for a SET/SEQUENCE consisting of nothing but OPTIONAL
		elements to indicate that it's OK to end up with a zero-length 
		entry, by default we don't allow an empty SET/SEQUENCE since if the 
		data didn't match any of the optional elements then the decoder 
		would get stuck in an endless loop.  At the moment this is OK 
		because the only time we can have an empty SEQUENCE is for the 
		basicConstraints extension, which is an entire extension for which
		the termination point is easily detected.

	FL_NONENCODING: The field is read and written but not associated with 
		any user data.  This is used for fields such as version numbers that 
		aren't used for encoding user-supplied data but that must be read and 
		written when processing an attribute 

	FL_OPTIONAL: The field is optional.

	FL_SETOF: Applied to the encapsulating SET/SEQUENCE of a SET OF x/
		SEQUENCE OF x to indicate that one or more inner fields may be 
		present.  The field marked with FL_SETOF in the encoding/decoding
		table is bookmarked, if all of the SET/SEQUENCE data isn't read the 
		first time through then the decoding table position is restarted 
		from the bookmark until the SET/SEQUENCE data is exhausted.

	FL_SPECIALENCODING: The attribute isn't encoded as part of the standard
		attributes but requires special-case encoding.  This exists for use
		with certificate requests to indicate that the attribute isn't 
		encoded encapsulated inside an extensionRequest but as a standalone
		attribute, because certificate request attributes are usually 
		encapsulated inside an extensionRequest wrapper, however occasionally 
		an attribute needs to be encoded in non-encapsulated form.  
		
		In addition this is just as much an attribute flag as an encoding 
		one since it controls the encoding of an entire attribute, we group 
		it with the encoding flags mostly because there's no room for 
		expansion any more in the attribute flags */

/* Type information and whole-attribute flags */

#define FL_VALID_CERT		0x0001		/* Valid in a certificate */
#define FL_VALID_ATTRCERT	0x0002		/* Valid in an attribute cert */
#define FL_VALID_CRL		0x0004		/* Valid in a CRL */
#define FL_VALID_CERTREQ	0x0008		/* Valid in a cert.request */
#define FL_VALID_REVREQ		0x0010		/* Valid in a rev.request */
#define FL_VALID_OCSPREQ	0x0010		/* Valid in an OCSP request */
#define FL_VALID_OCSPRESP	0x0010		/* Valid in an OCSP response */

#define FL_LEVEL_OBLIVIOUS	0x0000		/* Process at oblivious compliance level */
#define FL_LEVEL_REDUCED	0x0100		/* Process at reduced compliance level */
#define FL_LEVEL_STANDARD	0x0200		/* Process at standard compliance level */
#define FL_LEVEL_PKIX_PARTIAL 0x0300	/* Process at partial PKIX compliance level */
#define FL_LEVEL_PKIX_FULL	0x0400		/* Process at full PKIX compliance level */

#define FL_ATTR_NOCOPY		0x1000		/* Attr.isn't copied when attrs.copied */
#define FL_ATTR_CRITICAL	0x2000		/* Extension is marked critical */
#define FL_ATTR_ATTRSTART	0x4000		/* Start-of-attribute marker */
#define FL_ATTR_ATTREND		0x8000		/* End-of-attribute marker */

#define FL_VALID_MASK		0x1F		/* Mask for type-validity value */
#define FL_LEVEL_SHIFT		8			/* Shift amount to get into range 0...n */
#define FL_LEVEL_MASK		7			/* Mask for compliance level value */

/* Encoding flags */

#define FL_SEQEND			0x0001		/* End of constructed object */
#define FL_SEQEND_2			0x0002		/*  End of cons.obj + one nesting lvl.*/
#define FL_SEQEND_3			0x0003		/*  End of cons.obj + two nesting lvls.*/
#define FL_SEQEND_4			0x0004		/*  End of cons.obj + three nesting lvls.*/
#define FL_SEQEND_5			0x0005		/*  End of cons.obj + four nesting lvls.*/
#define FL_SEQEND_6			0x0006		/*  End of cons.obj + four nesting lvls.*/
#define FL_SEQEND_7			0x0007		/*  End of cons.obj + four nesting lvls.*/

#define FL_SEQEND_MASK		7			/* Mask for sequence control value */

#define FL_OPTIONAL			0x0010		/* Field is optional */
#define FL_DEFAULT			0x0020		/* Field has default value */
#define FL_EXPLICIT			0x0040		/* Field is explicitly tagged */
#define FL_IDENTIFIER		0x0080		/* Following field contains selection OID */
#define FL_SETOF			0x0100		/* Start of SET/SEQ OF values */
#define FL_EMPTYOK			0x0200		/* SET/SEQ may be empty */
#define FL_NONENCODING		0x0400		/* Field is a non-encoding value */
#define FL_MULTIVALUED		0x0800		/* Field can occur multiple times */
#define FL_SPECIALENCODING	0x1000		/* Attribute requires special-case encoding */

/* If a constructed field is nested (for example a SEQUENCE OF SEQUENCE) 
   then the FL_SEQEND may need to denote multiple levels of unnesting.  This 
   is done by using FL_SEQEND_n, the following macro can be used to extract 
   the actual level of nesting */

#define decodeNestingLevel( value )		( ( value ) & FL_SEQEND_MASK )

/* In order to be able to process broken certificates we allow for 
   processing them at various levels of standards compliance.  If the 
   current processing level is below that required for the extension then 
   we skip it and treat it as a blob extension */

#define decodeComplianceLevel( value ) \
		( ( ( value ) >> FL_LEVEL_SHIFT ) & FL_LEVEL_MASK )

/* Determine whether an attribute information entry represents the start of 
   the attribute */

#define isAttributeStart( attributeInfoPtr ) \
		( ( attributeInfoPtr )->typeInfoFlags & FL_ATTR_ATTRSTART )

/****************************************************************************
*																			*
*							Special-case Field Values						*
*																			*
****************************************************************************/

/* Some fields have an intrinsic value but no explicitly set value (that is,
   their mere presence communicates the information they are intended to 
   convey but the fields themselves contain no actual data).  This applies 
   for fields that contain OIDs that denote certain things such as 
   certificate policies or key usage.  To denote these identifier fields 
   the field type is set to FIELDTYPE_IDENTIFIER (note that we start the
   values at -2 rather than -1, which is the CRYPT_ERROR value).  When a 
   field of this type is encountered no data value is recorded but the OID 
   for the field is written to the certificate when the field is encoded.
   The parentheses are to catch potential erroneous use in an expression */

#define FIELDTYPE_IDENTIFIER	( -2 )

/* Some fields have no set value (these arise from ANY DEFINED BY
   definitions) or an opaque value (typically fixed parameters for type-and-
   value pairs).  To denote these fields the field type is set to
   FIELDTYPE_BLOB.  However this causes problems with type-checking since
   now absolutely anything can be passed in as valid data.  To allow at
   least some type-checking we provide a hint as to the general encoding of 
   the blob, which can be one of SEQUENCE, BIT STRING, or a genuine blob 
   used in ANY DEFINED BY constructions, for which we can at least check 
   that it's some sort of ASN.1 object.  There are also two cases in which
   an ANY blob is used where it'd be possible to use more specific blobs
   for OBJECT IDENTIFIERs and GeneralNames and that's in { OID, value }
   selection lists in which the ANY blob acts spefically as an end-of-
   list marker as well as being just a catchall type */

#define FIELDTYPE_BLOB_ANY				( -3 )
#define FIELDTYPE_BLOB_BITSTRING		( -4 )
#define FIELDTYPE_BLOB_SEQUENCE			( -5 )

/* When a field contains a CHOICE it can contain any one of the CHOICE 
   fields, as opposed to a FL_SETOF which can contain any of the fields that
   follow it.  Currently the only CHOICE fields contain OIDs as choices, the
   CHOICE fieldtype indicates that the value is stored in the field itself
   but the encoding is handled via a separate encoding table pointed to by
   extraData that maps the value to an OID */

#define FIELDTYPE_CHOICE		( -6 )

/* Some fields are composite fields that contain complete certificate data
   structures.  To denote these fields the field type is a special code that 
   specifies the type and the value member contains the handle or the data 
   member contains a pointer to the composite object */

#define FIELDTYPE_DN			( -7 )

/* As an extension of the above, some fields are complex enough to require
   complete alternative encoding tables.  The most obvious one is
   GeneralName but this is also used for some CHOICE types where the value
   selects a particular OID or entry from an alternative encoding table.  In
   this case the extraData member is a pointer to the alternative encoding
   table */

#define FIELDTYPE_SUBTYPED		( -8 )

/* Another variant of FIELDTYPE_DN is one where the field can contain one of
   a number of string types chosen from the ASN.1 string menagerie.  The two
   main classes that seem to be popular in standards are the DirectoryString,
   { BMPString | PrintableString | TeletexString | UniversalString | 
     UTF8String } and DisplayText, { BMPString | IA5String | UTF8String |
	 VisibleString }.  Rather than adding a list of the different string 
   types all marked as optional to the en/decoding tables (so that the 
   decoder stops whenever it reaches the one that matches the string value 
   being decoded) we provide a single TextString meta-type which has a 
   custom decoding routine that makes the appropriate choice between the 
   union of all of the above types */

#define FIELDTYPE_TEXTSTRING	( -9 )

/* Usually the field ID for the first field in an entry (the one containing
   the OID) is the overall attribute ID, however there are one or two
   exceptions in which the attribute ID and field ID are the same but are
   given in separate fields (examples of this are the altNames, which have
   a single field ID SUBJECT/ISSUERALTNAME that applies to the attribute as
   a whole but also to the one and only field in it.

   If this happens the field ID for the attribute as a whole is given the 
   value FIELDID_FOLLOWS to indicate that the actual ID is present at a 
   later point, with the first field that isn't a FIELDID_FOLLOWS code being 
   treated as the attribute ID */

#define FIELDID_FOLLOWS			( -10 )

/* Since there are multiple blob fields (due to the use of typing hints) we
   need a macro to determine whether a field is a blob of any form.  The 
   odd-looking range comparison below is because the fields have negative
   values */

#define isBlobField( field ) \
		( ( field ) <= FIELDTYPE_BLOB_ANY && \
		  ( field ) >= FIELDTYPE_BLOB_SEQUENCE )

/****************************************************************************
*																			*
*							Attribute Data Structures						*
*																			*
****************************************************************************/

/* The structure to store en/decoding information for an attribute field */

typedef struct {
	/* Information on the overall attribute.  These fields are only set
	   for overall attribute definitions */
	const BYTE FAR_BSS *oid;		/* OID for this attribute */

	/* Information on this particular field in the attribute.  The fieldType
	   is the field as defined (e.g. SEQUENCE, INTEGER), the 
	   fieldEncodingType is the field as encoded: 0 if it's the same as the 
	   field type or the tag if it's a tagged field.  The default tagging 
	   is to use implicit tags (e.g. [ 0 ] IMPLICIT SEQUENCE) with a field of 
	   type fieldType and encoding of type fieldEncodedType.  If FL_EXPLICIT 
	   is set it's an explicitly tagged field and both fields are used for 
	   the encoding */
	const CRYPT_ATTRIBUTE_TYPE fieldID;	/* Magic ID for this field */
#ifndef NDEBUG
	const char *description;		/* Text description */
#endif /* NDEBUG */
	const int fieldType;			/* ASN.1 tag/type for this field */
	const int fieldEncodedType;		/* ASN.1 tag for field as encoded */

	/* General status information */
	const int typeInfoFlags;		/* Attribute-processing */
	const int encodingFlags;		/* Encoding flags */

	/* Information to allow validity checking for this field */
	const int lowRange;				/* Min/max allowed if numeric/boolean */
	const int highRange;			/* Min/max length if string */
	const int defaultValue;			/* Default value if FL_DEFAULT set,
									   length if FIELDTYPE_BLOB */

	/* Extra data needed to process this field, either a pointer to an
	   alternative encoding table or a pointer to the validation function to
	   allow extended validity checking */
	const void *extraData;
	} ATTRIBUTE_INFO;

/* When building the debug version of the code we include a text string 
   describing the field which is being processed, this makes it easier to 
   track down the point in a certificate where cryptlib finds a problem */

#ifndef NDEBUG
  #define DESCRIPTION( text )		text,
#else
  #define DESCRIPTION( text )
#endif /* NDEBUG */

/* Attribute information flags.  These are:

	FLAG_BLOB: Disables all type-checking on the field, needed to handle
			some certificates that have invalid field encodings.

	FLAG_BLOB_PAYLOAD: Disables type checking on the field payload, for
			example checking that the chars in the string are valid for the
			given ASN.1 string type.

	FLAG_CRITICAL: The extension containing the field is marked criticial.

	FLAG_DEFAULTVALUE: The field has a value which is equal to the default
			for this field, so it doesn't get encoded.  This flag is set
			during the encoding pre-processing pass.

	FLAG_IGNORED: The field is recognised but was ignored at this compliance
			level.  This prevents the certificate from being rejected if the 
			field is marked critical.

	FLAG_LOCKED: The attribute can't be deleted once set, needed to handle
			fields that are added internally by cryptlib that shouldn't be
			deleted by users once set.

	FLAG_MULTIVALUED: Multiple instantiations of this field are allowed */

#define ATTR_FLAG_NONE			0x0000	/* No flag */
#define ATTR_FLAG_CRITICAL		0x0001	/* Critical cert extension */
#define ATTR_FLAG_LOCKED		0x0002	/* Field can't be modified */
#define ATTR_FLAG_BLOB			0x0004	/* Non-type-checked blob data */
#define ATTR_FLAG_BLOB_PAYLOAD	0x0008	/* Payload is non-type-checked blob data */
#define ATTR_FLAG_MULTIVALUED	0x0010	/* Multiple instances allowed */
#define ATTR_FLAG_DEFAULTVALUE	0x0020	/* Field has default value */
#define ATTR_FLAG_IGNORED		0x0040	/* Attribute ignored at this compl.level */
#define ATTR_FLAG_MAX			0x007F	/* Maximum possible flag value */

/* When comparing attribute fields we only want to compare relevant data and
   not incidental flags related to parsing or encoding actions.  The following
   mask defines the attribute flags that we want to compare */

#define ATTR_FLAGS_COMPARE_MASK	( ATTR_FLAG_CRITICAL )

/* The structure to hold a field of a certificate attribute */

typedef struct AL {
	/* Identification and encoding information for this attribute field or
	   attribute.  This consists of the field ID for the attribute as a
	   whole (e.g. CRYPT_CERTINFO_NAMECONSTRAINTS), for the attribute field 
	   (that is, one field of a full attribute, e.g. 
	   CRYPT_CERTINFO_EXCLUDEDSUBTREES) and for the subfield of the attribute 
	   field in the case of composite fields like GeneralNames, a pointer to 
	   the sync point used when encoding the attribute, and the encoded size 
	   of this field.  If it's a special-case attribute field such as a blob
	   field or a field with a fixed default value that's present only for
	   encoding purposes then the attributeID and fieldID are set to special 
	   values decoded by the isXXX() macros further down.  The subFieldID is 
	   only set if the fieldID is for a GeneralName field.

	   Although the field type information is contained in the
	   attributeInfoPtr it's sometimes needed before this has been set up
	   to handle special formatting requirements, for example to enable
	   special-case handling for a DN attribute field or to specify that an
	   OID needs to be decoded into its string representation before being
	   returned to the caller.  Because of this we store a copy of the field 
	   type information here to allow for this special processing */
	CRYPT_ATTRIBUTE_TYPE attributeID;/* Attribute ID */
	CRYPT_ATTRIBUTE_TYPE fieldID;	/* Attribute field ID */
	CRYPT_ATTRIBUTE_TYPE subFieldID;	/* Attribute subfield ID */
	const ATTRIBUTE_INFO *attributeInfoPtr;	/* Pointer to encoding sync point */
	int encodedSize;				/* Encoded size of this field */
	int fieldType;					/* Attribute field type */
	int flags;						/* Flags for this field */

	/* Sometimes a field is part of a constructed object or even a nested
	   series of constructed objects (these are always SEQUENCEs).  Since
	   this is purely an encoding issue there are no attribute list entries
	   for the SEQUENCE fields so when we perform the first pass over the
	   attribute list prior to encoding we remember the lengths of the
	   SEQUENCEs for later use.  Since we can have nested SEQUENCEs
	   containing a given field we store the lengths and pointers to the 
	   ATTRIBUTE_INFO table entries used to encode them in a fifo with the 
	   innermost one first and successive outer ones following it */
	ARRAY( ENCODING_FIFO_SIZE, fifoPos ) \
	int sizeFifo[ ENCODING_FIFO_SIZE + 2 ];	
									/* Encoded size of SEQUENCE containing 
									   this field, if present */
	ARRAY( ENCODING_FIFO_SIZE, fifoPos ) \
	const ATTRIBUTE_INFO *encodingFifo[ ENCODING_FIFO_SIZE + 2 ];
									/* Encoding table entry used to encode 
									   this SEQUENCE */
	int fifoEnd;					/* End of list of SEQUENCE sizes */
	int fifoPos;					/* Current position in list */

	/* The data payload for this attribute field or attribute.  If it's
	   numeric data such as a simple boolean, bitstring, or small integer,
	   we store it in the intValue member.  If it's an OID or some form of
	   string we store it in the variable-length buffer */
	long intValue;					/* Integer value for simple types */
	BUFFER_OPT_FIXED( valueLength ) \
	void *value;					/* Attribute value */
	int valueLength;				/* Attribute value length */

	/* The OID, for blob-type attributes */
	BYTE *oid;						/* Attribute OID */

	/* The previous and next list element in the linked list of elements */
	struct AL *prev, *next;

	/* Variable-length storage for the attribute data */
	DECLARE_VARSTRUCT_VARS;
	} ATTRIBUTE_LIST;

/****************************************************************************
*																			*
*								Attribute Functions							*
*																			*
****************************************************************************/

/* The validation function used to perform additional validation on fields */

typedef CHECK_RETVAL_FNPTR STDC_NONNULL_ARG( ( 1 ) ) \
		int ( *VALIDATION_FUNCTION )( const ATTRIBUTE_LIST *attributeListPtr );

/* Look up an ATTRIBUTE_INFO entry based on an OID */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
const ATTRIBUTE_INFO *oidToAttribute( IN_ENUM( ATTRIBUTE ) \
										const ATTRIBUTE_TYPE attributeType,
									  IN_BUFFER( oidLength ) const BYTE *oid, 
									  IN_LENGTH_OID const int oidLength );

/* Select the appropriate attribute info table for encoding/type checking */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getAttributeInfo( IN_ENUM( ATTRIBUTE ) const ATTRIBUTE_TYPE attributeType,
					  OUT const ATTRIBUTE_INFO **attributeInfoPtrPtr,
					  OUT_INT_Z int *noAttributeEntries );

/* Get the encoded tag for a field */

CHECK_RETVAL_RANGE( MAX_ERROR, MAX_TAG ) STDC_NONNULL_ARG( ( 1 ) ) \
int getFieldEncodedTag( const ATTRIBUTE_INFO *attributeInfoPtr );

/* Get the attribute and attributeID for a field ID */

CHECK_RETVAL_PTR \
const ATTRIBUTE_INFO *fieldIDToAttribute( IN_ENUM( ATTRIBUTE ) \
											const ATTRIBUTE_TYPE attributeType,
										  IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE fieldID, 
										  IN_ATTRIBUTE_OPT \
											const CRYPT_ATTRIBUTE_TYPE subFieldID,
										  OUT_OPT_ATTRIBUTE_Z \
											CRYPT_ATTRIBUTE_TYPE *attributeID );

/* Find the start of an attribute from an arbitrary position in an attribute 
   list */

CHECK_RETVAL_PTR \
ATTRIBUTE_LIST *findAttributeStart( IN_OPT const ATTRIBUTE_LIST *attributeListPtr );

/* Write an attribute field */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int writeAttributeField( INOUT_OPT STREAM *stream, 
						 INOUT ATTRIBUTE_LIST *attributeListPtr,
						 IN_RANGE( 0, 4 ) const int complianceLevel );

#endif /* _CERTATTR_DEFINED */
