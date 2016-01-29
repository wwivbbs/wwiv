/****************************************************************************
*																			*
*					  Certificate Chain Management Routines					*
*						Copyright Peter Gutmann 1996-2013					*
*																			*
****************************************************************************/

#if defined( INC_ALL ) 
  #include "cert.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
#else
  #include "cert/cert.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
#endif /* Compiler-specific includes */

/* When matching by subjectKeyIdentifier we don't use values less than 40
   bits because some CAs use monotonically increasing sequence numbers for 
   the sKID, which can clash with the same values when used by other CAs */

#define MIN_SKID_SIZE	5

/* A structure for storing pointers to parent and child (issuer and subject)
   names, key identifiers, and serial numbers (for finding a certificate by 
   issuerAndSerialNumber) and one for storing pointers to chaining 
   information */

typedef struct {
	BUFFER_FIXED( issuerDNsize ) \
	const void *issuerDN;
	BUFFER_FIXED( subjectDNsize ) \
	const void *subjectDN;
	int issuerDNsize, subjectDNsize;
	BUFFER_FIXED( serialNumberSize ) \
	const void *serialNumber;
	int serialNumberSize;
	BUFFER_OPT_FIXED( subjectKeyIDsize ) \
	const void *subjectKeyIdentifier;
	int subjectKeyIDsize;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	BUFFER_OPT_FIXED( issuerKeyIDsize ) \
	const void *issuerKeyIdentifier;
	int issuerKeyIDsize;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	const ATTRIBUTE_PTR *attributes;
	} CHAIN_INFO;

typedef struct {
	BUFFER_FIXED( DNsize ) \
	const void *DN;
	int DNsize;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	BUFFER_FIXED( keyIDsize ) \
	const void *keyIdentifier;
	int keyIDsize;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	} CHAINING_INFO;

/* Match an identifier in a chain information structure against a chaining 
   information structure */

#define matchDN( chainingInfo, chainInfo, type ) \
		( ( chainingInfo )->DNsize > 0 && \
		  ( chainingInfo )->DNsize == ( chainInfo )->type##DNsize && \
		  !memcmp( ( chainingInfo )->DN, ( chainInfo )->type##DN, \
				   ( chainInfo )->type##DNsize ) )

#define matchKeyID( chainingInfo, chainInfo, type ) \
		( ( chainingInfo )->keyIDsize > MIN_SKID_SIZE && \
		  ( chainingInfo )->keyIDsize == ( chainInfo )->type##KeyIDsize && \
		  !memcmp( ( chainingInfo )->keyIdentifier, \
				   ( chainInfo )->type##KeyIdentifier, \
				   ( chainInfo )->type##KeyIDsize ) )

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Copy subject or issuer chaining values from the chaining information */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void getSubjectChainingInfo( OUT CHAINING_INFO *chainingInfo,
									const CHAIN_INFO *chainInfo )
	{
	assert( isWritePtr( chainingInfo, sizeof( CHAINING_INFO ) ) );
	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) ) );

	memset( chainingInfo, 0, sizeof( CHAINING_INFO ) );
	chainingInfo->DN = chainInfo->subjectDN;
	chainingInfo->DNsize = chainInfo->subjectDNsize;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	chainingInfo->keyIdentifier = chainInfo->subjectKeyIdentifier;
	chainingInfo->keyIDsize = chainInfo->subjectKeyIDsize;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	}

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void getIssuerChainingInfo( OUT CHAINING_INFO *chainingInfo,
								   const CHAIN_INFO *chainInfo )
	{
	assert( isWritePtr( chainingInfo, sizeof( CHAINING_INFO ) ) );
	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) ) );

	memset( chainingInfo, 0, sizeof( CHAINING_INFO ) );
	chainingInfo->DN = chainInfo->issuerDN;
	chainingInfo->DNsize = chainInfo->issuerDNsize;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	chainingInfo->keyIdentifier = chainInfo->issuerKeyIdentifier;
	chainingInfo->keyIDsize = chainInfo->issuerKeyIDsize;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	}

/* Determine whether a given certificate is the subject or issuer for the 
   requested certificate based on the chaining information.  We chain by 
   issuer DN if possible but if that fails we use the keyID.  This is 
   somewhat dodgy since it can lead to the situation where a certificate 
   supposedly issued by Verisign Class 1 Public Primary Certification 
   Authority is actually issued by Honest Joe's Used Cars but the standard 
   requires this as a fallback (PKIX section 4.2.1.1).
   
   There are actually two different interpretations of chaining by keyID, 
   the first says that the keyID is a non-DN identifier that can survive 
   operations such as cross-certification and re-parenting so that if a 
   straight chain by DN fails then a chain by keyID is possible as a 
   fallback option.  The second is that the keyID is a disambiguator if 
   multiple paths in a chain-by-DN scenario are present in a spaghetti PKI.  
   Since the latter is rather unlikely to occur in a standard PKCS #7/SSL 
   certificate chain (half the implementations around wouldn't be able to 
   assemble the chain any more) we use the former interpretation by default 
   but enable the latter if useStrictChaining is set.
   
   If useStrictChaining is enabled we require that the DN *and* the keyID 
   match, which (even without a spaghetti PKI being in effect) is required 
   to handle PKIX weirdness in which multiple potential issuers can be 
   present in a chain due to CA certificate renewals/reparenting.  We don't 
   do this by default because too many CAs get keyID chaining wrong, leading 
   to apparent breaks in the chain when the keyID fails to match.
   
   We don't have to worry about strict chaining for the issuer match because
   we only use it when we're walking down the chain looking for a leaf 
   certificate */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN isSubject( const CHAINING_INFO *chainingInfo,
						  const CHAIN_INFO *chainInfo,
						  const BOOLEAN useStrictChaining )
	{
	BOOLEAN dnChains = FALSE, keyIDchains = FALSE;

	assert( isReadPtr( chainingInfo, sizeof( CHAINING_INFO ) ) );
	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) ) );

	/* Check for chaining by DN and keyID */
	if( matchDN( chainingInfo, chainInfo, subject ) )
		dnChains = TRUE;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	if( matchKeyID( chainingInfo, chainInfo, subject ) )
		keyIDchains = TRUE;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	/* If we're using strict chaining both the DN and the keyID must chain */
	if( useStrictChaining )
		return( dnChains && keyIDchains );

	/* We're not using strict chaining, either can chain */
	return( dnChains || keyIDchains );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static BOOLEAN isIssuer( const CHAINING_INFO *chainingInfo,
						 const CHAIN_INFO *chainInfo )
	{
	assert( isReadPtr( chainingInfo, sizeof( CHAINING_INFO ) ) );
	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) ) );

	/* In the simplest case we chain by name.  This works for almost all
	   certificates */
	if( matchDN( chainingInfo, chainInfo, issuer ) )
		return( TRUE );

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* If that fails we chain by keyID */
	if( matchKeyID( chainingInfo, chainInfo, issuer ) )
		return( TRUE );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	return( FALSE );
	}

/* Get the location and size of certificate attribute data required for
   chaining */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int getChainingAttribute( INOUT CERT_INFO *certInfoPtr,
								 IN_ATTRIBUTE \
									const CRYPT_ATTRIBUTE_TYPE attributeType,
								 OUT_BUFFER_ALLOC_OPT( *attributeLength ) \
									const void **attributePtrPtr, 
								 OUT_DATALENGTH_Z int *attributeLength )
	{
	ATTRIBUTE_PTR *attributePtr;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isReadPtr( attributePtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( attributeLength, sizeof( int * ) ) );

	ENSURES( attributeType == CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER || \
			 attributeType == CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER );

	/* Clear return values */
	*attributePtrPtr = NULL;
	*attributeLength = 0;

	/* Find the requested attribute and return a pointer to it */
	attributePtr = findAttributeField( certInfoPtr->attributes,
									   attributeType, CRYPT_ATTRIBUTE_NONE );
	if( attributePtr == NULL )
		{
		/* If the chaining attribute data isn't present this isn't an 
		   error */
		return( CRYPT_OK );
		}
	return( getAttributeDataPtr( attributePtr, ( void ** ) attributePtrPtr, 
								 attributeLength ) );
	}

/* Free a certificate chain */

STDC_NONNULL_ARG( ( 1 ) ) \
static void freeCertChain( IN_ARRAY( certChainSize ) \
								CRYPT_CERTIFICATE *iCertChain,
						   IN_RANGE( 1, MAX_CHAINLENGTH ) \
								const int certChainSize )
	{
	int i;

	assert( isWritePtr( iCertChain, sizeof( CRYPT_CERTIFICATE ) * certChainSize ) );

	REQUIRES_V( certChainSize > 0 && certChainSize <= MAX_CHAINLENGTH );

	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		{
		/* If we ran into an error while working with a certificate in the 
		   chain then this position may be empty, in which case we skip it */
		if( iCertChain[ i ] == CRYPT_ERROR )
			continue;

		/* Clear the certificate at this position */
		krnlSendNotifier( iCertChain[ i ], IMESSAGE_DESTROY );
		iCertChain[ i ] = CRYPT_ERROR;
		}
	}

/* Convert a certificate object into a certificate-chain object.  This is 
   necessary because at the point when we're importing the certificates
   to build the chain we don't know which one is the leaf so we have to
   import them all as certificates, and once that's done we can't change
   the object type any more.  Because of this the only way to change
   the object type is to re-create it as a certificate-chain object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int convertCertToChain( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertChain,
							   IN_HANDLE CRYPT_CERTIFICATE iCertificate,
							   const BOOLEAN isDataOnlyCert )
	{
	MESSAGE_CREATEOBJECT_INFO createInfo;
	DYNBUF certDB;
	int status;

	assert( isWritePtr( iCertChain, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( isHandleRangeValid( iCertificate ) );

	/* Clear return value */
	*iCertChain = CRYPT_ERROR;

	/* Export the leaf certificate and re-import it to create the required 
	   certificate-chain object, specifying KEYMGMT_FLAG_CERT_AS_CERTCHAIN 
	   to ensure that the certificate is imported as a single-entry
	   certificate chain */
	status = dynCreateCert( &certDB, iCertificate, 
							CRYPT_CERTFORMAT_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );
	setMessageCreateObjectIndirectInfoEx( &createInfo, 
							dynData( certDB ), dynLength( certDB ), 
							CRYPT_CERTTYPE_CERTIFICATE, isDataOnlyCert ? \
								KEYMGMT_FLAG_DATAONLY_CERT | \
								KEYMGMT_FLAG_CERT_AS_CERTCHAIN : \
								KEYMGMT_FLAG_CERT_AS_CERTCHAIN );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
							  &createInfo, OBJECT_TYPE_CERTIFICATE );
	dynDestroy( &certDB );
	if( cryptStatusError( status ) )
		return( status );

	/* We've now got the same certificate object as before, but with a 
	   subtype of certificate chain rather than certificate, replace the 
	   existing object with the new one */
	krnlSendNotifier( iCertificate, IMESSAGE_DESTROY );
	*iCertChain = createInfo.cryptHandle;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Build a Certificate Chain						*
*																			*
****************************************************************************/

/* Build up the parent/child pointers for a certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int buildChainInfo( OUT_ARRAY( certChainSize ) CHAIN_INFO *chainInfo,
						   IN_ARRAY( certChainSize ) \
								const CRYPT_CERTIFICATE *iCertChain,
						   IN_RANGE( 1, MAX_CHAINLENGTH ) \
								const int certChainSize )
	{
	int i;

	assert( isWritePtr( chainInfo, sizeof( CHAIN_INFO ) * certChainSize ) );
	assert( isReadPtr( iCertChain, sizeof( CRYPT_CERTIFICATE ) * certChainSize ) );

	REQUIRES( certChainSize > 0 && certChainSize <= MAX_CHAINLENGTH );

	/* Clear return value */
	memset( chainInfo, 0, sizeof( CHAIN_INFO ) * certChainSize );

	/* Extract the subject and issuer DNs and key identifiers from each
	   certificate.  Maintaining an external pointer into the internal
	   structure is safe since the objects are reference-counted and won't be
	   destroyed until the encapsulating certificate is destroyed */
	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		{
		CERT_INFO *certChainPtr;
		int status;

		status = krnlAcquireObject( iCertChain[ i ], OBJECT_TYPE_CERTIFICATE, 
									( void ** ) &certChainPtr, 
									CRYPT_ERROR_SIGNALLED );
		if( cryptStatusError( status ) )
			return( status );
		chainInfo[ i ].subjectDN = certChainPtr->subjectDNptr;
		chainInfo[ i ].subjectDNsize = certChainPtr->subjectDNsize;
		chainInfo[ i ].issuerDN = certChainPtr->issuerDNptr;
		chainInfo[ i ].issuerDNsize = certChainPtr->issuerDNsize;
		chainInfo[ i ].serialNumber = certChainPtr->cCertCert->serialNumber;
		chainInfo[ i ].serialNumberSize = certChainPtr->cCertCert->serialNumberLength;
		chainInfo[ i ].attributes = certChainPtr->attributes;
		status = getChainingAttribute( certChainPtr, 
									   CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER,
									   &chainInfo[ i ].subjectKeyIdentifier,
									   &chainInfo[ i ].subjectKeyIDsize );
		if( cryptStatusError( status ) )
			{
			krnlReleaseObject( certChainPtr->objectHandle );
			return( status );
			}
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
		status = getChainingAttribute( certChainPtr, 
									   CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER,
									   &chainInfo[ i ].issuerKeyIdentifier,
									   &chainInfo[ i ].issuerKeyIDsize );
		if( cryptStatusError( status ) )
			{
			krnlReleaseObject( certChainPtr->objectHandle );
			return( status );
			}
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
		krnlReleaseObject( certChainPtr->objectHandle );
		}
	ENSURES( i < MAX_CHAINLENGTH );

	return( CRYPT_OK );
	}

/* Find the leaf node in a (possibly unordered) certificate chain by walking 
   down the chain as far as possible.  The strategy we use is to pick an 
   initial certificate (which is often the leaf certificate anyway) and keep 
   looking for certificates that it (or its successors) have issued until we 
   reach the end of the chain.  Returns the position of the leaf node in the 
   chain.

   In addition to finding just the (nominal) leaf certificate, the caller 
   can also specify a preferred certificate through its usage type, useful 
   when the "chain" is actually an arbitrary certificate collection 
   containing multiple leaf certificates.  This isn't quite as simple as it 
   seems because for usage = X alongside the standard case of a single leaf 
   certificate with usage = X we can also have a leaf certificate with usage 
   Y, two leaf certificates with usage X, or a CA certificate (but no leaf 
   certificate) with usage X.  To deal with this we use the caller-specified 
   usage as a preferred-certificate hint rather than an absolute selection 
   criterion, with the different cases illustrated as follows:

		  X		- Return this one.
	...../
		 \
		  Y

		  X
	...../
		 \
		  X		- Return this one (because of the way the algorithm is 
				  implemented, not by any specific policy decision).

		  Y
	...../
		 \
		  Y		- Return this one, same as above.

				 Y
	... CA+X .../
				\
				 Y	- Return this one.

   In other words we always try and return a leaf certificate if one exists, 
   and make the selection based on usage if possible.

   Finally, the usage is treated as an absolute match (all usages must 
   match) rather than a selective match (one usage of potentially several 
   must match).  In other words if the caller specifies usage X+Y then the 
   certificate must have both of those usages available */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int findLeafNode( IN_ARRAY( certChainSize ) const CHAIN_INFO *chainInfo,
						 IN_RANGE( 1, MAX_CHAINLENGTH ) const int certChainSize,
						 IN_FLAGS_Z( KEYMGMT ) const int options )
	{
	const int requestedUsage = \
		( options == ( KEYMGMT_FLAG_USAGE_CRYPT | KEYMGMT_FLAG_USAGE_SIGN ) ) ? \
			CRYPT_KEYUSAGE_KEYENCIPHERMENT | CRYPT_KEYUSAGE_DIGITALSIGNATURE : \
		( options == KEYMGMT_FLAG_USAGE_CRYPT ) ? \
			CRYPT_KEYUSAGE_KEYENCIPHERMENT : \
		( options == KEYMGMT_FLAG_USAGE_SIGN ) ? \
			CRYPT_KEYUSAGE_DIGITALSIGNATURE : 0;
	int bestMatch = CRYPT_ERROR, optionalMatch, currentCertPos;

	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) * certChainSize ) );

	REQUIRES( certChainSize > 0 && certChainSize <= MAX_CHAINLENGTH );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_MASK_USAGEOPTIONS ) == 0 );

	/* If we don't find a better match, default to using the first 
	   certificate.  This is required in order to handle self-signed 
	   certificates, which are their own issuer and so never get passed to
	   the match-checking code.  Note that this can't handle sufficiently
	   perverse "chains" like one that contains two self-signed roots
	   with one having a signature usage and the other having an encryption
	   usage and the caller is asking for KEYMGMT_FLAG_USAGE_CRYPT, but
	   anything that weird shouldn't be expected to work properly in the
	   first place */
	optionalMatch = 0;

	/* Walk down the chain from the currently selected certificate checking 
	   for certificates issued by it until we can't go any further, with
	   optional filtering by certificate usage type.  Note that this 
	   algorithm handles chains with PKIX path-kludge certificates as well 
	   as normal ones since it marks a certificate as used once it processes 
	   it for the first time, avoiding potential endless loops on 
	   subject == issuer path-kludge certificates */
	for( currentCertPos = 0; 
		 currentCertPos < certChainSize && currentCertPos < MAX_CHAINLENGTH; 
		 currentCertPos++ )
		{
		CHAINING_INFO currentCertInfo;
		ATTRIBUTE_PTR *attributePtr;
		BOOLEAN certIsIssuer = FALSE;
		int childCertPos, value, status;

		getSubjectChainingInfo( &currentCertInfo, 
								&chainInfo[ currentCertPos ] );

		/* Try and find a certificate issued by the current certificate */
		for( childCertPos = 0; 
			 childCertPos < certChainSize && childCertPos < MAX_CHAINLENGTH; 
			 childCertPos++ )
			{
			/* If there's another certificate below the current one in the 
			   chain, mark the current one as used and move on to the next 
			   one */
			if( isIssuer( &currentCertInfo, &chainInfo[ childCertPos ] ) )
				{
				certIsIssuer = TRUE;
				break;
				}
			}
		ENSURES( childCertPos < MAX_CHAINLENGTH );

		/* If the current certificate isn't a leaf certificate, continue to 
		   the next one */
		if( certIsIssuer )
			continue;

		/* If we're not selecting certificates based on a specific usage 
		   type, we're done */
		if( requestedUsage == KEYMGMT_FLAG_NONE )
			{
			bestMatch = optionalMatch = currentCertPos;
			continue;
			}

		/* Get the certificate's key usage */
		attributePtr = findAttributeField( chainInfo[ currentCertPos ].attributes,
										   CRYPT_CERTINFO_KEYUSAGE, 
										   CRYPT_ATTRIBUTE_NONE );
		if( attributePtr == NULL )
			{
			/* There's no explicit key usage present, this certificate 
			   should be OK unless we find a more specific match */
			bestMatch = optionalMatch = currentCertPos;
			continue;
			}
		status = getAttributeDataValue( attributePtr, &value );
		if( cryptStatusError( status ) )
			continue;
		if( ( value & requestedUsage ) == requestedUsage )
			bestMatch = currentCertPos;
		else
			optionalMatch = currentCertPos;
		}
	ENSURES( currentCertPos < MAX_CHAINLENGTH );

	return( ( bestMatch != CRYPT_ERROR ) ? bestMatch : optionalMatch );
	}

/* Find a leaf node as identified by issuerAndSerialNumber,
   subjectKeyIdentifier, or certificate usage.  Returns the position of the 
   leaf node in the chain */

CHECK_RETVAL_RANGE( 0, MAX_CHAINLENGTH ) STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int findIdentifiedLeafNode( IN_ARRAY( certChainSize ) \
										const CHAIN_INFO *chainInfo,
								   IN_RANGE( 1, MAX_CHAINLENGTH ) \
										const int certChainSize,
								   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
								   IN_BUFFER( keyIDlength ) const void *keyID, 
								   IN_LENGTH_KEYID const int keyIDlength )
	{
	STREAM stream;
	void *serialNumber DUMMY_INIT_PTR, *issuerDNptr DUMMY_INIT_PTR;
	int issuerDNsize DUMMY_INIT, serialNumberSize;
	int i, status;

	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) * certChainSize ) );
	assert( isReadPtr( keyID, keyIDlength ) );

	REQUIRES( certChainSize > 0 && certChainSize <= MAX_CHAINLENGTH );
	REQUIRES( keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER );
	REQUIRES( keyID != NULL && \
			  keyIDlength >= MIN_SKID_SIZE && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );

	/* If it's a subjectKeyIdentifier walk down the chain looking for a
	   match */
	if( keyIDtype == CRYPT_IKEYID_KEYID )
		{
		for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
			{
			if( chainInfo[ i ].subjectKeyIDsize >= MIN_SKID_SIZE && \
				chainInfo[ i ].subjectKeyIDsize == keyIDlength && \
				!memcmp( chainInfo[ i ].subjectKeyIdentifier, keyID,
						 keyIDlength ) )
				return( i );
			}
		ENSURES( i < MAX_CHAINLENGTH );
		return( CRYPT_ERROR_NOTFOUND );
		}

	/* It's an issuerAndSerialNumber, extract the issuer DN and serial 
	   number */
	sMemConnect( &stream, keyID, keyIDlength );
	status = readSequence( &stream, NULL );
	if( cryptStatusOK( status ) )
		status = getStreamObjectLength( &stream, &issuerDNsize );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( &stream, &issuerDNptr, issuerDNsize );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( CRYPT_ERROR_NOTFOUND );
		}
	sSkip( &stream, issuerDNsize, MAX_INTLENGTH_SHORT );	/* Issuer DN */
	status = readGenericHole( &stream, &serialNumberSize, 1, BER_INTEGER );
	if( cryptStatusOK( status ) )				/* Serial number */
		status = sMemGetDataBlock( &stream, &serialNumber, 
								   serialNumberSize );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_NOTFOUND );
	ANALYSER_HINT( issuerDNptr != NULL );
	ANALYSER_HINT( serialNumber != NULL );

	/* Walk down the chain looking for the one identified by the 
	   issuerAndSerialNumber */
	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		{
		if( chainInfo[ i ].issuerDNsize > 0 && \
			chainInfo[ i ].issuerDNsize == issuerDNsize && \
			!memcmp( chainInfo[ i ].issuerDN, issuerDNptr,
					 issuerDNsize ) && \
			compareSerialNumber( chainInfo[ i ].serialNumber, 
								 chainInfo[ i ].serialNumberSize,
								 serialNumber, serialNumberSize ) )
			return( i );
		}
	ENSURES( i < MAX_CHAINLENGTH );

	return( CRYPT_ERROR_NOTFOUND );
	}

/* Sort the issuer certificates in a certificate chain, discarding any 
   unnecessary certificates.  If we're canonicalising an existing chain then 
   the start point in the chain is given by leafCert and the -1th certificate 
   is the end user certificate and isn't part of the ordering process.  If 
   we're building a new chain from an arbitrary set of certificates then the 
   start point is given by the chaining information for the leaf certificate.

   The canonicalisation of the chain can be handled in one of two ways, the 
   logical way and the PKIX way.  The latter allows apparently self-signed
   certificates in the middle of a chain due to certificate 
   renewals/reparenting, which completely breaks the standard certificate 
   convention that a self-signed certificate is a root CA.  This means that 
   without special handling the chain will terminate at a certificate that 
   appears to be (but isn't) the CA root certificate.  A sample chain of 
   this form (in this case involving an oldWithNew certificate) is as 
   follows:

	Issuer		Subject		Key/sKID	Sig/aKID
	------		-------		--------	----------
	Root		CA			ca_new		root
	CA			CA			ca_old		ca_new
	CA			EE			ee			ca_old

   In order to handle these chains we need to match by both DN *and* keyID,
   however since so many CAs get keyIDs wrong enabling this by default 
   would break many certificate chains.  To handle this we only enable the 
   extra-match behaviour if the compliance level is 
   CRYPT_COMPLIANCELEVEL_PKIX_FULL, for which people should be expecting all 
   sorts of bizarre behaviour anyway */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int sortCertChain( INOUT_ARRAY( certChainSize ) CRYPT_CERTIFICATE *iCertChain,
						  OUT_RANGE( 0, MAX_CHAINLENGTH ) int *orderedCertChainSize,
						  IN_ARRAY( certChainSize ) const CHAIN_INFO *chainInfo,
						  IN_RANGE( 1, MAX_CHAINLENGTH ) const int certChainSize,
						  IN_OPT const CHAINING_INFO *chainingInfo,
						  IN_RANGE( CRYPT_UNUSED, MAX_CHAINLENGTH ) \
							const int leafCertEntry,
						  IN_HANDLE_OPT const CRYPT_CERTIFICATE leafCert,
						  const BOOLEAN useStrictChaining )
	{
	CRYPT_CERTIFICATE orderedChain[ MAX_CHAINLENGTH + 8 ];
	CHAINING_INFO localChainingInfo;
	BOOLEAN chainInfoValid[ MAX_CHAINLENGTH + 8 ], moreMatches;
	const int maxMatchLevel = useStrictChaining ? 1 : 0;
	int orderedChainIndex, iterationCount, i;

	assert( isWritePtr( iCertChain, sizeof( CRYPT_CERTIFICATE ) * certChainSize ) );
	assert( isReadPtr( chainInfo, sizeof( CHAIN_INFO ) * certChainSize ) );
	assert( ( isHandleRangeValid( leafCert ) && chainingInfo == NULL && \
			  leafCertEntry == CRYPT_UNUSED ) || \
			( leafCert == CRYPT_UNUSED && \
			  isReadPtr( chainingInfo, sizeof( CHAINING_INFO ) ) && \
			  leafCertEntry >= 0 && leafCertEntry <= certChainSize ) );

	REQUIRES( certChainSize > 0 && certChainSize <= MAX_CHAINLENGTH );
	REQUIRES( ( isHandleRangeValid( leafCert ) && chainingInfo == NULL && \
				leafCertEntry == CRYPT_UNUSED ) || \
			  ( leafCert == CRYPT_UNUSED && chainingInfo != NULL && \
				leafCertEntry >= 0 && leafCertEntry <= certChainSize ) );

	/* Clear return value */
	*orderedCertChainSize = 0;

	/* Initially all chain entries except the one for the leaf certificate 
	   (defined either implicitly as the zero-th entry for leafCert or
	   explicitly as leafCertEntry) are valid */
	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		chainInfoValid[ i ] = TRUE;
	ENSURES( i < MAX_CHAINLENGTH );

	/* If we're canonicalising an existing chain there's a predefined chain
	   start that we copy over and prepare to look for the next certificate 
	   up the chain */
	if( leafCert != CRYPT_UNUSED )
		{
		getIssuerChainingInfo( &localChainingInfo, &chainInfo[ 0 ] );
		orderedChain[ 0 ] = leafCert;
		chainInfoValid[ 0 ] = FALSE;
		orderedChainIndex = 1;
		}
	else
		{
		/* We're building a new chain, the caller has supplied the chaining
		   information */
		memcpy( &localChainingInfo, chainingInfo, sizeof( CHAINING_INFO ) );
		chainInfoValid[ leafCertEntry ] = FALSE;
		orderedChainIndex = 0;
		}

	/* Build an ordered chain of certificates from the leaf to the root */
	for( moreMatches = TRUE, iterationCount = 0;
		 moreMatches && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		int matchLevel;

		/* We don't have a match until we discover otherwise */
		moreMatches = FALSE;

		/* Find the certificate with the current issuer as its subject.  If 
		   we're using strict chaining we first try a strict match 
		   (matchLevel = TRUE), if that fails we fall back to a standard 
		   match (matchLevel = FALSE).  This is required to handle the
		   significant number of CAs that don't get chaining by keyID 
		   right */
		for( matchLevel = maxMatchLevel; \
			 !moreMatches && matchLevel >= 0; matchLevel-- )
			{
			for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
				{
				/* If this isn't the issuer, continue */
				if( !chainInfoValid[ i ] || \
					!isSubject( &localChainingInfo, &chainInfo[ i ], 
								matchLevel ) )
					continue;

				/* We've found the issuer, move the certificate to the 
				   ordered chain and prepare to find its issuer */
				orderedChain[ orderedChainIndex++ ] = iCertChain[ i ];
				getIssuerChainingInfo( &localChainingInfo, &chainInfo[ i ] );
				chainInfoValid[ i ] = FALSE;
				moreMatches = TRUE;	/* Exit outer loop as well */
				break;
				}
			ENSURES( i < MAX_CHAINLENGTH );
			}
		ENSURES( orderedChainIndex <= certChainSize && \
				 orderedChainIndex < MAX_CHAINLENGTH );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* If there are any certificates left they're not needed for anything 
	   so we can free the resources */
	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		{
		if( chainInfoValid[ i ] )
			krnlSendNotifier( iCertChain[ i ], IMESSAGE_DECREFCOUNT );
		}
	ENSURES( i < MAX_CHAINLENGTH );

	/* Replace the existing chain with the ordered version */
	for( i = 0; i < certChainSize && i < MAX_CHAINLENGTH; i++ )
		iCertChain[ i ] = CRYPT_ERROR;
	if( orderedChainIndex > 0 )
		{
		memcpy( iCertChain, orderedChain,
				sizeof( CRYPT_CERTIFICATE ) * orderedChainIndex );
		}
	*orderedCertChainSize = orderedChainIndex;

	return( CRYPT_OK );
	}

/* Read a collection of certificates in a certificate chain into a 
   certificate object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int buildCertChain( OUT_HANDLE_OPT CRYPT_CERTIFICATE *iLeafCert, 
						   INOUT_ARRAY( certChainEnd ) CRYPT_CERTIFICATE *iCertChain, 
						   IN_RANGE( 1, MAX_CHAINLENGTH ) const int certChainEnd,
						   IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
						   IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
						   IN_LENGTH_KEYID_Z const int keyIDlength,
						   IN_FLAGS( KEYMGMT ) const int options )
	{
	CHAIN_INFO chainInfo[ MAX_CHAINLENGTH + 8 ];
	CERT_INFO *certChainPtr;
	CHAINING_INFO chainingInfo;
	const int keyUsageOptions = options & KEYMGMT_MASK_USAGEOPTIONS;
	int leafNodePos, newCertChainEnd, complianceLevel, status;

	assert( isWritePtr( iLeafCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isWritePtr( iCertChain, \
			sizeof( CRYPT_CERTIFICATE ) * certChainEnd ) );
	assert( ( keyIDtype == CRYPT_KEYID_NONE && \
			  keyID == NULL && keyIDlength == 0 ) || \
			( ( keyIDtype == CRYPT_IKEYID_KEYID || \
				keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER ) && \
			  isReadPtr( keyID, keyIDlength ) ) );

	REQUIRES( certChainEnd > 0 && certChainEnd <= MAX_CHAINLENGTH );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				keyID == NULL && keyIDlength == 0 ) || \
			  ( ( keyIDtype == CRYPT_IKEYID_KEYID || \
				  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER ) && \
				keyID != NULL && \
				keyIDlength >= MIN_SKID_SIZE && \
				keyIDlength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && \
			  options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				keyUsageOptions == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype != CRYPT_KEYID_NONE && \
				keyUsageOptions == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype == CRYPT_KEYID_NONE && \
				keyUsageOptions != KEYMGMT_FLAG_NONE ) );

	/* Clear return value */
	*iLeafCert = CRYPT_ERROR;

	status = krnlSendMessage( iCertChain[ 0 ], IMESSAGE_GETATTRIBUTE, 
							  &complianceLevel, 
							  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
	if( cryptStatusError( status ) )
		return( status );

	/* We start with a collection of certificates in unknown order (although 
	   it's common for the first certificate to be the leaf).  First we 
	   extract the chaining information and search the chain for the leaf 
	   node */
	status = buildChainInfo( chainInfo, iCertChain, certChainEnd );
	if( cryptStatusError( status ) )
		return( status );
	if( keyID != NULL )
		{
		leafNodePos = findIdentifiedLeafNode( chainInfo, certChainEnd,
											  keyIDtype, keyID, keyIDlength );
		}
	else
		{
		leafNodePos = findLeafNode( chainInfo, certChainEnd, 
									keyUsageOptions );
		}
	if( cryptStatusError( leafNodePos ) )
		return( leafNodePos );
	ENSURES( leafNodePos >= 0 && leafNodePos < certChainEnd );

	/* We've got the leaf node, remember the leaf certificate */
	*iLeafCert = iCertChain[ leafNodePos ];
	getIssuerChainingInfo( &chainingInfo, &chainInfo[ leafNodePos ] );

	/* Order the remaining certificates up to the root and discard any 
	   unneeded certificates */
	status = sortCertChain( iCertChain, &newCertChainEnd, chainInfo, 
					certChainEnd, &chainingInfo, leafNodePos, CRYPT_UNUSED,
					( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_FULL ) ? \
					  TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );
	if( newCertChainEnd <= 0 )
		{
		/* There's only one certificate in the chain either due to the 
		   chain containing only a single certificate or due to all other 
		   certificates being discarded, leave it as a standalone 
		   certificate rather than turning it into a chain */
		return( CRYPT_OK );
		}

#ifdef USE_CERTLEVEL_PKIX_FULL
	/* Walk up the chain re-setting the pseudo-selfsigned flag on any
	   chain-internal path-kludge certificates if necessary.  This means 
	   that if the chain contains n certificates we reset the flag on 
	   certificates 0...n-1.  This is required when there's a re-issued 
	   certificate kludged into the middle of the path to connect a new CA 
	   signing key with a certificate signed with the old key.  Note that 
	   this can't detect the case where the first certificate in the chain 
	   is a path kludge certificate with further certificates held 
	   externally, e.g. in the trusted certificate store, since it appears 
	   as a self-signed CA root certificate */
	if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_FULL )
		{
		int i;

		for( i = 0; i < newCertChainEnd - 1 && i < MAX_CHAINLENGTH; i++ )
			{
			CERT_INFO *certInfoPtr;
			int value;

			/* Check whether this is a self-signed certificate */
			status = krnlSendMessage( iCertChain[ i ], IMESSAGE_GETATTRIBUTE,
									  &value, CRYPT_CERTINFO_SELFSIGNED );
			if( cryptStatusError( status ) || !value )
				continue;

			/* Convert the self-signed flag into the pseudo self-signed/path
			   kludge flag */
			status = krnlAcquireObject( iCertChain[ i ], OBJECT_TYPE_CERTIFICATE, 
										( void ** ) &certInfoPtr, 
										CRYPT_ERROR_SIGNALLED );
			if( cryptStatusError( status ) )
				continue;
			certInfoPtr->flags &= ~CERT_FLAG_SELFSIGNED;
			certInfoPtr->flags |= CERT_FLAG_PATHKLUDGE;
			krnlReleaseObject( certInfoPtr->objectHandle );
			}
		ENSURES( i < MAX_CHAINLENGTH );
		}
#endif /* USE_CERTLEVEL_PKIX_FULL */

	/* Since the objects that make up the chain were imported as individual 
	   certificates because at the time of import we didn't know which one 
	   was the leaf, we have to convert the leaf's object-type from 
	   certificate to certificate chain */
	status = convertCertToChain( iLeafCert, *iLeafCert,
								 ( options & KEYMGMT_FLAG_DATAONLY_CERT ) ? \
								   TRUE : FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Finally, we've got the leaf certificate and a chain up to the root.  
	   Make the leaf a certificate-chain type and copy in the chain */
	status = krnlAcquireObject( *iLeafCert, OBJECT_TYPE_CERTIFICATE, 
								( void ** ) &certChainPtr, 
								CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( status ) )
		return( status );
	memcpy( certChainPtr->cCertCert->chain, iCertChain,
			sizeof( CRYPT_CERTIFICATE ) * newCertChainEnd );
	certChainPtr->cCertCert->chainEnd = newCertChainEnd;
	certChainPtr->type = CRYPT_CERTTYPE_CERTCHAIN;
	krnlReleaseObject( certChainPtr->objectHandle );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Copy a Certificate Chain						*
*																			*
****************************************************************************/

/* Determine whether a certificate is present in a certificate collection 
   based on its fingerprint */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isCertPresent( INOUT_ARRAY( certChainLen ) \
									BYTE certChainHashes[][ CRYPT_MAX_HASHSIZE + 8 ],
							  IN_RANGE( 0, MAX_CHAINLENGTH ) const int certChainLen, 
							  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert )
	{
	MESSAGE_DATA msgData;
	int i, status;

	assert( isWritePtr( certChainHashes, \
						MAX_CHAINLENGTH * CRYPT_MAX_HASHSIZE ) );

	REQUIRES_B( certChainLen >= 0 && certChainLen <= MAX_CHAINLENGTH );
				/* Apparent length may be zero for a chain of size 1 since
				   the leaf certificate has the effective index value -1 */
	REQUIRES_B( isHandleRangeValid( iCryptCert ) );

	/* Get the fingerprint of the (potential) next certificate in the 
	   collection.  This leaves it at the end of the existing collection of 
	   hashes so that if the certificate is then added to the chain its hash 
	   will also be present */
	setMessageData( &msgData, certChainHashes[ certChainLen ], 
					CRYPT_MAX_HASHSIZE );
	status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE_S,
							  &msgData, CRYPT_CERTINFO_FINGERPRINT_SHA1 );
	if( cryptStatusError( status ) )
		return( FALSE );

	/* Make sure that it isn't already present in the collection */
	for( i = 0; i < certChainLen && i < MAX_CHAINLENGTH; i++ )
		{
		if( !memcmp( certChainHashes[ i ], 
					 certChainHashes[ certChainLen ], msgData.length ) )
			return( TRUE );
		}
	return( FALSE );
	}

/* Copy a certificate chain into a certificate object and canonicalise the 
   chain by ordering the certificates from the leaf certificate up to the 
   root.  This function is used when signing a certificate with a certificate  
   chain and takes as input ( oldCert, oldCert.chain[ ... ] ) and produces 
   as output ( newCert, chain[ oldCert, oldCert.chain[ ... ] ], i.e. the 
   chain for the new certificate contains the old certificate and its 
   attached certificate chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyCertChain( INOUT CERT_INFO *certInfoPtr, 
				   IN_HANDLE const CRYPT_HANDLE certChain,
				   const BOOLEAN isCertCollection )
	{
	CRYPT_CERTIFICATE iChainCert;
	CERT_INFO *chainCertInfoPtr;
	CERT_CERT_INFO *destCertChainInfo = certInfoPtr->cCertCert;
	CERT_CERT_INFO *srcCertChainInfo;
	CHAIN_INFO chainInfo[ MAX_CHAINLENGTH + 8 ];
	BYTE certChainHashes[ MAX_CHAINLENGTH + 1 + 8 ][ CRYPT_MAX_HASHSIZE + 8 ];
	const int oldChainEnd = destCertChainInfo->chainEnd;
	int i, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( isHandleRangeValid( certChain ) );

	status = krnlSendMessage( certChain, IMESSAGE_GETDEPENDENT, &iChainCert, 
							  OBJECT_TYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( status );

	/* If we're building a certificate collection all that we need to ensure 
	   is non-duplicate certificates rather than a strict chain.  To handle 
	   duplicate checking we build a list of the fingerprints for each 
	   certificate in the chain */
	if( isCertCollection )
		{
		for( i = 0; i < destCertChainInfo->chainEnd && i < MAX_CHAINLENGTH; i++ )
			{
			MESSAGE_DATA msgData;

			setMessageData( &msgData, certChainHashes[ i ], 
							CRYPT_MAX_HASHSIZE );
			status = krnlSendMessage( destCertChainInfo->chain[ i ], 
									  IMESSAGE_GETATTRIBUTE_S, &msgData, 
									  CRYPT_CERTINFO_FINGERPRINT_SHA1 );
			if( cryptStatusError( status ) )
				return( status );
			}
		ENSURES( i < MAX_CHAINLENGTH );
		}

	/* Extract the base certificate from the chain and copy it over (the
	   isCertPresent() check also sets up the hash for the new certificate 
	   in the certChainHashes array) */
	status = krnlAcquireObject( iChainCert, OBJECT_TYPE_CERTIFICATE, 
								( void ** ) &chainCertInfoPtr, 
								CRYPT_ERROR_SIGNALLED );
	if( cryptStatusError( status ) )
		return( status );
	srcCertChainInfo = chainCertInfoPtr->cCertCert;
	if( !isCertCollection || \
		!isCertPresent( certChainHashes, destCertChainInfo->chainEnd, \
						iChainCert ) )
		{
		if( destCertChainInfo->chainEnd >= MAX_CHAINLENGTH )
			{
			krnlReleaseObject( chainCertInfoPtr->objectHandle );
			return( CRYPT_ERROR_OVERFLOW );
			}
		krnlSendNotifier( iChainCert, IMESSAGE_INCREFCOUNT );
		destCertChainInfo->chain[ destCertChainInfo->chainEnd++ ] = iChainCert;
		}

	/* Copy the rest of the chain.  Because we're about to canonicalise it
	   (which re-orders the certificates and deletes unused ones) we copy 
	   individual certificates over rather than copying only the base 
	   certificate and relying on the chain held in that */
	for( i = 0; i < srcCertChainInfo->chainEnd && i < MAX_CHAINLENGTH; i++ )
		{
		const CRYPT_CERTIFICATE iCopyCert = srcCertChainInfo->chain[ i ];

		/* If it's an (unordered) certificate collection, make sure that 
		   there are no duplicates */
		if( isCertCollection && \
			isCertPresent( certChainHashes, destCertChainInfo->chainEnd,
							srcCertChainInfo->chain[ i ] ) )
			continue;

		/* Copy the next certificate from the source to the destination */
		if( destCertChainInfo->chainEnd >= MAX_CHAINLENGTH )
			{
			status = CRYPT_ERROR_OVERFLOW;
			break;
			}
		krnlSendNotifier( iCopyCert, IMESSAGE_INCREFCOUNT );
		destCertChainInfo->chain[ destCertChainInfo->chainEnd++ ] = iCopyCert;
		}
	ENSURES( i < MAX_CHAINLENGTH );
	srcCertChainInfo = NULL;	/* Make the release explicit */
	krnlReleaseObject( chainCertInfoPtr->objectHandle );
	if( cryptStatusError( status ) )
		{
		/* An error at this point indicates that the upper limit on chain
		   length isn't sufficient so we throw a (debug) exception if we 
		   get here */
		DEBUG_DIAG(( "Maximum chain length %d exceeded", MAX_CHAINLENGTH ));
		assert( DEBUG_WARN );

		/* Clean up the newly-copied certificates if necessary */
		if( destCertChainInfo->chainEnd > oldChainEnd )
			{
			freeCertChain( &destCertChainInfo->chain[ oldChainEnd ],
						   destCertChainInfo->chainEnd - oldChainEnd );
			}

		return( status );
		}

	/* If we're building an unordered certificate collection, mark the 
	   certificate chain object as a certificate collection only and exit.  
	   This is a pure container object for which only the certificate chain 
	   member contains certificates, the base certificate object doesn't 
	   correspond to an actual certificate */
	if( isCertCollection )
		{
		certInfoPtr->flags |= CERT_FLAG_CERTCOLLECTION;
		return( CRYPT_OK );
		}

	/* If the chain being attached consists of a single certificate (which 
	   occurs when we're building a new chain by signing a certificate with 
	   a CA certificate) we don't have to bother doing anything else */
	if( oldChainEnd <= 0 )
		return( CRYPT_OK );

	/* Extract the chaining information from each certificate and use it to 
	   sort the chain.  Since we know what the leaf certificate is and since 
	   chaining information such as the encoded DN data in the certinfo 
	   structure may not have been set up yet if it contains an unsigned 
	   certificate we feed in the leaf certificate and omit the chaining 
	   information */
	status = buildChainInfo( chainInfo, destCertChainInfo->chain,
							 destCertChainInfo->chainEnd );
	if( cryptStatusError( status ) )
		{
		/* Clean up the newly-copied certificates if necessary */
		if( destCertChainInfo->chainEnd > oldChainEnd )
			{
			freeCertChain( &destCertChainInfo->chain[ oldChainEnd ],
						   destCertChainInfo->chainEnd - oldChainEnd );
			}

		return( status );
		}
	return( sortCertChain( destCertChainInfo->chain, &destCertChainInfo->chainEnd, 
						   chainInfo, destCertChainInfo->chainEnd, NULL, 
						   CRYPT_UNUSED, iChainCert, FALSE ) );
	}

/****************************************************************************
*																			*
*						Read Certificate-bagging Records					*
*																			*
****************************************************************************/

/* Read a single certificate in a chain */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readSingleCert( INOUT STREAM *stream, 
						   OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
						   IN_HANDLE const CRYPT_USER iCryptOwner,
						   IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
						   const BOOLEAN dataOnlyCert,
						   const BOOLEAN isIndefiniteLength )
	{
	int length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( type == CRYPT_CERTTYPE_CERTCHAIN || \
			  type == CRYPT_ICERTTYPE_CMS_CERTSET || \
			  type == CRYPT_ICERTTYPE_SSL_CERTCHAIN );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	/* If it's an SSL certificate chain then there's a 24-bit length field 
	   between certificates */
	if( type == CRYPT_ICERTTYPE_SSL_CERTCHAIN )
		{
		status = sSkip( stream, 3, 3 );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Find out how large the certificate data is and make sure that the 
	   size is approximately valid before we pass it on to other 
	   functions */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_CERTSIZE || length >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_BADDATA );

	/* Since SSL certificate chains contain certificates interspersed with 
	   non-certificate SSL length data the higher-level code can't check the 
	   encoding, so we have to perform the check here.  Technically this 
	   isn't really necessary since importCertFromStream() imports the 
	   certificate via a kernel message which means that the encoding is 
	   checked anyway, but this is an implementation-specific detail that we 
	   can't assume will hold for future versions so we make the check 
	   explicit here */
	if( type == CRYPT_ICERTTYPE_SSL_CERTCHAIN )
		{
		void *certDataPtr;
		int complianceLevel;

		status = krnlSendMessage( iCryptOwner, IMESSAGE_GETATTRIBUTE,
								  &complianceLevel,
								  CRYPT_OPTION_CERT_COMPLIANCELEVEL );
		if( cryptStatusError( status ) )
			return( status );
		if( complianceLevel > CRYPT_COMPLIANCELEVEL_OBLIVIOUS )
			{
			status = sMemGetDataBlock( stream, &certDataPtr, length );
			if( cryptStatusOK( status ) && \
				cryptStatusError( \
					checkObjectEncoding( certDataPtr, length ) ) )
				status = CRYPT_ERROR_BADDATA;
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Read the next certificate and add it to the chain.  When importing 
	   the chain from an external (untrusted) source we create standard 
	   certificates so that we can check the signatures on each link in the 
	   chain.  When importing from a trusted source we create data-only 
	   certificates, once we've got all of the certificates and know which 
	   certificate is the leaf we can go back and decode the public key 
	   information for it */
	status = importCertFromStream( stream, iCryptCert, iCryptOwner,
								   CRYPT_CERTTYPE_CERTIFICATE, length,
								   dataOnlyCert ? KEYMGMT_FLAG_DATAONLY_CERT : \
												  KEYMGMT_FLAG_NONE );
	if( cryptStatusError( status ) )
		return( status );

	/* If the certificate chain is encoded using the indefinite form and we 
	   find the EOC octets following the certificate that we've just read, 
	   tell the caller */
	if( isIndefiniteLength )
		{
		status = checkEOC( stream );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( *iCryptCert, IMESSAGE_DECREFCOUNT );
			*iCryptCert = CRYPT_ERROR;
			return( status );
			}
		if( status == TRUE )
			{
			/* We've seen EOC octets, tell the caller that we're done */
			return( OK_SPECIAL );
			}
		}

	return( CRYPT_OK );
	}

/* Read certificate chain/sequence information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readCertChain( INOUT STREAM *stream, 
				   OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCryptCert,
				   IN_HANDLE const CRYPT_USER iCryptOwner,
				   IN_ENUM( CRYPT_CERTTYPE ) const CRYPT_CERTTYPE_TYPE type,
				   IN_KEYID_OPT const CRYPT_KEYID_TYPE keyIDtype,
				   IN_BUFFER_OPT( keyIDlength ) const void *keyID, 
				   IN_LENGTH_KEYID_Z const int keyIDlength,
				   IN_FLAGS( KEYMGMT ) const int options )
	{
	CRYPT_CERTIFICATE iCertChain[ MAX_CHAINLENGTH + 8 ];
	const int dataOnlyCert = options & KEYMGMT_FLAG_DATAONLY_CERT;
	int certSequenceLength DUMMY_INIT, endPos = 0, certChainEnd = 0;
	int iterationCount, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( iCryptCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( ( keyIDtype == CRYPT_KEYID_NONE && keyID == NULL && \
			  keyIDlength == 0 ) || \
			( ( keyIDtype == CRYPT_IKEYID_KEYID || \
				keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER ) && \
			  isReadPtr( keyID, keyIDlength ) && \
			  keyIDlength >= MIN_SKID_SIZE ) );

	REQUIRES( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( type == CRYPT_CERTTYPE_CERTCHAIN || \
			  type == CRYPT_ICERTTYPE_CMS_CERTSET || \
			  type == CRYPT_ICERTTYPE_SSL_CERTCHAIN );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				keyID == NULL && keyIDlength == 0 ) || \
			  ( ( keyIDtype == CRYPT_IKEYID_KEYID || \
				  keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER ) && \
				keyID != NULL && \
				keyIDlength >= MIN_SKID_SIZE && \
				keyIDlength < MAX_ATTRIBUTE_SIZE ) );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~( KEYMGMT_MASK_USAGEOPTIONS | \
							 KEYMGMT_MASK_CERTOPTIONS ) ) == 0 );
	REQUIRES( ( keyIDtype == CRYPT_KEYID_NONE && \
				options == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype != CRYPT_KEYID_NONE && \
				options == KEYMGMT_FLAG_NONE ) || \
			  ( keyIDtype == CRYPT_KEYID_NONE && \
				options != KEYMGMT_FLAG_NONE ) );

	/* Clear return value */
	*iCryptCert = CRYPT_ERROR;

	switch( type )
		{
		case CRYPT_CERTTYPE_CERTCHAIN:
			/* The outer wrapper has already been read by the caller so
			   the certificate chain is everything that's left */
			certSequenceLength = sMemDataLeft( stream );
			break;

		case CRYPT_ICERTTYPE_CMS_CERTSET:
			status = readConstructedI( stream, &certSequenceLength, 0 );
			break;

		case CRYPT_ICERTTYPE_SSL_CERTCHAIN:
			/* There's no outer wrapper to give us length information for an 
			   SSL certificate chain, however the length will be equal to the 
			   total remaining stream size */
			certSequenceLength = sMemDataLeft( stream );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( ( certSequenceLength == CRYPT_UNUSED ) || \
			 ( certSequenceLength > 0 && \
			   certSequenceLength < MAX_INTLENGTH_SHORT ) );

	/* If it's a definite-length chain, determine where it ends */
	if( certSequenceLength != CRYPT_UNUSED )
		endPos = stell( stream ) + certSequenceLength;

	/* We've reached the certificate(s), read the collection of certificates 
	   into certificate objects.  We allow for a bit of slop for software 
	   that gets the length encoding wrong by a few bytes.  Note that the 
	   limit is given as FAILSAFE_ITERATIONS_MED since we're using it as a 
	   fallback check on the existing MAX_CHAINLENGTH check.  In other words 
	   anything over MAX_CHAINLENGTH is handled as a normal error and it's 
	   only if we exceed this that we have an internal error */
	for( iterationCount = 0;
		 ( certSequenceLength == CRYPT_UNUSED || \
		   stell( stream ) <= endPos - MIN_ATTRIBUTE_SIZE ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		CRYPT_CERTIFICATE iNewCert DUMMY_INIT;

		/* Make sure that we don't overflow the chain */
		if( certChainEnd >= MAX_CHAINLENGTH )
			{
			freeCertChain( iCertChain, certChainEnd );
			return( CRYPT_ERROR_OVERFLOW );
			}

		/* Read the next certificate in the chain */
		status = readSingleCert( stream, &iNewCert, iCryptOwner, type, 
								 dataOnlyCert,
								 ( certSequenceLength == CRYPT_UNUSED ) ? \
									TRUE : FALSE );
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			{
			if( certChainEnd > 0 )
				freeCertChain( iCertChain, certChainEnd );
			return( status );
			}
		iCertChain[ certChainEnd++ ] = iNewCert;
		if( status == OK_SPECIAL )
			break;	/* We've reached the end of the chain */
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	/* We must have read at least one certificate in order to create a 
	   chain */
	if( certChainEnd <= 0 )
		return( CRYPT_ERROR_BADDATA );

	/* Build the complete chain from the individual certificates */
	status = buildCertChain( iCryptCert, iCertChain, certChainEnd, 
							 keyIDtype, keyID, keyIDlength, options );
	if( cryptStatusError( status ) )
		{
		freeCertChain( iCertChain, certChainEnd );
		return( status );
		}

	return( CRYPT_OK );
	}

/* Fetch a sequence of certificates from an object to create a certificate 
   chain */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
int assembleCertChain( OUT CRYPT_CERTIFICATE *iCertificate,
					   IN_HANDLE const CRYPT_HANDLE iCertSource,
					   IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
					   IN_BUFFER( keyIDlength ) const void *keyID, 
					   IN_LENGTH_KEYID const int keyIDlength,
					   IN_FLAGS( KEYMGMT ) const int options )
	{
	CRYPT_CERTIFICATE iCertSet[ MAX_CHAINLENGTH + 8 ], iCertChain, lastCert;
	MESSAGE_KEYMGMT_INFO getnextcertInfo;
	const int chainOptions = options & KEYMGMT_FLAG_DATAONLY_CERT;
	int stateInfo = CRYPT_ERROR, certSetSize = 1;
	int iterationCount, status;

	assert( isWritePtr( iCertificate, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) && \
			keyIDlength > 0 && keyIDlength < MAX_ATTRIBUTE_SIZE );
			/* The keyID can be an arbitrary value, including ones from
			   PKCS #11 devices, so it can be as little as a single byte */

	REQUIRES( isHandleRangeValid( iCertSource ) );
	REQUIRES( keyIDtype > CRYPT_KEYID_NONE && \
			  keyIDtype < CRYPT_KEYID_LAST );
	REQUIRES( keyIDlength > 0 && keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( options >= KEYMGMT_FLAG_NONE && options < KEYMGMT_FLAG_MAX && \
			  ( options & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* Get the initial certificate based on the key ID */
	setMessageKeymgmtInfo( &getnextcertInfo, keyIDtype, keyID, keyIDlength, 
						   &stateInfo, sizeof( int ), 
						   options & KEYMGMT_MASK_CERTOPTIONS );
	status = krnlSendMessage( iCertSource, IMESSAGE_KEY_GETFIRSTCERT,
							  &getnextcertInfo, KEYMGMT_ITEM_PUBLICKEY );
	if( cryptStatusError( status ) )
		return( status );
	iCertSet[ 0 ] = lastCert = getnextcertInfo.cryptHandle;

	/* Fetch subsequent certificates that make up the chain based on the 
	   state information.  Since the basic options apply only to the leaf 
	   certificate we only allow the data-only certificate flag at this 
	   point.  See the comment in readCertChain() for the use of 
	   FAILSAFE_ITERATIONS_MED for the bounds check */
	setMessageKeymgmtInfo( &getnextcertInfo, CRYPT_KEYID_NONE, NULL, 0, 
						   &stateInfo, sizeof( int ), chainOptions );
	for( iterationCount = 0;
		 cryptStatusOK( status ) && \
			iterationCount < FAILSAFE_ITERATIONS_MED; 
		 iterationCount++ )
		{
		int selfSigned;

		/* If we've reached a self-signed (CA root) certificate, stop.  Note 
		   that this can't detect PKIX path-kludge certificates which look 
		   identical to CA root certificates and can only be reliably 
		   identified if they're present in the middle of a pre-built 
		   chain */
		status = krnlSendMessage( lastCert, IMESSAGE_GETATTRIBUTE, 
								  &selfSigned, CRYPT_CERTINFO_SELFSIGNED );
		if( cryptStatusError( status ) || selfSigned > 0 )
			break;

		/* Get the next certificate in the chain from the source, import it,
		   and add it to the collection */
		getnextcertInfo.cryptHandle = CRYPT_ERROR;	/* Reset result handle */
		status = krnlSendMessage( iCertSource, IMESSAGE_KEY_GETNEXTCERT,
								  &getnextcertInfo, KEYMGMT_ITEM_PUBLICKEY );
		if( cryptStatusError( status ) )
			{
			/* If we get a notfound error this is OK since it just means 
			   that we've reached the end of the chain */
			if( status == CRYPT_ERROR_NOTFOUND )
				status = CRYPT_OK;

			break;
			}

		/* Make sure that we don't overflow the chain */
		if( certSetSize >= MAX_CHAINLENGTH )
			{
			krnlSendNotifier( getnextcertInfo.cryptHandle,
							  IMESSAGE_DECREFCOUNT );
			status = CRYPT_ERROR_OVERFLOW;
			break;
			}

		iCertSet[ certSetSize++ ] = lastCert = getnextcertInfo.cryptHandle;
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );
	if( cryptStatusError( status ) )
		{
		freeCertChain( iCertSet, certSetSize );
		return( status );
		}

	/* Build the complete chain from the individual certificates */
	status = buildCertChain( &iCertChain, iCertSet, certSetSize, 
							 CRYPT_KEYID_NONE, NULL, 0, chainOptions );
	if( cryptStatusError( status ) )
		{
		freeCertChain( iCertSet, certSetSize );
		return( status );
		}

	/* Make sure that the data source that we're using actually gave us what
	   we asked for.  We can't do this for key IDs with data-only 
	   certificates since the IDs aren't present in the certificate but are 
	   generated dynamically in the associated context */
	if( !( ( keyIDtype == CRYPT_IKEYID_KEYID || \
			 keyIDtype == CRYPT_IKEYID_PGPKEYID ) && \
		   ( options & KEYMGMT_FLAG_DATAONLY_CERT ) ) )
		{
		status = iCryptVerifyID( iCertChain, keyIDtype, keyID, keyIDlength );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCertChain, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		}

	*iCertificate = iCertChain;
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*						Write Certificate-bagging Records					*
*																			*
****************************************************************************/

/* Determine the size of and write a certificate path from a base 
   certificate up to the root.  This gets a bit complicated because 
   alongside the overall length, SSL requires that we return component-by-
   component lengths that need to be inserted as 24-bit (3 byte) values 
   between each certificate.  To handle this we take as an optional 
   parameter an array used to store the individual path component lengths */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int sizeofCertPath( const CERT_INFO *certInfoPtr,
						   OUT_LENGTH_SHORT_Z int *certPathLength,
						   OUT_ARRAY_OPT_C( MAX_CHAINLENGTH ) int *certSizeInfo )
	{
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	int length = 0, i;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( certPathLength, sizeof( int ) ) );
	assert( certSizeInfo == NULL || \
			isWritePtr( certSizeInfo, sizeof( int ) * MAX_CHAINLENGTH ) );

	/* Clear return values */
	*certPathLength = 0;
	if( certSizeInfo != NULL )
		memset( certSizeInfo, 0, sizeof( int ) * MAX_CHAINLENGTH );

	/* Evaluate the size of the current certificate and the issuer 
	   certificates in the chain.  If it's a certificate collection it's 
	   just a container for random certificates but not a certificate in its 
	   own right so we skip the leaf certificate */
	if( !( certInfoPtr->flags & CERT_FLAG_CERTCOLLECTION ) )
		{
		length = certInfoPtr->certificateSize;
		if( certSizeInfo != NULL )
			length += 3;
		}
	for( i = 0; i < certChainInfo->chainEnd && i < MAX_CHAINLENGTH; i++ )
		{
		MESSAGE_DATA msgData;
		int status;

		setMessageData( &msgData, NULL, 0 );
		status = krnlSendMessage( certChainInfo->chain[ i ], 
								  IMESSAGE_CRT_EXPORT, &msgData, 
								  CRYPT_CERTFORMAT_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( status );
		length += msgData.length;
		if( certSizeInfo != NULL )
			{
			certSizeInfo[ i ] = msgData.length;
			length += 3;
			}
		}
	ENSURES( i < MAX_CHAINLENGTH );
	*certPathLength = length;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int writeCertPath( INOUT STREAM *stream, 
						  const CERT_INFO *certInfoPtr,
						  IN_ARRAY_OPT_C( MAX_CHAINLENGTH ) \
								const int *certSizeInfo )

	{
	CERT_CERT_INFO *certChainInfo = certInfoPtr->cCertCert;
	int i, status = CRYPT_OK;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( certSizeInfo == NULL || \
			isReadPtr( certSizeInfo, sizeof( int ) * MAX_CHAINLENGTH ) );

	/* Write the current certificate and the associated certificate chain up 
	   to the root.  If it's a certificate collection it's just a container 
	   for random certificates but not a certificate in its own right so we 
	   skip the leaf certificate */
	if( !( certInfoPtr->flags & CERT_FLAG_CERTCOLLECTION ) )
		{
		if( certSizeInfo != NULL )
			{
			sputc( stream, 0 );
			writeUint16( stream, certInfoPtr->certificateSize );
			}
		status = swrite( stream, certInfoPtr->certificate, 
						 certInfoPtr->certificateSize );
		}
	for( i = 0; cryptStatusOK( status ) && \
				i < certChainInfo->chainEnd && \
				i < MAX_CHAINLENGTH; i++ )
		{
		if( certSizeInfo != NULL )
			{
			sputc( stream, 0 );
			writeUint16( stream, certSizeInfo[ i ] );
			}
		status = exportCertToStream( stream, certChainInfo->chain[ i ], 
									 CRYPT_CERTFORMAT_CERTIFICATE );
		}
	ENSURES( i < MAX_CHAINLENGTH );

	return( status );
	}

/* Write certificate chain/sequence information:

	CertChain ::= SEQUENCE {
		contentType				OBJECT IDENTIFIER,	-- signedData
		content			  [ 0 ]	EXPLICIT SEQUENCE {
			version				INTEGER (1),
			digestAlgorithms	SET OF AlgorithmIdentifier,	-- SIZE(0)
			contentInfo			SEQUENCE {
				signedData		OBJECT IDENTIFIER	-- data
				}
			certificates  [ 0 ]	SET OF {
									Certificate
				}
			}
		signerInfos				SET OF SignerInfo			-- SIZE(0)
		} */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofCertCollection( const CERT_INFO *certInfoPtr,
						  IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormatType )
	{
	int length, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	REQUIRES( certFormatType == CRYPT_ICERTFORMAT_CERTSET || \
			  certFormatType == CRYPT_ICERTFORMAT_CERTSEQUENCE || \
			  certFormatType == CRYPT_ICERTFORMAT_SSL_CERTCHAIN );

	/* If it's an SSL certificate chain then we have to pass in a dummy
	   component-length pointer to ensure that it's evaluated as an SSL 
	   chain with intermediate lengths rather than a straight PKCS #7/CMS
	   chain */
	if( certFormatType == CRYPT_ICERTFORMAT_SSL_CERTCHAIN )
		{
		int certSizeInfo[ MAX_CHAINLENGTH + 8 ];

		status = sizeofCertPath( certInfoPtr, &length, certSizeInfo );
		}
	else
		{
		/* It's a standard PKCS #7/CMS chain */
		status = sizeofCertPath( certInfoPtr, &length, NULL );
		if( cryptStatusError( status ) )
			return( status );
		status = length = sizeofObject( length );
		}
	if( cryptStatusError( status ) )
		return( status );
	return( length );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertCollection( INOUT STREAM *stream, 
						 const CERT_INFO *certInfoPtr,
						 IN_ENUM( CRYPT_CERTFORMAT ) \
							const CRYPT_CERTFORMAT_TYPE certFormatType )
	{
	int certSizeInfo[ MAX_CHAINLENGTH + 8 ];
	int *certSizePtr = \
			( certFormatType == CRYPT_ICERTFORMAT_SSL_CERTCHAIN ) ? \
			certSizeInfo : NULL;
	int certCollectionLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	
	REQUIRES( certFormatType == CRYPT_ICERTFORMAT_CERTSET || \
			  certFormatType == CRYPT_ICERTFORMAT_CERTSEQUENCE || \
			  certFormatType == CRYPT_ICERTFORMAT_SSL_CERTCHAIN );

	status = sizeofCertPath( certInfoPtr, &certCollectionLength, 
							 certSizePtr );
	if( cryptStatusError( status ) )
		return( status );
	switch( certFormatType )
		{
		case CRYPT_ICERTFORMAT_CERTSET:
			writeConstructed( stream, certCollectionLength, 0 );
			break;

		case CRYPT_ICERTFORMAT_CERTSEQUENCE:
			writeSequence( stream, certCollectionLength );
			break;

		case CRYPT_ICERTFORMAT_SSL_CERTCHAIN:
			break;

		default:
			retIntError();
		}
	return( writeCertPath( stream, certInfoPtr, certSizePtr ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeCertChain( INOUT STREAM *stream, 
					const CERT_INFO *certInfoPtr )
	{
	int certSetLength, innerLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );

	status = sizeofCertPath( certInfoPtr, &certSetLength, NULL );
	if( cryptStatusError( status ) )
		return( status );

	/* Determine how big the encoded certificate chain/sequence will be */
	innerLength = sizeofShortInteger( 1 ) + ( int ) sizeofObject( 0 ) + \
					  ( int ) sizeofObject( sizeofOID( OID_CMS_DATA ) ) + \
					  ( int ) sizeofObject( certSetLength ) + \
					  ( int ) sizeofObject( 0 );

	/* Write the outer SEQUENCE wrapper, contentType, and content wrapper */
	writeSequence( stream, 
				   sizeofOID( OID_CMS_SIGNEDDATA ) + \
					( int ) sizeofObject( sizeofObject( innerLength ) ) );
	swrite( stream, OID_CMS_SIGNEDDATA, sizeofOID( OID_CMS_SIGNEDDATA ) );
	writeConstructed( stream, sizeofObject( innerLength ), 0 );
	writeSequence( stream, innerLength );

	/* Write the inner content */
	writeShortInteger( stream, 1, DEFAULT_TAG );
	writeSet( stream, 0 );
	writeSequence( stream, sizeofOID( OID_CMS_DATA ) );
	swrite( stream, OID_CMS_DATA, sizeofOID( OID_CMS_DATA ) );
	writeConstructed( stream, certSetLength, 0 );
	status = writeCertPath( stream, certInfoPtr, NULL );
	if( cryptStatusOK( status ) )
		status = writeSet( stream, 0 );
	return( status );
	}
#endif /* USE_CERTIFICATES */
