/****************************************************************************
*																			*
*						Certificate Usage Checking Routines					*
*						Copyright Peter Gutmann 1997-2008					*
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
*						ExtKeyUsage to Key Usage Routines					*
*																			*
****************************************************************************/

/* The following keyUsage settings are used based on extendedKeyUsage and
   Netscape key usage extensions.  In the following 'Y' = required, 'w' =
   written but apparently not required, 'S' = for signature keys only, 'E' = 
   for encryption keys only, 'KA' = for key agreement keys only.

						dig	non	key	dat	key	cer	crl	enc	dec
						sig	rep	enc	enc	agt	sig	sig	onl	onl
   PKIX:				-----------------------------------
	serverAuth			 S		 E		KA					[1]
	clientAuth			 S
	codeSign			 Y
	email				 Y	 Y	 E
	ipsecEndSys			 S		 E		KA
	ipsecTunnel			 S		 E		KA
	ipsecUser			 S		 E		KA
	timeStamping		 Y	 Y
	ocsp				 Y
	directoryService	 ?
   MS:					-----------------------------------
	individualCodeSign	 Y
	commercialCodeSign	 Y
	ctlSign				 Y
	tsa					 Y
	sgc							 E
	encryptedFS					 E
   NS:					-----------------------------------
	sgc							 E
   NS extensions:		-----------------------------------
	sslClient			 Y
	sslServer			 S		 E							[1]
	sMime				 S		 E
	objectSign			 Y
	sslCA                               	 Y	 w
	sMimeCA									 Y	 w
	objectSignCA							 Y	 w
   Verisign:			-----------------------------------
	sgcCA									 Y	 w
						-----------------------------------
						dig	non	key	dat	key	cer	crl	enc	dec
						sig	rep	enc	enc	agt	sig	sig	onl	onl

   [1] These keys need to potentially perform both decryption for RSA key 
       transport and signing for authenticating DH key agreement */

/* Mask for key usage types that we don't check for consistency against
   extended key usages.  The two CA usages don't occur in extended key usage,
   and no-one can agree on what non-repudiation is supposed to mean */

#define USAGE_MASK_NONRELEVANT	( CRYPT_KEYUSAGE_NONREPUDIATION | \
								  CRYPT_KEYUSAGE_KEYCERTSIGN | \
								  CRYPT_KEYUSAGE_CRLSIGN )

/* Flags to denote the algorithm type */

#define ALGO_TYPE_NONE			0
#define ALGO_TYPE_SIGN			1
#define ALGO_TYPE_CRYPT			2
#define ALGO_TYPE_KEYAGREEMENT	4
#define ALGO_TYPE_FLAG_NONE		0		/* Defines for range checking */
#define ALGO_TYPE_FLAG_MAX		0x07

/* Table mapping extended key usage values to key usage flags */

typedef struct {
	const CRYPT_ATTRIBUTE_TYPE usageType;
	const int keyUsageFlags;
	} EXT_USAGE_INFO;
	
static const EXT_USAGE_INFO FAR_BSS extendedUsageInfo[] = {
	{ CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING,/* individualCodeSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING,/* commercialCodeSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING,/* certTrustListSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING,	/* timeStampSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO,	/* serverGatedCrypto */
	  CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM,	/* encrypedFileSystem */
	  CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ CRYPT_CERTINFO_EXTKEY_SERVERAUTH,				/* serverAuth */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_CLIENTAUTH,				/* clientAuth */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_CODESIGNING,			/* codeSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION,		/* emailProtection */
	  KEYUSAGE_SIGN },
	{ CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM,			/* ipsecEndSystem */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL,			/* ipsecTunnel */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_IPSECUSER,				/* ipsecUser */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_TIMESTAMPING,			/* timeStamping */
	  KEYUSAGE_SIGN },
	{ CRYPT_CERTINFO_EXTKEY_OCSPSIGNING,			/* ocspSigning */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE,		/* directoryService */
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO,	/* serverGatedCrypto */
	  CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA,/* serverGatedCrypto CA */
	  KEYUSAGE_CA },
	{ CRYPT_ATTRIBUTE_NONE, 0 }, { CRYPT_ATTRIBUTE_NONE, 0 }
	};

#ifdef USE_CERT_OBSOLETE

/* Table mapping Netscape certificate-type flags to extended key usage 
   flags */

typedef struct {
	const int certType;
	const int keyUsageFlags;
	} NS_CERT_TYPE_INFO;
	
static const NS_CERT_TYPE_INFO FAR_BSS nsCertTypeInfo[] = {
	{ CRYPT_NS_CERTTYPE_SSLCLIENT,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE },
	{ CRYPT_NS_CERTTYPE_SSLSERVER,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE | CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ CRYPT_NS_CERTTYPE_SMIME,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE | CRYPT_KEYUSAGE_KEYENCIPHERMENT },
	{ CRYPT_NS_CERTTYPE_OBJECTSIGNING,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE | CRYPT_KEYUSAGE_NONREPUDIATION },
	{ CRYPT_NS_CERTTYPE_RESERVED, 0 },
	{ CRYPT_NS_CERTTYPE_SSLCA,
	  KEYUSAGE_CA },
	{ CRYPT_NS_CERTTYPE_SMIMECA,
	  KEYUSAGE_CA },
	{ CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA,
	  KEYUSAGE_CA },
	{ 0, 0 }, { 0, 0 }
	};
#endif /* USE_CERT_OBSOLETE */

/* Build up key usage flags consistent with the extended key usage purpose.  
   We don't have to perform any special-case handling for 
   anyExtendedKeyUsage (added in RFC 3280, section 4.2.1.13) since it's a 
   no-op extension whose presence is the equivalent of adding "|| TRUE" to 
   an expression */

CHECK_RETVAL_RANGE( CRYPT_KEYUSAGE_NONE, CRYPT_KEYUSAGE_FLAG_MAX ) STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int getExtendedKeyUsageFlags( const ATTRIBUTE_PTR *attributes,
									 IN_FLAGS( ALGO_TYPE ) const int algorithmType,
									 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
										CRYPT_ATTRIBUTE_TYPE *errorLocus )
	{
	int keyUsage = CRYPT_KEYUSAGE_NONE, i;

	assert( isReadPtr( attributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES( algorithmType > ALGO_TYPE_FLAG_NONE && \
			  algorithmType < ALGO_TYPE_FLAG_MAX );

	/* Clear return value */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;

	/* Most certificates don't contain an extendedKeyUsage extension so 
	   rather than iterating through a long list of non-present attribute 
	   fields we perform an early-out check to see whether there's actually
	   anything to do */
	if( !checkAttributePresent( attributes, CRYPT_CERTINFO_EXTKEYUSAGE ) )
		return( CRYPT_KEYUSAGE_NONE );

	/* There's an extendedKeyUsage extension present, build up the key usage
	   flags for it */
	for( i = 0; extendedUsageInfo[ i ].usageType != CRYPT_ATTRIBUTE_NONE && \
				i < FAILSAFE_ARRAYSIZE( extendedUsageInfo, EXT_USAGE_INFO ); 
		 i++ )
		{
		int extendedUsage = CRYPT_KEYUSAGE_NONE;

		/* If this usage isn't present, continue */
		if( !checkAttributeFieldPresent( attributes, 
										 extendedUsageInfo[ i ].usageType ) )
			continue;

		/* If the usage is consistent with the algorithm type, add it */
		if( algorithmType & ALGO_TYPE_SIGN )
			{
			extendedUsage |= \
				extendedUsageInfo[ i ].keyUsageFlags & ( KEYUSAGE_SIGN | \
														 KEYUSAGE_CA );
			}
		if( algorithmType & ALGO_TYPE_CRYPT )
			{
			extendedUsage |= \
				extendedUsageInfo[ i ].keyUsageFlags & KEYUSAGE_CRYPT;
			}
		if( algorithmType & ALGO_TYPE_KEYAGREEMENT )
			{
			extendedUsage |= \
				extendedUsageInfo[ i ].keyUsageFlags & KEYUSAGE_KEYAGREEMENT;
			}

		/* If there's no key usage consistent with the extended usage and the
		   extended usage isn't some special-case usage, return an error */
		if( extendedUsage == CRYPT_KEYUSAGE_NONE && \
			extendedUsageInfo[ i ].keyUsageFlags != CRYPT_KEYUSAGE_NONE )
			{
			*errorLocus = extendedUsageInfo[ i ].usageType;
			return( CRYPT_ERROR_INVALID );
			}

		keyUsage |= extendedUsage;
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( extendedUsageInfo, EXT_USAGE_INFO ) );

	return( keyUsage );
	}

#ifdef USE_CERT_OBSOLETE

/* Build up key usage flags consistent with the Netscape certificate-type 
   purpose */

CHECK_RETVAL_RANGE( CRYPT_KEYUSAGE_NONE, CRYPT_KEYUSAGE_MAX ) STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int getNetscapeCertTypeFlags( const ATTRIBUTE_PTR *attributes,
									 IN_FLAGS( ALGO_TYPE ) const int algorithmType,
									 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
										CRYPT_ATTRIBUTE_TYPE *errorLocus )
	{
	int nsCertType, keyUsage = 0, i, status;

	assert( isReadPtr( attributes, sizeof( ATTRIBUTE_PTR_STORAGE ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );

	REQUIRES( algorithmType > ALGO_TYPE_FLAG_NONE && \
			  algorithmType < ALGO_TYPE_FLAG_MAX );

	/* If there isn't a Netscape certificate-type extension present, exit */
	status = getAttributeFieldValue( attributes, CRYPT_CERTINFO_NS_CERTTYPE, 
									 CRYPT_ATTRIBUTE_NONE, &nsCertType );
	if( cryptStatusError( status ) )
		return( 0 );

	/* The Netscape certificate-type value is a bitfield containing the 
	   different certificate types.  For each certificate-type flag that's 
	   set we set the corresponding keyUsage flags */
	for( i = 0; nsCertTypeInfo[ i ].certType && \
				i < FAILSAFE_ARRAYSIZE( nsCertTypeInfo, NS_CERT_TYPE_INFO ); i++ )
		{
		int nsUsage = 0;

		/* If this isn't the currently-selected certificate type, continue */
		if( !( nsCertType & nsCertTypeInfo[ i ].certType ) )
			continue;

		/* If the usage is consistent with the algorithm type, add it */
		if( algorithmType & ALGO_TYPE_SIGN )
			nsUsage |= nsCertTypeInfo[ i ].keyUsageFlags & ( KEYUSAGE_SIGN | \
															 KEYUSAGE_CA );
		if( algorithmType & ALGO_TYPE_CRYPT )
			nsUsage |= nsCertTypeInfo[ i ].keyUsageFlags & KEYUSAGE_CRYPT;
		if( algorithmType & ALGO_TYPE_KEYAGREEMENT )
			nsUsage |= nsCertTypeInfo[ i ].keyUsageFlags & KEYUSAGE_KEYAGREEMENT;

		/* If there's no key usage consistent with the Netscape 
		   certificate-type, return an error */
		if( nsUsage == 0 )
			{
			*errorLocus = CRYPT_CERTINFO_NS_CERTTYPE;
			return( CRYPT_ERROR_INVALID );
			}

		keyUsage |= nsUsage;
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( nsCertTypeInfo, NS_CERT_TYPE_INFO ) );

	return( keyUsage );
	}
#endif /* USE_CERT_OBSOLETE */

/* Get key usage flags for a certificate based on its extended key 
   usage/Netscape cert-type.  Returns 0 if no extKeyUsage/cert-type values 
   present */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
int getKeyUsageFromExtKeyUsage( const CERT_INFO *certInfoPtr,
								OUT_FLAGS_Z( CRYPT_KEYUSAGE ) int *keyUsage,
								OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
									CRYPT_ATTRIBUTE_TYPE *errorLocus, 
								OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )
	{
	int algorithmType = ALGO_TYPE_NONE, localKeyUsage, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	/* Clear return values */
	*keyUsage = CRYPT_KEYUSAGE_NONE;
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If there are no attributes present there's nothing to do */
	if( certInfoPtr->attributes == NULL )
		return( CRYPT_OK );

	/* Determine the possible algorithm usage type(s) */
	if( isCryptAlgo( certInfoPtr->publicKeyAlgo ) )
		algorithmType |= ALGO_TYPE_CRYPT;
	if( isSigAlgo( certInfoPtr->publicKeyAlgo ) )
		algorithmType |= ALGO_TYPE_SIGN;
	if( isKeyxAlgo( certInfoPtr->publicKeyAlgo ) )
		algorithmType |= ALGO_TYPE_KEYAGREEMENT;
	ENSURES( algorithmType != ALGO_TYPE_NONE );

	/* Get the key usage flags for the given extended/Netscape usage type(s)
	   and algorithm type */
	status = localKeyUsage = \
		getExtendedKeyUsageFlags( certInfoPtr->attributes, algorithmType, 
								  errorLocus );
#ifdef USE_CERT_OBSOLETE
	if( !cryptStatusError( status ) )
		{
		status = getNetscapeCertTypeFlags( certInfoPtr->attributes, 
										   algorithmType, errorLocus );
		if( !cryptStatusError( status ) )
			localKeyUsage |= status;
		}
#endif /* USE_CERT_OBSOLETE */
	if( cryptStatusError( status ) )
		{
		/* We only have to set the error type at this point since the error
		   locus was set when we got the key usage flags */
		*errorType = CRYPT_ERRTYPE_CONSTRAINT;
		return( CRYPT_ERROR_INVALID );
		}
	*keyUsage = localKeyUsage;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Check Key/Certificate Usage						*
*																			*
****************************************************************************/

/* Check that a certificate/key is valid for a particular purpose.  This 
   function is used in one of two ways:

	1. Check that a key can be used for a particular purpose, regardless of
	   whether the certificate extensions that define the usage make any 
	   sense or not.  This is used when performing an object usage check 
	   such as whether a key can be used for signing or encryption.

	2. Check that the key usage is consistent.  This is used when performing
	   a general certificate validity check, indicated by setting the 
	   CHECKKEY_FLAG_GENCHECK  check flag.

   Processing is done in three phases:
   
	1. Fix up usage flags at lower compliance levels if necessary.
	2. Check for strict usability even if the flags don't make sense.
	3. Check consistency as per the PKIX and X.509 specs */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 5, 6 ) ) \
int checkKeyUsage( const CERT_INFO *certInfoPtr,
				   IN_FLAGS_Z( CHECKKEY ) const int flags, 
				   IN_FLAGS_Z( CRYPT_KEYUSAGE ) const int specificUsage,
				   IN_RANGE( CRYPT_COMPLIANCELEVEL_OBLIVIOUS, \
							 CRYPT_COMPLIANCELEVEL_LAST - 1 ) \
						const int complianceLevel,
				   OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
						CRYPT_ATTRIBUTE_TYPE *errorLocus,
				   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	ATTRIBUTE_PTR *attributePtr;
	const BOOLEAN isGeneralCheck = ( flags & CHECKKEY_FLAG_GENCHECK ) ? \
								   TRUE : FALSE;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	BOOLEAN keyUsageCritical = 0;
	int rawExtKeyUsage;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	BOOLEAN isCA = FALSE;
	const int trustedUsage = \
				( certInfoPtr->type == CRYPT_CERTTYPE_CERTIFICATE || \
				  certInfoPtr->type == CRYPT_CERTTYPE_CERTCHAIN ) ? \
				certInfoPtr->cCertCert->trustedUsage : CRYPT_UNUSED;
	int keyUsage, rawKeyUsage, extKeyUsage, caKeyUsage, value, status;

	assert( isReadPtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( flags >= CHECKKEY_FLAG_NONE && \
			  flags < CHECKKEY_FLAG_MAX );
	REQUIRES( specificUsage >= CRYPT_KEYUSAGE_FLAG_NONE && \
			  specificUsage < CRYPT_KEYUSAGE_FLAG_MAX );
	REQUIRES( complianceLevel >= CRYPT_COMPLIANCELEVEL_OBLIVIOUS && \
			  complianceLevel < CRYPT_COMPLIANCELEVEL_LAST );
	REQUIRES( ( ( flags & CHECKKEY_FLAG_CA ) && \
				( specificUsage & ( CRYPT_KEYUSAGE_KEYCERTSIGN | \
									CRYPT_KEYUSAGE_CRLSIGN ) ) ) || \
			  ( !( flags & CHECKKEY_FLAG_CA ) && \
				( ( specificUsage & ( CRYPT_KEYUSAGE_DIGITALSIGNATURE | \
									  CRYPT_KEYUSAGE_KEYENCIPHERMENT | \
									  CRYPT_KEYUSAGE_KEYAGREEMENT ) ) || \
				  ( specificUsage == CRYPT_KEYUSAGE_NONE ) ) ) );

	/* There is one universal case in which a key is regarded as invalid for
	   the requested use and that's when it's explicitly not trusted for the 
	   purpose.  Note that this check (in oblivious mode) differs slightly
	   from the later check (in reduced mode or higher) in that in oblivious
	   mode we ignore the certificate's actual key usage and check only the 
	   requested against trusted usage */
	if( specificUsage != CRYPT_KEYUSAGE_NONE && \
		trustedUsage != CRYPT_UNUSED && !( trustedUsage & specificUsage ) )
		{
		/* The issuer is explicitly not trusted to perform the requested 
		   operation */
		setErrorValues( CRYPT_CERTINFO_TRUSTED_USAGE,
						CRYPT_ERRTYPE_ISSUERCONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If we're running in oblivious mode there's nothing else to check */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_REDUCED )
		return( CRYPT_OK );

	/* Phase 1: Fix up values if required */

	/* Obtain assorted certificate information */
	status = getAttributeFieldValue( certInfoPtr->attributes, 
									 CRYPT_CERTINFO_CA, 
									 CRYPT_ATTRIBUTE_NONE, &value );
	if( cryptStatusOK( status ) && value > 0 )
		isCA = TRUE;
	status = getKeyUsageFromExtKeyUsage( certInfoPtr, &extKeyUsage, 
										 errorLocus, errorType );
	if( cryptStatusError( status ) )
		return( status );

	/* If it's a v1 self-signed certificate then the CA status and key usage 
	   are implicit/undefined */
	if( certInfoPtr->version == X509_V1 && \
		( certInfoPtr->flags & CERT_FLAG_SELFSIGNED ) )
		{
		/* If it's claiming to be a CA certificate by virtue of being a v1 
		   self-signed certificate then there can't be any v3 CA attributes 
		   (or any v3 attributes for that matter) present.  Unfortunately we 
		   can't just check for the complete non-presence of attributes 
		   because the certificate-import code will have converted an email 
		   address in the DN into the appropriate altName component, 
		   creating at least one valid (in this case) attribute */
		if( isGeneralCheck && \
			( checkAttributePresent( certInfoPtr->attributes, 
									 CRYPT_CERTINFO_BASICCONSTRAINTS ) || \
			  checkAttributePresent( certInfoPtr->attributes, 
									 CRYPT_CERTINFO_KEYUSAGE ) || \
			  extKeyUsage != 0 ) )
			{
			setErrorValues( CRYPT_CERTINFO_VERSION, 
							CRYPT_ERRTYPE_ATTR_VALUE );
			return( CRYPT_ERROR_INVALID );
			}

		/* It's a v1 self-signed certificate with no keyUsage present, don't
		   perform any usage-specific checks */
		return( CRYPT_OK );
		}

	/* Get the certificate's keyUsage.  If we're running at a reduced 
	   compliance level and the CA flag is set and keyUsage isn't or vice 
	   versa we synthesise the required value from the other value in order 
	   to pass the checks that follow */
	attributePtr = findAttributeField( certInfoPtr->attributes,
									   CRYPT_CERTINFO_KEYUSAGE, 
									   CRYPT_ATTRIBUTE_NONE );
	if( attributePtr != NULL )
		{
		/* Check whether the keyUsage extension is critical, needed for 
		   some odd PKIX-defined checks at higher compliance levels */
		status = getAttributeDataValue( attributePtr, &keyUsage );
		if( cryptStatusError( status ) )
			return( status );
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
		keyUsageCritical = \
			checkAttributeProperty( attributePtr, ATTRIBUTE_PROPERTY_CRITICAL );
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

		/* If the CA key usages are set make sure that the CA flag is set in
		   an appropriate manner */
		if( complianceLevel < CRYPT_COMPLIANCELEVEL_STANDARD && \
			( keyUsage & specificUsage & KEYUSAGE_CA ) && !isCA )
			isCA = TRUE;
		}
	else
		{
		/* There's no keyUsage information present, start with no usage
		   available */
		keyUsage = 0;

		/* If the CA flag is set make sure that the keyUsage is set in an
		   appropriate manner */
		if( complianceLevel < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL && isCA )
			keyUsage = KEYUSAGE_CA;

		/* Some broken certificates don't have any keyUsage present, which 
		   is meant to imply that the certificate can be used for any usage 
		   that the key is capable of modulo the magic usages keyCertSign 
		   and crlSign.  To handle this we map the algorithm type to the 
		   matching usage types.  In theory the usage may be further 
		   modified by the certificate policy, extKeyUsage, and who knows 
		   what else but in the presence of a certificate like that it's up 
		   to the user to sort out what they want to do with it.
		
		   Some even more broken certificates indicate their usage via a 
		   Netscape key usage (even though they use X.509 flags everywhere 
		   else - insanes numquam moriuntur) which means that we fail them 
		   if we're strictly applying the PKIX requirements at a higher 
		   compliance level.  At this lower level fixAttributes() will have 
		   mapped the Netscape usage to the equivalent X.509 usage so 
		   there's always a keyUsage present */
		if( isCryptAlgo( certInfoPtr->publicKeyAlgo ) )
			keyUsage |= CRYPT_KEYUSAGE_KEYENCIPHERMENT;
		if( isSigAlgo( certInfoPtr->publicKeyAlgo ) )
			keyUsage |= KEYUSAGE_SIGN;
		if( isKeyxAlgo( certInfoPtr->publicKeyAlgo ) )
			keyUsage |= CRYPT_KEYUSAGE_KEYAGREEMENT;
		}
	caKeyUsage = keyUsage & KEYUSAGE_CA;

	/* Apply the trusted-usage restrictions if necessary */
	rawKeyUsage = keyUsage;
#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	rawExtKeyUsage = extKeyUsage;
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */
	if( trustedUsage != CRYPT_UNUSED )
		{
		keyUsage &= trustedUsage;
		extKeyUsage &= trustedUsage;
		}

	/* Phase 2: Strict usability check */

	/* If we're looking for a CA certificate make sure that the 
	   basicConstraints CA flag is set and the keyUsage indicates a CA usage 
	   (PKIX sections 4.2.1.3 and 4.2.1.10).  RFC 2459 left this open, it 
	   was made explicit in RFC 3280.  If we're running at a reduced 
	   compliance level then the settings will have been adjusted as 
	   required earlier on */
	if( flags & CHECKKEY_FLAG_CA )
		{
		if( !isCA )
			{
			setErrorValues( CRYPT_CERTINFO_CA, CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		if( !( caKeyUsage & specificUsage ) )
			{
			setErrorValues( CRYPT_CERTINFO_KEYUSAGE, 
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* There is one universal case in which a key is regarded as invalid for
	   the requested use and that's when it's explicitly not trusted for the 
	   purpose */
	if( specificUsage != CRYPT_KEYUSAGE_NONE && \
		trustedUsage != CRYPT_UNUSED && !( specificUsage & keyUsage ) )
		{
		setErrorValues( CRYPT_CERTINFO_TRUSTED_USAGE,
						CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If we're doing a reduced level of checking, we're done */
	if( complianceLevel < CRYPT_COMPLIANCELEVEL_STANDARD )
		return( CRYPT_OK );

	/* If we're just performing a key-usability check rather than a general
	   check that the key usage is in order, we're done */
	if( !isGeneralCheck )
		return( CRYPT_OK );

	/* Phase 3: Consistency check */

	/* If the CA flag is set make sure that there's a keyUsage with one of 
	   the CA usages present.  Conversely if there are CA key usages 
	   present make sure that the CA flag is set.  In other words this
	   check tests for an XOR relation, ( CA && kU ) || ( !CA && !kU ).
	   
	   The CA flag is actually a leftover from an early v3 certificate 
	   concept and is made entirely redundant by the keyUsage flags but we 
	   have to check it regardless (PKIX sections 4.2.1.3 and 4.2.1.10).  
	   RFC 2459 left this open, it was made explicit in RFC 3280 */
	if( isCA )
		{
		/* It's a CA certificate, make sure that a CA keyUsage is set */
		if( !( caKeyUsage | extKeyUsage ) )
			{
			setErrorValues( CRYPT_CERTINFO_KEYUSAGE, 
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}	
	else
		{
		/* It's a non-CA certificate, make sure that no CA keyUsage is set */
		if( ( caKeyUsage | extKeyUsage ) & KEYUSAGE_CA )
			{
			setErrorValues( CRYPT_CERTINFO_CA, CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Check and enforce the keyUsage if required (PKIX section 4.2.1.3).  
	   RFC 2459 included some waffly text about critical vs. non-critical 
	   usage, RFC 3280 made this explicit regardless of criticality */
	if( specificUsage != CRYPT_KEYUSAGE_NONE )
		{
		BOOLEAN usageOK = FALSE;

		/* If it's a key agreement usage the checking gets a bit complex
		   (PKIX-ALGS section 2.3.3), we have to make sure that it's both a 
		   permitted usage and not an excluded usage */
		if( complianceLevel >= CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL && \
			( specificUsage & ( CRYPT_KEYUSAGE_ENCIPHERONLY | \
								CRYPT_KEYUSAGE_DECIPHERONLY ) ) )
			{
			const int excludedUsage = \
					( specificUsage & CRYPT_KEYUSAGE_ENCIPHERONLY ) ? \
					CRYPT_KEYUSAGE_DECIPHERONLY : CRYPT_KEYUSAGE_ENCIPHERONLY;

			if( ( keyUsage & specificUsage ) && !( keyUsage & excludedUsage ) )
				usageOK = TRUE;
			}
		else
			{
			/* Conventional usage flag, do a straight check */
			if( keyUsage & specificUsage )
				usageOK = TRUE;
			}
		if( !usageOK )
			{
			setErrorValues( ( rawKeyUsage & specificUsage ) ? \
								CRYPT_CERTINFO_TRUSTED_USAGE : \
								CRYPT_CERTINFO_KEYUSAGE,
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* Switch back to the original usage values (before adjustment by 
	   trusted-usage values) because after this point we're performing 
	   consistency checks on the values and need to check all of the bits */
	keyUsage = rawKeyUsage;

	/* Make sure that mutually exclusive flags aren't set (RFC 3279 section 
	   2.3.3) */
	if( ( keyUsage & CRYPT_KEYUSAGE_ENCIPHERONLY ) && \
		( keyUsage & CRYPT_KEYUSAGE_DECIPHERONLY ) )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}

	/* Make sure that the keyUsage flags represent capabilities that the 
	   algorithm is actually capable of.  RFC 2459 included some waffly text
	   about critical vs. non-critical usage, RFC 3280 made this explicit
	   regardless of criticality, although the details were actually moved
	   into RFC 3279, which specifies the algorithms used in PKIX */
	if( ( ( ( keyUsage & KEYUSAGE_CRYPT ) && \
			  !isCryptAlgo( certInfoPtr->publicKeyAlgo ) ) || \
		  ( ( keyUsage & ( KEYUSAGE_SIGN | KEYUSAGE_CA ) ) && \
			  !isSigAlgo( certInfoPtr->publicKeyAlgo ) ) || \
		  ( ( keyUsage & KEYUSAGE_KEYAGREEMENT ) && \
			  !isKeyxAlgo( certInfoPtr->publicKeyAlgo ) ) ) ) 
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
	/* Switch back to the original usage values as above.  This is done here
	   because we only use extKeyUsage at a heightened level of compliance
	   checking */
	extKeyUsage = rawExtKeyUsage;

	/* Mask out any non-relevant usages (e.g. certificate signing, which 
	   doesn't occur in extended key usages and has already been checked 
	   above) */
	keyUsage &= ~USAGE_MASK_NONRELEVANT;
	extKeyUsage &= ~USAGE_MASK_NONRELEVANT;

	/* If we're being asked to check for private-key constraints, check and 
	   enforce the privateKeyUsage attribute if there's one present */
	if( ( flags & CHECKKEY_FLAG_PRIVATEKEY ) && \
		checkAttributePresent( certInfoPtr->attributes,
							   CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD ) )
		{
		const time_t currentTime = getTime();
		time_t certTime;

		if( currentTime <= MIN_TIME_VALUE )
			{
			/* Time is broken, we can't reliably check for expiry times */
			setErrorValues( CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE, 
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		status = getAttributeFieldTime( certInfoPtr->attributes,
										CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE, 
										CRYPT_ATTRIBUTE_NONE, &certTime );
		if( cryptStatusOK( status ) && currentTime < certTime )
			{
			setErrorValues( CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE,
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		status = getAttributeFieldTime( certInfoPtr->attributes,
										CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER, 
										CRYPT_ATTRIBUTE_NONE, &certTime );
		if( cryptStatusOK( status ) && currentTime > certTime )
			{
			setErrorValues( CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER,
							CRYPT_ERRTYPE_CONSTRAINT );
			return( CRYPT_ERROR_INVALID );
			}
		}

	/* If there's no key usage based on extended key usage present or we're 
	   not doing at least partial PKIX checking, there's nothing further to 
	   check */
	if( !extKeyUsage || complianceLevel < CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL )
		return( CRYPT_OK );

	/* If the CA key usages are set then an encryption key usage shouldn't 
	   be set (RFC 3279 section 2.3.1) */
	if( isCA && ( keyUsage & extKeyUsage & KEYUSAGE_CRYPT ) )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If the usage and extended usage are critical (but only if both are 
	   critical, because PKIX says so) make sure that the given usage is 
	   consistent with the required usage (PKIX section 4.2.1.13).  To 
	   perform this check we first check for situations where we *don't* 
	   have to perform the check and only if none of these occur do we 
	   perform the actual check.
	   
	   Checking whether the extended usage is critical is a bit nontrivial, 
	   we have to check each possible extended usage since only one of them 
	   may be present so we check the criticality of the basic key usage 
	   first to allow quick short-circuit evaluation.
	   
	   In addition to the explicit criticality checks we also perform an
	   implicit check based on whether this is a freshly-generated, as-yet-
	   unsigned cryptlib certificate.  This is done for two reasons, firstly 
	   because an unsigned certificate won't have had the criticality flag 
	   set by the signing/encoding process so the extension always appears 
	   non-critical and secondly because we don't want cryptlib to generate 
	   inconsistent certificates, whether the extensions are marked critical 
	   or not (cryptlib always makes the keyUsage critical so at least for 
	   key usage it's no change from the standard behaviour) */
	if( certInfoPtr->certificate != NULL )
		{
		int attributeID;

		/* If there's no critical key usage present we can exit without
		   performing further checks */
		if( !keyUsageCritical )
			return( CRYPT_OK );

		/* If we find an extended key usage and it's non-critical (which 
		   means that all extended usages are non-critical since they're
		   all in the same extension), return */
		for( attributeID = CRYPT_CERTINFO_EXTKEYUSAGE + 1; 
			 attributeID < CRYPT_CERTINFO_NS_CERTTYPE; attributeID++ )
			{
			attributePtr = findAttributeField( certInfoPtr->attributes,
										attributeID, CRYPT_ATTRIBUTE_NONE );
			if( attributePtr != NULL && \
				!checkAttributeProperty( attributePtr, 
										 ATTRIBUTE_PROPERTY_CRITICAL ) )
				return( CRYPT_OK );
			}
		}

	/* Make sure that the extended key usage-based key usage is consistent 
	   with the actual key usage */
	if( ( keyUsage & extKeyUsage ) != extKeyUsage )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_CONSTRAINT );
		return( CRYPT_ERROR_INVALID );
		}

	/* If the encipherOnly or decipherOnly bits are set then the 
	   keyAgreement bit most also be set (PKIX section 4.2.1.3).  Actually 
	   the spec merely says "undefined" but we interpret this to mean that 
	   they should be consistent.  This situation occurs because the 
	   encipher/decipher-only usages were tacked on as modifiers long after 
	   keyAgreement was defined and make it entirely redundant, in the same 
	   way that the CA keyUsages make the basicConstraints CA flag 
	   redundant */
	if( ( keyUsage & ( CRYPT_KEYUSAGE_ENCIPHERONLY | \
					   CRYPT_KEYUSAGE_DECIPHERONLY ) ) && \
		!( keyUsage & CRYPT_KEYUSAGE_KEYAGREEMENT ) )
		{
		setErrorValues( CRYPT_CERTINFO_KEYUSAGE, CRYPT_ERRTYPE_ATTR_VALUE );
		return( CRYPT_ERROR_INVALID );
		}
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */
