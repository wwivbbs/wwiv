/****************************************************************************
*																			*
*						cryptlib PKCS #12 Read Routines						*
*						Copyright Peter Gutmann 1997-2010					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs12.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs12.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS12

/* OID information used to read a PKCS #12 keyset */

static const CMS_CONTENT_INFO FAR_BSS oidInfoEncryptedData = { 0, 2 };

static const FAR_BSS OID_INFO keyDataOIDinfo[] = {
	{ OID_CMS_ENCRYPTEDDATA, TRUE, &oidInfoEncryptedData },
	{ OID_CMS_DATA, FALSE },
	{ NULL, 0 }, { NULL, 0 }
	};

/* OID information used to read decrypted PKCS #12 objects */

static const FAR_BSS OID_INFO certBagOIDinfo[] = {
	{ OID_PKCS12_CERTBAG, 0 },
	{ NULL, 0 }, { NULL, 0 }
	};
static const FAR_BSS OID_INFO certOIDinfo[] = {
	{ OID_PKCS9_X509CERTIFICATE, 0 },
	{ NULL, 0 }, { NULL, 0 }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Copy PKCS #12 object information.  If there's already content present in 
   the destination information then we copy in additional information, for
   example to augment existing private-key data with an associated 
   certificate or vice-versa */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void copyObjectInfo( INOUT PKCS12_INFO *destPkcs12Info, 
							const PKCS12_INFO *srcPkcs12Info,
							const BOOLEAN isCertificate )
	{
	assert( isWritePtr( destPkcs12Info, sizeof( PKCS12_INFO ) ) );
	assert( isReadPtr( srcPkcs12Info, sizeof( PKCS12_INFO ) ) );

	destPkcs12Info->flags |= srcPkcs12Info->flags;
	if( isCertificate )
		{
		memcpy( &destPkcs12Info->certInfo, &srcPkcs12Info->certInfo, 
				sizeof( PKCS12_OBJECT_INFO ) );
		}
	else
		{
		memcpy( &destPkcs12Info->keyInfo, &srcPkcs12Info->keyInfo, 
				sizeof( PKCS12_OBJECT_INFO ) );
		}
	if( destPkcs12Info->labelLength <= 0 && \
		srcPkcs12Info->labelLength > 0 )
		{
		memcpy( destPkcs12Info->label, srcPkcs12Info->label,
				srcPkcs12Info->labelLength );
		destPkcs12Info->labelLength = srcPkcs12Info->labelLength;
		}
	if( destPkcs12Info->idLength <= 0 && \
		srcPkcs12Info->idLength > 0 )
		{
		memcpy( destPkcs12Info->id, srcPkcs12Info->id,
				srcPkcs12Info->idLength );
		destPkcs12Info->idLength = srcPkcs12Info->idLength;
		}
	}

/* PKCS #12's lack of useful indexing information makes it extremely 
   difficult to reliably track and match up object types like private keys
   and certificates.  The PKCS #15 code simply does a findEntry() to find 
   the entry that matches a newly-read object and attaches it to whatever's 
   already present for an existing entry if required (so for example it'd 
   attach a certificate to a previously-read private key), however with PKCS 
   #12 there's no reliable way to do this since entries may or may not have 
   ID information attached, which makes it impossible to reliably attach 
   keys to certificates.

   To deal with this as best we can, we apply the following strategy:

	1. For the first item read, we save it as is in the zero-th position
	   (the item may or may not have an ID attached, typically if it's an
	   encrypted certificate then it won't, if it's a private key then it
	   will).

	2. For subsequent items read, if there's no ID information present then 
	   we assume that it's attached to the previously-read first entry 
	   provided that it's compatible with it, for example an ID-less 
	   encrypted certificate attached to an encrypted key.  The "no ID
	   information present" can mean either that the currently-read item has 
	   no ID information or that the first item has no ID information.

	3. If there's ID information attached, we handle it as we would for a 
	   PKCS #15 item */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int findObjectEntryLocation( OUT_PTR_COND PKCS12_INFO **pkcs12infoPtrPtr, 
									IN_ARRAY( maxNoPkcs12objects ) \
										const PKCS12_INFO *pkcs12info, 
									IN_LENGTH_SHORT const int maxNoPkcs12objects, 
									IN_BUFFER_OPT( idLength ) const void *id, 
									IN_LENGTH_KEYID_Z const int idLength,
									IN_FLAGS( PKCS12 ) const int flags )

	{
	const PKCS12_INFO *pkcs12firstItem = &pkcs12info[ 0 ];
	const PKCS12_INFO *pkcs12infoPtr;
	int index;

	assert( isReadPtr( pkcs12infoPtrPtr, sizeof( PKCS12_INFO * ) ) );
	assert( isReadPtr( pkcs12info, sizeof( PKCS12_INFO ) * \
								   maxNoPkcs12objects ) );
	assert( ( id == NULL && idLength == 0 ) || \
			isReadPtr( id, idLength ) );

	REQUIRES( maxNoPkcs12objects >= 1 && \
			  maxNoPkcs12objects < MAX_INTLENGTH_SHORT );
	REQUIRES( ( id == NULL && idLength == 0 ) || \
			  ( id != NULL && \
				idLength > 0 && idLength < MAX_ATTRIBUTE_SIZE ) );

	/* Clear return value */
	*pkcs12infoPtrPtr = NULL;

	/* If this is the first entry being added, just add it as is */
	if( pkcs12FindFreeEntry( pkcs12info, maxNoPkcs12objects, \
							 &index ) != NULL && index == 0 )
		{
		*pkcs12infoPtrPtr = ( PKCS12_INFO * ) pkcs12firstItem;

		return( CRYPT_OK );
		}

	/* If there's no ID information present, try and find an appropriate 
	   existing item to attach it to.  This can occur in two variations,
	   either the existing entry is EncryptedData containing a certificate 
	   with no ID information and the newly-read item is a private key, or 
	   the existing item is a private key with ID information and the newly-
	   read item is EncryptedData containing a certificate with no ID 
	   information */
	if( ( pkcs12firstItem->labelLength == 0 && \
		  pkcs12firstItem->idLength == 0 ) || id == NULL )
		{
		int i;

		/* If the combination of items isn't an encrypted certificate (which 
		   has no ID data associated with it since it's EncryptedData) rather 
		   than a non-encrypted certificate or a private key (which is Data 
		   and should have ID data associated with it) then we can't process 
		   it */
		if( !( ( pkcs12firstItem->flags == PKCS12_FLAG_ENCCERT && \
				 flags == PKCS12_FLAG_PRIVKEY ) || \
			   ( pkcs12firstItem->flags == PKCS12_FLAG_PRIVKEY && \
				 flags == PKCS12_FLAG_ENCCERT ) ) )
			return( CRYPT_ERROR_BADDATA );

		pkcs12infoPtr = NULL;
		for( i = 0; i < maxNoPkcs12objects && \
					i < FAILSAFE_ITERATIONS_MED; i++ )
			{
			/* If this entry isn't in use, continue */
			if( pkcs12info[ i ].flags == PKCS12_FLAG_NONE )
				continue;

			/* If we've already found a matching entry then finding a second 
			   one is ambiguous since we don't know know which one to 
			   associate the newly-read no-ID entry with */
			if( pkcs12infoPtr != NULL )
				return( CRYPT_ERROR_DUPLICATE );

			/* We've found a potential location to add the ID-less entry */
			pkcs12infoPtr = &pkcs12info[ i ];
			}
		ENSURES( i < FAILSAFE_ITERATIONS_MED );
		ENSURES( pkcs12infoPtr != NULL )
				 /* There's always a zero-th entry present to act as the 
					known elephant in Cairo */

		*pkcs12infoPtrPtr = ( PKCS12_INFO * ) pkcs12infoPtr;

		return( CRYPT_OK );
		}

	/* There's an ID present, try and find a matching existing entry for 
	   it */
	pkcs12infoPtr = pkcs12FindEntry( pkcs12info, maxNoPkcs12objects, 
									 CRYPT_IKEYID_KEYID, id, idLength,
									 FALSE );
	if( pkcs12infoPtr != NULL )
		{
		*pkcs12infoPtrPtr = ( PKCS12_INFO * ) pkcs12infoPtr;

		return( CRYPT_OK );
		}

	/* This personality isn't present yet, find out where we can add the 
	   object data */
	pkcs12infoPtr = pkcs12FindFreeEntry( pkcs12info, maxNoPkcs12objects, 
										 NULL );
	if( pkcs12infoPtr == NULL )
		return( CRYPT_ERROR_OVERFLOW );
	*pkcs12infoPtrPtr = ( PKCS12_INFO * ) pkcs12infoPtr;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Import Keys/Certificates						*
*																			*
****************************************************************************/

/* Unwrap and import an encrypted certificate */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5, 7 ) ) \
static int importCertificate( const PKCS12_OBJECT_INFO *certObjectInfo,
							  IN_HANDLE const CRYPT_USER cryptOwner,
							  IN_BUFFER( passwordLen ) const void *password,
							  IN_LENGTH_TEXT const int passwordLen,
							  INOUT_BUFFER_FIXED( certObjectDataLen ) \
									void *certObjectData,
							  IN_LENGTH_SHORT const int certObjectDataLen,
							  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iDataCert )
	{
	CRYPT_CONTEXT iWrapContext;
	STREAM stream;
	long length;
	int certDataSize, status;

	assert( isReadPtr( certObjectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );
	assert( isReadPtr( password, passwordLen ) );
	assert( isWritePtr( certObjectData, certObjectDataLen ) );
	assert( isWritePtr( iDataCert, sizeof( CRYPT_CERTIFICATE ) ) );

	REQUIRES( cryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( cryptOwner ) );
	REQUIRES( passwordLen >= MIN_NAME_LENGTH && \
			  passwordLen <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( certObjectDataLen > MIN_OBJECT_SIZE && \
			  certObjectDataLen < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*iDataCert = CRYPT_ERROR;

	/* Create the wrap context used to decrypt the public certificate data */
	status = createPkcs12KeyWrapContext( ( PKCS12_OBJECT_INFO * ) certObjectInfo, 
										 cryptOwner, password, passwordLen, 
										 &iWrapContext, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Decrypt the certificate */
	status = krnlSendMessage( iWrapContext, IMESSAGE_CTX_DECRYPT, 
							  certObjectData, certObjectDataLen );
	krnlSendNotifier( iWrapContext, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );

	/* Make sure that the decryption succeeded.  We have to be a bit careful
	   here because there are so many garbled certificates used with the
	   equally-garbled PKCS #12 format that an invalid object doesn't
	   necessarily imply that an incorrect decryption key was used.  To 
	   avoid false positives, if we get an invalid encoding we try and read 
	   the outer layer of wrapping around the certificate object, and if 
	   that succeeds then it's a valid decrypt of a garbled certificate 
	   rather than an invalid key leading to an invalid certificate */
	status = length = checkObjectEncoding( certObjectData, 
										   certObjectDataLen );
	if( cryptStatusError( status ) )
		{
		sMemConnect( &stream, certObjectData, certObjectDataLen );
		readSequence( &stream, NULL );
		status = readSequence( &stream, NULL );
		sMemDisconnect( &stream );
		return( cryptStatusError( status ) ? \
				CRYPT_ERROR_WRONGKEY : CRYPT_ERROR_BADDATA );
		}
	certDataSize = length;

	/* Import the certificate as a data-only certificate.  At this point we 
	   have two redundant CMS headers, one within the other, with the nested 
	   inner header of the outer CMS header being the start of the inner CMS 
	   header.  To handle this we read the outer CMS header with the 
	   READCMS_FLAG_WRAPPERONLY flag set to avoid reading the start of the
	   inner header, which is then read by the second readCMSheader() 
	   call */
	sMemConnect( &stream, certObjectData, certDataSize );
	readSequence( &stream, NULL );
	status = readCMSheader( &stream, certBagOIDinfo, 
							FAILSAFE_ARRAYSIZE( certBagOIDinfo, OID_INFO ), 
							NULL, READCMS_FLAG_WRAPPERONLY );
	if( cryptStatusOK( status ) )
		{
		status = readCMSheader( &stream, certOIDinfo, 
								FAILSAFE_ARRAYSIZE( certOIDinfo, OID_INFO ), 
								&length, READCMS_FLAG_INNERHEADER | \
										 READCMS_FLAG_DEFINITELENGTH );
		}
	if( cryptStatusOK( status ) && \
		( length < MIN_OBJECT_SIZE || length > MAX_INTLENGTH_SHORT ) )
		status = CRYPT_ERROR_BADDATA;
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	status = importCertFromStream( &stream, iDataCert, cryptOwner,
								   CRYPT_CERTTYPE_CERTIFICATE, 
								   ( int ) length, 
								   KEYMGMT_FLAG_DATAONLY_CERT );
	sMemDisconnect( &stream );

	return( status );
	}

/* Import an encrypted private key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 6 ) ) \
static int importPrivateKey( const PKCS12_OBJECT_INFO *keyObjectInfo,
							 IN_HANDLE const CRYPT_USER cryptOwner,
							 IN_HANDLE CRYPT_CONTEXT iPrivKeyContext,
							 IN_BUFFER( passwordLen ) const void *password,
							 IN_LENGTH_TEXT const int passwordLen,
							 IN_BUFFER( keyObjectDataLen ) const void *keyObjectData,
							 IN_LENGTH_SHORT const int keyObjectDataLen,
							 IN_BUFFER_OPT( labelLength ) const void *label,
							 IN_LENGTH_SHORT_Z const int labelLength )
	{
	CRYPT_CONTEXT iWrapContext;
	MECHANISM_WRAP_INFO mechanismInfo;
	MESSAGE_DATA msgData;
	int status;

	assert( isReadPtr( keyObjectInfo, sizeof( PKCS12_OBJECT_INFO ) ) );
	assert( isReadPtr( password, passwordLen ) );
	assert( isReadPtr( keyObjectData, keyObjectDataLen) );
	assert( ( label == NULL && labelLength == 0 ) || \
			isReadPtr( label, labelLength ) );

	REQUIRES( cryptOwner == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( cryptOwner ) );
	REQUIRES( isHandleRangeValid( iPrivKeyContext ) );
	REQUIRES( passwordLen >= MIN_NAME_LENGTH && \
			  passwordLen <= CRYPT_MAX_TEXTSIZE );
	REQUIRES( keyObjectDataLen > MIN_OBJECT_SIZE && \
			  keyObjectDataLen < MAX_INTLENGTH_SHORT );
	REQUIRES( ( label == NULL && labelLength == 0 ) || \
			  ( label != NULL && \
				labelLength > 0 && labelLength < MAX_INTLENGTH_SHORT ) );

	/* Create the wrap context used to unwrap the private key */
	status = createPkcs12KeyWrapContext( ( PKCS12_OBJECT_INFO * ) keyObjectInfo, 
										 cryptOwner, password, passwordLen, 
										 &iWrapContext, FALSE );
	if( cryptStatusError( status ) )
		return( status );

	/* Set the key label.  We have to set the label before we load the key 
	   or the key load will be blocked by the kernel */
	if( label != NULL )
		{ 
		setMessageData( &msgData, ( MESSAGE_CAST ) label, \
						min( labelLength, CRYPT_MAX_TEXTSIZE ) ); 
		}
	else
		{ 
		setMessageData( &msgData, ( MESSAGE_CAST ) "Dummy label", 11 ); 
		}
	status = krnlSendMessage( iPrivKeyContext, IMESSAGE_SETATTRIBUTE_S, 
							  &msgData, CRYPT_CTXINFO_LABEL );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iWrapContext, IMESSAGE_DECREFCOUNT );
		return( status );
		}

	/* Import the encrypted private key into the PKC context */
	setMechanismWrapInfo( &mechanismInfo, ( MESSAGE_CAST * ) keyObjectData, 
						  keyObjectDataLen, NULL, 0, iPrivKeyContext, 
						  iWrapContext );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_IMPORT, 
							  &mechanismInfo, 
							  MECHANISM_PRIVATEKEYWRAP_PKCS8 );
	clearMechanismInfo( &mechanismInfo );
	krnlSendNotifier( iWrapContext, IMESSAGE_DECREFCOUNT );

	return( status );
	}

/****************************************************************************
*																			*
*							Read PKCS #12 Keys								*
*																			*
****************************************************************************/

/* Read a set of objects in a keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 6 ) ) \
static int readObjects( INOUT STREAM *stream, 
						INOUT_ARRAY( maxNoPkcs12objects ) PKCS12_INFO *pkcs12info, 
						IN_LENGTH_SHORT const int maxNoPkcs12objects, 
						IN_LENGTH_MIN( 32 ) const long endPos,
						const BOOLEAN isEncryptedCert,
						INOUT ERROR_INFO *errorInfo )
	{
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12info, sizeof( PKCS12_INFO ) * \
									maxNoPkcs12objects ) );

	REQUIRES( maxNoPkcs12objects >= 1 && \
			  maxNoPkcs12objects < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos >= 32 && endPos < MAX_BUFFER_SIZE );
	REQUIRES( errorInfo != NULL );

	/* Read each object in the current collection */
	for( iterationCount = 0; 
		 stell( stream ) < endPos && iterationCount < FAILSAFE_ITERATIONS_MED;
		 iterationCount++ )
		{
		PKCS12_INFO localPkcs12Info, *pkcs12infoPtr;
		PKCS12_OBJECT_INFO *pkcs12ObjectInfoPtr;
		BOOLEAN isCertificate = FALSE;

		/* Read one object */
		status = pkcs12ReadObject( stream, &localPkcs12Info, 
								   isEncryptedCert, errorInfo );
		if( cryptStatusError( status ) )
			return( status );
		if( localPkcs12Info.flags & ( PKCS12_FLAG_CERT | \
									  PKCS12_FLAG_ENCCERT ) )
			isCertificate = TRUE;

		/* Find the location where we'll copy over the new object data */
		status = findObjectEntryLocation( &pkcs12infoPtr, pkcs12info, 
										  maxNoPkcs12objects, 
										  localPkcs12Info.idLength <= 0 ? \
												NULL : localPkcs12Info.id,
										  localPkcs12Info.idLength,
										  localPkcs12Info.flags );
		if( cryptStatusError( status ) )
			{
			pkcs12freeObjectEntry( isCertificate ? \
					&localPkcs12Info.certInfo : &localPkcs12Info.keyInfo );
			if( status == CRYPT_ERROR_OVERFLOW )
				{
				retExt( CRYPT_ERROR_OVERFLOW, 
						( CRYPT_ERROR_OVERFLOW, errorInfo, 
						  "No more room in keyset data to add further "
						  "PKCS #12 items" ) );
				}
			retExt( status, 
					( status, errorInfo, 
					  "Couldn't reconcile ID-less object with existing "
					  "PKCS #12 objects" ) );
			}

		/* Copy the newly-read PKCS #12 object information into the PKCS #12
		   keyset info */
		pkcs12ObjectInfoPtr = isCertificate ? &pkcs12infoPtr->certInfo : \
											  &pkcs12infoPtr->keyInfo;
		if( pkcs12ObjectInfoPtr->data != NULL )
			{
			pkcs12freeObjectEntry( isCertificate ? \
					&localPkcs12Info.certInfo : &localPkcs12Info.keyInfo );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Multiple conflicting %s found in keyset",
					  isCertificate ? "certificates" : "keys" ) );
			}
		copyObjectInfo( pkcs12infoPtr, &localPkcs12Info, isCertificate );
		}

	return( CRYPT_OK );
	}

/* Read an entire keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int pkcs12ReadKeyset( INOUT STREAM *stream, 
					  OUT_ARRAY( maxNoPkcs12objects ) PKCS12_INFO *pkcs12info, 
					  IN_LENGTH_SHORT const int maxNoPkcs12objects, 
					  IN_LENGTH const long endPos,
					  INOUT ERROR_INFO *errorInfo )
	{
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs12info, sizeof( PKCS12_INFO ) * \
									maxNoPkcs12objects ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( maxNoPkcs12objects >= 1 && \
			  maxNoPkcs12objects < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( pkcs12info, 0, sizeof( PKCS12_INFO ) * maxNoPkcs12objects );

	/* Scan all of the objects in the keyset.  This gets quite complicated
	   because there are multiple points at which we can have a SET/SEQUENCE
	   OF and depending on the implementation multiple objects may be stored
	   at any of these points, so we have to handle multiple nesting points
	   at which we can find more than one of an object */
	for( status = CRYPT_OK, iterationCount = 0;
		 cryptStatusOK( status ) && stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_MED; iterationCount++ )
		{
		long length, innerEndPos = CRYPT_ERROR;
		int isEncrypted, noEOCs = 0;

		/* Read the CMS header encapsulation for the object.  At this point 
		   we get to more PKCS #12 stupidity, if we hit CMS EncryptedData 
		   (isEncrypted == TRUE) then it's actually a certificate (that 
		   doesn't need to be encrypted), and if we hit CMS Data 
		   (isEncrypted == FALSE) then it's usually a private key wrapped 
		   within a bizarre reinvention of CMS EncryptedData that's nested 
		   within the CMS Data, although in some rare cases it may be a 
		   SEQUENCE OF certificates and/or private key data that doesn't 
		   correspond to either of the above.  The combinations are:

			Data
				SEQUENCE OF
					ShroundedKeyBag | CertBag
			
			EncryptedData
				Data (= Certificate) 

		   So if we find EncryptedData then we know that it's definitely a 
		   certificate (so far nothing has tried to put private keys in
		   EncryptedData, although the spec allows it).  If we find Data 
		   then it could be anything, and we have to keep looking at a lower
		   level */
		status = isEncrypted = \
			readCMSheader( stream, keyDataOIDinfo, 
						   FAILSAFE_ARRAYSIZE( keyDataOIDinfo, OID_INFO ),
						   &length, READCMS_FLAG_NONE );
		if( cryptStatusError( status ) )
			{
			pkcs12Free( pkcs12info, maxNoPkcs12objects );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid PKCS #12 object header" ) );
			}

		/* Find out where the collection of PKCS #12 objects in the current
		   set of objects ends.  There may be only one, or many, and they
		   may or may not be of the same type */
		if( length != CRYPT_UNUSED )
			innerEndPos = stell( stream ) + length;
		else
			{
			/* In order to get to this point without finding a length we
			   need to have encountered three indefinite-length wrappers,
			   which means that we need to skip three EOCs at the end */
			noEOCs = 3;
			}
		if( !isEncrypted )
			{
			int innerLength;

			/* Skip the SET OF PKCS12Bag encapsulation */
			status = readSequenceI( stream, &innerLength );
			if( cryptStatusError( status ) )
				{
				pkcs12Free( pkcs12info, maxNoPkcs12objects );
				return( status );
				}
			if( length == CRYPT_UNUSED && innerLength != CRYPT_UNUSED )
				innerEndPos = stell( stream ) + innerLength;
			}
		if( innerEndPos == CRYPT_ERROR )
			{
			int innerLength;

			/* We still haven't got any length information, at this point 
			   the best that we can do is assume that there's a single 
			   encapsulated object present (which, in the rare situations
			   where this case arises, always seems to be the case) and
			   explicitly find its length before we can continue */
			status = getStreamObjectLength( stream, &innerLength );
			if( cryptStatusError( status ) )
				{
				pkcs12Free( pkcs12info, maxNoPkcs12objects );
				return( status );
				}
			innerEndPos = stell( stream ) + innerLength;

			/* In practice it's not quite this simple.  Firstly, this
			   approach is somewhat risky because we're calling 
			   getStreamObjectLength() on a stream that could in theory be 
			   non-seekable (although in practice file streams are always 
			   seekable so this is more a theoretical than an actual 
			   problem).  In addition PKCS #12 files usually contain a
			   single object that fits easily within the stream buffer, 
			   so even if the stream was non-seekable the call would 
			   usually work.

			   Secondly, and more critically, readObjects() calls
			   pkcs12ReadObject() which calls readRawObjectAlloc(), which
			   needs a definite length, again because it's dealing with a 
			   non-seekable stream (and in this case it really may be a non-
			   seekable stream).  Since we're dealing with an indefinite
			   length, readRawObjectAlloc() can't continue.
			   
			   A kludge workaround for this would be to pass the innerLength 
			   value in to readObjects() as a length hint and to tell it to 
			   bypass the call to readRawObjectAlloc() in favour of a direct
			   malloc() and fixed-length read, duplicating part of 
			   readRawObjectAlloc().  For now we'll leave this until there's 
			   an actual demand for it */
			pkcs12Free( pkcs12info, maxNoPkcs12objects );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Couldn't get PKCS #12 object length information" ) );

			}

		/* Read the set of objects */
		status = readObjects( stream, pkcs12info, maxNoPkcs12objects,
							  innerEndPos, isEncrypted, errorInfo );
		if( cryptStatusError( status ) )
			{
			pkcs12Free( pkcs12info, maxNoPkcs12objects );
			return( status );
			}

		/* Skip any EOCs that may be present.  In the simplest case where
		   the data was encoded using all indefinte-length encoding we know
		   how many EOCs are present and skip them all */
		if( noEOCs > 0 )
			{
			int i;

			for( i = 0; i < noEOCs; i++ )
				{
				const int value = checkEOC( stream );
				if( cryptStatusError( value ) )
					{
					pkcs12Free( pkcs12info, maxNoPkcs12objects );
					return( value );
					}
				if( value == FALSE )
					{
					pkcs12Free( pkcs12info, maxNoPkcs12objects );
					return( CRYPT_ERROR_BADDATA );
					}
				}
			}
		else
			{
			/* If the data was encoded using a mixture of definite and 
			   indefinite encoding there may be EOC's present even though 
			   the length is known so we skip them if necessary.  We have to 
			   make the reads speculative since the indefinite-length values 
			   were processed inside readCMSheader(), so we don't know how 
			   many there were */
			if( ( status = checkEOC( stream ) ) == TRUE )
				status = checkEOC( stream );
			if( cryptStatusError( status ) )
				{
				pkcs12Free( pkcs12info, maxNoPkcs12objects );
				return( status );
				}
			status = CRYPT_OK;	/* checkEOC() returns TRUE/FALSE */
			}
		}
	
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*									Get a Key								*
*																			*
****************************************************************************/

#if 0	/* Crack an RC2-40 key.  Call as 'keyCrack( certData, certDataSize )'
		   before the 'importCertificate()' call.  A more efficient version
		   of this is in ctx_rc2.c, the following code is really only 
		   present for testing the process with known-key data */

#pragma message( "#############################" )
#pragma message( "Building PKCS #12 key-cracker" )
#pragma message( "#############################" )

static int keyCrack( const void *encData, const int length )
	{
	CRYPT_CONTEXT cryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	MESSAGE_DATA msgData;
	BYTE data[ 32 ], key[ 32 ], *keyPtr = key + 14;
	int i, status;

	/* Test file: IV =  17 17 F1 B0 94 E8 EE F8		encData + 0
				  PT =	06 0B 2A 86 48 86 F7 0D
						-----------------------
				  XOR:	11 1C DB 36 DC 6E 19 F5 

	   So encr. above = CT block 2.
					  =	9C 4E 66 8A C7 6B 97 F5		encData + 8

	   Actual:    IV =  6F A0 7E A5 65 00 65 6C		encData + 0
				  PT =	06 0B 2A 86 48 86 F7 0D
						-----------------------
				  XOR:	69 AB 54 23 2D 86 92 61 

	   So encr. above = CT block 2.
					  =	34 AA F1 83 BD 9C C0 15		encData + 8 */
	
//	memcpy( data, "\x17\x17\xF1\xB0\x94\xE8\xEE\xF8", 8 );
	memcpy( data, "\x6F\xA0\x7E\xA5\x65\x00\x65\x6C", 8 );
	for( i = 0; i < 8; i++ )
		data[ i ] ^= i[ "\x06\x0B\x2A\x86\x48\x86\xF7\x0D" ];

	memcpy( key, "PKCS#12PKCS#12", 14 );
	memset( key + 14, 0, 5 );

//	memcpy( keyPtr, "\x13\x25\x0c\x1a\x60", 5 );	// Test PKCS #12 file, file #1.
//	memcpy( keyPtr, "\x2C\x28\x14\xC4\x01", 5 );	// "Tellus" PKCS #12 file, file #2

	for( i = 0; i < 256; i++ )
		{
		int keyIndex;

		printf( "Trying keys %02X xx.\n", i );
		fflush( stdout );
		while( keyPtr[ 0 ] == i )
			{
			setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RC2 );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
									  &createInfo, OBJECT_TYPE_CONTEXT );
			if( cryptStatusError( status ) )
				return( status );
			cryptContext = createInfo.cryptHandle;
			setMessageData( &msgData, key, 19 );
			status = krnlSendMessage( cryptContext, IMESSAGE_SETATTRIBUTE_S, 
									  &msgData, CRYPT_CTXINFO_KEY );
			if( cryptStatusOK( status ) )
				{
				setMessageData( &msgData, ( MESSAGE_CAST ) encData, 8 );
				status = krnlSendMessage( cryptContext, IMESSAGE_SETATTRIBUTE_S, 
										  &msgData, CRYPT_CTXINFO_IV );
				}
			if( cryptStatusOK( status ) )
				{
#if 0	/* For key-crack */
				memcpy( data, ( const BYTE * ) encData + 8, 8 );
				status = krnlSendMessage( cryptContext, IMESSAGE_CTX_DECRYPT, 
										  data, 8 );
#else	/* For full decrypt */
				status = krnlSendMessage( cryptContext, IMESSAGE_CTX_DECRYPT, 
										  ( MESSAGE_CAST ) \
											( ( const BYTE * ) encData + 8 ), 
										  length - 8 );
				DEBUG_DUMP( "crack_result", ( const BYTE * ) encData + 8,
							length - 8 );
#endif /* 0 */
				}
			krnlSendNotifier( cryptContext, IMESSAGE_DECREFCOUNT );
			if( cryptStatusError( status ) )
				return( status );
			if( data[ 0 ] == 0x06 && \
				!memcmp( data, "\x06\x0B\x2A\x86\x48\x86\xF7\x0D", 8 ) )
				{
				printf( "Found at %02X %02X %02X %02X %02X.\n",
						keyPtr[ 0 ], keyPtr[ 1 ], keyPtr[ 2 ],
						keyPtr[ 3 ], keyPtr[ 4 ] );
				fflush( stdout );
				return( CRYPT_OK );
				}
			for( keyIndex = 4; keyIndex >= 0; keyIndex++ )
				{
				keyPtr[ keyIndex ]++;
				if( keyPtr[ keyIndex ] > 0 )
					break;
				}
			if( keyIndex == 1 )
				{
				printf( "Trying keys %02X %02X.\n", 
						keyPtr[ 0 ], keyPtr[ 1 ] );
				fflush( stdout );
				}
			}
		}

	return( CRYPT_OK );
	}
#endif /* 0 */

/* Import a certificate as a data-only certificate object to be attached to 
   the private key */

#ifdef USE_CERTIFICATES

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4, 5, 7 ) ) \
static int importDataOnlyCertificate( const PKCS12_INFO *pkcs12infoPtr,	
									  IN_HANDLE const CRYPT_USER iCryptUser,
									  OUT_HANDLE_OPT CRYPT_CONTEXT *iCryptContext,
									  OUT_HANDLE_OPT CRYPT_CERTIFICATE *iDataCert,
									  IN_BUFFER( certIDlength ) const void *certID, 
									  IN_LENGTH_SHORT const int certIDlength,
									  INOUT ERROR_INFO *errorInfo )
	{
	CRYPT_CERTIFICATE iCertificate;
	const PKCS12_OBJECT_INFO *certObjectInfo = &pkcs12infoPtr->certInfo;
	const int certDataSize = certObjectInfo->payloadSize;
	DYNBUF pubKeyDB;
	STREAM stream;
	int status;

	assert( isReadPtr( pkcs12infoPtr, sizeof( PKCS12_INFO ) ) );
	assert( isWritePtr( iCryptContext, sizeof( CRYPT_CONTEXT ) ) );
	assert( isWritePtr( iDataCert, sizeof( CRYPT_CERTIFICATE ) ) );
	assert( isReadPtr( certID, certIDlength ) );

	REQUIRES( iCryptUser == DEFAULTUSER_OBJECT_HANDLE || \
			  isHandleRangeValid( iCryptUser ) );
	REQUIRES( certIDlength > 0 && certIDlength < MAX_INTLENGTH_SHORT );

	/* Clear return values */
	*iCryptContext = *iDataCert = CRYPT_ERROR;

	/* If it's an unencrypted certificate (they almost never are) then we 
	   can import it directly */
	if( pkcs12infoPtr->flags & PKCS12_FLAG_CERT )
		{
		sMemConnect( &stream, ( BYTE * ) certObjectInfo->data + \
										 certObjectInfo->payloadOffset, 
					 certDataSize );
		status = importCertFromStream( &stream, &iCertificate, iCryptUser,
									   CRYPT_CERTTYPE_CERTIFICATE, 
									   certDataSize, 
									   KEYMGMT_FLAG_DATAONLY_CERT );
		sMemDisconnect( &stream );
		}
	else
		{
		BYTE certDataBuffer[ 2048 + 8 ], *certData = certDataBuffer;

		REQUIRES( pkcs12infoPtr->flags & PKCS12_FLAG_ENCCERT );

		/* It's an encrypted certificate, we need to decrypt it before we 
		   can import it.  First we set up a buffer to decrypt the 
		   certificate data */
		if( certDataSize > 2048 )
			{
			if( certDataSize >= MAX_INTLENGTH_SHORT )
				return( CRYPT_ERROR_OVERFLOW );
			if( ( certData = clAlloc( "getItemFunction", \
									  certDataSize ) ) == NULL )
				return( CRYPT_ERROR_MEMORY );
			}
		memcpy( certData, 
				( BYTE * ) certObjectInfo->data + \
						   certObjectInfo->payloadOffset, certDataSize );

		/* Decrypt and import the certificate */
		status = importCertificate( certObjectInfo, iCryptUser, 
									certID, certIDlength, certData, 
									certDataSize, &iCertificate );
		zeroise( certData, certDataSize );
		if( certData != certDataBuffer )
			clFree( "getItemFunction", certData );
		}
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't recreate certificate from stored certificate "
				  "data" ) );
		}
		
	/* We've got the certificate, now create the public part of the context 
	   from the certificate's encoded public-key components */
	status = dynCreate( &pubKeyDB, iCertificate, CRYPT_IATTRIBUTE_SPKI );
	if( cryptStatusError( status ) )
		return( status );
	sMemConnect( &stream, dynData( pubKeyDB ), dynLength( pubKeyDB ) );
	status = iCryptReadSubjectPublicKey( &stream, iCryptContext,
										 SYSTEM_OBJECT_HANDLE, TRUE );
	sMemDisconnect( &stream );
	dynDestroy( &pubKeyDB );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCertificate, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't recreate public key from certificate" ) );
		}

	*iDataCert = iCertificate;
	return( CRYPT_OK );
	}
#endif /* USE_CERTIFICATES */

/* Get a key from a PKCS #12 keyset.  This gets pretty ugly both because 
   PKCS #12 keysets contain no effective indexing information (making it 
   impossible to look up objects within them) and because in most cases all 
   data, including public keys and certificates, is encrypted.  To handle 
   this we only allow private-key reads, and treat whatever's in the keyset 
   as being a match for any request, since without indexing information 
   there's no way to tell whether it really is a match or not */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
static int getItemFunction( INOUT KEYSET_INFO *keysetInfoPtr,
							OUT_HANDLE_OPT CRYPT_HANDLE *iCryptHandle,
							IN_ENUM( KEYMGMT_ITEM ) \
								const KEYMGMT_ITEM_TYPE itemType,
							IN_KEYID const CRYPT_KEYID_TYPE keyIDtype,
							IN_BUFFER( keyIDlength ) const void *keyID, 
							IN_LENGTH_KEYID const int keyIDlength,
							IN_OPT void *auxInfo, 
							INOUT_OPT int *auxInfoLength,
							IN_FLAGS_Z( KEYMGMT ) const int flags )
	{
#ifdef USE_CERTIFICATES
	CRYPT_CERTIFICATE iDataCert = CRYPT_ERROR;
#endif /* USE_CERTIFICATES */
	CRYPT_CONTEXT iCryptContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	const PKCS12_INFO *pkcs12infoPtr;
	const int auxInfoMaxLength = *auxInfoLength;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isWritePtr( iCryptHandle, sizeof( CRYPT_HANDLE ) ) );
	assert( isReadPtr( keyID, keyIDlength ) );
	assert( ( auxInfo == NULL && auxInfoMaxLength == 0 ) || \
			isReadPtr( auxInfo, auxInfoMaxLength ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 );
	REQUIRES( itemType == KEYMGMT_ITEM_PUBLICKEY || \
			  itemType == KEYMGMT_ITEM_PRIVATEKEY );
	REQUIRES( keyIDtype == CRYPT_KEYID_NAME || \
			  keyIDtype == CRYPT_KEYID_URI || \
			  keyIDtype == CRYPT_IKEYID_KEYID || \
			  keyIDtype == CRYPT_IKEYID_PGPKEYID || \
			  keyIDtype == CRYPT_IKEYID_ISSUERID );
	REQUIRES( keyIDlength >= MIN_NAME_LENGTH && \
			  keyIDlength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( ( auxInfo == NULL && *auxInfoLength == 0 ) || \
			  ( auxInfo != NULL && \
				*auxInfoLength > 0 && \
				*auxInfoLength < MAX_INTLENGTH_SHORT ) );
	REQUIRES( flags >= KEYMGMT_FLAG_NONE && flags < KEYMGMT_FLAG_MAX );

	/* Clear return values */
	*iCryptHandle = CRYPT_ERROR;

	/* Only private-key reads are possible */
	if( itemType != KEYMGMT_ITEM_PRIVATEKEY )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "PKCS #12 keysets only support private-key reads" ) );
		}

	/* PKCS #12 doesn't provide any useful indexing information that we
	   can use to look up a key apart from an optional label, so if we get 
	   a key ID type that we can't do anything with or we're matching on
	   the special-case key ID "[none]" we perform a fetch of the first 
	   matching private key on the basis that most PKCS #12 keysets only 
	   contain a single key of interest */
	if( keyIDtype == CRYPT_IKEYID_KEYID || \
		keyIDtype == CRYPT_IKEYID_PGPKEYID || \
		keyIDtype == CRYPT_IKEYID_ISSUERID || \
		( keyIDlength == 6 && !strCompare( keyID, "[none]", 6 ) ) )
		{
		pkcs12infoPtr = pkcs12FindEntry( keysetInfoPtr->keyData,
										 keysetInfoPtr->keyDataNoObjects,
										 CRYPT_KEYID_NAME, NULL, 0, 
										 TRUE );
		}
	else
		{
		/* Find a private-key entry */
		pkcs12infoPtr = pkcs12FindEntry( keysetInfoPtr->keyData,
										 keysetInfoPtr->keyDataNoObjects,
										 keyIDtype, keyID, keyIDlength, 
										 FALSE );
		}
	if( pkcs12infoPtr == NULL )
		{
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No information present for this ID" ) );
		}
	if( pkcs12infoPtr->keyInfo.data == NULL )
		{
		/* There's not enough information present to get a private key */
		retExt( CRYPT_ERROR_NOTFOUND, 
				( CRYPT_ERROR_NOTFOUND, KEYSET_ERRINFO, 
				  "No private key data present for this ID" ) );
		}

	/* If we're just checking whether an object exists, return now.  If all
	   that we want is the key label, copy it back to the caller and exit */
	if( flags & KEYMGMT_FLAG_CHECK_ONLY )
		return( CRYPT_OK );
	if( flags & KEYMGMT_FLAG_LABEL_ONLY )
		{
		return( attributeCopyParams( auxInfo, auxInfoMaxLength, 
									 auxInfoLength, pkcs12infoPtr->label, 
									 pkcs12infoPtr->labelLength ) );
		}

	/* Make sure that the user has supplied a password */
	if( auxInfo == NULL )
		return( CRYPT_ERROR_WRONGKEY );

	/* If there's a certificate present with the private key, import it as a 
	   data-only certificate object to be attached to the private key */
#ifdef USE_CERTIFICATES
	if( pkcs12infoPtr->certInfo.data != NULL )
		{
		status = importDataOnlyCertificate( pkcs12infoPtr, 
											keysetInfoPtr->ownerHandle, 
											&iCryptContext, &iDataCert,
											auxInfo, *auxInfoLength,
											KEYSET_ERRINFO );
		if( cryptStatusError( status ) )
			return( status );
		if( iDataCert != CRYPT_ERROR )
			{
			status = iCryptVerifyID( iDataCert, keyIDtype, keyID, 
									 keyIDlength );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
				krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
				retExt( status, 
						( status, KEYSET_ERRINFO, 
						  "Certificate fetched for ID type %d doesn't "
						  "actually correspond to the given ID", 
						  keyIDtype ) );
				}
			}
		}
	else
#endif /* USE_CERTIFICATES */
		{
		/* Create the private-key object that we'll be importing the key 
		   data into.  In yet another piece of design brilliance, the PKC 
		   algorithm that's needed to create the public/private-key context 
		   is stored inside the encrypted key data, so we can't create a 
		   context to import the key data into until we've already imported 
		   the key data.  To get around this we default to CRYPT_ALGO_RSA, 
		   which is almost always the case */
		setMessageCreateObjectInfo( &createInfo, CRYPT_ALGO_RSA );
		status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, 
								  IMESSAGE_DEV_CREATEOBJECT, &createInfo, 
								  OBJECT_TYPE_CONTEXT );
		if( cryptStatusError( status ) )
			return( status );
		iCryptContext = createInfo.cryptHandle;
		}

	/* Import the wrapped private key */
	status = importPrivateKey( &pkcs12infoPtr->keyInfo, 
					keysetInfoPtr->ownerHandle, iCryptContext,
					auxInfo, *auxInfoLength, 
					( const BYTE * ) pkcs12infoPtr->keyInfo.data + \
									 pkcs12infoPtr->keyInfo.payloadOffset,
					pkcs12infoPtr->keyInfo.payloadSize, 
					( pkcs12infoPtr->labelLength > 0 ) ? \
						pkcs12infoPtr->label : NULL, 
					pkcs12infoPtr->labelLength );
	if( cryptStatusError( status ) )
		{
		krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
		if( iDataCert != CRYPT_ERROR )
			krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
		retExt( status, 
				( status, KEYSET_ERRINFO, 
				  "Couldn't unwrap and import private key" ) );
		}

#ifdef USE_CERTIFICATES
	/* Connect the data-only certificate object to the private-key context 
	   if necessary.  This is an internal object used only by the context so 
	   we tell the kernel to mark it as owned by the context only */
	if( iDataCert != CRYPT_ERROR )
		{
		status = krnlSendMessage( iCryptContext, IMESSAGE_SETDEPENDENT, 
								  &iDataCert, SETDEP_OPTION_NOINCREF );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iCryptContext, IMESSAGE_DECREFCOUNT );
			krnlSendNotifier( iDataCert, IMESSAGE_DECREFCOUNT );
			retExt( status, 
					( status, KEYSET_ERRINFO, 
					  "Couldn't attach certificate to key" ) );
			}
		}
#endif /* USE_CERTIFICATES */
	*iCryptHandle = iCryptContext;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Keyset Access Routines							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initPKCS12get( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
			  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 );

	/* Set the access method pointers */
	keysetInfoPtr->getItemFunction = getItemFunction;

	return( CRYPT_OK );
	}
#endif /* USE_PKCS12 */
