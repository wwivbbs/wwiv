/****************************************************************************
*																			*
*					  Certificate Trust Management Routines					*
*						Copyright Peter Gutmann 1998-2015					*
*																			*
****************************************************************************/

/* The following code is actually part of the user rather than certificate
   routines but it pertains to certificates so we include it here.  Trust
   information mutex handling is done in the user object so there are no 
   mutexes required here.

   The interpretation of what represents a "trusted certificate" is somewhat 
   complex and open-ended, it's not clear whether what's being trusted is 
   the key in the certificate, the certificate, or the owner of the 
   certificate (corresponding to subjectKeyIdentifier, issuerAndSerialNumber/
   certHash, or subject DN).  The generally accepted form is to trust the 
   subject so we check for this in the certificate.  The modification for 
   trusting the key is fairly simple to make if required.
   
   Since the trust data sits in memory for extended periods of time, we're
   especially careful to defend against memory corruption issues */

#if defined( INC_ALL )
  #include "cert.h"
  #include "trustmgr_int.h"
  #include "trustmgr.h"
#else
  #include "cert/cert.h"
  #include "cert/trustmgr_int.h"
  #include "cert/trustmgr.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Sanity-check trust information */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN sanityCheckTrustInfo( const TRUST_INFO *trustInfo )
	{
	assert( isReadPtr( trustInfo, sizeof( TRUST_INFO ) ) );

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( trustInfo->certObject ) || \
		!DATAPTR_ISVALID( trustInfo->next ) || \
		!DATAPTR_ISVALID( trustInfo->prev ) )
		{
		DEBUG_PUTS(( "sanityCheckTrustInfo: Safe pointers" ));
		return( FALSE );
		}

	/* Check certificate data */
	if( trustInfo->iCryptCert != CRYPT_ERROR && \
		!isHandleRangeValid( trustInfo->iCryptCert ) )
		{
		DEBUG_PUTS(( "sanityCheckTrustInfo: Certificate object" ));
		return( FALSE );
		}
	if( DATAPTR_ISSET( trustInfo->certObject ) )
		{
		const void *certObjectPtr;

		certObjectPtr = DATAPTR_GET( trustInfo->certObject );
		ENSURES( certObjectPtr != NULL );

		if( trustInfo->certObjectLength < MIN_CERTSIZE || \
			trustInfo->certObjectLength >= MAX_INTLENGTH_SHORT )
			{
			DEBUG_PUTS(( "sanityCheckTrustInfo: Certificate data" ));
			return( FALSE );
			}
		if( checksumData( certObjectPtr, \
						  trustInfo->certObjectLength ) != trustInfo->certChecksum )
			{
			DEBUG_PUTS(( "sanityCheckTrustInfo: Certificate data corruption" ));
			return( FALSE );
			}
		}
	else
		{
		if( !DATAPTR_ISNULL( trustInfo->certObject ) || \
			trustInfo->certObjectLength != 0 || \
			trustInfo->certChecksum != 0 )
			{
			DEBUG_PUTS(( "sanityCheckTrustInfo: Spurious certificate data" ));
			return( FALSE );
			}
		}

	return( TRUE );
	}

/* Get a pointer to the trust information from a generic DATAPTR after 
   checking that it's valid */

CHECK_RETVAL_PTR \
static DATAPTR *getCheckTrustInfo( IN const DATAPTR trustInfo )
	{
	TRUST_INFO_CONTAINER *trustInfoContainer;

	/* Get a pointer to the trust information */
	trustInfoContainer = DATAPTR_GET( trustInfo );
	ENSURES_N( trustInfoContainer != NULL );

	/* Make sure that it's valid */
	if( checksumData( trustInfoContainer->trustInfo, 
					  sizeof( DATAPTR ) * TRUSTINFO_SIZE ) != \
		trustInfoContainer->trustInfoChecksum )
		return( NULL );

	return( trustInfoContainer->trustInfo );
	}

/* Update the checksum on the trust information */

static void updateTrustInfoChecksum( IN const DATAPTR trustInfo )
	{
	TRUST_INFO_CONTAINER *trustInfoContainer;

	/* Get a pointer to the trust information index.  We can't use 
	   getCheckTrustInfo() because we've changed it so the checksum won't
	   validate */
	trustInfoContainer = DATAPTR_GET( trustInfo );
	ENSURES_V( trustInfoContainer != NULL );

	/* Update the trust information checksum */
	trustInfoContainer->trustInfoChecksum = \
					checksumData( trustInfoContainer->trustInfo, 
								  sizeof( DATAPTR ) * TRUSTINFO_SIZE );
	}

/* Extract ID fields from an encoded certificate.  Since this isn't a
   certificate object we have to parse the encoded data to locate the fields
   that we're interested in */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
static int getCertIdInfo( IN_BUFFER( certObjectLength ) const void *certObject, 
						  IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
								const int certObjectLength,
						  OUT_PTR_COND void **subjectDNptrPtr,
						  OUT_LENGTH_SHORT_Z int *subjectDNsizePtr )
	{
	STREAM stream;
	void *subjectDNptr DUMMY_INIT_PTR;
	int subjectDNsize DUMMY_INIT, status;

	assert( isReadPtrDynamic( certObject, certObjectLength ) );
	assert( isWritePtr( subjectDNptrPtr, sizeof( void * ) ) );
	assert( isWritePtr( subjectDNsizePtr, sizeof( int ) ) );

	REQUIRES( certObjectLength >= MIN_CRYPT_OBJECTSIZE && \
			  certObjectLength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*subjectDNptrPtr = NULL;
	*subjectDNsizePtr = 0;

	/* Parse the certificate to locate the start of the encoded subject DN 
	   and certificate extensions (if present) */
	sMemConnect( &stream, certObject, certObjectLength );
	readSequence( &stream, NULL );			/* Outer wrapper */
	readSequence( &stream, NULL );			/* Inner wrapper */
	if( peekTag( &stream ) == MAKE_CTAG( 0 ) )
		readUniversal( &stream );			/* Version */
	readUniversal( &stream );				/* Serial number */
	readUniversal( &stream );				/* Signature algo */
	readUniversal( &stream );				/* Issuer DN */
	status = readUniversal( &stream );		/* Validity */
	if( cryptStatusOK( status ) )
		status = getStreamObjectLength( &stream, &subjectDNsize );
	if( cryptStatusOK( status ) && !isShortIntegerRangeNZ( subjectDNsize ) )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( &stream, &subjectDNptr, subjectDNsize );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	*subjectDNptrPtr = subjectDNptr;
	*subjectDNsizePtr = subjectDNsize;
	status = sSkip( &stream, subjectDNsize, MAX_INTLENGTH_SHORT );/* Subject DN */
#if 0	/* sKID lookup isn't used at present.  Also this code should use the 
		   parsing mechanism from dbx_rd.c to provide better checking */
	const BYTE *extensionPtr;
	int extensionSize = 0;
	status = readUniversal( &stream );		/* Public key */
	if( checkStatusPeekTag( stream, status, tag ) && \
		tag == MAKE_CTAG( 3 ) )
		{
		status = readConstructed( &stream, &extensionSize, 3 );
		if( cryptStatusOK( status ) && \
			!isShortIntegerRangeNZ( extensionSize ) )
			status = CRYPT_ERROR_BADDATA;
		if( cryptStatusOK( status ) )
			{
			extensionPtr = sMemBufPtr( &stream );
			sSkip( &stream, extensionSize, MAX_INTLENGTH_SHORT );
			}
		}
	if( !cryptStatusError( status ) )		/* Signature */
		status = readUniversal( &stream );
#endif /* 0 */
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

#if 0	/* sKID lookup isn't used at present.  Also this code should use the 
		   parsing mechanism from dbx_rd.c to provide better checking */
	/* Look for the subjectKeyID in the extensions.  It's easier to do a 
	   pattern match than to try and parse the extensions */
	subjectKeyIDptr = NULL;
	subjectKeyIDsize = 0;
	LOOP_LARGE( i = 0, i < extensionSize - 64, i++ )
		{
		/* Look for the OID.  This potentially skips two bytes at a time, 
		   but this is safe since the preceding bytes can never contain 
		   either of these two values (they're 0x30, len) */
		if( extensionPtr[ i++ ] != BER_OBJECT_IDENTIFIER || \
			extensionPtr[ i++ ] != 3 )
			continue;
		if( memcmp( extensionPtr + i, "\x55\x1D\x0E", 3 ) )
			continue;
		i += 3;

		/* We've found the OID (with 1.1e-12 error probability), skip the 
		   critical flag if necessary */
		if( extensionPtr[ i ] == BER_BOOLEAN )
			i += 3;

		/* Check for the OCTET STRING and a reasonable length */
		if( extensionPtr[ i++ ] != BER_OCTETSTRING || \
			extensionPtr[ i ] & 0x80 )
			continue;

		/* Extract the key ID */
		if( i + extensionPtr[ i ] <= extensionSize )
			{
			subjectKeyIDsize = extensionPtr[ i++ ];
			subjectKeyIDptr = extensionPtr + i;
			}
		}
	ENSURES( LOOP_BOUND_OK );
#endif /* 0 */

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Retrieve Trusted Certificate Information				*
*																			*
****************************************************************************/

/* Find the trust information entry for a given certificate.  This isn't 
   used within this module but is called from cryptusr.c */

CHECK_RETVAL_PTR \
void *findTrustEntry( IN const DATAPTR trustInfo, 
					  IN_HANDLE const CRYPT_CERTIFICATE iCryptCert,
					  const BOOLEAN getIssuerEntry )
	{
	const DATAPTR *trustInfoIndex;
	const TRUST_INFO *trustInfoCursor;
	DYNBUF nameDB;
	BYTE sHash[ HASH_DATA_SIZE + 8 ];
	BOOLEAN nameHashed = FALSE;
	int sCheck, trustInfoEntry, status, LOOP_ITERATOR;

	REQUIRES_N( DATAPTR_ISSET( trustInfo ) );
	REQUIRES_N( isHandleRangeValid( iCryptCert ) );
	REQUIRES_N( getIssuerEntry == TRUE || getIssuerEntry == FALSE );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES_N( trustInfoIndex != NULL );

	/* If we're trying to get a trusted issuer certificate and we're already 
	   at a self-signed (CA root) certificate, don't return it.  This check 
	   is necessary because self-signed certificates have issuer name == 
	   subject name so once we get to a self-signed certificate's subject DN 
	   an attempt to fetch its issuer would just repeatedly fetch the same 
	   certificate */
	if( getIssuerEntry )
		{
		BOOLEAN_INT selfSigned;

		status = krnlSendMessage( iCryptCert, IMESSAGE_GETATTRIBUTE, 
								  &selfSigned, CRYPT_CERTINFO_SELFSIGNED );
		if( cryptStatusError( status ) || selfSigned == TRUE )
			return( NULL );
		}

	/* Set up the information needed to find the trusted certificate */
	status = dynCreate( &nameDB, iCryptCert, getIssuerEntry ? \
						CRYPT_IATTRIBUTE_ISSUER : CRYPT_IATTRIBUTE_SUBJECT );
	if( cryptStatusError( status ) )
		return( NULL );
	sCheck = checksumData( dynData( nameDB ), dynLength( nameDB ) );
	trustInfoEntry = sCheck & ( TRUSTINFO_SIZE - 1 );
	ENSURES_N( trustInfoEntry >= 0 && trustInfoEntry < TRUSTINFO_SIZE );

	/* Check to see whether something with the requested DN is present */
	LOOP_MED( trustInfoCursor = DATAPTR_GET( trustInfoIndex[ trustInfoEntry ] ), 
			  trustInfoCursor != NULL,
			  trustInfoCursor = DATAPTR_GET( trustInfoCursor->next ) )
		{
		REQUIRES_N( sanityCheckTrustInfo( trustInfoCursor ) );

		/* Perform a quick check using a checksum of the name to weed out
		   most entries */
		if( trustInfoCursor->sCheck == sCheck )
			{
			if( !nameHashed )
				{
				hashData( sHash, HASH_DATA_SIZE, dynData( nameDB ), 
						  dynLength( nameDB ) );
				nameHashed = TRUE;
				}
			if( !memcmp( trustInfoCursor->sHash, sHash, HASH_DATA_SIZE ) )
				{
				dynDestroy( &nameDB );
				return( ( TRUST_INFO * ) trustInfoCursor );
				}
			}
		}
	ENSURES_N( LOOP_BOUND_OK );
	dynDestroy( &nameDB );

	return( NULL );
	}

/* Retrieve trusted certificates */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getTrustedCert( INOUT TYPECAST( TRUST_INFO * ) void *trustInfoPtr,
					OUT_HANDLE_OPT CRYPT_CERTIFICATE *iCertificate )
	{
	TRUST_INFO *trustInfo = trustInfoPtr;
	int status;

	assert( isWritePtr( trustInfoPtr, sizeof( TRUST_INFO ) ) );

	REQUIRES( sanityCheckTrustInfo( trustInfo ) );

	/* Clear return value */
	*iCertificate = CRYPT_ERROR;

	/* If the certificate hasn't already been instantiated yet, do so now */
	if( trustInfo->iCryptCert == CRYPT_ERROR )
		{
		MESSAGE_CREATEOBJECT_INFO createInfo;
		void *certObjectPtr;

		certObjectPtr = DATAPTR_GET( trustInfo->certObject );
		ENSURES( certObjectPtr != NULL );

		/* Instantiate the certificate */
		setMessageCreateObjectIndirectInfo( &createInfo, certObjectPtr,
											trustInfo->certObjectLength,
											CRYPT_CERTTYPE_CERTIFICATE );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
								  IMESSAGE_DEV_CREATEOBJECT_INDIRECT,
								  &createInfo, OBJECT_TYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			{
			/* If we couldn't instantiate the certificate and it's due to a
			   problem with the certificate rather than circumstances beyond
			   our control, warn about the issue since a (supposedly) known-
			   good certificate shouldn't cause problems on import */
			if( status != CRYPT_ERROR_MEMORY )
				{
				DEBUG_DIAG(( "Couldn't instantiate trusted certificate" ));
				assert( DEBUG_WARN );
				}
			return( status );
			}

		/* The certificate was successfully instantiated, free its stored 
		   encoded form */
		zeroise( certObjectPtr, trustInfo->certObjectLength );
		clFree( "getTrustedCert", certObjectPtr );
		DATAPTR_SET( trustInfo->certObject, NULL );
		trustInfo->certObjectLength = 0;
		trustInfo->certChecksum = 0;
		trustInfo->iCryptCert = createInfo.cryptHandle;
		}

	/* Return the trusted certificate */
	*iCertificate = trustInfo->iCryptCert;

	return( CRYPT_OK );
	}

CHECK_RETVAL_BOOL \
BOOLEAN trustedCertsPresent( IN const DATAPTR trustInfo )
	{
	const DATAPTR *trustInfoIndex;
	int i, LOOP_ITERATOR;

	REQUIRES( DATAPTR_ISSET( trustInfo ) );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES_B( trustInfoIndex != NULL );

	LOOP_EXT( i = 0, i < TRUSTINFO_SIZE, i++, TRUSTINFO_SIZE + 1 )
		{
		if( DATAPTR_ISSET( trustInfoIndex[ i ] ) )
			return( TRUE );
		}
	ENSURES_B( LOOP_BOUND_OK );

	return( FALSE );
	}

CHECK_RETVAL \
int enumTrustedCerts( IN const DATAPTR trustInfo, 
					  IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCtl,
					  IN_HANDLE_OPT const CRYPT_KEYSET iCryptKeyset )
	{
	const DATAPTR *trustInfoIndex;
	int i, LOOP_ITERATOR;

	REQUIRES( DATAPTR_ISSET( trustInfo ) );
	REQUIRES( ( iCryptCtl == CRYPT_UNUSED && \
				isHandleRangeValid( iCryptKeyset ) ) || \
			  ( isHandleRangeValid( iCryptCtl ) && \
				iCryptKeyset == CRYPT_UNUSED ) );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES_B( trustInfoIndex != NULL );

	/* Send each trusted certificate to the given object, either a 
	   certificate trust list or a keyset */
	LOOP_EXT( i = 0, i < TRUSTINFO_SIZE, i++, TRUSTINFO_SIZE + 1 )
		{
		TRUST_INFO *trustInfoCursor;
		int LOOP_ITERATOR_ALT;

		LOOP_MED_ALT( trustInfoCursor = DATAPTR_GET( trustInfoIndex[ i ] ), 
					  trustInfoCursor != NULL,
					  trustInfoCursor = DATAPTR_GET( trustInfoCursor->next ) )	
			{
			CRYPT_CERTIFICATE iCryptCert;
			int status;

			REQUIRES( sanityCheckTrustInfo( trustInfoCursor ) );

			status = getTrustedCert( trustInfoCursor, &iCryptCert );
			if( cryptStatusError( status ) )
				return( status );
			if( iCryptCtl != CRYPT_UNUSED )
				{
				/* We're sending trusted certificates to a certificate trust 
				   list */
				status = krnlSendMessage( iCryptCtl, IMESSAGE_SETATTRIBUTE,
										  ( MESSAGE_CAST ) &iCryptCert,
										  CRYPT_IATTRIBUTE_CERTCOLLECTION );
				}
			else
				{
				MESSAGE_KEYMGMT_INFO setkeyInfo;

				/* We're sending trusted certificates to a keyset */
				setMessageKeymgmtInfo( &setkeyInfo, CRYPT_KEYID_NONE, NULL, 0,
									   NULL, 0, KEYMGMT_FLAG_NONE );
				setkeyInfo.cryptHandle = iCryptCert;
				status = krnlSendMessage( iCryptKeyset, IMESSAGE_KEY_SETKEY, 
										  &setkeyInfo, 
										  KEYMGMT_ITEM_PUBLICKEY );
				}
			if( cryptStatusError( status ) )
				return( status );
			}
		ENSURES( LOOP_BOUND_OK_ALT );
		}
	ENSURES( LOOP_BOUND_OK );
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*					Add/Update Trusted Certificate Information				*
*																			*
****************************************************************************/

/* Add and delete a trust entry */

CHECK_RETVAL \
static int addEntry( IN const DATAPTR trustInfo, 
					 IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert, 
					 IN_BUFFER_OPT( certObjectLength ) const void *certObject, 
					 IN_LENGTH_SHORT_Z const int certObjectLength )
	{
	DATAPTR *trustInfoIndex;
	TRUST_INFO *newElement, *trustInfoCursor, *trustInfoLast;
	BYTE sHash[ HASH_DATA_SIZE + 8 ];
	BOOLEAN recreateCert = FALSE;
	int sCheck, trustInfoEntry, status, LOOP_ITERATOR;

	assert( ( certObject == NULL && certObjectLength == 0 && \
			  isHandleRangeValid( iCryptCert ) ) || \
			( isReadPtrDynamic( certObject, certObjectLength ) && \
			  iCryptCert == CRYPT_UNUSED ) );

	REQUIRES( ( certObject == NULL && certObjectLength == 0 && \
				isHandleRangeValid( iCryptCert ) ) || \
			  ( certObject != NULL && \
			    certObjectLength >= MIN_CRYPT_OBJECTSIZE && \
				certObjectLength < MAX_INTLENGTH_SHORT && \
				iCryptCert == CRYPT_UNUSED ) );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES( trustInfoIndex != NULL );

	/* If we're adding a certificate, check whether it has a context 
	   attached and if it does, whether it's a public-key context.  If 
	   there's no context attached (it's a data-only certificate) or the 
	   attached context is a private-key context (which we don't want to 
	   leave hanging around in memory, or which could be in a removable 
	   crypto device) we don't try and use the certificate but instead add 
	   the certificate data and then later re-instantiate a new certificate 
	   with attached public-key context if required */
	if( certObject == NULL )
		{
		CRYPT_CONTEXT iCryptContext;

		status = krnlSendMessage( iCryptCert, IMESSAGE_GETDEPENDENT,
								  &iCryptContext, OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			{
			/* There's no context associated with this certificate, we'll 
			   have to recreate it later */
			recreateCert = TRUE;
			}
		else
			{
			if( checkContextCapability( iCryptContext, 
										MESSAGE_CHECK_PKC_PRIVATE ) )
				{
				/* The context associated with the certificate is a private-
				   key context, recreate it later as a public-key context */
				recreateCert = TRUE;
				}
			}
		}

	/* Get the ID information for the certificate */
	if( certObject == NULL )
		{
		DYNBUF subjectDB;

		/* Generate the checksum and hash of the certificate object's 
		   subject name and key ID */
		status = dynCreate( &subjectDB, iCryptCert, 
							CRYPT_IATTRIBUTE_SUBJECT );
		if( cryptStatusError( status ) )
			return( status );
		sCheck = checksumData( dynData( subjectDB ), 
							   dynLength( subjectDB ) );
		hashData( sHash, HASH_DATA_SIZE, dynData( subjectDB ), 
				  dynLength( subjectDB ) );
#if 0	/* sKID lookup isn't used at present */
		DYNBUF subjectKeyDB;
		status = dynCreate( &subjectKeyDB, iCryptCert, 
							CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER );
		if( cryptStatusOK( status ) )
			{
			kCheck = checksumData( dynData( subjectKeyDB ), 
								   dynLength( subjectKeyDB ) );
			hashData( kHash, HASH_DATA_SIZE, dynData( subjectKeyDB ), 
					  dynLength( subjectKeyDB ) );
			dynDestroy( &subjectKeyDB );
			}
#endif /* 0 */
		dynDestroy( &subjectDB );
		}
	else
		{
		void *subjectDNptr;
		int subjectDNsize;

		/* Get the ID information from the encoded certificate */
		status = getCertIdInfo( certObject, certObjectLength, 
								&subjectDNptr, &subjectDNsize );
		ENSURES( cryptStatusOK( status ) );
		ANALYSER_HINT( subjectDNptr != NULL );

		/* Generate the checksum and hash of the encoded certificate's 
		   subject name and key ID */
		sCheck = checksumData( subjectDNptr, subjectDNsize );
		hashData( sHash, HASH_DATA_SIZE, subjectDNptr, subjectDNsize );
#if 0	/* sKID lookup isn't used at present */
		kCheck = checksumData( subjectKeyIDptr, subjectKeyIDsize );
		hashData( kHash, HASH_DATA_SIZE, subjectKeyIDptr, subjectKeyIDsize );
#endif /* 0 */
		}

	/* Find the insertion point and make sure that this entry isn't already 
	   present */
	trustInfoEntry = sCheck & ( TRUSTINFO_SIZE - 1 );
	ENSURES( trustInfoEntry >= 0 && trustInfoEntry < TRUSTINFO_SIZE );
	LOOP_MED( trustInfoCursor = trustInfoLast = \
					DATAPTR_GET( trustInfoIndex[ trustInfoEntry ] ), 
			  trustInfoCursor != NULL,
			  trustInfoCursor = DATAPTR_GET( trustInfoCursor->next ) )
		{
		REQUIRES( sanityCheckTrustInfo( trustInfoCursor ) );

		/* Perform a quick check using a checksum of the name to weed out
		   most entries */
		if( trustInfoCursor->sCheck == sCheck && \
			!memcmp( trustInfoCursor->sHash, sHash, HASH_DATA_SIZE ) )
			return( CRYPT_ERROR_DUPLICATE );
		trustInfoLast = trustInfoCursor;
		}
	ENSURES( LOOP_BOUND_OK );
	trustInfoCursor = trustInfoLast;

	/* Allocate memory for the new element and copy the information across */
	if( ( newElement  = ( TRUST_INFO * ) \
				clAlloc( "addEntry", sizeof( TRUST_INFO ) ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	memset( newElement, 0, sizeof( TRUST_INFO ) );
	newElement->sCheck = sCheck;
	memcpy( newElement->sHash, sHash, HASH_DATA_SIZE );
	DATAPTR_SET( newElement->certObject, NULL );
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );
	if( certObject != NULL || recreateCert )
		{
		DYNBUF certDB;
		void *objectPtr;
		int objectLength = certObjectLength;

		/* If we're using the data from an existing certificate object we 
		   need to extract the encoded data */
		if( recreateCert )
			{
			/* Get the encoded certificate */
			status = dynCreateCert( &certDB, iCryptCert, 
									CRYPT_CERTFORMAT_CERTIFICATE );
			if( cryptStatusError( status ) )
				{
				clFree( "addEntry", newElement );
				return( status );
				}
			certObject = dynData( certDB );
			objectLength = dynLength( certDB );
			}

		/* Remember the trusted certificate data for later use */
		REQUIRES( rangeCheck( objectLength, 1, MAX_INTLENGTH_SHORT ) );
		if( ( objectPtr = clAlloc( "addEntry", objectLength ) ) == NULL )
			{
			clFree( "addEntry", newElement );
			if( recreateCert )
				dynDestroy( &certDB );
			return( CRYPT_ERROR_MEMORY );
			}
		memcpy( objectPtr, certObject, objectLength );
		DATAPTR_SET( newElement->certObject, objectPtr );
		newElement->certObjectLength = objectLength;
		newElement->certChecksum = checksumData( objectPtr, objectLength );
		newElement->iCryptCert = CRYPT_ERROR;

		/* Clean up */
		if( recreateCert )
			dynDestroy( &certDB );
		}
	else
		{
		/* The trusted key exists as a standard certificate with a public-
		   key context attached, remember it for later */
		krnlSendNotifier( iCryptCert, IMESSAGE_INCREFCOUNT );
		newElement->iCryptCert = iCryptCert;
		}
	ENSURES( sanityCheckTrustInfo( newElement ) );

	/* Add the new entry to the list */
	insertDoubleListElement( &trustInfoIndex[ trustInfoEntry ], 
							 trustInfoCursor, newElement, TRUST_INFO );
	updateTrustInfoChecksum( trustInfo );

	return( CRYPT_OK );
	}

CHECK_RETVAL \
int addTrustEntry( IN const DATAPTR trustInfo, 
				   IN_HANDLE_OPT const CRYPT_CERTIFICATE iCryptCert,
				   IN_BUFFER_OPT( certObjectLength ) const void *certObject, 
				   IN_LENGTH_SHORT_Z const int certObjectLength,
				   const BOOLEAN addSingleCert )
	{
	BOOLEAN itemAdded = FALSE;
	int status;

	assert( ( certObject == NULL && certObjectLength == 0 && \
			  isHandleRangeValid( iCryptCert ) ) || \
			( isReadPtrDynamic( certObject, certObjectLength ) && \
			  iCryptCert == CRYPT_UNUSED ) );

	REQUIRES( DATAPTR_ISSET( trustInfo ) );
	REQUIRES( ( certObject == NULL && certObjectLength == 0 && \
				isHandleRangeValid( iCryptCert ) ) || \
			  ( certObject != NULL && \
			    certObjectLength >= MIN_CRYPT_OBJECTSIZE && \
				certObjectLength < MAX_INTLENGTH_SHORT && \
				iCryptCert == CRYPT_UNUSED ) );
	REQUIRES( addSingleCert == TRUE || addSingleCert == FALSE );

	/* If we're adding encoded certificate data then we can add it 
	   directly */
	if( certObject != NULL )
		{
		return( addEntry( trustInfo, CRYPT_UNUSED, certObject, 
						  certObjectLength ) );
		}

	/* Add the certificate/each certificate in the trust list */
	status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	if( addSingleCert )
		{
		status = addEntry( trustInfo, iCryptCert, NULL, 0 );
		if( cryptStatusOK( status ) )
			itemAdded = TRUE;
		}
	else
		{
		int loopStatus, LOOP_ITERATOR;

		/* It's a trust list, move to the start of the list and add each
		   certificate in it */
		status = krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_CURSORFIRST,
								  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
		if( cryptStatusError( status ) )
			{
			( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
									  MESSAGE_VALUE_FALSE, 
									  CRYPT_IATTRIBUTE_LOCKED );
			return( status );
			}
		LOOP_MED( loopStatus = CRYPT_OK, cryptStatusOK( loopStatus ),
				  loopStatus = krnlSendMessage( iCryptCert, 
									IMESSAGE_SETATTRIBUTE,
									MESSAGE_VALUE_CURSORNEXT,
									CRYPT_CERTINFO_CURRENT_CERTIFICATE ) )
			{
			/* An item being added may already be present, however we can't 
			   fail immediately because what's being added may be a chain 
			   containing further certificates so we keep track of whether 
			   we've successfully added at least one item and clear data 
			   duplicate errors */
			status = addEntry( trustInfo, iCryptCert, NULL, 0 );
			if( cryptStatusError( status ) )
				{
				if( status != CRYPT_ERROR_DUPLICATE )
					break;
				status = CRYPT_OK;
				}
			else
				itemAdded = TRUE;
			}
		ENSURES( LOOP_BOUND_OK );
		}
	( void ) krnlSendMessage( iCryptCert, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( status );
	if( !itemAdded )
		{
		/* We reached the end of the certificate chain without finding 
		   anything that we could add, return a data duplicate error */
		return( CRYPT_ERROR_DUPLICATE ); 
		}

	return( CRYPT_OK );
	}

RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deleteTrustEntry( IN const DATAPTR trustInfo, 
					  IN TYPECAST( TRUST_INFO * ) void *entryToDeletePtr )
	{
	DATAPTR *trustInfoIndex;
	TRUST_INFO *entryToDelete = ( TRUST_INFO * ) entryToDeletePtr;
	int trustInfoEntry;

	assert( isWritePtr( entryToDeletePtr, sizeof( TRUST_INFO ) ) );

	REQUIRES( DATAPTR_ISSET( trustInfo ) );
	REQUIRES( sanityCheckTrustInfo( entryToDelete ) );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES( trustInfoIndex != NULL );

	trustInfoEntry = entryToDelete->sCheck & ( TRUSTINFO_SIZE - 1 );
	REQUIRES( trustInfoEntry >= 0 && trustInfoEntry < TRUSTINFO_SIZE );

	/* Free the trust information entry contents */
	if( entryToDelete->iCryptCert != CRYPT_ERROR )
		krnlSendNotifier( entryToDelete->iCryptCert, IMESSAGE_DECREFCOUNT );
	if( DATAPTR_ISSET( entryToDelete->certObject ) )
		{
		void *certObjectPtr = DATAPTR_GET( entryToDelete->certObject );

		ENSURES( certObjectPtr != NULL );
		zeroise( certObjectPtr, entryToDelete->certObjectLength );
		clFree( "deleteTrustEntry", certObjectPtr );
		}

	/* Remove the trust entry from the list */
	deleteDoubleListElement( &trustInfoIndex[ trustInfoEntry ], entryToDelete, 
							 TRUST_INFO );
	updateTrustInfoChecksum( trustInfo );

	/* Clear all data in the trust entry and free the memory */
	memset( entryToDelete, 0, sizeof( TRUST_INFO ) );
	clFree( "deleteTrustEntry", entryToDelete );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*				Init/Shut down Trusted Certificate Information				*
*																			*
****************************************************************************/

/* Initialise and shut down the trust information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initTrustInfo( OUT_DATAPTR DATAPTR *trustInfoPtr )
	{
	TRUST_INFO_CONTAINER *trustInfoContainer;
	int i, LOOP_ITERATOR;

	assert( isWritePtr( trustInfoPtr, sizeof( DATAPTR ) ) );

	/* Clear return value */
	DATAPTR_SET_PTR( trustInfoPtr, NULL );

	/* Initialise the trust information table */
	trustInfoContainer = getTrustMgrStorage();
	memset( trustInfoContainer, 0, sizeof( TRUST_INFO_CONTAINER ) );
	LOOP_EXT( i = 0, i < TRUSTINFO_SIZE, i++, TRUSTINFO_SIZE + 1 )
		{
		DATAPTR_SET( trustInfoContainer->trustInfo[ i ], NULL );
		}
	ENSURES( LOOP_BOUND_OK );
	DATAPTR_SET_PTR( trustInfoPtr, trustInfoContainer );
	updateTrustInfoChecksum( *trustInfoPtr );

	ENSURES( getCheckTrustInfo( *trustInfoPtr ) != NULL );

	return( CRYPT_OK );
	}

void endTrustInfo( IN const DATAPTR trustInfo )
	{
	DATAPTR *trustInfoIndex;
	int i, LOOP_ITERATOR;

	REQUIRES_V( DATAPTR_ISSET( trustInfo ) );

	/* Check that the trust information is valid and get a pointer to the
	   trust information index */
	trustInfoIndex = getCheckTrustInfo( trustInfo );
	ENSURES_V( trustInfoIndex != NULL );

	/* Destroy the chain of items at each table position */
	LOOP_EXT( i = 0, i < TRUSTINFO_SIZE, i++, TRUSTINFO_SIZE + 1 )
		{
		TRUST_INFO *trustInfoCursor;

		/* Destroy any items in the list */
		LOOP_MED_INITCHECK( trustInfoCursor = DATAPTR_GET( trustInfoIndex[ i ] ), 
							trustInfoCursor != NULL )
			{
			TRUST_INFO *itemToFree = trustInfoCursor;

			trustInfoCursor = DATAPTR_GET( trustInfoCursor->next );
			deleteTrustEntry( trustInfo, itemToFree );
			}
		ENSURES_V( LOOP_BOUND_OK );

		/* Clear the list head */
		DATAPTR_SET( trustInfoIndex[ i ], NULL );
		}
	ENSURES_V( LOOP_BOUND_OK );

	updateTrustInfoChecksum( trustInfo );
	}
#endif /* USE_CERTIFICATES */
