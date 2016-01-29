/****************************************************************************
*																			*
*					Certificate Import Format-check Routines				*
*						Copyright Peter Gutmann 1997-2008					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "asn1_ext.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
#endif /* Compiler-specific includes */

/* Oddball OIDs that may be used to wrap certificates */

#define OID_X509_USERCERTIFICATE	MKOID( "\x06\x03\x55\x04\x24" )

/* If fed an unknown object from the external source we can (with some 
   difficulty) determine its type at runtime (although it's hardly LL(1)) 
   and import it as appropriate.  If fed an object by a cryptlib-internal 
   function, the exact type will always be known.
   
   If the data starts with a [0] it's CMS attributes.  If it starts with a 
   sequence followed by an OID it's a certificate chain/sequence or (rarely) 
   a certificate wrapped up in some weird packaging.  If it starts with a 
   sequence followed by an integer (version = 3) it's a PKCS #12 mess.  
   Otherwise it follows the general pattern SEQUENCE { tbsSomething, 
   signature }, and it's at this point that distinguishing the different 
   types gets tricky, with the following top-level formats being possible:

	Cert:			SEQUENCE { SEQUENCE {
						[0] EXPLICIT ... OPTIONAL,
							INTEGER,
							AlgorithmID,
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	Attribute cert:	SEQUENCE { SEQUENCE {
							INTEGER,
							SEQUENCE {
								[1]	Name,
								...
								}

	CRL:			SEQUENCE { SEQUENCE {
							INTEGER OPTIONAL,
							AlgorithmID,
							Name,
							UTCTime | GeneralizedTime

	Cert request:	SEQUENCE { SEQUENCE {
							INTEGER,
							Name,
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:	SEQUENCE { SEQUENCE {
							INTEGER,
							SEQUENCE {
								[0] ... [9]		-- cert request should have 
												-- [6] SubjectPublicKeyInfo

	CRMF rev.req:	SEQUENCE { SEQUENCE {
							[0] ... [9]			-- Should have [1] INTEGER 
												-- (= serialNo)

	OCSP request:	SEQUENCE { SEQUENCE {
							[0] EXPLICIT ... OPTIONAL,
							[1]	EXPLICIT ... OPTIONAL,
							SEQUENCE { SEQUENCE {
								SEQUENCE ...	-- For certID, abandoned v2 also
												-- allowed [0] | [1] | [2] | [3]
												-- for other IDs.
	
	OCSP resp:		SEQUENCE { SEQUENCE {
							[0] EXPLICIT ... OPTIONAL,
							[1] | [2] ...,		-- responderID = CHOICE [1] | [2]
							GeneralizedTime

	RTCS request:	SEQUENCE { SEQUENCE { 
							SEQUENCE { 
								OCTET STRING	-- certHash

	RTCS resp:		SEQUENCE { SEQUENCE 
							OCTET STRING		-- certHash

	PKI user:		SEQUENCE { SEQUENCE {		-- Name
							SET ... | empty 	-- RDN or zero-length DN

   The first step is to strip out the SEQUENCE { SEQUENCE, which is shared 
   by all objects.  In addition we can remove the [0] ... OPTIONAL and
   [1] ... OPTIONAL, which isn't useful in distinguishing anything.  Since 
   the standard OCSP response can also have [2] in place of the [1] and 
   leaving it in isn't notably useful we strip this as well.  Note that PKI 
   user information can be left in one of two states depending on whether 
   there's a DN present.  Rather than parse down into the rest of the PKI 
   user object (the next element is an AlgorithmID that clashes with a 
   certificate and CRL) we use the presence of a zero-length sequence to 
   identify a PKI user object with an absent DN.  This leaves the following:

	Cert:					INTEGER,
							AlgorithmID,
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	Attribute cert:			INTEGER,
							SEQUENCE {
								[1]	Name,
								...
								}

	CRL:					INTEGER OPTIONAL,
							AlgorithmID,
							Name,
							UTCTime | GeneralizedTime

	Cert request:			INTEGER,
							Name,
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:			INTEGER,
							SEQUENCE {
								[0] | [1] |	-	-- Implicitly tagged
								[3] ... [9]		-- [2] stripped

	CRMF rev.req:			[0] | [1] | -		-- Implicitly tagged
							[3] ... [9]			-- [2] stripped

	OCSP request:			SEQUENCE { SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs
	
	OCSP resp:				GeneralizedTime

	RTCS request:			SEQUENCE { 
								OCTET STRING	-- certHash

	RTCS resp:				OCTET STRING		-- certHash

	PKI user:				SET ...				-- RDN

   Next we have the INTEGER, which also isn't notably useful.  Stripping this
   leaves:

	Cert:					AlgorithmID,
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	Attribute cert:			SEQUENCE {
								[1]	Name,		-- Constructed tag
								...
								}

	CRL:					AlgorithmID,
							Name,
							UTCTime | GeneralizedTime

	Cert request:			Name,
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:			SEQUENCE {
								[0] | [1] | -	-- Primitive tag
								[3] ... [9]		-- [2] stripped

	CRMF rev.req:			[0] | [1] |	-		-- Primitive tag
							[3] ... [9]			-- [2] stripped

	OCSP request:			SEQUENCE { SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs
	
	OCSP resp:				GeneralizedTime

	RTCS request:			SEQUENCE { 
								OCTET STRING	-- certHash

	RTCS resp:				OCTET STRING		-- certHash

	PKI user:				SET ...				-- RDN

   We can now immediately identify the attribute certificate by the [1] ...
   constructed tag, a CRMF revocation request by the not-stripped [0] or [1] 
   primitive tags (implicitly tagged INTEGER) or [3]...[9] ..., an OCSP 
   response by the GeneralizedTime, an RTCS response by the OCTET STRING, 
   and the alternative PKI user variant by the SET ..., leaving:

	Cert:					AlgorithmID,
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	CRL:					AlgorithmID,
							Name,
							UTCTime | GeneralizedTime

	Cert request:			Name,
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:			SEQUENCE {
								[3] ... [9]

	OCSP request:			SEQUENCE { SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs

	RTCS request:			SEQUENCE { 
								OCTET STRING	-- certHash


   Expanding the complex types for certificate, CRL, and certificate 
   request, we get:

	Cert:					SEQUENCE {			-- AlgorithmID
								OBJECT IDENTIFIER,
								...
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	CRL:					SEQUENCE {			-- AlgorithmID
								OBJECT IDENTIFIER,
								...
							Name,
							UTCTime | GeneralizedTime

	Cert request:			SEQUENCE {			-- Name
								SET {
									...
								...
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:			SEQUENCE {
								[3] ... [9]

	OCSP request:			SEQUENCE { SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs

	RTCS request:			SEQUENCE { 
								OCTET STRING	-- certHash

   Stripping the first SEQUENCE { we get:

	Cert:						OBJECT IDENTIFIER,
								...
							Name,
							SEQUENCE {			-- Validity
								UTCTime | GeneralizedTime

	CRL:						OBJECT IDENTIFIER,
								...
							Name,
							UTCTime | GeneralizedTime

	Cert request:				SET {
									...
								...
							SEQUENCE {			-- SubjectPublicKeyInfo
								AlgorithmID

	CRMF request:			[3] ... [9]

	OCSP request:			SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs

	RTCS request:			OCTET STRING	-- certHash

   which allows us to distinguish certificates and CRLs (the two are 
   themselves distinguished by what follows the second Name), certificate  
   requests, and RTCS requests.  What's left now are the tricky ones, the 
   other request and response types:

	CRMF request:			[3] ... [9]

	OCSP request:			SEQUENCE {
								SEQUENCE ...	-- [0] | [1] | [2] | [3]
												-- for other IDs

   which can themselves be distinguished by the remaining data.

   Note that in practice we don't try and auto-recognise all of these 
   because some of them should never be fed to us by a user, for example a
   CRMF revocation request and PKIUser object is only valid in the context 
   of a CMP transaction and RTCS and OCSP objects are only valid in the 
   contxt of RTCS or OCSP transactions, however we make an exception for 
   CRMF certification requests and OCSP responses because they're used in 
   the self-test */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*							Format Check Routines							*
*																			*
****************************************************************************/

/* Process the PKCS #7/CMS wrapper that can surround a certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int processCertWrapper( INOUT STREAM *stream, 
							   OUT_LENGTH_SHORT_Z int *objectOffset, 
							   OUT_DATALENGTH_Z int *objectLength, 
							   IN_DATALENGTH_Z const int objectStartPos )
	{
	static const CMS_CONTENT_INFO FAR_BSS oidInfoSignedData = { 1, 3 };
	static const OID_INFO FAR_BSS signedDataOIDinfo[] = {
		{ OID_CMS_SIGNEDDATA, CRYPT_OK, &oidInfoSignedData },
		{ NULL, 0 }, { NULL, 0 }
		};
	static const OID_INFO FAR_BSS dataOIDinfo[] = {
		{ OID_CMS_DATA, CRYPT_OK, NULL },
		{ NULL, 0 }, { NULL, 0 }
		};
	long length;
	int setLength, localLength, offset DUMMY_INIT, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( objectOffset, sizeof( int ) ) );
	assert( isWritePtr( objectLength, sizeof( int ) ) );

	REQUIRES( objectStartPos >= 0 && objectStartPos < MAX_BUFFER_SIZE );

	/* Clear return values */
	*objectOffset = *objectLength = 0;

	/* Read the SignedData wrapper */
	sseek( stream, objectStartPos );
	status = readCMSheader( stream, signedDataOIDinfo, 
							FAILSAFE_ARRAYSIZE( signedDataOIDinfo, OID_INFO ), 
							&length, READCMS_FLAG_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the SET OF DigestAlgorithmIdentifier, empty for a pure 
	   certificate chain, nonempty for signed data or buggy certificate 
	   chains */
	status = readSet( stream, &setLength );
	if( cryptStatusOK( status ) && setLength > 0 )
		status = sSkip( stream, setLength, MAX_INTLENGTH_SHORT );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the ContentInfo information */
	status = readCMSheader( stream, dataOIDinfo, 
							FAILSAFE_ARRAYSIZE( dataOIDinfo, OID_INFO ), 
							&length, READCMS_FLAG_INNERHEADER );
	if( cryptStatusError( status ) )
		return( status );

	/* If we've been fed signed data (i.e. the ContentInfo has the content 
	   field present), skip the content to get to the certificate chain */
	if( length > 0 )
		{
		status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* If we have an indefinite length then there could be up to three 
		   EOCs present (one for each of the SEQUENCE, [0], and OCTET 
		   STRING), which we have to skip.  In theory there could be content
		   present as well but at that point we're going to be replicating
		   large chunks of the de-enveloping code so we only try and process
		   zero-length content, created by some buggy browser's cert-export
		   code (possibly Firefox)  */
		if( length == CRYPT_UNUSED )
			{
			int i;

			for( i = 0; i < 3; i++ )
				{
				status = checkEOC( stream );
				if( cryptStatusError( status ) )
					return( status );
				if( !status )
					break;
				}
			}
		}

	/* We've reached the inner content encapsulation, find out how long the 
	   content (i.e. the chain of certificates) is */
	status = getStreamObjectLength( stream, &localLength );
	if( cryptStatusOK( status ) )
		{
		offset = stell( stream );
		status = readConstructedI( stream, NULL, 0 );
		}
	if( cryptStatusError( status ) )
		return( status );

	/* Adjust for the [0] { ... } wrapper that contains the list of 
	   certificate, returning a pointer to the collection of certificates
	   without any encapsulation */
	localLength -= stell( stream ) - offset;
	if( localLength < MIN_CERTSIZE || localLength >= MAX_INTLENGTH )
		return( CRYPT_ERROR_BADDATA );
	*objectOffset = stell( stream );
	*objectLength = localLength;
	
	return( CRYPT_OK );
	}

/* Determine the object type and how long the total object is */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int getCertObjectInfo( INOUT STREAM *stream,
					   OUT_LENGTH_SHORT_Z int *objectOffset, 
					   OUT_DATALENGTH_Z int *objectLength, 
					   OUT_ENUM_OPT( CRYPT_CERTTYPE ) \
							CRYPT_CERTTYPE_TYPE *objectType,
					   IN_ENUM( CRYPT_CERTTYPE ) \
							const CRYPT_CERTTYPE_TYPE formatHint )
	{
	static const MAP_TABLE minLengthMapTable[] = {
				{ CRYPT_CERTTYPE_NONE, MIN_CERTSIZE },
				{ CRYPT_CERTTYPE_CERTIFICATE, MIN_CERTSIZE },
				{ CRYPT_CERTTYPE_ATTRIBUTE_CERT, MIN_CERTSIZE },
				{ CRYPT_CERTTYPE_CERTCHAIN, MIN_CERTSIZE },
				{ CRYPT_ICERTTYPE_CMS_CERTSET, MIN_CERTSIZE },
				{ CRYPT_ICERTTYPE_SSL_CERTCHAIN, MIN_CERTSIZE },
				{ CRYPT_CERTTYPE_CRL, 64 },
				{ CRYPT_CERTTYPE_OCSP_RESPONSE, 64 },
				{ CRYPT_CERTTYPE_REQUEST_CERT, 64 },
				{ CRYPT_CERTTYPE_RTCS_REQUEST, 32 },
				{ CRYPT_CERTTYPE_RTCS_RESPONSE, 32 },
				{ CRYPT_CERTTYPE_OCSP_REQUEST, 32 },
				{ CRYPT_CERTTYPE_CERTREQUEST, 32 },
				{ CRYPT_CERTTYPE_PKIUSER, 32 },
				{ CRYPT_CERTTYPE_CMS_ATTRIBUTES, 16 },
				{ CRYPT_CERTTYPE_REQUEST_REVOCATION, 16 },
				{ CRYPT_ICERTTYPE_REVINFO, 16 },
				{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
				};
	BOOLEAN isContextTagged = FALSE, isLongData = FALSE;
	int tag, minLength, length, offset, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( objectOffset, sizeof( int ) ) );
	assert( isWritePtr( objectLength, sizeof( int ) ) );
	assert( isWritePtr( objectType, sizeof( CRYPT_CERTTYPE_TYPE ) ) );

	REQUIRES( formatHint >= CRYPT_CERTTYPE_NONE && \
			  formatHint < CRYPT_CERTTYPE_LAST );

	/* Clear return values */
	*objectOffset = *objectLength = 0;
	*objectType = CRYPT_CERTTYPE_NONE;

	/* Figure out how much data we need to have for a minimum-length 
	   object */
	status = mapValue( formatHint, &minLength, minLengthMapTable,
					   FAILSAFE_ARRAYSIZE( minLengthMapTable, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );

	/* If it's an SSL certificate chain then there's no recognisable 
	   tagging, however the caller will have told us what it is */
	if( formatHint == CRYPT_ICERTTYPE_SSL_CERTCHAIN )
		{
		*objectLength = sMemDataLeft( stream );
		*objectType = CRYPT_ICERTTYPE_SSL_CERTCHAIN;
		return( CRYPT_OK );
		}

	/* Check whether we need to process the object wrapper using context-
	   specific tagging rather than the usual SEQUENCE */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( tag == MAKE_CTAG( 0 ) || \
		formatHint == CRYPT_ICERTTYPE_CMS_CERTSET )
		isContextTagged = TRUE;

	/* Get the object's length */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		{
#if INT_MAX > 32767
		long longLength;

		if( status != CRYPT_ERROR_OVERFLOW )
			return( status );

		/* CRLs can grow without bounds as more and more certificates are 
		   accumulated, to handle these we have to fall back to an 
		   unconstrained length read if a standard constrained length read 
		   fails.  We also remember the fact that it's an exceptionally long 
		   object and only allow this length if we (eventually) detect that 
		   it's associated with a CRL, for anything else we return later 
		   with a CRYPT_ERROR_OVERFLOW */
		sClearError( stream );
		sseek( stream, 0 );
		status = getLongStreamObjectLength( stream, &longLength );
		if( cryptStatusError( status ) )
			return( status );
		length = ( int ) longLength;
		isLongData = TRUE;
#else
		/* Mega-CRLs are too much for 16-bit systems */
		return( status );
#endif /* 16- vs. 32/64-bit systems */
		}
	if( length < minLength || length > MAX_INTLENGTH )
		return( CRYPT_ERROR_BADDATA );
	*objectLength = length;
	offset = stell( stream );

	/* Check that the start of the object is in order */
	if( isLongData )
		status = readLongSequence( stream, NULL );
	else
		status = readConstructedI( stream, NULL, 
								   isContextTagged ? 0 : DEFAULT_TAG );
	if( cryptStatusError( status ) )
		return( status );

	/* If the caller has specified that the data is in a fixed format, don't 
	   try and recognise any other format.  This prevents security holes of 
	   the type common in Windows software where data purportedly of type A 
	   is auto-recognised as harmful type B and processed as such after being
	   passed as type A by security-checking code */
	if( formatHint != CRYPT_CERTTYPE_NONE )
		{
		switch( formatHint )
			{
			case CRYPT_ICERTTYPE_REVINFO:
				/* Single CRL entry, treated as standard CRL with portions 
				   missing */
				*objectType = CRYPT_CERTTYPE_CRL;
				break;

			case CRYPT_CERTTYPE_CERTCHAIN:
				/* If it's a certificate chain then we have to read past the
				   PKCS #7/CMS wrapper to find the chain */
				status = processCertWrapper( stream, objectOffset, 
											 objectLength, offset );
				if( cryptStatusError( status ) )
					return( status );
				/* Fall through */

			default:
				*objectType = formatHint;
			}
										
		return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
		}

	/* First we check for the easy ones, CMS attributes, which begin with a 
	   [0] IMPLICIT SET */
	if( isContextTagged )
		{
		*objectType = CRYPT_CERTTYPE_CMS_ATTRIBUTES;
		return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
		}

	/* See what's next */
	status = tag = peekTag( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a PKCS #7 certificate chain there'll be an object identifier 
	   present, check the wrapper */
	if( tag == BER_OBJECT_IDENTIFIER )
		{
		status = processCertWrapper( stream, objectOffset, objectLength,
									 offset );
		if( cryptStatusError( status ) )
			return( status );
		*objectType = CRYPT_CERTTYPE_CERTCHAIN;
		return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
		}

	/* If it's a PKCS #12 mess there'll be a version number, 3, present */
	if( tag == BER_INTEGER )
		{
		long value;

		/* Strip off the amazing number of layers of bloat that PKCS #12 
		   lards a certificate with.  There are any number of different
		   interpretations of how to store certificates in a PKCS #12 file, 
		   the following is the one that (eventually) ends up in a 
		   certificate that we can read */
		status = readShortInteger( stream, &value );
		if( cryptStatusError( status ) )
			return( status );
		if( value != 3 )
			return( CRYPT_ERROR_BADDATA );
		readSequence( stream, NULL );
		readFixedOID( stream, OID_CMS_DATA, sizeofOID( OID_CMS_DATA ) );
		readConstructed( stream, NULL, 0 );
		readOctetStringHole( stream, NULL, 8, DEFAULT_TAG );
		readSequence( stream, NULL );
		readSequence( stream, NULL );
		readFixedOID( stream, OID_CMS_DATA, sizeofOID( OID_CMS_DATA ) );
		readConstructed( stream, NULL, 0 );
		readOctetStringHole( stream, NULL, 8, DEFAULT_TAG );
		readSequence( stream, NULL );
		readSequence( stream, NULL );
		readFixedOID( stream, 
					  MKOID( "\x06\x0B\x2A\x86\x48\x86\xF7\x0D\x01\x0C\x0A\x01\x03" ),
					  13 );
		readConstructed( stream, NULL, 0 );
		readSequence( stream, NULL );
		readFixedOID( stream, 
					  MKOID( "\x06\x0A\x2A\x86\x48\x86\xF7\x0D\x01\x09\x16\x01" ),
					  12 );
		readConstructed( stream, NULL, 0 );
		status = readOctetStringHole( stream, &length, 8, DEFAULT_TAG );
		if( cryptStatusError( status ) )
			return( status );
		offset = stell( stream );	/* Certificate start */
		readSequence( stream, NULL );
		status = readSequence( stream, NULL );
		if( cryptStatusError( status ) )
			return( status );

		/* We've finally reached the certificate, record its offset and 
		   length */
		*objectOffset = offset;
		*objectLength = length;
		*objectType = CRYPT_CERTTYPE_CERTIFICATE;
		return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
		}

	/* Read the inner sequence */
	if( isLongData )
		{
		long longLength;

		status = readLongSequence( stream, &longLength );
		if( cryptStatusOK( status ) && longLength == CRYPT_UNUSED )
			{
			/* If it's an (invalid) indefinite-length encoding then we can't 
			   do anything with it */
			status = CRYPT_ERROR_BADDATA;
			}
		}
	else
		status = readSequence( stream, NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Skip optional tagged fields and the INTEGER value */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 0 ) )
		status = readUniversal( stream );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 1 ) )
		status = readUniversal( stream );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 2 ) )
		status = readUniversal( stream );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == BER_INTEGER )
		status = readUniversal( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* If we've hit a GeneralizedTime it's an OCSP response, if we've hit 
	   a SET it's PKI user information, and if we've hit a [0] or [1] 
	   primitive tag (implicitly tagged INTEGER) or [3]...[9] it's a CRMF 
	   revocation request */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == BER_TIME_GENERALIZED )
		{
		*objectType = CRYPT_CERTTYPE_OCSP_RESPONSE;
		return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
		}
	if( cryptStatusError( status ) )
		return( status );	/* Residual error from peekTag() */

	/* Read the next SEQUENCE.  If it's followed by an OID it's the 
	   AlgorithmIdentifier in a certificate or CRL, if it's followed by a 
	   SET it's the Name in a certificate request, if it's followed by a
	   [1] constructed tag it's an attribute certificate, and if it's 
	   followed by a tag in the range [0]...[9] it's a horror from CRMF */
	status = readSequence( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length <= 0 || length > MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == BER_OBJECT_IDENTIFIER )
		{
		/* Skip the AlgorithmIdentifier data and the following Name.  For a
		   certificate we now have a SEQUENCE (from the Validity), for a CRL 
		   a UTCTime or GeneralizedTime */
		sSkip( stream, length, MAX_INTLENGTH_SHORT );
		readUniversal( stream );
		tag = readTag( stream );
		if( cryptStatusError( tag ) )
			return( tag );
		if( tag == BER_SEQUENCE )
			{
			*objectType = CRYPT_CERTTYPE_CERTIFICATE;
			return( isLongData ? CRYPT_ERROR_OVERFLOW : CRYPT_OK );
			}
		if( tag == BER_TIME_UTC || tag == BER_TIME_GENERALIZED )
			{
			*objectType = CRYPT_CERTTYPE_CRL;
			return( CRYPT_OK );
			}
		return( CRYPT_ERROR_BADDATA );
		}
	if( cryptStatusError( status ) )
		return( status );	/* Residual error from peekTag() */
	if( isLongData )
		{
		/* Beyond this point we shouldn't be seeing long-length objects */
		return( CRYPT_ERROR_OVERFLOW );
		}
	if( tag == MAKE_CTAG( 1 ) )
		{
		*objectType = CRYPT_CERTTYPE_ATTRIBUTE_CERT;
		return( CRYPT_OK );
		}
	if( tag == MAKE_CTAG_PRIMITIVE( 0 ) || \
		tag == MAKE_CTAG_PRIMITIVE( 1 ) || \
		( tag >= MAKE_CTAG( 2 ) && tag <= MAKE_CTAG( 9 ) ) )
		{
		*objectType = CRYPT_CERTTYPE_REQUEST_CERT;
		return( CRYPT_OK );
		}
	if( tag == BER_SET )
		{
		sSkip( stream, length, MAX_INTLENGTH_SHORT );
		readSequence( stream, NULL );
		tag = readTag( stream );
		if( cryptStatusError( tag ) )
			return( tag );
		if( tag == BER_OBJECT_IDENTIFIER )
			{
			*objectType = CRYPT_CERTTYPE_ATTRIBUTE_CERT;
			return( CRYPT_OK );
			}
		if( tag == BER_SEQUENCE )
			{
			*objectType = CRYPT_CERTTYPE_CERTREQUEST;
			return( CRYPT_OK );
			}
		return( CRYPT_ERROR_BADDATA );
		}

	/* It's nothing identifiable */
	return( CRYPT_ERROR_BADDATA );
	}
#endif /* USE_CERTIFICATES */
