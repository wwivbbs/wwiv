/****************************************************************************
*																			*
*					  cryptlib HTTP Keyset Mapping Routines					*
*						Copyright Peter Gutmann 1998-2004					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "keyset.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_HTTP

/* The default size of the HTTP read buffer.  This is adjusted dynamically if
   the data being read won't fit (e.g. large CRLs).  The default size is 
   fine for certificates */

#define HTTP_BUFFER_SIZE	MIN_BUFFER_SIZE

/****************************************************************************
*																			*
*						 		Utility Routines							*
*																			*
****************************************************************************/

/* Set up key information for a query */

CHECK_RETVAL_PTR \
static const char *getKeyName( IN_KEYID const CRYPT_KEYID_TYPE keyIDtype )
	{
	REQUIRES_N( isEnumRange( keyIDtype, CRYPT_KEYID ) );

	switch( keyIDtype )
		{
		case CRYPT_KEYID_NAME:
			return( "name" );

		case CRYPT_KEYID_URI:
			return( "uri" );

		case CRYPT_IKEYID_KEYID:
			return( "sKIDHash" );

		case CRYPT_IKEYID_ISSUERID:
			return( "iAndSHash" );

		case CRYPT_IKEYID_CERTID:
			return( "certHash" );
		}

	retIntError_Null();
	}

/****************************************************************************
*																			*
*						 		Keyset Access Routines						*
*																			*
****************************************************************************/

/* Retrieve a certificate/CRL from an HTTP server, either as a flat URL if 
   the key name is "[none]" or as a certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int getItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							STDC_UNUSED void *auxInfo, 
							STDC_UNUSED int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
	HTTP_INFO *httpInfo = keysetInfoPtr->keysetHTTP;
	HTTP_DATA_INFO httpDataInfo;
	HTTP_REQ_INFO httpReqInfo;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BOOLEAN hasExplicitKeyID = FALSE;
	void *keyData;
	long length;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtrDynamic( keyID, keyIDlength ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_HTTP );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || keyIDtype == CRYPT_KEYID_URI );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( auxInfo == NULL && *auxInfoLength == 0 );
	REQUIRES( isFlagRangeZ( flags, KEYMGMT ) );

	/* Set the keyID as the query portion of the URL if necessary */
	if( keyIDlength != 6 || strCompare( keyID, "[none]", 6 ) )
		{
		/* Make sure that the keyID is of an appropriate size */
		if( keyIDlength > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ARGERROR_STR1 );

		hasExplicitKeyID = TRUE;
		}

	/* If we haven't allocated a buffer for the data yet, do so now */
	if( !DATAPTR_ISSET( keysetInfoPtr->keyData ) )
		{
		/* Allocate the initial I/O buffer */
		keyData = safeBufferAlloc( HTTP_BUFFER_SIZE );
		if( keyData == NULL )
			return( CRYPT_ERROR_MEMORY );
		DATAPTR_SET( keysetInfoPtr->keyData, keyData );
		keysetInfoPtr->keyDataSize = HTTP_BUFFER_SIZE;
		}
	keyData = DATAPTR_GET( keysetInfoPtr->keyData );
	ENSURES( keyData != NULL );
	httpInfo->bufPos = 0;

	/* Set up the HTTP request information and send it to the server */
	if( hasExplicitKeyID )
		{
		const char *keyName = getKeyName( keyIDtype );

		ENSURES( keyName != NULL );
		memset( &httpReqInfo, 0, sizeof( HTTP_REQ_INFO ) );
		httpReqInfo.attribute = keyName;
		httpReqInfo.attributeLen = strlen( keyName );
		httpReqInfo.value = keyID;
		httpReqInfo.valueLen = keyIDlength;
		status = initHttpInfoReqEx( &httpDataInfo, &httpReqInfo );
		}
	else
		status = initHttpInfoReq( &httpDataInfo );
	if( cryptStatusError( status ) )
		return( status );
	status = swrite( &httpInfo->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &httpInfo->stream, &keysetInfoPtr->errorInfo );
		return( status );
		}

	/* Read back the response from the server.  Since we don't know the size 
	   of the data being read in advance we have to tell the stream I/O code 
	   to adjust the read buffer size if necessary */
	status = initHttpInfoRead( &httpDataInfo, keyData, 
							   keysetInfoPtr->keyDataSize );
	ENSURES( cryptStatusOK( status ) );
	httpDataInfo.bufferResize = TRUE;
	status = sread( &httpInfo->stream, &httpDataInfo,
					sizeof( HTTP_DATA_INFO ) );
	if( httpDataInfo.bufferResize )
		{
		/* The read buffer may have been adjusted even though an error code
		   was returned from a later operation so we process the resized 
		   flag before we check for an error status */
		keyData = httpDataInfo.buffer;
		DATAPTR_SET( keysetInfoPtr->keyData, keyData );
		keysetInfoPtr->keyDataSize = httpDataInfo.bufSize;
		}
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &httpInfo->stream, &keysetInfoPtr->errorInfo );

		/* If it's a not-found error this is non-fatal condition (it just
		   means that the requested certificate wasn't found but doesn't 
		   prevent us from submitting further requests) so we clear the 
		   stream status to allow further queries */
		if( status == CRYPT_ERROR_NOTFOUND )
			sClearError( &httpInfo->stream );
		return( status );
		}

	/* Find out how much data we got and perform a general check that
	   everything is OK.  We rely on this rather than the read byte count
	   since checking the ASN.1, which is the data that will actually be
	   processed, avoids any vagaries of server implementation oddities,
	   which may send extra null bytes or CRLFs or do who knows what else */
	status = getLongObjectLength( keyData, httpDataInfo.bytesAvail, 
								  &length );
	if( cryptStatusError( status ) )
		return( status );

	/* Create a certificate object from the returned data */
	setMessageCreateObjectIndirectInfo( &createInfo, keyData, length, 
										CRYPT_CERTTYPE_NONE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = iCryptVerifyID( createInfo.cryptHandle, keyIDtype, keyID, 
							 keyIDlength );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Certificate fetched for ID type %s doesn't actually "
				  "correspond to the given ID", 
				  getKeyIDName( keyIDtype ) ) );
		}
	*iCryptHandle = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Prepare to open a connection to an HTTP server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
						 IN_BUFFER( nameLength ) const char *name, 
						 IN_LENGTH_SHORT_Z const int nameLength,
						 IN_ENUM( CRYPT_KEYOPT ) const CRYPT_KEYOPT_TYPE options )
	{
	HTTP_INFO *httpInfo = keysetInfoPtr->keysetHTTP;
	NET_CONNECT_INFO connectInfo;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtrDynamic( name, nameLength ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_HTTP );
	REQUIRES( nameLength >= MIN_DNS_SIZE && nameLength < MAX_URL_SIZE );
	REQUIRES( options == CRYPT_KEYOPT_NONE );

	/* Set up the HTTP connection */
	initNetConnectInfo( &connectInfo, keysetInfoPtr->ownerHandle, CRYPT_ERROR, 
						CRYPT_ERROR, NET_OPTION_HOSTNAME );
	connectInfo.name = name;
	connectInfo.nameLength = nameLength;
	connectInfo.port = 80;
	status = sNetConnect( &httpInfo->stream, STREAM_PROTOCOL_HTTP, 
						  &connectInfo, &keysetInfoPtr->errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Since this isn't a general-purpose HTTP stream (of the kind used for 
	   the HTTP-as-a-substrate PKI protocols) but is only being used for 
	   HTTP 'GET' operations, we restrict the usage to just this operation */
	sioctlSet( &httpInfo->stream, STREAM_IOCTL_HTTPREQTYPES,  
			   STREAM_HTTPREQTYPE_GET );

	ENSURES( sanityCheckKeyset( keysetInfoPtr ) );

	return( CRYPT_OK );
	}

/* Close a previously-opened HTTP connection */

STDC_NONNULL_ARG( ( 1 ) ) \
static int shutdownFunction( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	HTTP_INFO *httpInfo = keysetInfoPtr->keysetHTTP;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( keysetInfoPtr->type == KEYSET_HTTP );

	sNetDisconnect( &httpInfo->stream );
	if( DATAPTR_ISSET( keysetInfoPtr->keyData ) )
		{
		void *keyData = DATAPTR_GET( keysetInfoPtr->keyData );

		ENSURES( keyData != NULL );

		zeroise( keyData, keysetInfoPtr->keyDataSize );
		safeBufferFree( keyData );
		DATAPTR_SET( keysetInfoPtr->keyData, NULL );
		keysetInfoPtr->keyDataSize = 0;
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodHTTP( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_HTTP );

	/* Set the access method pointers */
	FNPTR_SET( keysetInfoPtr->initFunction, initFunction );
	FNPTR_SET( keysetInfoPtr->shutdownFunction, shutdownFunction );
	FNPTR_SET( keysetInfoPtr->getItemFunction, getItemFunction );

	return( CRYPT_OK );
	}
#endif /* USE_HTTP */
