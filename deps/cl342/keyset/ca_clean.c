/****************************************************************************
*																			*
*					  cryptlib DBMS CA Cleanup Interface					*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

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

/* When we iterate through the entries in the certificate store we have to 
   protect ourselves against repeatedly fetching the same value over and 
   over again if there's a problem with the certificate store, which we do 
   by remembering data from the previous certificate that we fetched.  The 
   following value defines the amount of certificate data that we record 
   (since this doesn't protect against Byzantine failures, we also maintain
   an iteration counter that bails out after a certain number of iterations
   are exceeded) */

#define MAX_PREVCERT_DATA	128

#ifdef USE_DBMS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Delete a certificate request or partially-issued certificated if a 
   cleanup operation for it failed, recording the failure details in the 
   log */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int deleteIncompleteRequest( INOUT DBMS_INFO *dbmsInfo,
									IN_BUFFER( reqCertIDlength ) \
										const char *reqCertID, 
									IN_LENGTH_SHORT_Z const int reqCertIDlength, 
									IN_ERROR const int errorStatus,
									IN_STRING const char *reasonMessage )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	int status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isReadPtr( reqCertID, reqCertIDlength ) );

	REQUIRES( reqCertIDlength > 0 && reqCertIDlength < MAX_INTLENGTH_SHORT );
	REQUIRES( cryptStatusError( errorStatus ) );

	/* Delete the request and record the cause */
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, reqCertID, reqCertIDlength );
	status = dbmsUpdate( 
				"DELETE FROM certRequests WHERE certID = ?",
								 boundDataPtr, DBMS_UPDATE_NORMAL );
	updateCertErrorLog( dbmsInfo, errorStatus, reasonMessage, NULL, 0, 
						NULL, 0, reqCertID, reqCertIDlength, NULL, 0 );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int deleteIncompleteCert( INOUT DBMS_INFO *dbmsInfo,
								 const BOOLEAN isRenewal,
								 IN_ERROR const int errorStatus,
								 IN_STRING const char *reasonMessage )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	BYTE certID[ MAX_QUERY_RESULT_SIZE + 8 ];
	int certIDlength, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( cryptStatusError( errorStatus ) );

	/* Get the certID for the incomplete certificate to delete */
	status = dbmsQuery( isRenewal ? \
				"SELECT certID FROM certificates WHERE keyID LIKE '" KEYID_ESC2 "%'" : \
				"SELECT certID FROM certificates WHERE keyID LIKE '" KEYID_ESC1 "%'",
						certID, MAX_QUERY_RESULT_SIZE, &certIDlength, NULL, 
						DBMS_CACHEDQUERY_NONE, DBMS_QUERY_NORMAL );
	if( cryptStatusError( status ) )
		return( status );

	/* Delete the request and record the cause */
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, certID, certIDlength );
	status = dbmsUpdate( 
				"DELETE FROM certificates WHERE certID = ?",
								 boundDataPtr, DBMS_UPDATE_NORMAL );
	updateCertErrorLog( dbmsInfo, errorStatus, reasonMessage, NULL, 0, 
						NULL, 0, certID, certIDlength, NULL, 0 );
	return( status );
	}

/* Get a partially-issued certificate.  We have to perform the import
   ourselves since it's marked as an incompletely-issued certificate and so 
   is invisible to accesses performed via the standard certificate fetch 
   routines */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getNextPartialCert( INOUT DBMS_INFO *dbmsInfo,
							   OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
							   INOUT_BUFFER_FIXED( prevCertDataMaxLen ) \
								BYTE *prevCertData, 
							   IN_LENGTH_SHORT const int prevCertDataMaxLen, 
							   const BOOLEAN isRenewal )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	BYTE certificate[ MAX_QUERY_RESULT_SIZE + 8 ];
	char encodedCertData[ MAX_QUERY_RESULT_SIZE + 8 ];
	void *certPtr = hasBinaryBlobs( dbmsInfo ) ? \
					( void * ) certificate : encodedCertData;
	int certSize, status;			/* Cast needed for gcc */

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( prevCertData, prevCertDataMaxLen ) );

	REQUIRES( prevCertDataMaxLen > 0 && \
			  prevCertDataMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* Find the next certificate and import it.  Although this would appear 
	   to be fetching the same certificate over and over again, the caller 
	   will be deleting the currently-fetched certificate after we return it 
	   to them so in practice it fetches a new certificate each time (we
	   also perform a loop check on the prevCertData once we've fetched the
	   next certificate so we'll detect any looping fairly quickly) */
	status = dbmsQuery( isRenewal ? \
				"SELECT certData FROM certificates WHERE keyID LIKE '" KEYID_ESC2 "%'" : \
				"SELECT certData FROM certificates WHERE keyID LIKE '" KEYID_ESC1 "%'",
						certPtr, MAX_QUERY_RESULT_SIZE, &certSize, NULL, 
						DBMS_CACHEDQUERY_NONE, DBMS_QUERY_NORMAL );
	if( cryptStatusError( status ) )
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );

		/* We've processed all of the entries, this isn't an error */
		resetErrorInfo( dbmsInfo );
		return( OK_SPECIAL );
		}
	if( !hasBinaryBlobs( dbmsInfo ) )
		{
		status = base64decode( certificate, MAX_CERT_SIZE, &certSize,
							   encodedCertData, certSize, 
							   CRYPT_CERTFORMAT_NONE );
		if( cryptStatusError( status ) )
			{
			DEBUG_DIAG(( "Couldn't base64-decode data" ));
			assert( DEBUG_WARN );
			return( status );
			}
		}
	ENSURES( certSize > 0 && certSize <= MAX_CERT_SIZE );

	/* If we're stuck in a loop fetching the same value over and over, make
	   an emergency exit */
	if( !memcmp( prevCertData, certificate, \
				 min( certSize, prevCertDataMaxLen ) ) )
		{
		DEBUG_DIAG(( "Certificate-fetch loop detected" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_DUPLICATE );
		}
	memcpy( prevCertData, certificate, 
			min( certSize, prevCertDataMaxLen ) );

	/* Reset the first byte of the certificate data from the not-present 
	   magic value to allow it to be imported and create a certificate from 
	   it */
	certificate[ 0 ] = BER_SEQUENCE;
	setMessageCreateObjectIndirectInfo( &createInfo, certificate, certSize,
										CRYPT_CERTTYPE_CERTIFICATE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusOK( status ) )
		*iCertificate = createInfo.cryptHandle;
	return( status );
	}

/****************************************************************************
*																			*
*								CA Cleanup Functions						*
*																			*
****************************************************************************/

/* Perform a cleanup operation on the certificate store, removing 
   incomplete, expired, and otherwise leftover certificates.  Since we're 
   cleaning up the certificate store we try and continue even if an error 
   occurs, at least up to a limit.  In addition to protect against Byzantine
   failures we include an iteration counter and bail out after a certain 
   number of iterations.  This is a tradeoff between DoS protection and
   bailing out too early, using FAILSAFE_ITERATIONS_LARGE is a reasonable
   compromise */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int caCleanup( INOUT DBMS_INFO *dbmsInfo, 
			   IN_ENUM( CRYPT_CERTACTION ) const CRYPT_CERTACTION_TYPE action,
			   INOUT ERROR_INFO *errorInfo )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	BYTE prevCertData[ MAX_PREVCERT_DATA + 8 ];
	BYTE certID[ MAX_QUERY_RESULT_SIZE + 8 ];
	const time_t currentTime = getTime();
	int certIDlength, errorCount, iterationCount, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( action == CRYPT_CERTACTION_EXPIRE_CERT || \
			  action == CRYPT_CERTACTION_CLEANUP );
	REQUIRES( errorInfo != NULL );

	/* If the time is screwed up we can't perform time-based cleanup
	   actions */
	if( action == CRYPT_CERTACTION_EXPIRE_CERT && \
		currentTime <= MIN_TIME_VALUE )
		retIntError();

	/* Rumble through the certificate store either deleting leftover 
	   requests or expiring every certificate which is no longer current */
	memset( prevCertData, 0, MAX_PREVCERT_DATA );
	for( status = CRYPT_OK, errorCount = 0, iterationCount = 0;
		 status != CRYPT_ERROR_NOTFOUND && \
			errorCount < FAILSAFE_ITERATIONS_SMALL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		/* Find the certificate ID of the next expired certificate or next 
		   certificate request (revocation requests are handled later by 
		   completing the revocation).  Note that the select requires that 
		   the database glue code be capable of returning a single result 
		   and then finishing the query, for some back-ends there may be a 
		   need to explicitly cancel the query after the first result is 
		   returned if the database returns an entire result set */
		if( action == CRYPT_CERTACTION_EXPIRE_CERT )
			{
			initBoundData( boundDataPtr );
			setBoundDataDate( boundDataPtr, 0, &currentTime );
			status = dbmsQuery(
						"SELECT certID FROM certificates WHERE validTo < ?",
								certID, MAX_QUERY_RESULT_SIZE, &certIDlength, 
								boundDataPtr, DBMS_CACHEDQUERY_NONE, 
								DBMS_QUERY_NORMAL );
			}
		else
			{
			status = dbmsQuery(
						"SELECT certID FROM certRequests WHERE type = "
							TEXT_CERTTYPE_REQUEST_CERT,
								certID, MAX_QUERY_RESULT_SIZE, &certIDlength, 
								NULL, DBMS_CACHEDQUERY_NONE, 
								DBMS_QUERY_NORMAL );
			}
		if( cryptStatusError( status ) )
			{
			/* If we've processed all of the entries this isn't an error */
			if( status == CRYPT_ERROR_NOTFOUND )
				resetErrorInfo( dbmsInfo );
			else
				errorCount++;
			continue;
			}
		if( certIDlength > MAX_PREVCERT_DATA )
			{
			DEBUG_DIAG(( "Certificate ID data too large" ));
			assert( DEBUG_WARN );
			certIDlength = MAX_PREVCERT_DATA;
			}
		if( !memcmp( prevCertData, certID, certIDlength ) )
			{
			/* We're stuck in a loop fetching the same value over and over,
			   make an emergency exit */
			DEBUG_DIAG(( "Certificate-fetch loop detected" ));
			assert( DEBUG_WARN );
			break;
			}
		memcpy( prevCertData, certID, certIDlength );

		/* Clean up/expire the certificate.  Since CRYPT_CERTACTION_CLEANUP 
		   is a composite action that encompasses a whole series of 
		   operations we replace it with a more specific action code */
		status = updateCertLog( dbmsInfo,
								( action == CRYPT_CERTACTION_CLEANUP ) ? \
								CRYPT_CERTACTION_RESTART_CLEANUP : action,
								NULL, 0, NULL, 0, certID, certIDlength, 
								NULL, 0, DBMS_UPDATE_BEGIN );
		if( cryptStatusOK( status ) )
			{
			initBoundData( boundDataPtr );
			setBoundData( boundDataPtr, 0, certID, certIDlength );
			status = dbmsUpdate( ( action == CRYPT_CERTACTION_EXPIRE_CERT ) ? \
				"DELETE FROM certificates WHERE certID = ?" : \
				"DELETE FROM certRequests WHERE certID = ?",
								 boundDataPtr, DBMS_UPDATE_COMMIT );
			}
		else
			{
			/* Something went wrong, abort the transaction */
			dbmsUpdate( NULL, NULL, DBMS_UPDATE_ABORT );
			errorCount++;
			}
		}
	if( errorCount >= FAILSAFE_ITERATIONS_SMALL || \
		iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		{
		/* It's hard to tell what type of error an iterationCount-exceeded
		   situation really is, in theory it's a software error (either in
		   cryptlib's fetch logic or in the dabatase) but because we could be
		   cleaning up a large number of entries (for example if it's 
		   cleaning up the result of an app going into and endless loop and
		   requesting a large number of certificates) it's entirely possible 
		   that this is isn't an abnormal situation. Because of this we 
		   don't flag it as an internal error but simply warn in the debug 
		   build, although we do bail out after a fixed limit */
		DEBUG_DIAG(( "Certificate-fetch loop detected" ));
		assert( DEBUG_WARN );
		}

	/* If we ran into a problem, perform a fallback general delete of
	   entries that caused the problem */
	if( status != CRYPT_ERROR_NOTFOUND )
		{
		if( action == CRYPT_CERTACTION_EXPIRE_CERT )
			{
			updateCertErrorLogMsg( dbmsInfo, status, "Expire operation "
								   "failed, performing fallback straight "
								   "delete" );
			initBoundData( boundDataPtr );
			setBoundDataDate( boundDataPtr, 0, &currentTime );
			status = dbmsUpdate(
						"DELETE FROM certificates WHERE validTo < ?",
								 boundDataPtr, DBMS_UPDATE_NORMAL );
			}
		else
			{
			updateCertErrorLogMsg( dbmsInfo, status, "Certificate request "
								   "cleanup operation failed, performing "
								   "fallback straight delete" );
			status = dbmsStaticUpdate(
						"DELETE FROM certRequests WHERE type = "
							TEXT_CERTTYPE_REQUEST_CERT );
			}
		if( cryptStatusError( status ) )
			updateCertErrorLogMsg( dbmsInfo, status, "Fallback straight "
								   "delete failed" );
		}

	/* If it's an expiry action we've done the expired certificates, now 
	   remove any stale CRL entries and exit.  If there are no CRL entries 
	   in the expiry period this isn't an error, so we remap the error code 
	   if necessary */
	if( action == CRYPT_CERTACTION_EXPIRE_CERT )
		{
		initBoundData( boundDataPtr );
		setBoundDataDate( boundDataPtr, 0, &currentTime );
		status = dbmsUpdate(
					"DELETE FROM CRLs WHERE expiryDate < ?",
							 boundDataPtr, DBMS_UPDATE_NORMAL );
		if( status == CRYPT_ERROR_NOTFOUND )
			{
			resetErrorInfo( dbmsInfo );
			return( CRYPT_OK );
			}
		if( cryptStatusError( status ) )
			{
			retExtErr( status, 
					   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
						 "Couldn't delete stale CRL entries from "
						 "certificate store: " ) );
			}
		return( CRYPT_OK );
		}

	/* It's a restart, process any incompletely-issued certificates in the
	   same manner as the expiry/cleanup is handled.  Since we don't know at
	   what stage the issue process was interrupted we have to make a worst-
	   case assumption and do a full reversal as a compensating transaction
	   for an aborted certificate issue */
	memset( prevCertData, 0, MAX_PREVCERT_DATA );
	for( status = CRYPT_OK, errorCount = 0, iterationCount = 0;
		 status != CRYPT_ERROR_NOTFOUND && \
			errorCount < FAILSAFE_ITERATIONS_SMALL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		CRYPT_CERTIFICATE iCertificate;

		/* Get the next partially-issued certificate */
		status = getNextPartialCert( dbmsInfo, &iCertificate, 
									 prevCertData, MAX_PREVCERT_DATA, FALSE );
		if( cryptStatusError( status ) )
			{
			/* If we've processed all of the entries, we're done */
			if( status == OK_SPECIAL )
				break;

			/* If we're stuck in a loop fetching the same value over and 
			   over, make an emergency exit */
			if( status == CRYPT_ERROR_DUPLICATE )
				{
				DEBUG_DIAG(( "Certificate-fetch loop detected" ));
				assert( DEBUG_WARN );
				break;
				}

			/* It's some other type of problem, clear it and continue */
			status = deleteIncompleteCert( dbmsInfo, FALSE, status, 
										   "Couldn't get partially-issued "
										   "certificate to complete issue, "
										   "deleting and continuing" );
			errorCount++;
			continue;
			}

		/* We found a certificate to revoke, complete the revocation */
		status = revokeCertDirect( dbmsInfo, iCertificate,
								   CRYPT_CERTACTION_CERT_CREATION_REVERSE,
								   errorInfo );
		krnlSendNotifier( iCertificate, IMESSAGE_DECREFCOUNT );
		}
	if( errorCount >= FAILSAFE_ITERATIONS_SMALL || \
		iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		{
		/* See note with earlier code */
		DEBUG_DIAG(( "Certificate-fetch loop detected" ));
		assert( DEBUG_WARN );
		}

	/* If we ran into a problem, perform a fallback general delete of
	   entries that caused the problem */
	if( status != CRYPT_ERROR_NOTFOUND )
		{
		updateCertErrorLogMsg( dbmsInfo, status, "Partially-issued "
							   "certificate cleanup operation failed, "
							   "performing fallback straight delete" );
		status = dbmsStaticUpdate(
			"DELETE FROM certificates WHERE keyID LIKE '" KEYID_ESC1 "%'" );
		if( cryptStatusError( status ) )
			updateCertErrorLogMsg( dbmsInfo, status, "Fallback straight "
								   "delete failed" );
		}

	/* Now process any partially-completed renewals */
	memset( prevCertData, 0, MAX_PREVCERT_DATA );
	for( status = CRYPT_OK, errorCount = 0, iterationCount = 0;
		 status != CRYPT_ERROR_NOTFOUND && \
			errorCount < FAILSAFE_ITERATIONS_SMALL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		CRYPT_CERTIFICATE iCertificate;

		/* Get the next partially-completed certificate */
		status = getNextPartialCert( dbmsInfo, &iCertificate, 
									 prevCertData, MAX_PREVCERT_DATA, TRUE );
		if( cryptStatusError( status ) )
			{
			/* If we've processed all of the entries, we're done */
			if( status == OK_SPECIAL )
				break;

			/* If we're stuck in a loop fetching the same value over and 
			   over, make an emergency exit */
			if( status == CRYPT_ERROR_DUPLICATE )
				{
				DEBUG_DIAG(( "Certificate-fetch loop detected" ));
				assert( DEBUG_WARN );
				break;
				}

			/* It's some other type of problem, clear it and continue */
			status = deleteIncompleteCert( dbmsInfo, TRUE, status, 
										   "Couldn't get partially-renewed "
										   "certificate to complete renewal, "
										   "deleting and continuing" );
			errorCount++;
			continue;
			}

		/* We found a partially-completed certificate, complete the renewal */
		status = completeCertRenewal( dbmsInfo, iCertificate, errorInfo );
		krnlSendNotifier( iCertificate, IMESSAGE_DECREFCOUNT );
		}
	if( errorCount >= FAILSAFE_ITERATIONS_SMALL || \
		iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		{
		/* See note with earlier code */
		DEBUG_DIAG(( "Certificate-fetch loop detected" ));
		assert( DEBUG_WARN );
		}

	/* Finally, process any pending revocations */
	memset( prevCertData, 0, MAX_PREVCERT_DATA );
	for( status = CRYPT_OK, errorCount = 0, iterationCount = 0;
		 status != CRYPT_ERROR_NOTFOUND && \
			errorCount < FAILSAFE_ITERATIONS_SMALL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 iterationCount++ )
		{
		CRYPT_CERTIFICATE iCertRequest;
		int dummy;

		/* Find the next revocation request and import it.  This is slightly
		   ugly since we could grab it directly by fetching the data based on
		   the request type field, but there's no way to easily get to the
		   low-level import functions from here so we have to first fetch the
		   certificate ID and then pass that down to the lower-level 
		   functions to fetch the actual request */
		status = dbmsQuery(
					"SELECT certID FROM certRequests WHERE type = "
						TEXT_CERTTYPE_REQUEST_REVOCATION,
							certID, MAX_QUERY_RESULT_SIZE, &certIDlength, 
							NULL, DBMS_CACHEDQUERY_NONE, 
							DBMS_QUERY_NORMAL );
		if( cryptStatusError( status ) )
			{
			/* If we've processed all of the entries this isn't an error */
			if( status == CRYPT_ERROR_NOTFOUND )
				{
				resetErrorInfo( dbmsInfo );
				break;
				}

			errorCount++;
			continue;
			}
		if( certIDlength > MAX_PREVCERT_DATA )
			{
			DEBUG_DIAG(( "Certificate ID data too large" ));
			assert( DEBUG_WARN );
			certIDlength = MAX_PREVCERT_DATA;
			}
		if( !memcmp( prevCertData, certID, certIDlength ) )
			{
			/* We're stuck in a loop fetching the same value over and over,
			   make an emergency exit */
			DEBUG_DIAG(( "Certificate-fetch loop detected" ));
			assert( DEBUG_WARN );
			break;
			}
		memcpy( prevCertData, certID, certIDlength );
		status = getItemData( dbmsInfo, &iCertRequest, &dummy, 
							  KEYMGMT_ITEM_REVREQUEST, CRYPT_IKEYID_CERTID, 
							  certID, certIDlength, KEYMGMT_FLAG_NONE, 
							  errorInfo );
		if( cryptStatusError( status ) )
			{
			status = \
				deleteIncompleteRequest( dbmsInfo, certID, certIDlength, status, 
										 "Couldn't instantiate revocation "
										 "request from stored data, deleting "
										 "request and continuing" );
			errorCount++;
			continue;
			}

		/* Complete the revocation */
		status = caRevokeCert( dbmsInfo, iCertRequest, CRYPT_UNUSED,
							   CRYPT_CERTACTION_RESTART_REVOKE_CERT,
							   errorInfo );
		if( cryptStatusError( status ) )
			{
			/* If we get a not-found error this is an allowable condition 
			   since the certificate may have expired or been otherwise 
			   removed after the revocation request was received, so we 
			   just delete the entry */
			status = \
				deleteIncompleteRequest( dbmsInfo, certID, certIDlength, status, 
										 ( status == CRYPT_ERROR_NOTFOUND ) ? \
										 "Deleted revocation request for "
										 "non-present certificate" : \
										 "Couldn't revoke certificate from "
										 "revocation request, deleting "
										 "request and continuing" );
			}
		krnlSendNotifier( iCertRequest, IMESSAGE_DECREFCOUNT );
		}
	if( errorCount >= FAILSAFE_ITERATIONS_SMALL || \
		iterationCount >= FAILSAFE_ITERATIONS_LARGE )
		{
		/* See note with earlier code */
		DEBUG_DIAG(( "Certificate-fetch loop detected" ));
		assert( DEBUG_WARN );
		}

	/* If we ran into a problem, perform a fallback general delete of
	   entries that caused the problem */
	if( status != CRYPT_ERROR_NOTFOUND )
		{
		updateCertErrorLogMsg( dbmsInfo, status, "Revocation request "
							   "cleanup operation failed, performing "
							   "fallback straight delete" );
		status = dbmsStaticUpdate(
					"DELETE FROM certRequests WHERE type = "
						TEXT_CERTTYPE_REQUEST_REVOCATION );
		if( cryptStatusError( status ) )
			{
			updateCertErrorLogMsg( dbmsInfo, status, "Fallback straight "
								   "delete failed" );
			retExtErr( status, 
					   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
						 "Revocation request cleanup operation failed: " ) );
			}
		}

	return( resetErrorInfo( dbmsInfo ) );
	}
#endif /* USE_DBMS */
