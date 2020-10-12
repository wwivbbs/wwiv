/****************************************************************************
*																			*
*							cryptlib Keyset Routines						*
*						Copyright Peter Gutmann 1995-2007					*
*																			*
****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include "crypt.h"
#ifdef INC_ALL
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "pgp_rw.h"
  #include "keyset.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/pgp_rw.h"
  #include "keyset/keyset.h"
#endif /* Compiler-specific includes */

#ifdef USE_KEYSETS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check keyset data */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckKeyset( const KEYSET_INFO *keysetInfoPtr )
	{
	assert( isReadPtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	/* Check overall keyset data */
	if( keysetInfoPtr->type == KEYSET_FILE )
		{
		if( !isEnumRange( keysetInfoPtr->subType, KEYSET_SUBTYPE ) )
			{
			DEBUG_PUTS(( "sanityCheckKeyset: General info (file keyset)" ));
			return( FALSE );
			}
		}
	else
		{
		if( !isEnumRange( keysetInfoPtr->type, KEYSET ) || \
			keysetInfoPtr->subType != KEYSET_SUBTYPE_NONE )
			{
			DEBUG_PUTS(( "sanityCheckKeyset: General info" ));
			return( FALSE );
			}
		}
	if( !CHECK_FLAGS( keysetInfoPtr->flags, KEYSET_FLAG_NONE, 
					  KEYSET_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckKeyset: Flags" ));
		return( FALSE );
		}

	/* Check keyset data storage */
	if( DATAPTR_ISSET( keysetInfoPtr->keyData ) )
		{
		if( keysetInfoPtr->keyDataSize <= 0 || \
			keysetInfoPtr->keyDataSize >= MAX_BUFFER_SIZE || \
			keysetInfoPtr->keyDataNoObjects < 0 || \
			keysetInfoPtr->keyDataNoObjects > 16 )
			{
			DEBUG_PUTS(( "sanityCheckKeyset: Keyset data" ));
			return( FALSE );
			}
		}
	else
		{
		if( !DATAPTR_ISNULL( keysetInfoPtr->keyData ) || \
			keysetInfoPtr->keyDataSize != 0 || \
			keysetInfoPtr->keyDataNoObjects != 0 )
			{
			DEBUG_PUTS(( "sanityCheckKeyset: Spurious keyset data" ));
			return( FALSE );
			}
		}

	/* Check subtype-specific data */
	switch( keysetInfoPtr->type )
		{
		case KEYSET_FILE:
			{
			const FILE_INFO *fileInfo = keysetInfoPtr->keysetFile;

			if( fileInfo->iHardwareDevice != CRYPT_UNUSED && \
				!isHandleRangeValid( fileInfo->iHardwareDevice ) )
				{
				DEBUG_PUTS(( "sanityCheckKeyset: File info" ));
				return( FALSE );
				}
			break;
			}

#ifdef USE_HTTP
		case KEYSET_HTTP:
			{
			const HTTP_INFO *httpInfo = keysetInfoPtr->keysetHTTP;

			if( httpInfo->bufPos < 0 || \
				httpInfo->bufPos >= MAX_BUFFER_SIZE )
				{
				DEBUG_PUTS(( "sanityCheckKeyset: HTTP info" ));
				return( FALSE );
				}
			break;
			}
#endif /* USE_HTTP */

#ifdef USE_LDAP
		case KEYSET_LDAP:
			/* Nothing further to check */
			break;
#endif /* USE_LDAP */

#if defined( USE_DBMS )
		case KEYSET_DBMS:
			{
			const DBMS_INFO *dbmsInfo = keysetInfoPtr->keysetDBMS;

			/* Perform a rather ugly check of the DBMS flags, unfortunately
			   we can't see these values outside dbms.h */
			if( !CHECK_FLAGS( dbmsInfo->flags, 0, 0x20 ) )
				{
				DEBUG_PUTS(( "sanityCheckKeyset: DBMS info" ));
				return( FALSE );
				}
			break;
			}
#endif /* USE_DBMS */

		default:
			retIntError_Boolean();
		}

	/* Check associated handles */
	if( !isHandleRangeValid( keysetInfoPtr->objectHandle ) || \
		!( keysetInfoPtr->ownerHandle == DEFAULTUSER_OBJECT_HANDLE || \
		   isHandleRangeValid( keysetInfoPtr->ownerHandle ) ) )
		{
		DEBUG_PUTS(( "sanityCheckKeyset: Object handles" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Check that keyset function pointers have been set up correctly */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkKeysetFunctions( IN const KEYSET_INFO *keysetInfoPtr )
	{
	assert( isReadPtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	/* Check universal access functions */
	if( !FNPTR_ISSET( keysetInfoPtr->initFunction ) || \
		!FNPTR_ISSET( keysetInfoPtr->shutdownFunction ) || \
		!FNPTR_ISSET( keysetInfoPtr->getItemFunction ) )
		{
		DEBUG_PUTS(( "checkKeysetFunctions: Basic keyset functions" ));
		return( FALSE );
		}

	/* Check subtype-specific functions */
	switch( keysetInfoPtr->type )
		{
		case KEYSET_FILE:
#ifndef USE_PKCS12_WRITE
			if( keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 )
				{
				if( !FNPTR_ISNULL( keysetInfoPtr->setItemFunction ) )
					{
					DEBUG_PUTS(( "checkKeysetFunctions: Suprious PKCS #12 write function" ));
					return( FALSE );
					}
				}
			else
#endif /* USE_PKCS12_WRITE */
			if( !FNPTR_ISSET( keysetInfoPtr->setItemFunction ) )
				{
				DEBUG_PUTS(( "checkKeysetFunctions: Basic file access functions" ));
				return( FALSE );
				}
			if( keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 )
				{
				if( !FNPTR_ISSET( keysetInfoPtr->getSpecialItemFunction ) || \
					!FNPTR_ISSET( keysetInfoPtr->setSpecialItemFunction ) || \
					!FNPTR_ISSET( keysetInfoPtr->deleteItemFunction ) || \
					!FNPTR_ISSET( keysetInfoPtr->getFirstItemFunction ) || \
					!FNPTR_ISSET( keysetInfoPtr->getNextItemFunction ) )
					{
					DEBUG_PUTS(( "checkKeysetFunctions: P15 access functions" ));
					return( FALSE );
					}
				}
			break;

#ifdef USE_HTTP
		case KEYSET_HTTP:
			/* No additional access functions available */
			break;
#endif /* USE_HTTP */

#ifdef USE_LDAP
		case KEYSET_LDAP:
			if( !FNPTR_ISSET( keysetInfoPtr->setItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->deleteItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->isBusyFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->getAttributeFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->setAttributeFunction ) )
				{
				DEBUG_PUTS(( "checkKeysetFunctions: DBMS functions" ));
				return( FALSE );
				}
			break;
#endif /* USE_LDAP */

#if defined( USE_DBMS )
		case KEYSET_DBMS:
			if( !FNPTR_ISSET( keysetInfoPtr->getFirstItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->getNextItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->setItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->deleteItemFunction ) || \
				!FNPTR_ISSET( keysetInfoPtr->isBusyFunction ) )
				{
				DEBUG_PUTS(( "checkKeysetFunctions: DBMS functions" ));
				return( FALSE );
				}
			break;
#endif /* USE_DBMS */

		default:
			retIntError_Boolean();
		}

	return( TRUE );
	}

/* Clear the extended error information that may be present from a previous
   operation prior to beginning a new operation */

STDC_NONNULL_ARG( ( 1 ) ) \
static void resetErrorInfo( INOUT KEYSET_INFO *keysetInfoPtr )
	{
	ERROR_INFO *errorInfo = &keysetInfoPtr->errorInfo;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	memset( errorInfo, 0, sizeof( ERROR_INFO ) );
	}

/* Prepare to update a keyset, performing various access checks and pre-
   processing of information */

typedef struct {
	CRYPT_KEYID_TYPE keyIDtype;		/* KeyID type */
	BUFFER_FIXED( keyIDlength ) \
	const void *keyID;				/* KeyID value */
	int keyIDlength;
	} KEYID_INFO;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int initKeysetUpdate( INOUT KEYSET_INFO *keysetInfoPtr, 
							 INOUT_OPT KEYID_INFO *keyIDinfo, 
							 OUT_BUFFER_OPT_FIXED( keyIdMaxLength ) \
								void *keyIDbuffer,
							 IN_LENGTH_SHORT_Z const int keyIdMaxLength,
							 const BOOLEAN isRead )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( ( keyIDinfo == NULL && \
			  keyIDbuffer == NULL && keyIdMaxLength == 0 ) || \
			( isWritePtr( keyIDinfo, sizeof( KEYID_INFO ) ) && \
			  isReadPtrDynamic( keyIDbuffer, keyIdMaxLength ) ) );

	REQUIRES( ( keyIDinfo == NULL && \
				keyIDbuffer == NULL && keyIdMaxLength == 0 ) || \
			  ( keyIDinfo != NULL && \
				keyIDbuffer != NULL && keyIdMaxLength == KEYID_SIZE ) );
	REQUIRES( isRead == TRUE || isRead == FALSE );

	/* Clear return values */
	if( keyIDbuffer != NULL )
		memset( keyIDbuffer, 0, min( 16, keyIdMaxLength ) );

	/* If we're in the middle of a query we can't do anything else */
	if( FNPTR_ISSET( keysetInfoPtr->isBusyFunction ) )
		{
		const KEY_ISBUSY_FUNCTION isBusyFunction = \
					( KEY_ISBUSY_FUNCTION ) \
					FNPTR_GET( keysetInfoPtr->isBusyFunction );

		REQUIRES( isBusyFunction != NULL );

		if( isBusyFunction( keysetInfoPtr ) )
			return( CRYPT_ERROR_INCOMPLETE );
		}

	/* If we've been passed a full issuerAndSerialNumber as a key ID and the 
	   keyset needs an issuerID, convert it */
	if( keyIDinfo != NULL && \
		keyIDinfo->keyIDtype == CRYPT_IKEYID_ISSUERANDSERIALNUMBER && \
		( keysetInfoPtr->type == KEYSET_DBMS || \
		  ( keysetInfoPtr->type == KEYSET_FILE && \
		    keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 ) ) )
		{
		HASHINFO hashInfo;
		STREAM stream;
		int hashSize, payloadStart DUMMY_INIT, length, status;

		/* Hash the full iAndS to get an issuerID and use that for the 
		   keyID.  This is complicated by the fact that there exist one or 
		   two broken implementations out there that use a non-DER encoding
		   of the iAndS wrapper (for example encoding the length as 
		   '82 00 nn' instead of 'nn').  To handle this we read the wrapper
		   and then write our own correctly-encoded version to a buffer
		   that we hash seperately from the iAndS payload */
		sMemConnect( &stream, keyIDinfo->keyID, keyIDinfo->keyIDlength );
		status = readSequence( &stream, &length );
		if( cryptStatusOK( status ) )
			payloadStart = stell( &stream );
		sMemDisconnect( &stream );
		if( cryptStatusOK( status ) )
			{
			HASH_FUNCTION hashFunction;
			BYTE buffer[ 8 + 8 ];

			REQUIRES( payloadStart > 0 && \
					  payloadStart < keyIDinfo->keyIDlength );

			/* We've processed the wrapper, write our own known-good version
			   and then hash that and the iAndS payload */
			getHashParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, &hashSize );
			sMemOpen( &stream, buffer, 8 );
			status = writeSequence( &stream, length );
			ENSURES( cryptStatusOK( status ) );
			hashFunction( hashInfo, NULL, 0, buffer, stell( &stream ), 
						  HASH_STATE_START );
			sMemClose( &stream );
			hashFunction( hashInfo, keyIDbuffer, keyIdMaxLength, 
						  ( BYTE * ) keyIDinfo->keyID + payloadStart, 
						  keyIDinfo->keyIDlength - payloadStart, 
						  HASH_STATE_END );
			}
		else
			{
			HASH_FUNCTION_ATOMIC hashFunctionAtomic;

			/* The attempt to read the wrapper failed, just hash the whole 
			   thing as a blob and continue */
			getHashAtomicParameters( CRYPT_ALGO_SHA1, 0, &hashFunctionAtomic, 
									 &hashSize );
			hashFunctionAtomic( keyIDbuffer, keyIdMaxLength, keyIDinfo->keyID, 
								keyIDinfo->keyIDlength );
			}
		keyIDinfo->keyIDtype = CRYPT_IKEYID_ISSUERID;
		keyIDinfo->keyID = keyIDbuffer;
		keyIDinfo->keyIDlength = hashSize;
		}

	/* If this is a read access there's nothing further to do */
	if( isRead )
		return( CRYPT_OK );

	/* This is a write update, make sure that we can write to the keyset.  
	   This covers all possibilities, both keyset types for which writing 
	   isn't supported and individual keysets that we can't write to 
	   because of things like file permissions, so once we pass this check 
	   we know that we can write to the keyset */
	if( TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_READONLY ) )
		return( CRYPT_ERROR_PERMISSION );

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Flat-file Keyset Functions						*
*																			*
****************************************************************************/

/* OID information used to read the header of a PKCS #15 file.  Since the 
   PKCS #15 content can be further wrapped in CMS AuthData we have to check
   for both types of content */

static const CMS_CONTENT_INFO oidInfoPkcs15Data = { 0, 0 };

static const OID_INFO keyFileOIDinfo[] = {
	{ OID_PKCS15_CONTENTTYPE, TRUE, &oidInfoPkcs15Data },
	{ OID_CMS_AUTHDATA, FALSE, &oidInfoPkcs15Data },
	{ NULL, 0 }, { NULL, 0 }
	};

/* Identify a flat-file keyset type */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getKeysetType( INOUT STREAM *stream,
						  OUT_ENUM_OPT( KEYSET_SUBTYPE ) KEYSET_SUBTYPE *subType )
	{
	long length;
	int value, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( subType, sizeof( KEYSET_SUBTYPE ) ) );

	/* Clear return value */
	*subType = KEYSET_SUBTYPE_NONE;

	/* Try and guess the basic type */
	status = value = sPeek( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value == BER_SEQUENCE )
		{
		/* Read the length of the object, which should be between 64 and 64K 
		   bytes in size.  We have to allow for very tiny files to handle 
		   PKCS #15 files that contain only configuration data, and rather 
		   large ones to handle the existence of large numbers of trusted 
		   certificates, with a maximum of 32 objects * ~2K per object we 
		   can get close to 64K in size.  The length may also be zero if the 
		   indefinite encoding form is used.  Although PKCS #15 specifies 
		   the use of DER, it doesn't hurt to allow this at least for the 
		   outer wrapper, if Microsoft ever move to PKCS #15 they're bound 
		   to get it wrong */
		status = readLongSequence( stream, &length );
		if( cryptStatusError( status ) )
			return( status );
		if( length != CRYPT_UNUSED && ( length < 64 || length > 65535L ) )
			return( CRYPT_ERROR_BADDATA );

		/* Check for a PKCS #12/#15 file */
		status = value = peekTag( stream );
		if( cryptStatusError( status ) )
			return( status );
		if( value == BER_INTEGER )
			{
			long version;

			/* Check for a PKCS #12 version number */
			status = readShortInteger( stream, &version );
			if( cryptStatusError( status ) )
				return( status );
			if( version != 3 )
				return( CRYPT_ERROR_BADDATA );
			*subType = KEYSET_SUBTYPE_PKCS12;

			return( CRYPT_OK );
			}

		/* Check for a PKCS #15 file type, either direct PKCS #15 content 
		   or PKCS #15 wrapped in CMS AuthData */
		status = readOID( stream, keyFileOIDinfo, 
						  FAILSAFE_ARRAYSIZE( keyFileOIDinfo, OID_INFO ),
						  &value );
		if( cryptStatusError( status ) )
			return( status );
		*subType = KEYSET_SUBTYPE_PKCS15;

		return( CRYPT_OK );
		}
#ifdef USE_PGP
	value = pgpGetPacketType( value );
	if( value == PGP_PACKET_PUBKEY || value == PGP_PACKET_SECKEY )
		{
		KEYSET_SUBTYPE type;

		/* Determine the file type based on the initial CTB */
		type = ( value == PGP_PACKET_PUBKEY ) ? \
			   KEYSET_SUBTYPE_PGP_PUBLIC : KEYSET_SUBTYPE_PGP_PRIVATE;

		/* Perform a sanity check to make sure that the rest looks like a 
		   PGP keyring */
		status = pgpReadPacketHeader( stream, &value, &length, 64, 4096 );
		if( cryptStatusError( status ) )
			return( status );
		if( type == KEYSET_SUBTYPE_PGP_PUBLIC )
			{
			if( length < 64 || length > 1024  )
				return( CRYPT_ERROR_BADDATA );
			}
		else
			{
			if( length < 200 || length > 4096 )
				return( CRYPT_ERROR_BADDATA );
			}
		status = value = sgetc( stream );
		if( cryptStatusError( status ) )
			return( status );
		if( value != PGP_VERSION_2 && value != PGP_VERSION_3 && \
			value != PGP_VERSION_OPENPGP )
			return( CRYPT_ERROR_BADDATA );
		*subType = type;

		return( CRYPT_OK );
		}
#endif /* USE_PGP */

	/* "It doesn't look like anything from here" */
	return( CRYPT_ERROR_BADDATA );
	}

/* Open a flat-file keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5, 6 ) ) \
static int openKeysetStream( INOUT STREAM *stream, 
							 IN_BUFFER( nameLength ) const char *name,
							 IN_LENGTH_SHORT_MIN( MIN_NAME_LENGTH ) \
								const int nameLength,
							 IN_ENUM_OPT( CRYPT_KEYOPT ) \
								const CRYPT_KEYOPT_TYPE options,
							 OUT_BOOL BOOLEAN *isReadOnly, 
							 OUT_ENUM_OPT( KEYSET_SUBTYPE ) \
								KEYSET_SUBTYPE *keysetSubType )
	{
	KEYSET_SUBTYPE subType = KEYSET_SUBTYPE_PKCS15;
	char nameBuffer[ MAX_ATTRIBUTE_SIZE + 1 + 8 ];
	const int suffixPos = nameLength - 4;
	int openMode, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isReadPtrDynamic( name, nameLength ) );
	assert( isWritePtr( isReadOnly, sizeof( BOOLEAN ) ) );
	assert( isWritePtr( keysetSubType, sizeof( KEYSET_SUBTYPE ) ) );

	REQUIRES( isEnumRangeOpt( options, CRYPT_KEYOPT ) );
	REQUIRES( nameLength >= MIN_NAME_LENGTH && \
			  nameLength < MAX_ATTRIBUTE_SIZE );

	/* Clear return values */
	*isReadOnly = FALSE;
	*keysetSubType = KEYSET_SUBTYPE_NONE;

	/* Convert the keyset name into a null-terminated string */
	REQUIRES( rangeCheck( nameLength, 1, MAX_ATTRIBUTE_SIZE ) );
	memcpy( nameBuffer, name, nameLength );
	nameBuffer[ nameLength ] = '\0';

	/* Get the expected subtype based on the keyset name.  The default is
	   PKCS #15 if no contraindication is found in the file suffix */
	if( suffixPos > 0 && nameBuffer[ suffixPos ] == '.' )
		{
		if( !strCompare( nameBuffer + suffixPos + 1, "pgp", 3 ) || \
			!strCompare( nameBuffer + suffixPos + 1, "gpg", 3 ) || \
			!strCompare( nameBuffer + suffixPos + 1, "pkr", 3 ) )
			subType = KEYSET_SUBTYPE_PGP_PUBLIC;
		if( !strCompare( nameBuffer + suffixPos + 1, "skr", 3 ) )
			subType = KEYSET_SUBTYPE_PGP_PRIVATE;
		if( !strCompare( nameBuffer + suffixPos + 1, "pfx", 3 ) || \
			!strCompare( nameBuffer + suffixPos + 1, "p12", 3 ) )
			subType = KEYSET_SUBTYPE_PKCS12;
		}

	/* If the file is read-only, put the keyset into read-only mode */
	if( fileReadonly( nameBuffer ) )
		{
		/* If we want to create a new file we can't do it if we don't have
		   write permission */
		if( options == CRYPT_KEYOPT_CREATE )
			return( CRYPT_ERROR_PERMISSION );

		/* Open the file in read-only mode */
		*isReadOnly = TRUE;
		openMode = FILE_FLAG_READ;
		}
	else
		{
		/* If we're creating the file, open it in write-only mode.  Since
		   we'll (presumably) be storing private keys in it we mark it as
		   both private (owner-access-only ACL) and sensitive (store in
		   secure storage if possible) */
		if( options == CRYPT_KEYOPT_CREATE )
			{
			openMode = FILE_FLAG_WRITE | FILE_FLAG_EXCLUSIVE_ACCESS | \
					   FILE_FLAG_PRIVATE | FILE_FLAG_SENSITIVE;
			}
		else
			{
			/* Open it for read or read/write depending on whether the
			   readonly flag is set */
			openMode = ( options == CRYPT_KEYOPT_READONLY ) ? \
					   FILE_FLAG_READ : FILE_FLAG_READ | FILE_FLAG_WRITE;
			}
		}
	if( options == CRYPT_IKEYOPT_EXCLUSIVEACCESS )
		openMode |= FILE_FLAG_EXCLUSIVE_ACCESS;

	/* Pre-open the file containing the keyset.  This initially opens it in
	   read-only mode for auto-detection of the file type so we can check for
	   various problems */
	status = sFileOpen( stream, nameBuffer, FILE_FLAG_READ );
	if( cryptStatusError( status ) )
		{
		/* The file can't be opened, if the create-new-file flag isn't set 
		   return an error.  If it is set, make sure that we're trying to 
		   create a writeable keyset type */
		if( options != CRYPT_KEYOPT_CREATE )
			return( status );
		if( !isWriteableFileKeyset( subType ) )
			return( CRYPT_ERROR_NOTAVAIL );

		/* Try and create a new file */
		status = sFileOpen( stream, nameBuffer, openMode );
		if( cryptStatusError( status ) )
			{
			/* The file isn't open at this point so we have to exit 
			   explicitly rather than falling through to the error handler
			   below */
			return( status );
			}
		}
	else
		{
		/* If we're opening an existing keyset, get its type and make sure
		   that it's valid */
		if( options != CRYPT_KEYOPT_CREATE )
			{
			BYTE buffer[ SAFEBUFFER_SIZE( 512 ) + 8 ] STACK_ALIGN_DATA;

			memset( buffer, 0, 512 );	/* Keep static analysers happy */
			safeBufferInit( SAFEBUFFER_PTR( buffer ), 512 );
			sioctlSetString( stream, STREAM_IOCTL_IOBUFFER, 
							 SAFEBUFFER_PTR( buffer ), 512 );
			status = getKeysetType( stream, &subType );
			if( cryptStatusError( status ) )
				{
				/* "It doesn't look like anything from here" */
				sFileClose( stream );
				return( CRYPT_ERROR_BADDATA );
				}
			sseek( stream, 0 );
			sioctlSet( stream, STREAM_IOCTL_IOBUFFER, 0 );
			}

		/* If it's a cryptlib keyset we can open it in any mode */
		if( isWriteableFileKeyset( subType ) )
			{
			/* If we're opening it something other than read-only mode, 
			   reopen it in that mode.  Note that in theory this could make 
			   us subject to a TOCTTOU attack but the only reason that we're 
			   opening the file initially is to determine its type, so if an 
			   attacker slips in a different file on the re-open it'll 
			   either be a no-op if it's the same file type or we'll get a
			   CRYPT_ERROR_BADDATA if it's the same file type */
			if( openMode != FILE_FLAG_READ )
				{
				sFileClose( stream );
				status = sFileOpen( stream, nameBuffer, openMode );
				if( cryptStatusError( status ) )
					return( status );	/* Exit with file closed */
				}
			}
		else
			{
			/* If it's a non-cryptlib keyset we can't open it for anything 
			   other than read-only access.  We return a not-available error 
			   rather than a permission error since this isn't a problem with
			   access permissions for the file but the fact that the code to
			   write the key doesn't exist */
			if( options != CRYPT_KEYOPT_READONLY )
				status = CRYPT_ERROR_NOTAVAIL;
			}
		}
	if( cryptStatusError( status ) )
		{
		sFileClose( stream );
		return( status );
		}

	*keysetSubType = subType;
	return( CRYPT_OK );
	}

/* Some flat-file keysets have subtype-specific access restrictions that 
   are too specific to be captured by the general ACLs.  To handle these, we
   need to provide subtype-specific checking, which is handled by the 
   following function */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN isFileKeysetAccessPermitted( INOUT KEYSET_INFO *keysetInfoPtr, 
											IN_ENUM( KEYMGMT_ITEM ) \
												const KEYMGMT_ITEM_TYPE accessType,
											const BOOLEAN isRead )
	{
	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES_B( keysetInfoPtr->type == KEYSET_FILE );
	REQUIRES_B( isEnumRange( accessType, KEYMGMT_ITEM ) );
	REQUIRES_B( isRead == TRUE || isRead == FALSE );

	switch( keysetInfoPtr->subType )
		{
		case KEYSET_SUBTYPE_PGP_PUBLIC:
			/* PGP keysets have odd requirements for write in that a private
			   key is required in order for it to be written to a public 
			   keyring if it's a signing key.  This is because of the 
			   requirement to have signed metadata associated with the key, 
			   which requires the presence of a private key */
			if( accessType == KEYMGMT_ITEM_PUBLICKEY && isRead )
				return( TRUE );
			if( ( accessType == KEYMGMT_ITEM_PRIVATEKEY || \
				  accessType == KEYMGMT_ITEM_PUBLICKEY ) && !isRead )
				return( TRUE );
			return( FALSE );

		case KEYSET_SUBTYPE_PGP_PRIVATE:
			if( ( accessType == KEYMGMT_ITEM_PRIVATEKEY || \
				  accessType == KEYMGMT_ITEM_PUBLICKEY ) && isRead )
				return( TRUE );
			return( FALSE );

		case KEYSET_SUBTYPE_PKCS12:
			if( accessType == KEYMGMT_ITEM_PRIVATEKEY || \
				accessType == KEYMGMT_ITEM_PUBLICKEY )
				return( TRUE );
			return( FALSE );

		case KEYSET_SUBTYPE_PKCS15:
			if( accessType == KEYMGMT_ITEM_PRIVATEKEY || \
				accessType == KEYMGMT_ITEM_PUBLICKEY || \
				accessType == KEYMGMT_ITEM_SECRETKEY || \
				accessType == KEYMGMT_ITEM_DATA || \
				accessType == KEYMGMT_ITEM_KEYMETADATA )
				return( TRUE );
			return( FALSE );
		}

	retIntError_Boolean();
	}

/* Complete the open of a file keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
static int completeKeysetFileOpen( INOUT KEYSET_INFO *keysetInfoPtr,
								   IN_ENUM( KEYSET_SUBTYPE ) \
										KEYSET_SUBTYPE subType,
								   INOUT STREAM *stream,
								   IN_BUFFER( nameLength ) const char *name, 
								   IN_LENGTH_SHORT_MIN( MIN_NAME_LENGTH ) \
										const int nameLength,
								   IN_ENUM_OPT( CRYPT_KEYOPT ) \
										const CRYPT_KEYOPT_TYPE options )
	{
	FILE_INFO *fileInfo = keysetInfoPtr->keysetFile;
	BYTE buffer[ SAFEBUFFER_SIZE( STREAM_BUFSIZE ) + 8 ] STACK_ALIGN_DATA;
	KEY_INIT_FUNCTION initFunction;
	int status;

	assert( isWritePtr( keysetInfoPtr, sizeof( KEYSET_INFO ) ) );
	assert( isReadPtrDynamic( name, nameLength ) );
	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	REQUIRES( isEnumRange( subType, KEYSET_SUBTYPE ) );
	REQUIRES( nameLength >= MIN_NAME_LENGTH && \
			  nameLength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( isEnumRangeOpt( options, CRYPT_KEYOPT ) );

	/* Remember the key file's name (as a null-terminated string for 
	   filesystem access) and I/O stream */
	if( nameLength > MAX_PATH_LENGTH - 1 )
		return( CRYPT_ARGERROR_STR1 );
	keysetInfoPtr->subType = subType;
	REQUIRES( rangeCheck( nameLength, 1, MAX_PATH_LENGTH - 1 ) );
	memcpy( fileInfo->fileName, name, nameLength );
	fileInfo->fileName[ nameLength ] = '\0';
	memcpy( &fileInfo->stream, stream, sizeof( STREAM ) );

	/* Set various values to their default settings */
	fileInfo->iHardwareDevice = CRYPT_UNUSED;

	/* Make sure that we don't accidentally reuse the standalone stream */
	memset( stream, 0, sizeof( STREAM ) );

	/* Set up the access information for the file */
	switch( keysetInfoPtr->subType )
		{
		case KEYSET_SUBTYPE_PKCS12:
			status = setAccessMethodPKCS12( keysetInfoPtr );
			break;

		case KEYSET_SUBTYPE_PKCS15:
			status = setAccessMethodPKCS15( keysetInfoPtr );
			break;

		case KEYSET_SUBTYPE_PGP_PUBLIC:
			status = setAccessMethodPGPPublic( keysetInfoPtr );
			break;

		case KEYSET_SUBTYPE_PGP_PRIVATE:
			status = setAccessMethodPGPPrivate( keysetInfoPtr );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		{
		/* Normally if an access method is unavailable we'd return
		   CRYPT_ARGERROR_NUM1 to indicate that the overall CRYPT_KEYSET_xxx
		   type isn't supported, however in the case of CRYPT_KEYSET_FILE
		   we're dealing with subtypes rather than the CRYPT_KEYSET_FILE in
		   general.  To deal with this, if the subtype is anything other than
		   PKCS #15 files then we report it as CRYPT_ERROR_NOTAVAIL to indicate
		   that while CRYPT_KEYSET_FILE as a whole may be supported, this
		   particular subtype isn't.  For PKCS #15 files, the generic "file
		   keyset", we report it as a standard CRYPT_ARGERROR_NUM1 */
		if( status == CRYPT_ARGERROR_NUM1 && \
			subType != KEYSET_SUBTYPE_PKCS15 )
			return( CRYPT_ERROR_NOTAVAIL );

		return( status );
		}
	ENSURES( checkKeysetFunctions( keysetInfoPtr ) );

	/* Read the keyset contents into memory */
	initFunction = ( KEY_INIT_FUNCTION ) \
				   FNPTR_GET( keysetInfoPtr->initFunction );
	REQUIRES( initFunction != NULL );
	memset( buffer, 0, min( 16, STREAM_BUFSIZE ) );	
			/* Keep static analysers happy */
	safeBufferInit( SAFEBUFFER_PTR( buffer ), STREAM_BUFSIZE );
	sioctlSetString( &fileInfo->stream, STREAM_IOCTL_IOBUFFER, 
					 SAFEBUFFER_PTR( buffer ), STREAM_BUFSIZE );
	status = initFunction( keysetInfoPtr, NULL, 0, 
						   ( options == CRYPT_KEYOPT_CREATE ) ? \
							 CRYPT_KEYOPT_CREATE : CRYPT_KEYOPT_NONE );
	sioctlSet( &fileInfo->stream, STREAM_IOCTL_IOBUFFER, 0 );
	if( cryptStatusError( status ) )
		{
		/* Since this function is called on object creation, if it fails 
		   there's no object to get extended error information from so we 
		   dump the error info as a diagnostic for debugging purposes */
		DEBUG_DIAG_ERRMSG(( "Keyset open failed, status %s, error string:\n  %s", 
							getStatusName( status ),
							getErrorInfoString( &keysetInfoPtr->errorInfo ) ));
		return( status );
		}

	/* If we've got the keyset open in read-only mode then we don't need to 
	   touch it again since everything is cached in-memory, so we can close 
	   the file stream */
	if( ( keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 || \
		  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 || \
		  keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PRIVATE ) && \
		( options == CRYPT_KEYOPT_READONLY ) )
		sFileClose( &fileInfo->stream );
	else
		{
		/* Remember that the stream is still open for further access */
		SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_STREAM_OPEN );
		}
	SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_OPEN );
	if( options == CRYPT_KEYOPT_CREATE )
		SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_EMPTY );
	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Keyset Message Handler						*
*																			*
****************************************************************************/

/* Handle a message sent to a keyset object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int keysetMessageFunction( INOUT TYPECAST( KEYSET_INFO * ) \
									void *objectInfoPtr,
								  IN_MESSAGE const MESSAGE_TYPE message,
								  void *messageDataPtr,
								  IN_INT_Z const int messageValue )
	{
	KEYSET_INFO *keysetInfoPtr = ( KEYSET_INFO * ) objectInfoPtr;
	int status;

	assert( isWritePtr( objectInfoPtr, sizeof( KEYSET_INFO ) ) );

	REQUIRES( message == MESSAGE_DESTROY || \
			  sanityCheckKeyset( keysetInfoPtr ) );
	REQUIRES( isEnumRange( message, MESSAGE ) );
	REQUIRES( isShortIntegerRange( messageValue ) );

	/* Process the destroy object message */
	if( message == MESSAGE_DESTROY )
		{
		/* If the keyset is active, perform any required cleanup functions */
		if( TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_OPEN ) )
			{
			const KEY_SHUTDOWN_FUNCTION shutdownFunction = \
						( KEY_SHUTDOWN_FUNCTION ) \
						FNPTR_GET( keysetInfoPtr->shutdownFunction );

			REQUIRES( shutdownFunction != NULL );

			/* Shut down the keyset */
			status = shutdownFunction( keysetInfoPtr );
			if( cryptStatusError( status ) )
				{
				assert( INTERNAL_ERROR );

				/* The shutdown failed for some reason.  This can only 
				   really ever happen for file keysets, in general there's 
				   not much that we can do about this (see the long comment
				   about file-close failure conditions in io/file.c), 
				   however in order to avoid leaving a potentially corrupted
				   file on disk we try and delete it if the shutdown fails.
				   (There are a pile of tradeoffs to be made here, for 
				   example in theory we could rename the file to something
				   like .bak so that the user could try and recover 
				   whatever's left in there, however it's unlikely that 
				   they'll be able to do much with an unknown-condition 
				   binary blob and in any case since we have no idea what
				   condition the file is in it's probably best to remove it
				   rather than to leave who knows what lying around on 
				   disk) */
				if( keysetInfoPtr->type == KEYSET_FILE && \
					TEST_FLAG( keysetInfoPtr->flags, 
							   KEYSET_FLAG_STREAM_OPEN ) )
					{
					sFileClose( &keysetInfoPtr->keysetFile->stream );
					fileErase( keysetInfoPtr->keysetFile->fileName );
					}
				}

			/* If it's a non-file keyset or a file keyset without an open
			   stream, we're done.  Since we cache all information in a file 
			   keyset and close the stream immediately afterwards if we've 
			   opened it in read-only mode, we only close the underlying 
			   stream for a file keyset if it's still active.  Note the 
			   distinction between the keyset being active and the stream 
			   being active, for file keysets the keyset can be active 
			   without being associated with an open stream */
			if( keysetInfoPtr->type != KEYSET_FILE || \
				!TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_STREAM_OPEN ) )
				return( CRYPT_OK );

			/* The keyset has an open file stream */
			REQUIRES( keysetInfoPtr->type == KEYSET_FILE && \
					  TEST_FLAG( keysetInfoPtr->flags, 
								 KEYSET_FLAG_STREAM_OPEN ) );

			/* If the file keyset was updated in any way the update may have 
			   changed the overall file size, in which case we need to clear 
			   any leftover data from the previous version of the keyset 
			   before we close the file */
			if( TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY ) )
				fileClearToEOF( &keysetInfoPtr->keysetFile->stream );

			/* Close the keyset file (the keyset-specific handler sees only 
			   an I/O stream and doesn't perform any file-level functions, 
			   so we have to do this here) */
			status = sFileClose( &keysetInfoPtr->keysetFile->stream );
			if( cryptStatusError( status ) )
				{
				/* Try and remove the keyset if the file close failed and 
				   would have left the file in an indeterminate state, see 
				   the comment in io/file.c for more information */
				fileErase( keysetInfoPtr->keysetFile->fileName );
				}
			else
				{
				/* If it's a newly-created empty keyset file or one in which 
				   all of the keys have been deleted, remove it.  This 
				   situation can occur if there's some sort of error on 
				   writing and no keys are ever written to the keyset */
				if( TEST_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_EMPTY ) )
					fileErase( keysetInfoPtr->keysetFile->fileName );
				}
			}

		return( CRYPT_OK );
		}

	/* Process attribute get/set/delete messages */
	if( isAttributeMessage( message ) )
		{
		REQUIRES( message == MESSAGE_GETATTRIBUTE || \
				  message == MESSAGE_GETATTRIBUTE_S || \
				  message == MESSAGE_SETATTRIBUTE || \
				  message == MESSAGE_SETATTRIBUTE_S );

		/* If it's a keyset-specific attribute, forward it directly to
		   the low-level code */
#ifdef USE_LDAP
		if( messageValue >= CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS && \
			messageValue <= CRYPT_OPTION_KEYS_LDAP_EMAILNAME )
			{
			REQUIRES( keysetInfoPtr->type == KEYSET_LDAP );

			if( message == MESSAGE_SETATTRIBUTE || \
				message == MESSAGE_SETATTRIBUTE_S )
				{
				const KEY_SETATTRIBUTE_FUNCTION setAttributeFunction = \
							( KEY_SETATTRIBUTE_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->setAttributeFunction );

				REQUIRES( setAttributeFunction != NULL );

				status = setAttributeFunction( keysetInfoPtr, messageDataPtr, 
											   messageValue );
				if( status == CRYPT_ERROR_INITED )
					{
					setErrorInfo( keysetInfoPtr, messageValue, 
								  CRYPT_ERRTYPE_ATTR_PRESENT );
					return( CRYPT_ERROR_INITED );
					}
				}
			else
				{
				const KEY_GETATTRIBUTE_FUNCTION getAttributeFunction = \
							( KEY_GETATTRIBUTE_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->getAttributeFunction );

				REQUIRES( message == MESSAGE_GETATTRIBUTE || \
						  message == MESSAGE_GETATTRIBUTE_S );
				REQUIRES( getAttributeFunction != NULL );

				status = getAttributeFunction( keysetInfoPtr, messageDataPtr, 
											   messageValue );
				if( status == CRYPT_ERROR_NOTFOUND )
					{
					setErrorInfo( keysetInfoPtr, messageValue, 
								  CRYPT_ERRTYPE_ATTR_ABSENT );
					return( CRYPT_ERROR_NOTFOUND );
					}
				}
			return( status );
			}
#endif /* USE_LDAP */

		if( message == MESSAGE_GETATTRIBUTE )
			{
			return( getKeysetAttribute( keysetInfoPtr, 
										( int * ) messageDataPtr,
										messageValue ) );
			}
		if( message == MESSAGE_GETATTRIBUTE_S )
			{
			return( getKeysetAttributeS( keysetInfoPtr, 
										 ( MESSAGE_DATA * ) messageDataPtr,
										 messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE )
			{
			/* CRYPT_IATTRIBUTE_INITIALISED is purely a notification message 
			   with no parameters so we don't pass it down to the attribute-
			   handling code */
			if( messageValue == CRYPT_IATTRIBUTE_INITIALISED )
				return( CRYPT_OK );

			return( setKeysetAttribute( keysetInfoPtr, 
										*( ( int * ) messageDataPtr ),
										messageValue ) );
			}
		if( message == MESSAGE_SETATTRIBUTE_S )
			{
			const MESSAGE_DATA *msgData = ( MESSAGE_DATA * ) messageDataPtr;

			return( setKeysetAttributeS( keysetInfoPtr, msgData->data, 
										 msgData->length, messageValue ) );
			}

		retIntError();
		}

	/* Process messages that check a keyset */
	if( message == MESSAGE_CHECK )
		{
		/* The check for whether this keyset type can contain an object that 
		   can perform the requested operation has already been performed by 
		   the kernel so there's nothing further to do here */
		REQUIRES( ( messageValue != MESSAGE_CHECK_PKC_PRIVATE && \
					messageValue != MESSAGE_CHECK_PKC_DECRYPT && \
					messageValue != MESSAGE_CHECK_PKC_DECRYPT_AVAIL && \
					messageValue != MESSAGE_CHECK_PKC_SIGN && \
					messageValue != MESSAGE_CHECK_PKC_SIGN_AVAIL ) || 
				  ( keysetInfoPtr->type != KEYSET_DBMS && \
					keysetInfoPtr->type != KEYSET_LDAP && \
					keysetInfoPtr->type != KEYSET_HTTP ) );

		return( CRYPT_OK );
		}

	/* Process object-specific messages */
	if( message == MESSAGE_KEY_GETKEY )
		{
		const KEY_GETITEM_FUNCTION getItemFunction = \
							( KEY_GETITEM_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->getItemFunction );
		MESSAGE_KEYMGMT_INFO *getkeyInfo = \
							( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;
		CONST_INIT_STRUCT_3( KEYID_INFO keyIDinfo, \
							 getkeyInfo->keyIDtype, getkeyInfo->keyID, \
							 getkeyInfo->keyIDlength );
		BYTE keyIDbuffer[ KEYID_SIZE + 8 ];

		CONST_SET_STRUCT( keyIDinfo.keyIDtype = getkeyInfo->keyIDtype; \
						  keyIDinfo.keyID = getkeyInfo->keyID; \
						  keyIDinfo.keyIDlength = getkeyInfo->keyIDlength );

		REQUIRES( keyIDinfo.keyIDtype != CRYPT_KEYID_NONE && \
				  keyIDinfo.keyID != NULL && \
				  keyIDinfo.keyIDlength >= MIN_NAME_LENGTH && \
				  keyIDinfo.keyIDlength < MAX_ATTRIBUTE_SIZE );
		REQUIRES( messageValue != KEYMGMT_ITEM_PRIVATEKEY || \
				  keysetInfoPtr->type == KEYSET_FILE );
		REQUIRES( ( messageValue != KEYMGMT_ITEM_SECRETKEY && \
					messageValue != KEYMGMT_ITEM_DATA ) || \
				  ( keysetInfoPtr->type == KEYSET_FILE && \
					keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 ) );
		REQUIRES( ( messageValue != KEYMGMT_ITEM_REQUEST && \
					messageValue != KEYMGMT_ITEM_REVREQUEST && \
					messageValue != KEYMGMT_ITEM_REVOCATIONINFO && \
					messageValue != KEYMGMT_ITEM_PKIUSER ) || \
				  keysetInfoPtr->type == KEYSET_DBMS );
		REQUIRES( getItemFunction != NULL );

		/* Get the key */
		resetErrorInfo( keysetInfoPtr );
		status = initKeysetUpdate( keysetInfoPtr, &keyIDinfo, keyIDbuffer,
								   KEYID_SIZE, TRUE );
		if( cryptStatusError( status ) )
			return( status );
		if( keysetInfoPtr->type == KEYSET_FILE && \
			!isFileKeysetAccessPermitted( keysetInfoPtr, messageValue, 
										  TRUE ) )
			return( CRYPT_ARGERROR_OBJECT );
		return( getItemFunction( keysetInfoPtr, &getkeyInfo->cryptHandle, 
							messageValue, keyIDinfo.keyIDtype, 
							keyIDinfo.keyID, keyIDinfo.keyIDlength, 
							getkeyInfo->auxInfo, &getkeyInfo->auxInfoLength, 
							getkeyInfo->flags ) );
		}
	if( message == MESSAGE_KEY_SETKEY )
		{
		const KEY_SETITEM_FUNCTION setItemFunction = \
							( KEY_SETITEM_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->setItemFunction );
		MESSAGE_KEYMGMT_INFO *setkeyInfo = \
							( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( messageValue != KEYMGMT_ITEM_PRIVATEKEY || \
				  ( keysetInfoPtr->type == KEYSET_FILE && \
					( keysetInfoPtr->subType == KEYSET_SUBTYPE_PGP_PUBLIC || \
					  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 || \
					  keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS12 ) ) );
		REQUIRES( ( messageValue != KEYMGMT_ITEM_SECRETKEY && \
					messageValue != KEYMGMT_ITEM_DATA && \
					messageValue != KEYMGMT_ITEM_KEYMETADATA ) || \
				  ( keysetInfoPtr->type == KEYSET_FILE && \
					keysetInfoPtr->subType == KEYSET_SUBTYPE_PKCS15 ) );
		REQUIRES( ( messageValue != KEYMGMT_ITEM_REQUEST && \
					messageValue != KEYMGMT_ITEM_REVREQUEST && \
					messageValue != KEYMGMT_ITEM_REVOCATIONINFO && \
					messageValue != KEYMGMT_ITEM_PKIUSER ) || \
				  ( keysetInfoPtr->type == KEYSET_DBMS ) );
		REQUIRES( setItemFunction != NULL );

		/* Set the key.  This is currently the only way to associate a 
		   certificate with a context (that is, it's not possible to add a 
		   certificate to an existing context directly).  At first glance 
		   this should be possible since the required access checks are 
		   performed by the kernel: The object is of the correct type (a 
		   certificate), in the high state (it's been signed), and the 
		   certificate owner and context owner are the same.  However the 
		   actual process of attaching the certificate to the context is 
		   quite tricky.  The certificate will have a public-key context 
		   already attached to it from when the certificate was created or 
		   imported.  In order to attach this to the other context we'd need 
		   to first destroy the context associated with the certificate and 
		   then replace it with the other context, which is both messy and 
		   non-atomic.  There are also complications surrounding use with 
		   devices, where contexts aren't really full cryptlib objects but 
		   just dummy values that point back to the device for handling of 
		   operations.  Going via a keyset/device bypasses these issues, but 
		   doing it directly shows up all of these problems */
		resetErrorInfo( keysetInfoPtr );
		status = initKeysetUpdate( keysetInfoPtr, NULL, NULL, 0, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		if( keysetInfoPtr->type == KEYSET_FILE && \
			!isFileKeysetAccessPermitted( keysetInfoPtr, messageValue, 
										  FALSE ) )
			return( CRYPT_ARGERROR_OBJECT );
		status = setItemFunction( keysetInfoPtr, setkeyInfo->cryptHandle, 
							messageValue, setkeyInfo->auxInfo, 
							setkeyInfo->auxInfoLength, setkeyInfo->flags );
		if( cryptStatusError( status ) )
			return( status );

		/* The update succeeded, remember that the data in the keyset has 
		   changed */
		SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY );
		CLEAR_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_EMPTY );

		return( CRYPT_OK );
		}
	if( message == MESSAGE_KEY_DELETEKEY )
		{
		const KEY_DELETEITEM_FUNCTION deleteItemFunction = \
							( KEY_DELETEITEM_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->deleteItemFunction );
		MESSAGE_KEYMGMT_INFO *deletekeyInfo = \
							( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;
		CONST_INIT_STRUCT_3( KEYID_INFO keyIDinfo, \
							 deletekeyInfo->keyIDtype, deletekeyInfo->keyID, \
							 deletekeyInfo->keyIDlength );
		BYTE keyIDbuffer[ KEYID_SIZE + 8 ];

		CONST_SET_STRUCT( keyIDinfo.keyIDtype = deletekeyInfo->keyIDtype; \
						  keyIDinfo.keyID = deletekeyInfo->keyID; \
						  keyIDinfo.keyIDlength = deletekeyInfo->keyIDlength );

		REQUIRES( keyIDinfo.keyIDtype != CRYPT_KEYID_NONE && \
				  keyIDinfo.keyID != NULL && \
				  keyIDinfo.keyIDlength >= MIN_NAME_LENGTH && \
				  keyIDinfo.keyIDlength < MAX_ATTRIBUTE_SIZE );
		REQUIRES( deleteItemFunction != NULL );

		/* Delete the key */
		resetErrorInfo( keysetInfoPtr );
		status = initKeysetUpdate( keysetInfoPtr, &keyIDinfo, keyIDbuffer,
								   KEYID_SIZE, FALSE );
		if( cryptStatusError( status ) )
			return( status );
		status = deleteItemFunction( keysetInfoPtr, messageValue, 
							keyIDinfo.keyIDtype, keyIDinfo.keyID, 
							keyIDinfo.keyIDlength );
		if( cryptStatusOK( status ) )
			{
			/* The update succeeded, remember that the data in the keyset 
			   has changed */
			SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY );
			}
		return( status );
		}
	if( message == MESSAGE_KEY_GETFIRSTCERT )
		{
		const KEY_GETFIRSTITEM_FUNCTION getFirstItemFunction = \
							( KEY_GETFIRSTITEM_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->getFirstItemFunction );
		MESSAGE_KEYMGMT_INFO *getnextcertInfo = \
							( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;
		CONST_INIT_STRUCT_3( KEYID_INFO keyIDinfo, \
							 getnextcertInfo->keyIDtype, getnextcertInfo->keyID, \
							 getnextcertInfo->keyIDlength );
		BYTE keyIDbuffer[ KEYID_SIZE + 8 ];

		CONST_SET_STRUCT( keyIDinfo.keyIDtype = getnextcertInfo->keyIDtype; \
						  keyIDinfo.keyID = getnextcertInfo->keyID; \
						  keyIDinfo.keyIDlength = getnextcertInfo->keyIDlength );

		REQUIRES( keyIDinfo.keyIDtype != CRYPT_KEYID_NONE && \
				  keyIDinfo.keyID != NULL && \
				  keyIDinfo.keyIDlength >= MIN_NAME_LENGTH && \
				  keyIDinfo.keyIDlength < MAX_ATTRIBUTE_SIZE );
		REQUIRES( getnextcertInfo->auxInfo != NULL && \
				  getnextcertInfo->auxInfoLength == sizeof( int ) );
		REQUIRES( getFirstItemFunction != NULL );

		/* Fetch the first certificate in a sequence from the keyset */
		resetErrorInfo( keysetInfoPtr );
		status = initKeysetUpdate( keysetInfoPtr, &keyIDinfo, keyIDbuffer, 
								   KEYID_SIZE, TRUE );
		if( cryptStatusError( status ) )
			return( status );
		return( getFirstItemFunction( keysetInfoPtr,
							&getnextcertInfo->cryptHandle, 
							getnextcertInfo->auxInfo, messageValue, 
							keyIDinfo.keyIDtype, keyIDinfo.keyID, 
							keyIDinfo.keyIDlength, getnextcertInfo->flags ) );
		}
	if( message == MESSAGE_KEY_GETNEXTCERT )
		{
		const KEY_GETNEXTITEM_FUNCTION getNextItemFunction = \
							( KEY_GETNEXTITEM_FUNCTION ) \
							FNPTR_GET( keysetInfoPtr->getNextItemFunction );
		MESSAGE_KEYMGMT_INFO *getnextcertInfo = \
								( MESSAGE_KEYMGMT_INFO * ) messageDataPtr;

		REQUIRES( getnextcertInfo->keyIDtype == CRYPT_KEYID_NONE && \
				  getnextcertInfo->keyID == NULL && \
				  getnextcertInfo->keyIDlength == 0 );
		REQUIRES( ( getnextcertInfo->auxInfo == NULL && \
					getnextcertInfo->auxInfoLength == 0 ) || \
				  ( getnextcertInfo->auxInfo != NULL && \
					getnextcertInfo->auxInfoLength == sizeof( int ) ) );
				  /* The state variable may be absent for a one-off
					 fetch */
		REQUIRES( isFlagRangeZ( getnextcertInfo->flags, KEYMGMT ) && \
				  ( getnextcertInfo->flags & ~KEYMGMT_MASK_CERTOPTIONS ) == 0 );
		REQUIRES( getNextItemFunction != NULL );

		/* Fetch the next certificate in a sequence from the keyset */
		return( getNextItemFunction( keysetInfoPtr, 
							&getnextcertInfo->cryptHandle, 
							getnextcertInfo->auxInfo, getnextcertInfo->flags ) );
		}
#ifdef USE_DBMS
	if( message == MESSAGE_KEY_CERTMGMT )
		{
		MESSAGE_CERTMGMT_INFO *certMgmtInfo = \
								( MESSAGE_CERTMGMT_INFO * ) messageDataPtr;

		REQUIRES( messageValue >= CRYPT_CERTACTION_CERT_CREATION && \
				  messageValue <= CRYPT_CERTACTION_LAST_USER );

		/* Perform the certificate management operation */
		resetErrorInfo( keysetInfoPtr );
		status = initKeysetUpdate( keysetInfoPtr, NULL, NULL, 0, TRUE );
		if( cryptStatusError( status ) )
			return( status );
		status = keysetInfoPtr->keysetDBMS->certMgmtFunction( keysetInfoPtr,
							( certMgmtInfo->cryptCert != CRYPT_UNUSED ) ? \
								&certMgmtInfo->cryptCert : NULL, 
							certMgmtInfo->caKey, certMgmtInfo->request, 
							messageValue );
		if( cryptStatusOK( status ) )
			{
			/* The update succeeded, remember that the data in the keyset has
			   changed */
			SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_DIRTY );
			}
		return( status );
		}
#endif /* USE_DBMS */

	retIntError();
	}

/* Open a keyset.  This is a low-level function encapsulated by createKeyset()
   and used to manage error exits */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 7 ) ) \
static int openKeyset( OUT_HANDLE_OPT CRYPT_KEYSET *iCryptKeyset,
					   IN_HANDLE const CRYPT_USER iCryptOwner,
					   IN_ENUM( CRYPT_KEYSET ) const CRYPT_KEYSET_TYPE keysetType,
					   IN_BUFFER( nameLength ) const char *name, 
					   IN_LENGTH_SHORT_MIN( MIN_NAME_LENGTH ) const int nameLength,
					   IN_ENUM_OPT( CRYPT_KEYOPT ) const CRYPT_KEYOPT_TYPE options,
					   OUT_PTR_OPT KEYSET_INFO **keysetInfoPtrPtr )
	{
	KEYSET_INFO *keysetInfoPtr;
	STREAM stream;
	KEYSET_SUBTYPE keysetSubType DUMMY_INIT;
	OBJECT_SUBTYPE subType;
	KEY_INIT_FUNCTION initFunction;
	BOOLEAN isReadOnly = ( options == CRYPT_KEYOPT_READONLY ) ? TRUE : FALSE;
	int storageSize, status;

	assert( isWritePtr( iCryptKeyset, sizeof( CRYPT_KEYSET ) ) );
	assert( isReadPtrDynamic( name, nameLength ) );
	assert( isWritePtr( keysetInfoPtrPtr, sizeof( KEYSET_INFO * ) ) );

	REQUIRES( ( iCryptOwner == DEFAULTUSER_OBJECT_HANDLE ) || \
			  isHandleRangeValid( iCryptOwner ) );
	REQUIRES( isEnumRange( keysetType, CRYPT_KEYSET ) );
	REQUIRES( nameLength >= MIN_NAME_LENGTH && \
			  nameLength < MAX_ATTRIBUTE_SIZE );
	REQUIRES( isEnumRangeOpt( options, CRYPT_KEYOPT ) );

	/* Clear the return values */
	*iCryptKeyset = CRYPT_ERROR;
	*keysetInfoPtrPtr = NULL;

	/* Perform general checks that can be done before we create the object */
	if( ( keysetType == CRYPT_KEYSET_HTTP && \
		  options != CRYPT_KEYOPT_READONLY ) || \
		( keysetType == CRYPT_KEYSET_LDAP && \
		  options == CRYPT_KEYOPT_CREATE ) )
		{
		/* We can't open an HTTP keyset for anything other than read-only
		   access, and we can't create an LDAP directory */
		return( CRYPT_ERROR_PERMISSION );
		}
	if( keysetType == CRYPT_KEYSET_FILE && nameLength > MAX_PATH_LENGTH - 1 )
		return( CRYPT_ARGERROR_STR1 );

	/* Set up subtype-specific information */
	switch( keysetType )
		{
		case CRYPT_KEYSET_FILE:
			subType = SUBTYPE_KEYSET_FILE_READONLY;
			storageSize = sizeof( FILE_INFO );
			break;

		case CRYPT_KEYSET_HTTP:
			subType = SUBTYPE_KEYSET_HTTP;
			storageSize = sizeof( HTTP_INFO );
			break;

		case CRYPT_KEYSET_LDAP:
			subType = SUBTYPE_KEYSET_LDAP;
			storageSize = sizeof( LDAP_INFO );
			break;

		case CRYPT_KEYSET_ODBC:
		case CRYPT_KEYSET_DATABASE:
			subType = SUBTYPE_KEYSET_DBMS;
			storageSize = sizeof( DBMS_INFO );
			break;

		case CRYPT_KEYSET_ODBC_STORE:
		case CRYPT_KEYSET_DATABASE_STORE:
			subType = SUBTYPE_KEYSET_DBMS_STORE;
			storageSize = sizeof( DBMS_INFO );
			break;
		
		default:
			retIntError();
		}

	/* Handle compiler warnings of uninitialised variables, unfortunately 
	   since it's non-scalar data we can't do this with the usual 
	   DUMMY_INIT */
	memset( &stream, 0, sizeof( STREAM ) );	

	/* If it's a flat-file keyset which is implemented on top of an I/O 
	   stream make sure that we can open the stream before we try and 
	   create the keyset object */
	if( keysetType == CRYPT_KEYSET_FILE )
		{
		status = openKeysetStream( &stream, name, nameLength, options, 
								   &isReadOnly, &keysetSubType );
		if( cryptStatusError( status ) )
			return( status );

		/* If the keyset contains the full set of search keys and index
		   information needed to handle all keyset operations (e.g. 
		   certificate chain building, query by key usage types) we mark it 
		   as a full-function keyset with the same functionality as a DBMS 
		   keyset rather than just a generic flat-file store */
		if( keysetSubType == KEYSET_SUBTYPE_PKCS15 )
			subType = SUBTYPE_KEYSET_FILE;

		/* If it's a limited keyset type that nonetheless allows writing
		   at least one public/private key, mark it as a restricted-function
		   keyset */
#ifdef USE_PKCS12
		if( keysetSubType == KEYSET_SUBTYPE_PKCS12 )
			subType = SUBTYPE_KEYSET_FILE_PARTIAL;
#endif /* USE_PKCS12 */
#ifdef USE_PGP
		if( keysetSubType == KEYSET_SUBTYPE_PGP_PUBLIC )
			subType = SUBTYPE_KEYSET_FILE_PARTIAL;
#endif /* USE_PGP */

		/* Make sure that the open-mode that's been specified is compatible
		   with the object subtype */
		switch( subType )
			{
			case SUBTYPE_KEYSET_FILE:
				/* All access modes allowed */
				break;

			case SUBTYPE_KEYSET_FILE_PARTIAL:
				/* Update access not allowed */
				if( options != CRYPT_KEYOPT_READONLY && \
					options != CRYPT_KEYOPT_CREATE )
					return( CRYPT_ARGERROR_NUM2 );
				break;

			case SUBTYPE_KEYSET_FILE_READONLY:
				/* Only read access allowed */
				if( options != CRYPT_KEYOPT_READONLY )
					return( CRYPT_ARGERROR_NUM2 );
				break;

			default:
				retIntError();
			}
		}

	/* Create the keyset object */
	status = krnlCreateObject( iCryptKeyset, ( void ** ) &keysetInfoPtr, 
							   sizeof( KEYSET_INFO ) + storageSize, 
							   OBJECT_TYPE_KEYSET, subType, 
							   CREATEOBJECT_FLAG_NONE, iCryptOwner, 
							   ACTION_PERM_NONE_ALL, keysetMessageFunction );
	if( cryptStatusError( status ) )
		{
		if( keysetType == CRYPT_KEYSET_FILE )
			sFileClose( &stream );
		return( status );
		}
	ANALYSER_HINT( keysetInfoPtr != NULL );
	*keysetInfoPtrPtr = keysetInfoPtr;
	keysetInfoPtr->objectHandle = *iCryptKeyset;
	keysetInfoPtr->ownerHandle = iCryptOwner;
	if( isReadOnly )
		INIT_FLAGS( keysetInfoPtr->flags, KEYSET_FLAG_READONLY );
	else
		INIT_FLAGS( keysetInfoPtr->flags, KEYSET_FLAG_NONE );
	DATAPTR_SET( keysetInfoPtr->keyData, NULL );
	switch( keysetType )
		{
		case CRYPT_KEYSET_FILE:
			keysetInfoPtr->type = KEYSET_FILE;
			keysetInfoPtr->keysetFile = ( FILE_INFO * ) keysetInfoPtr->storage;
			break;

#ifdef USE_HTTP
		case CRYPT_KEYSET_HTTP:
			keysetInfoPtr->type = KEYSET_HTTP;
			keysetInfoPtr->keysetHTTP = ( HTTP_INFO * ) keysetInfoPtr->storage;
			break;
#endif /* USE_HTTP */

#ifdef USE_LDAP
		case CRYPT_KEYSET_LDAP:
			keysetInfoPtr->type = KEYSET_LDAP;
			keysetInfoPtr->keysetLDAP = ( LDAP_INFO * ) keysetInfoPtr->storage;
			break;
#endif /* USE_LDAP */

#ifdef USE_DBMS
		default:
			keysetInfoPtr->type = KEYSET_DBMS;
			keysetInfoPtr->keysetDBMS = ( DBMS_INFO * ) keysetInfoPtr->storage;
			break;
#endif /* USE_DBMS */
		}
	keysetInfoPtr->storageSize = storageSize;

	/* If it's a flat-file keyset which is implemented on top of an I/O 
	   stream, handle it specially */
	if( keysetType == CRYPT_KEYSET_FILE )
		{
		status = completeKeysetFileOpen( keysetInfoPtr, keysetSubType, 
										 &stream, name, nameLength, 
										 options );
		if( cryptStatusError( status ) )
			{
			sFileClose( &keysetInfoPtr->keysetFile->stream );
			if( options == CRYPT_KEYOPT_CREATE )
				{
				/* It's a newly-created file, make sure that we don't leave 
				   it lying around on disk */
				fileErase( keysetInfoPtr->keysetFile->fileName );
				}
			return( status );
			}

		ENSURES( sanityCheckKeyset( keysetInfoPtr ) );
		
		return( CRYPT_OK );
		}

	/* Wait for any async keyset driver binding to complete.  We do this as 
	   late as possible to prevent file keyset reads that occur on startup 
	   (for example to get configuration options) from stalling the startup 
	   process */
	if( !krnlWaitSemaphore( SEMAPHORE_DRIVERBIND ) )
		{
		/* The kernel is shutting down, bail out */
		DEBUG_DIAG(( "Exiting due to kernel shutdown" ));
		assert( DEBUG_WARN );
		return( CRYPT_ERROR_PERMISSION );
		}

	/* It's a specific type of keyset, set up the access information for it
	   and connect to it */
	switch( keysetType )
		{
		case CRYPT_KEYSET_ODBC:
		case CRYPT_KEYSET_DATABASE:
		case CRYPT_KEYSET_ODBC_STORE:
		case CRYPT_KEYSET_DATABASE_STORE:
			status = setAccessMethodDBMS( keysetInfoPtr, keysetType );
			break;

		case CRYPT_KEYSET_HTTP:
			status = setAccessMethodHTTP( keysetInfoPtr );
			break;

		case CRYPT_KEYSET_LDAP:
			status = setAccessMethodLDAP( keysetInfoPtr );
			break;

		default:
			retIntError();
		}
	if( cryptStatusError( status ) )
		return( status );
	ENSURES( checkKeysetFunctions( keysetInfoPtr ) );

	/* Initialise keyset access */
	initFunction = ( KEY_INIT_FUNCTION ) \
				   FNPTR_GET( keysetInfoPtr->initFunction );
	REQUIRES( initFunction != NULL );
	status = initFunction( keysetInfoPtr, name, nameLength, 
						   ( options == CRYPT_KEYOPT_CREATE ) ? \
							 CRYPT_KEYOPT_CREATE : CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		return( status );
	SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_OPEN );
	if( options == CRYPT_KEYOPT_CREATE )
		SET_FLAG( keysetInfoPtr->flags, KEYSET_FLAG_EMPTY );

	ENSURES( sanityCheckKeyset( keysetInfoPtr ) );

	return( CRYPT_OK );
	}

/* Create a keyset object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int createKeyset( INOUT MESSAGE_CREATEOBJECT_INFO *createInfo,
				  STDC_UNUSED const void *auxDataPtr, 
				  STDC_UNUSED const int auxValue )
	{
	CRYPT_KEYSET iCryptKeyset;
	KEYSET_INFO *keysetInfoPtr = NULL;
	int initStatus, status;

	assert( isWritePtr( createInfo, sizeof( MESSAGE_CREATEOBJECT_INFO ) ) );

	REQUIRES( auxDataPtr == NULL && auxValue == 0 );
	REQUIRES( isEnumRange( createInfo->arg1, CRYPT_KEYSET ) );
	REQUIRES( isEnumRangeOpt( createInfo->arg2, CRYPT_KEYOPT ) );
	REQUIRES( createInfo->strArgLen1 >= MIN_NAME_LENGTH && \
			  createInfo->strArgLen1 < MAX_ATTRIBUTE_SIZE );

	/* Pass the call on to the lower-level open function */
	initStatus = openKeyset( &iCryptKeyset, createInfo->cryptOwner,
							 createInfo->arg1, createInfo->strArg1, 
							 createInfo->strArgLen1, createInfo->arg2,
							 &keysetInfoPtr );
	if( cryptStatusError( initStatus ) )
		{
		/* If the create object failed, return immediately */
		if( keysetInfoPtr == NULL )
			return( initStatus );

		/* Since no object has been created there's nothing to get a 
		   detailed error string from, but we can at least send the 
		   information to the debug output */
		DEBUG_PRINT_COND( initStatus != CRYPT_ERROR_NOTAVAIL,
						  ( "Keyset open error: %s.\n",
							( keysetInfoPtr->errorInfo.errorStringLength > 0 ) ? \
							  keysetInfoPtr->errorInfo.errorString : \
							  "<<<No information available>>>" ));

		/* The init failed, make sure that the object gets destroyed when we 
		   notify the kernel that the setup process is complete */
		krnlSendNotifier( iCryptKeyset, IMESSAGE_DESTROY );
		}

	/* We've finished setting up the object-type-specific info, tell the
	   kernel that the object is ready for use */
	status = krnlSendMessage( iCryptKeyset, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_OK, CRYPT_IATTRIBUTE_STATUS );
	if( cryptStatusError( initStatus ) || cryptStatusError( status ) )
		return( cryptStatusError( initStatus ) ? initStatus : status );
	createInfo->cryptHandle = iCryptKeyset;
	return( CRYPT_OK );
	}

/* Generic management function for this class of object */

CHECK_RETVAL \
int keysetManagementFunction( IN_ENUM( MANAGEMENT_ACTION ) \
								const MANAGEMENT_ACTION_TYPE action )
	{
	static int initLevel = 0;
	int status;

	REQUIRES( action == MANAGEMENT_ACTION_INIT || \
			  action == MANAGEMENT_ACTION_SHUTDOWN );

	switch( action )
		{
		case MANAGEMENT_ACTION_INIT:
#ifdef CONFIG_FUZZ
			return( CRYPT_OK );
#endif /* CONFIG_FUZZ */
			status = dbxInitODBC();
			if( cryptStatusOK( status ) )
				{
				initLevel++;
				if( krnlIsExiting() )
					{
					/* The kernel is shutting down, exit */
					return( CRYPT_ERROR_PERMISSION );
					}
				status = dbxInitLDAP();
				}
			if( cryptStatusOK( status ) )
				initLevel++;
			return( status );

		case MANAGEMENT_ACTION_SHUTDOWN:
			if( initLevel > 1 )
				{
				dbxEndLDAP();
				}
			if( initLevel > 0 )
				{
				dbxEndODBC();
				}
			initLevel = 0;
			return( CRYPT_OK );
		}

	retIntError();
	}
#endif /* USE_KEYSETS */
