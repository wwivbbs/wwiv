/****************************************************************************
*																			*
*				cryptlib DBMS CA Certificate Revocation Interface			*
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

/* Get the certificate indicated in a revocation request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int getCertToRevoke( INOUT DBMS_INFO *dbmsInfo,
							OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
							IN_HANDLE const CRYPT_CERTIFICATE iCertRequest, 
							INOUT ERROR_INFO *errorInfo )
	{
	char issuerID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int dummy, issuerIDlength, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( isHandleRangeValid( iCertRequest ) );
	REQUIRES( errorInfo != NULL );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* Extract the certificate identity information from the request and try
	   and fetch it from the certificate store */
	status = getKeyID( issuerID, ENCODED_DBXKEYID_SIZE, &issuerIDlength, 
					   iCertRequest, CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( status ) )
		return( status );
	return( getItemData( dbmsInfo, iCertificate, &dummy, 
						 KEYMGMT_ITEM_PUBLICKEY, CRYPT_IKEYID_ISSUERID, 
						 issuerID, issuerIDlength, KEYMGMT_FLAG_NONE, 
						 errorInfo ) );
	}

/****************************************************************************
*																			*
*						Certificate Revocation Functions					*
*																			*
****************************************************************************/

/* Handle an indirect certificate revocation (one where we need to reverse a 
   certificate issue or otherwise remove the certificate without obtaining a 
   direct revocation request from the user).  The various revocation 
   situations are:

	Complete cert renewal				original certificate supplied
		CERTACTION_REVOKE_CERT			reason = superseded
										fail -> straight delete

	Reverse issue due to cancel in CMP	original certificate supplied
		CERTACTION_CREATION_REVERSE		reason = neverValid
										date = certificate issue date
										fail -> straight delete

	Undo issue after restart			original certificate supplied
		CERTACTION_CREATION_REVERSE		reason = neverValid
										date = certificate issue date
										fail -> straight delete

	( Standard revocation				original certificate not supplied
		CERTACTION_REVOKE_CERT			reason = <in request>
										delete request
										fail -> no action ) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int revokeCertDirect( INOUT DBMS_INFO *dbmsInfo,
					  IN_HANDLE const CRYPT_CERTIFICATE iCertificate,
					  IN_ENUM( CRYPT_CERTACTION ) \
						const CRYPT_CERTACTION_TYPE action,
					  INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CERTIFICATE iLocalCRL;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	time_t certDate;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCertificate ) );
	REQUIRES( action == CRYPT_CERTACTION_REVOKE_CERT || \
			  action == CRYPT_CERTACTION_CERT_CREATION_REVERSE );
	REQUIRES( errorInfo != NULL );

	/* Get any information needed for the revocation from the certificate */
	if( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, &certDate, sizeof( time_t ) );
		status = krnlSendMessage( iCertificate, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_VALIDFROM );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Create a (single-entry) CRL to contain the revocation information for 
	   the certificate and revoke it via the standard channels.  We go 
	   directly to a CRL rather than doing it via a revocation request 
	   because we need to add information that can only be added by a CA and 
	   only to a CRL */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CRL );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	iLocalCRL = createInfo.cryptHandle;
	status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE, 
							  ( MESSAGE_CAST ) &iCertificate, 
							  CRYPT_CERTINFO_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't create CRL entry from certificate to be "
				  "revoked" ) );
		}
	if( action == CRYPT_CERTACTION_REVOKE_CERT )
		{
		static const int crlReason = CRYPT_CRLREASON_SUPERSEDED;

		/* We're revoking the certificate because we're about to replace it, 
		   set the revocation reason to superseded */
		status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE, 
								  ( MESSAGE_CAST ) &crlReason, 
								  CRYPT_CERTINFO_CRLREASON );
		}
	else
		{
		static const int crlReason = CRYPT_CRLREASON_NEVERVALID;
		MESSAGE_DATA msgData;

		/* We're revoking a certificate issued in error, set the revocation 
		   and invalidity dates to the same value (the time of certificate 
		   issue) in the hope of ensuring that it's regarded as never being 
		   valid.  This isn't too accurate but since X.509 makes the 
		   assumption that all CAs are perfect and never make mistakes 
		   there's no other way to indicate that a certificate was issued in 
		   error.  In addition to this we set the extended reason to 
		   neverValid, but not too many implementations will check this */
		setMessageData( &msgData, &certDate, sizeof( time_t ) );
		status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_REVOCATIONDATE );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE_S, 
							&msgData, CRYPT_CERTINFO_INVALIDITYDATE );
		if( cryptStatusOK( status ) )
			status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE, 
							( MESSAGE_CAST ) &crlReason, 
							CRYPT_CERTINFO_CRLREASON );
		}
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE, 
								  MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't add revocation status information to CRL for "
				  "certificate revocation" ) );
		}
	status = caRevokeCert( dbmsInfo, iLocalCRL, iCertificate, action, 
						   errorInfo );
	krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
	return( status );
	}

/* Revoke a certificate from a revocation request */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int caRevokeCert( INOUT DBMS_INFO *dbmsInfo, 
				  IN_HANDLE const CRYPT_CERTIFICATE iCertRequest,
				  IN_HANDLE_OPT const CRYPT_CERTIFICATE iCertificate,
				  IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action,
				  INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CERTIFICATE iLocalCertificate = iCertificate;
	CRYPT_CERTIFICATE iLocalCRL = iCertRequest;
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	BYTE certData[ MAX_CERT_SIZE + 8 ];
	char reqCertID[ ENCODED_DBXKEYID_SIZE + 8 ], *reqCertIDptr = reqCertID;
	char subjCertID[ ENCODED_DBXKEYID_SIZE + 8 ];
	char specialCertID[ ENCODED_DBXKEYID_SIZE + 8 ];
	const BOOLEAN reqPresent = \
					( action == CRYPT_CERTACTION_RESTART_REVOKE_CERT || \
					  ( action == CRYPT_CERTACTION_REVOKE_CERT && \
						iCertificate == CRYPT_UNUSED ) ) ? TRUE : FALSE;
	int certDataLength = DUMMY_INIT, reqCertIDlength, subjCertIDlength;
	int specialCertIDlength = DUMMY_INIT, status = CRYPT_OK;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iCertRequest ) );
	REQUIRES( iCertificate == CRYPT_UNUSED || \
			  isHandleRangeValid( iCertificate ) );
	REQUIRES( action == CRYPT_CERTACTION_REVOKE_CERT || \
			  action == CRYPT_CERTACTION_RESTART_REVOKE_CERT || \
			  action == CRYPT_CERTACTION_CERT_CREATION_REVERSE );
	REQUIRES( errorInfo != NULL );

	/* This function handles a number of operations as summarised in the 
	   table below:

		Operation			Action				Request	On disk	Cert
		---------			------				-------	-------	----
		Complete revocation	RESTART_REVOKE_CERT	Rev.req	  Yes	 --
		on restart

		Standard revocation	REVOKE_CERT			Rev.req	  Yes	 --

		Complete renewal	REVOKE_CERT			crlEntry   --	Supplied

		Reverse issue (CMP	CREATION_REVERSE	crlEntry   --	Supplied
		or due to restart)

	   The following assertion checks that the certificate parameter is 
	   correct.  Checking the request parameter isn't so easy since it 
	   requires multiple function calls, and is done as part of the code */
	REQUIRES( ( action == CRYPT_CERTACTION_RESTART_REVOKE_CERT && \
				iCertificate == CRYPT_UNUSED ) || \
			  ( action == CRYPT_CERTACTION_REVOKE_CERT ) || \
			  ( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE && \
				isHandleRangeValid( iCertificate ) ) );

	/* If it's a standard revocation (rather than one done as part of an
	   internal certificate management operation, which passes in a single-
	   entry CRL) fetch the certificate that we're going to revoke and set 
	   up a CRL object to contain the revocation information */
	if( iCertificate == CRYPT_UNUSED )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;

		/* Get the certificate being revoked via the revocation request */
		status = getKeyID( reqCertID, ENCODED_DBXKEYID_SIZE, &reqCertIDlength, 
						   iCertRequest, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
		if( cryptStatusOK( status ) )
			status = getCertToRevoke( dbmsInfo, &iLocalCertificate,
									  iCertRequest, errorInfo );
		if( cryptStatusError( status ) )
			{
			retExtErr( status, 
					   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
						 "Couldn't find certificate to revoke in "
						 "certificate store: " ) );
			}

		/* Create the CRL to contain the revocation information */
		setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CRL );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
								  OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusOK( status ) )
			{
			/* Fill in the CRL from the revocation request */
			iLocalCRL = createInfo.cryptHandle;
			status = krnlSendMessage( iLocalCRL, IMESSAGE_SETATTRIBUTE,
									  ( MESSAGE_CAST ) &iCertRequest,
									  CRYPT_IATTRIBUTE_REVREQUEST );
			if( cryptStatusError( status ) )
				krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
			}
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iLocalCertificate, IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't create CRL from revocation request" ) );
			}

		}
	else
		{
		/* This is a direct revocation done as part of an internal 
		   certificate management operation, there's no explicit request 
		   for the revocation present and the caller has passed us a CRL 
		   ready to use */
		reqCertIDptr = NULL;
		reqCertIDlength = 0;
		}
	status = getKeyID( subjCertID, ENCODED_DBXKEYID_SIZE, &subjCertIDlength, 
					   iLocalCertificate, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		status = extractCertData( iLocalCRL, CRYPT_IATTRIBUTE_CRLENTRY,
								  certData, MAX_CERT_SIZE, &certDataLength );
	if( cryptStatusError( status ) )
		{
		/* If we created the necessary objects locally rather than having
		   them passed in by the caller we have to clean them up again
		   before we exit */
		if( iCertificate == CRYPT_UNUSED )
			{
			krnlSendNotifier( iLocalCertificate, IMESSAGE_DECREFCOUNT );
			krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
			}
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't extract CRL data to add to certificate store" ) );
		}

	/* If it's a certificate creation reversal operation then the 
	   certificate will be stored under a special temporary-status ID so we 
	   turn the general subject certID into the form required for 
	   special-case certificate data */
	if( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE )
		{
		memcpy( specialCertID, subjCertID, subjCertIDlength );
		memcpy( specialCertID, KEYID_ESC1, KEYID_ESC_SIZE );
		specialCertIDlength = subjCertIDlength;
		}

	/* Update the certificate store.  This is the ugliest CA operation since 
	   it touches every table, luckily it's performed only rarely.
	   
	   If this is a reversal operation or revocation of a certificate to be 
	   replaced, which is a direct follow-on to a certificate creation, 
	   there's no corresponding request present so we don't have to update 
	   the requests table */
	status = addCRL( dbmsInfo, iLocalCRL, iLocalCertificate,
					 DBMS_UPDATE_BEGIN, errorInfo );
	if( cryptStatusOK( status ) )
		{
		status = updateCertLog( dbmsInfo, action, NULL, 0, reqCertIDptr, 
								reqCertIDlength, subjCertID, 
								subjCertIDlength, certData, certDataLength, 
								DBMS_UPDATE_CONTINUE );
		}
	if( cryptStatusOK( status ) && reqPresent )
		{
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, reqCertID, reqCertIDlength );
		status = dbmsUpdate( 
			"DELETE FROM certRequests WHERE certID = ?",
							 boundDataPtr, DBMS_UPDATE_CONTINUE );
		}
	if( cryptStatusOK( status ) )
		{
		initBoundData( boundDataPtr );
		if( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE )
			{
			setBoundData( boundDataPtr, 0, specialCertID, 
						  specialCertIDlength );
			status = dbmsUpdate( 
				"DELETE FROM certificates WHERE certID = ?",
								 boundDataPtr, DBMS_UPDATE_COMMIT );
			}
		else
			{
			setBoundData( boundDataPtr, 0, subjCertID, subjCertIDlength );
			status = dbmsUpdate( 
				"DELETE FROM certificates WHERE certID = ?",
								 boundDataPtr, DBMS_UPDATE_COMMIT );
			}
		}
	else
		{
		/* Something went wrong, abort the transaction */
		dbmsUpdate( NULL, NULL, DBMS_UPDATE_ABORT );
		}
	if( iCertificate == CRYPT_UNUSED )
		{
		/* If we created the necessary objects locally rather than having 
		   them passed in by the caller we have to clean them up again 
		   before we exit */
		krnlSendNotifier( iLocalCertificate, IMESSAGE_DECREFCOUNT );
		krnlSendNotifier( iLocalCRL, IMESSAGE_DECREFCOUNT );
		}
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );

	/* The operation failed, record the details */
	updateCertErrorLog( dbmsInfo, status,
						( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE ) ? \
						"Certificate issue reversal operation failed, "
							"performing straight delete" : \
						( action == CRYPT_CERTACTION_REVOKE_CERT && \
						  iCertificate != CRYPT_UNUSED ) ? \
						"Revocation of certificate to be replaced failed, "
							"performing straight delete" :
						"Certificate revocation operation failed",
						NULL, 0, reqCertIDptr, reqCertIDlength, NULL, 0, 
						NULL, 0 );

	/* If it was a user-initiated operation we can't try and clean up the 
	   operation internally */
	if( reqPresent )
		{
		retExtErr( status, 
					   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
						 ( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE ) ? \
							"Certificate issue reversal operation failed: " : \
						 ( action == CRYPT_CERTACTION_REVOKE_CERT && \
						   iCertificate != CRYPT_UNUSED ) ? \
							"Revocation of certificate to be replaced "
								"failed: " :
							"Certificate revocation operation failed: " ) );
		}

	REQUIRES( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE || \
			  action == CRYPT_CERTACTION_REVOKE_CERT );

	/* It was a direct revocation done invisibly as part of an internal 
	   certificate management operation, try again with a straight delete.
	   "Where I come from Sherpa Tenzing there's no such word as 'asambhav 
	   chha'" */
	initBoundData( boundDataPtr );
	if( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE )
		{
		setBoundData( boundDataPtr, 0, specialCertID, specialCertIDlength );
		status = dbmsUpdate( 
					"DELETE FROM certificates WHERE certID = ?",
							 boundDataPtr, DBMS_UPDATE_NORMAL );
		}
	else
		{
		setBoundData( boundDataPtr, 0, subjCertID, subjCertIDlength );
		status = dbmsUpdate( 
					"DELETE FROM certificates WHERE certID = ?",
							 boundDataPtr, DBMS_UPDATE_NORMAL );
		}
	if( cryptStatusOK( status ) )
		{
		/* The fallback straight delete succeeded, exit with a warning */
		retExt( CRYPT_OK, 
				( CRYPT_OK, errorInfo, 
				  "Warning: Direct certificate revocation operation failed, "
				  "revocation was handled via straight delete" ) );
		}

	/* The fallback failed as well, we've run out of options */
	updateCertErrorLogMsg( dbmsInfo, status, 
						   "Fallback straight delete failed" );
	retExtErr( status, 
			   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
				 ( action == CRYPT_CERTACTION_CERT_CREATION_REVERSE ) ? \
					"Certificate issue reversal operation failed: " : \
				 ( action == CRYPT_CERTACTION_REVOKE_CERT && \
				   iCertificate != CRYPT_UNUSED ) ? \
					"Revocation of certificate to be replaced failed: " :
					"Certificate revocation operation failed: " ) );
	}

/* Create a CRL from revocation entries in the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int caIssueCRL( INOUT DBMS_INFO *dbmsInfo, 
				OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCRL,
				IN_HANDLE const CRYPT_CONTEXT caKey, 
				INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	BYTE crlEntry[ MAX_QUERY_RESULT_SIZE + 8 ];
	BOOLEAN crlEntryAdded = FALSE;
	char crlEntryBuffer[ MAX_QUERY_RESULT_SIZE + 8 ];
	void *crlEntryPtr = hasBinaryBlobs( dbmsInfo ) ? \
						crlEntry : ( void * ) crlEntryBuffer;
						/* Cast needed for gcc */
	char nameID[ ENCODED_DBXKEYID_SIZE + 8 ];
	char *operationString = "No error";
	int operationStatus = CRYPT_OK, nameIDlength;
	int errorCount, iterationCount, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( iCryptCRL, sizeof( CRYPT_CERTIFICATE ) ) );
	
	REQUIRES( isHandleRangeValid( caKey ) );
	REQUIRES( errorInfo != NULL );

	/* Extract the information that we need to build the CRL from the CA 
	   certificate */
	status = getKeyID( nameID, ENCODED_DBXKEYID_SIZE, &nameIDlength, 
					   caKey, CRYPT_IATTRIBUTE_SUBJECT );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the CRL object to hold the entries */
	setMessageCreateObjectInfo( &createInfo, CRYPT_CERTTYPE_CRL );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );

	/* Submit a query to fetch every CRL entry for this CA.  We don't have
	   to do a date check since the presence of revocation entries for
	   expired certificates is controlled by whether the CA's policy 
	   involves removing entries for expired certificates or not */
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, nameID, nameIDlength );
	status = dbmsQuery(
		"SELECT certData FROM CRLs WHERE nameID = ?",
						NULL, 0, NULL, boundDataPtr, 
						DBMS_CACHEDQUERY_NONE, DBMS_QUERY_START );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Couldn't initiate CRL data fetch from certificate "
					 "store: " ) );
		}

	/* Rumble through the certificate store fetching every entry and adding 
	   it to the CRL.  We only stop once we've run out of entries or we hit 
	   too many errors which ensures that some minor error at some point 
	   won't prevent the CRL from being issued, however if there was a 
	   problem somewhere we create a log entry to record it */
	for( errorCount = 0, iterationCount = 0;
		 status != CRYPT_ERROR_COMPLETE && \
			errorCount < FAILSAFE_ITERATIONS_SMALL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 iterationCount++ )
		{
		MESSAGE_DATA msgData;
		int crlEntryLength;

		/* Read the CRL entry data */
		status = dbmsQuery( NULL, crlEntryPtr, MAX_QUERY_RESULT_SIZE, 
							&crlEntryLength, NULL, DBMS_CACHEDQUERY_NONE, 
							DBMS_QUERY_CONTINUE );
		if( status == CRYPT_ERROR_COMPLETE )
			{
			/* We've got all the entries, complete the query and exit */
			dbmsStaticQuery( NULL, DBMS_CACHEDQUERY_NONE,
							 DBMS_QUERY_CANCEL );
			break;
			}
		if( cryptStatusOK( status ) && !hasBinaryBlobs( dbmsInfo ) )
			{
			status = base64decode( crlEntry, MAX_CERT_SIZE, &crlEntryLength,
								   crlEntryBuffer, crlEntryLength,
								   CRYPT_CERTFORMAT_NONE );
			}
		if( cryptStatusError( status ) )
			{
			/* Remember the error details for later if necessary */
			if( cryptStatusOK( operationStatus ) )
				{
				operationStatus = status;
				operationString = "Some CRL entries couldn't be read from "
								  "the certificate store";
				}
			errorCount++;
			continue;
			}

		/* Add the entry to the CRL */
		setMessageData( &msgData, crlEntry, crlEntryLength );
		status = krnlSendMessage( createInfo.cryptHandle,
								  IMESSAGE_SETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_CRLENTRY );
		if( cryptStatusError( status ) )
			{
			/* Remember the error details for later if necessary */
			if( cryptStatusOK( operationStatus ) )
				{
				operationStatus = status;
				operationString = "Some CRL entries couldn't be added to "
								  "the CRL";
				}
			errorCount++;
			continue;
			}

		crlEntryAdded = TRUE;
		}
	if( errorCount >= FAILSAFE_ITERATIONS_SMALL || \
		iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		{
		/* It's hard to tell what type of error an iterationCount-exceeded
		   situation really is, in theory it's a software error (either in
		   cryptlib's fetch logic or in the dabatase) but because of the
		   practice of creating mega-CRLs with thousands of entries it's
		   entirely possible that this is "normal", or at least "normal" in
		   the alternative reality of X.509. Because of this we don't flag
		   it as an internal error but simply warn in the debug build, 
		   although we do bail out rather than trying to construct some
		   monster CRL */
		DEBUG_DIAG(( "CRL-entry-fetch loop detected" ));
		assert( DEBUG_WARN );
		}
	if( cryptStatusError( operationStatus ) )
		{
		/* If nothing could be added to the CRL something is wrong, don't 
		   try and continue */
		if( !crlEntryAdded )
			{
			updateCertErrorLogMsg( dbmsInfo, operationStatus, 
								   "No CRL entries could be added to the "
								   "CRL" );
			retExt( operationStatus, 
					( operationStatus, errorInfo, 
					  "No CRL entries could be added to the CRL" ) );
			}

		/* At least some entries could be added to the CRL, record that there
		   was a problem but continue */
		updateCertErrorLogMsg( dbmsInfo, operationStatus, operationString );
		}

	/* We've got all the CRL entries, sign the CRL and return it to the
	   caller */
	status = krnlSendMessage( createInfo.cryptHandle, IMESSAGE_CRT_SIGN,
							  NULL, caKey );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ARGERROR_VALUE )
			status = CAMGMT_ARGERROR_CAKEY;	/* Map to correct error code */
		krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
		updateCertErrorLogMsg( dbmsInfo, operationStatus,
							   "CRL creation failed" );
		retExtArg( status, 
				   ( status, errorInfo, 
					 "Couldn't sign CRL to be issued" ) );
		}
	*iCryptCRL = createInfo.cryptHandle;
	updateCertLog( dbmsInfo, CRYPT_CERTACTION_ISSUE_CRL, 
				   NULL, 0, NULL, 0, NULL, 0, NULL, 0, DBMS_UPDATE_NORMAL );
	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
