/****************************************************************************
*																			*
*						  Certificate Signing Routines						*
*						Copyright Peter Gutmann 1997-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
#else
  #include "cert/cert.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Set the end time for a certificate object.  Because of the Y2038 problem
   we have to be careful about the maximum time value that we allow, if 
   it'll overflow then we clamp it at MAX_TIME_VALUE */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setEndTime( INOUT CERT_INFO *certInfoPtr,
					   IN_RANGE( 1, 20 * 365 ) const int timeInDays )
	{
	time_t timeInSeconds;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( timeInDays >= 1 && timeInDays <= 20 * 365 );

	timeInSeconds = timeInDays * 86400L;
	if( MAX_TIME_VALUE - certInfoPtr->startTime >= timeInSeconds )
		{
		/* The end time is past MAX_TIME_VALUE, clamp it at 
		   MAX_TIME_VALUE */
		certInfoPtr->endTime = MAX_TIME_VALUE;
		}
	else
		{
		certInfoPtr->endTime = certInfoPtr->startTime + timeInSeconds;
		}
	
	return( CRYPT_OK );
	}

/* Recover information normally set up on certificate import.  After 
   signing the certificate the data is present without the certificate 
   having been explicitly imported so we have to go back and perform the 
   actions normally performed on import here.  The subject DN and public key 
   data length was recorded when the certificate data was written but the 
   position of the other elements in the certificate can't be determined 
   until the certificate has been signed */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int getObjectDataPtr( INOUT STREAM *stream,
							 OUT_BUFFER_ALLOC_OPT( *dataLength ) void **dataPtrPtr,
							 OUT_LENGTH_Z int *dataLength )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( dataPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( dataLength, sizeof( int ) ) );

	/* Clear return values */
	*dataPtrPtr = NULL;
	*dataLength = 0;

	/* Get the length of the object.  This guarantees a non-zero length, 
	   finding a length of zero results in a CRYPT_ERROR_BADDATA */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( length > 0 );

	/* Get a reference to the object data */
	status = sMemGetDataBlock( stream, dataPtrPtr, length );
	if( cryptStatusError( status ) )
		return( status );
	*dataLength = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int recoverCertData( INOUT CERT_INFO *certInfoPtr,
							IN_ENUM( CRYPT_CERTTYPE ) \
								const CRYPT_CERTTYPE_TYPE certType, 
							IN_BUFFER( encodedCertDataLength ) \
								const void *encodedCertData,
							IN_LENGTH_SHORT_MIN( 16 ) \
								const int encodedCertDataLength )
	{
	STREAM stream;
	int tag, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtrDynamic( encodedCertData, encodedCertDataLength ) );

	REQUIRES( certType == CRYPT_CERTTYPE_CERTIFICATE || \
			  certType == CRYPT_CERTTYPE_CERTCHAIN || \
			  certType == CRYPT_CERTTYPE_CERTREQUEST || \
			  certType == CRYPT_CERTTYPE_REQUEST_CERT || \
			  certType == CRYPT_CERTTYPE_REQUEST_REVOCATION || \
			  certType == CRYPT_CERTTYPE_CRL || \
			  certType == CRYPT_CERTTYPE_PKIUSER );
	REQUIRES( encodedCertDataLength >= 16 && \
			  encodedCertDataLength < MAX_INTLENGTH_SHORT );

	/* If there's public-key or DN data stored with the certificate, free it 
	   since we now have a copy as part of the encoded certificate.  Since the 
	   publicKeyInfo/subject/issuerDNptr pointer points at the public-key/DN 
	   data, we clear this as well.
	   
	   Revocation requests present a slight exception to this since they 
	   store a subject DN that's not needed for the request itself but is 
	   required by the CMP protocol that they're used with (see the long 
	   comment in copyCertToRevRequest() in comp_cert.c), so we leave the
	   subject DN alone if there's one present and it's a revocation 
	   request */
	if( certInfoPtr->publicKeyData != NULL )
		{
		zeroise( certInfoPtr->publicKeyData, certInfoPtr->publicKeyInfoSize );
		clFree( "recoverCertData", certInfoPtr->publicKeyData );
		certInfoPtr->publicKeyData = certInfoPtr->publicKeyInfo = NULL;
		}
	if( certInfoPtr->subjectDNdata != NULL && \
		certType != CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		zeroise( certInfoPtr->subjectDNdata, certInfoPtr->subjectDNsize );
		clFree( "recoverCertData", certInfoPtr->subjectDNdata );
		certInfoPtr->subjectDNdata = certInfoPtr->subjectDNptr = NULL;
		}
	if( certInfoPtr->issuerDNdata != NULL )
		{
		zeroise( certInfoPtr->issuerDNdata , certInfoPtr->issuerDNsize );
		clFree( "recoverCertData", certInfoPtr->issuerDNdata );
		certInfoPtr->issuerDNdata = certInfoPtr->issuerDNptr = NULL;
		}

	/* If it's a PKCS #10 request parse the encoded form to locate the 
	   subject DN and public key */
	if( certType == CRYPT_CERTTYPE_CERTREQUEST )
		{
		sMemConnect( &stream, encodedCertData, encodedCertDataLength );
		readSequence( &stream, NULL );				/* Outer wrapper */
		readSequence( &stream, NULL );				/* Inner wrapper */
		status = readShortInteger( &stream, NULL );	/* Version */
		if( cryptStatusOK( status ) )
			{
			status = getObjectDataPtr( &stream, &certInfoPtr->subjectDNptr, 
									   &certInfoPtr->subjectDNsize );
			}
		if( cryptStatusOK( status ) )
			status = readUniversal( &stream );
		if( cryptStatusOK( status ) )
			{
			status = getObjectDataPtr( &stream, &certInfoPtr->publicKeyInfo, 
									   &certInfoPtr->publicKeyInfoSize );
			}
		sMemDisconnect( &stream );
		ENSURES( cryptStatusOK( status ) );

		return( CRYPT_OK );
		}

	/* If it's a CRMF request, parse the signed form to locate the start of
	   the encoded DN if there is one (the issuer DN is already set up when
	   the issuer certificate is added) and the public key.  The public key 
	   is actually something of a special case in that in the CRMF/CMP 
	   tradition it has a weird nonstandard tag which means that we can't 
	   directly use it elsewhere as a SubjectPublicKeyInfo blob.  In order 
	   to work around this the code that reads SPKIs allows something other
	   than a plain SEQUENCE for the outer wrapper */
	if( certType == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		sMemConnect( &stream, encodedCertData, encodedCertDataLength );
		readSequence( &stream, NULL );			/* Outer wrapper */
		readSequence( &stream, NULL );
		readUniversal( &stream );				/* Request ID */
		status = readSequence( &stream, NULL );	/* Inner wrapper */
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag == MAKE_CTAG( 4 ) )
			status = readUniversal( &stream );	/* Validity */
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag == MAKE_CTAG( 5 ) )
			{									/* Subj.name wrapper */
			status = readConstructed( &stream, NULL, 5 );
			if( cryptStatusOK( status ) )
				{
				status = getObjectDataPtr( &stream, &certInfoPtr->subjectDNptr, 
										   &certInfoPtr->subjectDNsize );
				}
			ENSURES( cryptStatusOK( status ) );
			status = readUniversal( &stream );	/* Subject name */
			}
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag != MAKE_CTAG( 6 ) )
			status = CRYPT_ERROR_BADDATA;		/* Public key */
		if( !cryptStatusError( status ) )
			{
			status = getObjectDataPtr( &stream, &certInfoPtr->publicKeyInfo, 
									   &certInfoPtr->publicKeyInfoSize );
			}
		ENSURES( cryptStatusOK( status ) );
		sMemDisconnect( &stream );

		return( CRYPT_OK );
		}
	if( certType == CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		sMemConnect( &stream, encodedCertData, encodedCertDataLength );
		readSequence( &stream, NULL );			/* Outer wrapper */
		status = readSequence( &stream, NULL );
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag == MAKE_CTAG_PRIMITIVE( 1 ) )
			status = readUniversal( &stream );	/* Serial number */
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag == MAKE_CTAG( 3 ) )
			{									/* Issuer.name wrapper */
			status = readConstructed( &stream, NULL, 3 );
			if( cryptStatusOK( status ) )
				{
				status = getObjectDataPtr( &stream, &certInfoPtr->issuerDNptr, 
										   &certInfoPtr->issuerDNsize );
				}
			ENSURES( cryptStatusOK( status ) );
			status = readUniversal( &stream );	/* Subject name */
			}
		ENSURES( cryptStatusOK( status ) );
		sMemDisconnect( &stream );

		return( CRYPT_OK );
		}

	/* If it's a CRL, parse the encoded form to locate the start of the 
	   issuer DN */
	if( certType == CRYPT_CERTTYPE_CRL )
		{
		sMemConnect( &stream, encodedCertData, encodedCertDataLength );
		readSequence( &stream, NULL );			/* Outer wrapper */
		status = readSequence( &stream, NULL );
		if( checkStatusPeekTag( &stream, status, tag ) && \
			tag == BER_INTEGER )
			status = readUniversal( &stream );	/* Version */
		if( !cryptStatusError( status ) )
			status = readUniversal( &stream );	/* AlgorithmID */
		if( cryptStatusOK( status ) )
			{
			status = getObjectDataPtr( &stream, &certInfoPtr->issuerDNptr, 
									   &certInfoPtr->issuerDNsize );
			}
		ENSURES( cryptStatusOK( status ) );
		sMemDisconnect( &stream );

		return( CRYPT_OK );
		}

	/* If it's PKI user data, parse the encoded form to locate the start of 
	   the PKI user DN */
	if( certType == CRYPT_CERTTYPE_PKIUSER )
		{
		sMemConnect( &stream, encodedCertData, encodedCertDataLength );
		status = readSequence( &stream, NULL );		/* Wrapper */
		if( cryptStatusOK( status ) )
			{
			status = getObjectDataPtr( &stream, &certInfoPtr->subjectDNptr, 
									   &certInfoPtr->subjectDNsize );
			}
		sMemDisconnect( &stream );
		ENSURES( cryptStatusOK( status ) );

		return( CRYPT_OK );
		}

	ENSURES( certType == CRYPT_CERTTYPE_CERTIFICATE || \
			 certType == CRYPT_CERTTYPE_CERTCHAIN );

	/* It's a certificate, parse the signed form to locate the start of the
	   encoded issuer and subject DN and public key */
	sMemConnect( &stream, encodedCertData, encodedCertDataLength );
	readSequence( &stream, NULL );			/* Outer wrapper */
	status = readSequence( &stream, NULL );	/* Inner wrapper */
	if( checkStatusPeekTag( &stream, status, tag ) && \
		tag == MAKE_CTAG( 0 ) )
		readUniversal( &stream );			/* Version */
	readUniversal( &stream );				/* Serial number */
	status = readUniversal( &stream );		/* Signature algo */
	if( cryptStatusOK( status ) )
		{
		status = getObjectDataPtr( &stream, &certInfoPtr->issuerDNptr, 
								   &certInfoPtr->issuerDNsize );
		}
	ENSURES( cryptStatusOK( status ) );
	readUniversal( &stream );				/* Issuer DN */
	status = readUniversal( &stream );		/* Validity */
	if( cryptStatusOK( status ) )
		{
#ifdef USE_CERTLEVEL_PKIX_FULL
		const int startPos = stell( &stream );
		int length;

		/* Full PKIX allows zero-length DNs, which can't be extracted using
		   getObjectDataPtr().  To deal with this we have to explicitly read
		   the SEQUENCE that encapsulates the DN and, if it's zero-length,
		   call sMemGetDataBlock() with the size of the zero-length object
		   given explicitly */
		status = readSequenceZ( &stream, &length );
		if( cryptStatusOK( status ) )
			{
			sseek( &stream, startPos );
			if( length > 0 )
				{
				status = getObjectDataPtr( &stream, 
										   &certInfoPtr->subjectDNptr, 
										   &certInfoPtr->subjectDNsize );
				}
			else
				{
				certInfoPtr->subjectDNsize = sizeofShortObject( 0 );
				status = sMemGetDataBlock( &stream, 
										   &certInfoPtr->subjectDNptr,
										   certInfoPtr->subjectDNsize );
				}
			}
#else
		status = getObjectDataPtr( &stream, &certInfoPtr->subjectDNptr, 
								   &certInfoPtr->subjectDNsize );
#endif /* USE_CERTLEVEL_PKIX_FULL */
		}
	ENSURES( cryptStatusOK( status ) );
	status = readUniversal( &stream );		/* Subject DN */
	if( cryptStatusOK( status ) )
		{
		status = getObjectDataPtr( &stream, &certInfoPtr->publicKeyInfo, 
								   &certInfoPtr->publicKeyInfoSize );
		}
	ENSURES( cryptStatusOK( status ) );
	sMemDisconnect( &stream );

	/* Since the certificate may be used for public-key operations as soon 
	   as it's signed we have to reconstruct the public-key context and 
	   apply to it the constraints that would be applied on import, the 
	   latter being done implicitly via the MESSAGE_SETDEPENDENT mechanism */
	sMemConnect( &stream, certInfoPtr->publicKeyInfo,
				 certInfoPtr->publicKeyInfoSize );
	status = iCryptReadSubjectPublicKey( &stream,
										 &certInfoPtr->iPubkeyContext,
										 SYSTEM_OBJECT_HANDLE, FALSE );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( certInfoPtr->objectHandle,
							  IMESSAGE_SETDEPENDENT,
							  &certInfoPtr->iPubkeyContext,
							  SETDEP_OPTION_NOINCREF );
	if( cryptStatusOK( status ) )
		CLEAR_FLAG( certInfoPtr->flags, CERT_FLAG_DATAONLY );
	return( status );
	}

/* Check the key being used to sign a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int checkSigningKey( INOUT CERT_INFO *certInfoPtr,
							const CERT_INFO *issuerCertInfoPtr,
							IN_HANDLE const CRYPT_CONTEXT iSignContext,
							const BOOLEAN isCertificate )
	{
	MESSAGE_DATA msgData;
	int complianceLevel, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isCertificate == TRUE || isCertificate == FALSE );

	/* Determine how much checking we need to perform */
	status = krnlSendMessage( certInfoPtr->ownerHandle,
							  IMESSAGE_GETATTRIBUTE, &complianceLevel,
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the signing key is associated with an issuer 
	   certificate rather than some other key-related object like a 
	   certificate request */
	if( issuerCertInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
		issuerCertInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
		return( CRYPT_ARGERROR_VALUE );

	/* Make sure that the signing key is associated with a completed issuer 
	   certificate.  If it's a self-signed certificate then we don't have to 
	   have a completed certificate present because the self-sign operation 
	   hasn't created it yet */
	if( !TEST_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED ) && \
		issuerCertInfoPtr->certificate == NULL )
		return( CRYPT_ARGERROR_VALUE );

	/* If it's a CRL then the signing certificate has to match the 
	   certificate(s) being revoked.  If it's an empty CRL then the issuer DN
	   isn't set (since no certificates have been added to the CRL, there's 
	   no issuer DN to copy over) so we only check this if there are entries
	   present */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CRL && \
		certInfoPtr->issuerDNptr != NULL )
		{
		REQUIRES( issuerCertInfoPtr->subjectDNptr != NULL );

		if( certInfoPtr->issuerDNsize != issuerCertInfoPtr->subjectDNsize || \
			memcmp( certInfoPtr->issuerDNptr, issuerCertInfoPtr->subjectDNptr,
					certInfoPtr->issuerDNsize ) )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_ISSUERNAME,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If it's an OCSP request or response then the signing certificate has 
	   to be valid for signing */
	if( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
		certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
		{
		return( checkKeyUsage( issuerCertInfoPtr, CHECKKEY_FLAG_NONE,
							   KEYUSAGE_SIGN, complianceLevel, 
							   &certInfoPtr->errorLocus,
							   &certInfoPtr->errorType ) );
		}

	/* If it's a non-self-signed object then it must be signed by a CA 
	   certificate */
	if( !TEST_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED ) )
		{
		status = checkKeyUsage( issuerCertInfoPtr, CHECKKEY_FLAG_CA,
								isCertificate ? CRYPT_KEYUSAGE_KEYCERTSIGN : \
												CRYPT_KEYUSAGE_CRLSIGN,
								complianceLevel, &certInfoPtr->errorLocus,
								&certInfoPtr->errorType );
		if( cryptStatusError( status ) && \
			certInfoPtr->errorType == CRYPT_ERRTYPE_CONSTRAINT )
			{
			/* If there was a constraint problem then it's something in the 
			   issuer's certificate rather than the certificate being signed 
			   so we have to change the error type accordingly.  What's 
			   reported isn't strictly accurate since the locus is in the 
			   issuer rather than subject certificate but it's the best that 
			   we can do */
			certInfoPtr->errorType = CRYPT_ERRTYPE_ISSUERCONSTRAINT;
			}

		return( status );
		}

	/* It's a self-signed certificate, the signing key must match the key in 
	   the certificate */
	setMessageData( &msgData, certInfoPtr->publicKeyID, KEYID_SIZE );
	status = krnlSendMessage( iSignContext, IMESSAGE_COMPARE, &msgData,
							  MESSAGE_COMPARE_KEYID );
	if( cryptStatusError( status ) )
		return( CRYPT_ARGERROR_VALUE );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Signing Setup Functions							*
*																			*
****************************************************************************/

/* Copy a signing certificate chain into the certificate being signed */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int copySigningCertChain( INOUT CERT_INFO *certInfoPtr,
								 IN_HANDLE const CRYPT_CONTEXT iSignContext )
	{
	CERT_CERT_INFO *certInfo = certInfoPtr->cCertCert;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( iSignContext ) );

	/* If there's a chain of certificates present (for example from a 
	   previous signing attempt that wasn't completed due to an error), free 
	   them */
	if( certInfo->chainEnd > 0 )
		{
		int i, LOOP_ITERATOR;
			
		LOOP_EXT( i = 0, i < certInfo->chainEnd, i++, MAX_CHAINLENGTH )
			krnlSendNotifier( certInfo->chain[ i ], IMESSAGE_DECREFCOUNT );
		ENSURES( LOOP_BOUND_OK );
		certInfo->chainEnd = 0;
		}

	/* If it's a self-signed certificate then it must be the only one in the 
	   chain (creating a chain like this doesn't make much sense but we 
	   handle it anyway) */
	if( TEST_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED ) )
		{
		if( certInfo->chainEnd > 0 )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CERTIFICATE,
						  CRYPT_ERRTYPE_ATTR_PRESENT );
			return( CRYPT_ERROR_INVALID );
			}
		
		return( CRYPT_OK );
		}

	/* Copy the certificate chain into the certificate to be signed */
	return( copyCertChain( certInfoPtr, iSignContext, FALSE ) );
	}

/* Set up any required timestamp data for a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setCertTimeinfo( INOUT CERT_INFO *certInfoPtr,
							IN_HANDLE_OPT const CRYPT_CONTEXT iSignContext,
							const BOOLEAN isCertificate )
	{
	const time_t currentTime = ( iSignContext == CRYPT_UNUSED ) ? \
							   getTime() : getReliableTime( iSignContext );
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( iSignContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iSignContext ) );
	REQUIRES( isCertificate == TRUE || isCertificate == FALSE );

	/* If it's some certificate variant or a CRL/OCSP response and the 
	   various timestamps haven't been set yet, start them at the current 
	   time and give them the default validity period or next update time if 
	   these haven't been set.  The time used is the local time, this is 
	   converted to GMT when we write it to the certificate.  Issues like 
	   validity period nesting and checking for valid time periods are 
	   handled elsewhere */
	if( isCertificate )
		{
		if( certInfoPtr->startTime <= MIN_TIME_VALUE )
			{
			/* If the time is screwed up then we can't continue */
			if( currentTime <= MIN_TIME_VALUE )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_VALIDFROM,
							  CRYPT_ERRTYPE_ATTR_VALUE );
				return( CRYPT_ERROR_NOTINITED );
				}
			certInfoPtr->startTime = currentTime;
			}
		if( certInfoPtr->endTime <= MIN_TIME_VALUE )
			{
			int validity;

			ENSURES( certInfoPtr->startTime > MIN_TIME_VALUE && \
					 certInfoPtr->startTime < MAX_TIME_VALUE );

			status = krnlSendMessage( certInfoPtr->ownerHandle, 
									  IMESSAGE_GETATTRIBUTE, &validity, 
									  CRYPT_OPTION_CERT_VALIDITY );
			if( cryptStatusError( status ) )
				return( status );
			status = setEndTime( certInfoPtr, validity );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
#ifdef USE_CERTREV
	if( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
		certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
		{
		/* If the time is messed up then we can't provide an indication of 
		   the revocation time */
		if( currentTime <= MIN_TIME_VALUE )
			{
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_VALIDFROM,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_NOTINITED );
			}

		/* Set the revocation/validity times */
		if( certInfoPtr->startTime <= MIN_TIME_VALUE )
			certInfoPtr->startTime = currentTime;
		if( certInfoPtr->endTime <= MIN_TIME_VALUE )
			{
			if( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
				{
				/* OCSP responses come directly from the certificate store 
				   and represent an atomic (and ephemeral) snapshot of the 
				   store state.  Because of this the next-update time occurs 
				   effectively immediately since the next snapshot could 
				   provide a different response */
				certInfoPtr->endTime = currentTime;
				}
			else
				{
				int updateInterval;

				status = krnlSendMessage( certInfoPtr->ownerHandle,
										  IMESSAGE_GETATTRIBUTE, &updateInterval,
										  CRYPT_OPTION_CERT_UPDATEINTERVAL );
				if( cryptStatusError( status ) )
					return( status );
				status = setEndTime( certInfoPtr, updateInterval );
				if( cryptStatusError( status ) )
					return( status );
				}
			}
		if( certInfoPtr->cCertRev->revocationTime <= MIN_TIME_VALUE )
			certInfoPtr->cCertRev->revocationTime = currentTime;
		}
#endif /* USE_CERTREV */

	return( CRYPT_OK );
	}


/* Perform any final initialisation of the certificate object before we sign 
   it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initSignatureInfo( INOUT CERT_INFO *certInfoPtr, 
							  IN_OPT const CERT_INFO *issuerCertInfoPtr,
							  IN_HANDLE_OPT const CRYPT_CONTEXT iSignContext,
							  const BOOLEAN isCertificate,
							  OUT_ALGO_Z CRYPT_ALGO_TYPE *hashAlgo,
							  IN_ENUM_OPT( CRYPT_SIGNATURELEVEL ) \
								const CRYPT_SIGNATURELEVEL_TYPE signatureLevel,
							  OUT_OPT_LENGTH_SHORT_Z int *extraDataLength )
	{
	const CRYPT_ALGO_TYPE signingAlgo = ( issuerCertInfoPtr != NULL ) ? \
				issuerCertInfoPtr->publicKeyAlgo : certInfoPtr->publicKeyAlgo;
	int value, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( hashAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );
	assert( extraDataLength == NULL || \
			isWritePtr( extraDataLength, sizeof( int ) ) );

	REQUIRES( iSignContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iSignContext ) );
	REQUIRES( isCertificate == TRUE || isCertificate == FALSE );
	REQUIRES( ( signatureLevel == CRYPT_SIGNATURELEVEL_NONE && \
				extraDataLength == NULL ) || \
			  ( signatureLevel >= CRYPT_SIGNATURELEVEL_SIGNERCERT && \
				signatureLevel < CRYPT_SIGNATURELEVEL_LAST && \
				extraDataLength != NULL ) );

	/* Clear return values */
	*hashAlgo = CRYPT_ALGO_NONE;
	if( extraDataLength != NULL )
		*extraDataLength = 0;

	/* If we need to include extra data in the signature, make sure that 
	   it's available and determine how big it'll be.  If there's no issuer 
	   certificate available and we've been asked for extra signature data 
	   then we fall back to providing just a raw signature rather than 
	   bailing out completely */
	if( extraDataLength != NULL && issuerCertInfoPtr != NULL )
		{
		ENSURES( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT || \
				 certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST );

		if( signatureLevel == CRYPT_SIGNATURELEVEL_SIGNERCERT )
			{
			status = exportCert( NULL, 0, extraDataLength,
								 CRYPT_CERTFORMAT_CERTIFICATE,
								 issuerCertInfoPtr );
			}
		else
			{
			MESSAGE_DATA msgData;

			ENSURES( signatureLevel == CRYPT_SIGNATURELEVEL_ALL );

			setMessageData( &msgData, NULL, 0 );
			status = krnlSendMessage( issuerCertInfoPtr->objectHandle,
									  IMESSAGE_CRT_EXPORT, &msgData,
									  CRYPT_ICERTFORMAT_CERTSEQUENCE );
			if( cryptStatusOK( status ) )
				*extraDataLength = msgData.length;
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If it's a certificate chain, copy over the signing certificate(s) */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
		{
		REQUIRES( isHandleRangeValid( iSignContext ) );

		status = copySigningCertChain( certInfoPtr, iSignContext );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Set up any required timestamps */
	status = setCertTimeinfo( certInfoPtr, iSignContext, isCertificate );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a certificate, set up the certificate serial number */
	if( isCertificate )
		{
		status = setSerialNumber( certInfoPtr, NULL, 0 );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Determine the hash algorithm to use and if it's a certificate or 
	   CRL remember it for when we write the certificate (the value is 
	   embedded in the certificate to prevent an obscure attack on unpadded 
	   RSA signature algorithms or (EC)DSA signatures) */
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &value, 
							  CRYPT_OPTION_ENCR_HASH );
	if( cryptStatusError( status ) )
		return( status );
	*hashAlgo = value;	/* int vs. enum */
	if( signingAlgo == CRYPT_ALGO_DSA )
		{
		/* If we're going to be signing with DSA then things get a bit 
		   complicated, the only OID defined for non-SHA1 DSA is for 256-bit
		   SHA2, and even then in order to use it with a generic 1024-bit 
		   key we have to truncate the hash.  It's not clear how many 
		   implementations can handle this, and if we're using a hash wider
		   than SHA-2/256 or a newer hash like SHAng then we can't encode 
		   the result at all.  To deal with this we restrict the hash used 
		   with DSA to SHA-1 only */
		*hashAlgo = CRYPT_ALGO_SHA1;
		}
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
		certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN || \
		certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT )
		certInfoPtr->cCertCert->hashAlgo = *hashAlgo;
#ifdef USE_CERTREV
	else
		{
		if( certInfoPtr->type == CRYPT_CERTTYPE_CRL )
			certInfoPtr->cCertRev->hashAlgo = *hashAlgo;
		}
#endif /* USE_CERTREV */

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Signing Functions							*
*																			*
****************************************************************************/

#if defined( USE_CERTREQ ) || defined( USE_CERTREV ) || \
	defined( USE_CERTVAL ) || defined( USE_PKIUSER )

/* Pseudo-sign certificate information by writing the outer wrapper around 
   the certificate object data and moving the object into the initialised 
   state */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int pseudoSignCertificate( INOUT CERT_INFO *certInfoPtr,
								  INOUT_BUFFER_FIXED( signedObjectMaxLength ) \
										void *signedObject,
								  IN_LENGTH_SHORT_MIN( 16 ) \
										const int signedObjectMaxLength,
								  IN_BUFFER( certObjectLength ) \
										const void *certObject,
								  IN_LENGTH_SHORT_MIN( 16 ) \
										const int certObjectLength )
	{
	STREAM stream;
	int signedObjectLength, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtrDynamic( signedObject, signedObjectMaxLength ) );
	assert( isReadPtrDynamic( certObject, certObjectLength ) );

	REQUIRES( signedObjectMaxLength >= 16 && \
			  signedObjectMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( certObjectLength >= 16 && \
			  certObjectLength <= signedObjectMaxLength && \
			  certObjectLength < MAX_INTLENGTH_SHORT );

	switch( certInfoPtr->type )
		{
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_PKIUSER:
			/* It's an unsigned OCSP request or PKI user information, write 
			   the outer wrapper */
			signedObjectLength = sizeofObject( certObjectLength );
			ENSURES( signedObjectLength >= 16 && \
					 signedObjectLength <= signedObjectMaxLength );
			sMemOpen( &stream, signedObject, signedObjectLength );
			writeSequence( &stream, certObjectLength );
			status = swrite( &stream, certObject, certObjectLength );
			sMemDisconnect( &stream );
			ENSURES( cryptStatusOK( status ) );
			if( certInfoPtr->type == CRYPT_CERTTYPE_PKIUSER )
				{
				status = recoverCertData( certInfoPtr, 
										  CRYPT_CERTTYPE_PKIUSER, 
										  signedObject, signedObjectLength );
				if( cryptStatusError( status ) )
					return( status );
				}
			break;

		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			/* It's an RTCS request/response or OCSP response, it's already
			   in the form required */
			signedObjectLength = certObjectLength;
			REQUIRES( rangeCheck( certObjectLength, 16, 
								  signedObjectMaxLength ) );
			memcpy( signedObject, certObject, certObjectLength );
			break;

		case CRYPT_CERTTYPE_REQUEST_CERT:
			{
			const int dataSize = certObjectLength + \
								 sizeofObject( sizeofShortInteger( 0 ) );

			ENSURES( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT );

			/* It's an encryption-only key, wrap up the certificate data 
			   with an indication that private key POP will be performed via 
			   out-of-band means and remember where the encoded data 
			   starts */
			signedObjectLength = sizeofObject( dataSize );
			ENSURES( signedObjectLength >= 16 && \
					 signedObjectLength <= signedObjectMaxLength );
			sMemOpen( &stream, signedObject, signedObjectLength );
			writeSequence( &stream, dataSize );
			swrite( &stream, certObject, certObjectLength );
			writeConstructed( &stream, sizeofShortInteger( 0 ), 2 );
			status = writeShortInteger( &stream, 0, 1 );
			sMemDisconnect( &stream );
			ENSURES( cryptStatusOK( status ) );
			status = recoverCertData( certInfoPtr, 
									  CRYPT_CERTTYPE_REQUEST_CERT,
									  signedObject, signedObjectLength );
			if( cryptStatusError( status ) )
				return( status );

			/* Indicate that the pseudo-signature has been checked (since we 
			   just created it), this also avoids nasty semantic problems 
			   with not-really-signed CRMF requests containing encryption-
			   only keys */
			SET_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
			break;
			}

		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			/* Revocation requests can't be signed so the (pseudo-)signed
			   data is just the object data */
			signedObjectLength = certObjectLength;
			REQUIRES( rangeCheck( certObjectLength, 16, 
								  signedObjectMaxLength ) );
			memcpy( signedObject, certObject, certObjectLength );
			status = recoverCertData( certInfoPtr, 
									  CRYPT_CERTTYPE_REQUEST_REVOCATION,
									  signedObject, signedObjectLength );
			if( cryptStatusError( status ) )
				return( status );

			/* Since revocation requests can't be signed we mark them as
			   pseudo-signed to avoid any problems that might arise from
			   this */
			SET_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
			break;

		default:
			retIntError();
		}
	certInfoPtr->certificate = signedObject;
	certInfoPtr->certificateSize = signedObjectLength;

	/* The object is now (pseudo-)signed and initialised */
	SET_FLAG( certInfoPtr->flags, CERT_FLAG_SIGCHECKED );
	if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		/* If it's a CRMF request with POP done via out-of-band means we
		   got here via a standard signing action (except that the key was
		   an encryption-only key), don't change the object state since the
		   kernel will do this as the post-signing step */
		return( CRYPT_OK );
		}
	return( krnlSendMessage( certInfoPtr->objectHandle,
							 IMESSAGE_SETATTRIBUTE, MESSAGE_VALUE_UNUSED,
							 CRYPT_IATTRIBUTE_INITIALISED ) );
	}
#endif /* USE_CERTREQ || USE_CERTREV || USE_CERTVAL || USE_PKIUSER */

/* Sign the certificate information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 6 ) ) \
static int signCertInfo( OUT_BUFFER( signedObjectMaxLength, \
									 *signedObjectLength ) \
							void *signedObject, 
						 IN_DATALENGTH const int signedObjectMaxLength, 
						 OUT_DATALENGTH_Z int *signedObjectLength,
						 IN_BUFFER( objectLength ) const void *object, 
						 IN_DATALENGTH const int objectLength,
						 INOUT CERT_INFO *certInfoPtr, 
						 IN_HANDLE const CRYPT_CONTEXT iSignContext,
						 IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						 IN_ENUM( CRYPT_SIGNATURELEVEL ) \
							const CRYPT_SIGNATURELEVEL_TYPE signatureLevel,
						 IN_LENGTH_SHORT_Z const int extraDataLength,
						 IN_OPT const CERT_INFO *issuerCertInfoPtr )
	{
	STREAM stream;
	const int extraDataType = \
			( signatureLevel == CRYPT_SIGNATURELEVEL_SIGNERCERT ) ? \
			CRYPT_CERTFORMAT_CERTIFICATE : CRYPT_ICERTFORMAT_CERTSEQUENCE;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtrDynamic( signedObject, signedObjectMaxLength ) );
	assert( isWritePtr( signedObjectLength, sizeof( int ) ) );
	assert( isReadPtrDynamic( object, objectLength ) && \
			cryptStatusOK( checkCertObjectEncoding( object, objectLength ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( signedObjectMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  signedObjectMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( objectLength >= 16 && \
			  objectLength <= signedObjectMaxLength && \
			  objectLength < MAX_BUFFER_SIZE );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( isEnumRangeOpt( signatureLevel, CRYPT_SIGNATURELEVEL ) );
	REQUIRES( isShortIntegerRange( extraDataLength ) );
	REQUIRES( extraDataLength <= 0 || issuerCertInfoPtr != NULL );

	/* Sign the certificate information.  CRMF and OCSP use a b0rken
	   signature format (the authors couldn't quite manage a cut & paste of
	   two lines of text) so if it's one of these we have to use nonstandard
	   formatting */
	if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT || \
		certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST )
		{
		X509SIG_FORMATINFO formatInfo;

		if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
			{
			/* [1] SEQUENCE */
			setX509FormatInfo( &formatInfo, 1, FALSE );
			}
		else
			{
			/* [0] EXPLICIT SEQUENCE */
			setX509FormatInfo( &formatInfo, 0, TRUE );
			}
		if( signatureLevel == CRYPT_SIGNATURELEVEL_SIGNERCERT )
			{
			formatInfo.extraLength = \
							sizeofShortObject( \
								sizeofShortObject( extraDataLength ) );
			}
		else
			{
			if( signatureLevel == CRYPT_SIGNATURELEVEL_ALL )
				{
				formatInfo.extraLength = \
							sizeofShortObject( extraDataLength );
				}
			}
		status = createX509signature( signedObject, signedObjectMaxLength, 
								signedObjectLength, object, objectLength, 
								iSignContext, hashAlgo, &formatInfo );
		}
	else
		{
		/* It's a standard signature */
		status = createX509signature( signedObject, signedObjectMaxLength, 
								signedObjectLength, object, objectLength, 
								iSignContext, hashAlgo, NULL );
		}
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_VALUE : status );

	/* If there's no extra data to handle then we're done */
	if( extraDataLength <= 0 )
		{
		ENSURES( cryptStatusOK( \
					checkCertObjectEncoding( signedObject, 
											 *signedObjectLength ) ) );
		return( CRYPT_OK );
		}
	
	/* The extra data consists of signing certificates, so we can't continue
	   if there are none provided.  Figuring out how we get to this point is 
	   rather complex, if we have a certificate, a CRL, or an OCSP object 
	   with an associated signing key then we have an issuer cert present 
	   (from signCert()).  If it's an OCSP request then the signature level 
	   is something other than CRYPT_SIGNATURELEVEL_NONE, at which point if 
	   there's an issuer certificate present then extraDataLength != 0 (from 
	   initSignatureInfo()).  After this, signCert() will exit if there's no 
	   signing key present since there's nothing further to do.  This means 
	   that when we get here and extraDataLength != 0 then it means that 
	   there's an issuer certificate present.  The following check ensures 
	   that this is indeed the case */
	ENSURES( issuerCertInfoPtr != NULL );

	/* If we need to include extra data with the signature attach it to the 
	   end of the signature */
	ENSURES( boundsCheck( *signedObjectLength, 
						  signedObjectMaxLength - *signedObjectLength, 
						  signedObjectMaxLength ) );
	sMemOpen( &stream, ( BYTE * ) signedObject + *signedObjectLength,
			  signedObjectMaxLength - *signedObjectLength );
	if( signatureLevel == CRYPT_SIGNATURELEVEL_SIGNERCERT )
		{
		writeConstructed( &stream, sizeofObject( extraDataLength ), 0 );
		status = writeSequence( &stream, extraDataLength );
		}
	else
		{
		ENSURES( signatureLevel == CRYPT_SIGNATURELEVEL_ALL );

		status = writeConstructed( &stream, extraDataLength, 0 );
		}
	if( cryptStatusOK( status ) )
		{
		status = exportCertToStream( &stream, issuerCertInfoPtr->objectHandle,
									 extraDataType );
		}
	if( cryptStatusOK( status ) )
		*signedObjectLength += stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( cryptStatusOK( \
					checkCertObjectEncoding( signedObject, 
											 *signedObjectLength ) ) );

	return( CRYPT_OK );
	}

/* Create the signed certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int createSignedObject( INOUT CERT_INFO *certInfoPtr, 
							   IN_OPT const CERT_INFO *issuerCertInfoPtr,
							   IN_HANDLE_OPT const CRYPT_CONTEXT iSignContext,
							   IN_ENUM_OPT( CRYPT_SIGNATURELEVEL ) \
									const CRYPT_SIGNATURELEVEL_TYPE signatureLevel,
							   IN_LENGTH_SHORT_Z const int extraDataLength,
							   IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
							   const BOOLEAN nonSigningKey )
	{
	WRITECERT_FUNCTION writeCertFunction;
	STREAM stream;
	BYTE certObjectBuffer[ 1024 + 8 ], *certObjectPtr = certObjectBuffer;
	void *signedCertObject;
	int certObjectLength DUMMY_INIT, signedCertObjectLength;
	int signedCertAllocSize, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( issuerCertInfoPtr == NULL || \
			isReadPtr( issuerCertInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( iSignContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iSignContext ) );
	REQUIRES( isEnumRangeOpt( signatureLevel, CRYPT_SIGNATURELEVEL ) );
	REQUIRES( isShortIntegerRange( extraDataLength ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( nonSigningKey == TRUE || nonSigningKey == FALSE );

	/* Select the function to use to write the certificate object to be
	   signed */
	writeCertFunction = getCertWriteFunction( certInfoPtr->type );
	ENSURES( writeCertFunction != NULL );

	/* Determine how big the encoded certificate information will be,
	   allocate memory for it and the full signed certificate, and write the
	   encoded certificate information */
	sMemNullOpen( &stream );
	status = writeCertFunction( &stream, certInfoPtr, issuerCertInfoPtr, 
								iSignContext );
	if( cryptStatusOK( status ) )
		certObjectLength = stell( &stream );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( isIntegerRangeNZ( certObjectLength ) );
	signedCertAllocSize = certObjectLength + 1024 + extraDataLength;
	if( certObjectLength > 1024 )
		{
		certObjectPtr = clDynAlloc( "signCert", certObjectLength );
		if( certObjectPtr == NULL )
			return( CRYPT_ERROR_MEMORY );
		}
	REQUIRES( rangeCheck( signedCertAllocSize, 1, MAX_BUFFER_SIZE ) );
	signedCertObject = clAlloc( "signCert", signedCertAllocSize );
	if( signedCertObject == NULL )
		{
		if( certObjectPtr != certObjectBuffer )
			clFree( "signCert", certObjectPtr );
		if( signedCertObject != NULL )
			clFree( "signCert", signedCertObject );
		return( CRYPT_ERROR_MEMORY );
		}
	sMemOpen( &stream, certObjectPtr, certObjectLength );
	status = writeCertFunction( &stream, certInfoPtr, issuerCertInfoPtr, 
								iSignContext );
	ENSURES( cryptStatusError( status ) || \
			 certObjectLength == stell( &stream ) );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		zeroise( certObjectPtr, certObjectLength );
		if( certObjectPtr != certObjectBuffer )
			clFree( "signCert", certObjectPtr );
		clFree( "signCert", signedCertObject );
		return( status );
		}
	ENSURES( cryptStatusOK( checkCertObjectEncoding( certObjectPtr, 
													 certObjectLength ) ) );

#if defined( USE_CERTREQ ) || defined( USE_CERTREV ) || \
	defined( USE_CERTVAL ) || defined( USE_PKIUSER )
	/* If there's no signing key present we pseudo-sign the certificate 
	   information by writing the outer wrapper and moving the object into 
	   the initialised state */
	if( nonSigningKey )
		{
		status = pseudoSignCertificate( certInfoPtr, signedCertObject,
										signedCertAllocSize, certObjectPtr, 
										certObjectLength );
		zeroise( certObjectPtr, certObjectLength );
		if( certObjectPtr != certObjectBuffer )
			clFree( "signCert", certObjectPtr );
		if( cryptStatusError( status ) )
			{
			clFree( "signCert", signedCertObject );
			return( status );
			}
		ANALYSER_HINT( certInfoPtr->certificate != NULL );
		ENSURES( cryptStatusOK( \
					checkCertObjectEncoding( certInfoPtr->certificate, 
											 certInfoPtr->certificateSize ) ) );

		return( CRYPT_OK );
		}
#endif /* USE_CERTREQ || USE_CERTREV || USE_CERTVAL || USE_PKIUSER */

	/* Sign the certificate information */
	status = signCertInfo( signedCertObject, signedCertAllocSize, 
						   &signedCertObjectLength, certObjectPtr, 
						   certObjectLength, certInfoPtr, iSignContext, 
						   hashAlgo, signatureLevel, extraDataLength,
						   issuerCertInfoPtr );
	zeroise( certObjectPtr, certObjectLength );
	if( certObjectPtr != certObjectBuffer )
		clFree( "signCert", certObjectPtr );
	if( cryptStatusError( status ) )
		{
		clFree( "signCert", signedCertObject );
		return( status );
		}
	certInfoPtr->certificate = signedCertObject;
	certInfoPtr->certificateSize = signedCertObjectLength;

	return( CRYPT_OK );
	}

/* Sign a certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int signCert( INOUT CERT_INFO *certInfoPtr, 
			  IN_HANDLE_OPT const CRYPT_CONTEXT iSignContext )
	{
	CRYPT_ALGO_TYPE hashAlgo;
	CERT_INFO *issuerCertInfoPtr = NULL;
#ifdef USE_CERTREV
	const CRYPT_SIGNATURELEVEL_TYPE signatureLevel = \
				( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST ) ? \
					certInfoPtr->cCertRev->signatureLevel : \
					CRYPT_SIGNATURELEVEL_NONE;
#else
	const CRYPT_SIGNATURELEVEL_TYPE signatureLevel = CRYPT_SIGNATURELEVEL_NONE;
#endif /* USE_CERTREV */
	const BOOLEAN isCertificate = \
			( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN ) ? TRUE : FALSE;
	BOOLEAN issuerCertAcquired = FALSE, nonSigningKey = FALSE;
	int extraDataLength = 0, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( certInfoPtr->certificate == NULL );
	REQUIRES( iSignContext == CRYPT_UNUSED || \
			  isHandleRangeValid( iSignContext ) );

	/* If it's a non-signing key then we have to create a special format of 
	   certificate request that isn't signed but contains an indication that 
	   the private key POP will be performed by out-of-band means.  We also 
	   have to check for the iSignContext being absent to handle OCSP 
	   requests for which the signature is optional so there may be no 
	   signing key present */
	if( iSignContext == CRYPT_UNUSED )
		nonSigningKey = TRUE;
	else
		{
		/* We've got a signing key, make sure that it's signature-capable */
		if( !checkContextCapability( iSignContext, 
									 MESSAGE_CHECK_PKC_SIGN_CA ) && \
			!checkContextCapability( iSignContext, 
									 MESSAGE_CHECK_PKC_SIGN ) )
			nonSigningKey = TRUE;
		}

	/* Obtain the issuer certificate from the private key if necessary 
	   (aliena nobis, nostra plus aliis placent) */
	if( isCertificate || \
		certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
		( ( certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
			certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE ) && \
		  !nonSigningKey ) )
		{
		/* If it's a self-signed certificate then the issuer is also the 
		   subject */
		if( TEST_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED ) )
			issuerCertInfoPtr = certInfoPtr;
		else
			{
			CRYPT_CERTIFICATE dataOnlyCert;

			/* Get the data-only certificate from the context */
			status = krnlSendMessage( iSignContext, IMESSAGE_GETDEPENDENT,
									  &dataOnlyCert, OBJECT_TYPE_CERTIFICATE );
			if( cryptStatusError( status ) )
				{
				return( ( status == CRYPT_ARGERROR_OBJECT ) ? \
						CRYPT_ARGERROR_VALUE : status );
				}
			status = krnlAcquireObject( dataOnlyCert, OBJECT_TYPE_CERTIFICATE,
										( MESSAGE_PTR_CAST ) &issuerCertInfoPtr,
										CRYPT_ARGERROR_VALUE );
			if( cryptStatusError( status ) )
				return( status );
			REQUIRES_OBJECT( sanityCheckCert( issuerCertInfoPtr ),
							 issuerCertInfoPtr->objectHandle );
			issuerCertAcquired = TRUE;
			}

		/* Check the signing key */
		status = checkSigningKey( certInfoPtr, issuerCertInfoPtr, 
								  iSignContext, isCertificate );
		if( cryptStatusError( status ) )
			{
			if( issuerCertAcquired == TRUE )
				krnlReleaseObject( issuerCertInfoPtr->objectHandle );
			return( status );
			}
		}

	/* Perform any final initialisation of the certificate object before we 
	   sign it */
	status = initSignatureInfo( certInfoPtr, issuerCertInfoPtr, 
								iSignContext, isCertificate, &hashAlgo,
								signatureLevel,
								( signatureLevel > CRYPT_SIGNATURELEVEL_NONE ) ? \
									&extraDataLength : NULL );
	if( cryptStatusError( status ) )
		{
		if( issuerCertAcquired == TRUE )
			krnlReleaseObject( issuerCertInfoPtr->objectHandle );
		return( status );
		}

	/* Create the signed certifcate object */
	status = createSignedObject( certInfoPtr, issuerCertInfoPtr, 
								 iSignContext, signatureLevel, 
								 extraDataLength, hashAlgo, nonSigningKey );
	if( issuerCertAcquired == TRUE )
		krnlReleaseObject( issuerCertInfoPtr->objectHandle );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( certInfoPtr->certificate != NULL );

	/* If it's a pseudo-signed certificate object, we're done */
	if( nonSigningKey )
		{
		ENSURES( sanityCheckCert( certInfoPtr ) );

		return( CRYPT_OK );
		}

	/* If it's a certification request it's now self-signed.  In addition 
	   the signature has been checked since we've just created it */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
		certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		SET_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
	SET_FLAG( certInfoPtr->flags, CERT_FLAG_SIGCHECKED );

	/* If it's not an object type with special-case post-signing
	   requirements we're done */
	if( certInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
		certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN && \
		certInfoPtr->type != CRYPT_CERTTYPE_CERTREQUEST && \
		certInfoPtr->type != CRYPT_CERTTYPE_REQUEST_CERT && \
		certInfoPtr->type != CRYPT_CERTTYPE_CRL )
		{
		ENSURES( sanityCheckCert( certInfoPtr ) );

		return( CRYPT_OK );
		}

	/* Recover information such as pointers to encoded certificate data */
	status = recoverCertData( certInfoPtr, certInfoPtr->type, 
							  certInfoPtr->certificate, 
							  certInfoPtr->certificateSize );
	if( cryptStatusError( status ) )
		return( status );

	ENSURES( sanityCheckCert( certInfoPtr ) );

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
