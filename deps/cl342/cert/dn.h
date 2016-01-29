/****************************************************************************
*																			*
*						  Certificate DN Header File						*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

#ifndef _DN_DEFINED

#define _DN_DEFINED

/* DN component information flags.  These are:

	FLAG_CONTINUED: Some implementations may place more than one AVA into 
		an RDN, this flag indicates that the RDN continues in the next DN 
		component structure.  

	FLAG_LOCKED: If the RDN/DN was set by specifying the entire DN at once 
		using a free-format text DN string it's not a good idea to allow 
		random changes to it so this flag marks the components as locked.

	FLAG_NOCHECK: If we're reading data from an external source the DN can 
		contain all sorts of strange stuff so we use this flag to tell the 
		DN component-handling code not to perform any validity checking on 
		the components as they're added */

#define DN_FLAG_NONE		0x00	/* No DN flag */
#define DN_FLAG_CONTINUED	0x01	/* RDN continues with another AVA */
#define DN_FLAG_LOCKED		0x02	/* RDN can't be modified */
#define DN_FLAG_NOCHECK		0x08	/* Don't check validity of components */
#define DN_FLAG_MAX			0x0F	/* Maximum possible flag value */

/* When comparing DN fields we only want to compare relevant data and not 
   incidental flags related to parsing or encoding actions.  The following
   mask defines the attribute flags that we want to compare */

#define DN_FLAGS_COMPARE_MASK	( DN_FLAG_CONTINUED )

/* The structure to hold a DN component */

typedef struct DC {
	/* DN component type and type information */
	int type;						/* cryptlib component type, either a
									   CRYPT_ATTRIBUTE_TYPE or an integer ID */
	const void *typeInfo;			/* Type information for this component, a
									   pointer to the DN_COMPONENT_INFO tbl */
	int flags;

	/* DN component data */
	BUFFER_FIXED( valueLength ) \
	void *value;					/* DN component value */
	int valueLength;				/* DN component value length */
	int valueStringType;			/* DN component native string type, 
									   encoded as a cookie used by dnstring.c */

	/* Encoding information: The ASN.1 encoded string type as a 
	   BER_STRING_xyz, the overall size of the RDN data (without the tag and 
	   length) if this is the first or only component of an RDN, and the size 
	   of the AVA containing this component.  If it's the first component of
	   a multi-AVA RDN then the DN_FLAG_CONTINUED flag will be set in the 
	   flags field */
	int asn1EncodedStringType, encodedRDNdataSize, encodedAVAdataSize;

	/* The next and previous list element in the linked list of DN
	   components */
	struct DC *next, *prev;

	/* Variable-length storage for the DN data */
	DECLARE_VARSTRUCT_VARS;
	} DN_COMPONENT;

/* Type information for DN components */

typedef struct {
	const int type;					/* cryptlib attribute type, or index 
									   value for non-cryptlib attributes */
	const BYTE *oid;				/* OID for this type */
	ARRAY_FIXED( nameLen ) \
	const char *name;				/* Name for this type */
	const int nameLen;
	ARRAY_FIXED( nameLen ) \
	const char *altName;			/* Alt. name for this type */
	const int altNameLen;
	const int maxLength;			/* Maximum allowed length for this type */
	const BOOLEAN ia5OK;			/* Whether IA5 is allowed for this comp.*/
	const BOOLEAN wcsOK;			/* Whether widechar is allowed for comp.*/
	} DN_COMPONENT_INFO;

/* Prototypes for functions in dn.c */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const DN_COMPONENT_INFO *findDNInfoByOID( IN_BUFFER( oidLength ) const BYTE *oid, 
										  IN_LENGTH_OID const int oidLength );
#ifdef USE_CERT_DNSTRING
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const DN_COMPONENT_INFO *findDNInfoByLabel( IN_BUFFER( labelLength ) const char *label, 
											IN_LENGTH_SHORT const int labelLength );
#endif /* USE_CERT_DNSTRING */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 7 ) ) \
int insertDNstring( INOUT DN_COMPONENT **dnComponentListPtrPtr, 
					IN_INT const int type,
					IN_BUFFER( valueLength ) const void *value, 
					IN_LENGTH_SHORT const int valueLength,
					IN_RANGE( 1, 20 ) const int valueStringType,
					IN_FLAGS_Z( DN ) const int flags, 
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );

/* Prototypes for functions in dnstring.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int copyToAsn1String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
					  IN_LENGTH_SHORT const int destMaxLen, 
					  OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen, 
					  IN_BUFFER( sourceLen ) const void *source, 
					  IN_LENGTH_SHORT const int sourceLen,
					  IN_RANGE( 0, 20 ) const int stringType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
int copyFromAsn1String( OUT_BUFFER( destMaxLen, *destLen ) void *dest, 
						IN_LENGTH_SHORT const int destMaxLen, 
						OUT_LENGTH_BOUNDED_Z( destMaxLen ) int *destLen, 
						OUT_RANGE( 0, 20 ) int *destStringType,
						IN_BUFFER( sourceLen ) const void *source, 
						IN_LENGTH_SHORT const int sourceLen,
						IN_TAG_ENCODED const int stringTag );

#endif /* _DN_DEFINED */
