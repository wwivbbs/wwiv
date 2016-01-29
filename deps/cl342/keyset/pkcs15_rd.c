/****************************************************************************
*																			*
*						cryptlib PKCS #15 Read Routines						*
*						Copyright Peter Gutmann 1996-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "keyset.h"
  #include "pkcs15.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "keyset/keyset.h"
  #include "keyset/pkcs15.h"
#endif /* Compiler-specific includes */

#ifdef USE_PKCS15

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Copy any new object ID information that we've just read across to the 
   object information */

STDC_NONNULL_ARG( ( 1, 2 ) ) \
static void copyObjectIdInfo( INOUT PKCS15_INFO *pkcs15infoPtr, 
							  const PKCS15_INFO *pkcs15objectInfo )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( pkcs15objectInfo, sizeof( PKCS15_INFO ) ) );

	/* If any new ID information has become available, copy it over.  The 
	   keyID defaults to the iD so we only copy the newly-read keyID over if 
	   it's something other than the existing iD */
	if( pkcs15objectInfo->keyIDlength > 0 && \
		( pkcs15infoPtr->iDlength != pkcs15objectInfo->keyIDlength || \
		  memcmp( pkcs15infoPtr->iD, pkcs15objectInfo->keyID,
				  pkcs15objectInfo->keyIDlength ) ) )
		{
		memcpy( pkcs15infoPtr->keyID, pkcs15objectInfo->keyID,
				pkcs15objectInfo->keyIDlength );
		pkcs15infoPtr->keyIDlength = pkcs15objectInfo->keyIDlength;
		}
	if( pkcs15objectInfo->iAndSIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->iAndSID, pkcs15objectInfo->iAndSID,
				pkcs15objectInfo->iAndSIDlength );
		pkcs15infoPtr->iAndSIDlength = pkcs15objectInfo->iAndSIDlength;
		}
	if( pkcs15objectInfo->subjectNameIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->subjectNameID, pkcs15objectInfo->subjectNameID,
				pkcs15objectInfo->subjectNameIDlength );
		pkcs15infoPtr->subjectNameIDlength = pkcs15objectInfo->subjectNameIDlength;
		}
	if( pkcs15objectInfo->issuerNameIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->issuerNameID, pkcs15objectInfo->issuerNameID,
				pkcs15objectInfo->issuerNameIDlength );
		pkcs15infoPtr->issuerNameIDlength = pkcs15objectInfo->issuerNameIDlength;
		}
	if( pkcs15objectInfo->pgp2KeyIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->pgp2KeyID, pkcs15objectInfo->pgp2KeyID,
				pkcs15objectInfo->pgp2KeyIDlength );
		pkcs15infoPtr->pgp2KeyIDlength = pkcs15objectInfo->pgp2KeyIDlength;
		}
	if( pkcs15objectInfo->openPGPKeyIDlength > 0 )
		{
		memcpy( pkcs15infoPtr->openPGPKeyID, pkcs15objectInfo->openPGPKeyID,
				pkcs15objectInfo->openPGPKeyIDlength );
		pkcs15infoPtr->openPGPKeyIDlength = pkcs15objectInfo->openPGPKeyIDlength;
		}
	}

/* Copy any new object payload information that we've just read across to 
   the object information */

STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int copyObjectPayloadInfo( INOUT PKCS15_INFO *pkcs15infoPtr, 
								  const PKCS15_INFO *pkcs15objectInfo,
								  IN_BUFFER( objectLength ) const void *object, 
								  IN_LENGTH_SHORT const int objectLength,
								  IN_ENUM( PKCS15_OBJECT ) \
									const PKCS15_OBJECT_TYPE type )
	{
	assert( isWritePtr( pkcs15infoPtr, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( pkcs15objectInfo, sizeof( PKCS15_INFO ) ) );
	assert( isReadPtr( object, objectLength ) );

	REQUIRES( objectLength > 0 && objectLength < MAX_INTLENGTH_SHORT );
	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );

	switch( type )
		{
		case PKCS15_OBJECT_PUBKEY:
			pkcs15infoPtr->type = PKCS15_SUBTYPE_NORMAL;
			pkcs15infoPtr->pubKeyData = ( void * ) object;
			pkcs15infoPtr->pubKeyDataSize = objectLength;
			pkcs15infoPtr->pubKeyOffset = pkcs15objectInfo->pubKeyOffset;
			pkcs15infoPtr->pubKeyUsage = pkcs15objectInfo->pubKeyUsage;
			break;

		case PKCS15_OBJECT_PRIVKEY:
			pkcs15infoPtr->type = PKCS15_SUBTYPE_NORMAL;
			pkcs15infoPtr->privKeyData = ( void * ) object;
			pkcs15infoPtr->privKeyDataSize = objectLength;
			pkcs15infoPtr->privKeyOffset = pkcs15objectInfo->privKeyOffset;
			pkcs15infoPtr->privKeyUsage = pkcs15objectInfo->privKeyUsage;
			break;

		case PKCS15_OBJECT_CERT:
			if( pkcs15infoPtr->type == PKCS15_SUBTYPE_NONE )
				pkcs15infoPtr->type = PKCS15_SUBTYPE_CERT;
			pkcs15infoPtr->certData = ( void * ) object;
			pkcs15infoPtr->certDataSize = objectLength;
			pkcs15infoPtr->certOffset = pkcs15objectInfo->certOffset;
			pkcs15infoPtr->trustedUsage = pkcs15objectInfo->trustedUsage;
			pkcs15infoPtr->implicitTrust = pkcs15objectInfo->implicitTrust;
			break;

		case PKCS15_OBJECT_SECRETKEY:
			/* We don't try and return an error for this, it's not something
			   that we can make use of but if it's ever reached it just ends 
			   up as an empty (non-useful) object entry */
			DEBUG_DIAG(( "Found secret-key object" ));
			assert_nofuzz( DEBUG_WARN );
			break;

		case PKCS15_OBJECT_DATA:
			pkcs15infoPtr->type = PKCS15_SUBTYPE_DATA;
			pkcs15infoPtr->dataType = pkcs15objectInfo->dataType;
			pkcs15infoPtr->dataData = ( void * ) object;
			pkcs15infoPtr->dataDataSize = objectLength;
			pkcs15infoPtr->dataOffset = pkcs15objectInfo->dataOffset;
			break;

		case PKCS15_OBJECT_UNRECOGNISED:
			/* This is a placeholder for an unrecognised object subtype such
			   as a certificate object whose subtype is SPKI or WTLS or 
			   X9.68, we record it as such but leave it as an empty (non-
			   useful) object entry */
			DEBUG_DIAG(( "Found unrecognised object subtype" ));
			pkcs15infoPtr->type = PKCS15_SUBTYPE_UNRECOGNISED;
			break;

		default:
			/* We don't try and return an error for this, it's not something
			   that we can make use of but if it's ever reached it just ends 
			   up as an empty (non-useful) object entry */
			DEBUG_DIAG(( "Found unknown object type %d", type ));
			assert( DEBUG_WARN );
			break;
		}

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Read a Keyset								*
*																			*
****************************************************************************/

/* Read a single object in a keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 6 ) ) \
static int readObject( INOUT STREAM *stream, 
					   OUT PKCS15_INFO *pkcs15objectInfo, 
					   OUT_BUFFER_ALLOC_OPT( *objectLengthPtr ) void **objectPtrPtr, 
					   OUT_LENGTH_SHORT_Z int *objectLengthPtr,
					   IN_ENUM( PKCS15_OBJECT ) const PKCS15_OBJECT_TYPE type, 
					   INOUT ERROR_INFO *errorInfo )
	{
	STREAM objectStream;
	void *objectData;
	int objectLength, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15objectInfo, sizeof( PKCS15_INFO ) ) );
	assert( isWritePtr( objectPtrPtr, sizeof( void * ) ) );
	assert( isWritePtr( objectLengthPtr, sizeof( int ) ) );
	
	REQUIRES( type > PKCS15_OBJECT_NONE && type < PKCS15_OBJECT_LAST );
	REQUIRES( errorInfo != NULL );

	/* Clear return values */
	memset( pkcs15objectInfo, 0, sizeof( PKCS15_INFO ) );
	*objectPtrPtr = NULL;
	*objectLengthPtr = 0;

	/* Read the current object's data */
	status = readRawObjectAlloc( stream, &objectData, &objectLength,
								 MIN_OBJECT_SIZE, MAX_INTLENGTH_SHORT - 1 );
	if( cryptStatusError( status ) )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Couldn't read PKCS #15 object data" ) );
		}
	ANALYSER_HINT( objectData != NULL );

	/* Read the object attributes from the in-memory object data */
	sMemConnect( &objectStream, objectData, objectLength );
	status = readObjectAttributes( &objectStream, pkcs15objectInfo, type, 
								   errorInfo );
	sMemDisconnect( &objectStream );
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		{
		clFree( "readObject", objectData );
		return( status );
		}

	/* Remember the encoded object data */
	*objectPtrPtr = objectData;
	*objectLengthPtr = objectLength;

	return( status );
	}

/* Read an entire keyset */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 5 ) ) \
int readPkcs15Keyset( INOUT STREAM *stream, 
					  OUT_ARRAY( maxNoPkcs15objects ) PKCS15_INFO *pkcs15info, 
					  IN_LENGTH_SHORT const int maxNoPkcs15objects, 
					  IN_LENGTH const long endPos,
					  INOUT ERROR_INFO *errorInfo )
	{
	int iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pkcs15info, sizeof( PKCS15_INFO ) * \
									maxNoPkcs15objects ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( maxNoPkcs15objects >= 1 && \
			  maxNoPkcs15objects < MAX_INTLENGTH_SHORT );
	REQUIRES( endPos > 0 && endPos > stell( stream ) && \
			  endPos < MAX_BUFFER_SIZE );

	/* Clear return value */
	memset( pkcs15info, 0, sizeof( PKCS15_INFO ) * maxNoPkcs15objects );

	/* Scan all of the objects in the keyset */
	for( status = CRYPT_OK, iterationCount = 0;
		 cryptStatusOK( status ) && stell( stream ) < endPos && \
			iterationCount < FAILSAFE_ITERATIONS_MED; iterationCount++ )
		{
		static const MAP_TABLE tagToTypeTbl[] = {
			{ CTAG_PO_PRIVKEY, PKCS15_OBJECT_PRIVKEY },
			{ CTAG_PO_PUBKEY, PKCS15_OBJECT_PUBKEY },
			{ CTAG_PO_TRUSTEDPUBKEY, PKCS15_OBJECT_PUBKEY },
			{ CTAG_PO_SECRETKEY, PKCS15_OBJECT_SECRETKEY },
			{ CTAG_PO_CERT, PKCS15_OBJECT_CERT },
			{ CTAG_PO_TRUSTEDCERT, PKCS15_OBJECT_CERT },
			{ CTAG_PO_USEFULCERT, PKCS15_OBJECT_CERT },
			{ CTAG_PO_DATA, PKCS15_OBJECT_DATA },
			{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
			};
		PKCS15_OBJECT_TYPE type = PKCS15_OBJECT_NONE;
		int tag, value DUMMY_INIT, innerEndPos, innerIterationCount;

		/* Map the object tag to a PKCS #15 object type */
		status = tag = peekTag( stream );
		if( cryptStatusError( status ) )
			{
			pkcs15Free( pkcs15info, maxNoPkcs15objects );
			return( status );
			}
		tag = EXTRACT_CTAG( tag );
		if( tag < CTAG_PO_PRIVKEY || tag >= CTAG_PO_LAST )
			status = CRYPT_ERROR_BADDATA;
		else
			{
			status = mapValue( tag, &value, tagToTypeTbl,
							   FAILSAFE_ARRAYSIZE( tagToTypeTbl, MAP_TABLE ) );
			}
		if( cryptStatusError( status ) )
			{
			pkcs15Free( pkcs15info, maxNoPkcs15objects );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid PKCS #15 object type %02X", tag ) );
			}
		type = value;

		/* Read the [n] [0] wrapper to find out what we're dealing with.  
		   Note that we set the upper limit at MAX_BUFFER_SIZE rather than
		   MAX_INTLENGTH_SHORT because some keysets with many large objects 
		   may have a combined group-of-objects length larger than 
		   MAX_INTLENGTH_SHORT */
		readConstructed( stream, NULL, tag );
		status = readConstructed( stream, &innerEndPos, CTAG_OV_DIRECT );
		if( cryptStatusError( status ) )
			{
			pkcs15Free( pkcs15info, maxNoPkcs15objects );
			return( status );
			}
		if( innerEndPos < MIN_OBJECT_SIZE || innerEndPos >= MAX_BUFFER_SIZE )
			{
			pkcs15Free( pkcs15info, maxNoPkcs15objects );
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid PKCS #15 object data size %d", 
					  innerEndPos ) );
			}
		innerEndPos += stell( stream );

		/* Scan all objects of this type */
		for( innerIterationCount = 0;
			 stell( stream ) < innerEndPos && \
				innerIterationCount < FAILSAFE_ITERATIONS_LARGE; 
			 innerIterationCount++ )
			{
			PKCS15_INFO pkcs15objectInfo, *pkcs15infoPtr = NULL;
			void *object;
			int objectLength;

			/* Read the object */
			status = readObject( stream, &pkcs15objectInfo, &object,
								 &objectLength, type, errorInfo );
			if( cryptStatusError( status ) )
				{
				if( status != OK_SPECIAL )
					{
					pkcs15Free( pkcs15info, maxNoPkcs15objects );
					return( status );
					}

				/* We read an object that we don't know what to do with, 
				   change the effective type to a placeholder */
				type = PKCS15_OBJECT_UNRECOGNISED;
				}
			ANALYSER_HINT( object != NULL );

			/* If we read an object with associated ID information, find out 
			   where to add the object data */
			if( pkcs15objectInfo.iDlength > 0 )
				{
				pkcs15infoPtr = findEntry( pkcs15info, maxNoPkcs15objects, 
										   CRYPT_KEYIDEX_ID, 
										   pkcs15objectInfo.iD,
										   pkcs15objectInfo.iDlength,
										   KEYMGMT_FLAG_NONE, FALSE );
				}
			if( pkcs15infoPtr == NULL )
				{
				int index;

				/* This personality isn't present yet, find out where we can 
				   add the object data and copy the fixed object information 
				   over */
				pkcs15infoPtr = findFreeEntry( pkcs15info, 
											   maxNoPkcs15objects, &index );
				if( pkcs15infoPtr == NULL )
					{
					clFree( "readKeyset", object );
					pkcs15Free( pkcs15info, maxNoPkcs15objects );
					retExt( CRYPT_ERROR_OVERFLOW, 
							( CRYPT_ERROR_OVERFLOW, errorInfo, 
							  "No more room in keyset to add further items" ) );
					}
				memcpy( pkcs15infoPtr, &pkcs15objectInfo, 
						sizeof( PKCS15_INFO ) );
				pkcs15infoPtr->index = index;
				}

			/* Copy over any ID information */
			copyObjectIdInfo( pkcs15infoPtr, &pkcs15objectInfo );

			/* Copy over any other new information that may have become
			   available.  The semantics when multiple date ranges are 
			   present (for example one for a key and one for a certificate) 
			   are a bit uncertain, we use the most recent date available on 
			   the assumption that this reflects the newest information */
			if( pkcs15objectInfo.validFrom > pkcs15infoPtr->validFrom )
				pkcs15infoPtr->validFrom = pkcs15objectInfo.validFrom;
			if( pkcs15objectInfo.validTo > pkcs15infoPtr->validTo )
				pkcs15infoPtr->validTo = pkcs15objectInfo.validTo;

			/* Copy the payload over */
			copyObjectPayloadInfo( pkcs15infoPtr, &pkcs15objectInfo,
								   object, objectLength, type );
			}
		ENSURES( innerIterationCount < FAILSAFE_ITERATIONS_LARGE );
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MED );

	return( CRYPT_OK );
	}
#endif /* USE_PKCS15 */
