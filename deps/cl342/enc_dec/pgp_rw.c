/****************************************************************************
*																			*
*				Miscellaneous (Non-ASN.1) Read/Write Routines				*
*						Copyright Peter Gutmann 1992-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "pgp_rw.h"
#else
  #include "crypt.h"
  #include "enc_dec/pgp_rw.h"
#endif /* Compiler-specific includes */

#ifdef USE_PGP

/****************************************************************************
*																			*
*							PGP Read/Write Routines							*
*																			*
****************************************************************************/

/* Read a length in OpenPGP or PGP2 format */

CHECK_RETVAL_LENGTH STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readOpenPGPLength( INOUT STREAM *stream, 
							  OUT_LENGTH_Z long *length,
							  OUT_BOOL BOOLEAN *indefiniteLength,
							  const BOOLEAN indefOK )
	{
	long localLength;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );
	assert( isWritePtr( indefiniteLength, sizeof( BOOLEAN ) ) );

	/* Clear return values */
	*length = 0;
	*indefiniteLength = FALSE;

	/* Get the initial length byte to allow us to decode what else is there.  
	   The error status is also checked later but we make it explicit here */
	localLength = sgetc( stream );
	if( cryptStatusError( localLength ) )
		return( localLength );

	/* 0...191 is a literal value */
	if( localLength <= 191 )
		{
		*length = localLength;
		return( CRYPT_OK );
		}

	/* 192...223 is a 13-bit value with offset 192, giving a length in the 
	   range 192...8383 */
	if( localLength <= 223 )
		{
		const int value = sgetc( stream );
		if( cryptStatusError( value ) )
			return( value );
		localLength = ( ( localLength - 192 ) << 8 ) + value + 192;
		if( localLength < 192 || localLength > 8383 )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		*length = localLength;

		return( CRYPT_OK );
		}

	/* 224...254 is PGP's annoying interpretation of indefinite-length 
	   encoding.  This is an incredible pain to handle but fortunately 
	   except for McAfee's PGP implementation and GPG under some 
	   circumstances when it's used in a pipeline it doesn't seem to be used 
	   by anything.  The only data type that would normally need indefinite 
	   lengths, compressed data, uses the 2.x CTB 0xA3 instead */
	if( localLength < 255 )
		{
		if( !indefOK )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

		/* Unlike ASN.1, which encodes an outer indefinite-length marker and 
		   then encodes each sub-segment as a data unit within it, PGP 
		   encodes a partial length as a sequence of power-of-two exponent 
		   values with a standard length encoding for the last sub-segment.
		   So once we're in indefinite-length mode we have to record the 
		   current *type* of the length (as well as its value) to determine 
		   whether more length packets follow */
		*indefiniteLength = TRUE;
		localLength = 1 << ( localLength & 0x1F );
		if( localLength < 1 || localLength >= MAX_INTLENGTH )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		*length = localLength;

		return( CRYPT_OK );
		}
	ENSURES( localLength == 255 );

	/* 255 is a marker that a standard 32-bit length follows */
	localLength = readUint32( stream );
	if( cryptStatusError( localLength ) )
		return( localLength );
	*length = localLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPGP2Length( INOUT STREAM *stream, 
						   OUT_LENGTH_Z long *length, 
						   IN_BYTE const int ctb )
	{
	long localLength;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES( ctb >= 0 && ctb <= 0xFF );

	/* Clear return value */
	*length = 0;

	/* It's a PGP 2.x CTB, decode the length as a byte, word, or long */
	switch( ctb & 3 )
		{
		case 0:
			localLength = sgetc( stream );
			break;

		case 1:
			localLength = readUint16( stream );
			break;

		case 2:
			localLength = readUint32( stream );
			break;

		default:
			/* A length value of 3 indicates that the data length is 
			   determined externally, this is a deprecated PGP 2.x value 
			   that we don't handle */
			localLength = CRYPT_ERROR_BADDATA;
		}
	if( cryptStatusError( localLength ) )
		return( localLength );
	if( localLength < 0 || localLength >= MAX_INTLENGTH )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	*length = localLength;

	return( CRYPT_OK );
	}

/* Read PGP variable-length length values and packet headers (CTB + length).  
   We also have a short-length version which is used to read small packets 
   such as keyrings and sigs and which ensures that the length is in the 
   range 1...16K */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int pgpReadLength( INOUT STREAM *stream, 
						  OUT_LENGTH_BOUNDED_Z( maxLength ) long *length, 
						  IN_BYTE const int ctb, 
						  IN_LENGTH_SHORT_Z const int minLength, 
						  IN_LENGTH const int maxLength, 
						  const BOOLEAN indefOK )
	{
	BOOLEAN indefiniteLength = FALSE;
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );

	REQUIRES_S( minLength >= 0 && minLength < MAX_INTLENGTH_SHORT && \
				minLength < maxLength && maxLength < MAX_INTLENGTH );

	/* Clear return value */
	*length = 0;

	/* If it doesn't look like PGP data, don't go any further */
	if( !pgpIsCTB( ctb ) )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );

	/* If it's an OpenPGP CTB, undo the hand-Huffman-coding */
	if( pgpGetPacketVersion( ctb ) == PGP_VERSION_OPENPGP )
		{
		status = readOpenPGPLength( stream, &localLength, 
									&indefiniteLength, indefOK );
		}
	else
		status = readPGP2Length( stream, &localLength, ctb );
	if( cryptStatusError( status ) )
		return( status );
	if( localLength < minLength || localLength > maxLength )
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
	*length = localLength;
	return( indefiniteLength ? OK_SPECIAL : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readPacketHeader( INOUT STREAM *stream, 
							 OUT_OPT_BYTE int *ctb, 
							 OUT_OPT_LENGTH_Z long *length, 
							 IN_LENGTH_SHORT_Z const int minLength, 
							 IN_LENGTH const int maxLength, 
							 const BOOLEAN indefOK )
	{
	long localLength;
	int localCTB, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ctb == NULL || isWritePtr( ctb, sizeof( int ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );
	
	REQUIRES_S( minLength >= 0 && minLength < MAX_INTLENGTH_SHORT && \
				minLength < maxLength && maxLength < MAX_INTLENGTH );

	/* Clear return values */
	if( ctb != NULL )
		*ctb = 0;
	if( length != NULL )
		*length = 0;

	/* Examine the CTB and figure out whether we need to perform any 
	   special-case handling */
	localCTB = sgetc( stream );
	if( cryptStatusError( localCTB ) )
		return( localCTB );
	if( !pgpIsCTB( localCTB ) )
		{
		/* If it doesn't look like PGP data, don't go any further */
		return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	if( localCTB == PGP_CTB_COMPRESSED )
		{
		/* If it's a compressed data packet, there's no length present.
		   Normally we reject any indefinite-length packets since these
		   can't be processed sensibly (PGP 2.x, which used intermediate
		   files for everything, just read to EOF, OpenPGP deprecates them
		   because this doesn't exactly lead to portable implementations).
		   However, compressed-data packets can only be stored in this
		   manner but can still be processed because the user has to
		   explicitly flush the data at some point and we assume that this
		   is EOF.
		   
		   For this reason we don't return OK_SPECIAL to indicate an 
		   indefinite-length encoding because this isn't a standard 
		   segmented encoding but a virtual definite-length that ends when
		   the user says it ends.   This is far uglier than the PKCS #7/CMS/
		   SMIME equivalent where we've got an explicit end-of-data
		   indication, but it's the best that we can do.
		   
		   In addition it's not clear what we should return as the "length"
		   value for this non-length, the contract with the caller says that
		   we'll only permit a returned value within the range 
		   { minLength ... maxLength } but there isn't any length present.  
		   To deal with this we return a fake length equal to minLength, 
		   which means that we stick to the contract, and which will be 
		   ignored by any caller that can process compressed data since, by 
		   definition, the length value is meaningless */
		if( ctb != NULL )
			*ctb = localCTB;
		if( length != NULL )
			*length = minLength;	/* See comment above */
		return( CRYPT_OK );	/* Not-really-indef. return status */
		}

	/* Now that we know the format, get the length information */
	status = pgpReadLength( stream, &localLength, localCTB,
							minLength, maxLength, indefOK );
	if( cryptStatusError( status ) )
		{
		int type;

		if( status != OK_SPECIAL )
			return( status );

		/* It's an indefinite-length encoding, this is only valid for
		   payload data packets so we make sure that we've got one of these
		   packet types present */
		ENSURES( indefOK );
		type = pgpGetPacketType( localCTB );
		if( type != PGP_PACKET_DATA && type != PGP_PACKET_COPR && \
			type != PGP_PACKET_ENCR && type != PGP_PACKET_ENCR_MDC )
			return( sSetError( stream, CRYPT_ERROR_BADDATA ) );
		}
	if( ctb != NULL )
		*ctb = localCTB;
	if( length != NULL )
		*length = localLength;

	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pgpReadShortLength( INOUT STREAM *stream, 
						OUT_LENGTH_SHORT_Z int *length, 
						IN_BYTE const int ctb )
	{
	long localLength;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );

	/* Clear return value */
	*length = 0;

	status = pgpReadLength( stream, &localLength, ctb, 0, 
							MAX_INTLENGTH_SHORT, FALSE );
	if( cryptStatusError( status ) )
		return( status );
	*length = ( int ) localLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeader( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						 OUT_OPT_LENGTH_Z long *length, 
						 IN_LENGTH_SHORT const int minLength,
						 IN_LENGTH const long maxLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ctb == NULL || isWritePtr( ctb, sizeof( int ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );
	
	REQUIRES_S( minLength >= 0 && minLength < MAX_INTLENGTH_SHORT );
	REQUIRES_S( maxLength > minLength && maxLength < MAX_INTLENGTH );

	return( readPacketHeader( stream, ctb, length, minLength, maxLength, 
							  FALSE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeaderI( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						  OUT_OPT_LENGTH_Z long *length, 
						  IN_LENGTH_SHORT const int minLength )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( ctb == NULL || isWritePtr( ctb, sizeof( int ) ) );
	assert( length == NULL || isWritePtr( length, sizeof( long ) ) );
	
	REQUIRES_S( minLength >= 0 && minLength < MAX_INTLENGTH_SHORT );

	return( readPacketHeader( stream, ctb, length, minLength, 
							  MAX_INTLENGTH - 1, TRUE ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pgpReadPartialLength( INOUT STREAM *stream, 
						  OUT_LENGTH_Z long *length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( long ) ) );
	
	/* This is a raw length value so we have to feed in a pseudo-CTB */
	return( pgpReadLength( stream, length, PGP_CTB_OPENPGP,
						   0, MAX_INTLENGTH - 1, TRUE ) );
	}

/* Write PGP variable-length length values and packet headers (CTB + 
   length) */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWriteLength( INOUT STREAM *stream, 
					IN_LENGTH const long length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( length > 0 && length < MAX_INTLENGTH );

	if( length <= 191 )
		return( sputc( stream, length ) );
	if( length <= 8383 )
		{
		const long adjustedLength = length - 192;

		sputc( stream, ( ( adjustedLength >> 8 ) & 0xFF ) + 192 );
		return( sputc( stream, ( adjustedLength & 0xFF ) ) );
		}
	sputc( stream, 0xFF );
	sputc( stream, ( length >> 24 ) & 0xFF );
	sputc( stream, ( length >> 16 ) & 0xFF );
	sputc( stream, ( length >> 8 ) & 0xFF );
	return( sputc( stream, ( length & 0xFF ) ) );
	}

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWritePacketHeader( INOUT STREAM *stream, 
						  IN_ENUM( PGP_PACKET ) const PGP_PACKET_TYPE packetType,
						  IN_LENGTH const long length )
	{
	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	
	REQUIRES_S( packetType > PGP_PACKET_NONE && \
				packetType < PGP_PACKET_LAST );
	REQUIRES_S( length > 0 && length < MAX_INTLENGTH );

	sputc( stream, PGP_CTB_OPENPGP | packetType );
	return( pgpWriteLength( stream, length ) );
	}
#endif /* USE_PGP */
