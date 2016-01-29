/****************************************************************************
*																			*
*						Certificate Function Header File 					*
*						Copyright Peter Gutmann 1996-2008					*
*																			*
****************************************************************************/

/* The huge complexity of the certificate management code means that there
   are a sufficient number of functions required that we confine the
   prototypes to their own file */

#ifndef _CERTFN_DEFINED

#define _CERTFN_DEFINED

/****************************************************************************
*																			*
*							DN Manipulation Functions						*
*																			*
****************************************************************************/

/* DN string functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5 ) ) \
int getAsn1StringInfo( IN_BUFFER( stringLen ) const void *string, 
					   IN_LENGTH_SHORT const int stringLen,
					   OUT_RANGE( 0, 20 ) int *stringType, 
					   OUT_TAG_ENCODED_Z int *asn1StringType,
					   OUT_LENGTH_SHORT_Z int *asn1StringLen,
					   const BOOLEAN isNativeString );

/* DN manipulation routines */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int insertDNComponent( INOUT_PTR DN_PTR **dnComponentListPtrPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE componentType,
					   IN_BUFFER( valueLength ) const void *value, 
					   IN_LENGTH_SHORT const int valueLength,
					   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteDNComponent( INOUT_PTR DN_PTR **dnComponentListPtrPtr, 
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
					   IN_BUFFER_OPT( valueLength ) const void *value, 
					   IN_LENGTH_SHORT_Z const int valueLength );
STDC_NONNULL_ARG( ( 1 ) ) \
void deleteDN( INOUT_PTR DN_PTR **dnComponentListPtrPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int getDNComponentInfo( INOUT const DN_PTR *dnComponentList,
						OUT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *type,
						OUT_BOOL BOOLEAN *dnContinues );
CHECK_RETVAL STDC_NONNULL_ARG( ( 6 ) ) \
int getDNComponentValue( IN_OPT const DN_PTR *dnComponentList,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
						 IN_RANGE( 0, 100 ) const int count,
						 OUT_BUFFER_OPT( valueMaxLength, \
										 *valueLength ) void *value, 
						 IN_LENGTH_SHORT_Z const int valueMaxLength, 
						 OUT_LENGTH_BOUNDED_Z( valueMaxLength ) \
							int *valueLength );

/* Copy and compare a DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyDN( OUT_PTR_COND DN_PTR **dnDest, 
			IN const DN_PTR *dnSrc );
CHECK_RETVAL_BOOL \
BOOLEAN compareDN( IN_OPT const DN_PTR *dnComponentList1,
				   IN_OPT const DN_PTR *dnComponentList2,
				   const BOOLEAN dn1substring,
				   OUT_OPT_PTR_xCOND DN_PTR **mismatchPointPtrPtr );

/* Select DN/GeneralName components */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectGeneralName( INOUT CERT_INFO *certInfoPtr,
					   IN_ATTRIBUTE_OPT const CRYPT_ATTRIBUTE_TYPE certInfoType,
					   IN_ENUM( SELECTION_OPTION ) const SELECTION_OPTION option );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectGeneralNameComponent( INOUT CERT_INFO *certInfoPtr,
								IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE certInfoType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int selectDN( INOUT CERT_INFO *certInfoPtr, 
			  IN_ATTRIBUTE_OPT const CRYPT_ATTRIBUTE_TYPE certInfoType,
			  IN_ENUM( SELECTION_OPTION ) const SELECTION_OPTION option );

/* Read/write a DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4 ) ) \
int checkDN( IN_OPT const DN_PTR *dnComponentList,
			 IN_FLAGS( CHECKDN ) const int checkFlags,
			 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
				CRYPT_ATTRIBUTE_TYPE *errorLocus,
			 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
				CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL_LENGTH \
int sizeofDN( INOUT_OPT DN_PTR *dnComponentList );
			  /* Non-const because it performs a pre-encoding pass */
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readDN( INOUT STREAM *stream, 
			OUT_PTR_COND DN_PTR **dnComponentListPtrPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeDN( INOUT STREAM *stream, 
			 IN_OPT const DN_PTR *dnComponentList,
			 IN_TAG const int tag );
#ifdef USE_CERT_DNSTRING
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readDNstring( INOUT_PTR DN_PTR **dnComponentListPtrPtr,
				  IN_BUFFER( stringLength ) const BYTE *string, 
				  IN_LENGTH_ATTRIBUTE const int stringLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeDNstring( INOUT STREAM *stream, 
				   IN_OPT const DN_PTR *dnComponentList );
#endif /* USE_CERT_DNSTRING */

/****************************************************************************
*																			*
*						Attribute Manipulation Functions					*
*																			*
****************************************************************************/

/* Find an attribute */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
ATTRIBUTE_PTR *findAttributeByOID( IN_OPT const ATTRIBUTE_PTR *attributePtr,
								   IN_BUFFER( oidLength ) const BYTE *oid, 
								   IN_LENGTH_OID const int oidLength );
CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttribute( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attributeID,
							  const BOOLEAN isFieldID );
CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttributeField( IN_OPT const ATTRIBUTE_PTR *attributePtr,
								   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
								   IN_ATTRIBUTE_OPT \
										const CRYPT_ATTRIBUTE_TYPE subFieldID );
CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *findAttributeFieldEx( IN_OPT const ATTRIBUTE_PTR *attributePtr,
									 IN_ATTRIBUTE \
										const CRYPT_ATTRIBUTE_TYPE fieldID );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ATTRIBUTE_PTR *findNextFieldInstance( const ATTRIBUTE_PTR *attributePtr );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ATTRIBUTE_PTR *findDnInAttribute( IN_OPT const ATTRIBUTE_PTR *attributePtr );

/* Get/set information about an attribute:

	PROPERTY_BLOBATTRIBUTE: The item contains a single blob-type attribute.

	PROPERTY_COMPLETEATTRIBUTE: This item isn't explicitly present in the 
		attribute list but represents an entire (constructed) attribute of
		which one field is present, see the long comment for 
		findAttributeFieldEx() in cert/ext.c for a detailed description.

	PROPERTY_CRITICAL: The item (which should be a complete attribute) has
		the critical flag set.

	PROPERTY_DEFAULTVALUE: The item is a dummy placeholder entry containing
		a default value for an attribute, this field isn't explicitly 
		present in the attribute list but exists only to contain this 
		default value.
	
	PROPERTY_DN: The item contains a composite DN rather than an integer/
		boolean/data value.

	PROPERTY_IGNORED: This item is a recognised attribute but is ignored at 
		the current compliance level.

	PROPERTY_VALUE: The integer value for the attribute.  This isn't really 
		an attribute property but we need to be able to set it in a few rare 
		cases when we're applying a constraint to an attribute where the 
		constraint modifies the attribute's integer value.

	PROPERTY_LOCKED: This item is locked against further changes.

	PROPERTY_OID: The data in this item is an encoded OID that needs to be
		decoded into the OID text representation before being returned to 
		the caller */

typedef enum {
	ATTRIBUTE_PROPERTY_NONE,		/* No attribute property type */
	ATTRIBUTE_PROPERTY_DEFAULTVALUE,/* Field has default value */
	ATTRIBUTE_PROPERTY_BLOBATTRIBUTE,	/* Item is a single blob attribute */
	ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE,/* Item is a complete attribute */
	ATTRIBUTE_PROPERTY_LOCKED,		/* Item is locked against changes */
	ATTRIBUTE_PROPERTY_CRITICAL,	/* Attribute is critical */
	ATTRIBUTE_PROPERTY_DN,			/* Attribute contains composite DN */
	ATTRIBUTE_PROPERTY_OID,			/* Attribute data is an OID */
	ATTRIBUTE_PROPERTY_IGNORED,		/* Attribute is ignored */
	ATTRIBUTE_PROPERTY_VALUE,		/* Attribute integer value */
	ATTRIBUTE_PROPERTY_LAST			/* Last possible property type */
	} ATTRIBUTE_PROPERTY_TYPE;

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkAttributeProperty( const ATTRIBUTE_PTR *attributePtr,
								IN_ENUM( ATTRIBUTE_PROPERTY ) \
									ATTRIBUTE_PROPERTY_TYPE property );
STDC_NONNULL_ARG( ( 1 ) ) \
void setAttributeProperty( INOUT ATTRIBUTE_PTR *attributePtr,
						   IN_ENUM( ATTRIBUTE_PROPERTY ) \
								ATTRIBUTE_PROPERTY_TYPE property,
						   IN_INT_Z const int optValue );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int getAttributeIdInfo( const ATTRIBUTE_PTR *attributePtr,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *attributeID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *fieldID,
						OUT_OPT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *subFieldID );
CHECK_RETVAL \
int getDefaultFieldValue( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID );
CHECK_RETVAL_BOOL \
BOOLEAN checkAttributePresent( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID );
CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeFieldPresent( IN_OPT const ATTRIBUTE_PTR *attributePtr,
									IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID );

/* Get attribute data.  See the comment by the SELECTION_INFO definition for
   why dnPtr uses double indirection for the pointer instead of single
   indirection */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataValue( IN const ATTRIBUTE_PTR *attributePtr,
						   OUT_INT_Z int *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataTime( IN const ATTRIBUTE_PTR *attributePtr,
						  OUT time_t *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getAttributeDataDN( IN const ATTRIBUTE_PTR *attributePtr,
						OUT_PTR DN_PTR ***dnPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int getAttributeDataPtr( IN const ATTRIBUTE_PTR *attributePtr,
						 OUT_BUFFER_ALLOC( *dataLength ) void **dataPtrPtr, 
						 OUT_LENGTH_SHORT_Z int *dataLength );

/* The pattern { findAttributeField(), getAttributeDataXYZ() } is used 
   frequently enough that we provide a common function for it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldValue( IN_OPT const ATTRIBUTE_PTR *attributePtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
							IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
							OUT_INT_Z int *value );
CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
int getAttributeFieldTime( IN_OPT const ATTRIBUTE_PTR *attributePtr,
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
						   IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
						   OUT time_t *value );

/* Enumerate entries in an attribute list.  This is a somewhat oddball 
   function that's required to handle a small number of special-case
   situations that can't be easily handled directly.  The alternative to 
   having the hardwired selections is to provide a general-purpose 
   enumeration capability and then have the caller extract whatever's 
   necessary from the attribute and decide whether they want to continue,
   however this creates really akward attribute-enumeration loops and,
   since there are only three cases that we need to handle, really isn't 
   worth the effort */

typedef enum {
	ATTRIBUTE_ENUM_NONE,		/* No attribute enumeration type */
	ATTRIBUTE_ENUM_BLOB,		/* Enumerate blob attributes */
	ATTRIBUTE_ENUM_NONBLOB,		/* Enumerate non-blob attributes */
	ATTRIBUTE_ENUM_LAST			/* Last possible attribute enumeration type */
	} ATTRIBUTE_ENUM_TYPE;

typedef struct {
	const ATTRIBUTE_PTR *attributePtr;	/* Currently selected attribute entry */
	ATTRIBUTE_ENUM_TYPE enumType;		/* Type of enumeration being performed */
	} ATTRIBUTE_ENUM_INFO;

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_PTR *getFirstAttribute( OUT ATTRIBUTE_ENUM_INFO *attrEnumInfo,
										IN_OPT const ATTRIBUTE_PTR *attributePtr,
										IN_ENUM( ATTRIBUTE_ENUM ) \
											const ATTRIBUTE_ENUM_TYPE enumType );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const ATTRIBUTE_PTR *getNextAttribute( INOUT ATTRIBUTE_ENUM_INFO *attrEnumInfo );

/* Since many of the attributes can be disabled to save space and reduce 
   complexity, we may need to check that an attribute that we want to use is
   actually available, for example if we're about to create it as part of an
   internal operation for which we don't want to present an unexpected error
   status to the caller.  The following function checks whether an attribute
   is enabled for use */

CHECK_RETVAL_BOOL \
BOOLEAN checkAttributeAvailable( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID );

/* Move the current attribute cursor.  The reason for the apparently-
   reversed values in the IN_RANGE() annotation are because the values are 
   -ve, so last comes before first  */

CHECK_RETVAL_PTR \
ATTRIBUTE_PTR *certMoveAttributeCursor( IN_OPT const ATTRIBUTE_PTR *currentCursor,
										IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType,
										IN_RANGE( CRYPT_CURSOR_LAST, \
												  CRYPT_CURSOR_FIRST ) \
											const int position );

/* For range-checking purposes we need to have ATTR_FLAG_NONE and 
   ATTR_FLAG_MAX defined, since these are defined in certattr.h which isn't
   visible in all certificate-using code we explicitly define the values 
   here if required */

#ifndef ATTR_FLAG_NONE
  #define ATTR_FLAG_NONE	0x0000
  #define ATTR_FLAG_MAX		0x007F
#endif /* ATTR_FLAG_NONE */
#if ATTR_FLAG_MAX != 0x007F
  #error Inconsistent definition of ATTR_FLAG_MAX in certattr.h/certfn.h
#endif /* ATTR_FLAG_MAX != 0x007F */

/* Add/delete/copy attributes/attribute fields */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 6 ) ) \
int addAttribute( IN_ATTRIBUTE const ATTRIBUTE_TYPE attributeType,
				  INOUT ATTRIBUTE_PTR **listHeadPtr, 
				  IN_BUFFER( oidLength ) const BYTE *oid, 
				  IN_LENGTH_OID const int oidLength,
				  const BOOLEAN critical, 
				  IN_BUFFER( dataLength ) const void *data, 
				  IN_LENGTH_SHORT const int dataLength, 
				  IN_FLAGS_Z( ATTR ) const int flags );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6, 7 ) ) \
int addAttributeField( INOUT ATTRIBUTE_PTR **listHeadPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
					   IN_ATTRIBUTE_OPT \
							const CRYPT_ATTRIBUTE_TYPE subFieldID,
					   const int value,
					   IN_FLAGS_Z( ATTR ) const int flags, 
					   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
					   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 7, 8 ) ) \
int addAttributeFieldString( INOUT ATTRIBUTE_PTR **listHeadPtr,
							 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
							 IN_ATTRIBUTE_OPT \
								const CRYPT_ATTRIBUTE_TYPE subFieldID,
							 IN_BUFFER( dataLength ) const void *data, 
							 IN_LENGTH_ATTRIBUTE const int dataLength,
							 IN_FLAGS_Z( ATTR ) const int flags, 
							 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int deleteAttributeField( INOUT ATTRIBUTE_PTR **attributePtr,
						  INOUT_OPT ATTRIBUTE_PTR **listCursorPtr,
						  INOUT ATTRIBUTE_PTR *listItem,
						  IN_OPT const DN_PTR *dnCursor );
RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int deleteCompositeAttributeField( INOUT ATTRIBUTE_PTR **attributePtr,
								   INOUT ATTRIBUTE_PTR **listCursorPtr,
								   INOUT ATTRIBUTE_PTR *listItem,
								   IN_OPT const DN_PTR *dnCursor );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int deleteAttribute( INOUT ATTRIBUTE_PTR **attributePtr,
					 INOUT_OPT ATTRIBUTE_PTR **listCursorPtr,
					 INOUT ATTRIBUTE_PTR *listItem,
					 IN_OPT const DN_PTR *dnCursor );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int deleteCompleteAttribute( INOUT ATTRIBUTE_PTR **attributeListPtr,
							 INOUT ATTRIBUTE_PTR **listCursorPtr,
							 const CRYPT_ATTRIBUTE_TYPE attributeID,
							 IN_OPT const DN_PTR *dnCursor );
STDC_NONNULL_ARG( ( 1 ) ) \
void deleteAttributes( INOUT ATTRIBUTE_PTR **attributePtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int copyAttributes( INOUT ATTRIBUTE_PTR **destHeadPtr,
					const ATTRIBUTE_PTR *srcPtr,
					OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int copyIssuerAttributes( INOUT ATTRIBUTE_PTR **destListHeadPtr,
						  const ATTRIBUTE_PTR *srcListPtr,
						  const CRYPT_CERTTYPE_TYPE type,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
#ifdef USE_CERTREQ
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyCRMFRequestAttributes( INOUT ATTRIBUTE_PTR **destListHeadPtr,
							   const ATTRIBUTE_PTR *srcListPtr );
#endif /* USE_CERTREQ */
#ifdef USE_CERTVAL
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyRTCSRequestAttributes( INOUT ATTRIBUTE_PTR **destListHeadPtr,
							   const ATTRIBUTE_PTR *srcListPtr );
#endif /* USE_CERTVAL */
#ifdef USE_CERTREV
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyOCSPRequestAttributes( INOUT ATTRIBUTE_PTR **destListHeadPtr,
							   const ATTRIBUTE_PTR *srcListPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyRevocationAttributes( INOUT ATTRIBUTE_PTR **destListHeadPtr,
							  const ATTRIBUTE_PTR *srcListPtr );
#endif /* USE_CERTREV */

/* Read/write a collection of attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
int checkAttributes( IN_ENUM( ATTRIBUTE ) const ATTRIBUTE_TYPE attributeType,
					 const ATTRIBUTE_PTR *listHeadPtr,
					 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL \
int sizeofAttributes( IN_OPT const ATTRIBUTE_PTR *attributePtr,
					  IN_ENUM_OPT( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE type );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeAttributes( INOUT STREAM *stream, 
					 INOUT ATTRIBUTE_PTR *attributePtr,
					 IN_ENUM_OPT( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
					 IN_LENGTH const int attributeSize );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5, 6 ) ) \
int readAttributes( INOUT STREAM *stream, 
					INOUT ATTRIBUTE_PTR **attributePtrPtr,
					IN_ENUM_OPT( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type, 
					IN_LENGTH_Z const int attributeLength,
					OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );

/****************************************************************************
*																			*
*					Validity Information Processing Functions				*
*																			*
****************************************************************************/

#ifdef USE_CERTVAL

/* Read/write validity information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofRtcsRequestEntry( STDC_UNUSED const VALIDITY_INFO *rtcsEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readRtcsRequestEntry( INOUT STREAM *stream, 
						  INOUT_PTR VALIDITY_INFO **listHeadPtrPtr );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeRtcsRequestEntry( INOUT STREAM *stream, 
						   const VALIDITY_INFO *rtcsEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofRtcsResponseEntry( INOUT VALIDITY_INFO *rtcsEntry,
							 const BOOLEAN isFullResponse );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readRtcsResponseEntry( INOUT STREAM *stream, 
						   INOUT_PTR VALIDITY_INFO **listHeadPtrPtr,
						   INOUT CERT_INFO *certInfoPtr,
						   const BOOLEAN isFullResponse );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeRtcsResponseEntry( INOUT STREAM *stream, 
						    const VALIDITY_INFO *rtcsEntry,
							const BOOLEAN isFullResponse );

/* Add/delete a validity entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addValidityEntry( INOUT_PTR VALIDITY_INFO **listHeadPtrPtr,
					  OUT_OPT_PTR_COND VALIDITY_INFO **newEntryPosition,
					  IN_BUFFER( valueLength ) const void *value, 
					  IN_LENGTH_FIXED( KEYID_SIZE ) const int valueLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3, 4 ) ) \
int prepareValidityEntries( IN_OPT const VALIDITY_INFO *listPtr, 
							OUT_PTR_xCOND VALIDITY_INFO **errorEntry,
							OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType );
STDC_NONNULL_ARG( ( 1 ) ) \
void deleteValidityEntries( INOUT_PTR VALIDITY_INFO **listHeadPtrPtr );

/* Copy a set of validity entries */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyValidityEntries( INOUT_PTR VALIDITY_INFO **destListHeadPtrPtr,
						 const VALIDITY_INFO *srcListPtr );

/* Check a certificate's validity status */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkRTCSResponse( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE const CRYPT_KEYSET iCryptKeyset );

#endif /* USE_CERTVAL */

/****************************************************************************
*																			*
*					Revocation Information Processing Functions				*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* Read/write revocation information */

CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCRLentry( INOUT REVOCATION_INFO *crlEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int readCRLentry( INOUT STREAM *stream, 
				  INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
				  IN_LENGTH_Z const int entryNo,
				  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
					CRYPT_ATTRIBUTE_TYPE *errorLocus,
				  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
					CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCRLentry( INOUT STREAM *stream, 
				   const REVOCATION_INFO *crlEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofOcspRequestEntry( INOUT REVOCATION_INFO *ocspEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOcspRequestEntry( INOUT STREAM *stream, 
						  INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						  INOUT CERT_INFO *certInfoPtr );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOcspRequestEntry( INOUT STREAM *stream, 
						   const REVOCATION_INFO *ocspEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofOcspResponseEntry( INOUT REVOCATION_INFO *ocspEntry );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
int readOcspResponseEntry( INOUT STREAM *stream, 
						   INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						   INOUT CERT_INFO *certInfoPtr );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeOcspResponseEntry( INOUT STREAM *stream, 
							const REVOCATION_INFO *ocspEntry,
							const time_t entryTime );

/* Add/delete a revocation entry */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int addRevocationEntry( INOUT_PTR REVOCATION_INFO **listHeadPtrPtr,
						OUT_OPT_PTR_COND REVOCATION_INFO **newEntryPosition,
						IN_KEYID_OPT const CRYPT_KEYID_TYPE valueType,
						IN_BUFFER( valueLength ) const void *value, 
						IN_LENGTH_SHORT const int valueLength,
						const BOOLEAN noCheck );

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5, 6 ) ) \
int prepareRevocationEntries( INOUT_OPT REVOCATION_INFO *listPtr, 
							  const time_t defaultTime,
							  OUT_PTR_xCOND REVOCATION_INFO **errorEntry,
							  const BOOLEAN isSingleEntry,
							  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType );
STDC_NONNULL_ARG( ( 1 ) ) \
void deleteRevocationEntries( INOUT_PTR REVOCATION_INFO **listHeadPtrPtr );

/* Copy a set of revocation entries */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyRevocationEntries( INOUT_PTR REVOCATION_INFO **destListHeadPtrPtr,
						   const REVOCATION_INFO *srcListPtr );

/* Check a certificate's revocation status */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCRL( INOUT CERT_INFO *certInfoPtr, 
			  IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkOCSPResponse( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE const CRYPT_KEYSET iCryptKeyset );

#endif /* USE_CERTREV */

/****************************************************************************
*																			*
*							Certificate Checking Functions					*
*																			*
****************************************************************************/

/* Check a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertBasic( INOUT CERT_INFO *subjectCertInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
int checkCert( INOUT CERT_INFO *subjectCertInfoPtr,
			   IN_OPT const CERT_INFO *issuerCertInfoPtr,
			   const BOOLEAN shortCircuitCheck,
			   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
					CRYPT_ATTRIBUTE_TYPE *errorLocus,
			   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
					CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertChain( INOUT CERT_INFO *certInfoPtr );

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int getKeyUsageFromExtKeyUsage( const CERT_INFO *certInfoPtr,
								OUT_FLAGS_Z( CRYPT_KEYUSAGE ) int *keyUsage,
								OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus, 
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5, 6 ) ) \
int checkKeyUsage( const CERT_INFO *certInfoPtr,
				   IN_FLAGS_Z( CHECKKEY ) const int flags, 
				   IN_FLAGS_Z( CRYPT_KEYUSAGE ) const int specificUsage,
				   IN_RANGE( CRYPT_COMPLIANCELEVEL_OBLIVIOUS, \
							 CRYPT_COMPLIANCELEVEL_LAST - 1 ) \
						const int complianceLevel,
				   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
				   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );

/* Check certificate constraints */

#ifdef USE_CERTLEVEL_PKIX_FULL
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5 ) ) \
int checkNameConstraints( const CERT_INFO *subjectCertInfoPtr,
						  const ATTRIBUTE_PTR *issuerAttributes,
						  const BOOLEAN isExcluded,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN isAnyPolicy( const ATTRIBUTE_PTR *attributePtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 7, 8 ) ) \
int checkPolicyConstraints( const CERT_INFO *subjectCertInfoPtr,
							const ATTRIBUTE_PTR *issuerAttributes,
							IN_ENUM_OPT( POLICY ) const POLICY_TYPE policyType,
							IN_OPT const POLICY_INFO *policyInfo,
							IN_RANGE( 0, MAX_CHAINLENGTH ) const int policyLevel,
							const BOOLEAN allowMappedPolicies,
							OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
								CRYPT_ATTRIBUTE_TYPE *errorLocus,
							OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
								CRYPT_ERRTYPE_TYPE *errorType );
#endif /* USE_CERTLEVEL_PKIX_FULL */
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int checkPathConstraints( const CERT_INFO *subjectCertInfoPtr,
						  IN_LENGTH_SHORT_Z const int pathLength,
						  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
							CRYPT_ATTRIBUTE_TYPE *errorLocus,
						  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

/* Sign/sig check a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int signCert( INOUT CERT_INFO *certInfoPtr, 
			  IN_HANDLE_OPT const CRYPT_CONTEXT iSignContext );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkCertValidity( INOUT CERT_INFO *certInfoPtr, 
					   IN_HANDLE_OPT const CRYPT_HANDLE iSigCheckObject );

/****************************************************************************
*																			*
*							Certificate Chain Functions						*
*																			*
****************************************************************************/

/* Read/write/copy a certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCertChain( INOUT STREAM *stream, 
				   OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
				   IN_HANDLE const CRYPT_USER iCryptOwner,
				   IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
				   IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
				   IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
				   IN_LENGTH_KEYID_Z const int keyIDlength,
				   IN_FLAGS( KEYMGMT ) const int options );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertChain( INOUT STREAM *stream, 
					const CERT_INFO *certInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyCertChain( INOUT CERT_INFO *certInfoPtr, 
				   IN_HANDLE const CRYPT_HANDLE certChain,
				   const BOOLEAN isCertCollection );

/* Read/write certificate collections in assorted formats */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCertCollection( const CERT_INFO *certInfoPtr,
						  IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormatType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertCollection( INOUT STREAM *stream, 
						 const CERT_INFO *certInfoPtr,
						 IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormatType );

/* Assemble a certificate chain from certificates read from an object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int assembleCertChain( OUT CRYPT_CERTIFICATE *iCertificate,
					   IN_HANDLE const CRYPT_HANDLE iCertSource,
					   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
					   IN_BUFFER( keyIDlength ) const void *keyID, 
					   IN_LENGTH_KEYID const int keyIDlength,
					   IN_FLAGS( KEYMGMT ) const int options );

/****************************************************************************
*																			*
*								Certificate Functions						*
*																			*
****************************************************************************/

/* Create a certificate object ready for further initialisation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificateInfo( OUT_PTR_COND CERT_INFO **certInfoPtrPtr, 
						   IN_HANDLE const CRYPT_USER iCryptOwner,
						   IN_ENUM( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE certType );

/* Add/get/delete a certificate component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addCertComponent( INOUT CERT_INFO *certInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					  const int certInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addCertComponentString( INOUT CERT_INFO *certInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
							IN_BUFFER( certInfoLength ) const void *certInfo, 
							IN_LENGTH_SHORT const int certInfoLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getCertComponent( INOUT CERT_INFO *certInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					  OUT_INT_Z int *certInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int getCertComponentString( INOUT CERT_INFO *certInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
							OUT_BUFFER_OPT( certInfoMaxLength, \
											*certInfoLength ) void *certInfo, 
							IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
							OUT_LENGTH_BOUNDED_Z( certInfoMaxLength ) \
								int *certInfoLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteCertComponent( INOUT CERT_INFO *certInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType );

/* Manage certificate attribute cursors */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setCertificateCursor( INOUT CERT_INFO *certInfoPtr, 
						  IN_RANGE( CRYPT_CURSOR_LAST, \
									CRYPT_CURSOR_FIRST ) /* Values are -ve */
								const int cursorMoveType );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAttributeCursor( INOUT CERT_INFO *certInfoPtr,
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
						IN const int value );

/* Import/export a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int importCert( IN_BUFFER( certObjectLength ) const void *certObject, 
				IN_DATALENGTH const int certObjectLength,
				OUT_HANDLE_OPT CRYPT_CERTIFICATE *certificate,
				IN_HANDLE const CRYPT_USER iCryptOwner,
				IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
				IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
				IN_LENGTH_KEYID_Z const int keyIDlength,
				IN_FLAGS_Z( KEYMGMT ) const int options,
				IN_ENUM_OPT( CRYPT_CERTTYPE ) \
					const CRYPT_CERTTYPE_TYPE formatHint );
CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int exportCert( OUT_BUFFER_OPT( certObjectMaxLength, *certObjectLength ) \
					void *certObject, 
				IN_DATALENGTH_Z const int certObjectMaxLength, 
				OUT_DATALENGTH_Z int *certObjectLength,
				IN_ENUM( CRYPT_CERTFORMAT ) \
					const CRYPT_CERTFORMAT_TYPE certFormatType,
				const CERT_INFO *certInfoPtr );

/* Oddball routines: work with a certificate's serial number */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setSerialNumber( INOUT CERT_INFO *certInfoPtr, 
					 IN_BUFFER_OPT( serialNumberLength ) const void *serialNumber, 
					 IN_LENGTH_SHORT_Z const int serialNumberLength );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 3 ) ) \
BOOLEAN compareSerialNumber( IN_BUFFER( canonSerialNumberLength ) \
								const void *canonSerialNumber,
							 IN_LENGTH_SHORT const int canonSerialNumberLength,
							 IN_BUFFER( serialNumberLength ) \
								const void *serialNumber,
							 IN_LENGTH_SHORT const int serialNumberLength );

/****************************************************************************
*																			*
*							Miscellaneous Functions							*
*																			*
****************************************************************************/

/* Convert a text-form OID to its binary form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int textToOID( IN_BUFFER( textOidLength ) const char *textOID, 
			   IN_LENGTH_TEXT const int textOidLength, 
			   OUT_BUFFER( binaryOidMaxLen, *binaryOidLen ) BYTE *binaryOID, 
			   IN_LENGTH_SHORT const int binaryOidMaxLen, 
			   OUT_LENGTH_BOUNDED_Z( binaryOidMaxLen ) \
					int *binaryOidLen );

/* Check that a text string contains valid characters for its string type.
   This is used in non-DN strings where we can't avoid the problem by varying
   the string type based on the characters being used */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkTextStringData( IN_BUFFER( stringLen ) const char *string, 
							 IN_LENGTH_SHORT const int stringLen,
							 const BOOLEAN isPrintableString );

/* Prototypes for functions in certext.c */

CHECK_RETVAL_BOOL \
BOOLEAN isValidField( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE fieldID,
					  IN_ENUM( CRYPT_CERTTYPE ) \
						const CRYPT_CERTTYPE_TYPE certType );

/* Prototypes for functions in certschk.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 8, 9 ) ) \
int checkCertDetails( INOUT CERT_INFO *subjectCertInfoPtr,
					  INOUT_OPT CERT_INFO *issuerCertInfoPtr,
					  IN_HANDLE_OPT const CRYPT_CONTEXT iIssuerPubKey,
					  IN_OPT const X509SIG_FORMATINFO *formatInfo,
					  const BOOLEAN trustAnchorCheck,
					  const BOOLEAN shortCircuitCheck,
					  const BOOLEAN basicCheckDone,
					  OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
					  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType );

/* Prototypes for functions in comp_cert.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyPublicKeyInfo( INOUT CERT_INFO *certInfoPtr,
					   IN_HANDLE_OPT const CRYPT_HANDLE cryptHandle,
					   IN_OPT const CERT_INFO *srcCertInfoPtr );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyCertObject( INOUT CERT_INFO *certInfoPtr,
					IN_HANDLE const CRYPT_CERTIFICATE addedCert,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					IN const int certInfo );

/* Prototypes for functions in comp_get.c */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
time_t *getRevocationTimePtr( const CERT_INFO *certInfoPtr );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
ATTRIBUTE_PTR *findAttributeComponent( const CERT_INFO *certInfoPtr,
										IN_ATTRIBUTE \
											const CRYPT_ATTRIBUTE_TYPE certInfoType );

/* Prototypes for functions in comp_pkiu.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int copyPkiUserToCertReq( INOUT CERT_INFO *certInfoPtr,
						  INOUT CERT_INFO *pkiUserInfoPtr );

/* Prototypes for functions in dn.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int convertEmail( INOUT CERT_INFO *certInfoPtr, 
				  INOUT DN_PTR **dnComponentListPtrPtr,
				  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE altNameType );

/* Prototypes for functions in ext.c */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN compareAttribute( const ATTRIBUTE_PTR *attributeField1,
						  const ATTRIBUTE_PTR *attributeField2 );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int fixAttributes( INOUT CERT_INFO *certInfoPtr );

/* Prototypes for functions in ext_def.c */

CHECK_RETVAL_BOOL \
BOOLEAN checkExtensionTables( void );

/* Prototypes for functions in imp_chk.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int getCertObjectInfo( INOUT STREAM *stream,
					   OUT_LENGTH_SHORT_Z int *objectOffset, 
					   OUT_DATALENGTH_Z int *objectLength, 
					   OUT_ENUM_OPT( CRYPT_CERTTYPE ) \
							CRYPT_CERTTYPE_TYPE *objectType,
					   IN_ENUM( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE formatHint );

/* Prototypes for functions in write_pre.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int preEncodeCertificate( INOUT CERT_INFO *subjectCertInfoPtr,
						  IN_OPT const CERT_INFO *issuerCertInfoPtr,
						  IN_FLAGS( PRE_SET ) const int actions );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int preCheckCertificate( INOUT CERT_INFO *subjectCertInfoPtr,
						 IN_OPT const CERT_INFO *issuerCertInfoPtr,
						 IN_FLAGS( PRE_CHECK ) const int actions, 
						 IN_FLAGS_Z( PRE ) const int flags );

#endif /* _CERTFN_DEFINED */
