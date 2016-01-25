/****************************************************************************
*																			*
*					cryptlib DBMS CA Certificate Add Interface				*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "keyset.h"
  #include "dbms.h"
#else
  #include "crypt.h"
  #include "keyset/keyset.h"
  #include "keyset/dbms.h"
#endif /* Compiler-specific includes */

#ifdef USE_DBMS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Check that the request that we've been passed is in order */

CHECK_RETVAL_BOOL \
BOOLEAN checkRequest( IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
					  IN_ENUM( CRYPT_CERTACTION ) \
						const CRYPT_CERTACTION_TYPE action )
	{
	MESSAGE_DATA msgData;
	int certType, value, status;

	REQUIRES_B( isHandleRangeValid( iCertRequest ) );
	REQUIRES_B( action == CRYPT_CERTACTION_CERT_CREATION || \
				action == CRYPT_CERTACTION_ISSUE_CERT || \
				action == CRYPT_CERTACTION_REVOKE_CERT || \
				action == CRYPT_CERTACTION_NONE );

	/* Make sure that the request type is consistent with the operation
	   being performed */
	status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE,
							  &certType, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( FALSE );
	switch( action )
		{
		case CRYPT_CERTACTION_CERT_CREATION:
		case CRYPT_CERTACTION_ISSUE_CERT:
			if( certType != CRYPT_CERTTYPE_CERTREQUEST && \
				certType != CRYPT_CERTTYPE_REQUEST_CERT )
				return( FALSE );
			break;

		case CRYPT_CERTACTION_REVOKE_CERT:
			if( certType != CRYPT_CERTTYPE_REQUEST_REVOCATION )
				return( FALSE );
			break;

		case CRYPT_CERTACTION_NONE:
			/* We're performing a straight add of a request to the store,
			   any request type is permitted */
			break;

		default:
			retIntError_Boolean();
		}

	/* Make sure that the request is completed and valid.  We don't check
	   the signature on revocation requests since they aren't signed, and
	   have to be careful with CRMF requests which can be unsigned for
	   encryption-only keys */
	status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CERTINFO_IMMUTABLE );
	if( cryptStatusError( status ) || !value )
		return( FALSE );
	switch( certType )
		{
		case CRYPT_CERTTYPE_REQUEST_CERT:
			status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE,
									  &value, CRYPT_CERTINFO_SELFSIGNED );
			if( cryptStatusOK( status ) && !value )
				{
				/* It's an unsigned CRMF request, make sure that it really 
				   is an encryption-only key */
				status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE,
										  &value, CRYPT_CERTINFO_KEYUSAGE );
				if( cryptStatusOK( status ) && ( value & KEYUSAGE_SIGN ) )
					return( FALSE );
				break;
				}

			/* Fall through */

		case CRYPT_CERTTYPE_CERTREQUEST:
			status = krnlSendMessage( iCertRequest, IMESSAGE_CRT_SIGCHECK,
									  NULL, CRYPT_UNUSED );
			if( cryptStatusError( status ) )
				return( FALSE );
			break;

		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* Revocation requests are unsigned so we can't perform a 
			   signature check on them */
			break;

		default:
			retIntError_Boolean();
		}

	/* Check that required parameters are present.  This is necessary for
	   CRMF requests where every single parameter is optional, for our use
	   we require that a certificate request contains at least a subject DN 
	   and public key and a revocation request contains at least an issuer 
	   DN and serial number */
	switch( certType )
		{
		case CRYPT_CERTTYPE_CERTREQUEST:
		case CRYPT_CERTTYPE_REQUEST_CERT:
			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_SUBJECT );
			if( cryptStatusError( status ) )
				return( FALSE );
			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, CRYPT_IATTRIBUTE_SPKI );
			if( cryptStatusError( status ) )
				return( FALSE );
			break;

		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( iCertRequest, IMESSAGE_GETATTRIBUTE_S,
									  &msgData, 
									  CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
			if( cryptStatusError( status ) )
				return( FALSE );
			break;

		default:
			retIntError_Boolean();
		}

	return( TRUE );
	}

/* Check that a revocation request is consistent with information held in the
   certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkRevRequest( INOUT DBMS_INFO *dbmsInfo,
							IN_HANDLE const CRYPT_CERTIFICATE iCertRequest )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	char issuerID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int certIDlength, issuerIDlength, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iCertRequest ) );

	/* Check that the certificate being referred to in the request is 
	   present and active */
	status = getKeyID( issuerID, ENCODED_DBXKEYID_SIZE, &issuerIDlength, 
					   iCertRequest, CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusOK( status ) )
		{
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, issuerID, issuerIDlength );
		status = dbmsQuery(
			"SELECT certData FROM certificates WHERE issuerID = ?",
							NULL, 0, NULL, boundDataPtr,
							DBMS_CACHEDQUERY_ISSUERID, DBMS_QUERY_CHECK );
		}
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );

	/* The certificate isn't an active certificate, it's either not present 
	   or not active, return an appropriate error code.  If this request has 
	   been entered into the certificate store log then it's a duplicate 
	   request, otherwise it's a request to revoke a non-present certificate 
	   (either that or something really obscure which is best reported as a 
	   non-present certificate problem) */
	status = getKeyID( certID, ENCODED_DBXKEYID_SIZE, &certIDlength, 
					   iCertRequest, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		{
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, certID, certIDlength );
		status = dbmsQuery(
			"SELECT certData FROM certLog WHERE certID = ?",
							NULL, 0, NULL, boundDataPtr,
							DBMS_CACHEDQUERY_NONE, DBMS_QUERY_CHECK );
		}
	return( cryptStatusOK( status ) ? \
			CRYPT_ERROR_DUPLICATE : CRYPT_ERROR_NOTFOUND );
	}

/****************************************************************************
*																			*
*							Certificate Add Functions						*
*																			*
****************************************************************************/

/* Add a new PKI user to the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int caAddPKIUser( INOUT DBMS_INFO *dbmsInfo, 
				  IN_HANDLE const CRYPT_CERTIFICATE iPkiUser,
				  INOUT ERROR_INFO *errorInfo )
	{
	BYTE certData[ MAX_CERT_SIZE + 8 ];
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int certDataLength, certIDlength = DUMMY_INIT, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iPkiUser ) );
	REQUIRES( errorInfo != NULL );

	/* Extract the information that we need from the PKI user object.  In
	   addition to simply obtaining the information for logging purposes we
	   also need to perform this action to tell the certificate management 
	   code to fill in the remainder of the (implicitly-added) user 
	   information  before we start querying fields as we add it to the 
	   certificate store.  Because of this we also need to place the certID 
	   fetch after the object export since it's in an incomplete state 
	   before this point */
	status = extractCertData( iPkiUser, CRYPT_ICERTFORMAT_DATA,
							  certData, MAX_CERT_SIZE, &certDataLength );
	if( cryptStatusOK( status ) )
		status = getKeyID( certID, ENCODED_DBXKEYID_SIZE, &certIDlength, 
						   iPkiUser, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't extract PKI user data to add to certificate "
				  "store" ) );
		}

	/* Update the certificate store */
	status = addCert( dbmsInfo, iPkiUser, CRYPT_CERTTYPE_PKIUSER,
					  CERTADD_NORMAL, DBMS_UPDATE_BEGIN, errorInfo );
	if( cryptStatusOK( status ) )
		{
		status = updateCertLog( dbmsInfo, CRYPT_CERTACTION_ADDUSER, 
								certID, certIDlength, NULL, 0, NULL, 0, 
								certData, certDataLength, 
								DBMS_UPDATE_COMMIT );
		}
	else
		{
		/* Something went wrong, abort the transaction */
		dbmsUpdate( NULL, NULL, DBMS_UPDATE_ABORT );
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "PKI user add operation failed: " ) );
		}

	return( status );
	}

/* Delete a PKI user from the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int caDeletePKIUser( INOUT DBMS_INFO *dbmsInfo, 
					 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
					 IN_BUFFER( keyIDlength ) const void *keyID, 
					 IN_LENGTH_KEYID const int keyIDlength,
					 INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CERTIFICATE iPkiUser;
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int certIDlength = DUMMY_INIT, dummy, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( errorInfo != NULL );

	/* Get information on the PKI user that we're about to delete */
	status = getItemData( dbmsInfo, &iPkiUser, &dummy, 
						  KEYMGMT_ITEM_PKIUSER, keyIDtype, 
						  keyID, keyIDlength, KEYMGMT_FLAG_NONE, 
						  errorInfo );
	if( cryptStatusOK( status ) )
		{
		status = getKeyID( certID, ENCODED_DBXKEYID_SIZE, &certIDlength, 
						   iPkiUser, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		krnlSendNotifier( iPkiUser, IMESSAGE_DECREFCOUNT );
		}
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Couldn't get information on PKI user to be "
					 "deleted: " ) );
		}

	/* Delete the PKI user information and record the deletion */
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, certID, certIDlength );
	status = dbmsUpdate( 
			"DELETE FROM pkiUsers WHERE certID = ?", 
						 boundDataPtr, DBMS_UPDATE_BEGIN );
	if( cryptStatusOK( status ) )
		status = updateCertLog( dbmsInfo, CRYPT_CERTACTION_DELETEUSER, 
								NULL, 0, NULL, 0, certID, certIDlength, 
								NULL, 0, DBMS_UPDATE_COMMIT );
	else
		{
		/* Something went wrong, abort the transaction */
		dbmsUpdate( NULL, NULL, DBMS_UPDATE_ABORT );
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "PKI user delete operation failed: " ) );
		}

	return( status );
	}

/* Add a certificate issue or revocation request to the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int caAddCertRequest( INOUT DBMS_INFO *dbmsInfo, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
					  IN_ENUM( CRYPT_CERTTYPE ) \
						const CRYPT_CERTTYPE_TYPE requestType, 
					  const BOOLEAN isRenewal, const BOOLEAN isInitialOp,
					  INOUT ERROR_INFO *errorInfo )
	{
	BYTE certData[ MAX_CERT_SIZE + 8 ];
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	char reqCertID[ ENCODED_DBXKEYID_SIZE + 8 ], *reqCertIDptr = reqCertID;
	int certIDlength, reqCertIDlength = DUMMY_INIT;
	int certDataLength = DUMMY_INIT, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iCertRequest ) );
	REQUIRES( requestType == CRYPT_CERTTYPE_CERTREQUEST || \
			  requestType == CRYPT_CERTTYPE_REQUEST_CERT || \
			  requestType == CRYPT_CERTTYPE_REQUEST_REVOCATION );
	REQUIRES( errorInfo != NULL );

	/* Make sure that the request is OK and if it's a revocation request 
	   make sure that it refers to a certificate which is both present in 
	   the store and currently active */
	if( !checkRequest( iCertRequest, CRYPT_CERTACTION_NONE ) )
		{
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, errorInfo, 
					 "Certificate request information "
					 "inconsistent/invalid" ) );
		}
	if( requestType == CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		status = checkRevRequest( dbmsInfo, iCertRequest );
		if( cryptStatusError( status ) )
			retExt( status, 
					( status, errorInfo, 
					  "Revocation request doesn't correspond to a currently "
					  "active certificate" ) );
		}

	/* Extract the information that we need from the certificate request */
	status = getKeyID( certID, ENCODED_DBXKEYID_SIZE, &certIDlength, 
					   iCertRequest, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		{
		status = extractCertData( iCertRequest, 
								  ( requestType == CRYPT_CERTTYPE_REQUEST_REVOCATION ) ? \
									CRYPT_ICERTFORMAT_DATA : \
									CRYPT_CERTFORMAT_CERTIFICATE,
								  certData, MAX_CERT_SIZE, &certDataLength );
		}
	if( cryptStatusOK( status ) )
		{
		status = getKeyID( reqCertID, ENCODED_DBXKEYID_SIZE, &reqCertIDlength, 
						   iCertRequest, CRYPT_IATTRIBUTE_AUTHCERTID );
		if( cryptStatusError( status ) )
			{
			/* If the request is being added directly by the user, there's no
			   authorising certificate/PKI user information present */
			reqCertIDptr = NULL;
			reqCertIDlength = 0;
			status = CRYPT_OK;
			}
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't extract certificate request data to add to "
				  "certificate store" ) );
		}

	/* Check that the PKI user who authorised this certificate issue still 
	   exists.  If the CA has deleted them all further requests for 
	   certificates fail */
	if( reqCertIDptr != NULL )
		{
		CRYPT_CERTIFICATE iPkiUser;

		status = caGetIssuingUser( dbmsInfo, &iPkiUser, reqCertID, 
								   reqCertIDlength, errorInfo );
		if( cryptStatusOK( status ) )
			krnlSendNotifier( iPkiUser, IMESSAGE_DECREFCOUNT );
		else
			{
			updateCertErrorLog( dbmsInfo, CRYPT_ERROR_DUPLICATE,
								"Certificate request submitted for "
								"nonexistent PKI user", NULL, 0, 
								reqCertID, reqCertIDlength, NULL, 0, 
								NULL, 0 );
			retExt( CRYPT_ERROR_PERMISSION, 
					( CRYPT_ERROR_PERMISSION, errorInfo, 
					  "Certificate request submitted for nonexistent PKI "
					  "user" ) );
			}
		}

	/* If there's an authorising PKI user present make sure that it hasn't
	   already been used to authorise the issuance of a certificate.  This 
	   is potentially vulnerable to the following race condition:

		1: check authCertID -> OK
		2: check authCertID -> OK
		1: add
		2: add

	   In theory we could detect this by requiring the reqCertID to be 
	   unique, however a PKI user can be used to request both a certificate 
	   and a revocation for the certificate and a signing certificate can 
	   be used to request an update or revocation of both itself and one or 
	   more associated encryption certificates.  We could probably handle 
	   this via the ID-mangling used for certIDs but this makes tracing 
	   events through the audit log complex since there'll now be different 
	   effective IDs for the authorising certificate depending on what it 
	   was authorising.  
	   
	   In addition it's not certain how many further operations a 
	   certificate (rather than a PKI user) can authorise, in theory a 
	   single signing certificate can authorise at least four further 
	   operations, these being the update of itself, the update of an 
	   associated encryption certificate, and the revocation of itself and 
	   the encryption certificate.  In addition its possible that a signing 
	   certificate could be used to authorise a series of short-duration 
	   encryption certificates or a variety of other combinations of 
	   operations.

	   Because of these issues we can't use a uniqueness constraint on the
	   reqCertID to enforce a single use of issuing authorisation by the
	   database ifself but have to do a manual check here, checking 
	   specifically for the case where a PKI user authorises a certificate 
	   issue.  In addition we can't even perform a general-purpose check 
	   because as soon as a single issue has been authorised for a user 
	   there'll be a request for that user logged so that all further 
	   attempts to submit a request (for example for a renewal, or an 
	   encryption certificate to go with a signing one) will fail.  Because 
	   of this we only perform a check on the first attempt to issue a 
	   certificate, indicated by the caller setting the isInitialOp flag */
	if( isInitialOp && reqCertIDptr != NULL )
		{
		BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;

		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, reqCertID, reqCertIDlength );
		status = dbmsQuery(
			"SELECT certID FROM certLog WHERE reqCertID = ? "
			"AND action = " TEXT_CERTACTION_REQUEST_CERT,
							NULL, 0, NULL, boundDataPtr, 
							DBMS_CACHEDQUERY_NONE, DBMS_QUERY_CHECK );
		if( cryptStatusOK( status ) )
			{
			/* This user has already authorised the issue of a certificate, 
			   it can't be used to issue a second certificate */
			updateCertErrorLog( dbmsInfo, CRYPT_ERROR_DUPLICATE,
								"Attempt to authorise additional certificate "
								"issue when a certificate for this user has "
								"already been issued", NULL, 0, 
								reqCertID, reqCertIDlength, NULL, 0, NULL, 0 );
			retExt( CRYPT_ERROR_DUPLICATE, 
					( CRYPT_ERROR_DUPLICATE, errorInfo, 
					  "Attempt to authorise additional certificate issue "
					  "when a certificate for this user has already been "
					  "issued" ) );
			}
		}

	/* Update the certificate store.  Since a revocation request generally 
	   won't have any fields of any significance set we have to use a 
	   special cut-down insert statement that doesn't expect to find any 
	   fields except the certificate ID */
	if( requestType == CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
		char encodedCertData[ MAX_ENCODED_CERT_SIZE + 8 ];

		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, certID, certIDlength );
		if( hasBinaryBlobs( dbmsInfo ) )
			{
			setBoundDataBlob( boundDataPtr, 1, certData, certDataLength );
			}
		else
			{
			int encodedCertDataLength;
			
			status = base64encode( encodedCertData, MAX_ENCODED_CERT_SIZE, 
								   &encodedCertDataLength, certData, 
								   certDataLength, CRYPT_CERTTYPE_NONE );
			if( cryptStatusError( status ) )
				{
				DEBUG_DIAG(( "Couldn't base64-encode data" ));
				assert( DEBUG_WARN );
				return( status );
				}
			setBoundData( boundDataPtr, 1, encodedCertData, 
						  encodedCertDataLength );
			}
		status = dbmsUpdate( 
				"INSERT INTO certRequests VALUES ("
				TEXT_CERTTYPE_REQUEST_REVOCATION ", '', '', '', '', '', '', "
				"'', ?, ?)", boundDataPtr, DBMS_UPDATE_BEGIN );
		}
	else
		{
		status = addCert( dbmsInfo, iCertRequest, CRYPT_CERTTYPE_REQUEST_CERT,
						  CERTADD_NORMAL, DBMS_UPDATE_BEGIN, errorInfo );
		}
	if( cryptStatusOK( status ) )
		{
		status = updateCertLog( dbmsInfo,
						( requestType == CRYPT_CERTTYPE_REQUEST_REVOCATION ) ? \
							CRYPT_CERTACTION_REQUEST_REVOCATION : \
						( isRenewal ) ? \
							CRYPT_CERTACTION_REQUEST_RENEWAL : \
							CRYPT_CERTACTION_REQUEST_CERT,
						certID, certIDlength, reqCertIDptr, reqCertIDlength, 
						NULL, 0, certData, certDataLength, 
						DBMS_UPDATE_COMMIT );
		}
	else
		{
		/* Something went wrong, abort the transaction */
		dbmsUpdate( NULL, NULL, DBMS_UPDATE_ABORT );
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Certificate request add operation failed: " ) );
		}

	return( status );
	}
#endif /* USE_DBMS */
