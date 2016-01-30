/****************************************************************************
*																			*
*							cryptlib DBMS Interface							*
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

/* A structure to store ID information extracted from a certificate before 
   it's added to the certificate store */

typedef struct {
	/* User identification data */
	BUFFER( CRYPT_MAX_TEXTSIZE, Clength ) \
	char C[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, SPlength ) \
	char SP[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, Llength ) \
	char L[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, Olength ) \
	char O[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, OUlength ) \
	char OU[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, CNlength ) \
	char CN[ CRYPT_MAX_TEXTSIZE + 8 ];
	BUFFER( CRYPT_MAX_TEXTSIZE, uriLength ) \
	char uri[ CRYPT_MAX_TEXTSIZE + 8 ];
	int Clength, SPlength, Llength, Olength, OUlength, CNlength, uriLength;

	/* Certificate identification data */
	BUFFER( ENCODED_DBXKEYID_SIZE, certIDlength ) \
	char certID[ ENCODED_DBXKEYID_SIZE + 8 ];
	BUFFER( ENCODED_DBXKEYID_SIZE, nameIDlength ) \
	char nameID[ ENCODED_DBXKEYID_SIZE + 8 ];
	BUFFER( ENCODED_DBXKEYID_SIZE, issuerIDlength ) \
	char issuerID[ ENCODED_DBXKEYID_SIZE + 8 ];
	BUFFER( ENCODED_DBXKEYID_SIZE, keyIDlength ) \
	char keyID[ ENCODED_DBXKEYID_SIZE + 8 ];
	int certIDlength, nameIDlength, issuerIDlength, keyIDlength;
	} CERT_ID_DATA;

#ifdef USE_DBMS

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get the SQL string to delete data from a given table */

CHECK_RETVAL_PTR \
static char *getDeleteString( IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType )
	{
	REQUIRES_N( itemType == KEYMGMT_ITEM_PKIUSER || \
				itemType == KEYMGMT_ITEM_PUBLICKEY );

	switch( itemType )
		{
		case KEYMGMT_ITEM_PKIUSER:
			return( "DELETE FROM pkiUsers WHERE " );

		case KEYMGMT_ITEM_PUBLICKEY:
			return( "DELETE FROM certificates WHERE " );
		}

	retIntError_Null();
	}

/****************************************************************************
*																			*
*							Extract ID Information							*
*																			*
****************************************************************************/

/* Extract user identification data from a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int extractCertNameData( IN_HANDLE const CRYPT_CERTIFICATE iCryptHandle,
								IN_ENUM( CRYPT_CERTTYPE ) \
									const CRYPT_CERTTYPE_TYPE certType,
								OUT CERT_ID_DATA *certIdData )
	{
	static const int nameValue = CRYPT_CERTINFO_SUBJECTNAME;
	static const int altNameValue = CRYPT_CERTINFO_SUBJECTALTNAME;
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( certIdData, sizeof( CERT_ID_DATA ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( certType == CRYPT_CERTTYPE_CERTIFICATE || \
			  certType == CRYPT_CERTTYPE_REQUEST_CERT || \
			  certType == CRYPT_CERTTYPE_PKIUSER );

	/* Clear return value */
	memset( certIdData, 0, sizeof( CERT_ID_DATA ) );

	/* Extract the DN and altName (URI) components.  This changes the 
	   currently selected DN components but this is OK because we've got the 
	   certificate locked and the prior state will be restored when we 
	   unlock it */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &nameValue, 
							  CRYPT_ATTRIBUTE_CURRENT );
	if( cryptStatusError( status ) )
		return( status );
	setMessageData( &msgData, certIdData->C, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_COUNTRYNAME );
	if( cryptStatusOK( status ) )
		certIdData->Clength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}
	setMessageData( &msgData, certIdData->SP, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_STATEORPROVINCENAME );
	if( cryptStatusOK( status ) )
		certIdData->SPlength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}
	setMessageData( &msgData, certIdData->L, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_LOCALITYNAME );
	if( cryptStatusOK( status ) )
		certIdData->Llength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}
	setMessageData( &msgData, certIdData->O, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_ORGANIZATIONNAME );
	if( cryptStatusOK( status ) )
		certIdData->Olength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}
	setMessageData( &msgData, certIdData->OU, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_ORGANIZATIONALUNITNAME );
	if( cryptStatusOK( status ) )
		certIdData->OUlength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}

	/* The CommonName component is the generic "name" associated with the 
	   certificate, to make sure that there's always at least something 
	   useful present to identify it we fetch the certificate holder name 
	   rather than the specific common name */
	setMessageData( &msgData, certIdData->CN, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_IATTRIBUTE_HOLDERNAME );
	if( cryptStatusOK( status ) )
		certIdData->CNlength = msgData.length;
	else
		{
		if( status != CRYPT_ERROR_NOTFOUND )
			return( status );
		}

	/* PKI user objects don't have URI information so if we're processing 
	   one of these we're done */
	if( certType == CRYPT_CERTTYPE_PKIUSER )
		return( CRYPT_OK );

	/* Get the URI for this certificate, in order of likelihood of 
	   occurrence */
	setMessageData( &msgData, certIdData->uri, CRYPT_MAX_TEXTSIZE );
	krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
					 ( MESSAGE_CAST ) &altNameValue, 
					 CRYPT_ATTRIBUTE_CURRENT );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_RFC822NAME );
	if( status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, certIdData->uri, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, 
								  CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER );
		}
	if( status == CRYPT_ERROR_NOTFOUND )
		{
		setMessageData( &msgData, certIdData->uri, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_DNSNAME );
		}
	if( cryptStatusOK( status ) )
		{
		int i;

		/* Force the URI (as stored) to lowercase to make case-insensitive 
		   matching easier.  In most cases we could ask the back-end to do 
		   this but this complicates indexing and there's no reason why we 
		   can't do it here */
		for( i = 0; i < msgData.length; i++ )
			certIdData->uri[ i ] = intToByte( toLower( certIdData->uri[ i ] ) );
		certIdData->uriLength = msgData.length;
		}
	
	return( status );
	}

/* Extract certificate identification data from a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int extractCertIdData( IN_HANDLE const CRYPT_CERTIFICATE iCryptHandle,
							  IN_ENUM( CRYPT_CERTTYPE ) \
								const CRYPT_CERTTYPE_TYPE certType,
							  INOUT CERT_ID_DATA *certIdData )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( certIdData, sizeof( CERT_ID_DATA ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( certType == CRYPT_CERTTYPE_CERTIFICATE || \
			  certType == CRYPT_CERTTYPE_REQUEST_CERT || \
			  certType == CRYPT_CERTTYPE_PKIUSER );

	/* Get general ID information */
	status = getKeyID( certIdData->certID, ENCODED_DBXKEYID_SIZE, 
					   &certIdData->certIDlength, iCryptHandle,
					   CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusError( status ) )
		return( status );

	/* Get object-specific ID information */
	if( certType == CRYPT_CERTTYPE_CERTIFICATE )
		{
		status = getKeyID( certIdData->nameID, ENCODED_DBXKEYID_SIZE, 
						   &certIdData->nameIDlength, iCryptHandle, 
						   CRYPT_IATTRIBUTE_SUBJECT );
		if( cryptStatusOK( status ) )
			status = getKeyID( certIdData->issuerID, ENCODED_DBXKEYID_SIZE, 
							   &certIdData->issuerIDlength, iCryptHandle, 
							   CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
		if( cryptStatusOK( status ) )
			status = getCertKeyID( certIdData->keyID, ENCODED_DBXKEYID_SIZE, 
								   &certIdData->keyIDlength, iCryptHandle );
		return( status );
		}
	if( certType == CRYPT_CERTTYPE_PKIUSER )
		{
		BYTE binaryKeyID[ 64 + 8 ];
		char encKeyID[ CRYPT_MAX_TEXTSIZE + 8 ];
		int binaryKeyIDlength;

		/* Get the PKI user ID.  We can't read this directly since it's
		   returned in text form for use by end users so we have to read the
		   encoded form, decode it, and then turn the decoded binary value
		   into a key ID.  We identify the result as a keyID,
		   (== subjectKeyIdentifier, which it isn't really) but we need to
		   use this to ensure that it's hashed/expanded out to the correct
		   size */
		setMessageData( &msgData, encKeyID, CRYPT_MAX_TEXTSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_PKIUSER_ID );
		if( cryptStatusError( status ) )
			return( status );
		status =  decodePKIUserValue( binaryKeyID, 64, &binaryKeyIDlength, 
									  encKeyID, msgData.length );
		if( cryptStatusOK( status ) )
			status = makeKeyID( certIdData->keyID, ENCODED_DBXKEYID_SIZE, 
								&certIdData->keyIDlength, CRYPT_IKEYID_KEYID, 
								binaryKeyID, binaryKeyIDlength );
		if( cryptStatusOK( status ) )
			status = getKeyID( certIdData->nameID, ENCODED_DBXKEYID_SIZE, 
							   &certIdData->nameIDlength, iCryptHandle, 
							   CRYPT_IATTRIBUTE_SUBJECT );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Extract certificate identification data from a CRL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
static int extractCrlIdData( IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL,
							 IN_HANDLE_OPT \
								const CRYPT_CERTIFICATE iCryptRevokeCert,
							 OUT CERT_ID_DATA *crlIdData )
	{
	int status;

	assert( isWritePtr( crlIdData, sizeof( CERT_ID_DATA ) ) );

	REQUIRES( isHandleRangeValid( iCryptCRL ) );
	REQUIRES( iCryptRevokeCert == CRYPT_UNUSED || \
			  isHandleRangeValid( iCryptRevokeCert ) );

	/* Clear return value */
	memset( crlIdData, 0, sizeof( CERT_ID_DATA ) );

	/* Get general ID information */
	status = getKeyID( crlIdData->issuerID, ENCODED_DBXKEYID_SIZE, 
					   &crlIdData->issuerIDlength, iCryptCRL, 
					   CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( status ) )
		return( status );

	/* If there's no certificate being revoked present (i.e. we're just 
	   adding a set of CRL entries), we're done */
	if( iCryptRevokeCert == CRYPT_UNUSED )
		return( CRYPT_OK );

	/* Get the certificate ID and the name ID of the issuer from the 
	   certificate being revoked */
	status = getKeyID( crlIdData->certID, ENCODED_DBXKEYID_SIZE, 
					   &crlIdData->certIDlength, iCryptRevokeCert,
					   CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusOK( status ) )
		status = getKeyID( crlIdData->nameID, ENCODED_DBXKEYID_SIZE, 
						   &crlIdData->nameIDlength, iCryptRevokeCert,
						   CRYPT_IATTRIBUTE_ISSUER );
	return( status );
	}

/****************************************************************************
*																			*
*							Database Add Routines							*
*																			*
****************************************************************************/

/* Add a certificate object (certificate, certificate request, PKI user) to 
   a certificate store.  Normally existing rows would be overwritten if we 
   added duplicate entries but the UNIQUE constraint on the indices will 
   catch this */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 6 ) ) \
int addCert( INOUT DBMS_INFO *dbmsInfo, 
			 IN_HANDLE const CRYPT_HANDLE iCryptHandle,
			 IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE certType, 
			 IN_ENUM( CERTADD ) const CERTADD_TYPE addType,
			 IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType, 
			 INOUT ERROR_INFO *errorInfo )
	{
	MESSAGE_DATA msgData;
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	CERT_ID_DATA certIdData;
	BYTE certData[ MAX_CERT_SIZE + 8 ];
	char encodedCertData[ MAX_ENCODED_CERT_SIZE + 8 ];
	const char *sqlString;
	time_t boundDate;
	int certDataLength = DUMMY_INIT, boundDataIndex, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( certType == CRYPT_CERTTYPE_CERTIFICATE || \
			  certType == CRYPT_CERTTYPE_REQUEST_CERT || \
			  certType == CRYPT_CERTTYPE_PKIUSER );
	REQUIRES( addType > CERTADD_NONE && addType < CERTADD_LAST );
	REQUIRES( updateType > DBMS_UPDATE_NONE && \
			  updateType < DBMS_UPDATE_LAST );
	REQUIRES( errorInfo != NULL );

	/* Extract name-related information from the certificate */
	status = extractCertNameData( iCryptHandle, certType, &certIdData );
	if( ( cryptStatusOK( status ) || status == CRYPT_ERROR_NOTFOUND ) && \
		( certType == CRYPT_CERTTYPE_CERTIFICATE ) )
		{
		setMessageData( &msgData, &boundDate, sizeof( time_t ) );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_VALIDTO );
		}
	else
		{
		if( status == CRYPT_ERROR_NOTFOUND )
			status = CRYPT_OK;
		}
	if( cryptStatusError( status ) )
		{
		/* Convert any low-level certificate-specific error into something 
		   generic that makes a bit more sense to the caller */
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, errorInfo, 
					 "Couldn't extract user identification information "
					 "from certificate" ) );
		}

	/* Get the ID information and certificate data from the certificate */
	status = extractCertIdData( iCryptHandle, certType, &certIdData );
	if( cryptStatusOK( status ) )
		{
		status = extractCertData( iCryptHandle, 
								  ( certType == CRYPT_CERTTYPE_PKIUSER ) ? \
									CRYPT_ICERTFORMAT_DATA : \
									CRYPT_CERTFORMAT_CERTIFICATE,
								  certData, MAX_CERT_SIZE, &certDataLength );
		}
	if( cryptStatusError( status ) )
		{
		/* Convert any low-level certificate-specific error into something 
		   generic that makes a bit more sense to the caller */
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, errorInfo, 
					 "Couldn't extract certificate data from "
					 "certificate" ) );
		}

	/* If this is a partial add (in which we add a certificate item which is 
	   in the initial stages of the creation process so that although the 
	   item may be physically present in the store it can't be accessed 
	   directly) we set the first byte to 0xFF to indicate this.  In 
	   addition we set the first two bytes of the IDs that have uniqueness 
	   constraints to an out-of-band value to prevent a clash with the 
	   finished entry when we complete the issue process and replace the 
	   partial version with the full version */
	if( addType == CERTADD_PARTIAL || addType == CERTADD_PARTIAL_RENEWAL )
		{
		const char *escapeStr = ( addType == CERTADD_PARTIAL ) ? \
								KEYID_ESC1 : KEYID_ESC2;

		certData[ 0 ] = 0xFF;
		memcpy( certIdData.issuerID, escapeStr, KEYID_ESC_SIZE );
		memcpy( certIdData.keyID, escapeStr, KEYID_ESC_SIZE );
		memcpy( certIdData.certID, escapeStr, KEYID_ESC_SIZE );
		}

	/* Set up the certificate object data to be written and send it to the
	   database */
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, certIdData.C, certIdData.Clength );
	setBoundData( boundDataPtr, 1, certIdData.SP, certIdData.SPlength );
	setBoundData( boundDataPtr, 2, certIdData.L, certIdData.Llength );
	setBoundData( boundDataPtr, 3, certIdData.O, certIdData.Olength );
	setBoundData( boundDataPtr, 4, certIdData.OU, certIdData.OUlength );
	setBoundData( boundDataPtr, 5, certIdData.CN, certIdData.CNlength );
	switch( certType )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
			setBoundData( boundDataPtr, 6, certIdData.uri, 
						  certIdData.uriLength );
			setBoundDataDate( boundDataPtr, 7, &boundDate );
			setBoundData( boundDataPtr, 8, certIdData.nameID, 
						  certIdData.nameIDlength );
			setBoundData( boundDataPtr, 9, certIdData.issuerID, 
						  certIdData.issuerIDlength );
			setBoundData( boundDataPtr, 10, certIdData.keyID, 
						  certIdData.keyIDlength );
			boundDataIndex = 11;
			sqlString = \
			"INSERT INTO certificates VALUES (?, ?, ?, ?, ?, ?, ?,"
											 "?, ?, ?, ?, ?, ?)";
			break;

		case CRYPT_CERTTYPE_REQUEST_CERT:
			setBoundData( boundDataPtr, 6, certIdData.uri, 
						  certIdData.uriLength );
			boundDataIndex = 7;
			sqlString = \
			"INSERT INTO certRequests VALUES ('" TEXT_CERTTYPE_REQUEST_CERT "', "
											 "?, ?, ?, ?, ?, ?, ?, ?, ?)";
			break;

		case CRYPT_CERTTYPE_PKIUSER:
			setBoundData( boundDataPtr, 6, certIdData.nameID, 
						  certIdData.nameIDlength );
			setBoundData( boundDataPtr, 7, certIdData.keyID, 
						  certIdData.keyIDlength );
			boundDataIndex = 8;
			sqlString = \
			"INSERT INTO pkiUsers VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
			break;

		default:
			retIntError();
		}
	setBoundData( boundDataPtr, boundDataIndex++, certIdData.certID, 
				  certIdData.certIDlength );
	if( hasBinaryBlobs( dbmsInfo ) )
		{
		setBoundDataBlob( boundDataPtr, boundDataIndex, 
						  certData, certDataLength );
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
			retIntError();
			}
		setBoundData( boundDataPtr, boundDataIndex, encodedCertData, 
					  encodedCertDataLength );
		}
	status = dbmsUpdate( sqlString, boundDataPtr, updateType );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Certificate add operation failed: " ) );
		}
	return( CRYPT_OK );
	}

/* Add a CRL to a certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int addCRL( INOUT DBMS_INFO *dbmsInfo, 
			IN_HANDLE const CRYPT_CERTIFICATE iCryptCRL,
			IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptRevokeCert,
			IN_ENUM( DBMS_UPDATE ) const DBMS_UPDATE_TYPE updateType, 
			INOUT ERROR_INFO *errorInfo )
	{
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	CERT_ID_DATA crlIdData;
	BYTE certData[ MAX_CERT_SIZE + 8 ];
	char encodedCertData[ MAX_ENCODED_CERT_SIZE + 8 ];
	const char *sqlString;
	time_t expiryDate = 0;
	int certDataLength = DUMMY_INIT, boundDataIndex, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( isHandleRangeValid( iCryptCRL ) );
	REQUIRES( ( isCertStore( dbmsInfo ) && \
				isHandleRangeValid( iCryptRevokeCert ) ) || \
			  ( !isCertStore( dbmsInfo ) && \
				iCryptRevokeCert == CRYPT_UNUSED ) );
	REQUIRES( updateType > DBMS_UPDATE_NONE && \
			  updateType < DBMS_UPDATE_LAST );
	REQUIRES( errorInfo != NULL );

	/* Get the ID information for the current CRL entry */
	status = extractCrlIdData( iCryptCRL, iCryptRevokeCert, &crlIdData );
	if( cryptStatusOK( status ) )
		status = extractCertData( iCryptCRL, CRYPT_IATTRIBUTE_CRLENTRY,
								  certData, MAX_CERT_SIZE, &certDataLength );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, &expiryDate, sizeof( time_t ) );
		status = krnlSendMessage( iCryptRevokeCert, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_CERTINFO_VALIDTO );
		}
	if( cryptStatusError( status ) )
		{
		/* Convert any low-level certificate-specific error into something 
		   generic that makes a bit more sense to the caller */
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, errorInfo, 
					 "Couldn't extract CRL data from CRL" ) );
		}

	/* Set up the certificate object data to be written and send it to the 
	   database.  Certificate stores contain extra inforomation that's 
	   needed to build a CRL so we have to vary the SQL string depending on 
	   the keyset type */
	initBoundData( boundDataPtr );
	if( isCertStore( dbmsInfo ) )
		{
		setBoundDataDate( boundDataPtr, 0, &expiryDate );
		setBoundData( boundDataPtr, 1, crlIdData.nameID, 
					  crlIdData.nameIDlength );
		setBoundData( boundDataPtr, 2, crlIdData.issuerID, 
					  crlIdData.issuerIDlength );
		setBoundData( boundDataPtr, 3, crlIdData.certID, 
					  crlIdData.certIDlength );
		boundDataIndex = 4;
		sqlString = "INSERT INTO CRLs VALUES (?, ?, ?, ?, ?)";

		}
	else
		{
		setBoundData( boundDataPtr, 0, crlIdData.issuerID, 
					  crlIdData.issuerIDlength );
		boundDataIndex = 1;
		sqlString = "INSERT INTO CRLs VALUES (?, ?)";
		}
	if( hasBinaryBlobs( dbmsInfo ) )
		{
		setBoundDataBlob( boundDataPtr, boundDataIndex, certData, 
						  certDataLength );
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
			retIntError();
			}
		setBoundData( boundDataPtr, boundDataIndex, encodedCertData, 
					  encodedCertDataLength );
		}
	status = dbmsUpdate( sqlString, boundDataPtr, updateType );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "CRL add operation failed: " ) );
		}

	return( CRYPT_OK );
	}

/* Add an item to the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							IN_HANDLE const CRYPT_HANDLE iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							STDC_UNUSED const char *password, 
							STDC_UNUSED const int passwordLength,
							IN_FLAGS( KEYMGMT ) const int flags )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	BOOLEAN seenNonDuplicate = FALSE;
	int type, iterationCount = 0, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO || \
			  itemType == KEYMGMT_ITEM_REQUEST || \
			  itemType == KEYMGMT_ITEM_REVREQUEST || \
			  itemType == KEYMGMT_ITEM_PKIUSER );
	REQUIRES( password == NULL && passwordLength == 0 );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Make sure that we've been given a certificate, certificate chain, or 
	   CRL (or a PKI user if it's a CA certificate store).  We can't do any 
	   more specific checking against the itemType because if it's coming 
	   from outside cryptlib it'll just be passed in as a generic 
	   certificate object with no distinction between object subtypes */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE,
							  &type, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_NUM1 );
	if( isCertStore( dbmsInfo ) )
		{
		/* The only item that can be inserted directly into a CA certificate
		   store is a CA request or PKI user information */
		if( type != CRYPT_CERTTYPE_CERTREQUEST && \
			type != CRYPT_CERTTYPE_REQUEST_CERT && \
			type != CRYPT_CERTTYPE_REQUEST_REVOCATION && \
			type != CRYPT_CERTTYPE_PKIUSER )
			{
			retExtArg( CRYPT_ARGERROR_NUM1, 
					   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
						 "Invalid item type for CA certificate store" ) );
			}

		if( itemType == KEYMGMT_ITEM_PKIUSER )
			{
			REQUIRES( type == CRYPT_CERTTYPE_PKIUSER );
			return( caAddPKIUser( dbmsInfo, iCryptHandle, KEYSET_ERRINFO ) );
			}

		/* It's a certificate request being added to a CA certificate 
		   store */
		REQUIRES( ( itemType == KEYMGMT_ITEM_REQUEST && \
					( type == CRYPT_CERTTYPE_CERTREQUEST || \
					  type == CRYPT_CERTTYPE_REQUEST_CERT ) ) || \
				  ( itemType == KEYMGMT_ITEM_REVREQUEST && \
				    type == CRYPT_CERTTYPE_REQUEST_REVOCATION ) );
		return( caAddCertRequest( dbmsInfo, iCryptHandle, type,
								  ( flags & KEYMGMT_FLAG_UPDATE ) ? \
									TRUE : FALSE, 
								  ( flags & KEYMGMT_FLAG_INITIALOP ) ? \
									TRUE : FALSE, KEYSET_ERRINFO ) );
		}
	if( type != CRYPT_CERTTYPE_CERTIFICATE && \
		type != CRYPT_CERTTYPE_CERTCHAIN && \
		type != CRYPT_CERTTYPE_CRL )
		{
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
					 "Item being added must be a CRL or certificate" ) );
		}

	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_REVOCATIONINFO );

	/* Lock the certificate or CRL for our exclusive use and select the 
	   first sub-item (certificate in a certificate chain, entry in a CRL), 
	   update the keyset with the certificate(s)/CRL entries, and unlock it 
	   to allow others access */
	status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST,
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		( void ) krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( status );
		}
	do
		{
		/* Add the certificate or CRL */
		if( type == CRYPT_CERTTYPE_CRL )
			status = addCRL( dbmsInfo, iCryptHandle, CRYPT_UNUSED,
							 DBMS_UPDATE_NORMAL, KEYSET_ERRINFO );
		else
			status = addCert( dbmsInfo, iCryptHandle,
							  CRYPT_CERTTYPE_CERTIFICATE, CERTADD_NORMAL,
							  DBMS_UPDATE_NORMAL, KEYSET_ERRINFO );

		/* An item being added may already be present but we can't fail
		   immediately because what's being added may be a chain containing
		   further certificates or a CRL containing further entries so we 
		   keep track of whether we've successfully added at least one item 
		   and clear data duplicate errors */
		if( status == CRYPT_OK )
			seenNonDuplicate = TRUE;
		else
			{
			if( status == CRYPT_ERROR_DUPLICATE )
				status = CRYPT_OK;
			}
		}
	while( cryptStatusOK( status ) && \
		   krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							MESSAGE_VALUE_CURSORNEXT,
							CRYPT_CERTINFO_CURRENT_CERTIFICATE ) == CRYPT_OK && \
		   iterationCount++ < FAILSAFE_ITERATIONS_MED );
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	( void ) krnlSendMessage( iCryptHandle, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusOK( status ) && !seenNonDuplicate )
		{
		/* We reached the end of the certificate chain/CRL without finding 
		   anything that we could add, return a data duplicate error */
		retExt( CRYPT_ERROR_DUPLICATE, 
				( CRYPT_ERROR_DUPLICATE, KEYSET_ERRINFO, 
				  "No new %s were found to add to the certificate store",
				  ( type == CRYPT_CERTTYPE_CRL ) ? \
					"CRL entries" : "certificates" ) );
		}

	return( status );
	}

/* Delete an item from the certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int deleteItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							   IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							   IN_BUFFER( keyIDlength ) const void *keyID, 
							   IN_LENGTH_KEYID const int keyIDlength )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	BOUND_DATA boundData[ BOUND_DATA_MAXITEMS ], *boundDataPtr = boundData;
	char sqlBuffer[ MAX_SQL_QUERY_SIZE + 8 ];
	char encodedKeyID[ ( CRYPT_MAX_TEXTSIZE * 2 ) + 8 ];
	const char *keyName = getKeyName( keyIDtype );
	const char *deleteString = getDeleteString( itemType );
	int encodedKeyIDlength, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PKIUSER );
	REQUIRES( ( !isCertStore( dbmsInfo ) && \
				itemType == KEYMGMT_ITEM_PUBLICKEY ) || \
			  ( isCertStore( dbmsInfo ) && \
				itemType == KEYMGMT_ITEM_PKIUSER ) );
	REQUIRES( keyIDtype > CRYPT_KEYID_NONE && \
			  keyIDtype < CRYPT_KEYID_LAST );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* Delete the item from the certificate store */
	status = makeKeyID( encodedKeyID, CRYPT_MAX_TEXTSIZE * 2, 
						&encodedKeyIDlength, keyIDtype, keyID, keyIDlength );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_STR1 );
	if( isCertStore( dbmsInfo ) )
		{
		/* The only item that can be deleted from a CA certificate store is 
		   PKI user information */
		if( itemType != KEYMGMT_ITEM_PKIUSER )
			{
			retExtArg( CRYPT_ARGERROR_NUM1, 
					   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
						 "Invalid operation for CA certificate store" ) );
			}

		return( caDeletePKIUser( dbmsInfo, keyIDtype, keyID, keyIDlength, 
								 KEYSET_ERRINFO ) );
		}
	ENSURES( keyName != NULL && deleteString != NULL );
	strlcpy_s( sqlBuffer, MAX_SQL_QUERY_SIZE, deleteString );
	strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, keyName );
	strlcat_s( sqlBuffer, MAX_SQL_QUERY_SIZE, " = ?" );
	initBoundData( boundDataPtr );
	setBoundData( boundDataPtr, 0, encodedKeyID, encodedKeyIDlength );
	status = dbmsUpdate( sqlBuffer, boundDataPtr, DBMS_UPDATE_NORMAL );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, KEYSET_ERRINFO, getDbmsErrorInfo( dbmsInfo ),
					 "Certificate delete operation failed: " ) );
		}
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Database Access Routines						*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initDBMSwrite( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_DBMS );

	keysetInfoPtr->setItemFunction = setItemFunction;
	keysetInfoPtr->deleteItemFunction = deleteItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
