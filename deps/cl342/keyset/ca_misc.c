/****************************************************************************
*																			*
*					cryptlib DBMS CA Certificate Misc Interface				*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#include <stdio.h>		/* For snprintf() */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "keyset.h"
  #include "dbms.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/keyset.h"
  #include "keyset/dbms.h"
#endif /* Compiler-specific includes */

#ifdef USE_DBMS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#if 0

/* Get the ultimate successor certificate for one that's been superseded */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getSuccessorCert( INOUT DBMS_INFO *dbmsInfo,
							 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
							 IN_BUFFER( initialCertIDlength ) \
								const char *initialCertID,
							 IN_LENGTH_SHORT const int initialCertIDlength )
	{
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int chainingLevel, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( *iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( initialCertID, initialCertIDlength ) );

	/* Walk through the chain of renewals in the certificate store log until 
	   we find the ultimate successor certificate to the current one */
	memcpy( certID, initialCertID, initialCertIDlength );
	for( chainingLevel = 0, status = CRYPT_ERROR_NOTFOUND;
		 status == CRYPT_ERROR_NOTFOUND && \
			chainingLevel < FAILSAFE_ITERATIONS_MED;
		 chainingLevel++ )
		{
		BYTE keyCertID[ DBXKEYID_SIZE + 8 ];
		char certData[ MAX_QUERY_RESULT_SIZE + 8 ];
		int certDataLength, length, dummy;

		/* Find the request to renew this certificate */
		status = dbmsQuery(
			"SELECT certID FROM certLog WHERE subjCertID = ? "
			"AND action = " TEXT_CERTACTION_REQUEST_RENEWAL,
							certData, &certDataLength, certID,
							strlen( certID ), 0, DBMS_CACHEDQUERY_NONE,
							DBMS_QUERY_NORMAL );
		if( cryptStatusError( status ) )
			return( status );

		/* Find the resulting certificate */
		memcpy( certID, certData,
				min( certDataLength, ENCODED_DBXKEYID_SIZE + 1 ) );
		certID[ MAX_ENCODED_DBXKEYID_SIZE ] = '\0';
		status = dbmsQuery(
			"SELECT certID FROM certLog WHERE reqCertID = ? "
				"AND action = " TEXT_CERTACTION_CERT_CREATION,
							certData, &certDataLength, certID,
							strlen( certID ), 0, DBMS_CACHEDQUERY_NONE,
							DBMS_QUERY_NORMAL );
		if( cryptStatusOK( status ) )
			{
			status = length = \
				base64decode( keyCertID, certData,
							  min( certDataLength, ENCODED_DBXKEYID_SIZE ),
							  CRYPT_CERTFORMAT_NONE );
			assert( !cryptStatusError( status ) );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* Try and get the replacement certificate */
		status = getItemData( dbmsInfo, iCertificate, &dummy,
							  getKeyName( CRYPT_IKEYID_CERTID ),
							  keyCertID, length, KEYMGMT_ITEM_PUBLICKEY,
							  KEYMGMT_FLAG_NONE, errorInfo );
		}
	if( chainingLevel >= FAILSAFE_ITERATIONS_MED )
		{
		/* We've chained through too many entries, bail out */
		return( CRYPT_ERROR_OVERFLOW );
		}

	return( status );
	}
#endif /* 0 */

/****************************************************************************
*																			*
*								Logging Functions							*
*																			*
****************************************************************************/

/* Add an entry to the CA log */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int updateCertLog( INOUT DBMS_INFO *dbmsInfo, 
				   IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action, 
				   IN_BUFFER_OPT( certIDlength ) const char *certID, 
				   IN_LENGTH_SHORT_Z const int certIDlength,
				   IN_BUFFER_OPT( reqCertIDlength ) const char *reqCertID, 
				   IN_LENGTH_SHORT_Z const int reqCertIDlength,
				   IN_BUFFER_OPT( subjCertIDlength ) const char *subjCertID, 
				   IN_LENGTH_SHORT_Z const int subjCertIDlength,
				   IN_BUFFER_OPT( dataLength ) const void *data, 
				   IN_LENGTH_SHORT_Z const int dataLength, 
				   IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	char sqlBuffer[ MAX_SQL_QUERY_SIZE + 8 ];
	char certIDbuffer[ ENCODED_DBXKEYID_SIZE + 8 ];
	char encodedCertData[ MAX_ENCODED_CERT_SIZE + 8 ];
	const time_t boundDate = getApproxTime();
	int localCertIDlength = certIDlength, sqlOffset, sqlLength, boundDataIndex;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( ( certID == NULL && certIDlength == 0 ) || \
			isReadPtr( certID, certIDlength ) );
	assert( ( reqCertID == NULL && reqCertIDlength == 0 ) || \
			isReadPtr( reqCertID, reqCertIDlength ) );
	assert( ( subjCertID == NULL && subjCertIDlength == 0 ) || \
			isReadPtr( subjCertID, subjCertIDlength ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );
	
	REQUIRES( action > CRYPT_CERTACTION_NONE && \
			  action < CRYPT_CERTACTION_LAST );
	REQUIRES( ( certID == NULL && certIDlength == 0 ) || \
			  ( certID != NULL && \
				certIDlength > 0 && \
				certIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( reqCertID == NULL && reqCertIDlength == 0 ) || \
			  ( reqCertID != NULL && \
				reqCertIDlength > 0 && \
				reqCertIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( subjCertID == NULL && subjCertIDlength == 0 ) || \
			  ( subjCertID != NULL && \
				subjCertIDlength > 0 && \
				subjCertIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && \
				dataLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( updateType > DBMS_UPDATE_NONE && \
			  updateType < DBMS_UPDATE_LAST );

	/* Build up the necessary SQL format string required to insert the log
	   entry.  This is complicated somewhat by the fact that some of the
	   values may be NULL so we have to insert them by naming the columns
	   (some databases allow the use of the DEFAULT keyword but this isn't
	   standardised enough to be safe) */
	strlcpy_s( sqlBuffer, MAX_SQL_QUERY_SIZE,
			  "INSERT INTO certLog (action, actionTime, certID" );
	if( reqCertID != NULL )
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, ", reqCertID" );
	if( subjCertID != NULL )
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, ", subjCertID" );
	if( data != NULL )
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, ", certData" );
	strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, ") VALUES (" );
	sqlOffset = strlen( sqlBuffer );
	sqlLength = MAX_SQL_QUERY_SIZE - sqlOffset;
	sprintf_s( sqlBuffer + sqlOffset, sqlLength, "%d, ?, ?", action );
	if( reqCertID != NULL )
		strlcat_s( sqlBuffer + sqlOffset, sqlLength, ", ?" );
	if( subjCertID != NULL )
		strlcat_s( sqlBuffer + sqlOffset, sqlLength, ", ?" );
	if( data != NULL )
		strlcat_s( sqlBuffer + sqlOffset, sqlLength, ", ?" );
	strlcat_s( sqlBuffer + sqlOffset, sqlLength, ")" );

	/* If we're not worried about the certID we just insert a nonce value
	   which is used to meet the constraints for a unique entry.  In order
	   to ensure that it doesn't clash with a real certID we set the first
	   four characters to an out-of-band value */
	if( certID == NULL )
		{
		MESSAGE_DATA msgData;
		BYTE nonce[ KEYID_SIZE + 8 ];
		int status;

		setMessageData( &msgData, nonce, KEYID_SIZE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusOK( status ) )
			status = base64encode( certIDbuffer, ENCODED_DBXKEYID_SIZE, 
								   &localCertIDlength, nonce, DBXKEYID_SIZE, 
								   CRYPT_CERTTYPE_NONE );
		if( cryptStatusError( status ) )
			{
			/* Normally this is a should-never-occur error, however if
			   cryptlib has been shut down from another thread the kernel
			   will fail all non shutdown-related calls with a permission
			   error.  To avoid false alarms, we mask out failures due to
			   permission errors */
			assert( ( status == CRYPT_ERROR_PERMISSION ) || DEBUG_WARN );
			return( status );
			}
		memset( certIDbuffer, '-', 4 );
		certID = certIDbuffer;
		}

	/* Set up the parameter information and update the certificate store 
	   log */
	initBoundData( boundDataPtr );
	setBoundDataDate( boundDataPtr, 0, &boundDate );
	setBoundData( boundDataPtr, 1, certID, localCertIDlength );
	boundDataIndex = 2;
	if( reqCertID != NULL )
		setBoundData( boundDataPtr, boundDataIndex++, reqCertID, 
					  reqCertIDlength );
	if( subjCertID != NULL )
		setBoundData( boundDataPtr, boundDataIndex++, subjCertID, 
					  subjCertIDlength );
	if( data != NULL )
		{
		if( hasBinaryBlobs( dbmsInfo ) )
			{
			setBoundDataBlob( boundDataPtr, boundDataIndex, 
							  data, dataLength );
			}
		else
			{
			int encodedDataLength, status;

			status = base64encode( encodedCertData, MAX_ENCODED_CERT_SIZE,
								   &encodedDataLength, data, dataLength, 
								   CRYPT_CERTTYPE_NONE );
			if( cryptStatusError( status ) )
				{
				DEBUG_DIAG(( "Couldn't base64-encode data" ));
				assert( DEBUG_WARN );
				return( status );
				}
			setBoundData( boundDataPtr, boundDataIndex, 
						  encodedCertData, encodedDataLength );
			}
		}
	return( dbmsUpdate( sqlBuffer, boundDataPtr, updateType ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateCertErrorLog( INOUT DBMS_INFO *dbmsInfo, 
						IN_ERROR const int errorStatus,
						IN_STRING const char *errorString, 
						IN_BUFFER_OPT( certIDlength ) const char *certID, 
						IN_LENGTH_SHORT_Z const int certIDlength,
						IN_BUFFER_OPT( reqCertIDlength ) const char *reqCertID, 
						IN_LENGTH_SHORT_Z const int reqCertIDlength,
						IN_BUFFER_OPT( subjCertIDlength ) const char *subjCertID, 
						IN_LENGTH_SHORT_Z const int subjCertIDlength,
						IN_BUFFER_OPT( dataLength ) const void *data, 
						IN_LENGTH_SHORT_Z const int dataLength )
	{
	STREAM stream;
	BYTE errorData[ 64 + MAX_CERT_SIZE + 8 ];
	const int errorStringLength = strlen( errorString );
	int errorDataLength = DUMMY_INIT, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( ( certID == NULL && certIDlength == 0 ) || \
			isReadPtr( certID, certIDlength ) );
	assert( ( reqCertID == NULL && reqCertIDlength == 0 ) || \
			isReadPtr( reqCertID, reqCertIDlength ) );
	assert( ( subjCertID == NULL && subjCertIDlength == 0 ) || \
			isReadPtr( subjCertID, subjCertIDlength ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtr( data, dataLength ) );

	REQUIRES( cryptStatusError( errorStatus ) );
	REQUIRES( errorString != NULL );
	REQUIRES( ( certID == NULL && certIDlength == 0 ) || \
			  ( certID != NULL && \
				certIDlength > 0 && \
				certIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( reqCertID == NULL && reqCertIDlength == 0 ) || \
			  ( reqCertID != NULL && \
				reqCertIDlength > 0 && \
				reqCertIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( subjCertID == NULL && subjCertIDlength == 0 ) || \
			  ( subjCertID != NULL && \
				subjCertIDlength > 0 && \
				subjCertIDlength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( ( data == NULL && dataLength == 0 ) || \
			  ( data != NULL && \
				dataLength > 0 && \
				dataLength < MAX_INTLENGTH_SHORT ) );

	/* Encode the error information:

		SEQUENCE {
			errorStatus	INTEGER,
			errorString	UTF8String,
			certData	ANY OPTIONAL
			} 

	   Note that the buffer we use is slightly larger than MAX_CERT_SIZE in 
	   order to accomodate the error status information alongside the 
	   largest possible certificate, in theory this means that if the database
	   back-end doesn't support binary blobs there won't be enough room in
	   the logging code to text-encode this worst-case scenario, but the use
	   of non-binary-blob capable database should be fairly rare so it's 
	   easier to just rely on the logging code to catch this unlikely 
	   scenario than to try and special-case around it */
	sMemOpen( &stream, errorData, 64 + MAX_CERT_SIZE );
	writeSequence( &stream, sizeofShortInteger( -errorStatus ) + \
							( int ) sizeofObject( errorStringLength ) + \
							dataLength );
	writeShortInteger( &stream, -errorStatus, DEFAULT_TAG );
	status = writeCharacterString( &stream, errorString, errorStringLength,
								   BER_STRING_UTF8 );
	if( dataLength > 0 )
		status = swrite( &stream, data, dataLength );
	if( cryptStatusOK( status ) )
		errorDataLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		sMemOpen( &stream, errorData, MAX_CERT_SIZE );
		writeSequence( &stream, sizeofObject( 1 ) + sizeofObject( 31 ) );
		writeShortInteger( &stream, -( CRYPT_ERROR_FAILED ), DEFAULT_TAG );
		status = writeCharacterString( &stream, 
									   "Error writing error information", 31,
									   BER_STRING_UTF8 );
		if( cryptStatusOK( status ) )
			errorDataLength = stell( &stream );
		sMemDisconnect( &stream );
		}
	ENSURES( cryptStatusOK( status ) );

	/* Update the certificate store log with the error information as the 
	   data value */
	return( updateCertLog( dbmsInfo, CRYPT_CERTACTION_ERROR, 
						   certID, certIDlength, reqCertID, reqCertIDlength, 
						   subjCertID, subjCertIDlength, 
						   errorData, errorDataLength, DBMS_UPDATE_NORMAL ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int updateCertErrorLogMsg( INOUT DBMS_INFO *dbmsInfo, 
						   IN_ERROR const int errorStatus,
						   IN_STRING const char *errorString )
	{
	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	return( updateCertErrorLog( dbmsInfo, errorStatus, errorString,
								NULL, 0, NULL, 0, NULL, 0, NULL, 0 ) );
	}

/****************************************************************************
*																			*
*							Miscellaneous CA Functions						*
*																			*
****************************************************************************/

/* Get the PKI user that originally authorised the issuance of a certificate.  
   This can involve chaining back through multiple generations of 
   certificates, for example to check authorisation on a revocation request 
   we might have to go through:

	rev_req:	get reqCertID = update_req
	update_req:	get reqCertID = cert_req
	cert_req:	get reqCertID = init_req
	init_req:	get reqCertID = pki_user */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 5 ) ) \
int caGetIssuingUser( INOUT DBMS_INFO *dbmsInfo, 
					  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iPkiUser,
					  IN_BUFFER( initialCertIDlength ) const char *initialCertID, 
					  IN_LENGTH_FIXED( ENCODED_DBXKEYID_SIZE ) \
							const int initialCertIDlength, 
					  INOUT ERROR_INFO *errorInfo )
	{
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int certIDlength, chainingLevel, dummy, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( iPkiUser, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( initialCertID, ENCODED_DBXKEYID_SIZE ) );

	REQUIRES( initialCertIDlength == ENCODED_DBXKEYID_SIZE );
	REQUIRES( errorInfo != NULL );

	/* Clear return value */
	*iPkiUser = CRYPT_ERROR;

	/* Walk through the chain of updates in the certificate store log until 
	   we find the PKI user that authorised the first certificate issue */
	memcpy( certID, initialCertID, initialCertIDlength );
	certIDlength = initialCertIDlength;
	for( chainingLevel = 0; chainingLevel < FAILSAFE_ITERATIONS_MED; 
		 chainingLevel++ )
		{
		BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
		BYTE certData[ MAX_QUERY_RESULT_SIZE + 8 ];
		int certDataLength;

		/* Find out whether this is a PKI user.  The comparison for the
		   action type is a bit odd since some back-ends will return the
		   action as text and some as a binary numeric value.  Rather than
		   relying on the back-end glue code to perform the appropriate
		   conversion we just check for either value type */
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, certID, certIDlength );
		status = dbmsQuery(
			"SELECT action FROM certLog WHERE certID = ?",
							certData, MAX_QUERY_RESULT_SIZE, &certDataLength, 
							boundDataPtr, DBMS_CACHEDQUERY_NONE, 
							DBMS_QUERY_NORMAL );
		if( cryptStatusError( status ) )
			return( status );
		if( certData[ 0 ] == CRYPT_CERTACTION_ADDUSER || \
			certData[ 0 ] == TEXTCH_CERTACTION_ADDUSER )
			{
			/* We've found the PKI user, we're done */
			break;
			}

		/* Find the certificate that was issued, recorded either as a
		   CERTACTION_CERT_CREATION for a multi-phase CMP-based certificate
		   creation or a CERTACTION_ISSUE_CERT for a one-step creation */
		status = dbmsQuery(
			"SELECT reqCertID FROM certLog WHERE certID = ?",
							certData, MAX_QUERY_RESULT_SIZE, &certDataLength, 
							boundDataPtr, DBMS_CACHEDQUERY_NONE, 
							DBMS_QUERY_NORMAL );
		if( cryptStatusError( status ) )
			return( status );
		certIDlength = min( certDataLength, ENCODED_DBXKEYID_SIZE );
		memcpy( certID, certData, certIDlength );

		/* Find the request to issue this certificate.  For a CMP-based issue
		   this will have an authorising object (found in the next iteration
		   through the loop), for a one-step issue it won't */
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, certID, certIDlength );
		status = dbmsQuery(
			"SELECT reqCertID FROM certLog WHERE certID = ?",
							certData, MAX_QUERY_RESULT_SIZE, &certDataLength, 
							boundDataPtr, DBMS_CACHEDQUERY_NONE, 
							DBMS_QUERY_NORMAL );
		if( cryptStatusError( status ) )
			return( status );
		certIDlength = min( certDataLength, ENCODED_DBXKEYID_SIZE );
		memcpy( certID, certData, certIDlength );
		}
	if( chainingLevel >= FAILSAFE_ITERATIONS_MED )
		{
		/* We've chained through too many entries, bail out */
		return( CRYPT_ERROR_OVERFLOW );
		}

	/* We've found the original PKI user, get the user information */
	return( getItemData( dbmsInfo, iPkiUser, &dummy, KEYMGMT_ITEM_PKIUSER,
						 CRYPT_IKEYID_CERTID, certID, certIDlength, 
						 KEYMGMT_FLAG_NONE, errorInfo ) );
	}

/****************************************************************************
*																			*
*						CA Certificate Management Interface					*
*																			*
****************************************************************************/

/* Perform a certificate management operation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int certMgmtFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							 OUT_OPT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
							 IN_HANDLE_OPT const CRYPT_CERTIFICATE caKey,
							 IN_HANDLE_OPT const CRYPT_CERTIFICATE request,
							 IN_ENUM( CRYPT_CERTACTION ) \
								const CRYPT_CERTACTION_TYPE action )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	char reqCertID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int reqCertIDlength, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( ( iCertificate == NULL ) || \
			isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( ( caKey == CRYPT_UNUSED ) || isHandleRangeValid( caKey ) );
	REQUIRES( ( request == CRYPT_UNUSED ) || isHandleRangeValid( request ) );
	REQUIRES( action > CRYPT_CERTACTION_NONE && \
			  action < CRYPT_CERTACTION_LAST );

	/* In order for various SQL query strings to use the correct values the
	   type values have to match their text equivalents defined at the start
	   of this file.  Since we can't check this at compile time we have to
	   do it here via an assertion */
	static_assert_opt( TEXT_CERTTYPE_REQUEST_CERT[ 0 ] - '0' == \
							CRYPT_CERTTYPE_REQUEST_CERT, \
					   "SQL data type" );
	static_assert_opt( TEXT_CERTTYPE_REQUEST_REVOCATION[ 0 ] - '0' == \
							CRYPT_CERTTYPE_REQUEST_REVOCATION, \
					   "SQL data type" );
	static_assert_opt( TEXT_CERTACTION_CREATE[ 0 ] - '0' == \
							CRYPT_CERTACTION_CREATE, \
					   "SQL data type" );
	static_assert( TEXTCH_CERTACTION_ADDUSER - '0' == \
						CRYPT_CERTACTION_ADDUSER, \
				   "SQL data type" );
	static_assert_opt( TEXT_CERTACTION_REQUEST_CERT[ 0 ] - '0' == \
							CRYPT_CERTACTION_REQUEST_CERT, \
					   "SQL data type" );
	static_assert( TEXTCH_CERTACTION_REQUEST_CERT - '0' == \
						CRYPT_CERTACTION_REQUEST_CERT, \
				   "SQL data type" );
	static_assert_opt( TEXT_CERTACTION_REQUEST_RENEWAL[ 0 ] - '0' == \
							CRYPT_CERTACTION_REQUEST_RENEWAL, \
					   "SQL data type" );
	static_assert( TEXTCH_CERTACTION_REQUEST_RENEWAL - '0' == \
				   CRYPT_CERTACTION_REQUEST_RENEWAL, \
				   "SQL data type" );
	static_assert_opt( TEXT_CERTACTION_CERT_CREATION[ 0 ] - '0' == \
							CRYPT_CERTACTION_CERT_CREATION / 10, \
					   "SQL data type" );
	static_assert_opt( TEXT_CERTACTION_CERT_CREATION[ 1 ] - '0' == \
							CRYPT_CERTACTION_CERT_CREATION % 10, \
					   "SQL data type" );

	/* Clear return value */
	if( iCertificate != NULL )
		*iCertificate = CRYPT_ERROR;

	/* If it's a simple certificate expire or cleanup, there are no 
	   parameters to check so we can perform the action immediately */
	if( action == CRYPT_CERTACTION_EXPIRE_CERT || \
		action == CRYPT_CERTACTION_CLEANUP )
		{
		REQUIRES( caKey == CRYPT_UNUSED );
		REQUIRES( request == CRYPT_UNUSED );

		return( caCleanup( dbmsInfo, action, KEYSET_ERRINFO ) );
		}

	/* If it's the completion of a certificate creation, process it */
	if( action == CRYPT_CERTACTION_CERT_CREATION_COMPLETE || \
		action == CRYPT_CERTACTION_CERT_CREATION_DROP || \
		action == CRYPT_CERTACTION_CERT_CREATION_REVERSE )
		{
		REQUIRES( caKey == CRYPT_UNUSED );

		return( caIssueCertComplete( dbmsInfo, request, action, 
									 KEYSET_ERRINFO ) );
		}

	/* Check that the CA key that we've been passed is in order.  These
	   checks are performed automatically during the issue process by the
	   kernel when we try and convert the request into a certificate, 
	   however we perform them explicitly here so that we can return a more 
	   meaningful error message to the caller */
	if( action == CRYPT_CERTACTION_ISSUE_CRL )
		{
		int value;

		/* If we're issuing a CRL, the key must be capable of CRL signing */
		status = krnlSendMessage( caKey, IMESSAGE_GETATTRIBUTE, &value,
								  CRYPT_CERTINFO_KEYUSAGE );
		if( cryptStatusError( status ) || \
			!( value & CRYPT_KEYUSAGE_CRLSIGN ) )
			{
			retExtArg( CAMGMT_ARGERROR_CAKEY, 
					   ( CAMGMT_ARGERROR_CAKEY, KEYSET_ERRINFO, 
						 "CA certificate isn't valid for CRL signing" ) );
			}
		}
	else
		{
		/* For anything other than a revocation action (which just updates 
		   the certificate store without doing anything else) the key must 
		   be a CA key */
		if( action != CRYPT_CERTACTION_REVOKE_CERT && \
			cryptStatusError( \
				krnlSendMessage( caKey, IMESSAGE_CHECK, NULL,
								 MESSAGE_CHECK_CA ) ) )
			{
			retExtArg( CAMGMT_ARGERROR_CAKEY, 
					   ( CAMGMT_ARGERROR_CAKEY, KEYSET_ERRINFO, 
						 "CA certificate isn't valid for certificate "
						 "signing" ) );
			}
		}

	/* If it's a CRL issue it's a read-only operation on the CRL store for 
	   which we only need the CA certificate (there's no request involved) */
	if( action == CRYPT_CERTACTION_ISSUE_CRL )
		{
		REQUIRES( request == CRYPT_UNUSED );

		return( caIssueCRL( dbmsInfo, iCertificate, caKey, KEYSET_ERRINFO ) );
		}

	/* We're processing an action that requires an explicit certificate 
	   request, perform further checks on the request */
	if( !checkRequest( request, action ) )
		{
		retExtArg( CAMGMT_ARGERROR_REQUEST, 
				   ( CAMGMT_ARGERROR_REQUEST, KEYSET_ERRINFO, 
					 "Certificate request information "
					 "inconsistent/invalid" ) );
		}

	/* Make sure that the request is present in the request table in order 
	   to issue a certificate for it.  Again, this will be checked later but 
	   we can return a more meaningful error here */
	status = getKeyID( reqCertID, ENCODED_DBXKEYID_SIZE, &reqCertIDlength, 
					   request, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusError( status ) )
		return( CAMGMT_ARGERROR_REQUEST );
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, reqCertID, reqCertIDlength );
	status = dbmsQuery(
		"SELECT certData FROM certRequests WHERE certID = ?",
						NULL, 0, NULL, boundDataPtr, 
						DBMS_CACHEDQUERY_NONE, DBMS_QUERY_CHECK );
	if( cryptStatusError( status ) )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "Certificate request doesn't correspond to any existing "
				  "request in the certificate store" ) );
		}

	/* If it's a revocation request, process it */
	if( action == CRYPT_CERTACTION_REVOKE_CERT )
		{
		REQUIRES( caKey == CRYPT_UNUSED );

		return( caRevokeCert( dbmsInfo, request, CRYPT_UNUSED,
							  CRYPT_CERTACTION_REVOKE_CERT, KEYSET_ERRINFO ) );
		}

	/* It's a certificate issue request, issue the certificate */
	REQUIRES( action == CRYPT_CERTACTION_ISSUE_CERT || \
			  action == CRYPT_CERTACTION_CERT_CREATION );
	REQUIRES( isHandleRangeValid( caKey ) );

	return( caIssueCert( dbmsInfo, iCertificate, caKey, request, action, 
						 KEYSET_ERRINFO ) );
	}

/* Set up the function pointers to the keyset methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSCA( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );

	keysetInfoPtr->keysetDBMS->certMgmtFunction = certMgmtFunction;

	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
