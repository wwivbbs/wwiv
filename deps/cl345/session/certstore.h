/****************************************************************************
*																			*
*					cryptlib Certstore Session Management					*
*					  Copyright Peter Gutmann 1998-2008						*
*																			*
****************************************************************************/

#ifndef _CERTSTORE_DEFINED

#define _CERTSTORE_DEFINED

/* The certstore HTTP content type */

#define CERTSTORE_CONTENT_TYPE		"application/pkix-cert"
#define CERTSTORE_CONTENT_TYPE_LEN	21

/* Processing flags for certstore query data.  These are:

	FLAG_BASE64: The attribute is base64-encoded and must be decoded before
		being returned to the caller */

#define CERTSTORE_FLAG_NONE		0x00	/* No special processing */
#define CERTSTORE_FLAG_BASE64	0x01	/* Data must be base64 */

/* A mapping of a query submitted as an HTTP GET to a cryptlib-specific
   attribute ID that can be used for an operation like a keyset query.  Note 
   that the first letter must be lowercase for the case-insensitive quick 
   match */

typedef struct {
	BUFFER_FIXED( attrNameLen ) \
	const char *attrName;				/* Attribute name from HTTP GET */
	const int attrNameLen;				/* Attribute name length */
	const int attrID;					/* Attribute ID */
	const int flags;					/* Processing flags */
	} CERTSTORE_READ_INFO;

/* Prototypes for functions in certstore.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int processCertQuery( INOUT SESSION_INFO *sessionInfoPtr,	
					  const HTTP_URI_INFO *httpReqInfo,
					  IN_ARRAY( queryReqInfoSize ) \
						const CERTSTORE_READ_INFO *queryReqInfo,
					  IN_RANGE( 1, 64 ) const int queryReqInfoSize,
					  OUT_ATTRIBUTE_Z int *attributeID, 
					  OUT_BUFFER_OPT( attributeMaxLen, *attributeLen ) \
						void *attribute, 
					  IN_LENGTH_SHORT_Z const int attributeMaxLen, 
					  OUT_OPT_LENGTH_SHORT_Z int *attributeLen );
STDC_NONNULL_ARG( ( 1 ) ) \
void sendCertErrorResponse( INOUT SESSION_INFO *sessionInfoPtr, 
							IN_ERROR const int errorStatus );

#endif /* _CERTSTORE_DEFINED */
