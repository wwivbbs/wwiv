/****************************************************************************
*																			*
*							X.509/PKI Signature Routines					*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "mech.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "mechs/mech.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*							X.509-style Signature Functions 				*
*																			*
****************************************************************************/

/* Create/check an X.509-style signature.  These work with objects of the
   form:

	signedObject ::= SEQUENCE {
		object				ANY,
		signatureAlgorithm	AlgorithmIdentifier,
		signature			BIT STRING
		}

   This is complicated by a variety of b0rken PKI protocols that couldn't
   quite manage a cut & paste of two lines of text, adding all sorts of
   unnecessary extra tagging and wrappers to the signature.  These odds and
   ends are specified in the formatInfo structure */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 4 ) ) \
int createX509signature( OUT_BUFFER( signedObjectMaxLength, \
									 *signedObjectLength ) \
							void *signedObject, 
						 IN_DATALENGTH_Z const int signedObjectMaxLength, 
						 OUT_DATALENGTH_Z int *signedObjectLength,
						 IN_BUFFER( objectLength ) const void *object, 
						 IN_DATALENGTH const int objectLength,
						 IN_HANDLE const CRYPT_CONTEXT iSignContext,
						 IN_ALGO const CRYPT_ALGO_TYPE hashAlgo,
						 IN_OPT const X509SIG_FORMATINFO *formatInfo )
	{
	CRYPT_CONTEXT iHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	BYTE dataSignature[ CRYPT_MAX_PKCSIZE + 128 + 8 ];
	int signatureLength, totalSigLength, status;

	assert( isWritePtr( signedObject, signedObjectMaxLength ) );
	assert( isWritePtr( signedObjectLength, sizeof( int ) ) );
	assert( isReadPtr( object, objectLength ) && \
			!cryptStatusError( checkObjectEncoding( object, \
													objectLength ) ) );
	assert( formatInfo == NULL || \
			isReadPtr( formatInfo, sizeof( X509SIG_FORMATINFO ) ) );

	REQUIRES( signedObjectMaxLength > MIN_CRYPT_OBJECTSIZE && \
			  signedObjectMaxLength < MAX_BUFFER_SIZE );
	REQUIRES( objectLength > 0 && objectLength < MAX_BUFFER_SIZE );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHashAlgo( hashAlgo ) );
	REQUIRES( formatInfo == NULL || \
			  ( ( formatInfo->tag >= 0 && \
				  formatInfo->tag < MAX_CTAG_VALUE ) && \
				( formatInfo->extraLength >= 0 && \
				  formatInfo->extraLength < MAX_INTLENGTH_SHORT ) ) );

	/* Clear return values */
	memset( signedObject, 0, min( 16, signedObjectMaxLength ) );
	*signedObjectLength = 0;

	/* Hash the data to be signed */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE, IMESSAGE_DEV_CREATEOBJECT,
							  &createInfo, OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iHashContext = createInfo.cryptHandle;
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
							  ( MESSAGE_CAST ) object, objectLength );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, "", 0 );
	if( cryptStatusError( status ) )
		return( status );

	/* Create the signature and calculate the overall length of the payload, 
	   optional signature wrapper, and signature data */
	status = createSignature( dataSignature, CRYPT_MAX_PKCSIZE + 128, 
							  &signatureLength, iSignContext, 
							  createInfo.cryptHandle, CRYPT_UNUSED, 
							  SIGNATURE_X509 );
	krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
	if( cryptStatusError( status ) )
		return( status );
	if( formatInfo == NULL )
		totalSigLength = signatureLength;
	else
		{
		/* It's a nonstandard format, figure out the size due to the 
		   additional signature wrapper and other odds and ends */
		totalSigLength = ( int ) \
				sizeofObject( signatureLength + formatInfo->extraLength );
		if( formatInfo->isExplicit )
			totalSigLength = ( int ) sizeofObject( totalSigLength );
		}
	ENSURES( totalSigLength > 40 && totalSigLength < MAX_BUFFER_SIZE );

	/* Make sure that there's enough room for the signed object in the 
	   output buffer.  This will be checked by the stream handling anyway 
	   but we make it explicit here */
	if( sizeofObject( objectLength + totalSigLength ) > signedObjectMaxLength )
		return( CRYPT_ERROR_OVERFLOW );

	/* Write the outer SEQUENCE wrapper and copy the payload into place 
	   behind it */
	sMemOpen( &stream, signedObject, signedObjectMaxLength );
	writeSequence( &stream, objectLength + totalSigLength );
	swrite( &stream, object, objectLength );

	/* If it's a nonstandard (b0rken PKI protocol) signature we have to 
	   kludge in a variety of additional wrappers and other junk around the 
	   signature */
	if( formatInfo != NULL )
		{
		if( formatInfo->isExplicit )
			{
			writeConstructed( &stream, 
							  sizeofObject( signatureLength + \
											formatInfo->extraLength ),
							  formatInfo->tag );
			writeSequence( &stream, 
						   signatureLength + formatInfo->extraLength );
			}
		else
			{
			writeConstructed( &stream, 
							  signatureLength + formatInfo->extraLength,
							  formatInfo->tag );
			}
		}

	/* Finally, append the signature */
	status = swrite( &stream, dataSignature, signatureLength );
	if( cryptStatusOK( status ) )
		*signedObjectLength = stell( &stream );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );

	assert( ( formatInfo != NULL && formatInfo->extraLength > 0 ) || \
			!cryptStatusError( checkObjectEncoding( signedObject, \
													*signedObjectLength ) ) );
	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkX509signature( IN_BUFFER( signedObjectLength ) const void *signedObject, 
						IN_DATALENGTH const int signedObjectLength,
						IN_HANDLE const CRYPT_CONTEXT iSigCheckContext,
						IN_OPT const X509SIG_FORMATINFO *formatInfo )
	{
	CRYPT_ALGO_TYPE signAlgo, hashAlgo;
	CRYPT_CONTEXT iHashContext;
	MESSAGE_CREATEOBJECT_INFO createInfo;
	STREAM stream;
	void *objectPtr DUMMY_INIT_PTR, *sigPtr;
	long length;
	int sigCheckAlgo, sigLength, hashParam, status;	/* int vs.enum */

	assert( isReadPtr( signedObject, signedObjectLength ) );
	assert( formatInfo == NULL || \
			isReadPtr( formatInfo, sizeof( X509SIG_FORMATINFO ) ) );

	REQUIRES( signedObjectLength > 0 && \
			  signedObjectLength < MAX_BUFFER_SIZE );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( formatInfo == NULL || \
			  ( ( formatInfo->tag >= 0 && \
				  formatInfo->tag < MAX_CTAG_VALUE ) && \
				( formatInfo->extraLength >= 0 && \
				  formatInfo->extraLength < MAX_INTLENGTH_SHORT ) ) );

	/* Make sure that the signing parameters are in order */
	status = krnlSendMessage( iSigCheckContext, IMESSAGE_GETATTRIBUTE,
							  &sigCheckAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );

	/* Check the start of the object and record the start and size of the
	   encapsulated signed object.  We have to use the long-length form of
	   the length functions to handle mega-CRLs */
	sMemConnect( &stream, signedObject, signedObjectLength );
	readLongSequence( &stream, NULL );
	status = getLongStreamObjectLength( &stream, &length );
	if( cryptStatusOK( status ) )
		status = sMemGetDataBlock( &stream, &objectPtr, length );
	if( cryptStatusOK( status ) )
		status = sSkip( &stream, length, SSKIP_MAX );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}

	/* If it's a broken signature, process the extra encapsulation */
	if( formatInfo != NULL )
		{
		if( formatInfo->isExplicit )
			{
			readConstructed( &stream, NULL, formatInfo->tag );
			status = readSequence( &stream, NULL );
			}
		else
			status = readConstructed( &stream, NULL, formatInfo->tag );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( status );
			}
		}

	/* Remember the location and size of the signature data */
	status = sMemGetDataBlockRemaining( &stream, &sigPtr, &sigLength );
	if( cryptStatusError( status ) )
		{
		sMemDisconnect( &stream );
		return( status );
		}
	status = readAlgoIDex( &stream, &signAlgo, &hashAlgo, &hashParam,
						   ALGOID_CLASS_PKCSIG );
	sMemDisconnect( &stream );
	if( cryptStatusError( status ) )
		return( status );
	ANALYSER_HINT( sigPtr != NULL );

	/* If the signature algorithm isn't what we expected the best that we 
	   can do is report a signature error */
	if( sigCheckAlgo != signAlgo )
		return( CRYPT_ERROR_SIGNATURE );

	/* Create a hash context from the algorithm identifier of the
	   signature */
	setMessageCreateObjectInfo( &createInfo, hashAlgo );
	status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
							  IMESSAGE_DEV_CREATEOBJECT, &createInfo,
							  OBJECT_TYPE_CONTEXT );
	if( cryptStatusError( status ) )
		return( status );
	iHashContext = createInfo.cryptHandle;
	if( hashParam != 0 )
		{
		/* Some hash algorithms have variable output size, in which case 
		   we need to explicitly tell the context which one we're working 
		   with */
		status = krnlSendMessage( iHashContext, IMESSAGE_SETATTRIBUTE,
								  &hashParam, CRYPT_CTXINFO_BLOCKSIZE );
		if( cryptStatusError( status ) )
			return( status );
		}

	/* Hash the signed data and check the signature on the object */
	status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
							  objectPtr, length );
	if( cryptStatusOK( status ) )
		status = krnlSendMessage( iHashContext, IMESSAGE_CTX_HASH, 
								  "", 0 );
	if( cryptStatusOK( status ) )
		{
		status = checkSignature( sigPtr, sigLength, iSigCheckContext,
								 iHashContext, CRYPT_UNUSED,
								 SIGNATURE_X509 );
		}
	krnlSendNotifier( iHashContext, IMESSAGE_DECREFCOUNT );

	return( status );
	}

/****************************************************************************
*																			*
*							PKI Protocol Signature Functions 				*
*																			*
****************************************************************************/

/* The various PKIX certificate management protocols are built using the 
   twin design guidelines that nothing should use a standard style of 
   signature and no two protocols should use the same nonstandard format, 
   the only way to handle these (without creating dozens of new signature 
   types, each with their own special-case handling) is to process most of 
   the signature information at the protocol level and just check the raw 
   signature here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int createRawSignature( OUT_BUFFER( sigMaxLength, *signatureLength ) \
							void *signature, 
						IN_LENGTH_SHORT_MIN( MIN_CRYPT_OBJECTSIZE ) \
							const int sigMaxLength, 
						OUT_LENGTH_BOUNDED_Z( sigMaxLength ) \
							int *signatureLength, 
						IN_HANDLE const CRYPT_CONTEXT iSignContext,
						IN_HANDLE const CRYPT_CONTEXT iHashContext )
	{
	assert( isWritePtr( signature, sigMaxLength ) );
	assert( isWritePtr( signatureLength, sizeof( int ) ) );

	REQUIRES( sigMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  sigMaxLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iSignContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );

	return( createSignature( signature, sigMaxLength, signatureLength, 
							 iSignContext, iHashContext, CRYPT_UNUSED,
							 SIGNATURE_RAW ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int checkRawSignature( IN_BUFFER( signatureLength ) const void *signature, 
					   IN_LENGTH_SHORT const int signatureLength,
					   IN_HANDLE const CRYPT_CONTEXT iSigCheckContext,
					   IN_HANDLE const CRYPT_CONTEXT iHashContext )
	{
	assert( isReadPtr( signature, signatureLength ) );

	REQUIRES( signatureLength > 0 && signatureLength < MAX_INTLENGTH_SHORT );
	REQUIRES( isHandleRangeValid( iSigCheckContext ) );
	REQUIRES( isHandleRangeValid( iHashContext ) );

	return( checkSignature( signature, signatureLength, iSigCheckContext,
							iHashContext, CRYPT_UNUSED, SIGNATURE_RAW ) );
	}
#endif /* USE_CERTIFICATES */
