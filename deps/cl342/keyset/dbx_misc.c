/****************************************************************************
*																			*
*						  cryptlib DBMS Misc Interface						*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "dbms.h"
  #include "keyset.h"
#else
  #include "crypt.h"
  #include "keyset/dbms.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_DBMS

/* The table structure for the various DBMS tables is (# = indexed, 
   * = unique, + = certificate store only):

	certificates:
		C, SP, L, O, OU, CN, email#, validTo, nameID#, issuerID#*, keyID#*, certID#*, certData
	CRLs:
		expiryDate+, nameID+, issuerID#*, certID#+, crl.certData
	pkiUsers+:
		C, SP, L, O, OU, CN, nameID#*, user.keyID#*, user.certID, user.certData
	certRequests+:
		type, C, SP, L, O, OU, CN, email, req.certID, req.certData
	certLog+:
		action, date, certID#*, req.certID, subjCertID, log.certData

   Note that in the CRL table the certID is the ID of the certificate being 
   revoked and not of the per-entry CRL data, and in the  PKIUsers table the 
   keyID isn't for a public key but a nonce used to identify the PKI user 
   and the nameID is used purely to ensure uniqueness of users.

   The certificate store contains a table for logging certificate management 
   operations (e.g. when issued, when revoked, etc etc).  The operations are 
   tied together by the certID of the object that the log entry pertains to, 
   associated with this in the log are optional certIDs of the request that 
   caused the action to be taken and of the subject that was affected by the 
   request.  For example in the most complex case of a revocation operation
   the certID would be of the CRL entry that was created, the req.certID
   would be for the revocation request, and the subjCertID would be for the
   certificate being revoked.  This allows a complete history of each item 
   to be built via the log.  The certLog has a UNIQUE INDEX on the certID 
   that detects attempts to add duplicates, although this unfortunately 
   requires the addition of dummy nonce certIDs to handle certain types of 
   actions that don't produce objects with certIDs (see the comment further 
   down on the use of constraints).

   The handling for each type of CA management operation is:

	CERTACTION_REQUEST_CERT/CERTACTION_REQUEST_RENEWAL/
	CERTACTION_REQUEST_REVOCATION: Stores the incoming requests and 
	generates a log entry.  Duplicate issue requests are detected by the 
	certLog.certID uniqueness constraint.  Available: request with certID: 

	  INSERT INTO certRequests VALUES ( <type>, <DN components>, <certID>, <request> );
	  INSERT INTO certLog VALUES
		(ACTION_REQUEST_CERT/RENEWAL/REVOCATION, $date, <certID>, NULL, NULL,
		  <request>);

	CERTACTION_ISSUE_CERT/CERTACTION_CERT_CREATION: Add the certificate and  
	remove the issue request.  Duplicate certificate issuance is detected by  
	the certLog.certID uniqueness constraint.  Available: request with 
	req.certID, certificate with certID

	  INSERT INTO certificates VALUES (<DN components>, <IDs>, <cert>);
	  INSERT INTO certLog VALUES
		(ACTION_ISSUE_CERT/CERT_CREATION, $date, <certID>, <req.certID>, NULL,
		  <cert>);
	  DELETE FROM certRequests WHERE certID = <req.certID>;

	CERTACTION_ISSUE_CRL: Read each CRL entry with caCert.nameID and 
	assemble the full CRL.  Requires an ongoing query:

	  SELECT FROM CRLs WHERE nameID = <caCert.nameID>

	CERTACTION_REVOKE_CERT: Add the CRL entry that causes the revocation, 
	delete the certificate and the request that caused the action.  
	Available: request with req.certID, certificate with cert.certID, CRL 
	entry with certID

	  INSERT INTO CRLs VALUES (<IDs>, <crlData>);
	  INSERT INTO certLog VALUES
		(ACTION_REVOKE_CERT, $date, <nonce>, <req.certID>, <cert.certID>, <crlData>);
	  DELETE FROM certRequests WHERE certID = <req.certID>;
	  DELETE FROM certificates WHERE certID = <cert.certID>;

	CERTACTION_EXPIRE_CERT/CERTACTION_RESTART_CLEANUP: Delete each expired 
	entry or clean up leftover certificate requests after a restart.  The 
	logging for these is a bit tricky, ideally we'd want to "INSERT INTO 
	certLog VALUES (ACTION_CERT_EXPIRE, $date, SELECT certID FROM 
	certificates WHERE validTo <= $date)" or the cleanup equivalent, however 
	this isn't possible both because it's not possible to mix static values 
	and a select result in an INSERT and because the certID is already 
	present from when the certificate/request was originally added.  We can 
	fix the former by making the static values part of the select result, 
	i.e. "INSERT INTO certLog VALUES SELECT ACTION_CERT_EXPIRE, $date, 
	certID FROM certificates WHERE validTo <= $date" but this still doesn't 
	fix the problem with the duplicate IDs.  In fact there isn't really a 
	certID present since it's an implicit action, but we can't make the 
	certID column null since it's not possible to index nullable columns.  
	As a result the only way that we can do it is to repetitively perform 
	"SELECT certID FROM certificates WHERE validTo <= $date" (or the 
	equivalent cleanup select) and for each time it succeeds follow it with:

	  INSERT INTO certLog VALUES
		(ACTION_EXPIRE_CERT, $date, <nonce>, NULL, <certID>);
	  DELETE FROM certificates WHERE certID = <certID>

	or

	  INSERT INTO certLog VALUES
		(ACTION_RESTART_CLEANUP, $date, <nonce>, NULL, <certID>);
	  DELETE FROM certRequests WHERE certID = <certID>

	This has the unfortunate side-effect that the update isn't atomic, we 
	could enforce this with "LOCK TABLE <name> IN EXCLUSIVE MODE" but the MS 
	databases don't support this and either require the use of baroque 
	mechanisms such as a "(TABLOCKX HOLDLOCK)" as a locking hint after the 
	table name in the first statement after the transaction is begun or 
	don't support this type of locking at all.  Because of this it isn't 
	really possible to make the update atomic, in particular for the cleanup 
	operation we rely on the caller to perform it at startup before anyone 
	else accesses the certificate store.  The fact that the update isn't 
	quite atomic isn't really a major problem, at worst it'll result in 
	either an expired certificate being visible or a leftover request 
	blocking a new request for a split second longer than they should.
	
	An additional feature that we could make use of for CA operations is the 
	use of foreign keys to ensure referential integrity, usually via entries 
	in the certificate log.  
	
	It's not clear though what the primary key (or keys in different tables) 
	should be.  We can't use certificates.certID as a foreign key for CRLs 
	because a general-purpose certificate keyset (rather than specifically a 
	CA certificate store) can store any CRLs and not just ones for 
	certificates in the keyset.  In addition we want to return the CRL entry 
	even if the certificate is deleted (in fact the creation of the CRL 
	entry implies the deletion of the corresponding certificate entry).  The 
	certificate store isn't something like a customer database where 
	everything is tied to a unique customer ID but more a transactional 
	system where an object appearing in one table requires the deletion of a 
	corresponding object in another table (e.g. certificate request -> 
	certificate, revocation request -> CRL + certificate deletion) which 
	can't be easily handled with something as simple as a foreign key but 
	has to be done with transactions.
	
	We could in theory require that all certificate requests be authorised 
	by adding an authCertID column to the certReq table and constraining it 
	with:

		FOREIGN KEY (authCertID) REFERENCES certLog.reqCertID

	but even then (apart from the overhead of adding extra indexed columns 
	just to ensure referential integrity) the syntax for this varies 
	somewhat between vendors so that it'd require assorted rewriting by the 
	back-end glue code to handle the different requirements for each 
	database type.  In addition since the foreign key constraint is 
	specified at table create time we could experience strange failures on 
	table creation requiring special-purpose workarounds where we remove the 
	foreign-key constraint in the hope that the table create then succeeds.

	An easier way to handle this is via manual references to entries in the 
	certificate log.  Since this is append-only, a manual presence check can 
	never return an incorrect result (an entry can't be removed between time 
	of check and time of use) so this provides the same result as using 
	referential integrity mechanisms.

	Another database feature that we could use is database triggers as a 
	backup for the access control settings.  For example (using one 
	particular SQL dialect) we could say:

		CREATE TRIGGER checkLog ON certLog FOR UPDATE, DELETE AS
			BEGIN
				ROLLBACK
			END

	However as the "dialect" reference in the above comment implies this 
	process is *extremely* back-end specific (far more so than access 
	controls and the use of foreign keys) so we can't really do much here 
	without ending up having to set different triggers for each back-end 
	type and even back-end version.

	A final feature that we could in theory use is the ability to impose 
	uniqueness constraints based on more than one column, for example the 
	certLog 'action' and 'certID' could be handled using a constraint of:

		UNIQUE (action, certID)

	however this isn't desirable for all combinations of (action, certID) 
	but only some, so that just the certID by itself must be unique in most 
	cases except for one or two where it's sufficient that (action, certID) 
	be unique.  Because of this, as with foreign keys, we need to enforce 
	the constraint via transactions, which gives us more control over how 
	things are handled */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Set up key ID information for a query.  There are two variations of
   this, makeKeyID() encodes an existing keyID value and getKeyID() reads an
   attribute from an object and encodes it using makeKeyID() */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int makeKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
			   IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
			   OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
			   IN_KEYID const CRYPT_KEYID_TYPE iDtype, 
			   IN_BUFFER( idValueLength ) const void *idValue, 
			   IN_LENGTH_SHORT const int idValueLength )
	{
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int idLength = idValueLength, status;

	assert( isWritePtr( keyID, keyIdMaxLen ) );
	assert( isWritePtr( keyIdLen, sizeof( int ) ) );
	assert( isReadPtr( keyID, idValueLength ) );

	REQUIRES( keyIdMaxLen >= 16 && keyIdMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( iDtype == CRYPT_KEYID_NAME || \
			  iDtype == CRYPT_KEYID_URI || \
			  iDtype == CRYPT_IKEYID_KEYID || \
			  iDtype == CRYPT_IKEYID_ISSUERID || \
			  iDtype == CRYPT_IKEYID_CERTID );
	REQUIRES( idValueLength > 0 && idValueLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( keyID, 0, min( 16, keyIdMaxLen ) );
	*keyIdLen = 0;

	/* Name and email address are used as is */
	if( iDtype == CRYPT_KEYID_NAME || iDtype == CRYPT_KEYID_URI )
		{
		if( idLength > CRYPT_MAX_TEXTSIZE * 2 )
			{
			/* Truncate to the database column size */
			idLength = CRYPT_MAX_TEXTSIZE * 2;
			}
		if( idLength > keyIdMaxLen )
			{
			/* Truncate to the output buffer size */
			idLength = keyIdMaxLen;
			}
		memcpy( keyID, idValue, idLength );
		if( iDtype == CRYPT_KEYID_URI )
			{
			int i;

			/* Force the search URI to lowercase to make case-insensitive 
			   matching easier.  In most cases we could ask the back-end to 
			   do this but this complicates indexing and there's no reason 
			   why we can't do it here */
			for( i = 0; i < idLength; i++ )
				keyID[ i ] = intToByte( toLower( keyID[ i ] ) );
			}
		*keyIdLen = idLength;

		return( CRYPT_OK );
		}

	/* A keyID is just a subjectKeyIdentifier, which is already supposed to 
	   be an SHA-1 hash but which in practice can be almost anything so we
	   always hash it to a fixed-length value */
	if( iDtype == CRYPT_IKEYID_KEYID )
		{
		HASHFUNCTION_ATOMIC hashFunctionAtomic;

		/* Get the hash algorithm information and hash the keyID to get
		   the fixed-length keyID */
		getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
								 NULL );
		hashFunctionAtomic( hashBuffer, CRYPT_MAX_HASHSIZE,
							idValue, idValueLength );
		idValue = hashBuffer;
		idLength = DBXKEYID_SIZE;
		}

	ENSURES( idLength >= DBXKEYID_SIZE && idLength <= keyIdMaxLen );

	/* base64-encode the key ID so that we can use it with database queries.
	   Since we only store 128 bits of a (usually 160 bit) ID to save space
	   (particularly where it's used in indices) and to speed lookups, this
	   encoding step has the side-effect of truncating the ID down to the
	   correct size */
	status = base64encode( keyID, keyIdMaxLen, keyIdLen, idValue, 
						   DBXKEYID_SIZE, CRYPT_CERTTYPE_NONE );
	if( cryptStatusError( status ) )
		retIntError();
	ENSURES( *keyIdLen == ENCODED_DBXKEYID_SIZE );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
			  IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
			  OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
			  IN_HANDLE const CRYPT_HANDLE iCryptHandle,
			  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE keyIDtype )
	{
	BYTE hashBuffer[ CRYPT_MAX_HASHSIZE + 8 ];
	int status;

	assert( isWritePtr( keyID, keyIdMaxLen ) );
	assert( isWritePtr( keyIdLen, sizeof( int ) ) );

	REQUIRES( keyIdMaxLen >= 16 && keyIdMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptHandle ) );
	REQUIRES( ( keyIDtype == CRYPT_CERTINFO_FINGERPRINT_SHA1 || \
				keyIDtype == CRYPT_IATTRIBUTE_AUTHCERTID ) || \
			  ( keyIDtype == CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER || \
				keyIDtype == CRYPT_IATTRIBUTE_ISSUER || \
				keyIDtype == CRYPT_IATTRIBUTE_SUBJECT || \
				keyIDtype == CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER || \
				keyIDtype == CRYPT_IATTRIBUTE_SPKI ) );

	/* Clear return values */
	memset( keyID, 0, min( 16, keyIdMaxLen ) );
	*keyIdLen = 0;

	/* Get the attribute from the certificate and hash it, unless it's 
	   already a hash */
	if( keyIDtype == CRYPT_CERTINFO_FINGERPRINT_SHA1 || \
		keyIDtype == CRYPT_IATTRIBUTE_AUTHCERTID )
		{
		MESSAGE_DATA msgData;

		setMessageData( &msgData, hashBuffer, CRYPT_MAX_HASHSIZE );
		status = krnlSendMessage( iCryptHandle, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, keyIDtype );
		if( cryptStatusError( status ) )
			return( status );
		ENSURES( msgData.length == KEYID_SIZE );
		}
	else
		{
		DYNBUF idDB;
		HASHFUNCTION_ATOMIC hashFunctionAtomic;
		int hashSize;

		/* Get the attribute data and hash it to get the ID */
		status = dynCreate( &idDB, iCryptHandle, keyIDtype );
		if( cryptStatusError( status ) )
			return( status );
		getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
								 &hashSize );
		hashFunctionAtomic( hashBuffer, CRYPT_MAX_HASHSIZE,
							dynData( idDB ), dynLength( idDB ) );
		ENSURES( hashSize == KEYID_SIZE );
		dynDestroy( &idDB );
		}

	return( makeKeyID( keyID, keyIdMaxLen, keyIdLen, CRYPT_IKEYID_CERTID, 
					   hashBuffer, KEYID_SIZE ) );
	}

/* Get a keyID for a certificate and pkiUser */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getCertKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
				  IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
				  OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
				  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert )
	{
	int status;

	assert( isWritePtr( keyID, keyIdMaxLen ) );
	assert( isWritePtr( keyIdLen, sizeof( int ) ) );

	REQUIRES( keyIdMaxLen >= 16 && keyIdMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* Clear return values */
	memset( keyID, 0, min( 16, keyIdMaxLen ) );
	*keyIdLen = 0;

	/* Certificate keyID handling isn't quite as simple as just reading an
	   attribute from the certificate since the subjectKeyIdentifier (if
	   present) may not be the same as the keyID if the certificate has come 
	   from a CA that does strange things with the sKID.  To resolve this we 
	   try and build the key ID from the sKID, if this isn't present we use 
	   the keyID (the sKID may have a nonstandard length since it's possible 
	   to stuff anything in there, getKeyID() will hash it to the standard 
	   size if the length is wrong) */
	status = getKeyID( keyID, keyIdMaxLen, keyIdLen, iCryptCert,
					   CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
	if( cryptStatusOK( status ) )
		return( status );

	/* There's no subjectKeyIdentifier, use the keyID.  Note that we can't
	   just read the CRYPT_IATTRIBUTE_KEYID attribute directly since this
	   may be a data-only certificate (either a standalone certificate or 
	   one from the middle of a chain) so we have to generate it indirectly 
	   by hashing the SubjectPublicKeyInfo, which is equivalent to the keyID 
	   and is always present in a certificate */
	return( getKeyID( keyID, keyIdMaxLen, keyIdLen, iCryptCert, 
					  CRYPT_IATTRIBUTE_SPKI ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int getPkiUserKeyID( OUT_BUFFER( keyIdMaxLen, *keyIdLen ) char *keyID, 
					 IN_LENGTH_SHORT_MIN( 16 ) const int keyIdMaxLen, 
					 OUT_LENGTH_BOUNDED_Z( keyIdMaxLen ) int *keyIdLen,
					 IN_HANDLE const CRYPT_CERTIFICATE iCryptCert )
	{
	MESSAGE_DATA msgData;
	BYTE binaryKeyID[ 64 + 8 ];
	char encKeyID[ CRYPT_MAX_TEXTSIZE + 8 ];
	int binaryKeyIDlength DUMMY_INIT, status;

	assert( isWritePtr( keyID, keyIdMaxLen ) );
	assert( isWritePtr( keyIdLen, sizeof( int ) ) );

	REQUIRES( keyIdMaxLen >= 16 && keyIdMaxLen < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iCryptCert ) );

	/* Clear return values */
	memset( keyID, 0, min( 16, keyIdMaxLen ) );
	*keyIdLen = 0;

	/* Get the keyID as the PKI user ID.  We can't read this directly since 
	   it's returned in text form for use by end users so we have to read 
	   the encoded form, decode it, and then turn the decoded binary value 
	   into a key ID.  We identify the result as a keyID, 
	   (== subjectKeyIdentifier, which it isn't really) but we need to use 
	   this to ensure that it's hashed/expanded out to the correct size */
	setMessageData( &msgData, encKeyID, CRYPT_MAX_TEXTSIZE );
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_PKIUSER_ID );
	if( cryptStatusOK( status ) )
		status =  decodePKIUserValue( binaryKeyID, 64, &binaryKeyIDlength, 
									  encKeyID, msgData.length );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( binaryKeyIDlength > 0 && \
				   binaryKeyIDlength < MAX_INTLENGTH_SHORT );
	return( makeKeyID( keyID, keyIdMaxLen, keyIdLen, CRYPT_IKEYID_KEYID, 
					   binaryKeyID, binaryKeyIDlength ) );
	}

/* Extract certificate data from a certificate object.  Note that the 
   formatType is given as an int rather than an enumerated type because
   it can be either a CRYPT_CERTFORMAT_TYPE or a CRYPT_ATTRIBUTE_TYPE */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int extractCertData( IN_HANDLE const CRYPT_CERTIFICATE iCryptCert, 
					 IN_INT const int formatType,
					 OUT_BUFFER( certDataMaxLength, *certDataLength ) \
						void *certDataBuffer, 
					 IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
						const int certDataMaxLength, 
					 OUT_LENGTH_BOUNDED_Z( certDataMaxLength ) \
						int *certDataLength )
	{
	MESSAGE_DATA msgData;
	int status;

	assert( isWritePtr( certDataBuffer, certDataMaxLength ) );
	assert( isWritePtr( certDataLength, sizeof( int ) ) );

	REQUIRES( isHandleRangeValid( iCryptCert ) );
	REQUIRES( formatType == CRYPT_CERTFORMAT_CERTIFICATE || \
			  formatType == CRYPT_ICERTFORMAT_DATA || \
			  formatType == CRYPT_IATTRIBUTE_CRLENTRY );
	REQUIRES( certDataMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  certDataMaxLength < MAX_INTLENGTH_SHORT );

	/* Make sure that there's no collision in format type values (although 
	   the switch() statement will also catch this by producing a compile 
	   error */
	static_assert( CRYPT_CERTFORMAT_CERTIFICATE != CRYPT_ICERTFORMAT_DATA && \
				   CRYPT_CERTFORMAT_CERTIFICATE != CRYPT_IATTRIBUTE_CRLENTRY, \
				   "Format type collision" );

	/* Clear return values */
	memset( certDataBuffer, 0, min( 16, certDataMaxLength ) );
	*certDataLength = 0;

	/* Extract the certificate object data */
	setMessageData( &msgData, certDataBuffer, certDataMaxLength );
	switch( formatType )
		{
		case CRYPT_CERTFORMAT_CERTIFICATE:
		case CRYPT_ICERTFORMAT_DATA:
			status = krnlSendMessage( iCryptCert, IMESSAGE_CRT_EXPORT,
									  &msgData, formatType );
			break;

		case CRYPT_IATTRIBUTE_CRLENTRY:
			status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S, 
									  &msgData, formatType );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	*certDataLength = msgData.length;

	return( CRYPT_OK );
	}

/* Some internal actions set extended error codes as a result of their 
   operation that the user shouldn't really see.  For example performing a 
   certificate cleanup will return a no-data-found error once the last 
   certificate is reached, which will be read by the user the next time that 
   they read the CRYPT_ATTRIBUTE_INT_ERRORCODE/CRYPT_ATTRIBUTE_INT_ERRORMESSAGE 
   even though the error came from a previous internal operation.  To avoid
   this problem we clean up the error status information when it's been set 
   by an internal operation */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int resetErrorInfo( INOUT DBMS_INFO *dbmsInfo )
	{
	DBMS_STATE_INFO *dbmsStateInfo = dbmsInfo->stateInfo;
	ERROR_INFO *errorInfo = &dbmsStateInfo->errorInfo;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	memset( errorInfo, 0, sizeof( ERROR_INFO ) );

	return( CRYPT_OK );
	}

/* Get names for various items */

CHECK_RETVAL_PTR \
char *getKeyName( IN_KEYID const CRYPT_KEYID_TYPE keyIDtype )
	{
	REQUIRES_N( keyIDtype > CRYPT_KEYID_NONE && \
				keyIDtype < CRYPT_KEYID_LAST );

	switch( keyIDtype )
		{
		case CRYPT_KEYID_NAME:
			return( "CN" );

		case CRYPT_KEYID_URI:
			return( "email" );

		case CRYPT_IKEYID_KEYID:
			return( "keyID" );

		case CRYPT_IKEYID_SUBJECTID:
			return( "nameID" );

		case CRYPT_IKEYID_ISSUERID:
			return( "issuerID" );

		case CRYPT_IKEYID_CERTID:
			return( "certID" );
		}

	retIntError_Null();
	}

/****************************************************************************
*																			*
*							Database Access Functions						*
*																			*
****************************************************************************/

/* Create a new key database */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int createDatabase( INOUT DBMS_INFO *dbmsInfo, 
						   const BOOLEAN hasPermissions, 
						   INOUT ERROR_INFO *errorInfo )
	{
	int updateProgress = 0, status;

	assert( isWritePtr( dbmsInfo, sizeof( DBMS_INFO ) ) );

	REQUIRES( errorInfo != NULL );

	/* Create tables for certificates, CRLs, certificate requests, PKI 
	   users, and CA logs.  We use CHAR rather than VARCHAR for the ID 
	   fields since these always have a fixed length and CHAR is faster than 
	   VARCHAR.  In addition we make as many columns as possible NOT NULL 
	   since these fields should always be present and because this is  
	   faster for most databases.  The BLOB type is nonstandard, this is 
	   rewritten by the database interface layer to the type which is 
	   appropriate for the database */
	status = dbmsStaticUpdate(
			"CREATE TABLE certificates ("
				"C CHAR(2), "
				"SP VARCHAR(64), "
				"L VARCHAR(64), "
				"O VARCHAR(64), "
				"OU VARCHAR(64), "
				"CN VARCHAR(64), "
				"email VARCHAR(64), "
				"validTo DATETIME NOT NULL, "
				"nameID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"issuerID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"keyID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certData BLOB NOT NULL)" );
	if( cryptStatusError( status ) )
		{
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Couldn't create certificate database: " ) );
		}
	if( isCertStore( dbmsInfo ) )
		{
		/* The certificate store contains in addition to the other CRL 
		   fields the certificate expiry time which is used to remove the 
		   entry from the CRL table once the certificate has expired anyway, 
		   the nameID which is used to force clustering of entries for each 
		   CA, and the ID of the certificate being revoked, which isn't 
		   available if we're creating it from a raw CRL */
		status = dbmsStaticUpdate(
			"CREATE TABLE CRLs ("
				"expiryDate DATETIME NOT NULL, "
				"nameID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"issuerID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL,"
				"certID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certData BLOB NOT NULL)" );
		if( cryptStatusOK( status ) )
			{
			updateProgress++;
			status = dbmsStaticUpdate(
			"CREATE TABLE pkiUsers ("
				"C CHAR(2), "
				"SP VARCHAR(64), "
				"L VARCHAR(64), "
				"O VARCHAR(64), "
				"OU VARCHAR(64), "
				"CN VARCHAR(64), "
				"nameID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"keyID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certData BLOB NOT NULL)" );
			}
		if( cryptStatusOK( status ) )
			{
			updateProgress++;
			status = dbmsStaticUpdate(
			"CREATE TABLE certRequests ("
				"type SMALLINT NOT NULL, "
				"C CHAR(2), "
				"SP VARCHAR(64), "
				"L VARCHAR(64), "
				"O VARCHAR(64), "
				"OU VARCHAR(64), "
				"CN VARCHAR(64), "
				"email VARCHAR(64), "
				"certID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"certData BLOB NOT NULL)" );
			}
		if( cryptStatusOK( status ) )
			{
			updateProgress++;
			status = dbmsStaticUpdate(
			"CREATE TABLE certLog ("
				"action SMALLINT NOT NULL, "
				"actionTime DATETIME NOT NULL, "
				"certID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL, "
				"reqCertID CHAR(" TEXT_DBXKEYID_SIZE "), "
				"subjCertID CHAR(" TEXT_DBXKEYID_SIZE "), "
				"certData BLOB)" );
			}
		}
	else
		{
		status = dbmsStaticUpdate(
			"CREATE TABLE CRLs ("
				"issuerID CHAR(" TEXT_DBXKEYID_SIZE ") NOT NULL,"
				"certData BLOB NOT NULL)" );
		}
	if( cryptStatusError( status ) )
		{
		/* Undo the previous table creations */
		dbmsStaticUpdate( "DROP TABLE certificates" );
		if( updateProgress > 0 )
			dbmsStaticUpdate( "DROP TABLE CRLs" );
		if( updateProgress > 1 )
			dbmsStaticUpdate( "DROP TABLE pkiUsers" );
		if( updateProgress > 2 )
			dbmsStaticUpdate( "DROP TABLE certRequests" );
		retExtErr( status, 
				   ( status, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Couldn't create certificate%s database: ",
					 isCertStore( dbmsInfo ) ? " store" : "" ) );
		}

	/* Create an index for the email address, nameID, issuerID, keyID, and
	   certID in the certificates table, the issuerID and certID in the CRLs
	   table (the CRL nameID isn't indexed since we only use it for linear
	   scans), the nameID and keyID in the PKI users table (the former isn't 
	   used but is made a UNIQUE INDEX to ensure that the same entry can't 
	   be added more than once) and the certID in the certificate log (this 
	   also isn't used but is made a UNIQUE INDEX to ensure that the same 
	   entry can't be added more than once).  In theory we could force the 
	   UNIQUE constraint on the column rather than indirectly via an index 
	   but some databases will quietly create a unique index anyway in order 
	   to do that so we make the creation of the index explicit rather than 
	   leaving it as a maybe/maybe not option for the database. + CRLF
	   
	   We have to give these unique names since some databases don't allow 
	   two indexes to have the same name even if they're in a different 
	   table.  Since most of the fields in the tables are supposed to be 
	   unique we can specify this for the indexes that we're creating, 
	   however we can't do it for the email address or the nameID in the 
	   certificates table since there could be multiple certificates present 
	   that differ only in key usage.  We don't index the other tables since 
	   indexes consume space and we don't expect to access any of these 
	   much */
	status = dbmsStaticUpdate(
			"CREATE INDEX emailIdx ON certificates(email)" );
	if( cryptStatusOK( status ) )
		status = dbmsStaticUpdate(
			"CREATE INDEX nameIDIdx ON certificates(nameID)" );
	if( cryptStatusOK( status ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX issuerIDIdx ON certificates(issuerID)" );
	if( cryptStatusOK( status ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX keyIDIdx ON certificates(keyID)" );
	if( cryptStatusOK( status ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX certIDIdx ON certificates(certID)" );
	if( cryptStatusOK( status ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX crlIssuerIDIdx ON CRLs (issuerID)" );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX crlCertIDIdx ON CRLs (certID)" );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX userKeyIDIdx ON pkiUsers (keyID)" );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX userNameIDIdx ON pkiUsers (nameID)" );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		status = dbmsStaticUpdate(
			"CREATE UNIQUE INDEX logCertIDIdx ON certLog (certID)" );
	if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
		{
		char dummyCertID[ ENCODED_DBXKEYID_SIZE + 8 ];
		const int dummyCertIDlength = ENCODED_DBXKEYID_SIZE;

		/* Create a special dummy certID with an out-of-band value to mark
		   the first entry in the log */
		memset( dummyCertID, '-', ENCODED_DBXKEYID_SIZE );

		/* Add the initial log entry recording the creation of the log */
		status = updateCertLog( dbmsInfo, CRYPT_CERTACTION_CREATE,
								dummyCertID, dummyCertIDlength, 
								NULL, 0, NULL, 0, NULL, 0,
								DBMS_UPDATE_NORMAL );
		}
	if( cryptStatusError( status ) )
		{
		/* Undo the creation of the various tables */
		dbmsStaticUpdate( "DROP TABLE certificates" );
		dbmsStaticUpdate( "DROP TABLE CRLs" );
		if( isCertStore( dbmsInfo ) )
			{
			dbmsStaticUpdate( "DROP TABLE pkiUsers" );
			dbmsStaticUpdate( "DROP TABLE certRequests" );
			dbmsStaticUpdate( "DROP TABLE certLog" );
			}
		retExtErr( CRYPT_ERROR_WRITE, 
				   ( CRYPT_ERROR_WRITE, errorInfo, getDbmsErrorInfo( dbmsInfo ),
					 "Couldn't create indexes for certificate%s database: ",
					 isCertStore( dbmsInfo ) ? " store" : "" ) );
		}

	/* If the back-end doesn't support access permissions (generally only 
	   toy ones like Access and Paradox) or it's not a CA certificate 
	   store, we're done */
	if( !hasPermissions || !isCertStore( dbmsInfo ) )
		return( CRYPT_OK );

	/* Set access controls for the certificate store tables:

						Users				CAs
		certRequests:	-					INS,SEL,DEL
		certificates:	SEL					INS,SEL,DEL
		CRLs:			-					INS,SEL,DEL
		pkiUsers:		-					INS,SEL,DEL
		certLog:		-					INS,SEL
	
	   Once role-based access controls are enabled we can allow only the CA 
	   user to update the certstore tables and allow others only read access 
	   to the certificates table.  In addition the revocation should be 
	   phrased as REVOKE ALL, GRANT <permitted> rather than revoking specific 
	   privileges since each database vendor has their own nonstandard 
	   additional privileges that a specific revoke won't cover.  
	   Unfortunately configuring this will be somewhat difficult since it
	   requires that cryptlib users create database user roles, which in turn
	   requires that they read the manual */
#if 1
	dbmsStaticUpdate( "REVOKE UPDATE ON certificates FROM PUBLIC" );
	dbmsStaticUpdate( "REVOKE UPDATE ON CRLs FROM PUBLIC" );
	dbmsStaticUpdate( "REVOKE UPDATE ON pkiUsers FROM PUBLIC" );
	dbmsStaticUpdate( "REVOKE UPDATE ON certRequests FROM PUBLIC" );
	dbmsStaticUpdate( "REVOKE DELETE,UPDATE ON certLog FROM PUBLIC" );
#else
	dbmsStaticUpdate( "REVOKE ALL ON certificates FROM PUBLIC" );
	dbmsStaticUpdate( "GRANT INSERT,SELECT,DELETE ON certificates TO ca" );
	dbmsStaticUpdate( "GRANT SELECT ON certificates TO PUBLIC" );
	dbmsStaticUpdate( "REVOKE ALL ON CRLs FROM PUBLIC" );
	dbmsStaticUpdate( "GRANT INSERT,SELECT,DELETE ON CRLs TO ca" );
	dbmsStaticUpdate( "REVOKE ALL ON pkiUsers FROM PUBLIC" );
	dbmsStaticUpdate( "GRANT INSERT,SELECT,DELETE ON pkiUsers TO ca" );
	dbmsStaticUpdate( "REVOKE ALL ON certRequests FROM PUBLIC" );
	dbmsStaticUpdate( "GRANT INSERT,SELECT,DELETE ON certRequests TO ca" );
	dbmsStaticUpdate( "REVOKE ALL ON certLog FROM PUBLIC" );
	dbmsStaticUpdate( "GRANT INSERT,SELECT ON certLog TO ca" );
#endif /* 1 */

	return( CRYPT_OK );
	}

/* Return status information for the keyset */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isBusyFunction( KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	return( ( keysetInfoPtr->keysetDBMS->flags & \
			  ( DBMS_FLAG_UPDATEACTIVE | DBMS_FLAG_QUERYACTIVE ) ) ? \
			  TRUE : FALSE );
	}

/* Open a connection to a database */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int initFunction( INOUT KEYSET_INFO *keysetInfoPtr, 
						 IN_BUFFER( nameLen ) const char *name,
						 IN_LENGTH_NAME const int nameLen,
						 IN_ENUM( CRYPT_KEYOPT ) const CRYPT_KEYOPT_TYPE options )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;
	DBMS_NAME_INFO nameInfo;
	int featureFlags, status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtr( name, nameLen ) );

	REQUIRES( nameLen >= MIN_NAME_LENGTH && nameLen < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options >= CRYPT_KEYOPT_NONE && options < CRYPT_KEYOPT_LAST );

	/* Parse the data source name.  The parsed form isn't needed at this 
	   point but it allows us to return a more meaningful error response */
	status = dbmsParseName( &nameInfo, name, nameLen );
	if( cryptStatusError( status ) )
		retExt( status, 
				( status, KEYSET_ERRINFO, "Invalid database name" ) );

	/* Perform a database back-end specific open */
	status = dbmsOpen( name, nameLen,
					   ( options == CRYPT_KEYOPT_READONLY ) ? \
							options : CRYPT_KEYOPT_NONE, &featureFlags );
	if( cryptStatusError( status ) )
		{
		DEBUG_PRINT(( "Couldn't open database, message '%s'.",
					  keysetInfoPtr->errorInfo.errorString ));
		endDbxSession( keysetInfoPtr );
		return( status );
		}

	/* If the back-end is read-only (which would be extremely unusual, 
	   usually related to misconfigured DBMS access permissions) and we're 
	   not opening it in read-only mode, signal an error */
	if( ( featureFlags & DBMS_FEATURE_FLAG_READONLY ) && \
		options != CRYPT_KEYOPT_READONLY )
		{
		endDbxSession( keysetInfoPtr );
		retExt( CRYPT_ERROR_PERMISSION, 
				( CRYPT_ERROR_PERMISSION, KEYSET_ERRINFO, 
				  "Certificate database can only be accessed in read-only "
				  "mode" ) );
		}

	/* If we're being asked to create a new database, create it and exit */
	if( options == CRYPT_KEYOPT_CREATE )
		{
		status = createDatabase( dbmsInfo, 
								 ( featureFlags & DBMS_FEATURE_FLAG_PRIVILEGES ) ? \
								   TRUE : FALSE, KEYSET_ERRINFO );
		if( cryptStatusOK( status ) && isCertStore( dbmsInfo ) )
			status = updateCertLog( dbmsInfo, CRYPT_CERTACTION_CONNECT,
									NULL, 0, NULL, 0, NULL, 0, NULL, 0,
									DBMS_UPDATE_NORMAL );
		if( cryptStatusError( status ) )
			{
			dbmsClose();
			endDbxSession( keysetInfoPtr );
			}
		return( status );
		}

	/* Check to see whether it's a certificate store.  We do this by 
	   checking for the presence of the certificate store creation entry in 
	   the log, this is always present with an action value of 
	   CRYPT_CERTACTION_CREATE */
	status = dbmsStaticQuery(
			"SELECT certData FROM certLog WHERE action = "
				TEXT_CERTACTION_CREATE,
							  DBMS_CACHEDQUERY_NONE, DBMS_QUERY_CHECK );
	if( cryptStatusOK( status ) )
		{
		/* It's a certificate store, if we're opening it as a non-certificate 
		   store it has to be in read-only mode.  We return an error rather 
		   than quietly changing the access mode to read-only both to make it 
		   explicit to the user at open time that they can't make changes 
		   and because we need to have the read-only flag set when we open 
		   the database to optimise the buffering and locking strategy, 
		   setting it at this point is too late */
		if( !isCertStore( dbmsInfo ) )
			{
			if( options != CRYPT_KEYOPT_READONLY )
				{
				dbmsClose();
				endDbxSession( keysetInfoPtr );
				retExt( CRYPT_ERROR_PERMISSION, 
						( CRYPT_ERROR_PERMISSION, KEYSET_ERRINFO, 
						  "Certificate store can't be accessed as a normal "
						  "database except in read-only mode" ) );
				}

			/* Remember that even though it's not functioning as a 
			   certificate store we can still perform some extended queries 
			   on it based on fields that are only present in certificate 
			   stores */
			dbmsInfo->flags |= DBMS_FLAG_CERTSTORE_FIELDS;

			return( CRYPT_OK );
			}

		/* If this isn't a read-only open, record a connection to the
		   store */
		if( options != CRYPT_KEYOPT_READONLY )
			{
			status = updateCertLog( dbmsInfo, CRYPT_CERTACTION_CONNECT,
									NULL, 0, NULL, 0, NULL, 0, NULL, 0,
									DBMS_UPDATE_NORMAL );
			if( cryptStatusError( status ) )
				{
				/* This is a critical error, if we can't update the access 
				   log at this point then it's not safe to use the 
				   certificate store (if we fail during a general update 
				   operation we give the caller the option of continuing 
				   since the transaction itself will have been rolled back 
				   so there's no permanent harm done) */
				if( status == CRYPT_ERROR_WRITE )
					{
					/* We'll usually fail at this point with a 
					   CRYPT_ERROR_WRITE due to being unable to update the
					   cetificate store log, but this is rather confusing 
					   for a keyset open so we convert it into a generic
					   CRYPT_ERROR_OPEN */
					status = CRYPT_ERROR_OPEN;
					}
				dbmsClose();
				endDbxSession( keysetInfoPtr );
				retExtArg( status, 
						   ( status, KEYSET_ERRINFO, 
							 "Couldn't update certificate store log" ) );
				}
			}

		return( CRYPT_OK );
		}

	/* It's not a certificate store, if the DBMS information indicates that 
	   we're expecting to open it as one tell the caller */
	if( isCertStore( dbmsInfo ) )
		{
		dbmsClose();
		endDbxSession( keysetInfoPtr );
		retExtArg( CRYPT_ARGERROR_NUM1, 
				   ( CRYPT_ARGERROR_NUM1, KEYSET_ERRINFO, 
					 "Keyset isn't a certificate store" ) );
		}

	/* Since the failure of the query above will set the extended error
	   information we have to explicitly clear it here to avoid making the
	   (invisible) query side-effects visible to the user */
	resetErrorInfo( dbmsInfo );

	return( CRYPT_OK );
	}

/* Close the connection to a database */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int shutdownFunction( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	/* If it's a certificate store opened in read/write mode, record a 
	   closed connection to the store */
	if( isCertStore( dbmsInfo ) && \
		keysetInfoPtr->options != CRYPT_KEYOPT_READONLY )
		{
		updateCertLog( dbmsInfo, CRYPT_CERTACTION_DISCONNECT, 
					   NULL, 0, NULL, 0, NULL, 0, NULL, 0, 
					   DBMS_UPDATE_NORMAL );
		}

	/* If we're in the middle of a query, cancel it.  We always use 
	   DBMS_CACHEDQUERY_NONE because this is the only query type that can
	   remain active outside the keyset object */
	if( dbmsInfo->flags & DBMS_FLAG_QUERYACTIVE )
		dbmsStaticQuery( NULL, DBMS_CACHEDQUERY_NONE, DBMS_QUERY_CANCEL );

	dbmsClose();
	return( endDbxSession( keysetInfoPtr ) );
	}

/****************************************************************************
*																			*
*							Database Access Routines						*
*																			*
****************************************************************************/

/* Set up the function pointers to the keyset methods */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setAccessMethodDBMS( INOUT KEYSET_INFO *keysetInfoPtr,
						 IN_ENUM( CRYPT_KEYSET ) const CRYPT_KEYSET_TYPE type )
	{
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	static_assert( DBMS_CACHEDQUERY_LAST == NO_CACHED_QUERIES, \
				   "Cached query ID" );

	REQUIRES( type > CRYPT_KEYSET_NONE && type < CRYPT_KEYSET_LAST );

	/* Set up the lower-level interface functions */
	status = initDbxSession( keysetInfoPtr, type );
	if( cryptStatusError( status ) )
		return( status );

	/* Set the access method pointers */
	keysetInfoPtr->initFunction = initFunction;
	keysetInfoPtr->shutdownFunction = shutdownFunction;
	status = initDBMSread( keysetInfoPtr );
	if( cryptStatusOK( status ) )
		status = initDBMSwrite( keysetInfoPtr );
	if( cryptStatusError( status ) )
		return( status );
	if( type == CRYPT_KEYSET_ODBC_STORE || \
		type == CRYPT_KEYSET_DATABASE_STORE )
		{
		status = initDBMSCA( keysetInfoPtr );
		if( cryptStatusError( status ) )
			return( status );
		}
	keysetInfoPtr->isBusyFunction = isBusyFunction;

	return( CRYPT_OK );
	}
#endif /* USE_DBMS */
