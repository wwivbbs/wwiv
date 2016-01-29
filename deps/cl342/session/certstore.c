/****************************************************************************
*																			*
*					cryptlib HTTP Certstore Session Management				*
*						Copyright Peter Gutmann 1998-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "misc_rw.h"
  #include "session.h"
  #include "certstore.h"
#else
  #include "crypt.h"
  #include "enc_dec/misc_rw.h"
  #include "session/session.h"
  #include "session/certstore.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*					Shared Certstore/SCEP Utility Functions					*
*																			*
****************************************************************************/

/* SCEP's bolted-on HTTP query mechanism requires that we use the HTTP 
   certstore access routines to return responses to certificate queries,
   so we enable the use of this code if either certstores or SCEP are
   used */

#if defined( USE_CERTSTORE ) || defined( USE_SCEP )

#ifdef USE_SCEP

/* Table mapping a query submitted as an HTTP GET to a cryptlib-internal 
   keyset query.  Note that the first letter must be lowercase for the
   case-insensitive quick match */

static const CERTSTORE_READ_INFO certstoreReadInfo[] = {
	{ "certHash", 8, CRYPT_IKEYID_CERTID, CERTSTORE_FLAG_BASE64 },
	{ "name", 4, CRYPT_KEYID_NAME, CERTSTORE_FLAG_NONE },
	{ "uri", 3, CRYPT_KEYID_URI, CERTSTORE_FLAG_NONE },
	{ "email", 5, CRYPT_KEYID_URI, CERTSTORE_FLAG_NONE },
	{ "sHash", 5, CRYPT_IKEYID_ISSUERID, CERTSTORE_FLAG_BASE64 },
	{ "iHash", 5, CRYPT_IKEYID_ISSUERID, CERTSTORE_FLAG_BASE64 },
	{ "iAndSHash", 9, CRYPT_IKEYID_ISSUERANDSERIALNUMBER, CERTSTORE_FLAG_BASE64 },
	{ "sKIDHash", 8, CRYPT_IKEYID_KEYID, CERTSTORE_FLAG_BASE64 },
	{ NULL, 0, CRYPT_KEYID_NONE, CERTSTORE_FLAG_NONE },
		{ NULL, 0, CRYPT_KEYID_NONE, CERTSTORE_FLAG_NONE }
	};
#endif /* USE_SCEP */

/* Convert a query attribute into a text string suitable for use with 
   retExt() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int queryAttributeToString( OUT_BUFFER_FIXED( textBufMaxLen ) char *textBuffer, 
								   IN_LENGTH_SHORT_MIN( 16 ) const int textBufMaxLen,
								   IN_BUFFER( attributeLen ) const BYTE *attribute, 
								   IN_LENGTH_SHORT const int attributeLen )
	{
	assert( isWritePtr( textBuffer, textBufMaxLen ) );
	assert( isReadPtr( attribute, attributeLen ) );

	REQUIRES( textBufMaxLen >= 16 && textBufMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( attributeLen > 0 && attributeLen < MAX_INTLENGTH_SHORT );

	/* Copy as much of the attribute as will fit across and clean it up so
	   that it can be returned to the user */
	memcpy( textBuffer, attribute, min( attributeLen, textBufMaxLen ) );
	sanitiseString( textBuffer, textBufMaxLen, attributeLen );

	return( CRYPT_OK );
	}

/* Process a certificate query and return the requested certificate.  See 
   the comment above for why this is declared non-static */

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
					  OUT_OPT_LENGTH_SHORT_Z int *attributeLen )
	{
	const CERTSTORE_READ_INFO *queryInfoPtr = NULL;
	const int firstChar = toLower( httpReqInfo->attribute[ 0 ] );
	int i, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isReadPtr( httpReqInfo, sizeof( HTTP_URI_INFO ) ) );
	assert( isReadPtr( queryReqInfo, 
					   sizeof( CERTSTORE_READ_INFO ) * queryReqInfoSize ) );
	assert( isWritePtr( attributeID, sizeof( int ) ) );
	assert( ( attribute == NULL && attributeMaxLen == 0 && \
			  attributeLen == NULL ) || \
			( isWritePtr( attribute, attributeMaxLen ) && \
			  isWritePtr( attributeLen, sizeof( int ) ) ) );

	REQUIRES( queryReqInfoSize > 0 && queryReqInfoSize <= 64 );
	REQUIRES( ( attribute == NULL && attributeMaxLen == 0 && \
				attributeLen == NULL ) || \
			  ( attribute != NULL && \
				attributeMaxLen > 0 && \
				attributeMaxLen < MAX_INTLENGTH_SHORT && \
				attributeLen != NULL ) );

	/* Clear return values */
	*attributeID = CRYPT_ATTRIBUTE_NONE;
	if( attribute != NULL )
		{
		memset( attribute, 0, min( 16, attributeMaxLen ) );
		*attributeLen = 0;
		}

	/* Convert the search attribute type into a cryptlib key ID */
	for( i = 0; i < queryReqInfoSize && \
				queryReqInfo[ i ].attrName != NULL; i++ )
		{
		if( httpReqInfo->attributeLen == queryReqInfo[ i ].attrNameLen && \
			queryReqInfo[ i ].attrName[ 0 ] == firstChar && \
			!strCompare( httpReqInfo->attribute, \
						 queryReqInfo[ i ].attrName, \
						 queryReqInfo[ i ].attrNameLen ) )
			{
			queryInfoPtr = &queryReqInfo[ i ];
			break;
			}
		}
	ENSURES( i < queryReqInfoSize );
	if( queryInfoPtr == NULL )
		{
		char queryText[ CRYPT_MAX_TEXTSIZE + 8 ];

		status = queryAttributeToString( queryText, CRYPT_MAX_TEXTSIZE,
										 httpReqInfo->attribute, 
										 httpReqInfo->attributeLen );
		ENSURES( cryptStatusOK( status ) );
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid certificate query attribute '%s'", queryText ) );
		}

	/* We've got a valid attribute, let the caller know which one it is.  If
	   that's all the information that they're after, we're done */
	*attributeID = queryInfoPtr->attrID;
	if( attribute == NULL )
		return( CRYPT_OK );

#ifdef USE_BASE64
	/* If the query data wasn't encoded in any way, we're done */
	if( !( queryInfoPtr->flags & CERTSTORE_FLAG_BASE64 ) )
		{
		return( attributeCopyParams( attribute, attributeMaxLen, 
									 attributeLen, httpReqInfo->value, 
									 httpReqInfo->valueLen ) );
		}

	/* The value was base64-encoded in transit, decode it to get the actual 
	   query data */
	status = base64decode( attribute, attributeMaxLen, attributeLen, 
						   httpReqInfo->value, httpReqInfo->valueLen, 
						   CRYPT_CERTFORMAT_NONE );
	if( cryptStatusError( status ) )
		{
		char queryText[ CRYPT_MAX_TEXTSIZE + 8 ];

		status = queryAttributeToString( queryText, CRYPT_MAX_TEXTSIZE,
										 httpReqInfo->value, 
										 httpReqInfo->valueLen );
		ENSURES( cryptStatusOK( status ) );
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid base64-encoded query value '%s'", queryText ) );
		}

	return( CRYPT_OK );
#else
	return( attributeCopyParams( attribute, attributeMaxLen, 
								 attributeLen, httpReqInfo->value, 
								 httpReqInfo->valueLen ) );
#endif /* USE_BASE64 */
	}

/* Send an HTTP error response to the client (the error status value is 
   mapped at the HTTP layer to an appropriate HTTP response).  We don't 
   return a status from this since the caller already has an error status 
   available */

STDC_NONNULL_ARG( ( 1 ) ) \
void sendCertErrorResponse( INOUT SESSION_INFO *sessionInfoPtr, 
							IN_ERROR const int errorStatus )
	{
	HTTP_DATA_INFO httpDataInfo;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES_V( cryptStatusError( errorStatus ) );

	initHttpDataInfo( &httpDataInfo, sessionInfoPtr->receiveBuffer,
					  sessionInfoPtr->receiveBufSize );
	httpDataInfo.reqStatus = errorStatus;
	( void ) swrite( &sessionInfoPtr->stream, &httpDataInfo, 
					 sizeof( HTTP_DATA_INFO ) );
	}
#endif /* USE_CERTSTORE || USE_SCEP */

/****************************************************************************
*																			*
*								Init/Shutdown Functions						*
*																			*
****************************************************************************/

#ifdef USE_CERTSTORE

/* Exchange data with an HTTP client */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int serverTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	HTTP_DATA_INFO httpDataInfo;
	HTTP_URI_INFO httpReqInfo;
	MESSAGE_KEYMGMT_INFO getkeyInfo;
	MESSAGE_DATA msgData;
	BYTE keyID[ CRYPT_MAX_TEXTSIZE + 8 ];
	int keyIDtype, keyIDLen, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_GET );

	/* Read the request data from the client.  We do a direct read rather 
	   than using readPkiDatagram() since we're reading an idempotent HTTP 
	   GET request and not a PKI datagram submitted via an HTTP POST */
	memset( &httpReqInfo, 0, sizeof( HTTP_URI_INFO ) );
	initHttpDataInfoEx( &httpDataInfo, sessionInfoPtr->receiveBuffer,
						sessionInfoPtr->receiveBufSize, &httpReqInfo );
	status = sread( &sessionInfoPtr->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream, 
						  &sessionInfoPtr->errorInfo );
		return( status );
		}

	/* Convert the certificate query into a certstore search key */
	status = processCertQuery( sessionInfoPtr, &httpReqInfo, 
							   certstoreReadInfo,
							   FAILSAFE_ARRAYSIZE( certstoreReadInfo, \
												   CERTSTORE_READ_INFO ),
							   &keyIDtype, keyID, CRYPT_MAX_TEXTSIZE, 
							   &keyIDLen );
	if( cryptStatusError( status ) )
		{
		sendCertErrorResponse( sessionInfoPtr, status );
		return( status );
		}

	/* Try and fetch the requested certificate.  Note that this is somewhat 
	   suboptimal since we have to instantiate the certificate only to 
	   destroy it again as soon as we've exported the certificate data, for 
	   a proper high-performance implementation the server would query the 
	   certificate database directly and send the stored encoded value to 
	   the client */
	setMessageKeymgmtInfo( &getkeyInfo, keyIDtype, keyID, keyIDLen, 
						   NULL, 0, KEYMGMT_FLAG_NONE );
	status = krnlSendMessage( sessionInfoPtr->cryptKeyset,
							  IMESSAGE_KEY_GETKEY, &getkeyInfo, 
							  KEYMGMT_ITEM_PUBLICKEY );
	if( cryptStatusError( status ) )
		{
		char queryText[ CRYPT_MAX_TEXTSIZE + 8 ];
		char textBuffer[ 64 + CRYPT_MAX_TEXTSIZE + 8 ];
		int textLength;

		/* Not finding a certificate in response to a request isn't a real 
		   error so all we do is return a warning to the caller.  
		   Unfortunately since we're not using retExt() we have to assemble
		   the message string ourselves */
		sendCertErrorResponse( sessionInfoPtr, status );
		status = queryAttributeToString( queryText, CRYPT_MAX_TEXTSIZE,
										 httpReqInfo.value, 
										 httpReqInfo.valueLen );
		ENSURES( cryptStatusOK( status ) );
		textLength = sprintf_s( textBuffer, 64 + CRYPT_MAX_TEXTSIZE,
								"Warning: Couldn't find certificate for '%s'", 
								queryText );
		setErrorString( SESSION_ERRINFO, textBuffer, textLength );

		return( CRYPT_OK );
		}

	/* Write the certificate to the session buffer */
	setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
					sessionInfoPtr->receiveBufSize );
	status = krnlSendMessage( getkeyInfo.cryptHandle, IMESSAGE_CRT_EXPORT, 
							  &msgData, CRYPT_CERTFORMAT_CERTIFICATE );
	krnlSendNotifier( getkeyInfo.cryptHandle, IMESSAGE_DESTROY );
	if( cryptStatusError( status ) )
		{
		char queryText[ CRYPT_MAX_TEXTSIZE + 8 ];
		int altStatus;

		sendCertErrorResponse( sessionInfoPtr, status );
		altStatus = queryAttributeToString( queryText, CRYPT_MAX_TEXTSIZE,
											httpReqInfo.value, 
											httpReqInfo.valueLen );
		ENSURES( cryptStatusOK( altStatus ) );
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Couldn't export requested certificate for '%s'", 
				  queryText ) );
		}
	sessionInfoPtr->receiveBufEnd = msgData.length;

	/* Send the result to the client */
	return( writePkiDatagram( sessionInfoPtr, CERTSTORE_CONTENT_TYPE, 
							  CERTSTORE_CONTENT_TYPE_LEN ) );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodCertstore( INOUT SESSION_INFO *sessionInfoPtr )
	{
	static const PROTOCOL_INFO protocolInfo = {
		/* General session information */
		TRUE,						/* Request-response protocol */
		SESSION_ISHTTPTRANSPORT,	/* Flags */
		80,							/* HTTP port */
		0,							/* Client flags */
		SESSION_NEEDS_KEYSET,		/* Server flags */
		1, 1, 1						/* Version 1 */
	
		/* Protocol-specific information */
		};

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Set the access method pointers.  The client-side implementation is 
	  just a standard HTTP fetch so there's no explicit certstore client
	  implementation */
	sessionInfoPtr->protocolInfo = &protocolInfo;
	if( isServer( sessionInfoPtr ) )
		sessionInfoPtr->transactFunction = serverTransact;
	else
		return( CRYPT_ERROR_NOTAVAIL );

	return( CRYPT_OK );
	}
#endif /* USE_CERTSTORE */
