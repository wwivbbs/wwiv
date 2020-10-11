/****************************************************************************
*																			*
*						 cryptlib SCEP Client Management					*
*						Copyright Peter Gutmann 1999-2017					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "session.h"
  #include "scep.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "session/session.h"
  #include "session/scep.h"
#endif /* Compiler-specific includes */

/* Prototypes for functions in pnppki.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pnpPkiSession( INOUT SESSION_INFO *sessionInfoPtr );

#ifdef USE_SCEP

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Import a SCEP CA certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int importCACertificate( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
								IN_BUFFER( certLength ) const void *certificate,
								IN_LENGTH_SHORT const int certLength,
								IN_FLAGS( KEYMGMT ) const int options )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	BOOLEAN isCertChain = FALSE;
	int tag, status;

	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtrDynamic( certificate, certLength ) );

	REQUIRES( certLength > 4 && certLength < MAX_INTLENGTH_SHORT );
	REQUIRES( options == KEYMGMT_FLAG_USAGE_CRYPT || \
			  options == KEYMGMT_FLAG_USAGE_SIGN );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	/* Depending on what the server feels like it can return either a single
	   certificate or a complete certificate chain, with the type denoted
	   by the HTTP-transport content type.  Because we have no easy way of
	   getting at this, we sniff the payload data to see what it contains.
	   The two objects begin with:

		Cert:		SEQUENCE { SEQUENCE ...
		Cert.chain:	SEQNEUCE { OID ...

	   so we can use the second tag to determine what we've got */
	sMemConnect( &stream, certificate, certLength );
	status = readSequence( &stream, NULL );
	if( checkStatusPeekTag( &stream, status, tag ) && \
		tag == BER_OBJECT_IDENTIFIER )
		{
		/* We've been sent a certificate chain, we need to import it as 
		   such */
		isCertChain = TRUE;
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Import the certificate */
	if( isCertChain )
		{
		setMessageCreateObjectIndirectInfoEx( &createInfo, certificate, 
							certLength, CRYPT_CERTTYPE_CERTCHAIN, options );
		}
	else
		{
		setMessageCreateObjectIndirectInfo( &createInfo, certificate, 
							certLength, CRYPT_CERTTYPE_CERTIFICATE );
		}
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	*iCryptCert = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/* Check whether two certificates are identical.  Since these may be present 
   at the end of a certificate chain, we have to jump through extra hoops to
   compare them */

static BOOLEAN isSameCertificate( IN_HANDLE const CRYPT_CERTIFICATE iCryptCert1,
								  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert2 )
	{
	BOOLEAN isSameCert = FALSE;
	int status;

	REQUIRES( isHandleRangeValid( iCryptCert1 ) );
	REQUIRES( isHandleRangeValid( iCryptCert2 ) );

	/* Lock the certificate chains for exclusive use */
	status = krnlSendMessage( iCryptCert1, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iCryptCert2, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		{
		( void ) krnlSendMessage( iCryptCert1, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( status );
		}

	/* Select the leaf certificate in both chains */
	status = krnlSendMessage( iCryptCert1, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST,
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCryptCert2, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_CURSORFIRST,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
		}
	if( cryptStatusOK( status ) )
		{
		int compareStatus;

		compareStatus = krnlSendMessage( iCryptCert1, IMESSAGE_COMPARE, 
										 ( MESSAGE_CAST ) &iCryptCert2,
										 MESSAGE_COMPARE_CERTOBJ );
		if( cryptStatusOK( compareStatus ) )
			isSameCert = TRUE;
		}
	( void ) krnlSendMessage( iCryptCert1, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, 
							  CRYPT_IATTRIBUTE_LOCKED );
	( void ) krnlSendMessage( iCryptCert2, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, 
							  CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		{
		/* It's not clear what we should return in the case of an error 
		   (mostly because it's a shouldn't-occur condition), we have two 
		   valid certificates so we shouldn't abort processing because a
		   compare operation failed.  Because of this we report a non-
		   match, which in most cases will allow things to proceeed as
		   required, and when it is a match it'll be caught later */
		return( FALSE );
		}

	return( isSameCert );
	}

/* Write a PKI datagram, with additional information communicated as part of 
   the HTTP metadata */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writePkiDatagramEx( INOUT SESSION_INFO *sessionInfoPtr, 
							   IN_BUFFER_OPT( contentTypeLen ) \
									const char *contentType, 
							   IN_LENGTH_TEXT_Z const int contentTypeLen )
	{
	HTTP_DATA_INFO httpDataInfo;
	static const HTTP_REQ_INFO httpReqInfo = {
		"operation", 9,
		"PKIOperation", 12, 
		"", 0
		};
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( contentType == NULL || \
			isReadPtrDynamic( contentType, contentTypeLen ) );

	REQUIRES( ( contentType == NULL && contentTypeLen ) || \
			  ( contentType != NULL && \
				contentTypeLen > 0 && contentTypeLen <= CRYPT_MAX_TEXTSIZE ) );
	REQUIRES( sessionInfoPtr->receiveBufEnd > 4 && \
			  sessionInfoPtr->receiveBufEnd < MAX_BUFFER_SIZE );

	/* Write the datagram.  Request/response sessions use a single buffer 
	   for both reads and writes, which is why we're (apparently) writing
	   the contents of the read buffer */
	status = initHttpInfoWriteEx( &httpDataInfo,
				sessionInfoPtr->receiveBuffer, sessionInfoPtr->receiveBufEnd, 
				sessionInfoPtr->receiveBufSize, &httpReqInfo );
	ENSURES( cryptStatusOK( status ) );
	httpDataInfo.contentType = contentType;
	httpDataInfo.contentTypeLen = contentTypeLen;
	status = swrite( &sessionInfoPtr->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		}
	else
		status = CRYPT_OK;	/* swrite() returns a byte count */
	sessionInfoPtr->receiveBufEnd = 0;

	return( status );
	}

#ifdef USE_BASE64

/* Some broken servers (and we're specifically talking Microsoft's one here) 
   don't handle POST but require the use of a POST disguised as a GET, for 
   which we provide the following variant of writePkiDatagram() that sends
   the POST as an HTTP GET */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int writePkiDatagramAsGet( INOUT SESSION_INFO *sessionInfoPtr )
	{
	HTTP_DATA_INFO httpDataInfo;
	static const HTTP_REQ_INFO httpReqInfo = {
		"operation", 9,
		"PKIOperation", 12, 
		"message=", 8
		};
	const int dataSize = sessionInfoPtr->receiveBufEnd;
	int fullEncodedLength, encodedLength, i, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( dataSize > 4 && dataSize < MAX_BUFFER_SIZE );

	/* The way that we do the encoding is to move the raw data up in the 
	   buffer to make room for the encoded form and then encode it into the 
	   freed-up room:

				+---------------+
				v	  Encode	|
		+---------------+---------------+
		| base64'd data	|	Raw data	|
		+---------------+---------------+
		|<-fullEncLen ->|<- dataSize -->|

	   First we have to determine how long the base64-encoded form of the 
	   message will be and make sure that it fits into the buffer */
	status = base64encodeLen( dataSize, &fullEncodedLength, 
							  CRYPT_CERTTYPE_NONE );
	ENSURES( cryptStatusOK( status ) );
	if( fullEncodedLength + dataSize >= sessionInfoPtr->receiveBufSize )
		return( CRYPT_ERROR_OVERFLOW );

	/* Move the message up in the buffer to make room for its encoded form 
	   and encode it */
	REQUIRES( boundsCheck( fullEncodedLength, dataSize, 
						   sessionInfoPtr->receiveBufSize ) );
	memmove( sessionInfoPtr->receiveBuffer + fullEncodedLength,
			 sessionInfoPtr->receiveBuffer, dataSize );
	status = base64encode( sessionInfoPtr->receiveBuffer, fullEncodedLength, 
						   &encodedLength, 
						   sessionInfoPtr->receiveBuffer + fullEncodedLength,
						   dataSize, CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* The base64 encoding in the form that we're calling it produces raw 
	   output without the trailing '=' padding bytes so we have to manually
	   insert the required padding in based on the calculated vs.actual
	   encoded length.  This can't overflow because we're simply padding the 
	   data out to the fullEncodedLength size that was calculated earlier */
	if( encodedLength < fullEncodedLength )
		{
		const int delta = fullEncodedLength - encodedLength;

		REQUIRES( delta > 0 && delta < 3 );
		REQUIRES( boundsCheck( encodedLength, delta, 
							   sessionInfoPtr->receiveBufSize ) );
		memcpy( sessionInfoPtr->receiveBuffer + encodedLength,
				"========", delta );
		}

	/* Now that it's base64-encoded it can no longer be sent as is because 
	   some base64 values, specifically '/', '+' and '=', are used for other
	   purposes in URLs.  Because of this we have to make another pass over
	   the data escaping these characters into the '%xx' form */
	LOOP_MAX( i = 0, i < fullEncodedLength, i++ )
		{
		char escapeBuffer[ 8 + 8 ];
		const int ch = sessionInfoPtr->receiveBuffer[ i ];

		/* If this isn't a special character, there's nothing to do */
		if( ch != '/' && ch != '+' && ch != '=' )
			continue;

		/* Make room for the escaped form and encode the value */
		if( fullEncodedLength + 2 >= sessionInfoPtr->receiveBufSize )
			return( CRYPT_ERROR_OVERFLOW );
		REQUIRES( boundsCheck( i + 2, fullEncodedLength - i, 
							   sessionInfoPtr->receiveBufSize ) );
		memmove( sessionInfoPtr->receiveBuffer + i + 2, 
				 sessionInfoPtr->receiveBuffer + i, fullEncodedLength - i );
		sprintf_s( escapeBuffer, 8, "%%%02X", ch );
		REQUIRES( boundsCheck( i, 3, sessionInfoPtr->receiveBufSize ) );
		memcpy( sessionInfoPtr->receiveBuffer + i, escapeBuffer, 3 );
		fullEncodedLength += 2;
		} 
	ENSURES( LOOP_BOUND_OK );

	/* Send the POST as an HTTP GET */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_POST_AS_GET );
	status = initHttpInfoWriteEx( &httpDataInfo, 
						sessionInfoPtr->receiveBuffer, fullEncodedLength, 
						sessionInfoPtr->receiveBufSize, &httpReqInfo );
	ENSURES( cryptStatusOK( status ) );
	httpDataInfo.contentType = SCEP_CONTENT_TYPE;
	httpDataInfo.contentTypeLen = SCEP_CONTENT_TYPE_LEN;
	status = swrite( &sessionInfoPtr->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_POST );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		}
	else
		status = CRYPT_OK;	/* swrite() returns a byte count */
	sessionInfoPtr->receiveBufEnd = 0;

	return( status );
	}
#endif /* USE_BASE64 */

/****************************************************************************
*																			*
*					Additional Request Management Functions					*
*																			*
****************************************************************************/

/* Process the various bolted-on additions to the basic SCEP protocol */

typedef enum { GETREQUEST_NONE, GETREQUEST_GETCACAPS, GETREQUEST_GETCACERT, 
			   GETREQUEST_LAST } GETREQUEST_TYPE;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int sendGetRequest( INOUT SESSION_INFO *sessionInfoPtr,
						   const GETREQUEST_TYPE requestType )
	{
	HTTP_DATA_INFO httpDataInfo;
#if 0	/* 23/3/18 Older versions of the SCEP draft specified the GetCACaps
				   format as '... "?operation=GetCACaps&message=" CA-IDENT' 
				   but no-one knew what CA-IDENT was meant to be, see the 
				   comment below.  Current versions of the draft specify
				   the message as '"?operation=GetCACaps"' */
	static const HTTP_REQ_INFO httpReqGetCACaps = {
		"operation", 9,
		"GetCACaps", 9,
		"message=*", 9
		};
	static const HTTP_REQ_INFO httpReqGetCACert = {
		"operation", 9,
		"GetCACert", 9,
		"message=*", 9
		};
#else
	static const HTTP_REQ_INFO httpReqGetCACaps = {
		"operation", 9,
		"GetCACaps", 9,
		};
	static const HTTP_REQ_INFO httpReqGetCACert = {
		"operation", 9,
		"GetCACert", 9,
		};
#endif /* 0 */
	HTTP_REQ_INFO *httpReqInfo = \
		( requestType == GETREQUEST_GETCACAPS ) ? \
		( void * ) &httpReqGetCACaps : ( void * ) &httpReqGetCACert;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	
	REQUIRES( isEnumRange( requestType, GETREQUEST ) );

	/* Perform an HTTP GET with arguments "operation=<command>&message=*".
	   The "message=" portion is rather unclear, according to the spec
	   it's "a string that represents the certification authority issuer 
	   identifier" but no-one (including the spec's authors) seem to know
	   what this is supposed to be.  We use '*' on the basis that it's
	   better than nothing */
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_GET );
	status = initHttpInfoReqEx( &httpDataInfo, httpReqInfo );
	ENSURES( cryptStatusOK( status ) );
	if( requestType == GETREQUEST_GETCACAPS )
		{
		/* Indicate that a response consisting of a text message, rather 
		   than PKI data, is valid for this operation */
		httpDataInfo.responseIsText = TRUE;
		}
	status = swrite( &sessionInfoPtr->stream, &httpDataInfo,
					 sizeof( HTTP_DATA_INFO ) );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		return( status );
		}
	status = initHttpInfoRead( &httpDataInfo, sessionInfoPtr->receiveBuffer,
							   sessionInfoPtr->receiveBufSize );
	ENSURES( cryptStatusOK( status ) );
	status = sread( &sessionInfoPtr->stream, &httpDataInfo,
					sizeof( HTTP_DATA_INFO ) );
	sioctlSet( &sessionInfoPtr->stream, STREAM_IOCTL_HTTPREQTYPES, 
			   STREAM_HTTPREQTYPE_POST );
	if( cryptStatusError( status ) )
		{
		sNetGetErrorInfo( &sessionInfoPtr->stream,
						  &sessionInfoPtr->errorInfo );
		retExtErr( status, 
				   ( status, &sessionInfoPtr->errorInfo,
				     &sessionInfoPtr->errorInfo,
					 "'%s' operation failed: ", 
					 ( requestType == GETREQUEST_GETCACAPS ) ? \
						"GetCACaps" : "GetCACert" ) );
		}
	sessionInfoPtr->receiveBufEnd = httpDataInfo.bytesAvail;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int getCACapabilities( INOUT SESSION_INFO *sessionInfoPtr )
	{
	STREAM stream;
	int length, lineCount, status, LOOP_ITERATOR;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	/* Get the CA capabilities */
	status = sendGetRequest( sessionInfoPtr, GETREQUEST_GETCACAPS );
	if( cryptStatusError( status ) )
		{
		/* Microsoft's NDES under Server 2003 and Server 2008 without the
		   KB2483564 hotfix either simply close the connection with no 
		   further output or send back a zero-length response.  If we get 
		   this then we provide a hint about what the problem could be.  
		   
		   This can be a bit problematic because we could also be talking 
		   to something that isn't a SCEP server, which will lead to a 
		   misleading diagnostic, but it should be safe to assume that the 
		   user is intending to talk to a SCEP server and that if we get 
		   this response then it's likely to be Server 2003/pre-hotfix 
		   2008.
		   
		   In any case even with GetCACaps enabled via a hotfix the results
		   are more or less useless, AES isn't listed as a supported
		   algorithm but is supported anyway, and the response is hardcoded
		   to use single DES (!!!) no matter what algorithm is used for the
		   request (see http://serverfault.com/questions/458643/can-i-
		   configure-wndows-ndes-server-to-use-triple-des-3des-algorithm-
		   for-pkcs7) */
		if( status == CRYPT_ERROR_READ )
			{
			retExt( CRYPT_ERROR_OPEN,
					( CRYPT_ERROR_OPEN, SESSION_ERRINFO, 
					  "Server closed the connection in response to a SCEP "
					  "GetCACaps message, if this is Windows Server 2003 "
					  "or 2008 then you need to upgrade to at least Server "
					  "2008 R2 with the KB2483564 hotfix in order to talk "
					  "to the server" ) );
			}
		return( status );
		}

	/* Read the GetCACaps response lines */
	sMemConnect( &stream, sessionInfoPtr->receiveBuffer, 
				 sessionInfoPtr->receiveBufEnd );
	LOOP_MED( lineCount = 0, 
			  sMemDataLeft( &stream ) > 0 && lineCount < 64, 
			  lineCount++ )
		{
		char buffer[ 512 + 8 ];

		status = readTextLine( ( READCHAR_FUNCTION ) sgetc, &stream, 
							   buffer, 512, &length, NULL, 
							   FALSE );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			retExt( CRYPT_ERROR_BADDATA,
					( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
					  "Invalid GETCACaps response line %d", lineCount ) );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	if( lineCount >= 64 )
		{
		sMemDisconnect( &stream );
		retExt( CRYPT_ERROR_BADDATA,
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Excessive number (more than %d) of GETCACaps response "
				  "lines", 64 ) );
		}

	/* We currently don't do much more with this, what the (effectively) 
	   dummy read does is allow us to fingerprint NDES so that we can work 
	   around its bugs later on */
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getCACertificate( INOUT SESSION_INFO *sessionInfoPtr,
							 INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	REQUIRES( sessionInfoPtr->iAuthInContext == CRYPT_ERROR );

	/* Get the CA certificate */
	status = sendGetRequest( sessionInfoPtr, GETREQUEST_GETCACERT );
	if( cryptStatusError( status ) )
		return( status );

	/* Since we can't use readPkiDatagram() because of the weird dual-
	   purpose HTTP transport used in SCEP where the main protocol uses
	   POST + read response while the bolted-on portions use various GET
	   variations, we have to duplicate portions of readPkiDatagram() here.  
	   See the readPkiDatagram() function for code comments explaining the 
	   following operations */
	if( sessionInfoPtr->receiveBufEnd < 4 || \
		sessionInfoPtr->receiveBufEnd >= MAX_BUFFER_SIZE )
		{
		retExt( CRYPT_ERROR_UNDERFLOW,
				( CRYPT_ERROR_UNDERFLOW, SESSION_ERRINFO, 
				  "Invalid CA certificate size %d", 
				  sessionInfoPtr->receiveBufEnd ) );
		}
	status = checkCertObjectEncodingLength( sessionInfoPtr->receiveBuffer, 
											sessionInfoPtr->receiveBufEnd,
											&sessionInfoPtr->receiveBufEnd );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid CA certificate encoding" ) );
		}
	DEBUG_DUMP_FILE( "scep_cacrt", sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );

	/* Import the CA/RA certificates and save it/them for later use.  There
	   may be distinct signature and encryption certificates stuffed into 
	   the same chain so we first try for a signature certificate */
	status = importCACertificate( &sessionInfoPtr->iAuthInContext,
								  sessionInfoPtr->receiveBuffer, 
								  sessionInfoPtr->receiveBufEnd,
								  KEYMGMT_FLAG_USAGE_SIGN );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid SCEP CA certificate" ) );
		}
	SET_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_FETCHEDCACERT );

	/* Now that we've got a signing certificate, check whether this single
	   certificate has the unusual additional capabilities that are required
	   for SCEP */
	if( !checkSCEPCACert( sessionInfoPtr->iAuthInContext, 
						  KEYMGMT_FLAG_NONE ) )
		{
		/* It doesn't have the required capabilities, assume that it's a
		   signature-only certificate and try again for an encryption
		   certificate */
		status = importCACertificate( &sessionInfoPtr->iCryptOutContext,
									  sessionInfoPtr->receiveBuffer, 
									  sessionInfoPtr->receiveBufEnd,
									  KEYMGMT_FLAG_USAGE_CRYPT );
		if( cryptStatusError( status ) )
			{
			retExt( status, 
					( status, SESSION_ERRINFO, 
					  "Invalid SCEP CA certificate" ) );
			}
		
		/* Because of the vagaries of dealing with chains containing 
		   multiple certificates, with or without keyUsage extensions, the
		   certificate-import code returns a best-match certificate rather
		   than an absolute-match certificate.  In particular if there's 
		   only one certificate present and it doesn't have the encryption
		   keyUsage that we require, it'll be returned anyway since it's the
		   only certificate that can be returned.  In order to deal with 
		   this we have to check whether the certificates at the end of the
		   two chains are identical */
		if( isSameCertificate( sessionInfoPtr->iAuthInContext,
							   sessionInfoPtr->iCryptOutContext ) )
			{
			/* There's only one certificate and it's signature-only, use the
			   signature-only form of SCEP */
			krnlSendNotifier( sessionInfoPtr->iCryptOutContext,
							  IMESSAGE_DECREFCOUNT );
			sessionInfoPtr->iCryptOutContext = CRYPT_ERROR;
			protocolInfo->caSignOnlyKey = TRUE;
			}
		}

	/* Process the server's key fingerprint */
	status = processKeyFingerprint( sessionInfoPtr );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the CA certificate has the unusual additional 
	   capabilities that are required to meet the SCEP protocol 
	   requirements */
	if( sessionInfoPtr->iCryptOutContext != CRYPT_ERROR )
		{
		/* There are distinct encryption and signing certificates (probably
		   from an RA) present, make sure that they meet the necessary
		   requirements */
		if( !checkSCEPCACert( sessionInfoPtr->iAuthInContext, 
							  KEYMGMT_FLAG_USAGE_SIGN ) || \
			!checkSCEPCACert( sessionInfoPtr->iCryptOutContext, 
							  KEYMGMT_FLAG_USAGE_CRYPT ) )
			{
			retExt( CRYPT_ERROR_INVALID, 
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "CA/RA certificate usage restrictions prevent them "
					  "from being used for SCEP" ) );
			}
		}
	else
		{
		/* There's a single multipurpose certificate present, make sure that 
		   it meets all of the requirements for SCEP use */
		if( !checkSCEPCACert( sessionInfoPtr->iAuthInContext, 
							  protocolInfo->caSignOnlyKey ? \
								KEYMGMT_FLAG_USAGE_SIGN : \
								KEYMGMT_FLAG_NONE ) )
			{
			retExt( CRYPT_ERROR_INVALID, 
					( CRYPT_ERROR_INVALID, SESSION_ERRINFO, 
					  "CA certificate usage restrictions prevent it from being "
					  "used for SCEP" ) );
			}
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Request Management Functions						*
*																			*
****************************************************************************/

/* Create a self-signed certificate for signing the request and decrypting
   the response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createScepCert( INOUT SESSION_INFO *sessionInfoPtr,
						   INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	CRYPT_CERTIFICATE iNewCert;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	time_t currentTime = getTime();
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	/* Create a certificate, add the certificate request and other 
	   information required by SCEP to it, and sign it.  To limit the 
	   exposure from having it floating around out there we give it a 
	   validity of a day, which is somewhat longer than required but may be 
	   necessary to get around time-zone issues in which the CA checks the 
	   expiry time relative to the time zone that it's in rather than GMT 
	   (although given some of the broken certificates used with SCEP it 
	   seems likely that many CAs do little to no checking at all).
	   
	   SCEP requires that the certificate serial number match the user name/
	   transaction ID, the spec actually says that the transaction ID should 
	   be a hash of the public key but since it never specifies exactly what 
	   is hashed ("MD5 hash on [sic] public key") this can probably be 
	   anything.  We use the user name, which is required to identify the 
	   pkiUser entry in the CA certificate store */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_SETATTRIBUTE,
							  &sessionInfoPtr->iCertRequest,
							  CRYPT_CERTINFO_CERTREQUEST );
	if( cryptStatusOK( status ) )
		{
		/* Set the certificate usage to signing (to sign the request) and
		   encryption (to decrypt the response, if the key is capable of
		   this).  We've already checked that the sign capability was
		   available when the key was added to the session.
		   
		   We delete the attribute before we try and set it in case there 
		   was already one present in the request */
		krnlSendMessage( createInfo.cryptHandle, IMESSAGE_DELETEATTRIBUTE, 
						 NULL, CRYPT_CERTINFO_KEYUSAGE );
		if( protocolInfo->clientSignOnlyKey )
			{
			static const int keyUsage = CRYPT_KEYUSAGE_DIGITALSIGNATURE;

			status = krnlSendMessage( createInfo.cryptHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &keyUsage, 
									  CRYPT_CERTINFO_KEYUSAGE );
			}
		else
			{
			static const int keyUsage = CRYPT_KEYUSAGE_DIGITALSIGNATURE | \
										CRYPT_KEYUSAGE_KEYENCIPHERMENT;

			status = krnlSendMessage( createInfo.cryptHandle, 
									  IMESSAGE_SETATTRIBUTE, 
									  ( MESSAGE_CAST ) &keyUsage, 
									  CRYPT_CERTINFO_KEYUSAGE );
			}
		}
	if( cryptStatusOK( status ) )
		{
		setMessageData( &msgData, ( MESSAGE_CAST ) &currentTime, 
						sizeof( time_t ) );
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_VALIDFROM );
		}
	if( cryptStatusOK( status ) )
		{
		currentTime += 86400;	/* 24 hours */
		setMessageData( &msgData, ( MESSAGE_CAST ) &currentTime, 
						sizeof( time_t ) );
		status = krnlSendMessage( createInfo.cryptHandle, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_VALIDTO );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_TRUE,
								  CRYPT_CERTINFO_SELFSIGNED );
		}
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_CRT_SIGN, NULL,
								  sessionInfoPtr->privateKey );
		}
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create ephemeral self-signed SCEP "
				  "certificate" ) );
		}

	/* Now that we have a certificate, attach it to the private key.  This 
	   is somewhat ugly since it alters the private key by attaching a 
	   certificate that (as far as the user is concerned) shouldn't really 
	   exist, but we need to do this to allow signing and decryption.  A 
	   side-effect is that it constrains the private-key actions to make 
	   them internal-only since it now has a certificate attached, hopefully 
	   the user won't notice this since the key will have a proper CA-issued 
	   certificate attached to it shortly.

	   To further complicate things, we can't directly attach the newly-
	   created certificate because it already has a public-key context 
	   attached to it, which would result in two keys being associated with 
	   the single certificate.  To resolve this, we create a second copy of 
	   the certificate as a data-only certificate and attach that to the 
	   private key */
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_GETATTRIBUTE, 
							  &iNewCert, CRYPT_IATTRIBUTE_CERTCOPY_DATAONLY );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( sessionInfoPtr->privateKey, 
								  IMESSAGE_SETDEPENDENT, &iNewCert, 
								  SETDEP_OPTION_NOINCREF );
		}
	if( cryptStatusOK( status ) )
		protocolInfo->iScepCert = createInfo.cryptHandle;
	else
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/* Complete the user-supplied PKCS #10 request by adding SCEP-internal
   attributes and information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int completeScepCertRequest( INOUT SESSION_INFO *sessionInfoPtr,
									INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	const ATTRIBUTE_LIST *attributeListPtr = \
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_PASSWORD );
	MESSAGE_DATA msgData;
	int status = CRYPT_ERROR_NOTINITED;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );

	/* Add the password to the PKCS #10 request as a ChallengePassword
	   attribute.  We always send this in its ASCII string form even if it's 
	   an encoded value because the ChallengePassword attribute has to be a 
	   text string */
	if( attributeListPtr != NULL )
		{
		/* Copy the user password for later in case we need it to decrypt a
		   response for a signature-only key */
		REQUIRES( rangeCheck( attributeListPtr->valueLength, 1, 
							  CRYPT_MAX_TEXTSIZE ) );
		memcpy( protocolInfo->userPassword, attributeListPtr->value,
				attributeListPtr->valueLength );
		protocolInfo->userPasswordSize = attributeListPtr->valueLength;

		INJECT_FAULT( CORRUPT_AUTHENTICATOR, 
					  SESSION_CORRUPT_AUTHENTICATOR_SCEP_1 );
		setMessageData( &msgData, attributeListPtr->value,
						attributeListPtr->valueLength );
		status = krnlSendMessage( sessionInfoPtr->iCertRequest, 
								  IMESSAGE_SETATTRIBUTE_S, &msgData, 
								  CRYPT_CERTINFO_CHALLENGEPASSWORD );
		INJECT_FAULT( CORRUPT_AUTHENTICATOR, 
					  SESSION_CORRUPT_AUTHENTICATOR_SCEP_2 );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Couldn't finalise PKCS #10 certificate request" ) );
			}
		}

	/* Sign the request */
	status = krnlSendMessage( sessionInfoPtr->iCertRequest,
							  IMESSAGE_CRT_SIGN, NULL,
							  sessionInfoPtr->privateKey );
	if( cryptStatusError( status ) )
		{
		retExtObj( status,
				   ( status, SESSION_ERRINFO, sessionInfoPtr->iCertRequest,
					 "Couldn't sign PKCS #10 certificate request" ) );
		}

	return( CRYPT_OK );
	}

/* Create the request type needed to continue after the server responds with
   an issue-pending response:

	issuerAndSubject ::= SEQUENCE {
		issuer		Name,
		subject		Name
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createScepPendingRequest( INOUT SESSION_INFO *sessionInfoPtr,
									 OUT_LENGTH_SHORT_Z int *dataLength )
	{
	STREAM stream;
	int issuerAndSubjectLen DUMMY_INIT, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );

	/* Clear return value */
	*dataLength = 0;

	/* Determine the overall length of the issuer and subject DNs */
	sMemNullOpen( &stream );
	status = exportAttributeToStream( &stream, 
									  sessionInfoPtr->iAuthInContext, 
									  CRYPT_IATTRIBUTE_SUBJECT );
	if( cryptStatusOK( status ) )
		{
		status = exportAttributeToStream( &stream, 
										  sessionInfoPtr->iCertRequest, 
										  CRYPT_IATTRIBUTE_SUBJECT );
		}
	if( cryptStatusOK( status ) )
		issuerAndSubjectLen = stell( &stream );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the issuerAndSubject to the session buffer */
	sMemOpen( &stream, sessionInfoPtr->receiveBuffer, 
			  sessionInfoPtr->receiveBufSize );
	writeSequence( &stream, issuerAndSubjectLen );
	status = exportAttributeToStream( &stream, 
									  sessionInfoPtr->iAuthInContext, 
									  CRYPT_IATTRIBUTE_SUBJECT );
	if( cryptStatusOK( status ) )
		{
		status = exportAttributeToStream( &stream, 
										  sessionInfoPtr->iCertRequest, 
										  CRYPT_IATTRIBUTE_SUBJECT );
		}
	if( cryptStatusOK( status ) )
		*dataLength = stell( &stream );
	sMemDisconnect( &stream );

	return( status );
	}

/* Create a SCEP request message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int createScepRequest( INOUT SESSION_INFO *sessionInfoPtr,
							  INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	const CRYPT_CERTIFICATE iCACryptContext = \
		( sessionInfoPtr->iCryptOutContext != CRYPT_ERROR ) ? \
		sessionInfoPtr->iCryptOutContext : sessionInfoPtr->iAuthInContext;
	CRYPT_CERTIFICATE iCmsAttributes;
	ERROR_INFO errorInfo;
	const BOOLEAN isPendingRequest = TEST_FLAG( sessionInfoPtr->protocolFlags, 
												SCEP_PFLAG_PENDING ) ? \
									 TRUE : FALSE;
	int dataLength, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );

	/* If it's a straight issue operation, extract the request data into the 
	   session buffer */
	if( !isPendingRequest )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, sessionInfoPtr->receiveBuffer,
						sessionInfoPtr->receiveBufSize );
		status = krnlSendMessage( sessionInfoPtr->iCertRequest,
								  IMESSAGE_CRT_EXPORT, &msgData,
								  CRYPT_CERTFORMAT_CERTIFICATE );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Couldn't get PKCS #10 request data from SCEP request "
					  "object" ) );
			}
		dataLength = msgData.length;
		}
	else
		{
		/* It's a continuation of a previous issue operation whose status 
		   the server has reported as pending, encode the special-case form
		   that's required for this operation */
		status = createScepPendingRequest( sessionInfoPtr, &dataLength );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, SESSION_ERRINFO, 
					  "Couldn't create SCEP request needed to continue from "
					  "an issue-pending response" ) );
			}
		}
	DEBUG_DUMP_FILE( isPendingRequest ? "scep_req0pend" : "scep_req0", 
					 sessionInfoPtr->receiveBuffer, dataLength );

	/* Phase 1: Encrypt the data using either the CA's key or the client's
	   password */
	if( protocolInfo->caSignOnlyKey )
		{
		status = envelopeWrap( sessionInfoPtr->receiveBuffer, dataLength,
							   sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, &dataLength, 
							   CRYPT_FORMAT_CMS, CRYPT_CONTENT_NONE, 
							   CRYPT_UNUSED, protocolInfo->userPassword, 
							   protocolInfo->userPasswordSize, 
							   &errorInfo );
		}
	else
		{
		status = envelopeWrap( sessionInfoPtr->receiveBuffer, dataLength,
							   sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, &dataLength, 
							   CRYPT_FORMAT_CMS, CRYPT_CONTENT_NONE, 
							   iCACryptContext, NULL, 0, &errorInfo );
		}
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't encrypt SCEP request data with CA key: " ) );
		}
	DEBUG_DUMP_FILE( isPendingRequest ? "scep_req1pend" : "scep_req1", 
					 sessionInfoPtr->receiveBuffer, dataLength );

	/* Create the SCEP signing attributes */
	status = createScepAttributes( sessionInfoPtr, protocolInfo,  
					&iCmsAttributes, isPendingRequest ? \
						MESSAGETYPE_GETCERTINITIAL : \
					( scepInfo->requestType == CRYPT_REQUESTTYPE_INITIALISATION ) ? \
						MESSAGETYPE_PKCSREQ : MESSAGETYPE_RENEWAL, 
					CRYPT_OK );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, SESSION_ERRINFO, 
				  "Couldn't create SCEP request signing attributes" ) );
		}

	/* Phase 2: Sign the data using the self-signed certificate and SCEP 
	   attributes */
	status = envelopeSign( sessionInfoPtr->receiveBuffer, dataLength,
						   sessionInfoPtr->receiveBuffer, 
						   sessionInfoPtr->receiveBufSize, 
						   &sessionInfoPtr->receiveBufEnd, 
						   CRYPT_CONTENT_NONE, sessionInfoPtr->privateKey, 
						   iCmsAttributes, &errorInfo );
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		{
		retExtErr( status,
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't sign request data with ephemeral SCEP "
					 "certificate: " ) );
		}
	DEBUG_DUMP_FILE( isPendingRequest ? "scep_req2pend" : "scep_req2", 
					 sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Response Management Functions						*
*																			*
****************************************************************************/

/* Check the status of a SCEP response */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkScepStatus( INOUT SESSION_INFO *sessionInfoPtr, 
							IN_HANDLE const CRYPT_CERTIFICATE iCmsAttributes )
	{
	typedef struct {
		const int failInfoValue;
		const int failStatus;
		const char *failInfoString;
		} FAILINFO_MESSAGE;
	static const FAILINFO_MESSAGE failInfoMsgTbl[] = {
		{ MESSAGEFAILINFO_BADALG_VALUE, CRYPT_ERROR_NOTAVAIL,
		  "Unrecognized or unsupported algorithm identifier" },
		{ MESSAGEFAILINFO_BADMESSAGECHECK_VALUE, CRYPT_ERROR_SIGNATURE,
		  "Integrity check failed" },
		{ MESSAGEFAILINFO_BADREQUEST_VALUE, CRYPT_ERROR_PERMISSION,
		  "Transaction not permitted or supported" },
		{ MESSAGEFAILINFO_BADTIME_VALUE, CRYPT_ERROR_INVALID,
		  "CMS signingTime attribute was not sufficiently close to the "
		  "system time" },
		{ MESSAGEFAILINFO_BADCERTID_VALUE, CRYPT_ERROR_NOTFOUND,
		  "No certificate could be identified matching the provided "
		  "criteria" },
		{ CRYPT_ERROR, CRYPT_ERROR_FAILED, "<Unknown failure reason>" },
			{ CRYPT_ERROR, CRYPT_ERROR_FAILED, "<Unknown failure reason>" }
		};
#ifdef USE_ERRMSGS
	const char *valueString = "<Unknown failure reason>";
#endif /* USE_ERRMSGS */
	int value, extValue, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCmsAttributes ) );

	/* Make sure that we've got the correct response type */
	status = getScepStatusValue( iCmsAttributes,
								 CRYPT_CERTINFO_SCEP_MESSAGETYPE, &value );
	if( cryptStatusError( status ) || value != MESSAGETYPE_CERTREP_VALUE )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, SESSION_ERRINFO, 
				  "Invalid SCEP response type, expected %d", 
				  MESSAGETYPE_CERTREP_VALUE ) );
		}

	/* Check the status of the operation */
	status = getScepStatusValue( iCmsAttributes, 
								 CRYPT_CERTINFO_SCEP_PKISTATUS, &value );
	if( cryptStatusError( status ) )
		value = MESSAGESTATUS_FAILURE_VALUE;
	if( value == MESSAGESTATUS_SUCCESS_VALUE )
		return( CRYPT_OK );

	/* There was a problem with the operation, get more detailed information
	   on what went wrong.  If we get a MESSAGESTATUS_PENDING result then we 
	   can't go any further until the CA makes up its mind about issuing us 
	   a certificate */
	if( value == MESSAGESTATUS_PENDING_VALUE )
		{
		SET_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_PENDING );
		retExt( CRYPT_ENVELOPE_RESOURCE, 
				( CRYPT_ENVELOPE_RESOURCE, SESSION_ERRINFO, 
				  "SCEP server reports that certificate status is "
				  "pending, try again later" ) );
		}

	/* It's some other sort of error, report the details to the user */
	status = getScepStatusValue( iCmsAttributes, 
								 CRYPT_CERTINFO_SCEP_FAILINFO, &extValue );
	if( cryptStatusOK( status ) )
		{
		int i, LOOP_ITERATOR;

		LOOP_SMALL( i = 0,
					failInfoMsgTbl[ i ].failInfoValue != extValue && \
						failInfoMsgTbl[ i ].failInfoValue != CRYPT_ERROR && \
						i < FAILSAFE_ARRAYSIZE( failInfoMsgTbl, \
												FAILINFO_MESSAGE ),
					i++ );
		ENSURES( LOOP_BOUND_OK );
		ENSURES( i < FAILSAFE_ARRAYSIZE( failInfoMsgTbl, FAILINFO_MESSAGE ) );
#ifdef USE_ERRMSGS
		valueString = failInfoMsgTbl[ i ].failInfoString;
#endif /* USE_ERRMSGS */
		value = extValue;
		status = failInfoMsgTbl[ i ].failStatus;
		}
	else
		{
		/* We can't report anything more than a generic "failed" */
		status = CRYPT_ERROR_FAILED;
		}
	retExt( status, 
			( status, SESSION_ERRINFO, 
			  "SCEP server reports that certificate issue operation "
			  "failed with error code %d (%s)", value, valueString ) );
	}

/* Check a SCEP response message */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkScepResponse( INOUT SESSION_INFO *sessionInfoPtr, 
							  INOUT SCEP_PROTOCOL_INFO *protocolInfo )
	{
	const ATTRIBUTE_LIST *userNamePtr = \
				findSessionInfo( sessionInfoPtr, CRYPT_SESSINFO_USERNAME );
	CRYPT_CERTIFICATE iCmsAttributes;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	ERROR_INFO errorInfo;
	BYTE buffer[ CRYPT_MAX_HASHSIZE + 8 ];
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int dataLength, sigResult, status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );
	assert( isWritePtr( protocolInfo, sizeof( SCEP_PROTOCOL_INFO ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );
	REQUIRES( userNamePtr != NULL );

	/* Reset any issue-pending status that may have been set from a previous
	   operation */
	CLEAR_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_PENDING );

	/* Phase 1: Sig-check the data using the CA's key */
	DEBUG_DUMP_FILE( "scep_resp2", sessionInfoPtr->receiveBuffer, 
					 sessionInfoPtr->receiveBufEnd );
	status = envelopeSigCheck( sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufEnd,
							   sessionInfoPtr->receiveBuffer, 
							   sessionInfoPtr->receiveBufSize, &dataLength, 
							   sessionInfoPtr->iAuthInContext, &sigResult,
							   NULL, &iCmsAttributes, &errorInfo );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Invalid CMS signed data in CA response: " ) );
		}
	DEBUG_DUMP_FILE_OPT( "scep_resp1", sessionInfoPtr->receiveBuffer, 
						 dataLength );	/* May be zero len.if error status */
	if( cryptStatusError( sigResult ) )
		{
		/* The signed data was valid but the signature on it wasn't, this is
		   a different style of error than the previous one */
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( sigResult, 
				( sigResult, SESSION_ERRINFO, 
				  "Bad signature on CA response data" ) );
		}
	CFI_CHECK_UPDATE( "envelopeSigCheck" );

	/* Check that the returned transaction ID matches our transaction ID */
	setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_SCEP_TRANSACTIONID );
	if( cryptStatusError( status ) || \
		msgData.length != userNamePtr->valueLength || \
		memcmp( buffer, userNamePtr->value, userNamePtr->valueLength ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Returned transaction ID doesn't match our original "
				  "transaction ID" ) );
		}

	/* Check that the returned nonce matches our initial nonce.  It's now
	   identified as a recipient nonce since it's coming from the 
	   responder.  This is somewhat superfluous given that the transactionID
	   serves the same purpose, but we check it because it's in the spec */
	setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCmsAttributes, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_SCEP_RECIPIENTNONCE );
	if( cryptStatusError( status ) || \
		msgData.length != protocolInfo->nonceSize || \
		memcmp( buffer, protocolInfo->nonce, protocolInfo->nonceSize ) )
		{
		krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
		retExt( CRYPT_ERROR_SIGNATURE,
				( CRYPT_ERROR_SIGNATURE, SESSION_ERRINFO, 
				  "Returned nonce doesn't match our original nonce" ) );
		}
	CFI_CHECK_UPDATE( "IMESSAGE_GETATTRIBUTE_S" );

	/* Check that the operation succeeded */
	status = checkScepStatus( sessionInfoPtr, iCmsAttributes );
	krnlSendNotifier( iCmsAttributes, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "checkScepStatus" );

	/* Phase 2: Decrypt the data using either our self-signed key or our
	   password */
	if( protocolInfo->clientSignOnlyKey )
		{
		status = envelopeUnwrap( sessionInfoPtr->receiveBuffer, dataLength,
								 sessionInfoPtr->receiveBuffer, 
								 sessionInfoPtr->receiveBufSize, &dataLength, 
								 CRYPT_UNUSED, protocolInfo->userPassword, 
								 protocolInfo->userPasswordSize, &errorInfo );
		}
	else
		{
		status = envelopeUnwrap( sessionInfoPtr->receiveBuffer, dataLength,
								 sessionInfoPtr->receiveBuffer, 
								 sessionInfoPtr->receiveBufSize, &dataLength, 
								 sessionInfoPtr->privateKey, NULL, 0, 
								 &errorInfo );
		}
	if( cryptStatusError( status ) )
		{
		registerCryptoFailure();
		retExtErr( status, 
				   ( status, SESSION_ERRINFO, &errorInfo,
					 "Couldn't decrypt CMS enveloped data in CA "
					 "response: " ) );
		}
	DEBUG_DUMP_FILE( "scep_resp0", sessionInfoPtr->receiveBuffer, 
					 dataLength );
	CFI_CHECK_UPDATE( "envelopeUnwrap" );

	/* Finally, import the returned certificate(s) as a PKCS #7 chain */
	setMessageCreateObjectIndirectInfo( &createInfo,
								sessionInfoPtr->receiveBuffer, dataLength,
								CRYPT_CERTTYPE_CERTCHAIN );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, SESSION_ERRINFO, 
				  "Invalid PKCS #7 certificate chain in CA response" ) );
		}
	sessionInfoPtr->iCertResponse = createInfo.cryptHandle;
	CFI_CHECK_UPDATE( "IMESSAGE_DEV_CREATEOBJECT_INDIRECT" );

	REQUIRES( CFI_CHECK_SEQUENCE_5( "envelopeSigCheck",
									"IMESSAGE_GETATTRIBUTE_S", "checkScepStatus",
									"envelopeUnwrap",
									"IMESSAGE_DEV_CREATEOBJECT_INDIRECT" ) );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							SCEP Client Functions							*
*																			*
****************************************************************************/

/* Exchange data with a SCEP server */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransact( INOUT SESSION_INFO *sessionInfoPtr )
	{
	SCEP_PROTOCOL_INFO protocolInfo;
	SCEP_INFO *scepInfo = sessionInfoPtr->sessionSCEP;
	STREAM_PEER_TYPE peerSystemType;
	const BOOLEAN isPendingRequest = TEST_FLAG( sessionInfoPtr->protocolFlags, 
												SCEP_PFLAG_PENDING ) ? \
									 TRUE : FALSE;
	BOOLEAN sendPostAsGet = FALSE;
	CFI_CHECK_TYPE CFI_CHECK_VALUE = CFI_CHECK_INIT;
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );

	/* If the user hasn't explicitly set a request type, default to an 
	   initialisation request */
	if( scepInfo->requestType == CRYPT_REQUESTTYPE_NONE )
		scepInfo->requestType = CRYPT_REQUESTTYPE_INITIALISATION;

	/* Try and find out which extended SCEP capabilities the CA supports */
	if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_GOTCACAPS ) )
		{
		/* The returned status from getCACapabilities() isn't currently 
		   used, the only system for which we need it is Microsoft's NDES 
		   and that has a broken implementation of it unless the exact 
		   conditions given in the long comment in getCACapabilities() are 
		   met, a nice catch-22 where we can't identify the broken system 
		   because the mechanism used to identify its brokenness is in turn 
		   broken.
		   
		   We do however use the function side-effects indirectly by using 
		   the HTTP GET that's sent to fingerprint the remote server, which 
		   (usually) allows us to tell whether we're talking to NDES */
		( void ) getCACapabilities( sessionInfoPtr );
		
		/* We've got the CA capabilities, don't try and read them again */
		SET_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_GOTCACAPS );
		}
	CFI_CHECK_UPDATE( "getCACapabilities" );

	/* See whether we can determine the remote system type, used to work 
	   around bugs in implementations (all of which are currently from
	   Microsoft) */
	status = sioctlGet( &sessionInfoPtr->stream, STREAM_IOCTL_GETPEERTYPE, 
						&peerSystemType, sizeof( STREAM_PEER_TYPE ) );
	if( cryptStatusOK( status ) && peerSystemType != STREAM_PEER_NONE )
		{
		switch( peerSystemType )
			{
			case STREAM_PEER_MICROSOFT:
			case STREAM_PEER_MICROSOFT_2008:
				/* Microsoft doesn't support HTTP POST, but then it also 
				   doesn't have a working implementation of GetCACaps that
				   would indicate that it doesn't support HTTP POST either */
				sendPostAsGet = TRUE;
				break;

			case STREAM_PEER_MICROSOFT_2012:
				/* This server version has a different set of bugs, it 
				   now supports GetCACaps but silently switches to OAEP when 
				   AES is used.  If we're not running a custom build with 
				   OAEP enabled then we probably won't be able to continue 
				   later so we provide a warning about this now.
				   
				   In addition it now supports HTTP POST, but returns a
				   zero-length response as an "HTTP/1.1 200 OK"-status 
				   message in response to a POSTed request, so we still need 
				   to rely on HTTP GET to communicate with it */
#ifndef USE_OAEP
				DEBUG_PUTS(( "Peer is Server 2012, if decryption of the "
							 "returned message fails then this is due to "
							 "the server erroneously using OAEP in its "
							 "response" ));
#endif /* USE_OAEP */
				sendPostAsGet = TRUE;
				break;

			default:
				retIntError();
			}
		}

	/* Get the issuing CA certificate via SCEP's bolted-on HTTP GET facility 
	   if necessary */
	initSCEPprotocolInfo( &protocolInfo );
	if( sessionInfoPtr->iAuthInContext == CRYPT_ERROR )
		{
		status = getCACertificate( sessionInfoPtr, &protocolInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "initSCEPprotocolInfo" );

	/* If this is an initialisation request and we're not still waiting for 
	   a previous pending issue operation to complete, complete the PKCS #10 
	   request by adding the required SCEP attributes and signing it, and 
	   create the self-signed certificate that we need in order to sign and 
	   decrypt messages */
	if( !checkContextCapability( sessionInfoPtr->privateKey, 
								 MESSAGE_CHECK_PKC_DECRYPT ) )
		{
		/* The client's key is signature-only, remember this for later when
		   we need to perform encrypted messaging */
		protocolInfo.clientSignOnlyKey = TRUE;
		}
	if( ( scepInfo->requestType == CRYPT_REQUESTTYPE_INITIALISATION ) && \
		!isPendingRequest )
		{
		status = completeScepCertRequest( sessionInfoPtr, &protocolInfo );
		if( cryptStatusOK( status ) )
			status = createScepCert( sessionInfoPtr, &protocolInfo );
		if( cryptStatusError( status ) )
			return( status );
		}
	CFI_CHECK_UPDATE( "createScepCert" );

	/* Get a new certificate from the server */
	status = createScepRequest( sessionInfoPtr, &protocolInfo );
	if( cryptStatusOK( status ) )
		{
#ifdef USE_BASE64
		if( sendPostAsGet )
			status = writePkiDatagramAsGet( sessionInfoPtr );
		else
#endif /* USE_BASE64 */
			{
			status = writePkiDatagramEx( sessionInfoPtr, SCEP_CONTENT_TYPE,
										 SCEP_CONTENT_TYPE_LEN );
			}
		}
	if( cryptStatusOK( status ) )
		status = readPkiDatagram( sessionInfoPtr, MIN_CRYPT_OBJECTSIZE );
	if( cryptStatusOK( status ) )
		status = checkScepResponse( sessionInfoPtr, &protocolInfo );
	if( ( scepInfo->requestType == CRYPT_REQUESTTYPE_INITIALISATION ) && \
		!isPendingRequest )
		{
		/* If this is a new request rather than a renewal or a retry of an 
		   existing pending one then we've created a temporary certificate, 
		   clean it up before we exit */
		krnlSendNotifier( protocolInfo.iScepCert, IMESSAGE_DECREFCOUNT );
		}
	if( cryptStatusError( status ) )
		return( status );
	CFI_CHECK_UPDATE( "checkScepResponse" );

	REQUIRES( CFI_CHECK_SEQUENCE_4( "getCACapabilities", "initSCEPprotocolInfo", 
									"createScepCert", "checkScepResponse" ) );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int clientTransactWrapper( INOUT SESSION_INFO *sessionInfoPtr )
	{
	int status;

	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	REQUIRES( sanityCheckSessionSCEP( sessionInfoPtr ) );

	/* If it's not a plug-and-play PKI session, just pass the call on down
	   to the client transaction function */
	if( !TEST_FLAG( sessionInfoPtr->protocolFlags, SCEP_PFLAG_PNPPKI ) )
		return( clientTransact( sessionInfoPtr ) );

	/* We're doing plug-and-play PKI, point the transaction function at the 
	   client-transact function to execute the PnP steps, then reset it back 
	   to the PnP wrapper after we're done */
	FNPTR_SET( sessionInfoPtr->transactFunction, clientTransact );
	status = pnpPkiSession( sessionInfoPtr );
	FNPTR_SET( sessionInfoPtr->transactFunction, clientTransactWrapper );
	return( status );
	}

/****************************************************************************
*																			*
*							Session Access Routines							*
*																			*
****************************************************************************/

STDC_NONNULL_ARG( ( 1 ) ) \
void initSCEPclientProcessing( SESSION_INFO *sessionInfoPtr )
	{
	assert( isWritePtr( sessionInfoPtr, sizeof( SESSION_INFO ) ) );

	FNPTR_SET( sessionInfoPtr->transactFunction, clientTransactWrapper );
	}
#endif /* USE_SCEP */
