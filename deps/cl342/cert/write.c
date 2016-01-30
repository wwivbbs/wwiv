/****************************************************************************
*																			*
*							Certificate Write Routines						*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* The X.509 version numbers */

enum { X509VERSION_1, X509VERSION_2, X509VERSION_3 };
enum { X509ACVERSION_1, X509ACVERSION_2 }; 

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )

/* Set/refresh a nonce in an RTCS/OCSP request (difficile est tenere quae 
   acceperis nisi exerceas) */

static int setNonce( INOUT ATTRIBUTE_PTR **attributePtrPtr,
					 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE nonceType )
	{
	ATTRIBUTE_PTR *attributePtr;
	MESSAGE_DATA msgData;
	void *noncePtr;
	int nonceLength, status;

	assert( isWritePtr( attributePtrPtr, sizeof( ATTRIBUTE_PTR * ) ) );

	REQUIRES( nonceType == CRYPT_CERTINFO_CMS_NONCE || \
			  nonceType == CRYPT_CERTINFO_OCSP_NONCE );

	/* To ensure freshness we always use a new nonce when we write an RTCS 
	   or OCSP request */
	attributePtr = findAttributeField( *attributePtrPtr, nonceType,
									   CRYPT_ATTRIBUTE_NONE );
	if( attributePtr == NULL )
		{
		CRYPT_ATTRIBUTE_TYPE dummy1;
		CRYPT_ERRTYPE_TYPE dummy2;
		BYTE nonce[ CRYPT_MAX_HASHSIZE + 8 ];

		/* There's no nonce present, add a new one */
		setMessageData( &msgData, nonce, 16 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_GETATTRIBUTE_S, &msgData,
								  CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusError( status ) )
			return( status );
		return( addAttributeFieldString( attributePtrPtr, nonceType, 
										 CRYPT_ATTRIBUTE_NONE, nonce, 16, 0, 
										 &dummy1, &dummy2 ) );
		}

	/* There's an existing nonce present, refresh it */
	status = getAttributeDataPtr( attributePtr, &noncePtr, &nonceLength );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( nonceLength == 16 );
	setMessageData( &msgData, noncePtr, 16 );
	return( krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S, 
							 &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE ) );
	}
#endif /* USE_CERTREV || USE_CERTVAL */

/****************************************************************************
*																			*
*							Write Certificate Objects						*
*																			*
****************************************************************************/

/* Write certificate information:

	CertificateInfo ::= SEQUENCE {
		version			  [ 0 ]	EXPLICIT INTEGER DEFAULT(0),
		serialNumber			INTEGER,
		signature				AlgorithmIdentifier,
		issuer					Name
		validity				Validity,
		subject					Name,
		subjectPublicKeyInfo	SubjectPublicKeyInfo,
		extensions		  [ 3 ]	Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeCertInfo( INOUT STREAM *stream, 
						  INOUT CERT_INFO *subjectCertInfoPtr,
						  const CERT_INFO *issuerCertInfoPtr,
						  IN_HANDLE const CRYPT_CONTEXT iIssuerCryptContext )
	{
	const CERT_CERT_INFO *certCertInfo = subjectCertInfoPtr->cCertCert;
	int algoIdInfoSize, length, extensionSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );
	
	REQUIRES( isHandleRangeValid( iIssuerCryptContext ) );

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		int isXyzzyCert, dnCheckFlag = PRE_CHECK_DN;

		/* If it's a XYZZY certificate then a complete DN isn't required */
		status = getCertComponent( subjectCertInfoPtr, CRYPT_CERTINFO_XYZZY, 
								   &isXyzzyCert );
		if( cryptStatusOK( status ) && isXyzzyCert )
			dnCheckFlag = PRE_CHECK_DN_PARTIAL;

		status = preEncodeCertificate( subjectCertInfoPtr, issuerCertInfoPtr,
									   PRE_SET_STANDARDATTR | PRE_SET_ISSUERATTR | \
									   PRE_SET_ISSUERDN | PRE_SET_VALIDITYPERIOD );
		if( cryptStatusError( status ) )
			return( status );
		status = preCheckCertificate( subjectCertInfoPtr, issuerCertInfoPtr,
									  PRE_CHECK_SPKI | dnCheckFlag | \
									  PRE_CHECK_ISSUERDN | PRE_CHECK_SERIALNO | \
						( ( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED ) ? \
									  0 : PRE_CHECK_NONSELFSIGNED_DN ),
						( issuerCertInfoPtr->subjectDNptr != NULL ) ? \
									  PRE_FLAG_DN_IN_ISSUERCERT : \
									  PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how the issuer name will be encoded */
	subjectCertInfoPtr->issuerDNsize = \
							( issuerCertInfoPtr->subjectDNptr != NULL ) ? \
							issuerCertInfoPtr->subjectDNsize : \
							sizeofDN( subjectCertInfoPtr->issuerName );
	subjectCertInfoPtr->subjectDNsize = \
							sizeofDN( subjectCertInfoPtr->subjectName );

	/* Determine the size of the certificate information */
	algoIdInfoSize = sizeofContextAlgoID( iIssuerCryptContext, 
										  certCertInfo->hashAlgo );
	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = sizeofInteger( certCertInfo->serialNumber,
							certCertInfo->serialNumberLength ) + \
			 algoIdInfoSize + \
			 subjectCertInfoPtr->issuerDNsize + \
			 sizeofObject( sizeofUTCTime() * 2 ) + \
			 subjectCertInfoPtr->subjectDNsize + \
			 subjectCertInfoPtr->publicKeyInfoSize;
	if( extensionSize > 0 )
		{
		length += sizeofObject( sizeofShortInteger( X509VERSION_3 ) ) + \
				  sizeofObject( sizeofObject( extensionSize ) );
		}

	/* Write the outer SEQUENCE wrapper */
	writeSequence( stream, length );

	/* If there are extensions present, mark this as a v3 certificate */
	if( extensionSize > 0 )
		{
		writeConstructed( stream, sizeofShortInteger( X509VERSION_3 ),
						  CTAG_CE_VERSION );
		writeShortInteger( stream, X509VERSION_3, DEFAULT_TAG );
		}

	/* Write the serial number and signature algorithm identifier */
	writeInteger( stream, certCertInfo->serialNumber,
				  certCertInfo->serialNumberLength, DEFAULT_TAG );
	status = writeContextAlgoID( stream, iIssuerCryptContext,
								 certCertInfo->hashAlgo );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the issuer name, validity period, subject name, and public key
	   information */
	if( issuerCertInfoPtr->subjectDNptr != NULL )
		status = swrite( stream, issuerCertInfoPtr->subjectDNptr,
						 issuerCertInfoPtr->subjectDNsize );
	else
		status = writeDN( stream, subjectCertInfoPtr->issuerName, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	writeSequence( stream, sizeofUTCTime() * 2 );
	writeUTCTime( stream, subjectCertInfoPtr->startTime, DEFAULT_TAG );
	writeUTCTime( stream, subjectCertInfoPtr->endTime, DEFAULT_TAG );
	status = writeDN( stream, subjectCertInfoPtr->subjectName, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		status = swrite( stream, subjectCertInfoPtr->publicKeyInfo,
						 subjectCertInfoPtr->publicKeyInfoSize );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the extensions */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_CERTIFICATE, extensionSize ) );
	}

/* Write attribute certificate information.  There are two variants of this, 
   v1 attributes certificates that were pretty much never used (the fact 
   that no-one had bothered to define any attributes to be used with them
   didn't help here) and v2 attribute certificates that are also almost
   never used but are newer, we write v2 certificates.  The original v1
   attribute certificate format was:

	AttributeCertificateInfo ::= SEQUENCE {
		version					INTEGER DEFAULT(0),
		owner			  [ 1 ]	Name,
		issuer					Name,
		signature				AlgorithmIdentifier,
		serialNumber			INTEGER,
		validity				Validity,
		attributes				SEQUENCE OF Attribute,
		extensions				Extensions OPTIONAL
		} 

   In v2 this changed to:

	AttributeCertificateInfo ::= SEQUENCE {
		version					INTEGER (1),
		holder					SEQUENCE {
			entityNames	  [ 1 ]	SEQUENCE OF {
				entityName[ 4 ]	EXPLICIT Name
								},
							}
		issuer			  [ 0 ]	SEQUENCE {
			issuerNames			SEQUENCE OF {
				issuerName[ 4 ]	EXPLICIT Name
								},
							}
		signature				AlgorithmIdentifier,
		serialNumber			INTEGER,
		validity				SEQUENCE {
			notBefore			GeneralizedTime,
			notAfter			GeneralizedTime
								},
		attributes				SEQUENCE OF Attribute,
		extensions				Extensions OPTIONAL
		} 

   In order to write the issuer and owner/holder DN as GeneralName we encode
   it using the DN choice of a GeneralName with explicit tag 4, see the 
   comments on GeneralName encoding in ext_def.c for an explanation of the
   tagging */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeAttributeCertInfo( INOUT STREAM *stream,
								   INOUT CERT_INFO *subjectCertInfoPtr,
								   const CERT_INFO *issuerCertInfoPtr,
								   IN_HANDLE const CRYPT_CONTEXT iIssuerCryptContext )
	{
	const CERT_CERT_INFO *certCertInfo = subjectCertInfoPtr->cCertCert;
	int algoIdInfoSize, length, extensionSize;
	int issuerNameSize, holderNameSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iIssuerCryptContext ) );

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		status = preEncodeCertificate( subjectCertInfoPtr, issuerCertInfoPtr,
									   PRE_SET_ISSUERDN | PRE_SET_ISSUERATTR | \
									   PRE_SET_VALIDITYPERIOD );
		if( cryptStatusError( status ) )
			return( status );
		status = preCheckCertificate( subjectCertInfoPtr, issuerCertInfoPtr, 
									  PRE_CHECK_DN | PRE_CHECK_ISSUERDN | \
									  PRE_CHECK_SERIALNO | \
						( ( subjectCertInfoPtr->flags & CERT_FLAG_SELFSIGNED ) ? \
									  0 : PRE_CHECK_NONSELFSIGNED_DN ),
						( issuerCertInfoPtr->subjectDNptr != NULL ) ? \
									  PRE_FLAG_DN_IN_ISSUERCERT : \
									  PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how the issuer name will be encoded */
	issuerNameSize = ( issuerCertInfoPtr->subjectDNptr != NULL ) ? \
					 issuerCertInfoPtr->subjectDNsize : \
					 sizeofDN( subjectCertInfoPtr->issuerName );
	holderNameSize = sizeofDN( subjectCertInfoPtr->subjectName );

	/* Determine the size of the certificate information */
	algoIdInfoSize = sizeofContextAlgoID( iIssuerCryptContext, 
										  certCertInfo->hashAlgo );
	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = ( int ) sizeofShortInteger( X509ACVERSION_2 ) + \
			 sizeofObject( sizeofObject( sizeofObject( holderNameSize ) ) ) + \
			 sizeofObject( sizeofObject( sizeofObject( issuerNameSize ) ) ) + \
			 algoIdInfoSize + \
			 sizeofInteger( certCertInfo->serialNumber,
							certCertInfo->serialNumberLength ) + \
			 sizeofObject( sizeofGeneralizedTime() * 2 ) + \
			 sizeofObject( 0 );
	if( extensionSize > 0 )
		length += ( int ) sizeofObject( extensionSize );

	/* Write the outer SEQUENCE wrapper and version */
	writeSequence( stream, length );
	writeShortInteger( stream, X509ACVERSION_2, DEFAULT_TAG );

	/* Write the owner and issuer name */
	writeSequence( stream, sizeofObject( sizeofObject( holderNameSize ) ) );
	writeConstructed( stream, sizeofObject( holderNameSize ), 
					  CTAG_AC_HOLDER_ENTITYNAME );
	writeConstructed( stream, holderNameSize, 4 );
	status = writeDN( stream, subjectCertInfoPtr->subjectName, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		{
		writeConstructed( stream, 
						  sizeofObject( sizeofObject( issuerNameSize ) ), 0 );
		writeSequence( stream, sizeofObject( issuerNameSize ) );
		writeConstructed( stream, issuerNameSize, 4 );
		if( issuerCertInfoPtr->subjectDNptr != NULL )
			status = swrite( stream, issuerCertInfoPtr->subjectDNptr,
							 issuerCertInfoPtr->subjectDNsize );
		else
			status = writeDN( stream, subjectCertInfoPtr->issuerName, DEFAULT_TAG );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Write the signature algorithm identifier, serial number and validity
	   period */
	writeContextAlgoID( stream, iIssuerCryptContext, certCertInfo->hashAlgo );
	writeInteger( stream, certCertInfo->serialNumber,
				  certCertInfo->serialNumberLength, DEFAULT_TAG );
	writeSequence( stream, sizeofGeneralizedTime() * 2 );
	writeGeneralizedTime( stream, subjectCertInfoPtr->startTime, 
						  DEFAULT_TAG );
	status = writeGeneralizedTime( stream, subjectCertInfoPtr->endTime, 
								   DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the attributes */
	status = writeSequence( stream, 0 );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the extensions */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_ATTRIBUTE_CERT, extensionSize ) );
	}

/****************************************************************************
*																			*
*								Write CRL Objects							*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* Write CRL information:

	CRLInfo ::= SEQUENCE {
		version					INTEGER DEFAULT(0),
		signature				AlgorithmIdentifier,
		issuer					Name,
		thisUpdate				UTCTime,
		nextUpdate				UTCTime OPTIONAL,
		revokedCertificates		SEQUENCE OF RevokedCerts,
		extensions		  [ 0 ]	Extensions OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCRLInfo( INOUT STREAM *stream, 
						 INOUT CERT_INFO *subjectCertInfoPtr,
						 IN_OPT const CERT_INFO *issuerCertInfoPtr,
						 IN_HANDLE_OPT const CRYPT_CONTEXT iIssuerCryptContext )
	{
	const CERT_REV_INFO *certRevInfo = subjectCertInfoPtr->cCertRev;
	REVOCATION_INFO *revocationInfo;
	const BOOLEAN isCrlEntry = ( issuerCertInfoPtr == NULL ) ? TRUE : FALSE;
	int length, algoIdInfoSize, extensionSize, revocationInfoLength = 0;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( issuerCertInfoPtr == NULL && \
			  iIssuerCryptContext == CRYPT_UNUSED ) || \
			( isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) && \
			  isHandleRangeValid( iIssuerCryptContext ) ) );

	REQUIRES( ( issuerCertInfoPtr == NULL && \
				iIssuerCryptContext == CRYPT_UNUSED ) || \
			  ( issuerCertInfoPtr != NULL && \
				isHandleRangeValid( iIssuerCryptContext ) ) );

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		if( isCrlEntry )
			{
			status = preEncodeCertificate( subjectCertInfoPtr, NULL,
										   PRE_SET_REVINFO );
			}
		else
			{
			status = preEncodeCertificate( subjectCertInfoPtr, 
										   issuerCertInfoPtr,
										   PRE_SET_ISSUERDN | \
										   PRE_SET_ISSUERATTR | \
										   PRE_SET_REVINFO );
			if( cryptStatusError( status ) )
				return( status );
			status = preCheckCertificate( subjectCertInfoPtr, 
										  issuerCertInfoPtr,
										  PRE_CHECK_ISSUERCERTDN | \
										  PRE_CHECK_ISSUERDN,
										  PRE_FLAG_DN_IN_ISSUERCERT );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Process CRL entries and version information */
	subjectCertInfoPtr->version = \
					( subjectCertInfoPtr->attributes != NULL ) ? 2 : 1;
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 revocationInfo != NULL && iterationCount < FAILSAFE_ITERATIONS_MAX; 
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		const int crlEntrySize = sizeofCRLentry( revocationInfo );

		if( cryptStatusError( crlEntrySize ) )
			return( crlEntrySize );
		revocationInfoLength += crlEntrySize;

		/* If there are per-entry extensions present it's a v2 CRL */
		if( revocationInfo->attributes != NULL )
			subjectCertInfoPtr->version = 2;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );

	/* If we're being asked to write a single CRL entry, we don't try and go
	   any further since the remaining CRL fields (and issuer information) 
	   may not be set up */
	if( isCrlEntry )
		return( writeCRLentry( stream, certRevInfo->currentRevocation ) );

	/* Determine how big the encoded CRL will be */
	algoIdInfoSize = sizeofContextAlgoID( iIssuerCryptContext, 
										  certRevInfo->hashAlgo );
	if( cryptStatusError( algoIdInfoSize ) )
		return( algoIdInfoSize  );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = algoIdInfoSize + \
			 issuerCertInfoPtr->subjectDNsize + \
			 sizeofUTCTime() + \
			 ( ( subjectCertInfoPtr->endTime > MIN_TIME_VALUE ) ? \
				sizeofUTCTime() : 0 ) + \
			 sizeofObject( revocationInfoLength );
	if( extensionSize > 0 )
		{
		length += sizeofShortInteger( X509VERSION_2 ) + \
			 	  sizeofObject( sizeofObject( extensionSize ) );
		}

	/* Write the outer SEQUENCE wrapper */
	writeSequence( stream, length );

	/* If there are extensions present, mark this as a v2 CRL */
	if( extensionSize > 0 )
		writeShortInteger( stream, X509VERSION_2, DEFAULT_TAG );

	/* Write the signature algorithm identifier, issuer name, and CRL time */
	status = writeContextAlgoID( stream, iIssuerCryptContext,
								 certRevInfo->hashAlgo );
	if( cryptStatusError( status ) )
		return( status );
	swrite( stream, issuerCertInfoPtr->subjectDNptr,
			issuerCertInfoPtr->subjectDNsize );
	status = writeUTCTime( stream, subjectCertInfoPtr->startTime, 
						   DEFAULT_TAG );
	if( subjectCertInfoPtr->endTime > MIN_TIME_VALUE )
		status = writeUTCTime( stream, subjectCertInfoPtr->endTime, 
							   DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the SEQUENCE OF revoked certificates wrapper and the revoked
	   certificate information */
	status = writeSequence( stream, revocationInfoLength );
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 cryptStatusOK( status ) && revocationInfo != NULL && \
			 iterationCount < FAILSAFE_ITERATIONS_MAX;
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		status = writeCRLentry( stream, revocationInfo );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );
	ANALYSER_HINT( subjectCertInfoPtr->attributes != NULL );

	/* Write the extensions */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_CRL, extensionSize ) );
	}
#endif /* USE_CERTREV */

/****************************************************************************
*																			*
*						Write Certificate Request Objects					*
*																			*
****************************************************************************/

#ifdef USE_CERTREQ

/* Write certificate request information:

	CertificationRequestInfo ::= SEQUENCE {
		version					INTEGER (0),
		subject					Name,
		subjectPublicKeyInfo	SubjectPublicKeyInfo,
		attributes		  [ 0 ]	SET OF Attribute
		}

   If extensions are present they are encoded as:

	SEQUENCE {							-- Attribute from X.501
		OBJECT IDENTIFIER {pkcs-9 14},	--   type
		SET OF {						--   values
			SEQUENCE OF {				-- ExtensionReq from CMMF draft
				<X.509v3 extensions>
				}
			}
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCertRequestInfo( INOUT STREAM *stream,
								 INOUT CERT_INFO *subjectCertInfoPtr,
								 STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
								 IN_HANDLE const CRYPT_CONTEXT iIssuerCryptContext )
	{
	int length, extensionSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( isHandleRangeValid( iIssuerCryptContext ) );/* Not used here */

	/* Make sure that everything is in order */
	if( sIsNullStream( stream ) )
		{
		status = preCheckCertificate( subjectCertInfoPtr, NULL, 
									  PRE_CHECK_SPKI | PRE_CHECK_DN_PARTIAL,
									  PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded certificate request will be */
	subjectCertInfoPtr->subjectDNsize = \
			sizeofDN( subjectCertInfoPtr->subjectName );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = sizeofShortInteger( 0 ) + \
			 subjectCertInfoPtr->subjectDNsize + \
			 subjectCertInfoPtr->publicKeyInfoSize;
	if( extensionSize > 0 )
		{
		length += sizeofObject( \
					sizeofObject( \
						sizeofOID( OID_PKCS9_EXTREQ ) + \
						sizeofObject( sizeofObject( extensionSize ) ) ) );
		}
	else
		length += ( int ) sizeofObject( 0 );

	/* Write the header, version number, DN, and public key information */
	writeSequence( stream, length );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	status = writeDN( stream, subjectCertInfoPtr->subjectName, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		status = swrite( stream, subjectCertInfoPtr->publicKeyInfo,
						 subjectCertInfoPtr->publicKeyInfoSize );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the attributes.  If there are no attributes we still have to 
	   write an (erroneous) zero-length field */
	if( extensionSize <= 0 )
		return( writeConstructed( stream, 0, CTAG_CR_ATTRIBUTES ) );
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_CERTREQUEST, extensionSize ) );
	}

/* Write CRMF certificate request information:

	CertReq ::= SEQUENCE {
		certReqID				INTEGER (0),
		certTemplate			SEQUENCE {
			validity	  [ 4 ]	SEQUENCE {
				validFrom [ 0 ]	EXPLICIT GeneralizedTime OPTIONAL,
				validTo	  [ 1 ] EXPLICIT GeneralizedTime OPTIONAL
				} OPTIONAL,
			subject		  [ 5 ]	EXPLICIT Name OPTIONAL,
			publicKey	  [ 6 ]	SubjectPublicKeyInfo,
			extensions	  [ 9 ]	SET OF Attribute OPTIONAL
			}
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCrmfRequestInfo( INOUT STREAM *stream,
								 INOUT CERT_INFO *subjectCertInfoPtr,
								 STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
								 IN_HANDLE const CRYPT_CONTEXT iIssuerCryptContext )
	{
	int payloadLength, extensionSize, subjectDNsize = 0, timeSize = 0;
	int status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( isHandleRangeValid( iIssuerCryptContext ) );/* Not used here */

	/* Make sure that everything is in order */
	if( sIsNullStream( stream ) )
		{
		status = preCheckCertificate( subjectCertInfoPtr, NULL, 
									  PRE_CHECK_SPKI | \
							( ( subjectCertInfoPtr->subjectName != NULL ) ? \
									  PRE_CHECK_DN_PARTIAL : 0 ),
									  PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded certificate request will be */
	payloadLength = subjectCertInfoPtr->publicKeyInfoSize;
	if( subjectCertInfoPtr->subjectName != NULL )
		{
		subjectCertInfoPtr->subjectDNsize = subjectDNsize = \
								sizeofDN( subjectCertInfoPtr->subjectName );
		payloadLength += sizeofObject( subjectDNsize );
		}
	if( subjectCertInfoPtr->startTime > MIN_TIME_VALUE )
		timeSize = sizeofObject( sizeofGeneralizedTime() );
	if( subjectCertInfoPtr->endTime > MIN_TIME_VALUE )
		timeSize += sizeofObject( sizeofGeneralizedTime() );
	if( timeSize > 0 ) 
		payloadLength += sizeofObject( timeSize );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	if( extensionSize > 0 )
		payloadLength += sizeofObject( extensionSize );

	/* Write the header, request ID, inner header, DN, and public key */
	writeSequence( stream, sizeofShortInteger( 0 ) + \
				   sizeofObject( payloadLength ) );
	writeShortInteger( stream, 0, DEFAULT_TAG );
	writeSequence( stream, payloadLength );
	if( timeSize > 0 )
		{
		writeConstructed( stream, timeSize, CTAG_CF_VALIDITY );
		if( subjectCertInfoPtr->startTime > MIN_TIME_VALUE )
			{
			writeConstructed( stream, sizeofGeneralizedTime(), 0 );
			writeGeneralizedTime( stream, subjectCertInfoPtr->startTime,
								  DEFAULT_TAG );
			}
		if( subjectCertInfoPtr->endTime > MIN_TIME_VALUE )
			{
			writeConstructed( stream, sizeofGeneralizedTime(), 1 );
			writeGeneralizedTime( stream, subjectCertInfoPtr->endTime,
								  DEFAULT_TAG );
			}
		}
	if( subjectDNsize > 0 )
		{
		writeConstructed( stream, subjectCertInfoPtr->subjectDNsize,
						  CTAG_CF_SUBJECT );
		status = writeDN( stream, subjectCertInfoPtr->subjectName,
						  DEFAULT_TAG );
		if( cryptStatusError( status ) )
			return( status );
		}
	sputc( stream, MAKE_CTAG( CTAG_CF_PUBLICKEY ) );
		   	/* Convert the SPKI SEQUENCE tag to the CRMF alternative */
	swrite( stream, ( BYTE * ) subjectCertInfoPtr->publicKeyInfo + 1,
			subjectCertInfoPtr->publicKeyInfoSize - 1 );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	writeConstructed( stream, extensionSize, CTAG_CF_EXTENSIONS );
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_REQUEST_CERT, extensionSize ) );
	}

/* Write CRMF revocation request information:

	RevDetails ::= SEQUENCE {
		certTemplate			SEQUENCE {
			serialNumber  [ 1 ]	INTEGER,
			issuer		  [ 3 ]	EXPLICIT Name,
			},
		crlEntryDetails			SET OF Attribute
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRevRequestInfo( INOUT STREAM *stream, 
								INOUT CERT_INFO *subjectCertInfoPtr,
								STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
								STDC_UNUSED const CRYPT_CONTEXT iIssuerCryptContext )
	{
	int payloadLength, extensionSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED );

	/* Make sure that everything is in order */
	if( sIsNullStream( stream ) )
		{
		status = preCheckCertificate( subjectCertInfoPtr, NULL, 
									  PRE_CHECK_ISSUERDN | PRE_CHECK_SERIALNO,
									  PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded certificate request will be */
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	payloadLength = sizeofInteger( subjectCertInfoPtr->cCertCert->serialNumber,
								   subjectCertInfoPtr->cCertCert->serialNumberLength ) + \
					sizeofObject( subjectCertInfoPtr->issuerDNsize );
	if( extensionSize > 0 )
		payloadLength += sizeofObject( extensionSize );

	/* Write the header, inner header, serial number and issuer DN */
	writeSequence( stream, sizeofObject( payloadLength ) );
	writeSequence( stream, payloadLength );
	writeInteger( stream, subjectCertInfoPtr->cCertCert->serialNumber,
				  subjectCertInfoPtr->cCertCert->serialNumberLength,
				  CTAG_CF_SERIALNUMBER );
	writeConstructed( stream, subjectCertInfoPtr->issuerDNsize,
					  CTAG_CF_ISSUER );
	status = swrite( stream, subjectCertInfoPtr->issuerDNptr,
					 subjectCertInfoPtr->issuerDNsize );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	writeConstructed( stream, extensionSize, CTAG_CF_EXTENSIONS );
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_REQUEST_REVOCATION, extensionSize ) );
	}
#endif /* USE_CERTREQ */

/****************************************************************************
*																			*
*						Write Validity-checking Objects						*
*																			*
****************************************************************************/

#ifdef USE_CERTVAL

/* Write an RTCS request:

	RTCSRequests ::= SEQUENCE {
		SEQUENCE OF SEQUENCE {
			certHash	OCTET STRING SIZE(20)
			},
		attributes		Attributes OPTIONAL
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRtcsRequestInfo( INOUT STREAM *stream, 
								 INOUT CERT_INFO *subjectCertInfoPtr,
								 STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
								 STDC_UNUSED \
									const CRYPT_CONTEXT iIssuerCryptContext )
	{
	CERT_VAL_INFO *certValInfo = subjectCertInfoPtr->cCertVal;
	VALIDITY_INFO *validityInfo;
	int length, extensionSize, requestInfoLength = 0;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED );

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		/* Generate a fresh nonce for the request */
		status = setNonce( &subjectCertInfoPtr->attributes, 
						   CRYPT_CERTINFO_CMS_NONCE );
		if( cryptStatusError( status ) )
			return( status );

		/* Perform the pre-encoding checks */
		status = preCheckCertificate( subjectCertInfoPtr, NULL, 
									  PRE_CHECK_VALENTRIES, PRE_FLAG_NONE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded RTCS request will be */
	for( validityInfo = certValInfo->validityInfo, iterationCount = 0;
		 validityInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityInfo = validityInfo->next, iterationCount++ )
		{
		const int requestEntrySize = sizeofRtcsRequestEntry( validityInfo );
		
		if( cryptStatusError( requestEntrySize ) )
			return( requestEntrySize );
		requestInfoLength += requestEntrySize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = sizeofObject( requestInfoLength ) + \
			 ( ( extensionSize > 0 ) ? sizeofObject( extensionSize ) : 0 );

	/* Write the outer SEQUENCE wrapper */
	writeSequence( stream, length );

	/* Write the SEQUENCE OF request wrapper and the request information */
	status = writeSequence( stream, requestInfoLength );
	for( validityInfo = certValInfo->validityInfo, iterationCount = 0;
		 cryptStatusOK( status ) && validityInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityInfo = validityInfo->next, iterationCount++ )
		{
		status = writeRtcsRequestEntry( stream, validityInfo );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_RTCS_REQUEST, extensionSize ) );
	}

/* Write an RTCS response:

	RTCSResponse ::= SEQUENCE OF SEQUENCE {
		certHash	OCTET STRING SIZE(20),
		RESPONSEINFO
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeRtcsResponseInfo( INOUT STREAM *stream,
								  INOUT CERT_INFO *subjectCertInfoPtr,
								  STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
								  STDC_UNUSED \
									const CRYPT_CONTEXT iIssuerCryptContext )
	{
	CERT_VAL_INFO *certValInfo = subjectCertInfoPtr->cCertVal;
	VALIDITY_INFO *validityInfo;
	int extensionSize, validityInfoLength = 0, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED );

	/* RTCS can legitimately return an empty response if there's a problem
	   with the responder so we don't require that any responses be present
	   as for CRLs/OCSP */

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		status = preEncodeCertificate( subjectCertInfoPtr, NULL,
									   PRE_SET_VALINFO );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded RTCS response will be */
	for( validityInfo = certValInfo->validityInfo, iterationCount = 0;
		 validityInfo != NULL && iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityInfo = validityInfo->next, iterationCount++ )
		{
		const int responseEntrySize = \
			sizeofRtcsResponseEntry( validityInfo,
				( certValInfo->responseType == RTCSRESPONSE_TYPE_EXTENDED ) ? \
				TRUE : FALSE );

		if( cryptStatusError( responseEntrySize ) )
			return( responseEntrySize );
		validityInfoLength += responseEntrySize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );

	/* Write the SEQUENCE OF status information wrapper and the certificate 
	   status information */
	status = writeSequence( stream, validityInfoLength );
	for( validityInfo = certValInfo->validityInfo, iterationCount = 0;
		 cryptStatusOK( status ) && validityInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 validityInfo = validityInfo->next, iterationCount++ )
		{
		status = writeRtcsResponseEntry( stream, validityInfo,
					( certValInfo->responseType == RTCSRESPONSE_TYPE_EXTENDED ) ? \
					TRUE : FALSE );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_RTCS_RESPONSE, extensionSize ) );
	}
#endif /* USE_CERTVAL */

/****************************************************************************
*																			*
*						Write Revocation-checking Objects					*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* Write an OCSP request:

	OCSPRequest ::= SEQUENCE {				-- Write, v1
		reqName		[1]	EXPLICIT [4] EXPLICIT DirectoryName OPTIONAL,
		reqList			SEQUENCE OF SEQUENCE {
						SEQUENCE {			-- certID
			hashAlgo	AlgorithmIdentifier,
			iNameHash	OCTET STRING,
			iKeyHash	OCTET STRING,
			serialNo	INTEGER
			} }
		}

	OCSPRequest ::= SEQUENCE {				-- Write, v2 (not used)
		version		[0]	EXPLICIT INTEGER (1),
		reqName		[1]	EXPLICIT [4] EXPLICIT DirectoryName OPTIONAL,
		reqList			SEQUENCE OF SEQUENCE {
			certID	[2]	EXPLICIT OCTET STRING	-- Certificate hash
			}
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeOcspRequestInfo( INOUT STREAM *stream, 
								 INOUT CERT_INFO *subjectCertInfoPtr,
								 IN_OPT const CERT_INFO *issuerCertInfoPtr,
								 IN_HANDLE_OPT \
									const CRYPT_CONTEXT iIssuerCryptContext )
	{
	CERT_REV_INFO *certRevInfo = subjectCertInfoPtr->cCertRev;
	REVOCATION_INFO *revocationInfo;
	int length, extensionSize, revocationInfoLength = 0;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iIssuerCryptContext ) );/* Not used here */

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		/* Generate a fresh nonce for the request */
		status = setNonce( &subjectCertInfoPtr->attributes, 
						   CRYPT_CERTINFO_OCSP_NONCE );
		if( cryptStatusError( status ) )
			return( status );

		/* Perform the pre-encoding checks */
		status = preEncodeCertificate( subjectCertInfoPtr, issuerCertInfoPtr, 
									   PRE_SET_REVINFO );
		if( cryptStatusError( status ) )
			return( status );
		if( issuerCertInfoPtr != NULL )
			{
			/* It's a signed request, there has to be an issuer DN present */
			status = preCheckCertificate( subjectCertInfoPtr, 
										  issuerCertInfoPtr, 
										  PRE_CHECK_ISSUERDN | \
										  PRE_CHECK_REVENTRIES,
										  PRE_FLAG_DN_IN_ISSUERCERT );
			}
		else
			{
			status = preCheckCertificate( subjectCertInfoPtr, NULL,
										  PRE_CHECK_REVENTRIES, 
										  PRE_FLAG_NONE );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded OCSP request will be */
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 revocationInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		const int requestEntrySize = sizeofOcspRequestEntry( revocationInfo );

		if( cryptStatusError( requestEntrySize ) )
			return( requestEntrySize );
		revocationInfoLength += requestEntrySize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = ( ( subjectCertInfoPtr->version == 2 ) ? \
				 sizeofObject( sizeofShortInteger( CTAG_OR_VERSION ) ) : 0 ) + \
			 ( ( issuerCertInfoPtr != NULL ) ? \
				 sizeofObject( sizeofObject( issuerCertInfoPtr->subjectDNsize ) ) : 0 ) + \
			 sizeofObject( revocationInfoLength );
	if( extensionSize > 0 )
		length += sizeofObject( sizeofObject( extensionSize ) );

	/* Write the outer SEQUENCE wrapper */
	writeSequence( stream, length );

	/* If we're using v2 identifiers, mark this as a v2 request */
	if( subjectCertInfoPtr->version == 2 )
		{
		writeConstructed( stream, sizeofShortInteger( 1 ), CTAG_OR_VERSION );
		writeShortInteger( stream, 1, DEFAULT_TAG );
		}

	/* If we're signing the request, write the issuer DN as a GeneralName */
	if( issuerCertInfoPtr != NULL )
		{
		writeConstructed( stream,
						  sizeofObject( issuerCertInfoPtr->subjectDNsize ), 1 );
		writeConstructed( stream, issuerCertInfoPtr->subjectDNsize, 4 );
		status = swrite( stream, issuerCertInfoPtr->subjectDNptr,
						 issuerCertInfoPtr->subjectDNsize );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Write the SEQUENCE OF revocation information wrapper and the
	   revocation information */
	status = writeSequence( stream, revocationInfoLength );
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 cryptStatusOK( status ) && revocationInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE;
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		status = writeOcspRequestEntry( stream, revocationInfo );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_OCSP_REQUEST, extensionSize ) );
	}

/* Write an OCSP response:

	OCSPResponse ::= SEQUENCE {
		version		[0]	EXPLICIT INTEGER (1),
		respID		[1]	EXPLICIT Name,
		producedAt		GeneralizedTime,
		responses		SEQUENCE OF Response
		exts		[1]	EXPLICIT Extensions OPTIONAL,
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int writeOcspResponseInfo( INOUT STREAM *stream,
								  INOUT CERT_INFO *subjectCertInfoPtr,
								  const CERT_INFO *issuerCertInfoPtr,
								  IN_HANDLE \
									const CRYPT_CONTEXT iIssuerCryptContext )
	{
	CERT_REV_INFO *certRevInfo = subjectCertInfoPtr->cCertRev;
	REVOCATION_INFO *revocationInfo;
	int length, extensionSize, revocationInfoLength = 0;
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subjectCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iIssuerCryptContext ) );/* Not used here */

	/* Perform any necessary pre-encoding steps */
	if( sIsNullStream( stream ) )
		{
		status = preCheckCertificate( subjectCertInfoPtr, issuerCertInfoPtr,
									  PRE_CHECK_ISSUERDN | \
									  PRE_CHECK_REVENTRIES,
									  PRE_FLAG_DN_IN_ISSUERCERT );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine how big the encoded OCSP response will be */
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 revocationInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		const int responseEntrySize = sizeofOcspResponseEntry( revocationInfo );

		if( cryptStatusError( responseEntrySize ) )
			return( responseEntrySize );
		revocationInfoLength += responseEntrySize;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	extensionSize = sizeofAttributes( subjectCertInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	length = sizeofObject( sizeofShortInteger( CTAG_OP_VERSION ) ) + \
			 sizeofObject( issuerCertInfoPtr->subjectDNsize ) + \
			 sizeofGeneralizedTime() + \
			 sizeofObject( revocationInfoLength );
	if( extensionSize > 0 )
		length += sizeofObject( sizeofObject( extensionSize ) );

	/* Write the outer SEQUENCE wrapper, version, and issuer DN and 
	   producedAt time */
	writeSequence( stream, length );
	writeConstructed( stream, sizeofShortInteger( 1 ), CTAG_OP_VERSION );
	writeShortInteger( stream, 1, DEFAULT_TAG );
	writeConstructed( stream, issuerCertInfoPtr->subjectDNsize, 1 );
	swrite( stream, issuerCertInfoPtr->subjectDNptr,
			issuerCertInfoPtr->subjectDNsize );
	status = writeGeneralizedTime( stream, subjectCertInfoPtr->startTime,
								   DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the SEQUENCE OF revocation information wrapper and the
	   revocation information */
	status = writeSequence( stream, revocationInfoLength );
	for( revocationInfo = certRevInfo->revocations, iterationCount = 0;
		 cryptStatusOK( status ) && revocationInfo != NULL && \
			iterationCount < FAILSAFE_ITERATIONS_LARGE; 
		 revocationInfo = revocationInfo->next, iterationCount++ )
		{
		status = writeOcspResponseEntry( stream, revocationInfo,
										 subjectCertInfoPtr->startTime );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_LARGE );
	if( cryptStatusError( status ) || extensionSize <= 0 )
		return( status );

	/* Write the attributes */
	return( writeAttributes( stream, subjectCertInfoPtr->attributes,
							 CRYPT_CERTTYPE_OCSP_RESPONSE, extensionSize ) );
	}
#endif /* USE_CERTREV */

/****************************************************************************
*																			*
*						Write CMS Attribute Objects							*
*																			*
****************************************************************************/

#ifdef USE_CMSATTR

/* Write CMS attributes */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCmsAttributes( INOUT STREAM *stream, 
							   INOUT CERT_INFO *attributeInfoPtr,
							   STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
							   STDC_UNUSED const CRYPT_CONTEXT iIssuerCryptContext )
	{
	int addDefaultAttributes, attributeSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( attributeInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED );
	REQUIRES( attributeInfoPtr->attributes != NULL );

	status = krnlSendMessage( DEFAULTUSER_OBJECT_HANDLE, 
							  IMESSAGE_GETATTRIBUTE, &addDefaultAttributes,
							  CRYPT_OPTION_CMS_DEFAULTATTRIBUTES );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that there's a hash and content type present */
	if( findAttributeField( attributeInfoPtr->attributes,
							CRYPT_CERTINFO_CMS_MESSAGEDIGEST,
							CRYPT_ATTRIBUTE_NONE ) == NULL )
		{
		setErrorInfo( attributeInfoPtr, CRYPT_CERTINFO_CMS_MESSAGEDIGEST,
					  CRYPT_ERRTYPE_ATTR_ABSENT );
		return( CRYPT_ERROR_INVALID );
		}
	if( !checkAttributePresent( attributeInfoPtr->attributes,
								CRYPT_CERTINFO_CMS_CONTENTTYPE ) )
		{
		const int value = CRYPT_CONTENT_DATA;

		/* If there's no content type and we're not adding it automatically,
		   complain */
		if( !addDefaultAttributes )
			{
			setErrorInfo( attributeInfoPtr, CRYPT_CERTINFO_CMS_CONTENTTYPE,
						  CRYPT_ERRTYPE_ATTR_ABSENT );
			return( CRYPT_ERROR_INVALID );
			}

		/* There's no content type present, treat it as straight data (which
		   means that this is signedData) */
		status = addCertComponent( attributeInfoPtr, 
								   CRYPT_CERTINFO_CMS_CONTENTTYPE, value );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If there's no signing time attribute present and we're adding the
	   default attributes, add it now.  This will usually already have been
	   added by the caller via getReliableTime(), if it hasn't then we
	   default to using the system time source because the signing object
	   isn't available at this point to provide a time source */
	if( addDefaultAttributes && \
		!checkAttributePresent( attributeInfoPtr->attributes,
								CRYPT_CERTINFO_CMS_SIGNINGTIME ) )
		{
		const time_t currentTime = getTime();

		/* If the time is screwed up then we can't provide a signed 
		   indication of the time */
		if( currentTime <= MIN_TIME_VALUE )
			{
			setErrorInfo( attributeInfoPtr, CRYPT_CERTINFO_VALIDFROM,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_NOTINITED );
			}

		status = addCertComponentString( attributeInfoPtr, 
										 CRYPT_CERTINFO_CMS_SIGNINGTIME,
										 &currentTime, sizeof( time_t ) );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Check that the attributes are in order and determine how big the whole
	   mess will be */
	status = checkAttributes( ATTRIBUTE_CMS, attributeInfoPtr->attributes,
							  &attributeInfoPtr->errorLocus,
							  &attributeInfoPtr->errorType );
	if( cryptStatusError( status ) )
		return( status );
	attributeSize = sizeofAttributes( attributeInfoPtr->attributes );
	if( cryptStatusError( attributeSize ) || attributeSize <= 0 )
		return( attributeSize );

	/* Write the attributes */
	return( writeAttributes( stream, attributeInfoPtr->attributes,
							 CRYPT_CERTTYPE_CMS_ATTRIBUTES, attributeSize ) );
	}
#endif /* USE_CMSATTR */

/****************************************************************************
*																			*
*							Write PKI User Objects							*
*																			*
****************************************************************************/

#ifdef USE_PKIUSER

/* Write PKI user information:

	userData ::= SEQUENCE {
		name				Name,			-- Name for CMP
		encAlgo				AlgorithmIdentifier,-- Algo to encrypt authenticators
		encPW				OCTET STRING,	-- Encrypted authenticators
		attributes			Attributes
		} */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 5, 7 ) ) \
static int getPkiUserInfo( INOUT CERT_PKIUSER_INFO *certUserInfo,
						   OUT_BUFFER( maxUserInfoSize, *userInfoSize ) \
								BYTE *userInfo, 
						   IN_LENGTH_SHORT_MIN( 64 ) const int maxUserInfoSize, 
						   OUT_LENGTH_SHORT_Z int *userInfoSize, 
						   OUT_BUFFER( maxAlgoIdSize, *algoIdSize ) BYTE *algoID, 
						   IN_LENGTH_SHORT_MIN( 16 ) const int maxAlgoIdSize, 
						   OUT_LENGTH_SHORT_Z int *algoIdSize )
	{
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	STREAM stream;
	static const int mode = CRYPT_MODE_CFB;	/* enum vs.int */
	int userInfoBufPos = DUMMY_INIT, i, status;

	assert( isWritePtr( certUserInfo, sizeof( CERT_PKIUSER_INFO ) ) );
	assert( isWritePtr( userInfo, maxUserInfoSize ) );
	assert( isWritePtr( userInfoSize, sizeof( int ) ) );
	assert( isWritePtr( algoID, maxAlgoIdSize ) );
	assert( isWritePtr( algoIdSize, sizeof( int ) ) );

	REQUIRES( maxUserInfoSize >= 64 && \
			  maxUserInfoSize < MAX_INTLENGTH_SHORT );
	REQUIRES( maxAlgoIdSize >= 16 && maxAlgoIdSize < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( userInfo, 0, maxUserInfoSize );
	memset( algoID, 0, maxAlgoIdSize );
	*userInfoSize = *algoIdSize = 0;

	/* Create a stream-cipher encryption context and use it to generate the 
	   user passwords.  These aren't encryption keys but just authenticators 
	   used for MACing so we don't go to the usual extremes to protect them.  
	   In addition we can't use the most obvious option for the stream 
	   cipher, RC4, because it may be disabled in some builds.  Instead we 
	   rely on 3DES, which is always available */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_3DES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE,
							  ( MESSAGE_CAST ) &mode, CRYPT_CTXINFO_MODE );
	if( cryptStatusOK( status ) )
		status = krnlSendNotifier( iCryptContext, IMESSAGE_CTX_GENKEY );
	if( cryptStatusOK( status ) )
		status = krnlSendNotifier( iCryptContext, IMESSAGE_CTX_GENIV );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Create the PKI user authenticators */
	memset( certUserInfo->pkiIssuePW, 0, PKIUSER_AUTHENTICATOR_SIZE );
	status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
							  certUserInfo->pkiIssuePW, 
							  PKIUSER_AUTHENTICATOR_SIZE );
	if( cryptStatusOK( status ) )
		{
		memset( certUserInfo->pkiRevPW, 0, PKIUSER_AUTHENTICATOR_SIZE );
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT,
								  certUserInfo->pkiRevPW,
								  PKIUSER_AUTHENTICATOR_SIZE );
		}
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Encode the user information so that it can be encrypted */
	sMemOpen( &stream, userInfo, maxUserInfoSize );
	writeSequence( &stream, 2 * sizeofObject( PKIUSER_AUTHENTICATOR_SIZE ) );
	writeOctetString( &stream, certUserInfo->pkiIssuePW,
					  PKIUSER_AUTHENTICATOR_SIZE, DEFAULT_TAG );
	status = writeOctetString( &stream, certUserInfo->pkiRevPW,
							   PKIUSER_AUTHENTICATOR_SIZE, DEFAULT_TAG );
	if( cryptStatusOK( status ) )
		userInfoBufPos = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Encrypt (or at least mask) the user information.  For forwards 
	   compatibility (and because the format requires the use of some form 
	   of encryption when encoding the data) we encrypt the user data, once 
	   user roles are fully implemented this can use the data storage key 
	   associated with the CA user to perform the encryption instead of a 
	   fixed interop key.  This isn't a security issue because the CA 
	   database is assumed to be secure (or at least the CA is in serious 
	   trouble if its database isn't secured), we encrypt because it's 
	   pretty much free and because it doesn't hurt either way.  Most CA 
	   guidelines merely require that the CA protect its user database via 
	   standard (physical/ACL) security measures so this is no less secure 
	   than what's required by various CA guidelines.

	   When we do this for real we probably need an extra level of 
	   indirection to go from the CA secret to the database decryption key 
	   so that we can change the encryption algorithm and so that we don't 
	   have to directly apply the CA's data storage key to the user 
	   database */
	setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_3DES );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iCryptContext = createInfo.cryptHandle;
	setMessageData( &msgData, "interop interop interop ", 24 );
	status = krnlSendMessage( iCryptContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_CTXINFO_KEY );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Add PKCS #5 padding to the end of the user information and encrypt 
	   it */
	REQUIRES( userInfoBufPos + 2 == PKIUSER_ENCR_AUTHENTICATOR_SIZE );
	for( i = 0; i < 2; i++ )
		userInfo[ userInfoBufPos++ ] = 2;
	status = krnlSendNotifier( iCryptContext, IMESSAGE_CTX_GENIV );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iCryptContext, IMESSAGE_CTX_ENCRYPT, 
								  userInfo, userInfoBufPos );
	if( cryptStatusOK( status ) )
		{
		sMemOpen( &stream, algoID, maxAlgoIdSize );
		status = writeCryptContextAlgoID( &stream, iCryptContext );
		if( cryptStatusOK( status ) )
			*algoIdSize = stell( &stream );
		sMemDisconnect( &stream );
		}
	krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );
	*userInfoSize = userInfoBufPos;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writePkiUserInfo( INOUT STREAM *stream, 
							 INOUT CERT_INFO *userInfoPtr,
							 STDC_UNUSED const CERT_INFO *issuerCertInfoPtr,
							 STDC_UNUSED const CRYPT_CONTEXT iIssuerCryptContext )
	{
	CERT_PKIUSER_INFO *certUserInfo = userInfoPtr->cCertUser;
	BYTE userInfo[ 128 + 8 ], algoID[ 128 + 8 ];
	int extensionSize, userInfoSize, algoIdSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( userInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( issuerCertInfoPtr == NULL );
	REQUIRES( iIssuerCryptContext == CRYPT_UNUSED );

	if( sIsNullStream( stream ) )
		{
		CRYPT_ATTRIBUTE_TYPE dummy1;
		CRYPT_ERRTYPE_TYPE dummy2;
		MESSAGE_DATA msgData;
		BYTE keyID[ 16 + 8 ];
		int keyIDlength = DUMMY_INIT;

		/* Generate the key identifier.  Once it's in user-encoded form the
		   full identifier can't quite fit so we adjust the size to the
		   maximum amount that we can encode by creating the encoded form 
		   (which trims the input to fit) and then decoding it again.  This 
		   is necessary because it's also used to locate the user information 
		   in a key store, if we used the un-adjusted form for the key ID then 
		   we couldn't locate the stored user information using the adjusted 
		   form */
		setMessageData( &msgData, keyID, 16 );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_GETATTRIBUTE_S,
								  &msgData, CRYPT_IATTRIBUTE_RANDOM_NONCE );
		if( cryptStatusOK( status ) )
			{
			char encodedKeyID[ 32 + 8 ];
			int encKeyIdSize;

			status = encodePKIUserValue( encodedKeyID, 32, &encKeyIdSize,
										 keyID, 16, 3 );
			if( cryptStatusOK( status ) )
				status = decodePKIUserValue( keyID, 16, &keyIDlength,
											 encodedKeyID, encKeyIdSize );
			}
		if( cryptStatusError( status ) )
			return( status );
		status = addAttributeFieldString( &userInfoPtr->attributes,
										  CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER,
										  CRYPT_ATTRIBUTE_NONE, keyID, 
										  keyIDlength, 0, &dummy1, &dummy2 );
		if( cryptStatusOK( status ) )
			{
			status = checkAttributes( ATTRIBUTE_CERTIFICATE,
									  userInfoPtr->attributes,
									  &userInfoPtr->errorLocus,
									  &userInfoPtr->errorType );
			}
		if( cryptStatusError( status ) )
			return( status );

		/* We can't generate the user information yet since we're doing the 
		   pre-encoding pass and writing to a null stream so we leave it for 
		   the actual encoding pass and only provide a size estimate for 
		   now */
		userInfoSize = PKIUSER_ENCR_AUTHENTICATOR_SIZE;

		/* Since we can't use the CAs data storage key yet we set the 
		   algorithm ID size to the size of the information for the fixed 
		   3DES key */
		algoIdSize = 22;
		}
	else
		{
		status = getPkiUserInfo( certUserInfo, userInfo, 128, &userInfoSize, 
								 algoID, 128, &algoIdSize );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine the size of the user information */
	userInfoPtr->subjectDNsize = sizeofDN( userInfoPtr->subjectName );
	extensionSize = sizeofAttributes( userInfoPtr->attributes );
	if( cryptStatusError( extensionSize ) )
		return( extensionSize );
	ENSURES( extensionSize > 0 && extensionSize < MAX_INTLENGTH_SHORT );

	/* Write the user DN, encrypted user information, and any supplementary 
	   information */
	status = writeDN( stream, userInfoPtr->subjectName, DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );
	swrite( stream, algoID, algoIdSize );
	writeOctetString( stream, userInfo, userInfoSize, DEFAULT_TAG );
	return( writeAttributes( stream, userInfoPtr->attributes,
							 CRYPT_CERTTYPE_PKIUSER, extensionSize ) );
	}
#endif /* USE_PKIUSER */

/****************************************************************************
*																			*
*						Write Function Access Information					*
*																			*
****************************************************************************/

typedef struct {
	const CRYPT_CERTTYPE_TYPE type;
	const WRITECERT_FUNCTION function;
	} CERTWRITE_INFO;
static const CERTWRITE_INFO FAR_BSS certWriteTable[] = {
	{ CRYPT_CERTTYPE_CERTIFICATE, writeCertInfo },
	{ CRYPT_CERTTYPE_CERTCHAIN, writeCertInfo },
	{ CRYPT_CERTTYPE_ATTRIBUTE_CERT, writeAttributeCertInfo },
#ifdef USE_CERTREV
	{ CRYPT_CERTTYPE_CRL, writeCRLInfo },
#endif /* USE_CERTREV */
#ifdef USE_CERTREQ
	{ CRYPT_CERTTYPE_CERTREQUEST, writeCertRequestInfo },
	{ CRYPT_CERTTYPE_REQUEST_CERT, writeCrmfRequestInfo },
	{ CRYPT_CERTTYPE_REQUEST_REVOCATION, writeRevRequestInfo },
#endif /* USE_CERTREQ */
#ifdef USE_CERTVAL
	{ CRYPT_CERTTYPE_RTCS_REQUEST, writeRtcsRequestInfo },
	{ CRYPT_CERTTYPE_RTCS_RESPONSE, writeRtcsResponseInfo },
#endif /* USE_CERTVAL */
#ifdef USE_CERTREV
	{ CRYPT_CERTTYPE_OCSP_REQUEST, writeOcspRequestInfo },
	{ CRYPT_CERTTYPE_OCSP_RESPONSE, writeOcspResponseInfo },
#endif /* USE_CERTREV */
#ifdef USE_CMSATTR
	{ CRYPT_CERTTYPE_CMS_ATTRIBUTES, writeCmsAttributes },
#endif /* USE_CMSATTR */
#ifdef USE_PKIUSER
	{ CRYPT_CERTTYPE_PKIUSER, writePkiUserInfo },
#endif /* USE_PKIUSER */
	{ CRYPT_CERTTYPE_NONE, NULL }, { CRYPT_CERTTYPE_NONE, NULL }
	};

CHECK_RETVAL_PTR \
WRITECERT_FUNCTION getCertWriteFunction( IN_ENUM( CRYPT_CERTTYPE ) \
											const CRYPT_CERTTYPE_TYPE certType )
	{
	int i;

	REQUIRES_N( certType > CRYPT_CERTTYPE_NONE && certType < CRYPT_CERTTYPE_LAST );

	for( i = 0; 
		 certWriteTable[ i ].type != CRYPT_CERTTYPE_NONE && \
			i < FAILSAFE_ARRAYSIZE( certWriteTable, CERTWRITE_INFO ); 
		 i++ )
		{
		if( certWriteTable[ i ].type == certType )
			return( certWriteTable[ i ].function );
		}
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( certWriteTable, CERTWRITE_INFO ) );

	return( NULL );
	}
#endif /* USE_CERTIFICATES */
