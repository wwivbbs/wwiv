/****************************************************************************
*																			*
*						Get Certificate String Components					*
*						Copyright Peter Gutmann 1997-2009					*
*																			*
****************************************************************************/

#include <stdio.h>		/* For sprintf() */
#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* The maximum magnitude of an individual OID arc.  Anything larger than 
   this is most likely an error (or something from Microsoft) */

#define OID_ARC_MAX		0x1000000L	/* 2 ^ 28 */

/* The minimum size for an OBJECT IDENTIFIER expressed as ASCII characters */

#define MIN_ASCII_OIDSIZE	7

/* Convert a binary OID to its text form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
static int oidToText( IN_BUFFER( binaryOidLen ) const BYTE *binaryOID, 
					  IN_LENGTH_OID const int binaryOidLen,
					  OUT_BUFFER( maxOidLen, *oidLen ) char *oid, 
					  IN_LENGTH_SHORT_MIN( 16 ) const int maxOidLen, 
					  OUT_LENGTH_SHORT_Z int *oidLen )
	{
	long value = 0;
	int i, length = 0, subLen;

	assert( isReadPtr( binaryOID, binaryOidLen ) );
	assert( isWritePtr( oid, maxOidLen ) );
	assert( isWritePtr( oidLen, sizeof( int ) ) );

	REQUIRES( binaryOidLen >= MIN_OID_SIZE && \
			  binaryOidLen <= MAX_OID_SIZE && \
			  binaryOidLen == sizeofOID( binaryOID ) );
	REQUIRES( maxOidLen >= 16 && maxOidLen < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	memset( oid, 0, min( 16, maxOidLen ) );
	*oidLen = 0;

	for( i = 2; i < binaryOidLen; i++ )
		{
		const BYTE data = binaryOID[ i ];
		const long valTmp = value << 7;

		/* Pick apart the encoding */
		if( value == 0 && data == 0x80 )
			{
			/* Invalid leading zero value, ( 0x80 & 0x7F ) == 0 */
			return( CRYPT_ERROR_BADDATA );
			}
		if( value >= ( OID_ARC_MAX >> 7 ) || \
			valTmp >= OID_ARC_MAX - ( data & 0x7F ) )
			return( CRYPT_ERROR_BADDATA );	/* Overflow */
		value = valTmp | ( data & 0x7F );
		if( value < 0 || value > OID_ARC_MAX )
			return( CRYPT_ERROR_BADDATA );	/* Range error */
		if( !( data & 0x80 ) )
			{
			/* Make sure that we don't overflow the buffer.  The value 20 is 
			   the maximum magnitude of a 64-bit int plus space plus 1-byte 
			   overflow */
			if( length >= maxOidLen - 20 )
				return( CRYPT_ERROR_BADDATA );

			if( length == 0 )
				{
				long x, y;

				/* The first two levels are encoded into one byte since the 
				   root level has only 3 nodes (40*x + y), however if x = 
				   joint-iso-itu-t(2) then y may be > 39, so we have to add 
				   special-case handling for this */
				x = value / 40;
				y = value % 40;
				if( x > 2 )
					{
					/* Handle special case for large y if x == 2 */
					y += ( x - 2 ) * 40;
					x = 2;
					}
				if( x < 0 || x > 2 || y < 0 || \
					( ( x < 2 && y > 39 ) || \
					  ( x == 2 && ( y > 50 && y != 100 ) ) ) )
					{
					/* If x = 0 or 1 then y has to be 0...39, for x = 3
					   it can take any value but there are no known 
					   assigned values over 50 except for one contrived
					   example in X.690 which sets y = 100, so if we see
					   something outside this range it's most likely an 
					   encoding error rather than some bizarre new ID 
					   that's just appeared */
					return( CRYPT_ERROR_BADDATA );
					}
				subLen = sprintf_s( oid, maxOidLen, "%ld %ld", x, y );
				}
			else
				{
				subLen = sprintf_s( oid + length, maxOidLen - length, 
									" %ld", value );
				}
			if( subLen < 2 || subLen > maxOidLen - length )
				return( CRYPT_ERROR_BADDATA );
			length += subLen;
			value = 0;
			}

		}
	if( value != 0 )
		{
		/* We stopped in the middle of a continued value, it's an invalid
		   encoding */
		return( CRYPT_ERROR_BADDATA );
		}
	*oidLen = length;

	return( CRYPT_OK );
	}

/* Convert an ASCII OID arc sequence into an encoded OID.  We allow dots as 
   well as whitespace for arc separators, these are an IETF-ism but are in 
   common use */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int scanValue( IN_BUFFER( strMaxLength ) const char *string, 
					  IN_LENGTH_TEXT const int strMaxLength,
					  OUT_INT_Z long *value )
	{
	int intValue, index, status;

	assert( isReadPtr( string, strMaxLength ) );
	assert( isWritePtr( value, sizeof( long ) ) );

	REQUIRES( strMaxLength > 0 && strMaxLength <= CRYPT_MAX_TEXTSIZE );

	/* Clear return value */
	*value = 0;

	/* Look for the end of the arc */
	for( index = 0; index < strMaxLength; index++ )
		{
		if( string[ index ] == ' ' || string[ index ] == '.' )
			break;
		}
	if( index <= 0 || index > CRYPT_MAX_TEXTSIZE )
		return( -1 );
	status = strGetNumeric( string, index, &intValue, 0, OID_ARC_MAX );
	if( cryptStatusError( status ) )
		return( -1 );
	*value = intValue;
	if( index < strMaxLength && \
		( string[ index ] == ' ' || string[ index ] == '.' ) )
		{
		/* There's more to go, skip the delimiter */
		index++;
		}
	return( index );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int textToOID( IN_BUFFER( textOidLength ) const char *textOID, 
			   IN_LENGTH_TEXT const int textOidLength, 
			   OUT_BUFFER( binaryOidMaxLen, *binaryOidLen ) BYTE *binaryOID, 
			   IN_LENGTH_SHORT const int binaryOidMaxLen, 
			   OUT_LENGTH_SHORT_Z int *binaryOidLen )
	{
	const char *textOidPtr;
	long value, value2;
	int length = 3, subLen, dataLeft, iterationCount, status;

	assert( isReadPtr( textOID, textOidLength ) );
	assert( isWritePtr( binaryOID, binaryOidMaxLen ) );
	assert( isWritePtr( binaryOidLen, sizeof( int ) ) );

	REQUIRES( textOidLength >= MIN_ASCII_OIDSIZE && \
			  textOidLength <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( binaryOidMaxLen >= 5 && \
			  binaryOidMaxLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	memset( binaryOID, 0, min( 16, binaryOidMaxLen ) );
	*binaryOidLen = 0;

	/* Perform some basic checks on the OID data */
	status = dataLeft = strStripWhitespace( &textOidPtr, textOID, 
											textOidLength );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_BADDATA );

	/* Make sure that the first two arcs are in order */
	subLen = scanValue( textOidPtr, dataLeft, &value );
	if( subLen <= 0 || subLen > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );
	textOidPtr += subLen;
	dataLeft -= subLen;
	if( dataLeft <= 0 || dataLeft > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );
	subLen = scanValue( textOidPtr, dataLeft, &value2 );
	if( subLen <= 0 || subLen > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );
	textOidPtr += subLen;
	dataLeft -= subLen;
	if( dataLeft <= 0 || dataLeft > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_BADDATA );
	if( value < 0 || value > 2 || value2 < 1 || \
		( ( value < 2 && value2 > 39 ) || ( value == 2 && value2 > 175 ) ) )
		return( CRYPT_ERROR_BADDATA );
	binaryOID[ 0 ] = 0x06;	/* OBJECT IDENTIFIER tag */
	binaryOID[ 2 ] = ( BYTE )( ( value * 40 ) + value2 );

	/* Convert the remaining arcs */
	for( iterationCount = 0; 
		 dataLeft > 0 && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		BOOLEAN hasHighBits = FALSE;

		/* Scan the next value and write the high octets (if necessary) with
		   flag bits set, followed by the final octet */
		subLen = scanValue( textOidPtr, dataLeft, &value );
		if( subLen <= 0 || subLen > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ERROR_BADDATA );
		textOidPtr += subLen;
		dataLeft -= subLen;
		if( dataLeft < 0 || dataLeft > CRYPT_MAX_TEXTSIZE )
			return( CRYPT_ERROR_BADDATA );
		if( value >= 0x200000L )					/* 2^21 */
			{
			if( length >= binaryOidMaxLen )
				return( CRYPT_ERROR_BADDATA );
			binaryOID[ length++ ] = intToByte( 0x80 | ( value >> 21 ) );
			value %= 0x200000L;
			hasHighBits = TRUE;
			}
		if( ( value >= 0x4000 ) || hasHighBits )	/* 2^14 */
			{
			if( length >= binaryOidMaxLen )
				return( CRYPT_ERROR_BADDATA );
			binaryOID[ length++ ] = intToByte( 0x80 | ( value >> 14 ) );
			value %= 0x4000;
			hasHighBits = TRUE;
			}
		if( ( value >= 0x80 ) || hasHighBits )		/* 2^7 */
			{
			if( length >= binaryOidMaxLen )
				return( CRYPT_ERROR_BADDATA );
			binaryOID[ length++ ] = intToByte( 0x80 | ( value >> 7 ) );
			value %= 128;
			}
		if( length >= binaryOidMaxLen )
			return( CRYPT_ERROR_BADDATA );
		binaryOID[ length++ ] = intToByte( value );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	binaryOID[ 1 ] = intToByte( length - 2 );
	*binaryOidLen = length;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Get Certificate Components						*
*																			*
****************************************************************************/

/* Get a certificate component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int getCertAttributeComponent( const CERT_INFO *certInfoPtr,
					IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
					OUT_BUFFER_OPT( certInfoMaxLength, certInfoLength ) \
						void *certInfo, 
					IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
					OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	ATTRIBUTE_PTR *attributePtr;
	void *dataPtr;
	int dataLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( certInfoType > CRYPT_ATTRIBUTE_NONE && \
			  certInfoType < CRYPT_ATTRIBUTE_LAST );
	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* Try and find this attribute in the attribute list */
	attributePtr = findAttributeComponent( certInfoPtr, certInfoType );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* String fields never have default values (only BOOLEANs do) and never 
	   denote complete-attribute entries (these are indicated by a present/
	   not-present BOOLEAN) */
	ENSURES( !checkAttributeProperty( attributePtr, 
									  ATTRIBUTE_PROPERTY_DEFAULTVALUE ) );
	ENSURES( !checkAttributeProperty( attributePtr,
									  ATTRIBUTE_PROPERTY_COMPLETEATRIBUTE ) );

	/* If the data type is an OID we have to convert it to a human-readable
	   form before we return it */
	if( checkAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_OID ) )
		{
		char textOID[ ( CRYPT_MAX_TEXTSIZE * 2 ) + 8 ];
		int textOidLength;

		status = getAttributeDataPtr( attributePtr, &dataPtr, &dataLength );
		if( cryptStatusError( status ) )
			return( status );
		status = oidToText( dataPtr, dataLength, textOID, 
							CRYPT_MAX_TEXTSIZE * 2, &textOidLength );
		if( cryptStatusError( status ) )
			return( status );
		*certInfoLength = textOidLength;
		if( certInfo == NULL )
			return( CRYPT_OK );
		return( attributeCopyParams( certInfo, certInfoMaxLength, 
									 certInfoLength, textOID, 
									 textOidLength ) );
		}

	/* Get the attribute component data */
	status = getAttributeDataPtr( attributePtr, &dataPtr, &dataLength );
	if( cryptStatusError( status ) )
		return( status );
	return( attributeCopyParams( certInfo, certInfoMaxLength, 
								 certInfoLength, dataPtr, dataLength ) );
	}

/* Get the hash of a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int getCertHash( INOUT CERT_INFO *certInfoPtr,
						IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType, 
						OUT_BUFFER_OPT( certInfoMaxLength, \
										*certInfoLength ) void *certInfo, 
						IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	static const MAP_TABLE hashAlgoMapTbl[] = {
		{ CRYPT_CERTINFO_FINGERPRINT_MD5, CRYPT_ALGO_MD5 },
		{ CRYPT_CERTINFO_FINGERPRINT_SHA1, CRYPT_ALGO_SHA1 },
		{ CRYPT_CERTINFO_FINGERPRINT_SHA2, CRYPT_ALGO_SHA2 },
		{ CRYPT_CERTINFO_FINGERPRINT_SHAng, CRYPT_ALGO_SHAng },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	HASHFUNCTION_ATOMIC hashFunctionAtomic;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ];
	int hashAlgo, hashSize, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( certInfoType == CRYPT_CERTINFO_FINGERPRINT_MD5 || \
			  certInfoType == CRYPT_CERTINFO_FINGERPRINT_SHA1 || \
			  certInfoType == CRYPT_CERTINFO_FINGERPRINT_SHA2 || \
			  certInfoType == CRYPT_CERTINFO_FINGERPRINT_SHAng );
	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* Get the hash algorithm information */
	status = mapValue( certInfoType, &hashAlgo, hashAlgoMapTbl, 
					   FAILSAFE_ARRAYSIZE( hashAlgoMapTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );
	getHashAtomicParameters( hashAlgo, 0, &hashFunctionAtomic, &hashSize );
	*certInfoLength = hashSize;
	if( certInfo == NULL )
		return( CRYPT_OK );
	ENSURES( certInfoPtr->certificate != NULL );

	/* Write the hash (fingerprint) to the output */
	if( hashAlgo == CRYPT_ALGO_SHA1 && certInfoPtr->certHashSet )
		{
		/* If we've got a cached certificate hash present, return that instead of 
		   re-hashing the certificate */
		return( attributeCopyParams( certInfo, certInfoMaxLength, 
									 certInfoLength, certInfoPtr->certHash, 
									 KEYID_SIZE ) );
		}
	hashFunctionAtomic( hash, CRYPT_MAX_HASHSIZE, certInfoPtr->certificate,
						certInfoPtr->certificateSize );
	if( hashAlgo == CRYPT_ALGO_SHA1 )
		{
		/* Remember the hash/fingerprint/oobCertID/certHash/thumbprint/
		   whatever for later since this is reused frequently */
		memcpy( certInfoPtr->certHash, hash, hashSize );
		certInfoPtr->certHashSet = TRUE;
		}
	return( attributeCopyParams( certInfo, certInfoMaxLength, 
								 certInfoLength, hash, hashSize ) );
	}

/* Get the ESSCertID for a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getESSCertID( INOUT CERT_INFO *certInfoPtr, 
						 OUT_BUFFER_OPT( certInfoMaxLength, \
										 *certInfoLength ) void *certInfo, 
						 IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						 OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	STREAM stream;
	BYTE certHash[ CRYPT_MAX_HASHSIZE + 8 ];
	int certHashSize, issuerSerialDataSize, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* Get the certificate ID */
	status = getCertHash( certInfoPtr, CRYPT_CERTINFO_FINGERPRINT_SHA1, 
						  certHash, CRYPT_MAX_HASHSIZE, &certHashSize );
	if( cryptStatusError( status ) )
		return( status );
	REQUIRES( certInfoPtr->cCertCert->serialNumber != NULL );

	/* Write the ESSCertID:

		ESSCertID ::= SEQUENCE {
			certHash		OCTET STRING SIZE(20),
			issuerSerial	SEQUENCE {
				issuer		SEQUENCE { [4] EXPLICIT Name },
				serial		INTEGER
				}
			} */
	issuerSerialDataSize = ( int ) \
			sizeofObject( sizeofObject( certInfoPtr->issuerDNsize ) ) + \
			sizeofInteger( certInfoPtr->cCertCert->serialNumber,
						   certInfoPtr->cCertCert->serialNumberLength );
	*certInfoLength = ( int ) \
			sizeofObject( sizeofObject( certHashSize ) + \
						  sizeofObject( issuerSerialDataSize ) );
	if( certInfo == NULL )
		return( CRYPT_OK );
	if( *certInfoLength <= 0 || *certInfoLength > certInfoMaxLength )
		return( CRYPT_ERROR_OVERFLOW );
	sMemOpen( &stream, certInfo, *certInfoLength );
	writeSequence( &stream, sizeofObject( certHashSize ) + \
							sizeofObject( issuerSerialDataSize ) );
	writeOctetString( &stream, certHash, certHashSize, DEFAULT_TAG );
	writeSequence( &stream, issuerSerialDataSize );
	writeSequence( &stream, sizeofObject( certInfoPtr->issuerDNsize ) );
	writeConstructed( &stream, certInfoPtr->issuerDNsize, 4 );
	swrite( &stream, certInfoPtr->issuerDNptr, certInfoPtr->issuerDNsize );
	status = writeInteger( &stream, certInfoPtr->cCertCert->serialNumber,
						   certInfoPtr->cCertCert->serialNumberLength, 
						   DEFAULT_TAG );
	sMemDisconnect( &stream );
	ENSURES( cryptStatusOK( status ) );

	return( status );
	}

/****************************************************************************
*																			*
*							Get Validity Components							*
*																			*
****************************************************************************/

#ifdef USE_CERTVAL

/* Get a pointer to the currently selected validity time */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
time_t *getValidityTimePtr( const CERT_INFO *certInfoPtr )
	{
	CERT_VAL_INFO *certValInfo = certInfoPtr->cCertVal;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES_N( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE );

	/* If there's a specific validity entry selected get its invalidity time, 
	   otherwise if there are invalid certificates present get the first 
	   certificate's invalidity time, otherwise get the default invalidity 
	   time */
	return( ( certValInfo->currentValidity != NULL ) ? \
				&certValInfo->currentValidity->invalidityTime : \
			( certValInfo->validityInfo != NULL ) ? \
				&certValInfo->validityInfo->invalidityTime : NULL );
	}
#endif /* USE_CERTVAL */

/****************************************************************************
*																			*
*							Get Revocation Components						*
*																			*
****************************************************************************/

#ifdef USE_CERTREV

/* Encode a single CRL entry into the external format, used when storing a
   CRL to a certificate store */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getCrlEntry( INOUT CERT_INFO *certInfoPtr, 
						OUT_BUFFER_OPT( certInfoMaxLength, \
										*certInfoLength ) void *certInfo, 
						IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;
	STREAM stream;
	WRITECERT_FUNCTION writeCertFunction;
	int crlEntrySize = DUMMY_INIT, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CRL );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	if( certRevInfo->currentRevocation == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* Determine how big the encoded CRL entry will be.  Doing it directly
	   in this manner is somewhat ugly but the only other way to do it would 
	   be to pseudo-sign the certificate object in order to write the data, 
	   which doesn't work for CRL entries where we could end up pseudo-
	   signing it multiple times */
	writeCertFunction = getCertWriteFunction( CRYPT_CERTTYPE_CRL );
	ENSURES( writeCertFunction != NULL );
	sMemNullOpen( &stream );
	status = writeCertFunction( &stream, certInfoPtr, NULL, CRYPT_UNUSED );
	if( cryptStatusOK( status ) )
		crlEntrySize = stell( &stream );
	sMemClose( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Write the encoded single CRL entry */
	*certInfoLength = crlEntrySize;
	if( certInfo == NULL )
		return( CRYPT_OK );
	if( crlEntrySize <= 0 || crlEntrySize > certInfoMaxLength )
		return( CRYPT_ERROR_OVERFLOW );
	sMemOpen( &stream, certInfo, crlEntrySize );
	status = writeCertFunction( &stream, certInfoPtr, NULL,  CRYPT_UNUSED );
	sMemDisconnect( &stream );

	return( status );
	}

/* Get a pointer to the currently selected revocation time */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
time_t *getRevocationTimePtr( const CERT_INFO *certInfoPtr )
	{
	CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES_N( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
				certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
				certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );

	/* If there's a specific revocation entry selected, get its revocation 
	   time, otherwise if there are revoked certificates present get the 
	   first certificate's revocation time, otherwise get the default 
	   revocation time */
	return( ( certRevInfo->currentRevocation != NULL ) ? \
				&certRevInfo->currentRevocation->revocationTime : \
			( certRevInfo->revocations != NULL ) ? \
				&certRevInfo->revocations->revocationTime : \
			( certRevInfo->revocationTime ) ? \
				&certRevInfo->revocationTime : NULL );
	}
#endif /* USE_CERTREV */

/****************************************************************************
*																			*
*						Get Certificate Owner Components					*
*																			*
****************************************************************************/

/* Get the issuerAndSerialNumber for a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getIAndS( const CERT_INFO *certInfoPtr, 
					 OUT_BUFFER_OPT( certInfoMaxLength, \
									 *certInfoLength ) void *certInfo, 
					 IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
					 OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	STREAM stream;
	void *serialNumber;
	int serialNumberLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

#ifdef USE_CERTREV
	/* If it's a CRL, use the serial number of the currently selected CRL 
	   entry */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CRL )
		{
		REVOCATION_INFO *crlInfoPtr = certInfoPtr->cCertRev->currentRevocation;

		REQUIRES( crlInfoPtr != NULL );

		serialNumber = crlInfoPtr->id;
		serialNumberLength = crlInfoPtr->idLength;
		}
	else
#endif /* USE_CERTREV */
		{
		serialNumber = certInfoPtr->cCertCert->serialNumber;
		serialNumberLength = certInfoPtr->cCertCert->serialNumberLength;
		}
	ENSURES( serialNumber != NULL );
	*certInfoLength = ( int ) \
					  sizeofObject( certInfoPtr->issuerDNsize + \
									sizeofInteger( serialNumber, \
												   serialNumberLength ) );
	if( certInfo == NULL )
		return( CRYPT_OK );
	if( *certInfoLength <= 0 || *certInfoLength > certInfoMaxLength )
		return( CRYPT_ERROR_OVERFLOW );
	sMemOpen( &stream, certInfo, *certInfoLength );
	writeSequence( &stream, certInfoPtr->issuerDNsize + \
				   sizeofInteger( serialNumber, serialNumberLength ) );
	swrite( &stream, certInfoPtr->issuerDNptr, certInfoPtr->issuerDNsize );
	status = writeInteger( &stream, serialNumber, serialNumberLength,
						   DEFAULT_TAG );
	sMemDisconnect( &stream );

	return( status );
	}

/* Look for a named DN component (e.g. "surname = Smith") in an RFC 1779-
   encoded DN.  We have to use the text-string encoded form because we're
   looking for arbitrarily odd components not all of which are handled
   directly by cryptlib */

#if 0	/* 18/7/08 Unlikely that we'd ever find a certificate this broken,
		   and it's just a potential attack vector due to the complexity of
		   the processing */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 6 ) ) \
static int extractDnComponent( IN_BUFFER( encodedDnLength ) \
									const char *encodedDn, 
							   IN_LENGTH_SHORT const int encodedDnLength, 
							   IN_BUFFER( componentNameLength ) \
									const char *componentName, 
							   IN_LENGTH_SHORT const int componentNameLength,
							   OUT_LENGTH_SHORT_Z int *startOffset,
							   OUT_LENGTH_SHORT_Z int *length )
	{
	int startPos, endPos;

	assert( isReadPtr( encodedDn, encodedDnLength ) );
	assert( isReadPtr( componentName, componentNameLength ) );
	assert( isWritePtr( startOffset, sizeof( int ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( encodedDnLength > 0 && \
			  encodedDnLength < MAX_INTLENGTH_SHORT );
	REQUIRES( componentNameLength > 0 && \
			  componentNameLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*startOffset = *length = 0;
	
	/* Try and find the component type name in the RFC 1779-encoded DN 
	   string.  This scans the DN string for a matching type in a
	   type-and-value pair, e.g. "surname = Smith" */
	startPos = strFindStr( encodedDn, encodedDnLength, 
						   componentName, componentNameLength );
	if( startPos < 0 )
		return( -1 );
	startPos += componentNameLength;	/* Skip type indicator */
	
	/* Extract the component value */
	for( endPos = startPos; endPos < encodedDnLength && \
							encodedDn[ endPos ] != ',' && \
							encodedDn[ endPos ] != '+'; endPos++ );
	if( endPos > startPos && \
		encodedDn[ endPos ] == '+' && \
		encodedDn[ endPos - 1 ] == ' ' )
		endPos--;	/* Strip trailing space */
	if( endPos <= startPos )
		return( -1 );
	
	*startOffset = startPos;
	*length = endPos - startPos;

	return( CRYPT_OK );
	}

/* Assemble name components from an RFC 1779-encoded DN string.  We have to 
   use the text-string encoded form because we're looking for arbitrarily 
   odd components not all of which are handled directly by cryptlib */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4 ) ) \
static int getNameFromDN( OUT_BUFFER_OPT( nameMaxLength, *nameLength ) void *name, 
						  IN_LENGTH_SHORT_Z const int nameMaxLength, 
						  OUT_LENGTH_SHORT_Z int *nameLength, 
						  IN_BUFFER( encodedDnLength ) const char *encodedDn, 
						  IN_LENGTH_SHORT const int encodedDnLength )
	{
	int startPos, length, status;

	assert( ( name == NULL && nameMaxLength == 0 ) || \
			( isWritePtr( name, nameMaxLength ) ) );
	assert( isWritePtr( nameLength, sizeof( int ) ) );
	assert( isReadPtr( encodedDn, encodedDnLength ) );

	REQUIRES( ( name == NULL && nameMaxLength == 0 ) || \
			  ( name != NULL && \
				nameMaxLength > 0 && \
			    nameMaxLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( encodedDnLength > 0 && \
			  encodedDnLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	if( name != NULL )
		memset( name, 0, min( 16, nameMaxLength ) );
	*nameLength = 0;
	
	/* Look for a pseudonym */
	status = extractDnComponent( encodedDn, encodedDnLength, 
								 "oid.2.5.4.65=", 13, &startPos, &length );
	if( cryptStatusOK( status ) && \
		length > 0 && length <= nameMaxLength )
		{
		return( attributeCopyParams( name, nameMaxLength, nameLength, 
									 encodedDn + startPos, length ) );
		}

	/* Look for givenName + surname */
	status = extractDnComponent( encodedDn, encodedDnLength, 
								 "G=", 2, &startPos, &length );
	if( cryptStatusOK( status ) && \
		length > 0 && length <= nameMaxLength )
		{
		char nameBuffer[ MAX_ATTRIBUTE_SIZE + 8 ];
		int startPos2, length2;

		status = extractDnComponent( encodedDn, encodedDnLength, 
									 "S=", 2, &startPos2, &length2 );
		if( cryptStatusOK( status ) && \
			length2 > 0 && length + length2 <= nameMaxLength && \
						   length + length2 < MAX_ATTRIBUTE_SIZE )
			{
			memcpy( nameBuffer, encodedDn + startPos, length );
			memcpy( nameBuffer + length, encodedDn + startPos2, length2 );
			return( attributeCopyParams( name, nameMaxLength, nameLength, 
										 nameBuffer, length + length2 ) );
			}
		}

	/* We couldn't find anything useful */	
	return( CRYPT_ERROR_NOTFOUND );
	}
#endif /* 0 */

/* Get the certificate holder's name, usually the commonName but if that's
   not present then some commonName-equivalent */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getHolderName( const CERT_INFO *certInfoPtr, 
						  OUT_BUFFER_OPT( certInfoMaxLength, \
										  *certInfoLength ) void *certInfo, 
						  IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						  OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* First we try for a CN */
	status = getDNComponentValue( certInfoPtr->subjectName, 
								  CRYPT_CERTINFO_COMMONNAME, 0, certInfo, 
								  certInfoMaxLength, certInfoLength );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );

#if 0	/* 18/7/08 Unlikely that we'd ever find a certificate this broken,
		   and it's just a potential attack vector due to the complexity of
		   the processing */
	/* If that fails we try for either a pseudonym or givenName + surname.
	   Since these are part of the vast collection of oddball DN attributes
	   that aren't handled directly we have to get the encoded DN form and
	   look for them by OID (ugh) */
	sMemOpen( &stream, encodedDnBuffer, MAX_ATTRIBUTE_SIZE );
	status = writeDNstring( &stream, certInfoPtr->subjectName );
	if( cryptStatusOK( status ) )
		status = getNameFromDN( certInfo, certInfoMaxLength, certInfoLength, 
								encodedDnBuffer, stell( &stream ) );
	sMemDisconnect( &stream );
	if( cryptStatusOK( status ) )
		return( status );
#endif /* 0 */

	/* It's possible (although highly unlikely) that a certificate won't 
	   have a usable CN-equivalent in some form, in which case we use the OU
	   instead.  If that also fails we use the O.  This gets a bit messy, 
	   but duplicating the OU / O into the CN seems to be the best way to 
	   handle this */
	status = getDNComponentValue( certInfoPtr->subjectName, 
								  CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, 0,
								  certInfo, certInfoMaxLength, 
								  certInfoLength );
	if( cryptStatusError( status ) )
		status = getDNComponentValue( certInfoPtr->subjectName, 
									  CRYPT_CERTINFO_ORGANIZATIONNAME, 0,
									  certInfo, certInfoMaxLength, 
									  certInfoLength );
	return( status );
	}

/* Get the certificate holder's URI, usually an email address but sometimes
   also a URL */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getHolderURI( const CERT_INFO *certInfoPtr, 
						 OUT_BUFFER_OPT( certInfoMaxLength, \
										 *certInfoLength ) void *certInfo, 
						 IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						 OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	ATTRIBUTE_PTR *attributePtr;
	void *dataPtr;
	int dataLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* Find the subjectAltName, which contains the URI information */
	attributePtr = findAttribute( certInfoPtr->attributes,
								  CRYPT_CERTINFO_SUBJECTALTNAME, TRUE );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* There's altName data present, try for an email address and if that 
	   fails, a URL and an FQDN */
	attributePtr = findAttributeField( attributePtr, 
									   CRYPT_CERTINFO_SUBJECTALTNAME,
									   CRYPT_CERTINFO_RFC822NAME );
	if( attributePtr == NULL )
		attributePtr = findAttributeField( attributePtr, 
										   CRYPT_CERTINFO_SUBJECTALTNAME,
										   CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER );
	if( attributePtr == NULL )
		attributePtr = findAttributeField( attributePtr, 
										   CRYPT_CERTINFO_SUBJECTALTNAME,
										   CRYPT_CERTINFO_DNSNAME );
	if( attributePtr == NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* Get the attribute component data */
	status = getAttributeDataPtr( attributePtr, &dataPtr, &dataLength );
	if( cryptStatusError( status ) )
		return( status );
	return( attributeCopyParams( certInfo, certInfoMaxLength, 
								 certInfoLength, dataPtr, dataLength ) );
	}

/****************************************************************************
*																			*
*					Get Miscellaneous Certificate Components				*
*																			*
****************************************************************************/

#ifdef USE_PKIUSER

/* Encode PKI user information (IDs and passwords) into the external 
   text-encoded format */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
static int getPkiUserInfo( const CERT_INFO *certInfoPtr, 
						   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType, 
						   OUT_BUFFER_OPT( certInfoMaxLength, \
										   *certInfoLength ) void *certInfo, 
						   IN_LENGTH_SHORT_Z const int certInfoMaxLength, 
						   OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	CERT_PKIUSER_INFO *certUserInfo = certInfoPtr->cCertUser;
	char encUserInfo[ CRYPT_MAX_TEXTSIZE + 8 ];
	BYTE userInfo[ 128 + 8 ], *userInfoPtr = userInfo;
	int userInfoLength, encUserInfoLength, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( certInfoType == CRYPT_CERTINFO_PKIUSER_ID || \
			  certInfoType == CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD || \
			  certInfoType == CRYPT_CERTINFO_PKIUSER_REVPASSWORD );
	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
			    certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	if( certInfoType == CRYPT_CERTINFO_PKIUSER_ID )
		{
		status = getCertAttributeComponent( certInfoPtr,
											CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER,
											userInfo, 128, &userInfoLength );
		ENSURES( cryptStatusOK( status ) );
		}
	else
		{
		userInfoPtr = ( certInfoType == CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD ) ? \
					  certUserInfo->pkiIssuePW : certUserInfo->pkiRevPW;
		userInfoLength = PKIUSER_AUTHENTICATOR_SIZE;
		}
	status = encodePKIUserValue( encUserInfo, CRYPT_MAX_TEXTSIZE, 
								 &encUserInfoLength, userInfoPtr,
								 userInfoLength,
								 ( certInfoType == \
								   CRYPT_CERTINFO_PKIUSER_ID ) ? 3 : 4 );
	zeroise( userInfo, CRYPT_MAX_TEXTSIZE );
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( cryptStatusOK( \
				decodePKIUserValue( userInfo, 128, &userInfoLength,
									encUserInfo, encUserInfoLength ) ) );
	status = attributeCopyParams( certInfo, certInfoMaxLength, 
								  certInfoLength, encUserInfo, 
								  encUserInfoLength );
	zeroise( encUserInfo, CRYPT_MAX_TEXTSIZE );

	return( status );
	}
#endif /* USE_PKIUSER */

/****************************************************************************
*																			*
*							Get a Certificate Component						*
*																			*
****************************************************************************/

/* Get a certificate component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5 ) ) \
int getCertComponentString( INOUT CERT_INFO *certInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE certInfoType,
							OUT_BUFFER_OPT( certInfoMaxLength, \
											*certInfoLength ) void *certInfo, 
							IN_LENGTH_SHORT const int certInfoMaxLength, 
							OUT_LENGTH_SHORT_Z int *certInfoLength )
	{
	const void *data = NULL;
	int dataLength = 0, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			( certInfo != NULL && \
			  certInfoMaxLength > 0 && \
			  certInfoMaxLength < MAX_INTLENGTH_SHORT && \
			  isWritePtr( certInfo, certInfoMaxLength ) ) );
	assert( isWritePtr( certInfoLength, sizeof( int ) ) );

	REQUIRES( isAttribute( certInfoType ) || \
			  isInternalAttribute( certInfoType ) );
	REQUIRES( ( certInfo == NULL && certInfoMaxLength == 0 ) || \
			  ( certInfo != NULL && \
				certInfoMaxLength > 0 && \
				certInfoMaxLength < MAX_INTLENGTH_SHORT ) );

	/* Clear return values */
	if( certInfo != NULL )
		memset( certInfo, 0, min( 16, certInfoMaxLength ) );
	*certInfoLength = 0;

	/* If it's a GeneralName or DN component, return it.  These are 
	   special-case attribute values so they have to come before the 
	   general attribute-handling code */
	if( isGeneralNameComponent( certInfoType ) )
		{
		SELECTION_STATE selectionState;
		ATTRIBUTE_PTR *attributePtr = DUMMY_INIT_PTR;
		void *dataPtr;

		/* Find the requested GeneralName component and return it to the
		   caller.  Since selectGeneralNameComponent() changes the current
		   selection within the GeneralName, we save the selection state
		   around the call */
		saveSelectionState( selectionState, certInfoPtr );
		status = selectGeneralNameComponent( certInfoPtr, certInfoType );
		if( cryptStatusOK( status ) )
			attributePtr = certInfoPtr->attributeCursor;
		restoreSelectionState( selectionState, certInfoPtr );
		if( cryptStatusError( status ))
			return( status );
		ENSURES( attributePtr != NULL );

		/* Get the attribute component data */
		status = getAttributeDataPtr( attributePtr, &dataPtr, &dataLength );
		if( cryptStatusError( status ) )
			return( status );
		return( attributeCopyParams( certInfo, certInfoMaxLength, 
									 certInfoLength, dataPtr, dataLength ) );
		}
	if( isDNComponent( certInfoType ) )
		{
		int count = 0;
		
		/* If this is the currently selected item in the DN, the caller may
		   be asking for the n-th occurrence rather than the initial one */
		if( certInfoPtr->currentSelection.dnComponent == certInfoType )
			count = certInfoPtr->currentSelection.dnComponentCount;

		/* Find the requested DN component and return it to the caller */
		status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
						   MUST_BE_PRESENT );
		if( cryptStatusError( status ) )
			return( status );
		return( getDNComponentValue( *certInfoPtr->currentSelection.dnPtr,
									 certInfoType, count, certInfo, 
									 certInfoMaxLength, certInfoLength ) );
		}

	/* If it's standard certificate or CMS attribute, return it */
	if( ( certInfoType >= CRYPT_CERTINFO_FIRST_EXTENSION && \
		  certInfoType <= CRYPT_CERTINFO_LAST_EXTENSION ) || \
		( certInfoType >= CRYPT_CERTINFO_FIRST_CMS && \
		  certInfoType <= CRYPT_CERTINFO_LAST_CMS ) )
		{
		return( getCertAttributeComponent( certInfoPtr, certInfoType,
										   certInfo, certInfoMaxLength, 
										   certInfoLength ) );
		}

	/* If it's anything else, handle it specially */
	switch( certInfoType )
		{
		case CRYPT_CERTINFO_FINGERPRINT_MD5:
		case CRYPT_CERTINFO_FINGERPRINT_SHA1:
		case CRYPT_CERTINFO_FINGERPRINT_SHA2:
		case CRYPT_CERTINFO_FINGERPRINT_SHAng:
			return( getCertHash( certInfoPtr, certInfoType, certInfo, 
								 certInfoMaxLength, certInfoLength ) );

		case CRYPT_CERTINFO_SERIALNUMBER:
			switch( certInfoPtr->type )
				{
#ifdef USE_CERTREV
				case CRYPT_CERTTYPE_CRL:
					{
					const CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;

					const REVOCATION_INFO *revInfoPtr = \
						( certRevInfo->currentRevocation != NULL ) ? \
						certRevInfo->currentRevocation : certRevInfo->revocations;

					if( revInfoPtr != NULL )
						{
						data = revInfoPtr->id;
						dataLength = revInfoPtr->idLength;
						}
					break;
					}
#endif /* USE_CERTREV */

#ifdef USE_CERTREQ
				case CRYPT_CERTTYPE_REQUEST_REVOCATION:
					data = certInfoPtr->cCertReq->serialNumber;
					dataLength = certInfoPtr->cCertReq->serialNumberLength;
					break;
#endif /* USE_CERTREQ */

				case CRYPT_CERTTYPE_CERTIFICATE:
				case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
				case CRYPT_CERTTYPE_CERTCHAIN:
					data = certInfoPtr->cCertCert->serialNumber;
					dataLength = certInfoPtr->cCertCert->serialNumberLength;
					break;

				default:
					retIntError();
				}
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, data, dataLength ) );

		case CRYPT_CERTINFO_VALIDFROM:
		case CRYPT_CERTINFO_THISUPDATE:
			if( certInfoPtr->startTime > MIN_CERT_TIME_VALUE )
				{
				data = &certInfoPtr->startTime;
				dataLength = sizeof( time_t );
				}
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, data, dataLength ) );

		case CRYPT_CERTINFO_VALIDTO:
		case CRYPT_CERTINFO_NEXTUPDATE:
			if( certInfoPtr->endTime > MIN_CERT_TIME_VALUE )
				{
				data = &certInfoPtr->endTime;
				dataLength = sizeof( time_t );
				}
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, data, dataLength ) );

#ifdef USE_CERT_OBSOLETE 
		case CRYPT_CERTINFO_ISSUERUNIQUEID:
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, 
										 certInfoPtr->cCertCert->issuerUniqueID, 
										 certInfoPtr->cCertCert->issuerUniqueIDlength ) );

		case CRYPT_CERTINFO_SUBJECTUNIQUEID:
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, 
										 certInfoPtr->cCertCert->subjectUniqueID, 
										 certInfoPtr->cCertCert->subjectUniqueIDlength ) );
#endif /* USE_CERT_OBSOLETE */

		case CRYPT_CERTINFO_REVOCATIONDATE:
			switch( certInfoPtr->type )
				{
#ifdef USE_CERTREV
				case CRYPT_CERTTYPE_CRL:
				case CRYPT_CERTTYPE_OCSP_RESPONSE:
					data = getRevocationTimePtr( certInfoPtr );
					break;
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL 
				case CRYPT_CERTTYPE_RTCS_RESPONSE:
					data = getValidityTimePtr( certInfoPtr );
					break;
#endif /* USE_CERTVAL */

				default:
					retIntError();
				}
			if( data != NULL )
				dataLength = sizeof( time_t );
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, data, dataLength ) );

#ifdef USE_CERT_DNSTRING
		case CRYPT_CERTINFO_DN:
			{
			STREAM stream;

			/* Export the entire DN in string form */
			status = selectDN( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
							   MUST_BE_PRESENT );
			if( cryptStatusError( status ) )
				return( status );
			sMemOpenOpt( &stream, certInfo, certInfoMaxLength );
			status = writeDNstring( &stream, 
									*certInfoPtr->currentSelection.dnPtr );
			if( cryptStatusOK( status ) )
				*certInfoLength = stell( &stream );
			sMemDisconnect( &stream );
			return( status );
			}
#endif /* USE_CERT_DNSTRING */

#ifdef USE_PKIUSER
		case CRYPT_CERTINFO_PKIUSER_ID:
		case CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD:
		case CRYPT_CERTINFO_PKIUSER_REVPASSWORD:
			return( getPkiUserInfo( certInfoPtr, certInfoType, certInfo, 
									certInfoMaxLength, certInfoLength ) );
#endif /* USE_PKIUSER */

#ifdef USE_CERTREV
		case CRYPT_IATTRIBUTE_CRLENTRY:
			return( getCrlEntry( certInfoPtr, certInfo, certInfoMaxLength, 
								 certInfoLength ) );
#endif /* USE_CERTREV */

		case CRYPT_IATTRIBUTE_SUBJECT:
			/* Normally these attributes are only present for signed objects
			   (i.e. ones that are in the high state) but CRMF requests 
			   acting as CMP revocation requests aren't signed so we have to
			   set the ACLs to allow the attribute to be read in the low 
			   state as well.  Since this only represents a programming 
			   error rather than a real access violation we catch it here 
			   with an assertion */
			assert( ( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION && \
					  certInfoPtr->certificate == NULL ) || \
					certInfoPtr->certificate != NULL  );
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, 
										 certInfoPtr->subjectDNptr, 
										 certInfoPtr->subjectDNsize ) );

		case CRYPT_IATTRIBUTE_ISSUER:
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, 
										 certInfoPtr->issuerDNptr, 
										 certInfoPtr->issuerDNsize ) );

		case CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER:
			return( getIAndS( certInfoPtr, certInfo, certInfoMaxLength, 
							  certInfoLength ) );

		case CRYPT_IATTRIBUTE_HOLDERNAME:
			return( getHolderName( certInfoPtr, certInfo, certInfoMaxLength, 
								   certInfoLength ) );

		case CRYPT_IATTRIBUTE_HOLDERURI:
			return( getHolderURI( certInfoPtr, certInfo, certInfoMaxLength, 
								  certInfoLength ) );

		case CRYPT_IATTRIBUTE_SPKI:
			{
			BYTE *dataStartPtr = certInfo;

			status = attributeCopyParams( certInfo, certInfoMaxLength, 
										  certInfoLength, 
										  certInfoPtr->publicKeyInfo, 
										  certInfoPtr->publicKeyInfoSize );
			if( cryptStatusError( status ) )
				return( status );
			if( dataStartPtr != NULL && dataStartPtr[ 0 ] == MAKE_CTAG( 6 ) )
				{
				/* Fix up CRMF braindamage */
				*dataStartPtr = BER_SEQUENCE;
				}
			return( CRYPT_OK );
			}

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
		case CRYPT_IATTRIBUTE_RESPONDERURL:
			/* An RTCS/OCSP URL may be present if it was copied over from a 
			   certificate that's being checked, however if there wasn't any 
			   authorityInfoAccess information present then the URL won't 
			   have been initialised.  Since this attribute isn't accessed 
			   via the normal certificate attribute mechanisms we have to 
			   explictly check for its non-presence */
			switch( certInfoPtr->type )
				{
#ifdef USE_CERTREV
				case CRYPT_CERTTYPE_OCSP_REQUEST:
					if( certInfoPtr->cCertRev->responderUrl == NULL )
						return( CRYPT_ERROR_NOTFOUND );
					return( attributeCopyParams( certInfo, certInfoMaxLength, 
								certInfoLength, 
								certInfoPtr->cCertRev->responderUrl, 
								certInfoPtr->cCertRev->responderUrlSize ) );
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL 
				case CRYPT_CERTTYPE_RTCS_REQUEST:
					if( certInfoPtr->cCertVal->responderUrl == NULL )
						return( CRYPT_ERROR_NOTFOUND );
					return( attributeCopyParams( certInfo, certInfoMaxLength, 
								certInfoLength, 
								certInfoPtr->cCertVal->responderUrl, 
								certInfoPtr->cCertVal->responderUrlSize ) );
#endif /* USE_CERTVAL */

				default:
					retIntError();
				}
#endif /* USE_CERTREV || USE_CERTVAL */

#ifdef USE_CERTREQ
		case CRYPT_IATTRIBUTE_AUTHCERTID:
			/* An authorising certificate identifier will be present if
			   the request was handled by cryptlib but not if it came from
			   an external source so we have to make sure that there's 
			   something actually present before we try to return it */
			if( !memcmp( certInfoPtr->cCertReq->authCertID,
						 "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) )
				return( CRYPT_ERROR_NOTFOUND );
			return( attributeCopyParams( certInfo, certInfoMaxLength, 
										 certInfoLength, 
										 certInfoPtr->cCertReq->authCertID, 
										 KEYID_SIZE ) );
#endif /* USE_CERTREQ */

		case CRYPT_IATTRIBUTE_ESSCERTID:
			return( getESSCertID( certInfoPtr, certInfo, certInfoMaxLength, 
								  certInfoLength ) );
		}

	retIntError();
	}
#endif /* USE_CERTIFICATES */
