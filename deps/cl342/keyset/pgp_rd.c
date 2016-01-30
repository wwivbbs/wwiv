/****************************************************************************
*																			*
*						 cryptlib PGP Key Read Routines						*
*						Copyright Peter Gutmann 1992-2014					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "pgp_rw.h"
  #include "keyset.h"
  #include "pgp_key.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "enc_dec/pgp_rw.h"
  #include "keyset/keyset.h"
  #include "keyset/pgp_key.h"
#endif /* Compiler-specific includes */

/* Make sure that the maximum number of PGP userIDs that we record is less
   than the failsafe bound on the loop that reads userIDs */

#if MAX_PGP_USERIDS >= FAILSAFE_ITERATIONS_MED
  #error MAX_PGP_USERIDS must be less than FAILSAFE_ITERATIONS_MED since this is used to bound loops
#endif /* MAX_PGP_USERIDS >= FAILSAFE_ITERATIONS_MED */

#ifdef USE_PGPKEYS

/****************************************************************************
*																			*
*								Utility Routines							*
*																			*
****************************************************************************/

/* Get the size of an encoded MPI and skip the payload data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4 ) ) \
static int getMPIsize( INOUT STREAM *stream, 
					   IN_LENGTH_PKC const int minMpiSize, 
					   IN_LENGTH_PKC const int maxMpiSize,
					   OUT_LENGTH_SHORT_Z int *length )
	{
	const int position = stell( stream );
	int dummy, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( length, sizeof( int ) ) );

	REQUIRES( minMpiSize > 0 && maxMpiSize > 0 && \
			  minMpiSize <= maxMpiSize && maxMpiSize <= CRYPT_MAX_PKCSIZE );
	REQUIRES( !cryptStatusError( position ) );

	/* Clear return value */
	*length = 0;
	
	status = readInteger16Ubits( stream, NULL, &dummy, minMpiSize, 
								 maxMpiSize );
	if( cryptStatusError( status ) )
		return( status );
	*length = stell( stream ) - position;

	return( CRYPT_OK );
	}

/* Determine the minimum allowed packet size for a given packet type.  The 
   minimum-length packet that we can encounter is a single-byte trust 
   packet, then two bytes for a userID, and three bytes for a marker 
   packet.  Other than that all packets must be at least eight bytes in 
   length */

CHECK_RETVAL_RANGE_NOERROR( 0, 8 ) \
static int getMinPacketSize( IN_BYTE const int packetType )
	{
	ENSURES_EXT( ( packetType >= 0 && \
				   packetType <= 0xFF ), 0 );
				 /* This could be any packet type including new values not
				    covered by the PGP_PACKET_TYPE range so we can't be too
					picky about values */

	return( ( packetType == PGP_PACKET_TRUST ) ? 1 : \
			( packetType == PGP_PACKET_USERID ) ? 2 : \
			( packetType == PGP_PACKET_MARKER ) ? 3 : 8 );
	}

/* Scan a sequence of key packets to find the extent of the packet group */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int scanPacketGroup( INOUT STREAM *stream,
							OUT_LENGTH_BOUNDED_Z( totalLength ) \
								int *packetGroupLength,
							IN_LENGTH_SHORT const int totalLength )
	{
	BOOLEAN firstPacket = TRUE;
	int endPos, noPackets, status;

	assert( isReadPtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( packetGroupLength, sizeof( int ) ) );

	REQUIRES( totalLength > 0 && totalLength < MAX_INTLENGTH_SHORT );

	/* Clear return value */
	*packetGroupLength = 0;

	for( endPos = 0, noPackets = 0; 
		 endPos < totalLength && noPackets < FAILSAFE_ITERATIONS_MED; 
		 noPackets++ )
		{
		long length;
		int ctb, type;

		/* Get the next CTB */
		status = ctb = sPeek( stream );
		if( cryptStatusError( status ) )
			return( status );
		if( !( pgpIsCTB( ctb ) ) )
			return( CRYPT_ERROR_BADDATA );

		/* Check the packet type and make sure that it's valid.  This also
		   catches odd things like compressed-data packets, which aren't
		   indefinite-length but also don't really have a length so that we 
		   can't skip them.  These shouldn't be present in keyrings anyway, 
		   but can crop up due to data corruption */
		type = pgpGetPacketType( ctb );
		if( type != PGP_PACKET_SIGNATURE && type != PGP_PACKET_SECKEY && \
			type != PGP_PACKET_PUBKEY && type != PGP_PACKET_SECKEY_SUB && \
			type != PGP_PACKET_MARKER && type != PGP_PACKET_TRUST && \
			type != PGP_PACKET_USERID && type != PGP_PACKET_PUBKEY_SUB && \
			type != PGP_PACKET_USERATTR )
			{
			DEBUG_DIAG(( "Encountered invalid keyring packet type %d", type ));
			assert_nofuzz( DEBUG_WARN );
			return( CRYPT_ERROR_BADDATA );
			}
		if( firstPacket )
			{
			/* Make sure that the packet group starts with the expected 
			   packet type */
			if( type != PGP_PACKET_PUBKEY && type != PGP_PACKET_SECKEY )
				return( CRYPT_ERROR_BADDATA );
			firstPacket = FALSE;
			}
		else
			{
			/* If we've found the start of a new packet group, we're done */
			if( type == PGP_PACKET_PUBKEY || type == PGP_PACKET_SECKEY )
				break;
			}

		/* Skip the current packet in the buffer */
		status = pgpReadPacketHeader( stream, NULL, &length, 
									  getMinPacketSize( type ),
									  MAX_INTLENGTH_SHORT );
		if( cryptStatusOK( status ) )
			status = sSkip( stream, length, MAX_INTLENGTH_SHORT );
		if( cryptStatusError( status ) )
			return( status );
		endPos = stell( stream );
		}
	if( noPackets >= FAILSAFE_ITERATIONS_MED )
		{
		/* If we've found this many packets in a row all supposedly 
		   belonging to the same key then there's something wrong */
		DEBUG_DIAG(( "Encountered more than %d packets for a single key", 
					 noPackets ));
		assert_nofuzz( DEBUG_WARN );
		return( CRYPT_ERROR_BADDATA );
		}

	/* Remember where the current packet group ends */
	*packetGroupLength = endPos;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*							Process Key Packet Components					*
*																			*
****************************************************************************/

/* Read the information needed to decrypt a private key */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int readPrivateKeyDecryptionInfo( INOUT STREAM *stream, 
										 INOUT PGP_KEYINFO *keyInfo )
	{
	CRYPT_QUERY_INFO queryInfo;
	const int ctb = sgetc( stream );
	int ivSize, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyInfo, sizeof( PGP_KEYINFO ) ) );

	/* Clear return values */
	keyInfo->cryptAlgo = keyInfo->hashAlgo = CRYPT_ALGO_NONE;
	keyInfo->saltSize = keyInfo->keySetupIterations = 0;

	/* Before we go any further make sure that we were at least able to read 
	   the CTB */
	if( cryptStatusError( ctb ) )
		return( ctb );

	/* If no encryption is being used we mark the key as unusable.  This 
	   isn't exactly the correct thing to do, but storing plaintext private 
	   keys on disk is extremely dangerous and we probably shouldn't be
	   using them, and in any case an attempt to import an unencrypted key 
	   will trigger so many security check failures in the key unwrap code 
	   that it's not even worth trying */
	if( ctb == 0 )
		return( CRYPT_ERROR_NOSECURE );

#ifdef USE_PGP2
	/* If it's a direct algorithm specifier then it's a PGP 2.x packet with 
	   raw IDEA encryption */
	if( ctb == PGP_ALGO_IDEA )
		{
		keyInfo->cryptAlgo = CRYPT_ALGO_IDEA;
		keyInfo->hashAlgo = CRYPT_ALGO_MD5;
		status = sread( stream, keyInfo->iv, 8 );
		if( cryptStatusError( status ) )
			return( status );
		keyInfo->ivSize = 8;

		return( CRYPT_OK );
		}
#endif /* USE_PGP2 */

	/* Must be an S2K specifier */
	if( ctb != PGP_S2K && ctb != PGP_S2K_HASHED )
		return( CRYPT_ERROR_BADDATA );

	/* Get the key wrap algorithm and S2K information */
	status = readPgpAlgo( stream, &keyInfo->cryptAlgo, 
						  &keyInfo->cryptAlgoParam, PGP_ALGOCLASS_PWCRYPT );
	if( cryptStatusError( status ) )
		{
		/* If it's an unknown algorithm type, skip this packet */
		if( status == CRYPT_ERROR_NOTAVAIL )
			return( OK_SPECIAL );

		return( status );
		}
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_QUERYCAPABILITY, &queryInfo,
							  keyInfo->cryptAlgo );
	if( cryptStatusError( status ) )
		return( status );
	ivSize = queryInfo.blockSize;
	status = readPgpS2K( stream, &keyInfo->hashAlgo, &keyInfo->hashAlgoParam, 
						 keyInfo->salt, PGP_SALTSIZE, &keyInfo->saltSize, 
						 &keyInfo->keySetupIterations );
	if( cryptStatusError( status ) )
		{
		/* If it's an unknown algorithm type, skip this packet */
		if( status == CRYPT_ERROR_NOTAVAIL )
			return( OK_SPECIAL );

		return( status );
		}
	if( ctb == PGP_S2K_HASHED )
		{
		/* The legacy PGP 2.x key integrity protection format used a simple 
		   16-bit additive checksum of the encrypted MPI payload, the newer 
		   OpenPGP format uses a SHA-1 MDC.  There's also a halfway format 
		   used in older OpenPGP versions that still uses the 16-bit 
		   checksum but encrypts the entire MPI data block rather than just 
		   the payload */
		keyInfo->hashedChecksum = TRUE;
		}
	status = sread( stream, keyInfo->iv, ivSize );
	if( cryptStatusError( status ) )
		return( status );
	keyInfo->ivSize = ivSize;

	return( CRYPT_OK );
	}

/* Read public-key components */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readRSAKeyComponents( INOUT STREAM *stream, 
								 INOUT PGP_KEYINFO *keyInfo,
								 OUT_INT_Z int *pubKeyComponentLength )
	{
	int length, totalLength = 1, status;
				/* Initial length 1 is for the algorithm ID byte */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isWritePtr( pubKeyComponentLength, sizeof( int ) ) );

	/* Clear return value */
	*pubKeyComponentLength = 0;

	/* Read RSA n + e.  The LSBs of n serve as the PGP 2.x key ID so we copy 
	   the data out if PGP 2.x support is enabled */
	status = getMPIsize( stream, MIN_PKCSIZE, CRYPT_MAX_PKCSIZE, 
						 &length );			/* n */
	if( cryptStatusError( status ) )
		return( status );
	totalLength += length;
	
#ifdef USE_PGP2
	/* Move back and copy out the last PGP_KEYID_SIZE bytes of n as the PGP 
	   2.x key ID */
	static_assert( PGP_KEYID_SIZE < MIN_PKCSIZE, "PGP keyID size" );
	status = sseek( stream, stell( stream ) - PGP_KEYID_SIZE );
	if( cryptStatusOK( status ) )
		status = sread( stream, keyInfo->pgp2KeyID, PGP_KEYID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
#endif /* USE_PGP2 */
	status = getMPIsize( stream, 1, CRYPT_MAX_PKCSIZE, &length );
	if( cryptStatusError( status ) )		/* e */
		return( status );
	*pubKeyComponentLength = totalLength + length;

	return( CRYPT_OK );
	}

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readDLPKeyComponents( INOUT STREAM *stream, 
								 INOUT PGP_KEYINFO *keyInfo,
								 OUT_INT_Z int *pubKeyComponentLength )
	{
	int length, totalLength = 1, status;
				/* Initial length 1 is for the algorithm ID byte */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isWritePtr( pubKeyComponentLength, sizeof( int ) ) );

	/* Clear return value */
	*pubKeyComponentLength = 0;

	/* DSA/Elgamal: p + g + y */
	status = getMPIsize( stream, MIN_PKCSIZE, CRYPT_MAX_PKCSIZE, &length );	
	if( cryptStatusOK( status ) )				/* p */
		{
		totalLength += length;
		status = getMPIsize( stream, 1, CRYPT_MAX_PKCSIZE, &length );
		}										/* g */
	if( cryptStatusOK( status ) )
		{
		totalLength += length;
		status = getMPIsize( stream, MIN_PKCSIZE, CRYPT_MAX_PKCSIZE, 
							 &length );			/* y */
		}
	if( cryptStatusError( status ) )
		return( status );
	totalLength += length;
	if( keyInfo->pkcAlgo ==  CRYPT_ALGO_DSA )
		{
		/* DSA has q as well */
		status = getMPIsize( stream, bitsToBytes( 155 ), CRYPT_MAX_PKCSIZE, 
							 &length );			/* q */
		if( cryptStatusError( status ) )
			return( status );
		totalLength += length;
		}
	*pubKeyComponentLength = totalLength;

	return( CRYPT_OK );
	}

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readECCKeyComponents( INOUT STREAM *stream, 
								 INOUT PGP_KEYINFO *keyInfo,
								 OUT_INT_Z int *pubKeyComponentLength )
	{
	int length, totalLength = 2, status;
				/* Initial len.2 is for the algorithm ID and OID len.byte */

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isWritePtr( pubKeyComponentLength, sizeof( int ) ) );

	/* Clear return value */
	*pubKeyComponentLength = 0;

	/* Skip the ECC OID, which is preceded by a length byte */
	status = length = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length < MIN_OID_SIZE || length >= MAX_OID_SIZE )
		return( CRYPT_ERROR_BADDATA );
	status = sSkip( stream, length, MAX_OID_SIZE );
	if( cryptStatusError( status ) )
		return( status );
	totalLength += length;

	/* ECC: qx/qy as an ECC point */
	status = getMPIsize( stream, MIN_PKCSIZE_ECCPOINT, 
						 CRYPT_MAX_PKCSIZE_ECC, &length );	
	if( cryptStatusError( status ) )
		return( status );
	totalLength += length;

	/* If we're reading an ECDSA key, we're done */
	if( keyInfo->pkcAlgo == CRYPT_ALGO_ECDSA )
		{
		*pubKeyComponentLength = totalLength;
		return( CRYPT_OK );
		}

	/* ECDH keys are followed by further data that has nothing to do with 
	   ECDH but that's stuffed in with the key for no obvious reason */
	status = length = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( length != 3 )
		return( CRYPT_ERROR_BADDATA );
	status = sSkip( stream, 3, 3 );
	if( cryptStatusError( status ) )
		return( status );
	*pubKeyComponentLength = totalLength + 1 + length;

	return( CRYPT_OK );
	}

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
static int readPublicKeyComponents( INOUT STREAM *stream, 
									INOUT PGP_KEYINFO *keyInfo,
									OUT_INT_Z int *pubKeyComponentLength )
	{
	int pgpPkcAlgo, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( keyInfo, sizeof( PGP_KEYINFO ) ) );
	assert( isWritePtr( pubKeyComponentLength, sizeof( int ) ) );

	/* Clear return value */
	*pubKeyComponentLength = 0;

	/* Get the public-key algorithm type */
	status = pgpPkcAlgo = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );

	/* Set up the algorithm info and read the key components */
	switch( pgpPkcAlgo )
		{
		case PGP_ALGO_RSA:
		case PGP_ALGO_RSA_ENCRYPT:
		case PGP_ALGO_RSA_SIGN:
			keyInfo->pkcAlgo = CRYPT_ALGO_RSA;
			if( pgpPkcAlgo != PGP_ALGO_RSA_SIGN )
				keyInfo->usageFlags = KEYMGMT_FLAG_USAGE_CRYPT;
			if( pgpPkcAlgo != PGP_ALGO_RSA_ENCRYPT )
				keyInfo->usageFlags |= KEYMGMT_FLAG_USAGE_SIGN;
			return( readRSAKeyComponents( stream, keyInfo, 
										  pubKeyComponentLength ) );

		case PGP_ALGO_DSA:
			keyInfo->pkcAlgo = CRYPT_ALGO_DSA;
			keyInfo->usageFlags = KEYMGMT_FLAG_USAGE_SIGN;
			return( readDLPKeyComponents( stream, keyInfo, 
										  pubKeyComponentLength ) );

		case PGP_ALGO_ELGAMAL:
			keyInfo->pkcAlgo = CRYPT_ALGO_ELGAMAL;
			keyInfo->usageFlags = KEYMGMT_FLAG_USAGE_CRYPT;
			return( readDLPKeyComponents( stream, keyInfo, 
										  pubKeyComponentLength ) );

		case PGP_ALGO_ECDH:
			keyInfo->pkcAlgo = CRYPT_ALGO_ECDH;
			keyInfo->usageFlags = KEYMGMT_FLAG_USAGE_CRYPT;
			return( readECCKeyComponents( stream, keyInfo, 
										  pubKeyComponentLength ) );

		case PGP_ALGO_ECDSA:
			keyInfo->pkcAlgo = CRYPT_ALGO_ECDSA;
			keyInfo->usageFlags = KEYMGMT_FLAG_USAGE_SIGN;
			return( readECCKeyComponents( stream, keyInfo, 
										  pubKeyComponentLength ) );

		default:
			/* It's an unknown algorithm, skip this key */
			return( OK_SPECIAL );
		}

	retIntError();
	}

/* Read a sequence of userID packets */

CHECK_RETVAL_SPECIAL STDC_NONNULL_ARG( ( 1 ) ) \
static int readUserID( INOUT STREAM *stream, 
					   INOUT_OPT PGP_INFO *pgpInfo )
	{
	long packetLength;
	int ctb, packetType DUMMY_INIT, noPackets, status DUMMY_INIT;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( pgpInfo == NULL || \
			isWritePtr( pgpInfo, sizeof( PGP_INFO ) ) );

	ENSURES( pgpInfo == NULL || \
			 ( pgpInfo->lastUserID >= 0 && \
			   pgpInfo->lastUserID <= MAX_PGP_USERIDS ) );

	/* Skip keyring trust packets, signature packets, and any private 
	   packets (GPG uses packet type 61, which might be a DSA self-
	   signature).

	   PGP has two ways of indicating key usage, either directly via the key 
	   type (e.g. PGP_ALGO_RSA_ENCRYPT vs. PGP_ALGO_RSA_SIGN) or in a rather 
	   schizophrenic manner in signature packets by allowing the signer to 
	   specify an X.509-style key usage.  Since it can appear in both self-
	   sigs and certification signatures, the exact usage for a key is 
	   somewhat complex to determine as a certification signer could 
	   indicate that they trust the key when it's used for signing while a 
	   self-signer could indicate that the key should be used for 
	   encryption.  This appears to be a preference indication rather than a 
	   hard limit like the X.509 keyUsage and also contains other odds and 
	   ends as well such as key splitting indicators.  For now we don't make 
	   use of these flags as it's a bit difficult to figure out what's what, 
	   and in any case DSA vs. Elgamal doesn't need any further constraints 
	   since there's only one usage possible */
	for( noPackets = 0; noPackets < FAILSAFE_ITERATIONS_MED; noPackets++ )
		{
		/* See what we've got.  If we've run out of input due to reading the 
		   end of the packet group or it's a non-key-related packet, we're 
		   done */
		status = ctb = sPeek( stream );
		if( cryptStatusError( status ) )
			break;
		packetType = pgpGetPacketType( ctb );
		if( packetType != PGP_PACKET_TRUST && \
			packetType != PGP_PACKET_SIGNATURE && \
			packetType != PGP_PACKET_USERATTR && \
			!pgpIsReservedPacket( packetType ) )
			break;

		/* Skip the packet */
		status = pgpReadPacketHeader( stream, &ctb, &packetLength, 
									  getMinPacketSize( packetType ),
									  MAX_INTLENGTH_SHORT );
		if( cryptStatusOK( status ) )
			status = sSkip( stream, packetLength, MAX_INTLENGTH_SHORT );
		if( cryptStatusError( status ) )
			return( status );
		}
	if( cryptStatusError( status ) )
		{
		/* Running out of input due to reading the end of the packet group 
		   is a valid condition so we don't return a fatal error code if 
		   this occurss */
		return( ( status == CRYPT_ERROR_UNDERFLOW ) ? OK_SPECIAL : status );
		}
	if( noPackets >= FAILSAFE_ITERATIONS_MED )
		return( CRYPT_ERROR_BADDATA );

	/* If we've reached the end of the current collection of key packets, 
	   let the caller know that we're done */
	if( packetType != PGP_PACKET_USERID )
		return( OK_SPECIAL );

	/* Record the userID (unless we're skipping the packet).  If there are 
	   more userIDs than we can record then we silently ignore them.  This 
	   handles keys with weird numbers of userIDs without rejecting them 
	   just because they have, well, a weird number of userIDs */
	status = pgpReadPacketHeader( stream, &ctb, &packetLength, 
								  getMinPacketSize( packetType ), 1024 );
	if( cryptStatusError( status ) )
		return( status );
	if( pgpInfo != NULL && pgpInfo->lastUserID < MAX_PGP_USERIDS )
		{
		void *dataPtr;

		status = sMemGetDataBlock( stream, &dataPtr, packetLength );
		if( cryptStatusError( status ) )
			return( status );
		pgpInfo->userID[ pgpInfo->lastUserID ] = dataPtr;
		pgpInfo->userIDlen[ pgpInfo->lastUserID++ ] = ( int ) packetLength;
		}
	return( sSkip( stream, packetLength, MAX_INTLENGTH_SHORT ) );
	}

/****************************************************************************
*																			*
*									Read a Key								*
*																			*
****************************************************************************/

/* Read a single key in a group of key packets.  A packet group consists of 
   a jumble of packets concatenated together following a primary key with 
   the jumble continuing until we encounter another primary key, which is 
   why we need the auxiliary scanPacketGroup() function to look ahead in the
   data stream to determine when to stop.  A typical set of packets might be:

	DSA key
	UserID
	Binding signature of DSA key and UserID
	Trust rating
	Elgamal subkey
	Binding signature of DSA key and Elgamal subkey
	Trust rating

   but almost anything else is possible, there can be arbitrary further 
   userIDs, keys, and other data floating around, all glued together in 
   various locations with binding signatures (or possibly not, since the
   signatures are optional).

   All of this is read into memory in a PGP_INFO structure with the 
   encoded data retained as follows:

	pgpInfo[ index ]->keyData			= entire packet group
					->key->pubKeyData	= DSA/RSA pubkey payload
					->key->privKeyData	= DSA/RSA privkey payload (without
										  decryption information, which is
										  stored separately)
					->subKey->pubKeyData= Elgamal pubkey payload
					->subKey->privKeyData=Elgamal privkey as before.

   This tries to simplify things somewhat because in practice there can be a 
   more or less arbitrary number of subkeys present, not just the obvious 
   encryption subkeys signed with the primary signing key but also further
   signing keys as subkeys, with a binding signature between the primary and
   subkey made using the subkey instead of the primary key (although it can
   also contain another binding signature from the primary key as well).

   The more or less arbitrarily complex nature of all of these bits and 
   pieces is why PGP 5, which used a keying format that was still vastly
   simpler than the current one, was split into two separate applications
   of which the more complex one did nothing but key handling and the other,
   simpler one did everything else in PGP.  It's also why we don't support
   public keyring writes, it would require a complete keyring management
   application just to deal with all of this complexity.

   Even just to read all of this stuff we have to simplify the processing a
   bit by recording the first primary key and subkey and skipping anything 
   else that may be present, the addition of arbitrary numbers of further
   subkeys each potentially with their own per-subkey userIDs is too complex
   to handle without again building a complete key management application, 
   and in any case such oddball keys are fairly rare and the basic greedy
   algorithm for handling them seems to work fine */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
static int readKey( INOUT STREAM *stream, 
					INOUT PGP_INFO *pgpInfo, 
					IN_INT_SHORT_Z const int keyGroupNo,
					INOUT ERROR_INFO *errorInfo )
	{
	PGP_KEYINFO *keyInfo = &pgpInfo->key;
	HASHFUNCTION hashFunction;
	HASHINFO hashInfo;
	BYTE hash[ CRYPT_MAX_HASHSIZE + 8 ], packetHeader[ 16 + 8 ];
	BOOLEAN isPublicKey = TRUE;
	void *pubKeyPayload;
	long packetLength;
	int pubKeyPos, pubKeyPayloadPos, endPos, pubKeyPayloadLen;
	int ctb, length, value, hashSize, noUserIDs, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( keyGroupNo >= 0 && keyGroupNo < MAX_INTLENGTH_SHORT );
	REQUIRES( errorInfo != NULL );

	/* Process the CTB and packet length */
	status = ctb = sPeek( stream );
	if( cryptStatusError( status ) )
		{
		/* If there was an error reading the CTB, which is the first byte of 
		   the packet group, it means that we've run out of data so we 
		   return the status as a not-found error rather than the actual
		   stream status */
		return( CRYPT_ERROR_NOTFOUND );
		}
	switch( pgpGetPacketType( ctb ) )
		{
		case PGP_PACKET_SECKEY_SUB:
			keyInfo = &pgpInfo->subKey;
			isPublicKey = FALSE;
			break;

		case PGP_PACKET_SECKEY:
			isPublicKey = FALSE;
			break;

		case PGP_PACKET_PUBKEY_SUB:
			keyInfo = &pgpInfo->subKey;
			break;

		case PGP_PACKET_PUBKEY:
			break;

		default:
			retExt( CRYPT_ERROR_BADDATA, 
					( CRYPT_ERROR_BADDATA, errorInfo, 
					  "Invalid PGP CTB %02X for key packet group %d", 
					  ctb, keyGroupNo ) );
		}
	status = pgpReadPacketHeader( stream, NULL, &packetLength, 64, 
								  MAX_INTLENGTH_SHORT );
	if( cryptStatusError( status ) )
		{
		retExt( status,
				( status, errorInfo, 
				  "Invalid PGP key packet header for key packet group %d", 
				  keyGroupNo ) );
		}
	if( packetLength < 64 || packetLength > sMemDataLeft( stream ) )
		{
		retExt( CRYPT_ERROR_BADDATA, 
				( CRYPT_ERROR_BADDATA, errorInfo, 
				  "Invalid PGP key packet length %ld for key packet group %d", 
				  packetLength, keyGroupNo ) );
		}

	/* Since there can (in theory) be arbitrary numbers of further subkeys 
	   and other odds and ends attached to an existing key and the details 
	   of what to do with these things gets a bit vague, we just skip any 
	   additional subkeys that may be present.
	   
	   How to handle userIDs in this case is a bit unclear since there could
	   be different userIDs attached to a subkey, however stripping them 
	   would make the main key invisible if searched for by one of the 
	   subkey IDs, so we read them and treat them as if they belonged to the
	   main key (it's uncertain whether such an oddball configuration of key
	   packets even exists, or what to do with them if it does) */
	if( keyInfo->pkcAlgo != CRYPT_ALGO_NONE )
		{
		status = sSkip( stream, packetLength, MAX_INTLENGTH_SHORT );
		for( noUserIDs = 0; 
			 cryptStatusOK( status ) && noUserIDs < FAILSAFE_ITERATIONS_MED; 
			 noUserIDs++ )
			{
			status = readUserID( stream, pgpInfo );
			}
		if( cryptStatusOK( status ) && noUserIDs >= FAILSAFE_ITERATIONS_MED )
			status = CRYPT_ERROR_OVERFLOW;
		if( cryptStatusError( status ) && status != OK_SPECIAL )
			{
			retExt( status, 
					( status, errorInfo, 
					  "Invalid additional PGP subkey information for key "
					  "packet group %d", keyGroupNo ) );
			}

		/* We've skipped the current subkey, we're done */
		return( CRYPT_OK );
		}

	/* Determine which bits make up the public and the private key data.  The
	   public-key data starts at the version number and includes the date,
	   validity, and public-key components.  Since there's no length 
	   information included for this data block we have to record bookmarks
	   and then later retroactively calculate the length based on how much
	   data we've read in the meantime:

		pubKeyPos pubKeyPayload			 privKey				 endPos
			|		|						|						|
			v		v						v						v
		+---+-------------------------------+-----------------------+
		|hdr|		|		Public key		|		Private key		|
		+---+-------------------------------+-----------------------+
			|		|<--pubKeyPayloadLen--->|						|
			|								|						|
			|<------- pubKeyDataLen ------->|<-- privKeyDataLen --->| 
			|<----------------- packetLength ---------------------->| */
	pubKeyPos = stell( stream );
	endPos = pubKeyPos + packetLength;
	ENSURES( endPos > pubKeyPos && endPos < MAX_BUFFER_SIZE );
	status = value = sgetc( stream );
	if( cryptStatusError( status ) )
		return( status );
	if( value != PGP_VERSION_2 && value != PGP_VERSION_3 && \
		value != PGP_VERSION_OPENPGP )
		{
		/* Unknown version number, skip this packet */
		return( OK_SPECIAL );
		}
	pgpInfo->isOpenPGP = ( value == PGP_VERSION_OPENPGP ) ? TRUE : FALSE;

	/* Build the packet header, which is hashed along with the key components
	   to get the OpenPGP keyID.  This is generated anyway when the context
	   is created but we need to generate it here as well in order to locate
	   the key in the first place:

		byte		ctb = 0x99
		byte[2]		length
		byte		version = 4
		byte[4]		key generation time
	  [	byte[2]		validity time - PGP 2.x only ]
		byte[]		key data

	   We can't add the length or key data yet since we have to parse the
	   key data to know how long it is, so we can only build the static part
	   of the header at this point */
	packetHeader[ 0 ] = 0x99;
	packetHeader[ 3 ] = PGP_VERSION_OPENPGP;

	/* Read the timestamp and skip the validity period (for PGP 2.x keys) */
	status = sread( stream, packetHeader + 4, 4 );
	if( cryptStatusOK( status ) && !pgpInfo->isOpenPGP )
		status = sSkip( stream, 2, 2 );
	if( cryptStatusError( status ) )
		return( status );

	/* Read the public key components */
	pubKeyPayloadPos = stell( stream );
	status = readPublicKeyComponents( stream, keyInfo, &length );
	if( cryptStatusError( status ) )
		{
		/* If the error status is OK_SPECIAL then the problem was an
		   unrecognised algorithm or something similar so we just skip the 
		   packet */
		if( status == OK_SPECIAL )
			{
			DEBUG_DIAG(( "Encountered unrecognised algorithm while "
						 "reading PGP key %d", keyGroupNo ));
			assert_nofuzz( DEBUG_WARN );
			return( OK_SPECIAL );
			}
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PGP public-key components for key packet group %d",
				  keyGroupNo ) );
		}

	/* Now that we know where the public key data starts and finishes, we 
	   can set up references to it */
	keyInfo->pubKeyDataLen = stell( stream ) - pubKeyPos;
	status = sMemGetDataBlockAbs( stream, pubKeyPos, &keyInfo->pubKeyData, 
								  keyInfo->pubKeyDataLen );
	if( cryptStatusError( status ) )
		{
		assert( DEBUG_WARN );
		return( status );
		}
	pubKeyPayloadLen = stell( stream ) - pubKeyPayloadPos;
	status = sMemGetDataBlockAbs( stream, pubKeyPayloadPos, &pubKeyPayload, 
								  pubKeyPayloadLen );
	if( cryptStatusError( status ) )
		{
		assert( DEBUG_WARN );
		return( status );
		}

	/* Complete the packet header that we read earlier on by adding the
	   length information */
	packetHeader[ 1 ] = intToByte( ( ( 1 + 4 + length ) >> 8 ) & 0xFF );
	packetHeader[ 2 ] = intToByte( ( 1 + 4 + length ) & 0xFF );

	/* Hash the data needed to generate the OpenPGP keyID */
	getHashParameters( CRYPT_ALGO_SHA1, 0, &hashFunction, &hashSize );
	hashFunction( hashInfo, NULL, 0, packetHeader, 1 + 2 + 1 + 4, 
				  HASH_STATE_START );
	hashFunction( hashInfo, hash, CRYPT_MAX_HASHSIZE, 
				  pubKeyPayload, pubKeyPayloadLen, HASH_STATE_END );
	memcpy( keyInfo->openPGPkeyID, hash + hashSize - PGP_KEYID_SIZE,
			PGP_KEYID_SIZE );

	/* If it's a private keyring, process the private key components */
	if( !isPublicKey )
		{
		/* Handle decryption information for private-key components if 
		   necessary */
		status = readPrivateKeyDecryptionInfo( stream, keyInfo );
		if( cryptStatusError( status ) )
			{
			/* If the error status is OK_SPECIAL then the problem was an
			   unrecognised algorithm or something similar so we just skip
			   the packet */
			if( status == OK_SPECIAL )
				{
				DEBUG_DIAG(( "Encountered unrecognised algorithm while "
							 "reading PGP private key %d", keyGroupNo ));
				assert_nofuzz( DEBUG_WARN );
				return( OK_SPECIAL );
				}
			retExt( status, 
					( status, errorInfo, 
					  "Invalid PGP private-key decryption information for "
					  "key packet group %d", keyGroupNo ) );
			}

		/* What's left is the private-key data */
		keyInfo->privKeyDataLen = endPos - stell( stream );
		if( keyInfo->privKeyDataLen < 16 || \
			keyInfo->privKeyDataLen > MAX_INTLENGTH_SHORT )
			return( CRYPT_ERROR_BADDATA );
		status = sMemGetDataBlock( stream, &keyInfo->privKeyData, 
								   keyInfo->privKeyDataLen );
		if( cryptStatusOK( status ) )
			{
			status = sSkip( stream, keyInfo->privKeyDataLen, 
							MAX_INTLENGTH_SHORT );
			}
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Read any associated subpacket(s), of which the only ones of real 
	   interest are the userID packet(s).  readUserID() returns OK_SPECIAL
	   once it's run out of packets so this status isn't treated as an 
	   error */
	for( noUserIDs = 0; 
		 cryptStatusOK( status ) && noUserIDs < FAILSAFE_ITERATIONS_MED; 
		 noUserIDs++ )
		{
		status = readUserID( stream, pgpInfo );
		}
	if( cryptStatusOK( status ) && noUserIDs >= FAILSAFE_ITERATIONS_MED )
		status = CRYPT_ERROR_OVERFLOW;
	if( cryptStatusError( status ) && status != OK_SPECIAL )
		{
		retExt( status, 
				( status, errorInfo, 
				  "Invalid PGP userID information for key packet group %d",
				  keyGroupNo ) );
		}

	/* If there's no user ID present, set a generic label */
	if( pgpInfo->lastUserID <= 0 )
		{
		pgpInfo->userID[ 0 ] = "PGP key (no user ID found)";
		pgpInfo->userIDlen[ 0 ] = 26;
		pgpInfo->lastUserID = 1;
		}

	return( CRYPT_OK );
	}

/* Process the information in the packet group */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 6 ) ) \
static int processPacketGroup( INOUT STREAM *stream, 
							   INOUT PGP_INFO *pgpInfo,
							   IN_OPT const KEY_MATCH_INFO *keyMatchInfo,
							   INOUT_OPT PGP_KEYINFO **matchedKeyInfoPtrPtr,
							   IN_INT_SHORT_Z const int keyGroupNo,
							   INOUT ERROR_INFO *errorInfo )
	{
	int noPackets, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( ( keyMatchInfo == NULL && matchedKeyInfoPtrPtr == NULL ) || \
			( isReadPtr( keyMatchInfo, sizeof( KEY_MATCH_INFO ) ) && \
			  isWritePtr( matchedKeyInfoPtrPtr, sizeof( PGP_KEYINFO * ) ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( ( keyMatchInfo == NULL && matchedKeyInfoPtrPtr == NULL ) || \
			  ( keyMatchInfo != NULL && matchedKeyInfoPtrPtr != NULL && \
				pgpInfo->keyData != NULL && \
				pgpInfo->keyDataLen > 0 && \
				pgpInfo->keyDataLen < MAX_INTLENGTH_SHORT ) );
	REQUIRES( keyGroupNo >= 0 && keyGroupNo < MAX_INTLENGTH_SHORT );
	REQUIRES( errorInfo != NULL );

	/* Reset the index information before we read the current key(s), since 
	   it may already have been initialised during a previous (incomplete) 
	   key read */
	resetPGPInfo( pgpInfo );

	/* Read all the packets in this packet group */
	for( status = CRYPT_OK, noPackets = 0;
		 cryptStatusOK( status ) && sMemDataLeft( stream ) > 0 && \
			noPackets++ < FAILSAFE_ITERATIONS_MED;
		 noPackets++ )
		{
		status = readKey( stream, pgpInfo, keyGroupNo, errorInfo );
		}
	if( cryptStatusError( status ) )
		return( status );
	if( noPackets >= FAILSAFE_ITERATIONS_MED )
		return( CRYPT_ERROR_OVERFLOW );

	/* If we're reading all keys, we're done */
	if( keyMatchInfo == NULL )
		return( CRYPT_OK );

	/* We're searching for a particular key, see if this is the one */
	if( pgpCheckKeyMatch( pgpInfo, &pgpInfo->key, keyMatchInfo ) )
		{
		*matchedKeyInfoPtrPtr = &pgpInfo->key;
		return( CRYPT_OK );
		}
	if( pgpCheckKeyMatch( pgpInfo, &pgpInfo->subKey, keyMatchInfo ) )
		{
		*matchedKeyInfoPtrPtr = &pgpInfo->subKey;
		return( CRYPT_OK );
		}

	/* No match, tell the caller to keep looking */
	return( CRYPT_ERROR_NOTFOUND );
	}

/****************************************************************************
*																			*
*								Read a Keyring								*
*																			*
****************************************************************************/

/* Read an entire keyring.  This function can be used in one of two ways, if 
   key match information is supplied then each packet will be checked 
   against it and the read will exit when a match is found (used for public 
   keyrings).  If no key match information is supplied then all keys will be 
   read into memory (used for private keyrings) */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4, 8, 9 ) ) \
static int processKeyringPackets( INOUT STREAM *stream, 
								  IN_ARRAY( maxNoPgpObjects ) PGP_INFO *pgpInfo, 
								  IN_LENGTH_SHORT const int maxNoPgpObjects,
								  OUT_BUFFER_FIXED( bufSize ) BYTE *buffer, 
								  IN_LENGTH_FIXED( KEYRING_BUFSIZE ) const int bufSize,
								  IN_OPT const KEY_MATCH_INFO *keyMatchInfo,
								  INOUT_OPT PGP_KEYINFO **matchedKeyInfoPtrPtr,
								  OUT BOOLEAN *unhandledDataPresent,
								  INOUT ERROR_INFO *errorInfo )
	{
	BOOLEAN moreData, insecureKeys = FALSE;
	int bufEnd, keyGroupNo = 0, iterationCount, status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) * maxNoPgpObjects ) );
	assert( isWritePtr( buffer, bufSize ) );
	assert( ( keyMatchInfo == NULL && matchedKeyInfoPtrPtr == NULL ) || \
			( isReadPtr( keyMatchInfo, sizeof( KEY_MATCH_INFO ) ) && \
			  isWritePtr( matchedKeyInfoPtrPtr, sizeof( PGP_KEYINFO * ) ) ) );
	assert( isWritePtr( unhandledDataPresent, sizeof( BOOLEAN ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( maxNoPgpObjects >= 1 && maxNoPgpObjects < MAX_INTLENGTH_SHORT );
	REQUIRES( bufSize == KEYRING_BUFSIZE );
	REQUIRES( ( keyMatchInfo == NULL && matchedKeyInfoPtrPtr == NULL ) || \
			  ( keyMatchInfo != NULL && matchedKeyInfoPtrPtr != NULL && \
				pgpInfo->keyData != NULL && \
				pgpInfo->keyDataLen == KEYRING_BUFSIZE ) );
	REQUIRES( errorInfo != NULL );

	/* Clear return value */
	*unhandledDataPresent = FALSE;

	/* Scan all of the objects in the keyset.  This is implemented as a 
	   sliding window that reads a certain amount of data into a lookahead 
	   buffer and then tries to identify a packet group in the buffer */
	for( moreData = TRUE, bufEnd = 0, iterationCount = 0;
		 ( moreData || bufEnd > 0 ) && \
			iterationCount < FAILSAFE_ITERATIONS_MAX;
		 iterationCount++ )
		{
		PGP_INFO *pgpInfoPtr = &pgpInfo[ keyGroupNo ];
		STREAM keyStream;
		int length DUMMY_INIT;	/* Init needed by gcc */

		/* Fill the lookahead buffer:

			 buffer		 bufEnd			 bufSize
				|			|				|
				v			v				v
				+-----------+---------------+
				|///////////|				|
				+-----------+---------------+ 
							|
							+-- length --> */
		if( moreData )
			{
			REQUIRES( bufEnd >= 0 && bufEnd < bufSize );

			status = length = sread( stream, buffer + bufEnd,
									 bufSize - bufEnd );
			if( cryptStatusError( status ) || length <= 0 )
				{
				/* If we read nothing and there's nothing left in the buffer,
				   we're done */
				if( bufEnd <= 0 )
					{
					/* If we've previously read at least one group of key 
					   packets then we're OK */
					if( keyGroupNo > 0 )
						return( CRYPT_OK );

					/* There's no existing data still in the buffer and an
					   attempt to read more resulted in an error, we can't 
					   go any further */
					return( cryptStatusError( status ) ? status : \
							( keyMatchInfo != NULL ) ? CRYPT_ERROR_NOTFOUND : \
													   CRYPT_ERROR_UNDERFLOW );
					}

				/* There's still data in the buffer from a previous read, we 
				   can continue until we drain it */
				length = 0;
				moreData = FALSE;
				}
			else
				{
				/* If we didn't get as much as we requested then there's 
				   nothing left to read */
				if( length < bufSize - bufEnd )
					moreData = FALSE;
				}
			bufEnd += length;

			ENSURES( bufEnd > 0 && bufEnd <= bufSize );
			}

		/* Determine the size of the group of key packets in the buffer */
		sMemConnect( &keyStream, buffer, bufEnd );
		status = scanPacketGroup( &keyStream, &length, bufEnd );
		sMemDisconnect( &keyStream );
		if( cryptStatusError( status ) )
			{
			retExt( status,
					( status, errorInfo, 
					  "Couldn't parse key packet group %d", keyGroupNo ) );
			}
		ENSURES( length > 0 && length <= bufEnd );

		/* Move the packet group from the keyring buffer to the key data */  
		if( keyMatchInfo == NULL )
			{
			/* It's a private-key read of all packets, allocate room for the 
			   current packet group */
			if( ( pgpInfoPtr->keyData = \
						clAlloc( "processKeyringPackets", length ) ) == NULL )
				return( CRYPT_ERROR_MEMORY );
			}
		else
			{
			/* It's a public-key read and we're just scanning through the 
			   packets looking for a match.  In this case pgpInfoPtr == 
			   &pgpInfo[ 0 ], with pgpInfoPtr->keyData being a fixed buffer 
			   of size KEYRING_BUFSIZE, with length <= KEYRING_BUFSIZE */
			ENSURES( length <= KEYRING_BUFSIZE );
			}
		memcpy( pgpInfoPtr->keyData, buffer, length );
		pgpInfoPtr->keyDataLen = length;

		/* Remove the packet group from the read buffer:

				 length	  bufEnd			  length
					|		|					|
					v		v					v
			+-------+-------+---+		+-------+-----------+
			|///////|\\\\\\\|	| ---->	|\\\\\\\|			|
			+-------+-------+---+		+-------+-----------+
			   To	   To
			 remove	  move */
		if( length < bufEnd )
			{
			REQUIRES( rangeCheck( length, bufEnd - length, bufSize ) );
			memmove( buffer, buffer + length, bufEnd - length );
			}
		bufEnd -= length;
		ENSURES( bufEnd >= 0 && bufEnd < bufSize );

		/* Process the current packet group */
		sMemConnect( &keyStream, pgpInfoPtr->keyData, 
					 pgpInfoPtr->keyDataLen );
		status = processPacketGroup( &keyStream, pgpInfoPtr, keyMatchInfo,
									 matchedKeyInfoPtrPtr, keyGroupNo, 
									 errorInfo );
		sMemDisconnect( &keyStream );
		if( cryptStatusError( status ) )
			{
			/* If we were looking for a match for a particular key and we
			   didn't find it, continue */
			if( keyMatchInfo != NULL && status == CRYPT_ERROR_NOTFOUND )
				continue;

			/* If it's not a recoverable error, exit */
			if( status != OK_SPECIAL && status != CRYPT_ERROR_NOSECURE )
				return( status );

			/* Remember that we hit something that we couldn't process, and
			   optionally an (unusable) unprotected key */
			*unhandledDataPresent = TRUE;
			if( status == CRYPT_ERROR_NOSECURE )
				insecureKeys = TRUE;
			if( keyMatchInfo == NULL )
				{
				/* Free the entry that we allocated earlier */
				pgpFreeEntry( pgpInfo );
				}
			continue;
			}

		/* If we're looking for a particular key, we've found it */
		if( keyMatchInfo != NULL )
			return( CRYPT_OK );

		/* We're reading all keys, move on to the next empty slot.  Note 
		   that we only get to this point if we've been able to do something 
		   with the key, if not then the unhandledDataPresent flag will be 
		   set without moving on to the next key group */
		keyGroupNo++;
		if( keyGroupNo >= maxNoPgpObjects )
			{
			retExt( CRYPT_ERROR_OVERFLOW, 
					( CRYPT_ERROR_OVERFLOW, errorInfo, 
					  "Maximum keyset object item count %d reached, no more "
					  "room to add further items", maxNoPgpObjects ) );
			}
		}
	ENSURES( iterationCount < FAILSAFE_ITERATIONS_MAX );
			 /* This is safe since under normal circumstances we'll hit the 
				keyGroupNo bound long before we encounter this check */

	/* If we were looking for a specific match, we haven't found it */
	if( keyMatchInfo != NULL )
		return( CRYPT_ERROR_NOTFOUND );

	/* If we haven't found any keys that we can use, let the caller know.  
	   The error code to return here is a bit complex because we can skip 
	   keys either because there's something in the key data that we can't
	   process or because the keys aren't sufficiently protected for us to
	   trust them.  The most complex case is when both situations occur.  
	   To keep it simple, if there are any insecure keys then we report
	   CRYPT_ERROR_NOSECURE on the basis that if the keys had been 
	   appropriately secured then we might have been able to process them 
	   and return one */
	if( keyGroupNo <= 0 )
		{
		return( insecureKeys ? CRYPT_ERROR_NOSECURE : \
							   CRYPT_ERROR_NOTFOUND );
		}

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
int pgpScanPubKeyring( INOUT STREAM *stream, 
					   INOUT PGP_INFO *pgpInfo, 
					   const KEY_MATCH_INFO *keyMatchInfo,
					   INOUT PGP_KEYINFO **matchedKeyInfoPtrPtr,
					   INOUT ERROR_INFO *errorInfo )
	{
	BYTE buffer[ KEYRING_BUFSIZE + 8 ], *streamBuffer;
	BOOLEAN unhandledDataPresent;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) ) );
	assert( isReadPtr( keyMatchInfo, sizeof( KEY_MATCH_INFO ) ) );
	assert( isWritePtr( matchedKeyInfoPtrPtr, sizeof( PGP_KEYINFO * ) ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( pgpInfo->keyData != NULL && \
			  pgpInfo->keyDataLen == KEYRING_BUFSIZE );
	REQUIRES( keyMatchInfo != NULL && matchedKeyInfoPtrPtr != NULL );
	REQUIRES( errorInfo != NULL );

	/* Clear the return value */
	*matchedKeyInfoPtrPtr = NULL;

	/* PGP keyrings just contain an arbitrary collection of packets 
	   concatenated together so we can't tell in advance how much data we 
	   should be reading.  Because of this we have to set the file stream to 
	   allow partial reads without returning a read error */
	sioctlSet( stream, STREAM_IOCTL_PARTIALREAD, TRUE );

	/* Since we're scanning an arbitrarily-large collection of packets for a 
	   match we need to allocate a stream buffer, since the stream for the 
	   public keyring is merely an open file handle that can be scanned and 
	   re-scanned as required to locate keys.  The somewhat awkward 
	   buffering scheme is used because we need one buffer for the stream 
	   and another to process keyring packets in, we can't use one for both */
	if( ( streamBuffer = clAlloc( "readKeyring", STREAM_BUFSIZE ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	sioctlSetString( stream, STREAM_IOCTL_IOBUFFER, streamBuffer, 
					 STREAM_BUFSIZE );
	status = processKeyringPackets( stream, pgpInfo, 1, buffer, 
									KEYRING_BUFSIZE, keyMatchInfo, 
									matchedKeyInfoPtrPtr, 
									&unhandledDataPresent, errorInfo );
	sioctlSet( stream, STREAM_IOCTL_IOBUFFER, 0 );
	clFree( "readKeyring", streamBuffer );
	if( cryptStatusError( status ) )
		return( status );

	/* If we couldn't process one or more packets let the caller know that 
	   not all keyring data is present in memory */
	return( unhandledDataPresent ? OK_SPECIAL : CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int pgpReadPrivKeyring( INOUT STREAM *stream, 
						IN_ARRAY( maxNoPgpObjects ) PGP_INFO *pgpInfo, 
						IN_LENGTH_SHORT const int maxNoPgpObjects,
						INOUT ERROR_INFO *errorInfo )
	{
	BYTE buffer[ KEYRING_BUFSIZE + 8 ];
	BOOLEAN unhandledDataPresent;
	int status;

	assert( isWritePtr( stream, sizeof( STREAM ) ) );
	assert( isWritePtr( pgpInfo, sizeof( PGP_INFO ) * maxNoPgpObjects ) );
	assert( isWritePtr( errorInfo, sizeof( ERROR_INFO ) ) );

	REQUIRES( maxNoPgpObjects >= 1 && maxNoPgpObjects < MAX_INTLENGTH_SHORT );
	REQUIRES( errorInfo != NULL );

	/* PGP keyrings just contain an arbitrary collection of packets 
	   concatenated together so we can't tell in advance how much data we 
	   should be reading.  Because of this we have to set the file stream to 
	   allow partial reads without returning a read error */
	sioctlSet( stream, STREAM_IOCTL_PARTIALREAD, TRUE );

	/* Read all of the keyring packets into memory */
	status = processKeyringPackets( stream, pgpInfo, maxNoPgpObjects, 
									buffer, KEYRING_BUFSIZE, NULL, NULL, 
									&unhandledDataPresent, errorInfo );
	if( cryptStatusError( status ) )
		return( status );

	/* If we couldn't process one or more packets let the caller know that 
	   not all keyring data is present in memory */
	return( unhandledDataPresent ? OK_SPECIAL : CRYPT_OK );
	}
#endif /* USE_PGPKEYS */
