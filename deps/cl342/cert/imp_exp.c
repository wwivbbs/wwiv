/****************************************************************************
*																			*
*						Certificate Import/Export Routines					*
*						Copyright Peter Gutmann 1997-2012					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*					base64/PEM/SMIME Processing Functions					*
*																			*
****************************************************************************/

#ifdef USE_BASE64

/* Decode a base64/PEM/SMIME-encoded certificate into a temporary buffer */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int decodeCertificate( IN_BUFFER( certObjectLength ) const void *certObject, 
							  IN_DATALENGTH const int certObjectLength,
							  OUT_PTR_COND void **newObject, 
							  OUT_DATALENGTH_Z int *newObjectLength )
	{
	void *decodedObjectPtr;
	int decodedLength, status;

	assert( isReadPtr( certObject, certObjectLength ) );
	assert( isWritePtr( newObject, sizeof( void * ) ) );
	assert( isWritePtr( newObjectLength, sizeof( int ) ) );

	REQUIRES( certObjectLength >= MIN_CERTSIZE && \
			  certObjectLength < MAX_BUFFER_SIZE );

	/* Clear return values */
	*newObject = NULL;
	*newObjectLength = 0;

	/* Allocate a temporary buffer to decode the certificate object data 
	   into */
	status = base64decodeLen( certObject, certObjectLength, 
							  &decodedLength );
	if( cryptStatusError( status ) )
		return( status );
	if( decodedLength < MIN_CERTSIZE || \
		decodedLength >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_UNDERFLOW );
	if( ( decodedObjectPtr = clAlloc( "decodeCertificate", \
									  decodedLength + 8 ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );

	/* Decode the base64/PEM/SMIME-encoded certificate data into the 
	   temporary buffer */
	status = base64decode( decodedObjectPtr, decodedLength, &decodedLength,
						   certObject, certObjectLength, 
						   CRYPT_CERTFORMAT_TEXT_CERTIFICATE );
	if( cryptStatusOK( status ) )
		{
		/* Make sure that the decoded data length is still valid.  We don't 
		   allow long lengths here because we shouldn't be getting things 
		   like mega-CRLs as email messages */
		if( decodedLength < MIN_CERTSIZE || \
			decodedLength >= MAX_INTLENGTH_SHORT )
			status = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( status ) )
		{
		clFree( "decodeCertificate", decodedObjectPtr );
		return( status );
		}
	*newObject = decodedObjectPtr;
	*newObjectLength = decodedLength;

	return( CRYPT_OK );
	}

/* Detect the encoded form of certificate data, either raw binary, raw
   binary with a MIME header, or some form of text encoding.  Autodetection 
   in the presence of EBCDIC gets a bit more complicated because the text
   content can potentially be either EBCDIC or ASCII so we have to add an
   extra layer of checking for EBCDIC */

#ifdef EBCDIC_CHARS

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int checkTextEncoding( IN_BUFFER( certObjectLength ) const void *certObject, 
							  IN_DATALENGTH const int certObjectLength,
							  OUT_PTR void **newObject, 
							  OUT_DATALENGTH_Z int *newObjectLength )
	{
	CRYPT_CERTFORMAT_TYPE format;
	void *asciiObject = NULL;
	int offset, status;

	assert( isReadPtr( certObject, certObjectLength ) );
	assert( isWritePtr( newObject, sizeof( void * ) ) );
	assert( isWritePtr( newObjectLength, sizeof( int ) ) );

	REQUIRES( certObjectLength >= MIN_CERTSIZE && \
			  certObjectLength < MAX_BUFFER_SIZE );

	/* Initialise the return values to the default settings, the identity
	   transformation */
	*newObject = ( void * ) certObject;
	*newObjectLength = certObjectLength;

	/* Check for a header that identifies some form of encoded object */
	status = base64checkHeader( certObject, certObjectLength, &format, 
								&offset );
	if( cryptStatusError( status ) )
		{
		int status;

		/* If we get a decoding error (i.e. it's not either an unencoded 
		   object or some form of ASCII encoded object) try again assuming
		   that the source is EBCDIC */
		if( ( asciiObject = clAlloc( "checkTextEncoding",
									 certObjectLength + 8 ) ) == NULL )
			return( CRYPT_ERROR_MEMORY );
		status = ebcdicToAscii( asciiObject, certObject, certObjectLength );
		if( cryptStatusError( status ) )
			return( status );
		certObject = asciiObject;
		status = base64checkHeader( certObject, certObjectLength, &format, 
									&offset );
		if( cryptStatusOK( status ) && \
			( format != CRYPT_ICERTFORMAT_SMIME_CERTIFICATE && \
			  format != CRYPT_CERTFORMAT_TEXT_CERTIFICATE ) )
			{
			clFree( "checkTextEncoding", asciiObject );
			asciiObject = NULL;
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* If it's not encoded in any way, we're done */
	if( format == CRYPT_CERTFORMAT_NONE )
		return( CRYPT_OK );

	/* Make sure that the length after (potentially) skipping the header is 
	   still valid, since this will now be different from the length that 
	   was validated by the kernel */
	if( certObjectLength - offset < MIN_CERTSIZE || \
		certObjectLength - offset > MAX_INTLENGTH )
		{
		if( asciiObject != NULL )
			clFree( "checkTextEncoding", asciiObject );
		return( CRYPT_ERROR_UNDERFLOW );
		}

	if( format == CRYPT_ICERTFORMAT_SMIME_CERTIFICATE || \
		format == CRYPT_CERTFORMAT_TEXT_CERTIFICATE )
		{
		status = decodeCertificate( ( const char * ) certObject + offset, 
									certObjectLength - offset, newObject,
									newObjectLength );
		if( asciiObject != NULL )
			clFree( "checkTextEncoding", asciiObject );
		if( cryptStatusError( status ) )
			return( status );

		/* Let the caller know that they have to free the temporary decoding
		   buffer before they exit.  This is somewhat ugly but necessary for
		   the EBCDIC case because of the extra level of conversion that's 
		   required before we can base64-decode it */
		return( OK_SPECIAL );
		}

	/* If it's binary-encoded MIME data we don't need to decode it but still 
	   need to skip the MIME header */
	if( format == CRYPT_CERTFORMAT_CERTIFICATE || \
		format == CRYPT_CERTFORMAT_CERTCHAIN )
		{
		ENSURES( offset > 0 && offset < MAX_INTLENGTH );

		*newObject = ( BYTE * ) certObject + offset;
		*newObjectLength = certObjectLength - offset;
		}

	return( CRYPT_OK );
	}
#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int checkTextEncoding( IN_BUFFER( certObjectLength ) const void *certObject, 
							  IN_DATALENGTH const int certObjectLength,
							  OUT_PTR void **objectData, 
							  OUT_DATALENGTH int *objectDataLength )
	{
	CRYPT_CERTFORMAT_TYPE format;
	int offset, status;

	assert( isReadPtr( certObject, certObjectLength ) );
	assert( isWritePtr( objectData, sizeof( void * ) ) );
	assert( isWritePtr( objectDataLength, sizeof( int ) ) );

	REQUIRES( certObjectLength >= MIN_CERTSIZE && \
			  certObjectLength < MAX_BUFFER_SIZE );

	/* Initialise the return values to the default settings, the identity
	   transformation */
	*objectData = ( void * ) certObject;
	*objectDataLength = certObjectLength;

	/* Check for a header that identifies some form of encoded object */
	status = base64checkHeader( certObject, certObjectLength, &format, 
								&offset );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's not encoded in any way, we're done */
	if( format == CRYPT_CERTFORMAT_NONE )
		return( CRYPT_OK );

	/* Make sure that the length after (potentially) skipping the header is 
	   still valid, since this will now be different from the length that 
	   was validated by the kernel */
	if( certObjectLength - offset < MIN_CERTSIZE || \
		certObjectLength - offset > MAX_INTLENGTH )
		return( CRYPT_ERROR_UNDERFLOW );

	/* Remember the position of the payload, which may be either binary 
	   (with a MIME header) or base64-encoded (with or without a header) */
	*objectData = ( BYTE * ) certObject + offset;
	*objectDataLength = certObjectLength - offset;
	ENSURES( rangeCheckZ( offset, *objectDataLength, certObjectLength ) );

	/* It's base64-encoded let the caller know that it needs decoding */
	return( ( format == CRYPT_ICERTFORMAT_SMIME_CERTIFICATE || \
			  format == CRYPT_CERTFORMAT_TEXT_CERTIFICATE ) ? \
			OK_SPECIAL : CRYPT_OK );
	}
#endif /* EBCDIC_CHARS */
#endif /* USE_BASE64 */

/****************************************************************************
*																			*
*								Import a Certificate						*
*																			*
****************************************************************************/

/* Import a certificate object.  If the import type is set to create a data-
   only certificate then its publicKeyInfo pointer is set to the start of 
   the encoded public key to allow it to be decoded later */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int importCert( IN_BUFFER( certObjectLength ) const void *certObject, 
				IN_DATALENGTH const int certObjectLength,
				OUT_HANDLE_OPT CRYPT_CERTIFICATE *certificate,
				IN_HANDLE const CRYPT_USER iCryptOwner,
				IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
				IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
				IN_LENGTH_KEYID_Z const int keyIDlength,
				IN_FLAGS_Z( KEYMGMT ) const int options,
				IN_ENUM_OPT( CRYPT_CERTTYPE ) \
					const CRYPT_CERTTYPE_TYPE formatHint )
	{
	CERT_INFO *certInfoPtr;
	CRYPT_CERTTYPE_TYPE type, baseType;
	STREAM stream;
	READCERT_FUNCTION readCertFunction;
	BOOLEAN isDecodedObject = FALSE;
	void *certObjectPtr = ( void * ) certObject, *certBuffer;
	int objectLength = certObjectLength, length, offset = 0;
	int complianceLevel, initStatus = CRYPT_OK, status;

	assert( isReadPtr( certObject, certObjectLength ) );
	assert( isWritePtr( certificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( ( keyIDtype == CRYPT_KEYID_NONE && \
			  keyID == NULL && keyIDlength == 0 ) || \
			( keyIDtype != CRYPT_KEYID_NONE && \
			  isReadPtr( keyID, keyIDlength ) ) );

	REQUIRES( certObjectLength >= 16 && \
			  certObjectLength < MAX_BUFFER_SIZE );
			  /* May be CMS attribute (short) or a mega-CRL (long ) */
	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				keyID == NULL && keyIDlength == 0 ) || \
			  ( ( keyIDtype > CRYPT_KEYID_NONE && \
				  keyIDtype < CRYPT_KEYID_LAST ) && \
				keyID != NULL && \
				keyIDlength >= MIN_NAME_LENGTH && \
				keyIDlength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				options == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype != CRYPT_KEYID_NONE && \
				options == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype == CRYPT_KEYID_NONE && \
				options != KEYMGMT_FLAG_NONE ) );
	REQUIRES( formatHint >= CRYPT_CERTTYPE_NONE && \
			  formatHint < CRYPT_CERTTYPE_LAST );

	/* Clear return value */
	*certificate = CRYPT_ERROR;

	/* Determine how much checking we need to perform */
	status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE, 
							  &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's not a pre-specified or special-case format, check whether 
	   it's some form of encoded certificate object, and decode it if 
	   required */
#ifdef USE_BASE64
	if( formatHint == CRYPT_CERTTYPE_NONE )
		{
		status = checkTextEncoding( certObject, certObjectLength,
									&certObjectPtr, &objectLength );
		if( cryptStatusError( status ) )
			{
			if( status != OK_SPECIAL )
				return( status );
#ifndef EBCDIC_CHARS
			status = decodeCertificate( certObjectPtr, objectLength, 
										&certObjectPtr, &objectLength );
			if( cryptStatusError( status ) )
				return( status );
#endif /* EBCDIC_CHARS */

			/* The certificate object has been decoded into a temporary 
			   buffer, remember that we have to free it before we exit */
			isDecodedObject = TRUE;
			}		
		}
#endif /* USE_BASE64 */

	/* Check the object's encoding unless we're running in oblivious mode 
	   (qui omnes insidias timet in nullas incidit - Syrus).  In addition we 
	   can't perform the check for SSL certificate chains because there's 
	   SSL length data between each certificate, so it has to be performed 
	   later when each certificate in the chain is read */
#ifndef CONFIG_FUZZ
	if( complianceLevel > CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
		formatHint != CRYPT_ICERTTYPE_SSL_CERTCHAIN )
		{
		status = objectLength = \
			checkObjectEncoding( certObjectPtr, objectLength );
		if( cryptStatusError( status ) )
			{
			if( isDecodedObject )
				clFree( "importCert", certObjectPtr );
			return( status );
			}
		}
#endif /* CONFIG_FUZZ */

	/* Determine the object's type and length */
	sMemConnect( &stream, certObjectPtr, objectLength );
	status = getCertObjectInfo( &stream, &offset, &length, &type, 
								formatHint );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		{
		if( isDecodedObject )
			clFree( "importCert", certObjectPtr );
		return( status );
		}

	/* If we're fuzzing the base64 decoding then we can exit now.  We exit 
	   with an error code since we haven't actually created a certificate at 
	   this point */
#ifdef CONFIG_FUZZ
	if( isDecodedObject )
		{
		clFree( "importCert", certObjectPtr );
		return( CRYPT_ERROR_BADDATA );
		}
#endif /* CONFIG_FUZZ */

	/* Some of the certificate types have multiple formats that are mapped 
	   to a single base type, for example the various ways of representing
	   a certificate chain that all end up as a CRYPT_CERTTYPE_CERTCHAIN.
	   Before we continue we have to sort out what input type will end up as
	   which base type */
	switch( type )
		{
		case CRYPT_ICERTTYPE_CMS_CERTSET:
		case CRYPT_ICERTTYPE_SSL_CERTCHAIN:
			baseType = CRYPT_CERTTYPE_CERTCHAIN;
			break;

		default:
			baseType = type;
			break;
		}

	/* If it's a certificate chain this is handled specially since we need 
	   to import a plurality of certificates at once */
	if( baseType == CRYPT_CERTTYPE_CERTCHAIN )
		{
		/* Read the certificate chain into a collection of internal 
		   certificate objects.  This returns a handle to the leaf 
		   certificate in the chain with the remaining certificates being 
		   accessible within it via the certificate cursor functions.  
		   Because the different chain types are used only to distinguish 
		   the chain wrapper type on import, the final object type which is 
		   created is always a CRYPT_CERTTYPE_CERTCHAIN no matter what the 
		   import format was */
		ENSURES( rangeCheckZ( offset, length, objectLength ) );
		sMemConnect( &stream, ( BYTE * ) certObjectPtr + offset, length );
		status = readCertChain( &stream, certificate, iCryptOwner, type, 
								keyIDtype, keyID, keyIDlength, options );
		sMemDisconnect( &stream );
		if( isDecodedObject )
			clFree( "importCert", certObjectPtr );
		return( status );
		}

	ENSURES( keyIDtype == CRYPT_KEYID_NONE && keyID == NULL && \
			 keyIDlength == 0 );

	/* Select the function to use to read the certificate object */
	readCertFunction = getCertReadFunction( baseType );
	if( readCertFunction == NULL )
		{
		/* In theory this should be an internal error if there's no decode 
		   function corresponding to an identified certificate object type
		   present, however with the ability to selectively disable some
		   capabilities it may just correspond to a disabled capability so
		   we return a not-available error instead */
		if( isDecodedObject )
			clFree( "importCert", certObjectPtr );
		return( CRYPT_ERROR_NOTAVAIL );
		}
	
	/* Allocate a buffer to store a copy of the object so that we can 
	   preserve the original for when it's needed again later, and try and 
	   create the certificate object.  All of the objects (including the CMS 
	   attributes, which in theory aren't needed for anything further) need 
	   to be kept around in their encoded form, which is often incorrect and 
	   therefore can't be reconstructed from the decoded information.  The 
	   readXXX() functions also record pointers to the various required 
	   encoded fields such as DNs and key data so that they can be recovered 
	   later in their (possibly incorrect) form and these pointers need to 
	   be to a persistent copy of the encoded object.  In addition the 
	   certificate objects need to be kept around anyway for signature 
	   checks and possible re-export */
	if( ( certBuffer = clAlloc( "importCert", length ) ) == NULL )
		{
		if( isDecodedObject )
			clFree( "importCert", certObjectPtr );
		return( CRYPT_ERROR_MEMORY );
		}

	/* Create the certificate object */
	status = createCertificateInfo( &certInfoPtr, iCryptOwner, 
						( options & KEYMGMT_FLAG_CERT_AS_CERTCHAIN ) ? \
						  CRYPT_CERTTYPE_CERTCHAIN : baseType );
	if( cryptStatusError( status ) )
		{
		if( isDecodedObject )
			clFree( "importCert", certObjectPtr );
		clFree( "importCert", certBuffer );
		return( status );
		}
	*certificate = certInfoPtr->objectHandle;

	/* If we're doing a deferred read of the public key components (they'll
	   be decoded later when we know whether we need them) set the data-only
	   flag to ensure that we don't try to decode them */
	if( options & KEYMGMT_FLAG_DATAONLY_CERT )
		certInfoPtr->flags |= CERT_FLAG_DATAONLY;

	/* If we're reading a single entry from a CRL, indicate that the 
	   resulting object is a standalone single CRL entry rather than a proper
	   CRL */
	if( formatHint == CRYPT_ICERTTYPE_REVINFO )
		certInfoPtr->flags |= CERT_FLAG_CRLENTRY;

	/* Copy in the certificate object for later use */
	ENSURES( rangeCheckZ( offset, length, objectLength ) );
	memcpy( certBuffer, ( BYTE * ) certObjectPtr + offset, length );
	certInfoPtr->certificate = certBuffer;
	certInfoPtr->certificateSize = length;

	/* Parse the data into the certificate object.  Note that we have to use 
	   the copy in the certBuffer rather than the original since the 
	   readXXX() functions will record pointers to various required encoded 
	   fields */
	sMemConnect( &stream, certBuffer, length );
	if( baseType != CRYPT_CERTTYPE_CMS_ATTRIBUTES && \
		baseType != CRYPT_CERTTYPE_RTCS_REQUEST && \
		baseType != CRYPT_CERTTYPE_RTCS_RESPONSE )
		{
		/* Skip the outer wrapper */
		readLongSequence( &stream, NULL );
		}
	status = readCertFunction( &stream, certInfoPtr );
	if( cryptStatusOK( status ) && \
		( baseType == CRYPT_CERTTYPE_CERTIFICATE || \
		  baseType == CRYPT_CERTTYPE_ATTRIBUTE_CERT || \
		  baseType == CRYPT_CERTTYPE_CRL || \
		  baseType == CRYPT_CERTTYPE_CERTREQUEST ) )
		{
		CRYPT_ALGO_TYPE dummyAlgo1, dummyAlgo2;
		int dummyParam;

		/* If it's one of the standard certificate object types, check that
		   the signature information is present and valid.  This will be
		   checked explicitly anyway when the object's signature is checked,
		   but we perform at least a basic sanity check here to make sure 
		   that the information is actually present since the check might
		   otherwise be skipped if the user doesn't perform an explicit
		   signature check.
		   
		   Since this is only a sanity/presence-check, we don't perform any 
		   algorithm or signature-data validation but just verify that the 
		   information is present and seems approximately valid, using the 
		   code that's used in readX509Signature() in mechs/sign_rw.c.  Any 
		   in-depth validation is deferred for the full signature check 
		   because at this point we don't know whether the presence of (say) 
		   an RSA or a DSA or an ECDSA signature is correct, or whether the 
		   signature key size is appropriate, without having the signing key 
		   present.

		   For the other object types like CMP/OCSP/RTCS requests and 
		   responses we don't perform the check for the dual reasons that 
		   the signatures are optional for many of the object types and that 
		   if they are present they have gratuitously nonstandard formats, 
		   or in some cases aren't signatures at all but things like MAC 
		   values.  Since these can only be verified in the context of the
		   protocol with which they're used, we defer the checking to the
		   protocol-specific code */
		status = readAlgoIDex( &stream, &dummyAlgo1, &dummyAlgo2, 
							   &dummyParam, ALGOID_CLASS_PKCSIG );
		if( cryptStatusOK( status ) )
			status = readBitStringHole( &stream, NULL, 18 + 18, DEFAULT_TAG );
		}
	sMemDisconnect( &stream );
	if( isDecodedObject )
		clFree( "importCert", certObjectPtr );
	if( cryptStatusError( status ) )
		{
		/* The import failed, make sure that the object gets destroyed when 
		   we notify the kernel that the setup process is complete.  We also
		   have to explicitly destroy the attached context since at this
		   point it hasn't been associated with the certificate yet so it
		   won't be automatically destroyed by the kernel when the 
		   certificate is destroyed */
		krnlSendNotifier( *certificate, IMESSAGE_DESTROY );
		if( certInfoPtr->iPubkeyContext != CRYPT_ERROR )
			{
			krnlSendNotifier( certInfoPtr->iPubkeyContext, 
							  IMESSAGE_DECREFCOUNT );
			certInfoPtr->iPubkeyContext = CRYPT_ERROR;
			}
		initStatus = status;
		}

	/* We've finished setting up the object-type-specific information, tell 
	   the kernel that the object is ready for use */
	status = krnlSendMessage( *certificate, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		{
		*certificate = CRYPT_ERROR;
		return( cryptStatusError( initStatus ) ? initStatus : status );
		}

	/* If this is a type of object that has a public key associated with it, 
	   notify the kernel that the given context is attached to the 
	   certificate.  Note that we can only do this at this point because the 
	   certificate object can't receive general messages until its status is 
	   set to OK.  In addition since this is an internal object used only by 
	   the certificate we tell the kernel not to increment its reference 
	   count when it attaches it to the certificate object.  Finally, we're 
	   ready to go so we mark the object as initialised (we can't do this 
	   before the initialisation is complete because the kernel won't 
	   forward the message to a not-ready-for-use object)*/
	if( certInfoPtr->iPubkeyContext != CRYPT_ERROR )
		{
		krnlSendMessage( *certificate, IMESSAGE_SETDEPENDENT,
						 &certInfoPtr->iPubkeyContext, 
						 SETDEP_OPTION_NOINCREF );
		}
	return( krnlSendMessage( *certificate, IMESSAGE_SETATTRIBUTE,
							 MESSAGE_VALUE_UNUSED, 
							 CRYPT_IATTRIBUTE_INITIALISED ) );
	}

/****************************************************************************
*																			*
*								Import a Certificate						*
*																			*
****************************************************************************/

/* Export a certificate object.  This just writes the internal encoded 
   object data to an external buffer.  For certificate/certificate chain 
   export the possibilities are as follows:

						Export
	Type  |		Cert				Chain
	------+--------------------+---------------
	Cert  | Cert			   | Cert as chain
		  |					   |
	Chain | Currently selected | Chain
		  | cert in chain	   |					*/

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 5 ) ) \
int exportCert( OUT_BUFFER_OPT( certObjectMaxLength, *certObjectLength ) \
					void *certObject, 
				IN_DATALENGTH_Z const int certObjectMaxLength, 
				OUT_DATALENGTH_Z int *certObjectLength,
				IN_ENUM( CRYPT_CERTFORMAT ) \
					const CRYPT_CERTFORMAT_TYPE certFormatType,
				const CERT_INFO *certInfoPtr )
	{
	const CRYPT_CERTFORMAT_TYPE baseFormatType = \
		( certFormatType == CRYPT_CERTFORMAT_TEXT_CERTIFICATE || \
		  certFormatType == CRYPT_CERTFORMAT_XML_CERTIFICATE ) ? \
			CRYPT_CERTFORMAT_CERTIFICATE : \
		( certFormatType == CRYPT_CERTFORMAT_TEXT_CERTCHAIN || \
		  certFormatType == CRYPT_CERTFORMAT_XML_CERTCHAIN ) ? \
			CRYPT_CERTFORMAT_CERTCHAIN : \
			certFormatType;
	STREAM stream;
#ifdef USE_BASE64
	void *buffer;
#endif /* USE_BASE64 */
	int length, encodedLength, status = CRYPT_ARGERROR_VALUE;

	assert( ( certObject == NULL && certObjectMaxLength == 0 ) || \
			isWritePtr( certObject, certObjectMaxLength ) );
	assert( isWritePtr( certObjectLength, sizeof( int ) ) );
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( ( certObject == NULL && certObjectMaxLength == 0 ) || \
			  ( certObject != NULL && \
				certObjectMaxLength >= MIN_CERTSIZE && \
				certObjectMaxLength < MAX_BUFFER_SIZE ) );
	REQUIRES( certFormatType > CRYPT_CERTFORMAT_NONE && \
			  certFormatType < CRYPT_CERTFORMAT_LAST );

	/* If it's an internal format, write it and exit */
	if( certFormatType == CRYPT_ICERTFORMAT_CERTSET || \
		certFormatType == CRYPT_ICERTFORMAT_CERTSEQUENCE || \
		certFormatType == CRYPT_ICERTFORMAT_SSL_CERTCHAIN )
		{
		length = sizeofCertCollection( certInfoPtr, certFormatType );

		if( cryptStatusError( length ) )
			return( length );
		*certObjectLength = length;
		if( certObject == NULL )
			return( CRYPT_OK );
		if( length > certObjectMaxLength )
			return( CRYPT_ERROR_OVERFLOW );
		sMemOpen( &stream, certObject, length );
		status = writeCertCollection( &stream, certInfoPtr,
									  certFormatType );
		sMemDisconnect( &stream );
		return( status );
		}

	/* Determine how big the output object will be */
	length = encodedLength = certInfoPtr->certificateSize;
				/* Placed here to get rid of not-inited warnings */
	if( baseFormatType == CRYPT_CERTFORMAT_CERTCHAIN )
		{
		STREAM nullStream;

		ENSURES( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
				 certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN );

		sMemNullOpen( &nullStream );
		status = writeCertChain( &nullStream, certInfoPtr );
		if( cryptStatusOK( status ) )
			length = encodedLength = stell( &nullStream );
		sMemClose( &nullStream );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( baseFormatType != certFormatType )
		{
#ifdef USE_BASE64 
		/* If we're exporting a certificate or certificate chain then the 
		   final format will be affected by the format specifier as per 
		   the table in the comment at the start of this function, so for 
		   these two types the final format is defined by the format 
		   specifier, not by the underlying object type */
		if( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
			certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN )
			{
			status = base64encodeLen( length, &encodedLength, 
				( baseFormatType == CRYPT_CERTFORMAT_CERTCHAIN ) ? \
				CRYPT_CERTTYPE_CERTCHAIN : CRYPT_CERTTYPE_CERTIFICATE );
			}
		else
			{
			status = base64encodeLen( length, &encodedLength, 
									  certInfoPtr->type );
			}
		if( cryptStatusError( status ) )
			return( status );
#else
		/* If text-encoding isn't enabled then an attempt to use it is an 
		   error */
		return( CRYPT_ARGERROR_VALUE );
#endif /* USE_BASE64 */
		}

	/* Set up the length information */
	*certObjectLength = encodedLength;
	if( certObject == NULL )
		return( CRYPT_OK );
	if( encodedLength > certObjectMaxLength )
		return( CRYPT_ERROR_OVERFLOW );
	if( !isWritePtr( certObject, encodedLength ) )
		return( CRYPT_ARGERROR_STR1 );

	/* If it's a simple format, write either the DER-encoded object data or 
	   its base64 / S/MIME-encoded form directly to the output */
	if( certFormatType == CRYPT_CERTFORMAT_CERTIFICATE || \
		certFormatType == CRYPT_ICERTFORMAT_DATA )
		{
		memcpy( certObject, certInfoPtr->certificate, length );
		return( CRYPT_OK );
		}
#ifdef USE_BASE64
	if( certFormatType == CRYPT_CERTFORMAT_TEXT_CERTIFICATE || \
		certFormatType == CRYPT_CERTFORMAT_XML_CERTIFICATE )
		{
		return( base64encode( certObject, certObjectMaxLength, 
							  certObjectLength, certInfoPtr->certificate,
							  certInfoPtr->certificateSize, 
							  certInfoPtr->type ) );
		}
#endif /* USE_BASE64 */

	/* It's a straight certificate chain, write it directly to the output */
	if( certFormatType == CRYPT_CERTFORMAT_CERTCHAIN )
		{
		sMemOpen( &stream, certObject, length );
		status = writeCertChain( &stream, certInfoPtr );
		sMemDisconnect( &stream );
		return( status );
		}

	/* It's a base64 / S/MIME-encoded certificate chain, write it to a 
	   temporary buffer and then encode it to the output */
#ifdef USE_BASE64
	ENSURES( certFormatType == CRYPT_CERTFORMAT_TEXT_CERTCHAIN || \
			 certFormatType == CRYPT_CERTFORMAT_XML_CERTCHAIN );
	if( ( buffer = clAlloc( "exportCert", length ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	sMemOpen( &stream, buffer, length );
	status = writeCertChain( &stream, certInfoPtr );
	if( cryptStatusOK( status ) )
		{
		status = base64encode( certObject, certObjectMaxLength, 
							   certObjectLength, buffer, length, 
							   CRYPT_CERTTYPE_CERTCHAIN );
		}
	sMemClose( &stream );
	clFree( "exportCert", buffer );
#endif /* USE_BASE64 */

	return( status );
	}
#endif /* USE_CERTIFICATES */
