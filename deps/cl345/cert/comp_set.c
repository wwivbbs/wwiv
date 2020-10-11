/****************************************************************************
*																			*
*							Set Certificate Components						*
*						Copyright Peter Gutmann 1997-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "certattr.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "cert/certattr.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*							Serial-Number Routines							*
*																			*
****************************************************************************/

/* Set the serial number for a certificate.  In theory we could store this 
   as a static value in the configuration database but this has three
   disadvantages: Updating the serial number updates the entire 
   configuration database (including things the user might not want
   updated), if the configuration database update fails then the serial 
   number never changes, and the predictable serial number allows tracking 
   of the number of certificates that have been issued by the CA.  Because 
   of this we just use a 64-bit nonce if the user doesn't supply a value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setSerialNumber( INOUT CERT_INFO *certInfoPtr, 
					 IN_BUFFER_OPT( serialNumberLength ) const void *serialNumber, 
					 IN_LENGTH_SHORT_Z const int serialNumberLength )
	{
	MESSAGE_DATA msgData;
	BYTE *serialNumberPtr;
	int length DUMMY_INIT, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( serialNumber == NULL && serialNumberLength == 0 ) || \
			( isReadPtrDynamic( serialNumber, serialNumberLength ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN || \
			  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION );
	REQUIRES( ( serialNumber == NULL && serialNumberLength == 0 ) || \
			  ( serialNumber != NULL && \
				serialNumberLength > 0 && \
				serialNumberLength <= MAX_SERIALNO_SIZE ) );

   	/* Get a pointer to the location where we're going to store the new 
	   serial number, either a user-supplied value or one that we've 
	   generated ourselves.  However if a serial number has already been set 
	   explicitly then we don't override it with an implicitly-set one */
	switch( certInfoPtr->type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_CERTCHAIN:
			if( certInfoPtr->cCertCert->serialNumber != NULL )
				{
				ENSURES( serialNumber == NULL && serialNumberLength == 0 );

				return( CRYPT_OK );
				}
			serialNumberPtr = certInfoPtr->cCertCert->serialNumberBuffer;
			certInfoPtr->cCertCert->serialNumber = serialNumberPtr;
			break;

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			if( certInfoPtr->cCertReq->serialNumber != NULL )
				{
				ENSURES( serialNumber == NULL && serialNumberLength == 0 );

				return( CRYPT_OK );
				}
			serialNumberPtr = certInfoPtr->cCertReq->serialNumberBuffer;
			certInfoPtr->cCertReq->serialNumber = serialNumberPtr;
			break;
#endif /* USE_CERTREV */

		default:
			retIntError();
		}

	/* If we're using user-supplied serial number data, canonicalise it into
	   a form suitable for use as an INTEGER hole */
	if( serialNumber != NULL )
		{
		STREAM stream;
		BYTE buffer[ 4 + MAX_SERIALNO_SIZE + 8 ];
		int bufPos;
 
		sMemOpen( &stream, buffer, 4 + MAX_SERIALNO_SIZE );
		status = writeInteger( &stream, serialNumber, serialNumberLength,
							   DEFAULT_TAG );
		if( cryptStatusOK( status ) )
			length = stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusError( status ) )
			return( status );
		bufPos = 2;		/* Skip INTEGER tag + length at the start */
		length -= 2;

		/* Now that we've canonicalised the serial number, check that its 
		   size is still within bounds */
		if( length < 1 || length > MAX_SERIALNO_SIZE )
			return( CRYPT_ERROR_OVERFLOW );

		/* If it's too long to fit into the certificate's internal serial-
		   number buffer, allocate storage for it dynamically */
		if( length > SERIALNO_BUFSIZE )
			{
			if( ( serialNumberPtr = clDynAlloc( "setSerialNumber", 
												length ) ) == NULL )
				return( CRYPT_ERROR_MEMORY );
			}

		/* Copy across the canonicalised serial number value.  Since the 
		   serial number data may now be held in dynamically-allocated 
		   memory, we have to update the serial number pointer */
		memcpy( serialNumberPtr, buffer + bufPos, length );
#ifdef USE_CERTREV
		if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION )
			{
			certInfoPtr->cCertReq->serialNumber = serialNumberPtr;
			certInfoPtr->cCertReq->serialNumberLength = length;
			}
		else
#endif /* USE_CERTREV */
			{
			certInfoPtr->cCertCert->serialNumber = serialNumberPtr;
			certInfoPtr->cCertCert->serialNumberLength = length;
			}

		return( CRYPT_OK );
		}

	/* Generate a random (but fixed-length) serial number, ensure that the 
	   first byte of the value that we use is nonzero (to guarantee a DER 
	   encoding), and clear the high bit to provide a constant-length ASN.1 
	   encoded value */
	static_assert( DEFAULT_SERIALNO_SIZE + 1 <= SERIALNO_BUFSIZE, \
				   "Buffer size" );
	setMessageData( &msgData, serialNumberPtr, DEFAULT_SERIALNO_SIZE + 1 );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_GETATTRIBUTE_S, &msgData,
							  CRYPT_IATTRIBUTE_RANDOM_NONCE );
	if( cryptStatusError( status ) )
		return( status );
	serialNumberPtr[ 0 ] &= 0x7F;	/* Clear the sign bit */
	if( serialNumberPtr[ 0 ] == 0 )
		{
		/* The first byte is zero, see if we can use the extra byte of data 
		   that we fetched as a nonzero byte.  If that's zero too, just set 
		   it to 1 */
		serialNumberPtr[ 0 ] = \
				intToByte( serialNumberPtr[ DEFAULT_SERIALNO_SIZE ] & 0x7F );
		if( serialNumberPtr[ 0 ] == 0 )
			serialNumberPtr[ 0 ] = 1;
		}

	/* Copy across the canonicalised serial number value */
#ifdef USE_CERTREV
	if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION )
		{
		certInfoPtr->cCertReq->serialNumberLength = DEFAULT_SERIALNO_SIZE;
		}
	else
#endif /* USE_CERTREV */
		{
		certInfoPtr->cCertCert->serialNumberLength = DEFAULT_SERIALNO_SIZE;
		}

	return( CRYPT_OK );
	}

/* Compare a serial number in canonical form to a generic serial number,
   with special handling for leading-zero truncation.  This one can get a
   bit tricky because for a long time Microsoft would fairly consistently 
   encode serial numbers incorrectly so we normalise the values to have no 
   leading zero, which is the lowest common denominator */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 3 ) ) \
BOOLEAN compareSerialNumber( IN_BUFFER( canonSerialNumberLength ) \
								const void *canonSerialNumber,
							 IN_LENGTH_SHORT const int canonSerialNumberLength,
							 IN_BUFFER( serialNumberLength ) \
								const void *serialNumber,
							 IN_LENGTH_SHORT const int serialNumberLength )
	{
	const BYTE *canonSerialNumberPtr = canonSerialNumber;
	const BYTE *serialNumberPtr = serialNumber;
	int canonSerialLength = canonSerialNumberLength;
	int serialLength, LOOP_ITERATOR;

	assert( isReadPtrDynamic( canonSerialNumber, canonSerialNumberLength ) );
	assert( isReadPtrDynamic( serialNumber, serialNumberLength ) );

	REQUIRES_B( isShortIntegerRangeNZ( canonSerialNumberLength ) );
	REQUIRES_B( isShortIntegerRangeNZ( serialNumberLength ) );

	/* Internal serial numbers are canonicalised so all we need to do is 
	   strip a possible leading zero */
	if( canonSerialNumberPtr[ 0 ] == 0 )
		{
		canonSerialNumberPtr++;
		canonSerialLength--;
		}
	ENSURES_B( canonSerialLength == 0 || canonSerialNumberPtr[ 0 ] != 0 );

	/* Serial numbers from external sources can be arbitarily strangely
	   encoded so we strip leading zeroes until we get to actual data */
	LOOP_LARGE( serialLength = serialNumberLength,
				serialLength > 0 && serialNumberPtr[ 0 ] == 0,
				( serialLength--, serialNumberPtr++ ) );
	ENSURES( LOOP_BOUND_OK );

	/* Finally we've got them in a form where we can compare them */
	if( canonSerialLength != serialLength )
		return( FALSE );
	if( canonSerialLength <= 0 )
		{
		/* It's an all-zeroes serialNumber, there's nothing to compare */
		return( TRUE );
		}
	return( memcmp( canonSerialNumberPtr, serialNumberPtr, 
					serialLength ) ? FALSE : TRUE );
	}

/****************************************************************************
*																			*
*						Set Miscellaneous Information						*
*																			*
****************************************************************************/

/* Set XYZZY certificate information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int setXyzzyInfo( INOUT CERT_INFO *certInfoPtr )
	{
	CRYPT_ATTRIBUTE_TYPE dummy1;
	CRYPT_ERRTYPE_TYPE dummy2;
	DATAPTR_ATTRIBUTE attributePtr;
	const time_t currentTime = getApproxTime();
	int isXyzzyCert, keyUsage = CRYPT_KEYUSAGE_NONE, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* Make sure that we haven't already set up this certificate as a XYZZY
	   certificate */
	status = getCertComponent( certInfoPtr, CRYPT_CERTINFO_XYZZY, 
							   &isXyzzyCert );
	if( cryptStatusOK( status ) && isXyzzyCert )
		{
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_XYZZY,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		return( CRYPT_ERROR_INITED );
		}

	/* Get the appropriate key usage types that are possible for this
	   certificate.  We only do this if there's a key present, if it hasn't 
	   been set yet (in other words if the caller sets CRYPT_CERTINFO_XYZZY 
	   before they set CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO) then this will
	   be done in the code that sets the public key */
	if( certInfoPtr->publicKeyAlgo != CRYPT_ALGO_NONE )
		{
		status = checkKeyUsage( certInfoPtr, CHECKKEY_FLAG_NONE, 
								CRYPT_KEYUSAGE_KEYENCIPHERMENT,
								CRYPT_COMPLIANCELEVEL_STANDARD,
								&dummy1, &dummy2 );
		if( cryptStatusOK( status ) )
			keyUsage = CRYPT_KEYUSAGE_KEYENCIPHERMENT;
		status = checkKeyUsage( certInfoPtr, CHECKKEY_FLAG_NONE, 
								CRYPT_KEYUSAGE_DIGITALSIGNATURE,
								CRYPT_COMPLIANCELEVEL_STANDARD,
								&dummy1, &dummy2 );
		if( cryptStatusOK( status ) )
			keyUsage |= KEYUSAGE_SIGN | KEYUSAGE_CA;
		if( !( keyUsage & CRYPT_KEYUSAGE_DIGITALSIGNATURE ) )
			{
			/* We have to have at least a signing capability available in 
			   order to create a self-signed certificate */
			setErrorInfo( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
						  CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Clear any existing attribute values before trying to set new ones.  
	   We don't check the return values for these operations because we 
	   could get a success or error status depending on whether a component 
	   is present or not, and in any case any problem with deleting a 
	   present component will be caught when we try and set the new value
	   further on */
	certInfoPtr->startTime = certInfoPtr->endTime = 0;
	( void ) deleteCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE );
	( void ) deleteCertComponent( certInfoPtr, 
								  CRYPT_CERTINFO_CERTIFICATEPOLICIES );

	/* Give the certificate a 20-year expiry time, make it a self-signed CA 
	   certificate with all key usage types enabled, and set the policy OID 
	   to identify it as a XYZZY certificate */
	certInfoPtr->startTime = currentTime;
	certInfoPtr->endTime = certInfoPtr->startTime + ( 86400L * 365 * 20 );
	SET_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
	status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_CA, TRUE );
	if( cryptStatusOK( status ) && keyUsage != CRYPT_KEYUSAGE_NONE )
		{
		status = addCertComponent( certInfoPtr, CRYPT_CERTINFO_KEYUSAGE,
								   keyUsage );
		}
	if( cryptStatusOK( status ) )
		{
		status = addCertComponentString( certInfoPtr, 
										 CRYPT_CERTINFO_CERTPOLICYID,
										 OID_CRYPTLIB_XYZZYCERT,
										 sizeofOID( OID_CRYPTLIB_XYZZYCERT ) );
		}
	if( cryptStatusOK( status ) )
		{
		attributePtr = findAttributeFieldEx( certInfoPtr->attributes,
											 CRYPT_CERTINFO_CERTPOLICYID );
		ENSURES( DATAPTR_ISSET( attributePtr ) );
		setAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_LOCKED, 0 );
		}
	return( status );
	}

#ifdef USE_CERT_DNSTRING

/* Convert a DN in string form into a certificate DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getEncodedDn( INOUT CERT_INFO *certInfoPtr, 
						 IN_BUFFER( dnStringLength ) const void *dnString,
						 IN_LENGTH_ATTRIBUTE const int dnStringLength )
	{
	SELECTION_STATE savedState;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtrDynamic( dnString, dnStringLength ) );

	REQUIRES( isShortIntegerRangeNZ( dnStringLength ) );

	/* If there's already a DN set then we can't do anything else.  Since 
	   this potentially changes the DN selection state, we save the state 
	   around the check */
	saveSelectionState( savedState, certInfoPtr );
	status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE, MUST_BE_PRESENT );
	if( cryptStatusOK( status ) && \
		DATAPTR_ISNULL_PTR( certInfoPtr->currentSelection.dnPtr ) )
		{
		/* There's a DN selected but it's empty (in other words it's been 
		   marked for create-on-access but doesn't actually exist yet), 
		   we're OK */
		status = CRYPT_ERROR;
		}
	restoreSelectionState( savedState, certInfoPtr );
	if( cryptStatusOK( status ) )
		return( CRYPT_ERROR_INITED );

	/* Read the entire DN from its string form into the selected DN */
	status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE, CREATE_IF_ABSENT );
	if( cryptStatusError( status ) )
		return( status );
	status = readDNstring( certInfoPtr->currentSelection.dnPtr,
						   dnString, dnStringLength );
	if( cryptStatusOK( status ) && \
		certInfoPtr->currentSelection.updateCursor )
		{
		/* If we couldn't update the cursor earlier on because the attribute
		   field in question hadn't been created yet, do it now.  Since this 
		   is merely a side-effect of the DN-read operation we ignore the 
		   return status */
		( void ) selectGeneralName( certInfoPtr,
									certInfoPtr->currentSelection.generalName,
									MAY_BE_ABSENT );
		}
	return( status );
	}
#endif /* USE_CERT_DNSTRING */

/****************************************************************************
*																			*
*									Add a Component							*
*																			*
****************************************************************************/

/* Add a certificate component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int addCertComponent( INOUT CERT_INFO *certInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					  const int certInfo )
	{
	CRYPT_CERTIFICATE addedCert;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );

	/* If we're adding data to a certificate, clear the error information */
	if( !isPseudoInformation( certInfoType ) )
		clearErrorInfo( certInfoPtr );

	/* If it's a GeneralName or DN selection component, add it.  These are 
	   special-case attribute values so they have to come before the 
	   attribute-handling code */
	if( isGeneralNameSelectionComponent( certInfoType ) )
		{
		status = selectGeneralName( certInfoPtr, certInfoType,
									MAY_BE_ABSENT );
		if( cryptStatusError( status ) )
			return( status );
		return( selectGeneralName( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
								   MUST_BE_PRESENT ) );
		}

	/* If it's standard certificate or CMS attribute, add it to the 
	   certificate */
	if( isValidExtension( certInfoType ) )
		{
		int localCertInfoType = certInfoType;

		/* Revocation reason codes are actually a single range of values
		   spread across two different extensions so we adjust the
		   (internal) type based on the reason code value */
		if( certInfoType == CRYPT_CERTINFO_CRLREASON || \
			certInfoType == CRYPT_CERTINFO_CRLEXTREASON )
			{
			localCertInfoType = ( certInfo < CRYPT_CRLREASON_LAST ) ? \
					CRYPT_CERTINFO_CRLREASON : CRYPT_CERTINFO_CRLEXTREASON;
			}

		/* If it's a CRL, RTCS, or OCSP per-entry attribute, add the
		   attribute to the currently selected entry unless it's a
		   revocation request, in which case it goes in with the main
		   attributes */
#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
		if( isRevocationEntryComponent( localCertInfoType ) && \
			certInfoPtr->type != CRYPT_CERTTYPE_REQUEST_REVOCATION )
			{
			switch( certInfoPtr->type )
				{
  #ifdef USE_CERTVAL
				case CRYPT_CERTTYPE_RTCS_REQUEST:
				case CRYPT_CERTTYPE_RTCS_RESPONSE:
					{
					CERT_VAL_INFO *certValInfo = certInfoPtr->cCertVal;
					VALIDITY_INFO *validityInfoPtr;

					if( DATAPTR_ISNULL( certValInfo->currentValidity ) )
						return( CRYPT_ERROR_NOTFOUND );
					validityInfoPtr = DATAPTR_GET( certValInfo->currentValidity );
					ENSURES( validityInfoPtr != NULL );
					return( addAttributeField( &validityInfoPtr->attributes, 
									localCertInfoType, CRYPT_ATTRIBUTE_NONE, 
									certInfo, ATTR_FLAG_NONE, 
									&certInfoPtr->errorLocus, 
									&certInfoPtr->errorType ) );
					}
  #endif /* USE_CERTVAL */
  #ifdef USE_CERTREV
				case CRYPT_CERTTYPE_CRL:
				case CRYPT_CERTTYPE_OCSP_REQUEST:
				case CRYPT_CERTTYPE_OCSP_RESPONSE:
					{
					CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;
					REVOCATION_INFO *revInfoPtr;

					if( DATAPTR_ISNULL( certRevInfo->currentRevocation ) )
						return( CRYPT_ERROR_NOTFOUND );
					revInfoPtr = DATAPTR_GET( certRevInfo->currentRevocation );
					ENSURES( revInfoPtr != NULL );
					return( addAttributeField( &revInfoPtr->attributes,
									localCertInfoType, CRYPT_ATTRIBUTE_NONE,
									certInfo, ATTR_FLAG_NONE, 
									&certInfoPtr->errorLocus, 
									&certInfoPtr->errorType ) );
					}
  #endif /* USE_CERTREV */
				}

			retIntError();
			}
#endif /* USE_CERTREV || USE_CERTVAL */

		return( addAttributeField( &certInfoPtr->attributes,
						localCertInfoType, CRYPT_ATTRIBUTE_NONE, certInfo, 
						ATTR_FLAG_NONE,  &certInfoPtr->errorLocus, 
						&certInfoPtr->errorType ) );
		}

	/* If it's anything else, handle it specially */
	switch( certInfoType )
		{
		case CRYPT_CERTINFO_SELFSIGNED:
			if( certInfo )
				SET_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
			else
				CLEAR_FLAG( certInfoPtr->flags, CERT_FLAG_SELFSIGNED );
			return( CRYPT_OK );

		case CRYPT_CERTINFO_XYZZY:
			return( setXyzzyInfo( certInfoPtr ) );

		case CRYPT_CERTINFO_CURRENT_CERTIFICATE:
			return( setCertificateCursor( certInfoPtr, certInfo ) );

		case CRYPT_ATTRIBUTE_CURRENT_GROUP:
		case CRYPT_ATTRIBUTE_CURRENT:
		case CRYPT_ATTRIBUTE_CURRENT_INSTANCE:
			return( setAttributeCursor( certInfoPtr, certInfoType, certInfo ) );

		case CRYPT_CERTINFO_TRUSTED_USAGE:
			certInfoPtr->cCertCert->trustedUsage = certInfo;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_TRUSTED_IMPLICIT:
			{
			int value;

			/* This option is only valid for CA certificates */
			status = getAttributeFieldValue( certInfoPtr->attributes, 
											 CRYPT_CERTINFO_KEYUSAGE,
											 CRYPT_ATTRIBUTE_NONE, &value );
			if( cryptStatusError( status ) || !( value & KEYUSAGE_CA ) )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CA,
							  CRYPT_ERRTYPE_ATTR_ABSENT );
				return( CRYPT_ARGERROR_NUM1 );
				}
			return( krnlSendMessage( certInfoPtr->ownerHandle,
									 IMESSAGE_USER_TRUSTMGMT,
									 &certInfoPtr->objectHandle,
									 certInfo ? MESSAGE_TRUSTMGMT_ADD : \
												MESSAGE_TRUSTMGMT_DELETE ) );
			}

#ifdef USE_CERTREV
		case CRYPT_CERTINFO_SIGNATURELEVEL:
			certInfoPtr->cCertRev->signatureLevel = certInfo;
			return( CRYPT_OK );
#endif /* USE_CERTREV */

		case CRYPT_CERTINFO_VERSION:
			certInfoPtr->version = certInfo;
			return( CRYPT_OK );

		case CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO:
			return( copyPublicKeyInfo( certInfoPtr, certInfo, NULL ) );

		case CRYPT_CERTINFO_CERTIFICATE:
			/* If it's a certificate, copy across various components or
			   store the entire certificate where required */
			status = krnlSendMessage( certInfo, IMESSAGE_GETDEPENDENT, 
									  &addedCert, OBJECT_TYPE_CERTIFICATE );
			if( cryptStatusError( status ) )
				return( status );

			/* If it's a certificate chain then we're adding the complete 
			   certificate, just store it and exit */
			if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
				{
				CERT_CERT_INFO *certCertInfoPtr = certInfoPtr->cCertCert;
				int i, LOOP_ITERATOR;

				if( certCertInfoPtr->chainEnd >= MAX_CHAINLENGTH - 1 )
					return( CRYPT_ERROR_OVERFLOW );

				/* Perform a simple check to make sure that it hasn't been
				   added already */
				LOOP_EXT( i = 0, 
						  i < certCertInfoPtr->chainEnd && i < MAX_CHAINLENGTH,
						  i++, MAX_CHAINLENGTH + 1 )
					{
					if( cryptStatusOK( \
						krnlSendMessage( addedCert, IMESSAGE_COMPARE,
										 &certCertInfoPtr->chain[ i ],
										 MESSAGE_COMPARE_CERTOBJ ) ) )
						{
						setErrorInfo( certInfoPtr,
									  CRYPT_CERTINFO_CERTIFICATE,
									  CRYPT_ERRTYPE_ATTR_PRESENT );
						return( CRYPT_ERROR_INITED );
						}
					}
				ENSURES( LOOP_BOUND_OK );

				/* Add the user certificate and increment its reference 
				   count */
				krnlSendNotifier( addedCert, IMESSAGE_INCREFCOUNT );
				certCertInfoPtr->chain[ certCertInfoPtr->chainEnd++ ] = addedCert;

				return( CRYPT_OK );
				}

			return( copyCertObject( certInfoPtr, addedCert, 
									CRYPT_CERTINFO_CERTIFICATE, certInfo ) );

		case CRYPT_CERTINFO_CACERTIFICATE:
			/* We can't add another CA certificate if there's already one 
			   present, in theory this is valid but it's more likely to be 
			   an implementation problem than an attempt to query multiple 
			   CAs through a single responder */
			if( certInfoPtr->certHashSet )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CACERTIFICATE,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			/* Get the certificate handle and make sure that it really is a 
			   CA certificate */
			status = krnlSendMessage( certInfo, IMESSAGE_GETDEPENDENT, 
									  &addedCert, OBJECT_TYPE_CERTIFICATE );
			if( cryptStatusError( status ) )
				return( status );
			if( !checkContextCapability( addedCert, 
										 MESSAGE_CHECK_PKC_SIGCHECK_CA ) && \
				!checkContextCapability( addedCert, 
										 MESSAGE_CHECK_PKC_SIGN_CA ) )
				return( CRYPT_ARGERROR_NUM1 );

			return( copyCertObject( certInfoPtr, addedCert, 
									CRYPT_CERTINFO_CACERTIFICATE, CRYPT_UNUSED ) );

#ifdef USE_CERTREQ
		case CRYPT_CERTINFO_CERTREQUEST:
			/* Make sure that we haven't already got a public key (either as
			   a context or encoded key data) or DN present */
			if( ( certInfoPtr->iPubkeyContext != CRYPT_ERROR || \
				  certInfoPtr->publicKeyInfo != NULL ) || \
				DATAPTR_ISSET( certInfoPtr->subjectName ) )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CERTREQUEST,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}

			return( copyCertObject( certInfoPtr, certInfo, 
									CRYPT_CERTINFO_CERTREQUEST, CRYPT_UNUSED ) );
#endif /* USE_CERTREQ */

#ifdef USE_PKIUSER
		case CRYPT_CERTINFO_PKIUSER_RA:
			/* Make sure that this flag isn't already set */
			if( certInfoPtr->cCertUser->isRA )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_CERTREQUEST,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			certInfoPtr->cCertUser->isRA = certInfo;
			return( CRYPT_OK );
#endif /* USE_PKIUSER */

		case CRYPT_IATTRIBUTE_CERTCOLLECTION:
			return( copyCertChain( certInfoPtr, certInfo, TRUE ) );

		case CRYPT_IATTRIBUTE_RTCSREQUEST:
		case CRYPT_IATTRIBUTE_OCSPREQUEST:
		case CRYPT_IATTRIBUTE_REVREQUEST:
		case CRYPT_IATTRIBUTE_PKIUSERINFO:
#ifdef USE_DBMS	/* Only used by CA code */
		case CRYPT_IATTRIBUTE_BLOCKEDATTRS:
#endif /* USE_DBMS */
			return( copyCertObject( certInfoPtr, certInfo, certInfoType, 
									CRYPT_UNUSED ) );

#ifdef USE_CERTREQ
		case CRYPT_IATTRIBUTE_REQFROMRA:
			certInfoPtr->cCertReq->requestFromRA = certInfo;
			return( CRYPT_OK );
#endif /* USE_CERTREQ */
		}

	retIntError();
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int addCertComponentString( INOUT CERT_INFO *certInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
							IN_BUFFER( certInfoLength ) const void *certInfo, 
							IN_LENGTH_SHORT const int certInfoLength )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtrDynamic( certInfo, certInfoLength ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );
	REQUIRES( isShortIntegerRangeNZ( certInfoLength ) );

	/* If we're adding data to a certificate, clear the error information */
	if( !isPseudoInformation( certInfoType ) )
		clearErrorInfo( certInfoPtr );

	/* If it's a GeneralName or DN component, add it.  These are special-
	   case attribute values so they have to come before the attribute-
	   handling code */
	if( isGeneralNameComponent( certInfoType ) )
		{
		CRYPT_ATTRIBUTE_TYPE fieldID;
		SELECTION_INFO savedSelectionInfo DUMMY_INIT_STRUCT;
		BOOLEAN isImplicitSelect = FALSE, cursorInGeneralName = FALSE;

		/* GeneralName DN components have an awkward special-case processing
		   requirement based on the fact that we try and hide the need to
		   stuff a value like an email address or server name into an altName
		   extension in order to be able to store it.  This means that, while
		   adding a CN just adds a CN, adding an email address needs to do:

			create altName if required;
			select altName;
			add email address (to altName);

		   However, once the altName has been implicitly selected in this
		   manner, adding further DN components adds them to the (implicitly-
		   selected) DN in the altName rather than the subjectName.

		   Because of this, if the currently selected DN is the subjectName
		   then we have to reset the selection after we add the component to
		   ensure that subsequent DN values don't end up in the altName DN */
		if( isSubjectNameSelected( certInfoPtr ) || \
			isIssuerNameSelected( certInfoPtr ) )
			{
			savedSelectionInfo = certInfoPtr->currentSelection;
			isImplicitSelect = TRUE;
			}

		/* Another special-case situation can occur when the cursor is 
		   pointing at the GeneralName and the addition of the new attribute
		   field changes the structure of the GeneralName.  In the worst 
		   case a new GeneralName field may be added before the current
		   cursor position, so that after the add it points past the start 
		   of the GeneralName.  To deal with this we have to remember to fix 
		   up the cursor after the add if required */
		if( DATAPTR_ISSET( certInfoPtr->attributes ) && \
			DATAPTR_SAME( certInfoPtr->attributes, 
						  certInfoPtr->attributeCursor ) )
			cursorInGeneralName = TRUE;

		status = selectGeneralName( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
									CREATE_IF_ABSENT );
		if( cryptStatusError( status ) )
			return( status );
		if( DATAPTR_ISSET( certInfoPtr->attributeCursor ) )
			{
			status = getAttributeIdInfo( certInfoPtr->attributeCursor, NULL, 
										 &fieldID, NULL );
			if( cryptStatusError( status ) )
				return( status );
			}
		else
			fieldID = certInfoPtr->currentSelection.generalName;
		status = addAttributeFieldString( &certInfoPtr->attributes, 
							fieldID, certInfoType, certInfo, certInfoLength, 
							ATTR_FLAG_NONE, &certInfoPtr->errorLocus, 
							&certInfoPtr->errorType );
		if( cryptStatusError( status ) )
			return( status );
		if( isImplicitSelect )
			{
			/* The GeneralName was implicitly selected (for example by the 
			   addition of an email address, which switches the selection 
			   from the subjectName to the altName), re-select the original
			   name */
			certInfoPtr->currentSelection = savedSelectionInfo;
			}
		else
			{
			/* If we couldn't update the cursor earlier on because the
			   attribute field in question hadn't been created yet, do it
			   now.  Since this is merely a side-effect of this operation, 
			   we ignore the return status */
			if( certInfoPtr->currentSelection.updateCursor )
				{
				( void ) selectGeneralName( certInfoPtr,
								certInfoPtr->currentSelection.generalName,
								MAY_BE_ABSENT );
				}
			}
		if( cursorInGeneralName && \
			!DATAPTR_SAME( certInfoPtr->attributes, 
						   certInfoPtr->attributeCursor ) )
			{
			/* The attribute cursor was pointing at the GeneralName, but the
			   addition of a new field has modified its structure, we need 
			   to reset the cursor to point at the start of the updated
			   GeneralName.  This is somewhat complex because the 
			   GeneralName is still selected, but the cursor now points past
			   the start of it, so an isGeneralNameSelected() check in
			   comp_curs.c will always pass.  To deal with this, we 
			   explicitly clear the attribute cursor and then tell
			   selectGeneralName() to re-select the GeneralName, which sets
			   the cursor to the start of the GeneralName */
			DATAPTR_SET( certInfoPtr->attributeCursor, NULL );
			certInfoPtr->currentSelection.generalName = fieldID;
			status = selectGeneralName( certInfoPtr, 
								CRYPT_ATTRIBUTE_NONE,
								MUST_BE_PRESENT );
			ENSURES( cryptStatusOK( status ) );
			}
		return( CRYPT_OK );
		}
	if( isDNComponent( certInfoType ) )
		{
		/* Add the string component to the DN */
		status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
						   CREATE_IF_ABSENT );
		if( cryptStatusError( status ) )
			{
			certInfoPtr->errorLocus = certInfoType;
			return( status );
			}
		status = insertDNComponent( certInfoPtr->currentSelection.dnPtr,
									certInfoType, certInfo, 
									certInfoLength, &certInfoPtr->errorType );
		if( cryptStatusOK( status ) && \
			certInfoPtr->currentSelection.updateCursor )
			{
			/* If we couldn't update the cursor earlier on because the
			   attribute field in question hadn't been created yet, do it
			   now.  Since this is merely a side-effect of this operation, 
			   we ignore the return status */
			( void ) selectGeneralName( certInfoPtr,
										certInfoPtr->currentSelection.generalName,
										MAY_BE_ABSENT );
			}
		if( cryptStatusError( status ) && status != CRYPT_ERROR_MEMORY )
			certInfoPtr->errorLocus = certInfoType;
		return( status );
		}

	/* If it's standard certificate or CMS attribute, add it to the 
	   certificate */
	if( isValidExtension( certInfoType ) )
		{
		int localCertInfoType = certInfoType;

		/* If it's a CRL, RTCS, or OCSP per-entry attribute, add the
		   attribute to the currently selected entry unless it's a
		   revocation request, in which case it goes in with the main
		   attributes */
#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
		if( isRevocationEntryComponent( localCertInfoType ) && \
			certInfoPtr->type != CRYPT_CERTTYPE_REQUEST_REVOCATION )
			{
			switch( certInfoPtr->type )
				{
  #ifdef USE_CERTVAL
				case CRYPT_CERTTYPE_RTCS_REQUEST:
				case CRYPT_CERTTYPE_RTCS_RESPONSE:
					{
					CERT_VAL_INFO *certValInfo = certInfoPtr->cCertVal;
					VALIDITY_INFO *validityInfoPtr;

					if( DATAPTR_ISNULL( certValInfo->currentValidity ) )
						return( CRYPT_ERROR_NOTFOUND );
					validityInfoPtr = DATAPTR_GET( certValInfo->currentValidity );
					ENSURES( validityInfoPtr != NULL );
					return( addAttributeFieldString( &validityInfoPtr->attributes,
									localCertInfoType, CRYPT_ATTRIBUTE_NONE,
									certInfo, certInfoLength, ATTR_FLAG_NONE,
									&certInfoPtr->errorLocus, 
									&certInfoPtr->errorType ) );
					}
  #endif /* USE_CERTVAL */
  #ifdef USE_CERTREV
				case CRYPT_CERTTYPE_CRL:
				case CRYPT_CERTTYPE_OCSP_REQUEST:
				case CRYPT_CERTTYPE_OCSP_RESPONSE:
					{
					CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;
					REVOCATION_INFO *revInfoPtr;

					if( DATAPTR_ISNULL( certRevInfo->currentRevocation ) )
						return( CRYPT_ERROR_NOTFOUND );
					revInfoPtr = DATAPTR_GET( certRevInfo->currentRevocation );
					ENSURES( revInfoPtr != NULL );
					return( addAttributeFieldString( &revInfoPtr->attributes,
									localCertInfoType, CRYPT_ATTRIBUTE_NONE,
									certInfo, certInfoLength, ATTR_FLAG_NONE,
									&certInfoPtr->errorLocus, 
									&certInfoPtr->errorType ) );
					}
  #endif /* USE_CERTREV */
				}

			retIntError();
			}
#endif /* USE_CERTREV || USE_CERTVAL */

		return( addAttributeFieldString( &certInfoPtr->attributes,
						localCertInfoType, CRYPT_ATTRIBUTE_NONE, certInfo, 
						certInfoLength, ATTR_FLAG_NONE, 
						&certInfoPtr->errorLocus, &certInfoPtr->errorType ) );
		}

	/* If it's anything else, handle it specially */
	switch( certInfoType )
		{
		case CRYPT_CERTINFO_SERIALNUMBER:
			ENSURES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE );
			if( certInfoPtr->cCertCert->serialNumber != NULL )
				{
				setErrorInfo( certInfoPtr, CRYPT_CERTINFO_SERIALNUMBER,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			return( setSerialNumber( certInfoPtr, certInfo,
									 certInfoLength ) );

		case CRYPT_CERTINFO_VALIDFROM:
		case CRYPT_CERTINFO_THISUPDATE:
			{
			const time_t certTime = *( ( time_t * ) certInfo );

			if( certInfoPtr->startTime > 0 )
				{
				setErrorInfo( certInfoPtr, certInfoType,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			if( certInfoPtr->endTime > 0 && \
				certTime >= certInfoPtr->endTime )
				{
				setErrorInfo( certInfoPtr,
							  ( certInfoType == CRYPT_CERTINFO_VALIDFROM ) ? \
								CRYPT_CERTINFO_VALIDTO : CRYPT_CERTINFO_NEXTUPDATE,
							  CRYPT_ERRTYPE_CONSTRAINT );
				return( CRYPT_ARGERROR_STR1 );
				}
			certInfoPtr->startTime = certTime;
			return( CRYPT_OK );
			}

		case CRYPT_CERTINFO_VALIDTO:
		case CRYPT_CERTINFO_NEXTUPDATE:
			{
			const time_t certTime = *( ( time_t * ) certInfo );

			if( certInfoPtr->endTime > 0 )
				{
				setErrorInfo( certInfoPtr, certInfoType,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			if( certInfoPtr->startTime > 0 && \
				certTime <= certInfoPtr->startTime )
				{
				setErrorInfo( certInfoPtr,
							  ( certInfoType == CRYPT_CERTINFO_VALIDTO ) ? \
								CRYPT_CERTINFO_VALIDFROM : CRYPT_CERTINFO_THISUPDATE,
							  CRYPT_ERRTYPE_CONSTRAINT );
				return( CRYPT_ARGERROR_STR1 );
				}
			certInfoPtr->endTime = certTime;
			return( CRYPT_OK );
			}

#ifdef USE_CERTREV
		case CRYPT_CERTINFO_REVOCATIONDATE:
			{
			const time_t certTime = *( ( time_t * ) certInfo );
			time_t *revocationTimePtr = getRevocationTimePtr( certInfoPtr );

			if( revocationTimePtr == NULL )
				return( CRYPT_ERROR_NOTFOUND );
			if( *revocationTimePtr > 0 )
				{
				setErrorInfo( certInfoPtr, certInfoType,
							  CRYPT_ERRTYPE_ATTR_PRESENT );
				return( CRYPT_ERROR_INITED );
				}
			*revocationTimePtr = certTime;
			return( CRYPT_OK );
			}
#endif /* USE_CERTREV */

#ifdef USE_CERT_DNSTRING
		case CRYPT_CERTINFO_DN:
			return( getEncodedDn( certInfoPtr, certInfo, certInfoLength ) );
#endif /* USE_CERT_DNSTRING */

#ifdef USE_CERTREV
		case CRYPT_IATTRIBUTE_CRLENTRY:
			{
			STREAM stream;

			ENSURES( certInfoPtr->type == CRYPT_CERTTYPE_CRL );

			/* The revocation information is being provided to us in pre-
			   encoded form from a certificate store, decode it so that we 
			   can add it to the CRL */
			sMemConnect( &stream, certInfo, certInfoLength );
			status = readCRLentry( &stream,
								   &certInfoPtr->cCertRev->revocations, 0,
								   &certInfoPtr->errorLocus,
								   &certInfoPtr->errorType );
			sMemDisconnect( &stream );
			return( status );
			}
#endif /* USE_CERTREV */

#ifdef USE_CERTREQ
		case CRYPT_IATTRIBUTE_AUTHCERTID:
			ENSURES( certInfoLength == KEYID_SIZE );
			memcpy( certInfoPtr->cCertReq->authCertID, certInfo, 
					KEYID_SIZE );
			return( CRYPT_OK );
#endif /* USE_CERTREQ */
		}

	retIntError();
	}
#endif /* USE_CERTIFICATES */
