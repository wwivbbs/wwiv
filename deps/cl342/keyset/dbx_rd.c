/****************************************************************************
*																			*
*							cryptlib DBMS Interface							*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "dbms.h"
  #include "keyset.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "keyset/dbms.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_DBMS

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* The most common query types can be performed using cached access plans 
   and query data.  The following function determines whether a particular
   query can be performed using cached information, returning the cache 
   entry for the query if so */

CHECK_RETVAL_ENUM( DBMS_CACHEDQUERY ) \
static DBMS_CACHEDQUERY_TYPE getCachedQueryType( IN_ENUM_OPT( KEYMGMT_ITEM ) \
													const KEYMGMT_ITEM_TYPE itemType,
												 IN_KEYID_OPT \
													const CRYPT_KEYID_TYPE keyIDtype )
	{
	REQUIRES_EXT( ( itemType == KEYMGMT_ITEM_NONE || \
					itemType == KEYMGMT_ITEM_REQUEST || \
					itemType == KEYMGMT_ITEM_REVREQUEST || \
					itemType == KEYMGMT_ITEM_PKIUSER || \
					itemType == KEYMGMT_ITEM_PUBLICKEY || \
					itemType == KEYMGMT_ITEM_REVOCATIONINFO ), 
				  DBMS_CACHEDQUERY_NONE );
				  /* KEYMGMT_ITEM_NONE is for ongoing queries */
	REQUIRES_EXT( ( itemType == KEYMGMT_ITEM_NONE && \
					keyIDtype == CRYPT_KEYID_NONE ) || \
				  ( keyIDtype > CRYPT_KEYID_NONE && \
					keyIDtype < CRYPT_KEYID_LAST ), DBMS_CACHEDQUERY_NONE );
				  /* { KEYMGMT_ITEM_NONE, CRYPT_KEYID_NONE } is for ongoing 
				     queries */

	/* If we're not reading from the standard certificates table the query 
	   won't be cached */
	if( itemType != KEYMGMT_ITEM_PUBLICKEY )
		return( DBMS_CACHEDQUERY_NONE );

	/* Check whether we're querying on a cacheable key value type */
	switch( keyIDtype )
		{
		case CRYPT_KEYID_URI:
			return( DBMS_CACHEDQUERY_URI );

		case CRYPT_IKEYID_ISSUERID:
			return( DBMS_CACHEDQUERY_ISSUERID );

		case CRYPT_IKEYID_CERTID:
			return( DBMS_CACHEDQUERY_CERTID );

		case CRYPT_IKEYID_SUBJECTID:
			return( DBMS_CACHEDQUERY_NAMEID );
		}

	return( DBMS_CACHEDQUERY_NONE );
	}

/* Get the SQL string to fetch data from a given table */

CHECK_RETVAL_PTR \
static char *getSelectString( IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType )
	{
	REQUIRES_N( itemType == KEYMGMT_ITEM_REQUEST || \
				itemType == KEYMGMT_ITEM_REVREQUEST || \
				itemType == KEYMGMT_ITEM_PKIUSER || \
				itemType == KEYMGMT_ITEM_PUBLICKEY || \
				itemType == KEYMGMT_ITEM_REVOCATIONINFO );

	switch( itemType )
		{
		case KEYMGMT_ITEM_REQUEST:
		case KEYMGMT_ITEM_REVREQUEST:
			return( "SELECT certData FROM certRequests WHERE " );

		case KEYMGMT_ITEM_PKIUSER:
			return( "SELECT certData FROM pkiUsers WHERE " );

		case KEYMGMT_ITEM_PUBLICKEY:
			return( "SELECT certData FROM certificates WHERE " );

		case KEYMGMT_ITEM_REVOCATIONINFO:
			return( "SELECT certData FROM CRLs WHERE " );
		}

	retIntError_Null();
	}

/* Check an encoded certificate for a matching key usage.  The semantics of 
   key usage flags are vague in the sense that the query "Is this key valid 
   for X" is easily resolved but the query "Which key is appropriate for X" 
   is NP-hard due to the potential existence of unbounded numbers of
   certificates with usage semantics expressed in an arbitrary number of
   ways.  For now we distinguish between signing and encryption keys (this,
   at least, is feasible) by doing a quick check for keyUsage if we get
   multiple certificates with the same DN and choosing the one with the 
   appropriate key usage.

   Rather than performing a relatively expensive certificate import for each 
   certificate we find the keyUsage by doing an optimised search through the 
   certificate data for its encoded form.  The pattern that we look for is:

	OID				06 03 55 1D 0F + SEQUENCE at n-2
	BOOLEAN			(optional)
	OCTET STRING {
		BIT STRING	(value)

   Note that this function isn't security-critical because it doesn't perform
   an actual usage check, all it needs to do is find the key that's most 
   likely to be usable for purpose X.  If it gets it wrong the certificate-
   level checking will catch the problem */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkCertUsage( IN_BUFFER( certLength ) const BYTE *certificate, 
							   IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
								const int certLength,
							   IN_FLAGS( KEYMGMT ) const int requestedUsage )
	{
	const int keyUsageMask = ( requestedUsage & KEYMGMT_FLAG_USAGE_CRYPT ) ? \
							 CRYPT_KEYUSAGE_KEYENCIPHERMENT : KEYUSAGE_SIGN;
	int i;

	assert( isReadPtr( certificate, certLength ) );
	
	REQUIRES( certLength >= MIN_CRYPT_OBJECTSIZE && \
			  certLength < MAX_INTLENGTH_SHORT );
	REQUIRES( requestedUsage > KEYMGMT_FLAG_NONE && \
			  requestedUsage < KEYMGMT_FLAG_MAX );
	REQUIRES( requestedUsage & KEYMGMT_MASK_USAGEOPTIONS );

	/* Scan the payload portion of the certificate for the keyUsage 
	   extension.  The certificate is laid out approximately as:

		[ junk ][ DN ][ times ][ DN ][ pubKey ][ attrs ][ sig ]

	   so we know there's at least 128 + MIN_PKCSIZE bytes at the start and
	   MIN_PKCSIZE bytes at the end that we don't have to bother poking 
	   around in */
	for( i = 128 + MIN_PKCSIZE; i < certLength - MIN_PKCSIZE; i++ )
		{
		STREAM stream;
		int keyUsage, status;

		/* Look for the OID.  This potentially skips two bytes at a time but 
		   this is safe because the preceding bytes can never contain either 
		   of these two values (they're 0x30 + 11...15) */
		if( certificate[ i++ ] != BER_OBJECT_IDENTIFIER || \
			certificate[ i++ ] != 3 )
			continue;
		if( certificate[ i - 4 ] != BER_SEQUENCE )
			continue;
		if( memcmp( certificate + i, "\x55\x1D\x0F", 3 ) )
			continue;
		i += 3;

		/* We've found the OID (with 2.8e-14 error probability), skip
		   the critical flag if necessary */
		if( certificate[ i ] == BER_BOOLEAN )
			i += 3;

		/* Read the OCTET STRING wrapper and BIT STRING */
		sMemConnect( &stream, certificate + i, 
					 certLength - ( i + MIN_PKCSIZE ) );
		readOctetStringHole( &stream, NULL, 4, DEFAULT_TAG );
		status = readBitString( &stream, &keyUsage );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			continue;

		/* Check whether the requested usage is allowed */
		return( ( keyUsage & keyUsageMask ) ? TRUE : FALSE );
		}

	/* No key usage found, assume that any usage is OK */
	return( TRUE );
	}

/* Check that the object that we've fetched actually matches what we asked 
   for.  Obviously this should be the case, but if the underlying data store 
   can lie to us and the caller blindly accepts the certificate and uses it 
   they could end up trying to fetch a certificate belonging to A, being fed 
   one belonging to B, and "verify" information from B believing it to be 
   from A */

CHECK_RETVAL STDC_NONNULL_ARG( ( 4 ) ) \
static int checkObjectIDMatch( IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
							   IN_ENUM_OPT( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType, 
							   IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype, 
							   IN_BUFFER( keyIDlength ) \
									const void *keyID, 
							   IN_LENGTH_KEYID const int keyIDlength )
	{
	BYTE decodedID[ DBXKEYID_SIZE + 8 ];
	int length = keyIDlength, status;

	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REQUEST || \
			  itemType == KEYMGMT_ITEM_REVREQUEST || \
			  itemType == KEYMGMT_ITEM_PKIUSER || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO );
	REQUIRES( keyIDtype > CRYPT_KEYID_NONE && keyIDtype < CRYPT_KEYID_LAST );
	REQUIRES( keyID != NULL && \
			  keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Fetching an item based on its keyID is somewhat complex because the
	   value, which can be of arbitrary length or form, is hashed to create
	   a fixed-size keyID.  Because of this we can't use iCryptVerifyID() 
	   but have to extract the ID from the object in the form that it's used 
	   by the underlying data store and compare that */
	if( itemType == KEYMGMT_ITEM_PUBLICKEY && \
		keyIDtype == CRYPT_IKEYID_KEYID )
		{
		BYTE storedKeyID[ DBXKEYID_SIZE + 8 ];
		int storedKeyIDlength;

		REQUIRES( keyIDlength == ENCODED_DBXKEYID_SIZE );

		status = getCertKeyID( storedKeyID, ENCODED_DBXKEYID_SIZE, 
							   &storedKeyIDlength, iCryptCert );
		if( cryptStatusOK( status ) && \
			!memcmp( keyID, storedKeyID, DBXKEYID_SIZE ) )
			return( CRYPT_OK );

		return( CRYPT_ERROR_INVALID );
		}
	if( itemType == KEYMGMT_ITEM_PKIUSER && \
		keyIDtype == CRYPT_IKEYID_KEYID )
		{
		BYTE storedKeyID[ DBXKEYID_SIZE + 8 ];
		int storedKeyIDlength;

		REQUIRES( keyIDlength == ENCODED_DBXKEYID_SIZE );

		status = getPkiUserKeyID( storedKeyID, ENCODED_DBXKEYID_SIZE, 
								  &storedKeyIDlength, iCryptCert );
		if( cryptStatusOK( status ) && \
			!memcmp( keyID, storedKeyID, DBXKEYID_SIZE ) )
			return( CRYPT_OK );

		return( CRYPT_ERROR_INVALID );
		}

	/* The internal key ID types are binary values and will be base64-
	   encoded in order to be usable with the database back-end, before we 
	   can compare them we have to covert them back into their original 
	   binary form */
	if( keyIDtype >= CRYPT_KEYID_LAST_EXTERNAL )
		{
		status = base64decode( decodedID, DBXKEYID_SIZE, &length, keyID, 
							   keyIDlength, CRYPT_CERTFORMAT_NONE );
		if( cryptStatusError( status ) )
			return( status );
		keyID = decodedID;
		}

	/* Make sure that the ID that we've been passed to fetch an object 
	   matches the ID used in the fetched object */
	return( iCryptVerifyID( iCryptCert, keyIDtype, keyID, length ) );
	}

/****************************************************************************
*																			*
*							Database Fetch Routines							*
*																			*
****************************************************************************/

/* Fetch a sequence of certificates from a data source.  This is called in 
   one of two ways, either indirectly by the certificate code to fetch the 
   first and subsequent certificates in a chain or directly by the user 
   after submitting a query to the keyset (which doesn't return any data) 
   to read the results of the query.  The schema for calls is:

	state = NULL:		query( NULL, &data, CONTINUE );
	state, point query:	query( SQL, &data, NORMAL );
	state, multi-cert:	query( SQL, &data, START );
						( followed by 'query( NULL, &data, CONTINUE )') */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 9 ) ) \
int getItemData( INOUT DBMS_INFO *dbmsInfo, 
				 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
				 OUT_OPT int *stateInfo, 
				 IN_ENUM_OPT( KEYMGMT_ITEM ) const KEYMGMT_ITEM_TYPE itemType, 
				 IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype, 
				 IN_BUFFER_OPT( keyValueLength ) const char *keyValue, 
				 IN_LENGTH_KEYID_Z const int keyValueLength,
				 IN_FLAGS_Z( KEYMGMT ) const int options,
				 INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	const DBMS_CACHEDQUERY_TYPE cachedQueryType = \
								getCachedQueryType( itemType, keyIDtype );
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	BYTE certificate[ MAX_CERT_SIZE + 8 ];
	char sqlBuffer[ MAX_SQL_QUERY_SIZE + 8 ];
	const char *queryString;
	char certDataBuffer[ MAX_QUERY_RESULT_SIZE + 8 ];
	void *certDataPtr = hasBinaryBlobs( dbmsInfo ) ? \
						certificate : ( void * ) certDataBuffer;
						/* Cast needed for gcc */
	DBMS_QUERY_TYPE queryType;
	BOOLEAN multiCertQuery = ( options & KEYMGMT_MASK_USAGEOPTIONS ) ? \
							 TRUE : FALSE;
	int certDataLength DUMMY_INIT, iterationCount, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( ( stateInfo == NULL ) || \
			isWritePtr( stateInfo, sizeof( int ) ) );
	assert( ( keyValueLength > MIN_NAME_LENGTH && \
			  isReadPtr( keyValue, keyValueLength ) && \
			  ( keyIDtype > CRYPT_KEYID_NONE && \
				keyIDtype < CRYPT_KEYID_LAST ) ) || \
			( keyValueLength == 0 && keyValue == NULL && \
			  keyIDtype == CRYPT_KEYID_NONE ) );

	REQUIRES( ( itemType == KEYMGMT_ITEM_NONE && \
				keyIDtype == CRYPT_KEYID_NONE && keyValue == NULL && \
				keyValueLength == 0 && stateInfo == NULL ) ||
			  /* This variant is for ongoing queries, for which information 
			     will have been submitted when the query was started */
			  ( itemType > KEYMGMT_ITEM_NONE && \
			    itemType < KEYMGMT_ITEM_LAST && \
				keyIDtype > CRYPT_KEYID_NONE && \
				keyIDtype < CRYPT_KEYID_LAST && \
				keyValue != NULL && \
				keyValueLength >= MIN_NAME_LENGTH && 
				keyValueLength < MAX_ATTRIBUTE_SIZE && \
				stateInfo != NULL ) );
	REQUIRES( itemType == KEYMGMT_ITEM_NONE || \
			  itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REQUEST || \
			  itemType == KEYMGMT_ITEM_REVREQUEST || \
			  itemType == KEYMGMT_ITEM_PKIUSER || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX );
	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	*iCertificate = CRYPT_ERROR;
	if( stateInfo != NULL )
		*stateInfo = CRYPT_ERROR;

	/* Make sure that we can never explicitly fetch anything with an ID that
	   indicates that it's physically but not logically present, for example
	   certificates that have been created but not fully issued yet, 
	   certificate items that are on hold, and similar items */
	if( keyValue != NULL && keyValueLength >= KEYID_ESC_SIZE && \
		( !memcmp( keyValue, KEYID_ESC1, KEYID_ESC_SIZE ) || \
		  !memcmp( keyValue, KEYID_ESC2, KEYID_ESC_SIZE ) ) )
		{
		/* Eheu, litteras istas reperire non possum */
		return( CRYPT_ERROR_NOTFOUND );
		}

	/* Perform a slight optimisation to eliminate unnecessary multi-
	   certificate queries: If we're querying by certID or issuerID only one 
	   certificate can ever match so there's no need to perform a multi-
	   certificate query even if key usage options are specified */
	if( keyIDtype == CRYPT_IKEYID_ISSUERID || \
		keyIDtype == CRYPT_IKEYID_CERTID )
		multiCertQuery = FALSE;

	/* Set the query to begin the fetch */
	if( stateInfo != NULL )
		{
		const char *keyName = getKeyName( keyIDtype );
		const char *selectString = getSelectString( itemType );

		ENSURES( keyName != NULL && selectString != NULL );
		strlcpy_s( sqlBuffer, MAX_SQL_QUERY_SIZE, selectString );
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, keyName );
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, " = ?" );
		queryString = sqlBuffer;
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, keyValue, keyValueLength );
		queryType = multiCertQuery ? DBMS_QUERY_START : DBMS_QUERY_NORMAL;
		}
	else
		{
		/* It's an ongoing query, just fetch the next set of results */
		queryString = NULL;
		boundDataPtr = NULL;
		queryType = DBMS_QUERY_CONTINUE;
		}

	/* Retrieve the results from the query */
	for( iterationCount = 0; iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		/* Retrieve the certificate data and base64-decode it if necessary */
		status = dbmsQuery( queryString, certDataPtr, 
							hasBinaryBlobs( dbmsInfo ) ? \
								MAX_CERT_SIZE : MAX_SQL_QUERY_SIZE,
							&certDataLength, boundDataPtr, cachedQueryType, 
							queryType );
		if( cryptStatusError( status ) )
			{
			/* Convert the error code to a more appropriate value if
			   necessary */
			return( ( multiCertQuery && ( status == CRYPT_ERROR_COMPLETE ) ) ? \
					CRYPT_ERROR_NOTFOUND : status );
			}
		if( !hasBinaryBlobs( dbmsInfo ) )
			{
			status = base64decode( certificate, MAX_CERT_SIZE, 
								   &certDataLength, certDataBuffer, 
								   certDataLength, CRYPT_CERTFORMAT_NONE );
			if( cryptStatusError( status ) )
				{
				retExt( status, 
						( status, errorInfo, 
						  "Couldn't decode certificate from stored encoded "
						  "certificate data" ) );
				}
			}

		/* We've started the fetch, from now on we're only fetching further
		   results */
		queryString = NULL;
		boundDataPtr = NULL;
		if( queryType == DBMS_QUERY_START )
			queryType = DBMS_QUERY_CONTINUE;

		ENSURES( certDataLength >= 16 && certDataLength <= MAX_CERT_SIZE );
		ENSURES( ( stateInfo != NULL && \
				   ( queryType == DBMS_QUERY_NORMAL || \
					 queryType == DBMS_QUERY_CONTINUE ) ) || \
				 ( stateInfo == NULL && queryType == DBMS_QUERY_CONTINUE ) );

		/* If the first byte of the certificate data is 0xFF then this is an 
		   item that's physically but not logically present (see the comment 
		   above in the check for the keyValue), which means that we can't 
		   explicitly fetch it (te audire non possum, musa sapientum fixa 
		   est in aure) */
		if( certificate[ 0 ] == 0xFF )
			{
			/* If it's a multi-certificate query try again with the next 
			   result */
			if( multiCertQuery )
				continue;
			
			/* It's a point query, we found something but it isn't there.
			   "Can't you understand English you arse, we're not at home"
			   -- Jeremy Black, "The Boys from Brazil" */
			return( CRYPT_ERROR_NOTFOUND );
			}

		/* If more than one certificate is present and the requested key 
		   usage doesn't match the one indicated in the certificate, try 
		   again */
		if( multiCertQuery && \
			!checkCertUsage( certificate, certDataLength, options ) )
			continue;

		/* We got what we wanted, exit */
		break;
		}
	if( iterationCount >= FAILSAFE_ITERATIONS_MED )
		{
		retExt_IntError( CRYPT_ERROR_NOTFOUND, 
						 ( CRYPT_ERROR_NOTFOUND, errorInfo, 
						   "Couldn't find matching certificate after "
						   "processing %d items", FAILSAFE_ITERATIONS_MED ) );
		}

	/* If we've been looking through multiple certificates, cancel the 
	   outstanding query, which will still be in progress */
	if( multiCertQuery )
		dbmsStaticQuery( NULL, cachedQueryType, DBMS_QUERY_CANCEL );

	/* Create a certificate object from the encoded certificate data.  If 
	   we're reading revocation information the data is a single CRL entry 
	   so we have to tell the certificate import code to treat it as a 
	   special case of a CRL.  If we're reading a certificate request it 
	   could be either a PKCS #10 or CRMF request so we have to use auto-
	   detection rather than specifying an exact format */
	setMessageCreateObjectIndirectInfo( &createInfo, certificate, 
										certDataLength,
		( itemType == KEYMGMT_ITEM_PUBLICKEY || \
		  itemType == KEYMGMT_ITEM_NONE ) ? CRYPT_CERTTYPE_CERTIFICATE : \
		( itemType == KEYMGMT_ITEM_REQUEST ) ? CRYPT_CERTTYPE_NONE : \
		( itemType == KEYMGMT_ITEM_REVREQUEST ) ? CRYPT_CERTTYPE_REQUEST_REVOCATION : \
		( itemType == KEYMGMT_ITEM_PKIUSER ) ? CRYPT_CERTTYPE_PKIUSER : \
		( itemType == KEYMGMT_ITEM_REVOCATIONINFO ) ? CRYPT_ICERTTYPE_REVINFO : \
		CRYPT_CERTTYPE_NONE );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't recreate certificate from stored certificate "
				  "data" ) );
		}
	if( itemType != KEYMGMT_ITEM_NONE )
		{
		/* If we're doing a fetch based on some form of identifier, make 
		   sure that the data source has really given us the item that we've 
		   asked for */
		status = checkObjectIDMatch( createInfo.cryptHandle, itemType,
									 keyIDtype, keyValue, keyValueLength );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( createInfo.cryptHandle, 
							  IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, errorInfo, 
					  "Certificate fetched for ID type %d doesn't actually "
					  "correspond to the given ID", keyIDtype ) );
			}
		}
	*iCertificate = createInfo.cryptHandle;

	/* If this was a read with state held externally remember where we got
	   to so that we can fetch the next certificate in the sequence */
	if( stateInfo != NULL )
		*stateInfo = *iCertificate;
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 6 ) ) \
static int getFirstItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								 OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								 OUT int *stateInfo,
								 IN_ENUM( KEYMGMT_ITEM ) \
									const KEYMGMT_ITEM_TYPE itemType,
								 IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								 IN_BUFFER( keyIDlength ) const void *keyID, 
								 IN_LENGTH_KEYID const int keyIDlength,
								 IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	char encodedKeyID[ ( CRYPT_MAX_TEXTSIZE * 2 ) + 8 ];
	int encodedKeyIDlength, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( iCertificate == NULL || \
			isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( stateInfo, sizeof( int ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( itemType == KEYMGMT_ITEM_NONE || \
			  itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REQUEST || \
			  itemType == KEYMGMT_ITEM_REVREQUEST || \
			  itemType == KEYMGMT_ITEM_PKIUSER || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO );
	REQUIRES( keyIDtype > CRYPT_KEYID_NONE && \
			  keyIDtype < CRYPT_KEYID_LAST );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX );
	REQUIRES( ( options & KEYMGMT_MASK_USAGEOPTIONS ) != \
			  KEYMGMT_MASK_USAGEOPTIONS );

	/* Fetch the first data item */
	status = makeKeyID( encodedKeyID, CRYPT_MAX_TEXTSIZE * 2, 
						&encodedKeyIDlength, keyIDtype, keyID, keyIDlength );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_STR1 );
	return( getItemData( dbmsInfo, iCertificate, stateInfo, itemType, 
						 keyIDtype, encodedKeyID, encodedKeyIDlength, 
						 options, KEYSET_ERRINFO ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getNextItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
								INOUT int *stateInfo, 
								IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( ( stateInfo == NULL ) || \
			isWritePtr( stateInfo, sizeof( int ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX );
	REQUIRES( ( options & KEYMGMT_MASK_USAGEOPTIONS ) != \
			  KEYMGMT_MASK_USAGEOPTIONS );

	/* If we're fetching the next certificate in a sequence based on 
	   externally-held state information, set the key ID to the nameID of 
	   the previous certificate's issuer, identified using the general
	   cryptlib ID of CRYPT_IKEYID_SUBJECTID (the term "nameID" is used
	   only for database keysets) */
	if( stateInfo != NULL )
		{
		char encodedKeyID[ ENCODED_DBXKEYID_SIZE + 8 ];
		int encodedKeyIDlength, status;

		status = getKeyID( encodedKeyID, ENCODED_DBXKEYID_SIZE, 
						   &encodedKeyIDlength, *stateInfo, 
						   CRYPT_IATTRIBUTE_ISSUER );
		if( cryptStatusError( status ) )
			return( status );
		return( getItemData( dbmsInfo, iCertificate, stateInfo, 
							 KEYMGMT_ITEM_PUBLICKEY, CRYPT_IKEYID_SUBJECTID, 
							 encodedKeyID, encodedKeyIDlength, options, 
							 KEYSET_ERRINFO ) );
		}

	/* Fetch the next data item in an ongoing query */
	return( getItemData( dbmsInfo, iCertificate, NULL, KEYMGMT_ITEM_NONE, 
						 CRYPT_KEYID_NONE, NULL, 0, options, 
						 KEYSET_ERRINFO ) );
	}

/* Retrieve a certificate object from the database */

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
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	
	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( itemType == KEYMGMT_ITEM_NONE || \
			  itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REQUEST || \
			  itemType == KEYMGMT_ITEM_REVREQUEST || \
			  itemType == KEYMGMT_ITEM_PKIUSER || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO );
	REQUIRES( keyIDtype > CRYPT_KEYID_NONE && \
			  keyIDtype < CRYPT_KEYID_LAST );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( auxInfo == NULL && *auxInfoLength == 0 );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && \
			  flags < KEYMGMT_FLAG_MAX );
	REQUIRES( ( flags & KEYMGMT_MASK_USAGEOPTIONS ) != \
			  KEYMGMT_MASK_USAGEOPTIONS );

	/* Clear return value */
	*iCryptHandle = CRYPT_ERROR;

	/* There are some query types that can only be satisfied by a 
	   certificate store since a standard database doesn't contain the 
	   necessary fields.  Before we do anything else we make sure that we 
	   can resolve the query using the current database type */
	if( !( dbmsInfo->flags & DBMS_FLAG_CERTSTORE_FIELDS ) )
		{
		/* A standard database doesn't contain a certificate ID in the 
		   revocation information since the CRL that it's populated from 
		   only contains an issuerAndSerialNumber, so we can't resolve 
		   queries for revocation information using a certificate ID */
		if( itemType == KEYMGMT_ITEM_REVOCATIONINFO && \
			keyIDtype == CRYPT_IKEYID_CERTID )
			{
			retExt( CRYPT_ERROR_NOTFOUND, 
					( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
					  "Operation is only valid for certificate stores" ) );
			}
		}

	/* If this is a CA management item fetch, fetch the data from the CA 
	   certificate store */
	if( itemType == KEYMGMT_ITEM_REQUEST || \
		itemType == KEYMGMT_ITEM_REVREQUEST || \
		itemType == KEYMGMT_ITEM_PKIUSER || \
		( itemType == KEYMGMT_ITEM_REVOCATIONINFO && \
		  !( flags & KEYMGMT_FLAG_CHECK_ONLY ) ) )
		{
		int dummy;

		/* If we're getting the issuing PKI user (which means that the key ID
		   that's being queried on is that of an issued certificate that the 
		   PKI user owns rather than that of the PKI user themselves) fetch 
		   the user information via a special function */
		if( itemType == KEYMGMT_ITEM_PKIUSER && \
			( flags & KEYMGMT_FLAG_GETISSUER ) )
			{
			char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
			int certIDlength;

			REQUIRES( keyIDtype == CRYPT_IKEYID_CERTID );

			/* The information required to locate the PKI user from one of 
			   their certificates is only present in a certificate store */
			if( !isCertStore( dbmsInfo ) )
				{
				retExt( CRYPT_ERROR_NOTFOUND, 
						( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
						  "Operation is only valid for certificate stores" ) );
				}

			/* Get the PKI user based on the certificate */
			status = makeKeyID( certID, ENCODED_DBXKEYID_SIZE, &certIDlength,
								CRYPT_IKEYID_CERTID, keyID, keyIDlength );
			if( cryptStatusError( status ) )
				return( CRYPT_ARGERROR_STR1 );
			return( caGetIssuingUser( dbmsInfo, iCryptHandle, 
									  certID, certIDlength, 
									  KEYSET_ERRINFO ) );
			}

		/* This is just a standard read from a non-certificate table rather
		   than the certificate table so we call the get-first certificate 
		   function directly rather than going via the indirect certificate-
		   import code.  Since it's a direct call we need to provide a dummy 
		   return variable for the state information that's normally handled 
		   by the indirect-import code */
		return( getFirstItemFunction( keysetInfoPtr, iCryptHandle, &dummy,
									  itemType, keyIDtype, keyID, 
									  keyIDlength, KEYMGMT_FLAG_NONE ) );
		}

	/* If we're doing a check only, just check whether the item is present
	   without fetching any data */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		{
		BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
		char sqlBuffer[ MAX_SQL_QUERY_SIZE + 8 ];
		char encodedKeyID[ ENCODED_DBXKEYID_SIZE + 8 ];
		const char *keyName = getKeyName( keyIDtype );
		const char *selectString = getSelectString( itemType );
		int encodedKeyIDlength;

		REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
				  itemType == KEYMGMT_ITEM_REVOCATIONINFO );
		REQUIRES( keyIDlength == KEYID_SIZE );
		REQUIRES( keyIDtype == CRYPT_IKEYID_ISSUERID || \
				  keyIDtype == CRYPT_IKEYID_CERTID );
		ENSURES( keyName != NULL && selectString != NULL );

		/* Check whether this item is present.  We don't care about the 
		   result data, all we want to know is whether it's there or not so 
		   we just do a check rather than a fetch of any data */
		status = makeKeyID( encodedKeyID, ENCODED_DBXKEYID_SIZE, 
							&encodedKeyIDlength, keyIDtype, 
							keyID, KEYID_SIZE );
		if( cryptStatusError( status ) )
			return( CRYPT_ARGERROR_STR1 );
		strlcpy_s( sqlBuffer, MAX_SQL_QUERY_SIZE, selectString );
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, keyName );
		strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, " = ?" );
		initBoundData( boundDataPtr );
		setBoundData( boundDataPtr, 0, encodedKeyID, encodedKeyIDlength );
		return( dbmsQuery( sqlBuffer, NULL, 0, NULL, boundDataPtr,
						   getCachedQueryType( itemType, keyIDtype ),
						   DBMS_QUERY_CHECK ) );
		}

	/* Import the certificate by doing an indirect read, which fetches 
	   either a single certificate or an entire chain if it's present */
	return( iCryptImportCertIndirect( iCryptHandle, keysetInfoPtr->objectHandle,
									  keyIDtype, keyID, keyIDlength,
									  flags & KEYMGMT_MASK_CERTOPTIONS ) );
	}

/* Add special data to the database.  Technically this is a set-function but 
   because it initiates a query-fetch it's included with the get-functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int setSpecialItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
								   IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE dataType,
								   IN_BUFFER( dataLength ) const void *data, 
								   IN_LENGTH_SHORT const int dataLength )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	char sqlBuffer[ MAX_SQL_QUERY_SIZE + 8 ];
	const KEYMGMT_ITEM_TYPE itemType = \
						( dataType == CRYPT_KEYINFO_QUERY_REQUESTS ) ? \
						KEYMGMT_ITEM_REQUEST : KEYMGMT_ITEM_PUBLICKEY;
	const char *selectString = getSelectString( itemType );
	int sqlLength, sqlQueryLength, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	ENSURES( dataType == CRYPT_KEYINFO_QUERY || \
			 dataType == CRYPT_KEYINFO_QUERY_REQUESTS );
	ENSURES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			 itemType == KEYMGMT_ITEM_REQUEST );
	ENSURES( selectString != NULL );

	/* The kernel enforces a size range from 6...CRYPT_MAX_TEXTSIZE but we 
	   perform an explicit check here against possible database-specific 
	   values that may be more specific than the kernel's one-size-fits-all 
	   values */
	if( dataLength < 6 || dataLength > MAX_SQL_QUERY_SIZE - 64 )
		{
		retExtArg( CRYPT_ARGERROR_STR1, 
				   ( CRYPT_ARGERROR_STR1, KEYSET_ERRINFO, 
					 "Invalid query length, should be from 6...%d "
					 "characters", MAX_SQL_QUERY_SIZE - 64 ) );
		}

	/* If we're cancelling an existing query, pass it on down */
	if( dataLength == 6 && !strCompare( data, "cancel", dataLength ) )
		{
		return( dbmsStaticQuery( NULL, DBMS_CACHEDQUERY_NONE,
								 DBMS_QUERY_CANCEL ) );
		}

	ENSURES( !keysetInfoPtr->isBusyFunction( keysetInfoPtr ) );

	/* Rewrite the user-supplied portion of the query using the actual 
	   column names and append it to the SELECT statement.  This is a 
	   special case free-format query where we can't use bound parameters 
	   because the query data must be interpreted as SQL, unlike standard 
	   queries where we definitely don't want it (mis-)interpreted as SQL.  
	   dbmsFormatQuery() tries to sanitise the query as much as it can but 
	   in general we rely on developers reading the warnings in the 
	   documentation about the appropriate use of this capability */
	strlcpy_s( sqlBuffer, MAX_SQL_QUERY_SIZE, selectString );
	sqlLength = strlen( sqlBuffer );
	status = dbmsFormatQuery( sqlBuffer + sqlLength, 
							 ( MAX_SQL_QUERY_SIZE - 1 ) - sqlLength, 
							 &sqlQueryLength, data, dataLength );
	if( cryptStatusError( status ) )
		{
		retExtArg( CRYPT_ARGERROR_STR1, 
				   ( CRYPT_ARGERROR_STR1, KEYSET_ERRINFO, 
					 "Invalid query format" ) );
		}
	return( dbmsStaticQuery( sqlBuffer, DBMS_CACHEDQUERY_NONE, 
							 DBMS_QUERY_START ) );
	}

/****************************************************************************
*																			*
*							Database Access Routines						*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSread( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );

	keysetInfoPtr->getItemFunction = getItemFunction;
	keysetInfoPtr->getFirstItemFunction = getFirstItemFunction;
	keysetInfoPtr->getNextItemFunction = getNextItemFunction;
	keysetInfoPtr->setSpecialItemFunction = setSpecialItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
