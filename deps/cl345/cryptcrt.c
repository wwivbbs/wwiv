/****************************************************************************
*																			*
*					cryptlib Certificate Management Routines				*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

/* "By the power vested in me, I now declare this text string and this bit
	string 'name' and 'key'.  What RSA has joined, let no man put asunder".
											-- Bob Blakley */
#include <ctype.h>
#include "crypt.h"
#ifdef INC_ALL
  #include "cert.h"
  #include "asn1.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1.h"
#endif /* Compiler-specific includes */

/* Warn about building at nonstandard compliance levels */

#if defined( _MSC_VER ) || defined( __GNUC__ )
  #if defined( USE_CERTLEVEL_PKIX_FULL )
	#pragma message( "  Building with nonstandard compliance level CERTLEVEL_PKIX_FULL." )
  #elif defined( USE_CERTLEVEL_PKIX_PARTIAL )
	#pragma message( "  Building with nonstandard compliance level CERTLEVEL_PKIX_PARTIAL." )
  #elif defined( USE_CERTLEVEL_PKIX_PARTIAL )
  #endif /* Certificate compliance level */
#endif /* Warn about special features enabled */

/* The minimum size for an OBJECT IDENTIFIER expressed as ASCII characters */

#define MIN_ASCII_OIDSIZE	7

#if defined( USE_CERTIFICATES )

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Compare values to data in a certificate */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN compareCertInfo( const CERT_INFO *certInfoPtr, 
								IN_ENUM( MESSAGE_COMPARE ) \
									const MESSAGE_COMPARE_TYPE compareType,
								IN_BUFFER_OPT( dataLength ) const void *data,
								IN_LENGTH_SHORT_Z const int dataLength,
								IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert )
	{
	int status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( data == NULL && dataLength == 0 ) || \
			isReadPtrDynamic( data, dataLength ) );

	REQUIRES_B( sanityCheckCert( certInfoPtr ) );
	REQUIRES_B( isEnumRange( compareType, MESSAGE_COMPARE ) );
	REQUIRES_B( ( compareType == MESSAGE_COMPARE_CERTOBJ && \
				  data == NULL && dataLength == 0 && \
				  isHandleRangeValid( iCryptCert ) ) || \
				( compareType != MESSAGE_COMPARE_CERTOBJ && \
				  data != NULL && isShortIntegerRangeNZ( dataLength ) && \
				  iCryptCert == CRYPT_UNUSED ) );

	switch( compareType )
		{
		case MESSAGE_COMPARE_SUBJECT:
			if( dataLength != certInfoPtr->subjectDNsize || \
				memcmp( data, certInfoPtr->subjectDNptr,
						certInfoPtr->subjectDNsize ) )
				return( FALSE );
			return( TRUE );

		case MESSAGE_COMPARE_ISSUERANDSERIALNUMBER:
			{
			STREAM stream;
			void *dataPtr DUMMY_INIT_PTR;
			int dataLeft DUMMY_INIT, serialNoLength, length;

			if( certInfoPtr->type != CRYPT_CERTTYPE_CERTIFICATE && \
				certInfoPtr->type != CRYPT_CERTTYPE_CERTCHAIN )
				return( FALSE );

			/* Comparing an iAndS can get quite tricky because of assorted 
			   braindamage in encoding methods so that two dissimilar 
			   iAndSs aren't necessarily supposed to be regarded as non-
			   equal.  First we try a trivial reject check, if that passes 
			   we compare the issuerName and serialNumber with corrections 
			   for common encoding braindamage.  Note that even this 
			   comparison can fail since older versions of the Entegrity 
			   toolkit (from the early 1990s) rewrote T61Strings in 
			   certificates as PrintableStrings in recipientInfo which means 
			   that any kind of straight comparison on these would fail.  We 
			   don't bother handling this sort of thing, and it's likely 
			   that most other software won't either (this situation only 
			   occurs when a certificate issuerName contains PrintableString 
			   text incorrectly encoded as T61String, which is rare enough 
			   that it required artifically-created certificates just to 
			   reproduce the problem).  In addition the trivial reject check 
			   can also fail since in an extreme encoding braindamage case a 
			   BMPString rewritten as a PrintableString would experience a 
			   large enough change in length to fail the check, but as with 
			   the Entegrity problem this is a level of brokenness up with 
			   which we will not put */
			length = sizeofShortObject( \
							certInfoPtr->issuerDNsize + \
							sizeofShortObject( \
								certInfoPtr->cCertCert->serialNumberLength ) );
			if( length < dataLength - 2 || length > dataLength + 2 )
				{
				/* Trivial reject, the lengths are too dissimilar for any 
				   fixup attempts to work */
				return( FALSE );
				}

			/* We also disallow obviously-invalid lengths at this point to 
			   ensure that we don't try and do anything with invalid data */
			if( length < 16 || length > MAX_INTLENGTH_SHORT || \
				dataLength < 16 || dataLength > MAX_INTLENGTH_SHORT )
				return( FALSE );

			/* We got past the trivial reject check, try a more detailed check, 
			   first the issuerName */
			sMemConnect( &stream, data, dataLength );
			status = readSequence( &stream, NULL );
			if( cryptStatusOK( status ) )
				{
				dataLeft = dataLength - stell( &stream );
				status = sMemGetDataBlock( &stream, &dataPtr, dataLeft );
				}
			if( cryptStatusOK( status ) )			
				status = getObjectLength( dataPtr, dataLeft, &length );
			if( cryptStatusOK( status ) )			
				status = readUniversal( &stream );
			if( cryptStatusError( status ) )
				{
				sMemDisconnect( &stream );
				return( FALSE );
				}
			ANALYSER_HINT( dataPtr != NULL );
			if( length != certInfoPtr->issuerDNsize || \
				memcmp( dataPtr, certInfoPtr->issuerDNptr,
						certInfoPtr->issuerDNsize ) )
				{
				sMemDisconnect( &stream );
				return( FALSE );
				}

			/* Compare the serialNumber */
			status = readGenericHole( &stream, &serialNoLength, 1, 
									  BER_INTEGER );
			if( cryptStatusOK( status ) )
				{
				status = sMemGetDataBlock( &stream, &dataPtr, 
										   serialNoLength );
				}
			sMemDisconnect( &stream );
			if( cryptStatusError( status ) )
				return( FALSE );
			if( !compareSerialNumber( certInfoPtr->cCertCert->serialNumber,
									  certInfoPtr->cCertCert->serialNumberLength,
									  dataPtr, serialNoLength ) )
				return( FALSE );

			return( TRUE );
			}

		case MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER:
			{
			BYTE sKID[ 128 + 8 ];
			int sKIDlength;

			/* Fetch the certificate's subjectKeyIdentifier and see whether 
			   it matches what we've been given */
			status = getCertComponentString( ( CERT_INFO * ) certInfoPtr, 
											 CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER, 
											 sKID, 128, &sKIDlength );
			if( cryptStatusError( status ) )
				return( FALSE );
			return( ( sKIDlength == dataLength && \
					  !memcmp( sKID, data, sKIDlength ) ) ? TRUE : FALSE );
			}

		case MESSAGE_COMPARE_FINGERPRINT_SHA1:
		case MESSAGE_COMPARE_FINGERPRINT_SHA2:
		case MESSAGE_COMPARE_FINGERPRINT_SHAng:
		case MESSAGE_COMPARE_CERTOBJ:
			{
			static const MAP_TABLE fingerprintMapTable[] = {
				{ MESSAGE_COMPARE_CERTOBJ, CRYPT_CERTINFO_FINGERPRINT_SHA1 },
				{ MESSAGE_COMPARE_FINGERPRINT_SHA1, CRYPT_CERTINFO_FINGERPRINT_SHA1 },
				{ MESSAGE_COMPARE_FINGERPRINT_SHA2, CRYPT_CERTINFO_FINGERPRINT_SHA2 },
				{ MESSAGE_COMPARE_FINGERPRINT_SHAng, CRYPT_CERTINFO_FINGERPRINT_SHAng },
				{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
				};
			MESSAGE_DATA msgData;
			BYTE fingerPrint[ CRYPT_MAX_HASHSIZE + 8 ];
			int fingerPrintLength, attributeToCompare;

			status = mapValue( compareType, &attributeToCompare, 
							   fingerprintMapTable,
							   FAILSAFE_ARRAYSIZE( fingerprintMapTable, \
												   MAP_TABLE ) );
			ENSURES( cryptStatusOK( status ) );

			/* If the certificate hasn't been signed yet we can't compare 
			   the fingerprint */
			if( certInfoPtr->certificate == NULL )
				return( FALSE );
			
			/* Get the certificate fingerprint and compare it to what we've 
			   been given */
			status = getCertComponentString( ( CERT_INFO * ) certInfoPtr, 
											 attributeToCompare, fingerPrint, 
											 CRYPT_MAX_HASHSIZE, 
											 &fingerPrintLength );
			if( cryptStatusError( status ) )
				return( FALSE );

			/* If it's a straight fingerprint compare, compare the 
			   certificate fingerprint to the user-supplied value */
			if( compareType != MESSAGE_COMPARE_CERTOBJ )
				{
				return( ( dataLength == fingerPrintLength && \
						  !memcmp( data, fingerPrint, fingerPrintLength ) ) ? \
						TRUE : FALSE );
				}
			REQUIRES( isHandleRangeValid( iCryptCert ) );

			/* It's a full certificate compare, compare the encoded 
			   certificate data via the fingerprints */
			setMessageData( &msgData, fingerPrint, fingerPrintLength );
			status = krnlSendMessage( iCryptCert, IMESSAGE_COMPARE, &msgData,
									  MESSAGE_COMPARE_FINGERPRINT_SHA1 );
			return( cryptStatusOK( status ) ? TRUE : FALSE );
			}
		}

	retIntError_Boolean();
	}

/* Check the usage of a certificate against a MESSAGE_CHECK_TYPE check */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int checkCertUsage( INOUT CERT_INFO *certInfoPtr, 
						   IN_ENUM( MESSAGE_CHECK ) \
							const MESSAGE_CHECK_TYPE checkType )
	{
	int complianceLevel, keyUsageValue, checkKeyFlag = CHECKKEY_FLAG_NONE;
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isEnumRange( checkType, MESSAGE_CHECK ) );

	/* Map the check type to a key usage that we check for */
	switch( checkType )
		{
		case MESSAGE_CHECK_PKC_PRIVATE:
			/* This check type can be encountered when checking a private 
			   key with a certificate attached */
			keyUsageValue = CRYPT_KEYUSAGE_NONE;
			checkKeyFlag = CHECKKEY_FLAG_PRIVATEKEY;
			break;

		case MESSAGE_CHECK_PKC_ENCRYPT:
		case MESSAGE_CHECK_PKC_ENCRYPT_AVAIL:
			keyUsageValue = CRYPT_KEYUSAGE_KEYENCIPHERMENT;
			break;

		case MESSAGE_CHECK_PKC_DECRYPT:
		case MESSAGE_CHECK_PKC_DECRYPT_AVAIL:
			keyUsageValue = CRYPT_KEYUSAGE_KEYENCIPHERMENT;
			checkKeyFlag = CHECKKEY_FLAG_PRIVATEKEY;
			break;

		case MESSAGE_CHECK_PKC_SIGN:
		case MESSAGE_CHECK_PKC_SIGN_AVAIL:
			/* In theory we should also check for KEYUSAGE_CA alongside 
			   KEYUSAGE_SIGN, however KEYUSAGE_CA is an odd special case of 
			   signing in which a signing key is allowed to sign any 
			   conceivable bit pattern except for one that looks like a 
			   certificate, and vice versa.  This means that while a CA key 
			   can be used for signing, it can't be used to sign anything 
			   other than a certificate, so a check for general signing 
			   capability should fail.
			   
			   This however leads to a problematic situation in which a 
			   check for the signing capability of a context would indicate
			   that it's unusable if it's associated with a CA certificate
			   rather than a general signing certificate.  Because of this
			   we have a special-case check that checks for data-that's-a-
			   certificate signing capability rather than just data-that-
			   isnt-a-certificate signing capability */
			keyUsageValue = KEYUSAGE_SIGN;
			checkKeyFlag = CHECKKEY_FLAG_PRIVATEKEY;
			break;

		case MESSAGE_CHECK_PKC_SIGCHECK:
		case MESSAGE_CHECK_PKC_SIGCHECK_AVAIL:
			/* See above comment for the non-check for KEYUSAGE_CA */
			keyUsageValue = KEYUSAGE_SIGN;
			break;

		case MESSAGE_CHECK_PKC_SIGN_CA:
		case MESSAGE_CHECK_PKC_SIGN_CA_AVAIL:
			/* See comment above for the check of KEYUSAGE_CA */
			keyUsageValue = KEYUSAGE_CA;
			checkKeyFlag = CHECKKEY_FLAG_CA | CHECKKEY_FLAG_PRIVATEKEY;
			break;

		case MESSAGE_CHECK_PKC_SIGCHECK_CA:
		case MESSAGE_CHECK_PKC_SIGCHECK_CA_AVAIL:
			/* See comment above for the check of KEYUSAGE_CA.  
			   MESSAGE_CHECK_CACERT_FINAL is a special-case version of 
			   MESSAGE_CHECK_PKC_SIGN/SIGCHECK_CA that's used internally by
			   the kernel */
			keyUsageValue = KEYUSAGE_CA;
			checkKeyFlag = CHECKKEY_FLAG_CA;
			break;

		case MESSAGE_CHECK_PKC_KA_EXPORT:
		case MESSAGE_CHECK_PKC_KA_EXPORT_AVAIL:
			/* exportOnly usage falls back to plain keyAgreement if 
			   necessary */
			keyUsageValue = CRYPT_KEYUSAGE_KEYAGREEMENT | \
							CRYPT_KEYUSAGE_ENCIPHERONLY;
			break;

		case MESSAGE_CHECK_PKC_KA_IMPORT:
		case MESSAGE_CHECK_PKC_KA_IMPORT_AVAIL:
			/* importOnly usage falls back to plain keyAgreement if 
			   necessary */
			keyUsageValue = CRYPT_KEYUSAGE_KEYAGREEMENT | \
							CRYPT_KEYUSAGE_DECIPHERONLY;
			break;

		case MESSAGE_CHECK_PKC:
			/* If we're just checking for generic PKC functionality then 
			   any kind of usage is OK */
			return( CRYPT_OK );

		case MESSAGE_CHECK_CERT:
			/* A generic check for certificate validity that doesn't require
			   full certificate processing as a MESSAGE_CRT_SIGCHECK would,
			   used when there's no signing certificate available but we 
			   want to perform a generic check that the certificate is 
			   generally OK, such as not being expired */
			status = checkCertBasic( certInfoPtr );
			if( cryptStatusError( status ) )
				{
				/* Convert the status value to the correct form */
				return( CRYPT_ARGERROR_OBJECT );
				}

			/* Check the validity.  Because this is a nonspecific check we 
			   allow any key usage */
			keyUsageValue = CRYPT_KEYUSAGE_KEYENCIPHERMENT | \
							KEYUSAGE_SIGN | KEYUSAGE_CA;
			checkKeyFlag = CHECKKEY_FLAG_GENCHECK;
			break;	

		default:
			retIntError();
		}
	ENSURES( keyUsageValue != CRYPT_KEYUSAGE_NONE || \
			 checkKeyFlag != CHECKKEY_FLAG_NONE );

	/* Certificate requests are special-case objects in that the key they 
	   contain is usable only for signature checking of the self-signature 
	   on the object (it can't be used for general-purpose usages, which 
	   would make it equivalent to a trusted self-signed certificate).  This 
	   is problematic because the keyUsage may indicate that the key is 
	   valid for other things as well, or not valid for signature checking.  
	   To get around this we indicate that the key has a single trusted 
	   usage, signature checking, and disallow any other usage regardless of 
	   what the keyUsage says.  The actual keyUsage usage is only valid once 
	   the request has been converted into a certificate */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTREQUEST || \
		certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_CERT )
		{
		if( checkType == MESSAGE_CHECK_PKC_SIGCHECK || \
			checkType == MESSAGE_CHECK_PKC_SIGCHECK_AVAIL )
			return( CRYPT_OK );
		setErrorInfo( certInfoPtr, CRYPT_CERTINFO_TRUSTED_USAGE, 
					  CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ARGERROR_OBJECT );
		}

	/* Only certificate objects with associated public keys are valid for 
	   check messages (which are checking the capabilities of the key) */
	REQUIRES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			  certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

	/* Certificate collections are pure container objects for which the base 
	   certificate object doesn't correspond to an actual certificate */
	REQUIRES( !TEST_FLAG( certInfoPtr->flags, CERT_FLAG_CERTCOLLECTION ) );

	/* Check the key usage for the certificate */
	status = krnlSendMessage( certInfoPtr->ownerHandle, 
							  IMESSAGE_GETATTRIBUTE, &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );
	status = checkKeyUsage( certInfoPtr, checkKeyFlag, keyUsageValue, 
							complianceLevel, &certInfoPtr->errorLocus, 
							&certInfoPtr->errorType );
	if( cryptStatusError( status ) )
		{
		/* Convert the status value to the correct form */
		return( CRYPT_ARGERROR_OBJECT );
		}

	return( CRYPT_OK );
	}

/* Export the certificate's data contents in ASN.1-encoded form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exportCertData( CERT_INFO *certInfoPtr, 
						   IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormat,
						   OUT_BUFFER_OPT( certDataMaxLength, *certDataLength ) \
							void *certData,
						   IN_DATALENGTH_Z const int certDataMaxLength,
						   OUT_DATALENGTH_Z int *certDataLength )
	{
	int status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( ( certData == NULL && certDataMaxLength == 0 ) || \
			isWritePtrDynamic( certData, certDataMaxLength ) );
	assert( isWritePtr( certDataLength, sizeof( int ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( isEnumRange( certFormat, CRYPT_CERTFORMAT ) );
	REQUIRES( ( certData == NULL && certDataMaxLength == 0 ) || \
			  ( certData != NULL && \
				certDataMaxLength > 0 && \
				certDataMaxLength < MAX_BUFFER_SIZE ) );

	/* Clear return value */
	*certDataLength = 0;

	/* Unsigned object types like CMS attributes aren't signed like other 
	   certificate objects so they aren't pre-encoded when we sign them and 
	   have the potential to change on each use if the same CMS attributes 
	   are reused for multiple signatures.  Because of this we write them 
	   out on export rather than copying the pre-encoded form from an 
	   internal buffer */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CMS_ATTRIBUTES )
		{
		WRITECERT_FUNCTION writeCertFunction;
		STREAM stream;

		REQUIRES( certFormat == CRYPT_ICERTFORMAT_DATA );

		writeCertFunction = \
				getCertWriteFunction( CRYPT_CERTTYPE_CMS_ATTRIBUTES );
		ENSURES( writeCertFunction != NULL );
		sMemOpenOpt( &stream, certData, certDataMaxLength );
		status = writeCertFunction( &stream, certInfoPtr, NULL, 
									CRYPT_UNUSED );
		if( cryptStatusOK( status ) )
			*certDataLength = stell( &stream );
		sMemDisconnect( &stream );

		return( status );
		}

	/* Some objects aren't signed or are pseudo-signed or optionally signed 
	   and have to be handled specially.  RTCS requests and responses are 
	   never signed (they're pure data containers like CMS attributes, with 
	   protection being provided by CMS).  OCSP requests can be optionally 
	   signed but usually aren't, so if we're fed an OCSP request without 
	   any associated encoded data we pseudo-sign it to produce encoded data.  
	   PKI user data is never signed but needs to go through a one-off setup 
	   process to initialise the user data fields, so it has the same 
	   semantics as a pseudo-signed object.  CRMF revocation requests are 
	   never signed (thus ruling out suicide-note revocations) */
	if( ( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST || \
		  certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE || \
		  certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
		  certInfoPtr->type == CRYPT_CERTTYPE_PKIUSER || \
		  certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION ) && \
		certInfoPtr->certificate == NULL )
		{
		status = signCert( certInfoPtr, CRYPT_UNUSED );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If we're exporting a single certificate from a chain, lock the 
	   currently selected certificate in the chain and export that */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
		certInfoPtr->cCertCert->chainPos >= 0 && \
		( certFormat == CRYPT_CERTFORMAT_CERTIFICATE || \
		  certFormat == CRYPT_CERTFORMAT_TEXT_CERTIFICATE || \
		  certFormat == CRYPT_CERTFORMAT_XML_CERTIFICATE ) )
		{
		CERT_INFO *certChainInfoPtr;

		ENSURES( certInfoPtr->cCertCert->chainPos >= 0 && \
				 certInfoPtr->cCertCert->chainPos < MAX_CHAINLENGTH );
		status = krnlAcquireObject( certInfoPtr->cCertCert->chain[ certInfoPtr->cCertCert->chainPos ], 
									OBJECT_TYPE_CERTIFICATE, 
									( MESSAGE_PTR_CAST ) &certChainInfoPtr,
									CRYPT_ARGERROR_OBJECT );
		if( cryptStatusError( status ) )
			return( status );
		status = exportCert( certData, certDataMaxLength, certDataLength, 
							 certFormat, certChainInfoPtr );
		krnlReleaseObject( certChainInfoPtr->objectHandle );
		return( status );
		}

	ENSURES( ( TEST_FLAG( certInfoPtr->flags, 
						  CERT_FLAG_CERTCOLLECTION ) && \
			   certInfoPtr->certificate == NULL ) || \
			 certInfoPtr->certificate != NULL );
	return( exportCert( certData, certDataMaxLength, certDataLength, 
						certFormat, certInfoPtr ) );
	}

/****************************************************************************
*																			*
*					Internal Certificate/Key Management Functions			*
*																			*
****************************************************************************/

/* Import a certificate blob or certificate chain by sending get_next_cert 
   messages to the source object to obtain all the certificates in a chain.  
   Returns the length of the certificate.
   
   This isn't really a direct certificate function since the control flow 
   sequence is:

	import indirect: 
		GETNEXTCERT -> source object
			source object: 
				CREATEOBJECT_INDIRECT -> system device
					system device: createCertificate() 
		GETNEXTCERT -> source object
			source object: 
				CREATEOBJECT_INDIRECT -> system device
					system device: createCertificate() 
		[...]					

   however this seems to be the best place to put the code (sol lucet 
   omnibus) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int iCryptImportCertIndirect( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate,
							  IN_HANDLE const CRYPT_HANDLE iCertSource, 
							  IN_ENUM( CRYPT_KEYID ) \
								const CRYPT_KEYID_TYPE keyIDtype,
							  IN_BUFFER( keyIDlength ) const void *keyID, 
							  IN_LENGTH_SHORT const int keyIDlength,
							  IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtrDynamic( keyID, keyIDlength ) );

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( isEnumRange( keyIDtype, CRYPT_KEYID ) );
	REQUIRES( isShortIntegerRangeNZ( keyIDlength ) );
	REQUIRES( isFlagRangeZ( options, KEYMGMT ) && \
			  ( options & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* We're importing a sequence of certificates as a chain from a source 
	   object, assemble the collection via the object */
	return( assembleCertChain( iCertificate, iCertSource, keyIDtype, 
							   keyID, keyIDlength, options ) );
	}

/* Check that a given key ID actually corresponds to the certificate that 
   we've got.  This is used to verify that the certificate that a get-key
   function has given us is actually the correct one for the key ID that
   we specified.  Obviously this should be the case, but if the get-key
   can lie to us and the caller blindly accepts the certificate and uses
   it they could end up trying to fetch a certificate belonging to A,
   being fed one belonging to B, and "verify" information from B believing
   it to be from A.

   This is another oddly-placed function that's placed here mostly because
   there's nowhere else obvious to put it */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int iCryptVerifyID( IN_HANDLE const CRYPT_CERTIFICATE iCertificate,
					IN_ENUM( CRYPT_KEYID ) const CRYPT_KEYID_TYPE keyIDtype,
					IN_BUFFER( keyIDlength ) const void *keyID, 
					IN_LENGTH_SHORT const int keyIDlength )
	{
	MESSAGE_COMPARE_TYPE compareType;
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtrDynamic( keyID, keyIDlength ) );

	REQUIRES( isHandleRangeValid( iCertificate ) );
	REQUIRES( isEnumRange( keyIDtype, CRYPT_KEYID ) );
	REQUIRES( keyIDlength >= min( MIN_ID_LENGTH, MIN_NAME_LENGTH ) && \
			  keyIDlength < MAX_INTLENGTH_SHORT );

	switch( keyIDtype )
		{
		case CRYPT_KEYID_NAME:
		case CRYPT_KEYID_URI:
			/* The definitions of these are sufficiently vague, generally 
			   being "whatever identifier is commonly associated with the 
			   certificate", that we can't easily reject a certificate based
			   on them */
			return( CRYPT_OK );

		case CRYPT_IKEYID_KEYID:
			compareType = MESSAGE_COMPARE_KEYID;
			break;

		case CRYPT_IKEYID_PGPKEYID:
			/* This key ID has two subtypes, the PGP and the OpenPGP ID.  To
			   deal with this we try for the PGP one now and then fall back
			   to a second check with the OpenPGP one if this one fails */
			compareType = MESSAGE_COMPARE_KEYID_PGP;
			break;

		case CRYPT_IKEYID_CERTID:
			/* Some sources (database keysets) truncate the certID to 128 
			   bits/16 bytes so we have to explicitly read the certID and
			   then compare that subset that we've been given */
			if( keyIDlength != KEYID_SIZE )
				{
				BYTE buffer[ CRYPT_MAX_HASHSIZE + 8 ];

				setMessageData( &msgData, buffer, CRYPT_MAX_HASHSIZE );
				status = krnlSendMessage( iCertificate, 
										  IMESSAGE_GETATTRIBUTE_S, &msgData, 
										  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
				if( cryptStatusError( status ) || \
					msgData.length < keyIDlength || \
					memcmp( buffer, keyID, keyIDlength ) )
					return( CRYPT_ERROR_INVALID );

				return( CRYPT_OK );
				}
			compareType = MESSAGE_COMPARE_FINGERPRINT_SHA1;
			break;

		case CRYPT_IKEYID_SUBJECTID:
		case CRYPT_IKEYID_ISSUERID:
			{
			DYNBUF nameDB;
			BYTE nameHash[ KEYID_SIZE + 8 ];
			const int idLength = min( keyIDlength, KEYID_SIZE );

			/* Compare the hashed subjectName (used by PKCS #15 files and 
			   database keysets) or the hashed issuerAndSerialNumber (used by 
			   PKCS #15 files).  We can't hard-wire in KEYID_SIZE for the
			   size of the hashed value because some sources use the full
			   KEYID_SIZE and some truncate it to 128 bits/16 bytes, so we
			   use the shorter of KEYID_SIZE and the user-provided ID size */
			status = dynCreate( &nameDB, iCertificate, 
								( keyIDtype == CRYPT_IKEYID_SUBJECTID ) ? \
									CRYPT_IATTRIBUTE_SUBJECT : \
									CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
			if( cryptStatusError( status ) )
				return( status );
			hashData( nameHash, idLength, dynData( nameDB ), 
					  dynLength( nameDB ) );
			dynDestroy( &nameDB );
			return( memcmp( nameHash, keyID, idLength ) ? \
					CRYPT_ERROR_INVALID : CRYPT_OK );
			}

		case CRYPT_IKEYID_ISSUERANDSERIALNUMBER:
			compareType = MESSAGE_COMPARE_ISSUERANDSERIALNUMBER;
			break;

		default:
			retIntError();
		}

	/* Make sure that the key ID that we've been given matches the one in 
	   the certificate.  What error code to use to indicate a problem is
	   rather unclear since there's really no way to say "the data source
	   lied to us about the certificate that it returned", the least
	   inappropriate code seems to be CRYPT_ERROR_INVALID (in any case since
	   this situation should never really occur it's not worth losing too
	   much sleep over it) */
	setMessageData( &msgData, ( MESSAGE_DATA * ) keyID, keyIDlength );
	status = krnlSendMessage( iCertificate, IMESSAGE_COMPARE, &msgData, 
							  compareType );
	if( cryptStatusOK( status ) )
		return( CRYPT_OK );

	/* If we were looking for a PGP keyID and the comparison on the OpenPGP
	   ID failed, fall back to checking the older PGP 2.x ID */
	if( keyIDtype == CRYPT_IKEYID_PGPKEYID )
		{
		status = krnlSendMessage( iCertificate, IMESSAGE_COMPARE, &msgData, 
								  MESSAGE_COMPARE_KEYID_OPENPGP );
		if( cryptStatusOK( status ) )
			return( CRYPT_OK );
		}

	DEBUG_DIAG_ERRMSG(( "Certificate fetched for ID type %s doesn't "
						"actually correspond to the given ID", 
						getKeyIDName( keyIDtype ) ));
	return( CRYPT_ERROR_INVALID );
	}

/****************************************************************************
*																			*
*						Certificate Management API Functions				*
*																			*
****************************************************************************/

/* Handle attribute data sent to or read from a certificate object.  We have
   to do this in a standalone function since it's called from several places
   in the certificate message handler */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int processCertAttribute( INOUT CERT_INFO *certInfoPtr,
								 IN_MESSAGE const MESSAGE_TYPE message,
								 INOUT void *messageDataPtr, 
								 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;
	int *valuePtr = ( int * ) messageDataPtr;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( sanityCheckCert( certInfoPtr ) );
	REQUIRES( message == MESSAGE_GETATTRIBUTE || \
			  message == MESSAGE_GETATTRIBUTE_S || \
			  message == MESSAGE_SETATTRIBUTE || \
			  message == MESSAGE_SETATTRIBUTE_S || \
			  message == MESSAGE_DELETEATTRIBUTE );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Process get/set/delete attribute messages */
	if( message == MESSAGE_GETATTRIBUTE )
		{
		if( attribute == CRYPT_ATTRIBUTE_ERRORTYPE )
			{
			*valuePtr = certInfoPtr->errorType;
			return( CRYPT_OK );
			}
		if( attribute == CRYPT_ATTRIBUTE_ERRORLOCUS )
			{
			*valuePtr = certInfoPtr->errorLocus;
			return( CRYPT_OK );
			}
		return( getCertComponent( certInfoPtr, attribute, valuePtr ) );
		}
	if( message == MESSAGE_GETATTRIBUTE_S )
		{
		return( getCertComponentString( certInfoPtr, attribute, 
										msgData->data, msgData->length, 
										&msgData->length ) );
		}
	if( message == MESSAGE_SETATTRIBUTE )
		{
		const int value = *valuePtr;
		BOOLEAN validCursorPosition;
		
		if( certInfoPtr->type == CRYPT_CERTTYPE_CMS_ATTRIBUTES )
			{
			validCursorPosition = \
				( attribute >= CRYPT_CERTINFO_FIRST_CMS && \
				  attribute <= CRYPT_CERTINFO_LAST_CMS ) ? TRUE : FALSE;
			}
		else
			{
			validCursorPosition = \
				( attribute >= CRYPT_CERTINFO_FIRST_EXTENSION && \
				  attribute <= CRYPT_CERTINFO_LAST_EXTENSION ) ? TRUE : FALSE;
			}

		/* If it's a completed certificate we can only add a restricted 
		   class of component selection control values to the object.  We
		   don't use continuation characters for the more complex isXYZ()
		   expressions because the resulting string is too long for some
		   broken compilers */
		REQUIRES( certInfoPtr->certificate == NULL || \
				  isDNSelectionComponent( attribute ) ||
				  isGeneralNameSelectionComponent( attribute ) || 
				  /* Cursor control */
				  attribute == CRYPT_CERTINFO_CURRENT_CERTIFICATE || \
				  attribute == CRYPT_ATTRIBUTE_CURRENT_GROUP || \
				  attribute == CRYPT_ATTRIBUTE_CURRENT || \
				  attribute == CRYPT_ATTRIBUTE_CURRENT_INSTANCE ||
				  /* Certificate handling control component */
				  attribute == CRYPT_CERTINFO_TRUSTED_USAGE || \
				  attribute == CRYPT_CERTINFO_TRUSTED_IMPLICIT || \
				  attribute == CRYPT_IATTRIBUTE_INITIALISED || 
				  /* Misc.components */
				  attribute == CRYPT_IATTRIBUTE_PKIUSERINFO );

		/* If it's an initialisation message, there's nothing to do (we get 
		   these when importing a certificate, when the import is complete 
		   the import code sends this message to move the certificate into 
		   the high state because it's already signed) */
		if( attribute == CRYPT_IATTRIBUTE_INITIALISED )
			return( CRYPT_OK );

		/* If the passed-in value is a cursor-positioning code, make sure 
		   that it's valid */
		if( value < 0 && value != CRYPT_UNUSED && \
			( value > CRYPT_CURSOR_FIRST || value < CRYPT_CURSOR_LAST ) &&
			!validCursorPosition && attribute != CRYPT_CERTINFO_SELFSIGNED )
			return( CRYPT_ARGERROR_NUM1 );

		return( addCertComponent( certInfoPtr, attribute, value ) );
		}
	if( message == MESSAGE_SETATTRIBUTE_S )
		{
		return( addCertComponentString( certInfoPtr, attribute, 
										msgData->data, msgData->length ) );
		}
	if( message == MESSAGE_DELETEATTRIBUTE )
		return( deleteCertComponent( certInfoPtr, attribute ) );

	retIntError();
	}

/* Handle a message sent to a certificate context */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int certificateMessageFunction( INOUT TYPECAST( CERT_INFO * ) \
										void *objectInfoPtr,
									   IN_MESSAGE const MESSAGE_TYPE message,
									   void *messageDataPtr,
									   IN_INT_Z const int messageValue )
	{
	CERT_INFO *certInfoPtr = ( CERT_INFO * ) objectInfoPtr;

	assert( isWritePtr( objectInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( message == MESSAGE_DESTROY || \
			  sanityCheckCert( certInfoPtr ) );
	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( ( message == MESSAGE_CRT_SIGCHECK && \
				messageValue == CRYPT_UNUSED ) || \
			  isIntegerRange( messageValue ) );

	/* Process destroy object messages */
	if( message == MESSAGE_DESTROY )
		{
		/* Clear the encoded certificate and miscellaneous components if
		   necessary.  Note that there's no need to clear the associated
		   encryption context (if any) since this is a dependent object of
		   the certificate and is destroyed by the kernel when the 
		   certificate is destroyed */
		if( certInfoPtr->certificate != NULL )
			{
			zeroise( certInfoPtr->certificate, certInfoPtr->certificateSize );
			clFree( "certificateMessageFunction", certInfoPtr->certificate );
			}
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
			certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
			{
			if( certInfoPtr->cCertCert->serialNumber != NULL && \
				certInfoPtr->cCertCert->serialNumber != \
					certInfoPtr->cCertCert->serialNumberBuffer )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertCert->serialNumber );
				}
			}
#ifdef USE_CERTREQ
		if( certInfoPtr->type == CRYPT_CERTTYPE_REQUEST_REVOCATION )
			{
			if( certInfoPtr->cCertReq->serialNumber != NULL && \
				certInfoPtr->cCertReq->serialNumber != \
					certInfoPtr->cCertReq->serialNumberBuffer )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertReq->serialNumber );
				}
			}
#endif /* USE_CERTREQ */
#ifdef USE_CERT_OBSOLETE 
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE )
			{
			if( certInfoPtr->cCertCert->subjectUniqueID != NULL )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertCert->subjectUniqueID );
				}
			if( certInfoPtr->cCertCert->issuerUniqueID != NULL )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertCert->issuerUniqueID );
				}
			}
#endif /* USE_CERT_OBSOLETE */
		if( certInfoPtr->publicKeyData != NULL )
			clFree( "certificateMessageFunction", certInfoPtr->publicKeyData  );
		if( certInfoPtr->subjectDNdata != NULL )
			clFree( "certificateMessageFunction", certInfoPtr->subjectDNdata );
		if( certInfoPtr->issuerDNdata != NULL )
			clFree( "certificateMessageFunction", certInfoPtr->issuerDNdata );
#ifdef USE_CERTREV
		if( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
			certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
			certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
			{
			if( certInfoPtr->cCertRev->responderUrl != NULL )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertRev->responderUrl );
				}
			}
#endif /* USE_CERTREV */
#ifdef USE_CERTVAL
		if( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE )
			{
			if( certInfoPtr->cCertVal->responderUrl != NULL )
				{
				clFree( "certificateMessageFunction", 
						certInfoPtr->cCertVal->responderUrl );
				}
			}
#endif /* USE_CERTVAL */

		/* Clear the DN's if necessary */
		if( DATAPTR_ISSET( certInfoPtr->issuerName ) )
			deleteDN( &certInfoPtr->issuerName );
		if( DATAPTR_ISSET( certInfoPtr->subjectName ) )
			deleteDN( &certInfoPtr->subjectName );

		/* Clear the attributes and validity/revocation info if necessary */
		if( DATAPTR_ISSET( certInfoPtr->attributes ) )
			deleteAttributes( &certInfoPtr->attributes );
#ifdef USE_CERTVAL
		if( certInfoPtr->type == CRYPT_CERTTYPE_RTCS_REQUEST || \
			certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE )
			{
			CERT_VAL_INFO *certValInfo = certInfoPtr->cCertVal;

			if( DATAPTR_ISSET( certValInfo->validityInfo ) )
				deleteValidityEntries( &certValInfo->validityInfo );
			}
#endif /* USE_CERTVAL */
#ifdef USE_CERTREV
		if( certInfoPtr->type == CRYPT_CERTTYPE_CRL || \
			certInfoPtr->type == CRYPT_CERTTYPE_OCSP_REQUEST || \
			certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE )
			{
			CERT_REV_INFO *certRevInfo = certInfoPtr->cCertRev;

			if( DATAPTR_ISSET( certRevInfo->revocations ) )
				deleteRevocationEntries( &certRevInfo->revocations );
			}
#endif /* USE_CERTREV */

		/* Clear the certificate chain if necessary */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
			certInfoPtr->cCertCert->chainEnd > 0 )
			{
			int i, LOOP_ITERATOR;

			ENSURES( certInfoPtr->cCertCert->chainEnd >= 0 && \
					 certInfoPtr->cCertCert->chainEnd < MAX_CHAINLENGTH );
			LOOP_EXT( i = 0, i < certInfoPtr->cCertCert->chainEnd, i++, 
					  MAX_CHAINLENGTH )
				{
				krnlSendNotifier( certInfoPtr->cCertCert->chain[ i ],
								  IMESSAGE_DECREFCOUNT );
				}
			ENSURES( LOOP_BOUND_OK );
			}

		return( CRYPT_OK );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		/* If it's a certificate chain lock the currently selected 
		   certificate in the chain unless the message being processed is a 
		   certificate cursor movement command or something specifically 
		   directed at the entire chain (for example a get type or self-
		   signed status command - we want to get the type/status of the 
		   chain, not of the certificates within it) */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
			certInfoPtr->cCertCert->chainPos >= 0 && \
			!( ( message == MESSAGE_SETATTRIBUTE ) && \
			   ( messageValue == CRYPT_CERTINFO_CURRENT_CERTIFICATE ) ) && \
			!( ( message == MESSAGE_GETATTRIBUTE ) && \
			   ( messageValue == CRYPT_CERTINFO_CERTTYPE || \
				 messageValue == CRYPT_CERTINFO_SELFSIGNED ) ) )
			{
			CERT_INFO *certChainInfoPtr;
			int status;

			ENSURES( certInfoPtr->cCertCert->chainPos >= 0 && \
					 certInfoPtr->cCertCert->chainPos < MAX_CHAINLENGTH );
			status = krnlAcquireObject( certInfoPtr->cCertCert->chain[ certInfoPtr->cCertCert->chainPos ], 
										OBJECT_TYPE_CERTIFICATE, 
										( MESSAGE_PTR_CAST ) &certChainInfoPtr,
										CRYPT_ARGERROR_OBJECT );
			if( cryptStatusError( status ) )
				return( status );
			ANALYSER_HINT( certChainInfoPtr != NULL );
			status = processCertAttribute( certChainInfoPtr, message, 
										   messageDataPtr, messageValue );
			krnlReleaseObject( certChainInfoPtr->objectHandle );
			return( status );
			}

		return( processCertAttribute( certInfoPtr, message, messageDataPtr, 
									  messageValue ) );
		}

	/* Process messages that compare the object */
	if( message == MESSAGE_COMPARE )
		{
		const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

		if( messageValue == MESSAGE_COMPARE_CERTOBJ )
			{
			/* A certificate object compare passes in a certificate handle 
			   rather than data */
			return( compareCertInfo( certInfoPtr, messageValue, NULL, 0,
									 *( ( CRYPT_CERTIFICATE * ) messageDataPtr ) ) ? \
									 CRYPT_OK : CRYPT_ERROR );
			}
		return( compareCertInfo( certInfoPtr, messageValue, msgData->data,
								 msgData->length, CRYPT_UNUSED ) ? \
								 CRYPT_OK : CRYPT_ERROR );
		}

	/* Process messages that check a certificate */
	if( message == MESSAGE_CHECK )
		return( checkCertUsage( certInfoPtr, messageValue ) );

	/* Process internal notification messages */
	if( message == MESSAGE_CHANGENOTIFY )
		{
		/* If the object is being accessed for cryptlib-internal use, save/
		   restore the internal state */
		if( messageValue == MESSAGE_CHANGENOTIFY_STATE )
			{
			if( messageDataPtr == MESSAGE_VALUE_TRUE )
				{
				/* Save the current volatile state so that any changes made 
				   while the object is in use aren't reflected back to the 
				   caller after the cryptlib-internal use has completed */
				saveSelectionState( certInfoPtr->selectionState, 
									certInfoPtr );
				}
			else
				{
				/* Restore the volatile state from before the object was 
				   used */
				restoreSelectionState( certInfoPtr->selectionState, 
									   certInfoPtr );
				}

			return( CRYPT_OK );
			}

		retIntError();
		}

	/* Process object-specific messages */
	if( message == MESSAGE_CRT_SIGN )
		{
		int status;

		REQUIRES( certInfoPtr->certificate == NULL );

		/* Make sure that the signing object can actually be used for 
		   signing.  This could be CA signing, or general-purpose signing.  
		   first we check for CA signing capability */
		status = krnlSendMessage( messageValue, IMESSAGE_CHECK, NULL,
								  MESSAGE_CHECK_PKC_SIGN_CA );
		if( cryptStatusError( status ) )
			{
			/* If it's something that can only be signed by a CA and this 
			   isn't a CA key, it's an error */
			if( certInfoPtr->type == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
				certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN || \
				certInfoPtr->type == CRYPT_CERTTYPE_CRL )
				return( CRYPT_ARGERROR_VALUE );

			/* Check for a general-purpose signing capability */
			status = krnlSendMessage( messageValue, IMESSAGE_CHECK, NULL,
									  MESSAGE_CHECK_PKC_SIGN );
			if( cryptStatusError( status ) )
				{
				/* This isn't available either, the only time that we can 
				   use a signing object that can't sign is when we have a 
				   CRMF request, which can be created with an encryption-
				   only key if the private key POP is performed via an out-
				   of-band mechanism.  If this is the case we make sure that 
				   the key can decrypt, which is the other way of performing 
				   POP if a signing key isn't available */
				if( certInfoPtr->type != CRYPT_CERTTYPE_REQUEST_CERT )
					return( CRYPT_ARGERROR_VALUE );
				status = krnlSendMessage( messageValue, IMESSAGE_CHECK, NULL,
										  MESSAGE_CHECK_PKC_DECRYPT );
				if( cryptStatusError( status ) )
					return( CRYPT_ARGERROR_VALUE );
				}
			}

		/* We're changing data in a certificate, clear the error 
		   information */
		clearErrorInfo( certInfoPtr );

		return( signCert( certInfoPtr, messageValue ) );
		}
	if( message == MESSAGE_CRT_SIGCHECK )
		{
		REQUIRES( certInfoPtr->certificate != NULL || \
				  certInfoPtr->type == CRYPT_CERTTYPE_RTCS_RESPONSE || \
				  certInfoPtr->type == CRYPT_CERTTYPE_OCSP_RESPONSE );

		/* We're checking data in a certificate, clear the error 
		   information */
		clearErrorInfo( certInfoPtr );

		return( checkCertValidity( certInfoPtr, messageValue ) );
		}
	if( message == MESSAGE_CRT_EXPORT )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

		return( exportCertData( certInfoPtr, messageValue, 
								msgData->data, msgData->length,
								&msgData->length ) );
		}

	retIntError();
	}

/* Create a certificate object, returning a pointer to the locked 
   certificate info ready for further initialisation */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificateInfo( OUT_PTR_COND CERT_INFO **certInfoPtrPtr, 
						   IN_HANDLE const CRYPT_USER iCryptOwner,
						   IN_ENUM( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE certType )
	{
	CRYPT_CERTIFICATE iCertificate;
	CERT_INFO *certInfoPtr;
	OBJECT_SUBTYPE subType;
	int storageSize, status;

	assert( isWritePtr( certInfoPtrPtr, sizeof( CERT_INFO * ) ) );

	REQUIRES( ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( isEnumRange( certType, CRYPT_CERTTYPE ) );

	/* Clear return value */
	*certInfoPtrPtr = NULL;

	/* Set up subtype-specific information */
	switch( certType )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
#ifdef USE_ATTRCERT
		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
#endif /* USE_ATTRCERT */
			subType = ( certType == CRYPT_CERTTYPE_CERTIFICATE ) ? \
					  SUBTYPE_CERT_CERT : SUBTYPE_CERT_ATTRCERT;
			storageSize = sizeof( CERT_CERT_INFO );
			break;

		case CRYPT_CERTTYPE_CERTCHAIN:
			/* A certificate chain is a special case of a certificate (and/
			   or vice versa) so it uses the same subtype-specific 
			   storage */
			subType = SUBTYPE_CERT_CERTCHAIN;
			storageSize = sizeof( CERT_CERT_INFO );
			break;

#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_CERTREQUEST:
			subType = SUBTYPE_CERT_CERTREQ;
			storageSize = 0;
			break;

		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			subType = ( certType == CRYPT_CERTTYPE_REQUEST_CERT ) ? \
					  SUBTYPE_CERT_REQ_CERT : SUBTYPE_CERT_REQ_REV;
			storageSize = sizeof( CERT_REQ_INFO );
			break;
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
			subType = SUBTYPE_CERT_CRL;
			storageSize = sizeof( CERT_REV_INFO );
			break;

		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			subType = ( certType == CRYPT_CERTTYPE_OCSP_REQUEST ) ? \
					  SUBTYPE_CERT_OCSP_REQ : SUBTYPE_CERT_OCSP_RESP;
			storageSize = sizeof( CERT_REV_INFO );
			break;
#endif /* USE_CERTREV */

#ifdef USE_CMSATTR
		case CRYPT_CERTTYPE_CMS_ATTRIBUTES:
			subType = SUBTYPE_CERT_CMSATTR;
			storageSize = 0;
			break;
#endif /* USE_CMSATTR */

#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			subType = ( certType == CRYPT_CERTTYPE_RTCS_REQUEST ) ? \
					  SUBTYPE_CERT_RTCS_REQ : SUBTYPE_CERT_RTCS_RESP;
			storageSize = sizeof( CERT_VAL_INFO );
			break;
#endif /* USE_CERTVAL */

#ifdef USE_PKIUSER
		case CRYPT_CERTTYPE_PKIUSER:
			subType = SUBTYPE_CERT_PKIUSER;
			storageSize = sizeof( CERT_PKIUSER_INFO );
			break;
#endif /* USE_PKIUSER */

		default:
			/* In theory this should be a retIntError() but since some 
			   certificate types could be disabled we return a more
			   conservative not-available error */
			return( CRYPT_ERROR_NOTAVAIL );
		}

	/* Create the certificate object */
	status = krnlCreateObject( &iCertificate, ( void ** ) &certInfoPtr, 
							   sizeof( CERT_INFO ) + storageSize, 
							   OBJECT_TYPE_CERTIFICATE, subType,
							   CREATEOBJECT_FLAG_NONE, iCryptOwner, 
							   ACTION_PERM_NONE_ALL, 
							   certificateMessageFunction );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( certInfoPtr != NULL );
	certInfoPtr->objectHandle = iCertificate;
	certInfoPtr->ownerHandle = iCryptOwner;
	certInfoPtr->type = certType;
	INIT_FLAGS( certInfoPtr->flags, CERT_FLAG_NONE );
	DATAPTR_SET( certInfoPtr->subjectName, NULL );
	DATAPTR_SET( certInfoPtr->issuerName, NULL );
	DATAPTR_SET( certInfoPtr->attributes, NULL );
	DATAPTR_SET( certInfoPtr->attributeCursor, NULL );
	DATAPTR_SET( certInfoPtr->selectionState.savedAttributeCursor, NULL );
	switch( certInfoPtr->type )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
		case CRYPT_CERTTYPE_CERTCHAIN:
			certInfoPtr->cCertCert = ( CERT_CERT_INFO * ) certInfoPtr->storage;
			certInfoPtr->cCertCert->chainPos = CRYPT_ERROR;
			certInfoPtr->cCertCert->trustedUsage = CRYPT_ERROR;
			break;

#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_REQUEST_CERT:
		case CRYPT_CERTTYPE_REQUEST_REVOCATION:
			certInfoPtr->cCertReq = ( CERT_REQ_INFO * ) certInfoPtr->storage;
			break;
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV
		case CRYPT_CERTTYPE_CRL:
		case CRYPT_CERTTYPE_OCSP_REQUEST:
		case CRYPT_CERTTYPE_OCSP_RESPONSE:
			certInfoPtr->cCertRev = ( CERT_REV_INFO * ) certInfoPtr->storage;
			DATAPTR_SET( certInfoPtr->cCertRev->revocations, NULL );
			DATAPTR_SET( certInfoPtr->cCertRev->currentRevocation, NULL );
			break;
#endif /* USE_CERTREV */

#ifdef USE_CERTVAL
		case CRYPT_CERTTYPE_RTCS_REQUEST:
		case CRYPT_CERTTYPE_RTCS_RESPONSE:
			certInfoPtr->cCertVal = ( CERT_VAL_INFO * ) certInfoPtr->storage;
			DATAPTR_SET( certInfoPtr->cCertVal->validityInfo, NULL );
			DATAPTR_SET( certInfoPtr->cCertVal->currentValidity, NULL );
			break;
#endif /* USE_CERTREV */

#ifdef USE_PKIUSER
		case CRYPT_CERTTYPE_PKIUSER:
			certInfoPtr->cCertUser = ( CERT_PKIUSER_INFO * ) certInfoPtr->storage;
			break;
#endif /* USE_PKIUSER */

#ifdef USE_CERTREQ
		case CRYPT_CERTTYPE_CERTREQUEST:
			/* No special storage requirements */
			break;
#endif /* USE_CERTREQ */

#ifdef USE_CMSATTR
		case CRYPT_CERTTYPE_CMS_ATTRIBUTES:
			/* No special storage requirements */
			break;
#endif /* USE_CMSATTR */

		default:
			retIntError();
		}

	/* Set up the default version number.  These values are set here mostly 
	   so that attempting to read the version attribute won't return a 
	   version of 0.

	   In some cases this is an indication only and will be modified based 
	   on information added to the object (for example the CRL version is 
	   implicitly set based on whether extensions are added or not).  If this 
	   can happen we start with the lowest version available (the default 
	   v1) which will be automatically incremented whenever information that 
	   can't be represented with that format version is added */
	switch( certType )
		{
		case CRYPT_CERTTYPE_CERTIFICATE:
		case CRYPT_CERTTYPE_CERTCHAIN:
			certInfoPtr->version = 3;
			break;

		case CRYPT_CERTTYPE_ATTRIBUTE_CERT:
			certInfoPtr->version = 2;
			break;

		default:
			certInfoPtr->version = 1;
			break;
		}

	/* Set up any internal objects to contain invalid handles */
	certInfoPtr->iPubkeyContext = CRYPT_ERROR;

	/* Set the state information to its initial state */
	initSelectionInfo( certInfoPtr );

	/* Return the certificate information */
	*certInfoPtrPtr = certInfoPtr;

	return( CRYPT_OK );
	}

/* Create a certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificate( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo, 
					   STDC_UNUSED const void *auxDataPtr, 
					   STDC_UNUSED const int auxValue )
	{
	CRYPT_CERTIFICATE iCertificate;
	CERT_INFO *certInfoPtr;
	int status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( isEnumRange( createInfo->arg1, CRYPT_CERTTYPE ) );
	REQUIRES( createInfo->arg2 == 0 && createInfo->strArg1 == NULL && \
			  createInfo->strArgLen1 == 0 );

	/* Pass the call on to the lower-level open function */
	status = createCertificateInfo( &certInfoPtr, createInfo->cryptOwner,
									createInfo->arg1 );
	if( cryptStatusError( status ) )
		return( status );
	iCertificate = certInfoPtr->objectHandle;

	/* We've finished setting up the object-type-specific info, tell the 
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCertificate, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusOK( status ) )
		createInfo->cryptHandle = iCertificate;
	return( status );
	}

/* Create a certificate by instantiating it from its encoded form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificateIndirect( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
							   STDC_UNUSED const void *auxDataPtr, 
							   STDC_UNUSED const int auxValue )
	{
	CRYPT_CERTIFICATE iCertificate;
	int status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( isEnumRangeOpt( createInfo->arg1, CRYPT_CERTTYPE ) );
	REQUIRES( createInfo->strArg1 != NULL );
	REQUIRES( createInfo->strArgLen1 > 16 && \
			  createInfo->strArgLen1 < MAX_INTLENGTH ); 
			  /* May be CMS attribute (short) or a mega-CRL (long ) */
	REQUIRES( ( createInfo->arg2 == 0 && createInfo->strArg2 == NULL && \
				createInfo->strArgLen2 == 0 ) || \
			  ( ( createInfo->arg2 == CRYPT_IKEYID_KEYID || \
				  createInfo->arg2 == CRYPT_IKEYID_ISSUERANDSERIALNUMBER ) && \
				createInfo->strArg2 != NULL && \
				createInfo->strArgLen2 > 2 && \
				createInfo->strArgLen2 < MAX_INTLENGTH_SHORT ) );
	REQUIRES( isFlagRangeZ( createInfo->arg3, KEYMGMT ) && \
			  ( createInfo->arg3 & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
	REQUIRES( createInfo->arg2 == 0 || createInfo->arg3 == 0 );

	/* Pass the call through to the low-level import function */
	status = importCert( createInfo->strArg1, createInfo->strArgLen1,
						 &iCertificate, createInfo->cryptOwner,
						 createInfo->arg2, createInfo->strArg2, 
						 createInfo->strArgLen2, createInfo->arg3,
						 createInfo->arg1 );
	if( cryptStatusOK( status ) )
		createInfo->cryptHandle = iCertificate;
	return( status );
	}

/* Generic management function for this class of object */

CHECK_RETVAL \
int certManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action )
	{
	REQUIRES( action == MANAGEMENT_ACTION_PRE_INIT );

	switch( action )
		{
		case MANAGEMENT_ACTION_PRE_INIT:
#ifndef CONFIG_FUZZ
			if( !sanityCheckExtensionTables() )
				{
				DEBUG_DIAG(( "Certificate class initialisation failed" ));
				retIntError();
				}
#endif /* !CONFIG_FUZZ */
			initAttributes();
			return( CRYPT_OK );
		}

	retIntError();
	}

/****************************************************************************
*																			*
*						Certificate Extension Blob Functions				*
*																			*
****************************************************************************/

/* Get/add/delete certificate attributes */

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 3, 6 ) ) \
C_RET cryptGetCertExtension( C_IN CRYPT_CERTIFICATE certificate,
							 C_IN char C_PTR oid, 
							 C_OUT int C_PTR criticalFlag,
							 C_OUT_OPT void C_PTR extension, 
							 C_IN int extensionMaxLength,
							 C_OUT int C_PTR extensionLength )
	{
	CERT_INFO *certInfoPtr;
	DATAPTR_ATTRIBUTE attributePtr;
	BYTE binaryOID[ MAX_OID_SIZE + 8 ];
	void *dataPtr;
#ifdef EBCDIC_CHARS
	char asciiOID[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
#endif /* EBCDIC_CHARS */
	int binaryOidLen, value, dataLength, status;

	/* Perform basic parameter error checking */
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isReadPtr( oid, MIN_ASCII_OIDSIZE ) )
		return( CRYPT_ERROR_PARAM2 );
	if( !isWritePtr( criticalFlag, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM3 );
	*criticalFlag = CRYPT_ERROR;
	if( extension != NULL )
		{
		if( extensionMaxLength <= 4 || \
			extensionMaxLength >= MAX_INTLENGTH_SHORT )
			return( CRYPT_ERROR_PARAM5 );
		if( !isWritePtrDynamic( extension, extensionMaxLength ) )
			return( CRYPT_ERROR_PARAM4 );
		memset( extension, 0, min( 16, extensionMaxLength ) );
		}
	if( !isWritePtr( extensionLength, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM6 );
	*extensionLength = 0;
	if( strlen( oid ) < MIN_ASCII_OIDSIZE || \
		strlen( oid ) > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_PARAM2 );
#ifdef EBCDIC_CHARS
	strlcpy_s( asciiOID, CRYPT_MAX_TEXTSIZE, oid );
	ebcdicToAscii( asciiOID, asciiOID, strlen( asciiOID ) );
	if( cryptStatusError( textToOID( asciiOID, strlen( asciiOID ), 
									 binaryOID, MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#else
	if( cryptStatusError( textToOID( oid, strlen( oid ), binaryOID, 
									 MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#endif /* EBCDIC_CHARS */

	/* Perform object error checking.  Normally this is handled by the 
	   kernel, however since this function accesses multiple parameters and
	   the target isn't a cryptlib attribute we have to handle the access
	   ourselves here.  In order to avoid potential race conditions we 
	   check whether the object is internal twice, once before we lock it 
	   and again afterwards.  We perform the check by reading the locked
	   property attribute, which is always available */
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM1 );
	status = krnlAcquireObject( certificate, OBJECT_TYPE_CERTIFICATE, 
								( MESSAGE_PTR_CAST ) &certInfoPtr, 
								CRYPT_ERROR_PARAM1 );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE, &value,
							  CRYPT_PROPERTY_LOCKED );
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PARAM1 );
		}

	/* Lock the currently selected certificate in a certificate chain if 
	   necessary */
	if( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
		certInfoPtr->cCertCert->chainPos >= 0 )
		{
		CERT_INFO *certChainInfoPtr;

		ENSURES( certInfoPtr->cCertCert->chainPos >= 0 && \
				 certInfoPtr->cCertCert->chainPos < MAX_CHAINLENGTH );
		status = krnlAcquireObject( certInfoPtr->cCertCert->chain[ certInfoPtr->cCertCert->chainPos ], 
									OBJECT_TYPE_CERTIFICATE, 
									( MESSAGE_PTR_CAST ) &certChainInfoPtr, 
									CRYPT_ERROR_PARAM1 );
		krnlReleaseObject( certInfoPtr->objectHandle );
		if( cryptStatusError( status ) )
			return( status );
		certInfoPtr = certChainInfoPtr;
		}

	/* Locate the attribute identified by the OID and get its information */
	attributePtr = findAttributeByOID( certInfoPtr->attributes, 
									   binaryOID, binaryOidLen );
	if( DATAPTR_ISNULL( attributePtr ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_NOTFOUND );
		}
	status = getBlobAttributeDataPtr( attributePtr, &dataPtr, &dataLength );
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( status );
		}
	ANALYSER_HINT( dataPtr != NULL );
	*criticalFlag = checkAttributeProperty( attributePtr, 
											ATTRIBUTE_PROPERTY_CRITICAL ) ? \
					1 : 0;	/* Note use of external-format BOOLEAN values */
	status = attributeCopyParams( extension, extensionMaxLength, 
								  extensionLength, dataPtr, dataLength );
	krnlReleaseObject( certInfoPtr->objectHandle );
	return( status );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) \
C_RET cryptAddCertExtension( C_IN CRYPT_CERTIFICATE certificate,
							 C_IN char C_PTR oid, C_IN int criticalFlag,
							 C_IN void C_PTR extension,
							 C_IN int extensionLength )
	{
	CERT_INFO *certInfoPtr;
	BYTE binaryOID[ MAX_OID_SIZE + 8 ];
#ifdef EBCDIC_CHARS
	char asciiOID[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
#endif /* EBCDIC_CHARS */
	int binaryOidLen, value, status;

	/* Perform basic parameter error checking */
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isReadPtr( oid, MIN_ASCII_OIDSIZE ) )
		return( CRYPT_ERROR_PARAM2 );
	if( extensionLength <= 4 || extensionLength > MAX_ATTRIBUTE_SIZE )
		return( CRYPT_ERROR_PARAM5 );
	if( !isReadPtrDynamic( extension, extensionLength ) )
		return( CRYPT_ERROR_PARAM4 );
	if( cryptStatusError( checkCertObjectEncoding( extension, 
												   extensionLength ) ) )
		return( CRYPT_ERROR_PARAM4 );
	if( strlen( oid ) < MIN_ASCII_OIDSIZE || \
		strlen( oid ) > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_PARAM2 );
#ifdef EBCDIC_CHARS
	strlcpy_s( asciiOID, CRYPT_MAX_TEXTSIZE, oid );
	ebcdicToAscii( asciiOID, asciiOID, strlen( asciiOID ) );
	if( cryptStatusError( textToOID( asciiOID, strlen( asciiOID ), 
									 binaryOID, MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#else
	if( cryptStatusError( textToOID( oid, strlen( oid ), binaryOID,
									 MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#endif /* EBCDIC_CHARS */

	/* Perform object error checking.  Normally this is handled by the 
	   kernel, however since this function accesses multiple parameters and
	   the target isn't a cryptlib attribute we have to handle the access
	   ourselves here.  In order to avoid potential race conditions we 
	   check whether the object is internal twice, once before we lock it 
	   and again afterwards.  We perform the check by reading the locked
	   property attribute, which is always available */
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM1 );
	status = krnlAcquireObject( certificate, OBJECT_TYPE_CERTIFICATE, 
								( MESSAGE_PTR_CAST ) &certInfoPtr, 
								CRYPT_ERROR_PARAM1 );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE, &value,
							  CRYPT_PROPERTY_LOCKED );
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PARAM1 );
		}
	if( certInfoPtr->certificate != NULL || \
		( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
		  certInfoPtr->cCertCert->chainPos >= 0 ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PERMISSION );
		}
	if( certInfoPtr->type == CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
		criticalFlag != CRYPT_UNUSED )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PARAM3 );
		}

	/* Add the attribute to the certificate */
	status = addAttribute( \
				( certInfoPtr->type == CRYPT_CERTTYPE_CMS_ATTRIBUTES ) ? \
					ATTRIBUTE_CMS : ATTRIBUTE_CERTIFICATE, 
				&certInfoPtr->attributes, binaryOID, binaryOidLen,
				( certInfoPtr->type == CRYPT_CERTTYPE_CMS_ATTRIBUTES ) ? \
					FALSE : ( criticalFlag ? TRUE : FALSE ), 
				extension, extensionLength, 0 );
	if( status == CRYPT_ERROR_INITED )
		{
		/* If the attribute is already present, set error information for it.
		   We can't set an error locus since it's an unknown blob */
		setErrorInfo( certInfoPtr, CRYPT_ATTRIBUTE_NONE,
					  CRYPT_ERRTYPE_ATTR_PRESENT );
		}
	krnlReleaseObject( certInfoPtr->objectHandle );
	return( status );
	}

C_NONNULL_ARG( ( 2 ) ) \
C_RET cryptDeleteCertExtension( C_IN CRYPT_CERTIFICATE certificate,
								C_IN char C_PTR oid )
	{
	CERT_INFO *certInfoPtr;
	DATAPTR_ATTRIBUTE attributePtr;
	BYTE binaryOID[ MAX_OID_SIZE + 8 ];
#ifdef EBCDIC_CHARS
	char asciiOID[ CRYPT_MAX_TEXTSIZE + 1 + 8 ];
#endif /* EBCDIC_CHARS */
	int binaryOidLen, value, status;

	/* Perform basic parameter error checking */
	if( !isHandleRangeValid( certificate ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isReadPtr( oid, MIN_ASCII_OIDSIZE ) )
		return( CRYPT_ERROR_PARAM2 );
	if( strlen( oid ) < MIN_ASCII_OIDSIZE || \
		strlen( oid ) > CRYPT_MAX_TEXTSIZE )
		return( CRYPT_ERROR_PARAM2 );
#ifdef EBCDIC_CHARS
	strlcpy_s( asciiOID, CRYPT_MAX_TEXTSIZE, oid );
	ebcdicToAscii( asciiOID, asciiOID, strlen( asciiOID ) );
	if( cryptStatusError( textToOID( asciiOID, strlen( asciiOID ), 
									 binaryOID, MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#else
	if( cryptStatusError( textToOID( oid, strlen( oid ), binaryOID,
									 MAX_OID_SIZE, &binaryOidLen ) ) )
		return( CRYPT_ERROR_PARAM2 );
#endif /* EBCDIC_CHARS */

	/* Perform object error checking.  Normally this is handled by the 
	   kernel, however since this function accesses multiple parameters and
	   the target isn't a cryptlib attribute we have to handle the access
	   ourselves here.  In order to avoid potential race conditions we 
	   check whether the object is internal twice, once before we lock it 
	   and again afterwards.  We perform the check by reading the locked
	   property attribute, which is always available */
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE,
							  &value, CRYPT_CERTINFO_CERTTYPE );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM1 );
	status = krnlAcquireObject( certificate, OBJECT_TYPE_CERTIFICATE, 
								( MESSAGE_PTR_CAST ) &certInfoPtr, 
								CRYPT_ERROR_PARAM1 );
	if( cryptStatusError( status ) )
		return( status );
	status = krnlSendMessage( certificate, MESSAGE_GETATTRIBUTE, &value,
							  CRYPT_PROPERTY_LOCKED );
	if( cryptStatusError( status ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PARAM1 );
		}
	if( certInfoPtr->certificate != NULL || \
		( certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN && \
		  certInfoPtr->cCertCert->chainPos >= 0 ) )
		{
		krnlReleaseObject( certInfoPtr->objectHandle );
		return( CRYPT_ERROR_PERMISSION );
		}

	/* Find the attribute identified by the OID and delete it */
	attributePtr = findAttributeByOID( certInfoPtr->attributes, 
									   binaryOID, binaryOidLen );
	if( DATAPTR_ISNULL( attributePtr ) )
		status = CRYPT_ERROR_NOTFOUND;
	else
		{
		DATAPTR_DN dnDummy;

		/* Since a blob attribute doesn't have any internal structure,
		   there's no possibility of a DN being associated with it, so we
		   pass in a dummy null DN reference */
		DATAPTR_SET( dnDummy, NULL );
		( void ) deleteAttribute( &certInfoPtr->attributes, 
								  &certInfoPtr->attributeCursor, 
								  attributePtr, &dnDummy );
		}
	krnlReleaseObject( certInfoPtr->objectHandle );
	return( status );
	}

#elif defined( USE_PSEUDOCERTIFICATES )

/****************************************************************************
*																			*
*							Pseudo-Certificate Functions					*
*																			*
****************************************************************************/

/* Functions to deal with pseudo-certificates, basic certificate-like 
   objects that serve purely as containers for encoded certificate data,
   and as targets for kernel messages for protocols like SSL/TLS and
   CMS/PKCS #7 that require the use of certificate objects */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int certificateMessageFunction( INOUT TYPECAST( CERT_INFO * ) \
										void *objectInfoPtr,
									   IN_MESSAGE const MESSAGE_TYPE message,
									   void *messageDataPtr,
									   IN_INT_Z const int messageValue )
	{
	CERT_INFO *certInfoPtr = ( CERT_INFO * ) objectInfoPtr;

	assert( isWritePtr( objectInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( ( message == MESSAGE_CRT_SIGCHECK && \
				messageValue == CRYPT_UNUSED ) || \
			  isIntegerRange( messageValue ) );

	if( message == MESSAGE_DESTROY )
		{
		/* Clear the encoded certificate if necessary.  This is the only 
		   certificate component that's used for pseudo-certificates */
		if( certInfoPtr->certificate != NULL )
			{
			zeroise( certInfoPtr->certificate, certInfoPtr->certificateSize );
			clFree( "certificateMessageFunction", certInfoPtr->certificate );
			}

		return( CRYPT_OK );
		}
	if( message == MESSAGE_GETATTRIBUTE )
		{
		int *valuePtr = ( int * ) messageDataPtr;

		/* Used to perform basic object checks on pseudo-certificates */
		if( messageValue == CRYPT_CERTINFO_IMMUTABLE )
			{
			*valuePtr = TRUE;
			return( CRYPT_OK );
			}
		if( messageValue == CRYPT_CERTINFO_CERTTYPE )
			{
			*valuePtr = CRYPT_CERTTYPE_CERTIFICATE;
			return( CRYPT_OK );
			}
		}
	if( message == MESSAGE_SETATTRIBUTE )
		{
		/* Needed to handle object initialisation */
		if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
			return( CRYPT_OK );
		}
	if( message == MESSAGE_CHECK )
		{
		/* Needed to handle making a certificate a dependent object of a 
		   private key */
		if( messageValue == MESSAGE_CHECK_PKC_SIGN_AVAIL || \
			messageValue == MESSAGE_CHECK_PKC_SIGCHECK_AVAIL || \
			messageValue == MESSAGE_CHECK_PKC_ENCRYPT_AVAIL || \
			messageValue == MESSAGE_CHECK_PKC_DECRYPT_AVAIL )
			return( CRYPT_OK );

		/* Needed to handle checking of private keys with certificates 
		   attached */
		if( messageValue == MESSAGE_CHECK_PKC_PRIVATE || \
			messageValue == MESSAGE_CHECK_PKC_DECRYPT || \
			messageValue == MESSAGE_CHECK_PKC_SIGN )
			return( CRYPT_OK );

		/* Needed to handle general certificate health checks */
		if( messageValue == MESSAGE_CHECK_CERT )
			return( CRYPT_OK );

		return( CRYPT_ARGERROR_OBJECT );
		}
	if( message == MESSAGE_CRT_EXPORT )
		{
		MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

		return( attributeCopy( msgData, certInfoPtr->certificate, 
							   certInfoPtr->certificateSize ) );
		}

	/* Nothing else is handled */
	return( CRYPT_ERROR_NOTAVAIL );
	}

/* Create a certificate by instantiating it from its encoded form */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createCertificateIndirect( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
							   STDC_UNUSED const void *auxDataPtr, 
							   STDC_UNUSED const int auxValue )
	{
	CRYPT_CERTIFICATE iCertificate;
	CERT_INFO *certInfoPtr;
	void *certBuffer;
	int status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( isEnumRangeOpt( createInfo->arg1, CRYPT_CERTTYPE ) );
	REQUIRES( createInfo->strArg1 != NULL );
	REQUIRES( createInfo->strArgLen1 > 16 && \
			  createInfo->strArgLen1 < MAX_INTLENGTH ); 
			  /* May be CMS attribute (short) or a mega-CRL (long ) */
	REQUIRES( createInfo->arg2 == 0 && createInfo->strArg2 == NULL && \
			  createInfo->strArgLen2 == 0 );	/* keyID */
	REQUIRES( isFlagRangeZ( createInfo->arg3, KEYMGMT ) && \
			  ( createInfo->arg3 & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
	REQUIRES( createInfo->arg2 == 0 || createInfo->arg3 == 0 );

	/* Allocate a buffer for the encoded certificate data and create and 
	   initialise the certificate object associated with it */
	REQUIRES( rangeCheck( createInfo->strArgLen1, 
						  16 + 1, MAX_INTLENGTH - 1 ) );
	if( ( certBuffer = clAlloc( "createCertificateIndirect", 
								createInfo->strArgLen1 ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	status = krnlCreateObject( &iCertificate, ( void ** ) &certInfoPtr, 
							   sizeof( CERT_INFO ) + sizeof( CERT_CERT_INFO ), 
							   OBJECT_TYPE_CERTIFICATE, SUBTYPE_CERT_CERT,
							   CREATEOBJECT_FLAG_NONE, 
							   DEFAULTUSER_OBJECT_HANDLE, ACTION_PERM_NONE_ALL, 
							   certificateMessageFunction );
	if( cryptStatusError( status ) )
		{
		clFree( "createCertificateIndirect", certBuffer );
		return( status );
		}
	ANALYSER_HINT( certInfoPtr != NULL );
	certInfoPtr->objectHandle = iCertificate;
	certInfoPtr->ownerHandle = DEFAULTUSER_OBJECT_HANDLE;
	certInfoPtr->type = CRYPT_CERTTYPE_CERTIFICATE;
	certInfoPtr->cCertCert = ( CERT_CERT_INFO * ) certInfoPtr->storage;
	certInfoPtr->cCertCert->chainPos = CRYPT_ERROR;
	certInfoPtr->cCertCert->trustedUsage = CRYPT_ERROR;
	certInfoPtr->iPubkeyContext = CRYPT_ERROR;
	certInfoPtr->flags |= CERT_FLAG_DATAONLY;
	DATAPTR_SET( certInfoPtr->subjectName, NULL );
	DATAPTR_SET( certInfoPtr->issuerName, NULL );
	initSelectionInfo( certInfoPtr );

	/* Copy in the certificate object for later use */
	memcpy( certBuffer, createInfo->strArg1, createInfo->strArgLen1 );
	certInfoPtr->certificate = certBuffer;
	certInfoPtr->certificateSize = createInfo->strArgLen1;

	/* We've finished setting up the object-type-specific information, tell 
	   the kernel that the object is ready for use */
	status = krnlSendMessage( iCertificate, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusOK( status ) )
		{
		status = krnlSendMessage( iCertificate, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_UNUSED, 
								  CRYPT_IATTRIBUTE_INITIALISED );
		}
	if( cryptStatusOK( status ) )
		createInfo->cryptHandle = iCertificate;
	return( status );
	}
#endif /* USE_CERTIFICATES */
