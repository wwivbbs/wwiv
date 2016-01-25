/****************************************************************************
*																			*
*						Encoded Object Query Routines						*
*					  Copyright Peter Gutmann 1992-2008						*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "pgp_rw.h"
  #include "mech.h"
#else
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "enc_dec/pgp_rw.h"
  #include "mechs/mech.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get information on an ASN.1 object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int getObjectInfo( INOUT STREAM *stream, 
						  OUT QUERY_INFO *queryInfo )
	{
	const long startPos = stell( stream );
	long value;
	int tag, length, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* We always need at least MIN_CRYPT_OBJECTSIZE more bytes to do
	   anything */
	if( sMemDataLeft( stream ) < MIN_CRYPT_OBJECTSIZE )
		return( CRYPT_ERROR_UNDERFLOW );

	/* Get the type, length, and version information */
	status = getStreamObjectLength( stream, &length );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->formatType = CRYPT_FORMAT_CRYPTLIB;
	queryInfo->size = length;
	tag = peekTag( stream );
	if( cryptStatusError( tag ) )
		return( tag );
	readGenericHole( stream, NULL, 16, tag );
	status = readShortInteger( stream, &value );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->version = value;
	switch( tag )
		{
		case BER_SEQUENCE:
			/* This could be a signature or a PKC-encrypted key, see what
			   follows */
			switch( value )
				{
				case KEYTRANS_VERSION:
				case KEYTRANS_EX_VERSION:
					queryInfo->type = CRYPT_OBJECT_PKCENCRYPTED_KEY;
					break;

				case SIGNATURE_VERSION:
				case SIGNATURE_EX_VERSION:
					queryInfo->type = CRYPT_OBJECT_SIGNATURE;
					break;

				default:
					return( CRYPT_ERROR_BADDATA );
				}
			if( value == KEYTRANS_VERSION || value == SIGNATURE_VERSION )
				queryInfo->formatType = CRYPT_FORMAT_CMS;
			break;

		case MAKE_CTAG( CTAG_RI_KEYAGREE ):
			/* It's CMS' wierd X9.42-inspired key agreement mechanism, we
			   can't do much with this (mind you neither can anyone else)
			   so we should probably really treat it as a 
			   CRYPT_ERROR_BADDATA if we encounter it rather than just 
			   ignoring it */
			queryInfo->type = CRYPT_OBJECT_NONE;
			DEBUG_DIAG(( "Found keyAgreeRecipientInfo" ));
			assert( DEBUG_WARN );
			break;

		case MAKE_CTAG( CTAG_RI_PWRI ):
			queryInfo->type = CRYPT_OBJECT_ENCRYPTED_KEY;
			break;

		default:
			queryInfo->type = CRYPT_OBJECT_NONE;
			if( tag > MAKE_CTAG( CTAG_RI_PWRI ) && \
				tag <= MAKE_CTAG( CTAG_RI_MAX ) )
				{
				/* This is probably a new RecipientInfo type, skip it */
				DEBUG_DIAG(( "Found unknown RecipientInfo %X", tag ));
				assert( DEBUG_WARN );
				break;
				}
			return( CRYPT_ERROR_BADDATA );
		}

	/* Reset the stream and make sure that all of the data is present */
	sseek( stream, startPos );
	return( sMemDataLeft( stream ) < queryInfo->size ? \
			CRYPT_ERROR_UNDERFLOW : CRYPT_OK );
	}

#ifdef USE_PGP

/* Get information on a PGP data object.  This doesn't reset the stream like
   the ASN.1 equivalent because the PGP header is complex enough that it
   can't be read inline like the ASN.1 header */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getPgpPacketInfo( INOUT STREAM *stream, 
					  OUT QUERY_INFO *queryInfo )
	{
	const long startPos = stell( stream );
	long offset, length;
	int ctb, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Read the packet header and extract information from the CTB.  Note
	   that the assignment of version numbers is speculative only because
	   it's possible to use PGP 2.x packet headers to wrap up OpenPGP
	   packets */
	status = pgpReadPacketHeader( stream, &ctb, &length, 8 );
	if( cryptStatusError( status ) )
		return( status );
	queryInfo->formatType = CRYPT_FORMAT_PGP;
	queryInfo->version = pgpGetPacketVersion( ctb );
	offset = stell( stream );
	if( cryptStatusError( offset ) )
		return( offset );
	queryInfo->size = ( offset - startPos ) + length;
	switch( pgpGetPacketType( ctb ) )
		{
		case PGP_PACKET_SKE:
			queryInfo->type = CRYPT_OBJECT_ENCRYPTED_KEY;
			break;

		case PGP_PACKET_PKE:
			queryInfo->type = CRYPT_OBJECT_PKCENCRYPTED_KEY;
			break;

		case PGP_PACKET_SIGNATURE:
			queryInfo->type = CRYPT_OBJECT_SIGNATURE;
			break;

		case PGP_PACKET_SIGNATURE_ONEPASS:
			/* First half of a one-pass signature, this is given a special
			   type of 'none' since it's not a normal packet */
			queryInfo->type = CRYPT_OBJECT_NONE;
			break;

		default:
			DEBUG_DIAG(( "Found unrecognised ctb %X", ctb ));
			assert( DEBUG_WARN );
			return( CRYPT_ERROR_BADDATA );
		}

	/* Make sure that all of the data is present without resetting the 
	   stream */
	return( ( sMemDataLeft( stream ) < length ) ? \
			CRYPT_ERROR_UNDERFLOW : CRYPT_OK );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*								Object Query Routines						*
*																			*
****************************************************************************/

/* Low-level object query functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int queryAsn1Object( INOUT void *streamPtr, OUT QUERY_INFO *queryInfo )
	{
	QUERY_INFO basicQueryInfo;
	STREAM *stream = streamPtr;
	const long startPos = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Determine basic object information.  This also verifies that all of 
	   the object data is present in the stream */
	status = getObjectInfo( stream, &basicQueryInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* Call the appropriate routine to find out more about the object */
	switch( basicQueryInfo.type )
		{
		case CRYPT_OBJECT_ENCRYPTED_KEY:
			{
			const READKEK_FUNCTION readKekFunction = \
									getReadKekFunction( KEYEX_CMS );

			if( readKekFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readKekFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_PKCENCRYPTED_KEY:
			{
			const READKEYTRANS_FUNCTION readKeytransFunction = \
				getReadKeytransFunction( ( basicQueryInfo.formatType == CRYPT_FORMAT_CMS ) ? \
										 KEYEX_CMS : KEYEX_CRYPTLIB );

			if( readKeytransFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readKeytransFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_SIGNATURE:
			{
			const READSIG_FUNCTION readSigFunction = \
				getReadSigFunction( ( basicQueryInfo.formatType == CRYPT_FORMAT_CMS ) ? \
									SIGNATURE_CMS : SIGNATURE_CRYPTLIB );

			if( readSigFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readSigFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_NONE:
			/* New, unrecognised RecipientInfo type */
			status = readUniversal( stream );
			break;

		default:
			retIntError();
		}
	sseek( stream, startPos );
	if( cryptStatusError( status ) )
		{
		zeroise( queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Augment the per-object query information with the basic query 
	   information that we got earlier */
	queryInfo->formatType = basicQueryInfo.formatType;
	queryInfo->type = basicQueryInfo.type;
	queryInfo->size = basicQueryInfo.size;
	queryInfo->version = basicQueryInfo.version;

	return( CRYPT_OK );
	}

#ifdef USE_PGP

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int queryPgpObject( INOUT void *streamPtr, OUT QUERY_INFO *queryInfo )
	{
	QUERY_INFO basicQueryInfo;
	STREAM *stream = streamPtr;
	const long startPos = stell( stream );
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( queryInfo, sizeof( QUERY_INFO ) ) );

	/* Clear return value */
	memset( queryInfo, 0, sizeof( QUERY_INFO ) );

	/* Determine basic object information.  This also verifies that all of 
	   the object data is present in the stream */
	status = getPgpPacketInfo( stream, &basicQueryInfo );
	sseek( stream, startPos );
	if( cryptStatusError( status ) )
		return( status );

	/* Call the appropriate routine to find out more about the object */
	switch( basicQueryInfo.type )
		{
		case CRYPT_OBJECT_ENCRYPTED_KEY:
			{
			const READKEK_FUNCTION readKekFunction = \
									getReadKekFunction( KEYEX_PGP );

			if( readKekFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readKekFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_PKCENCRYPTED_KEY:
			{
			const READKEYTRANS_FUNCTION readKeytransFunction = \
									getReadKeytransFunction( KEYEX_PGP );

			if( readKeytransFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readKeytransFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_SIGNATURE:
			{
			const READSIG_FUNCTION readSigFunction = \
									getReadSigFunction( SIGNATURE_PGP );

			if( readSigFunction == NULL )
				return( CRYPT_ERROR_NOTAVAIL );
			status = readSigFunction( stream, queryInfo );
			break;
			}

		case CRYPT_OBJECT_NONE:
			/* First half of a one-pass signature */
			status = readPgpOnepassSigPacket( stream, queryInfo );
			break;

		default:
			retIntError();
		}
	sseek( stream, startPos );
	if( cryptStatusError( status ) )
		{
		zeroise( queryInfo, sizeof( QUERY_INFO ) );
		return( status );
		}

	/* Augment the per-object query information with the basic query 
	   information that we got earlier */
	queryInfo->formatType = basicQueryInfo.formatType;
	if( queryInfo->type == CRYPT_OBJECT_NONE )
		{
		/* The non-type CRYPT_OBJECT_NONE denotes the first half of a one-
		   pass signature packet, in which case the actual type is given in
		   the packet data */
		queryInfo->type = basicQueryInfo.type;
		}
	queryInfo->size = basicQueryInfo.size;
	if( queryInfo->version == 0 )
		{
		/* PGP has multiple packet version numbers sprayed all over the
		   place, and just because an outer version is X doesn't mean that
		   a subsequent inner version can't be Y.  The information is really
		   only used to control the formatting of what gets read, so we
		   just report the first version that we encounter */
		queryInfo->version = basicQueryInfo.version;
		}

	return( CRYPT_OK );
	}
#endif /* USE_PGP */

/****************************************************************************
*																			*
*						External Object Query Interface						*
*																			*
****************************************************************************/

/* Query an object.  This is just a wrapper that provides an external
   interface for the lower-level object-query routines */

C_RET cryptQueryObject( C_IN void C_PTR objectData,
						C_IN int objectDataLength,
						C_OUT CRYPT_OBJECT_INFO C_PTR cryptObjectInfo )
	{
	QUERY_INFO queryInfo = DUMMY_INIT_STRUCT;	/* If USE_PGP undef'd */
	STREAM stream;
	int value, length = objectDataLength, status;

	/* Perform basic error checking and clear the return value */
	if( objectDataLength <= MIN_CRYPT_OBJECTSIZE || \
		objectDataLength >= MAX_INTLENGTH )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtr( objectData, objectDataLength ) )
		return( CRYPT_ERROR_PARAM1 );
	if( !isWritePtrConst( cryptObjectInfo, sizeof( CRYPT_OBJECT_INFO ) ) )
		return( CRYPT_ERROR_PARAM3 );
	memset( cryptObjectInfo, 0, sizeof( CRYPT_OBJECT_INFO ) );

	/* Query the object.  This is just a wrapper for the lower-level object-
	   query functions.  Note that we use sPeek() rather than peekTag() 
	   because we want to continue processing (or at least checking for) PGP 
	   data if it's no ASN.1 */
	sMemConnect( &stream, ( void * ) objectData, length );
	status = value = sPeek( &stream );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	if( value == BER_SEQUENCE || value == MAKE_CTAG( CTAG_RI_PWRI ) )
		status = queryAsn1Object( &stream, &queryInfo );
	else
		{
#ifdef USE_PGP
		status = queryPgpObject( &stream, &queryInfo );
#else
		status = CRYPT_ERROR_BADDATA;
#endif /* USE_PGP */
		}
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Copy the externally-visible fields across */
	cryptObjectInfo->objectType = queryInfo.type;
	cryptObjectInfo->cryptAlgo = queryInfo.cryptAlgo;
	cryptObjectInfo->cryptMode = queryInfo.cryptMode;
	if( queryInfo.type == CRYPT_OBJECT_SIGNATURE )
		cryptObjectInfo->hashAlgo = queryInfo.hashAlgo;
	if( queryInfo.type == CRYPT_OBJECT_ENCRYPTED_KEY && \
		queryInfo.saltLength > 0 )
		{
		memcpy( cryptObjectInfo->salt, queryInfo.salt, queryInfo.saltLength );
		cryptObjectInfo->saltSize = queryInfo.saltLength;
		if( queryInfo.keySetupAlgo != CRYPT_ALGO_NONE )
			cryptObjectInfo->hashAlgo = queryInfo.keySetupAlgo;
		}

	return( CRYPT_OK );
	}
