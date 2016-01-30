/****************************************************************************
*																			*
*							Key Exchange Routines							*
*						Copyright Peter Gutmann 1993-2007					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "asn1.h"
  #include "asn1_ext.h"
  #include "misc_rw.h"
  #include "pgp_rw.h"
  #include "mech.h"
#else
  #include "crypt.h"
  #include "enc_dec/asn1.h"
  #include "enc_dec/asn1_ext.h"
  #include "enc_dec/misc_rw.h"
  #include "enc_dec/pgp_rw.h"
  #include "mechs/mech.h"
#endif /* Compiler-specific includes */

#ifdef USE_INT_CMS

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Try and determine the format of the encrypted data */

CHECK_RETVAL_ENUM( CRYPT_FORMAT ) STDC_NONNULL_ARG( ( 1 ) ) \
static CRYPT_FORMAT_TYPE getFormatType( IN_BUFFER( dataLength ) const void *data, 
										IN_DATALENGTH const int dataLength )
	{
	STREAM stream;
	long value;
	int status;

	assert( isReadPtr( data, dataLength ) );

	REQUIRES_EXT( dataLength > MIN_CRYPT_OBJECTSIZE && \
				  dataLength < MAX_BUFFER_SIZE, CRYPT_FORMAT_NONE );

	sMemConnect( &stream, data, min( 16, dataLength ) );

	/* Figure out what we've got.  PKCS #7/CMS/SMIME keyTrans begins:

		keyTransRecipientInfo ::= SEQUENCE {
			version		INTEGER (0|2),

	   while a kek begins:

		kekRecipientInfo ::= [3] IMPLICIT SEQUENCE {
			version		INTEGER (0),

	   which allows us to determine which type of object we have.  Note that 
	   we use sPeek() rather than peekTag() because we want to continue
	   processing (or at least checking for) PGP data if it's no ASN.1 */
	if( sPeek( &stream ) == BER_SEQUENCE )
		{
		CRYPT_FORMAT_TYPE formatType;

		readSequence( &stream, NULL );
		status = readShortInteger( &stream, &value );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( CRYPT_FORMAT_NONE );
			}
		switch( value )
			{
			case KEYTRANS_VERSION:
				formatType = CRYPT_FORMAT_CMS;
				break;

			case KEYTRANS_EX_VERSION:
				formatType = CRYPT_FORMAT_CRYPTLIB;
				break;

			default:
				formatType = CRYPT_FORMAT_NONE;
			}
		sMemDisconnect( &stream );

		return( formatType );
		}
	if( sPeek( &stream ) == MAKE_CTAG( CTAG_RI_PWRI ) )
		{
		readConstructed( &stream, NULL, CTAG_RI_PWRI );
		status = readShortInteger( &stream, &value );
		if( cryptStatusError( status ) )
			{
			sMemDisconnect( &stream );
			return( CRYPT_FORMAT_NONE );
			}
		sMemDisconnect( &stream );

		return( ( value == PWRI_VERSION ) ? \
				CRYPT_FORMAT_CRYPTLIB : CRYPT_FORMAT_NONE );
		}

#ifdef USE_PGP
	/* It's not ASN.1 data, check for PGP data */
	status = pgpReadPacketHeader( &stream, NULL, &value, 30, 8192 );
	if( cryptStatusOK( status ) && value > 30 && value < 8192 )
		{
		sMemDisconnect( &stream );
		return( CRYPT_FORMAT_PGP );
		}
#endif /* USE_PGP */

	sMemDisconnect( &stream );

	return( CRYPT_FORMAT_NONE );
	}

/* Check the key wrap key being used to import/export a session key */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
static int checkWrapKey( IN_HANDLE int importKey, 
						 OUT_ALGO_Z CRYPT_ALGO_TYPE *cryptAlgo,
						 const BOOLEAN isImport )
	{
	int localCryptAlgo, status;

	assert( isWritePtr( cryptAlgo, sizeof( CRYPT_ALGO_TYPE ) ) );

	REQUIRES( isHandleRangeValid( importKey ) );

	/* Clear return value */
	*cryptAlgo = CRYPT_ALGO_NONE;

	/* Make sure that the context is valid and get the algorithm being 
	   used */
	status = krnlSendMessage( importKey, MESSAGE_GETATTRIBUTE,
							  &localCryptAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( isPkcAlgo( localCryptAlgo ) )
		{
		/* The DLP algorithms have specialised data-formatting requirements
		   and can't normally be directly accessed via external messages,
		   and PKC operations in general may be restricted to internal access
		   only if they have certificates that restrict their use associated
		   with them.  However if we're performing a high-level key import 
		   (rather than a low-level raw context operation) this is OK since 
		   the low-level operation is being performed via these higher-level
		   routines which handle the formatting requirements.  Doing the 
		   check via an internal message is safe at this point since we've 
		   already checked the context's external accessibility when we got 
		   the algorithm info */
		status = krnlSendMessage( importKey, IMESSAGE_CHECK, NULL,
								  isImport ? MESSAGE_CHECK_PKC_DECRYPT : \
											 MESSAGE_CHECK_PKC_ENCRYPT );
		}
	else
		{
		status = krnlSendMessage( importKey, MESSAGE_CHECK, NULL,
								  MESSAGE_CHECK_CRYPT );
		}
	if( cryptStatusError( status ) )
		return( status );

	*cryptAlgo = localCryptAlgo;
	return( CRYPT_OK );
	}

/* Check that the context data is encodable using the chosen format */

CHECK_RETVAL \
static int checkContextsEncodable( IN_HANDLE const CRYPT_HANDLE exportKey,
								   IN_ALGO const CRYPT_ALGO_TYPE exportAlgo,
								   IN_HANDLE const CRYPT_CONTEXT sessionKeyContext,
								   IN_ENUM( CRYPT_FORMAT ) \
									const CRYPT_FORMAT_TYPE formatType )
	{
	const BOOLEAN exportIsPKC = isPkcAlgo( exportAlgo ) ? TRUE : FALSE;
	BOOLEAN sessionIsMAC = FALSE;
	int sessionKeyAlgo, sessionKeyMode DUMMY_INIT, status;

	REQUIRES( isHandleRangeValid( exportKey ) );
	REQUIRES( exportAlgo > CRYPT_ALGO_NONE && exportAlgo < CRYPT_ALGO_LAST );
	REQUIRES( isHandleRangeValid( sessionKeyContext ) );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST_EXTERNAL );

	/* Get any required context information */
	status = krnlSendMessage( sessionKeyContext, MESSAGE_GETATTRIBUTE,
							  &sessionKeyAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM3 );
	if( isMacAlgo( sessionKeyAlgo ) )
		sessionIsMAC = TRUE;
	else
		{
		status = krnlSendMessage( sessionKeyContext, MESSAGE_GETATTRIBUTE,
								  &sessionKeyMode, CRYPT_CTXINFO_MODE );
		if( cryptStatusError( status ) )
			return( CRYPT_ERROR_PARAM3 );
		}

	switch( formatType )
		{
		case CRYPT_FORMAT_CRYPTLIB:
		case CRYPT_FORMAT_CMS:
		case CRYPT_FORMAT_SMIME:
			/* Check that the export algorithm is encodable */
			if( exportIsPKC )
				{
				if( cryptStatusError( sizeofAlgoID( exportAlgo ) ) )
					return( CRYPT_ERROR_PARAM1 );
				}
			else
				{
				int exportMode;	/* int vs.enum */

				/* If it's a conventional key export, the key wrap mechanism 
				   requires the use of CBC mode for the wrapping */
				status = krnlSendMessage( exportKey, MESSAGE_GETATTRIBUTE, 
										  &exportMode, CRYPT_CTXINFO_MODE );
				if( cryptStatusError( status ) )
					return( CRYPT_ERROR_PARAM1 );
				if( exportMode != CRYPT_MODE_CBC || \
					cryptStatusError( \
						sizeofAlgoIDex( exportAlgo, exportMode, 0 ) ) )
					return( CRYPT_ERROR_PARAM1 );
				}

			/* Check that the session-key algorithm is encodable */
			if( sessionIsMAC )
				status = sizeofAlgoID( sessionKeyAlgo );
			else
				status = checkAlgoID( sessionKeyAlgo, sessionKeyMode );
			if( cryptStatusError( status ) )
				return( CRYPT_ERROR_PARAM3 );

			return( CRYPT_OK );

#ifdef USE_PGP
		case CRYPT_FORMAT_PGP:
			{
			int dummy;

			/* Check that the export algorithm is encodable */
			if( cryptStatusError( \
					cryptlibToPgpAlgo( exportAlgo, &dummy ) ) )
				return( CRYPT_ERROR_PARAM1 );

			/* Check that the session-key algorithm is encodable */
			if( exportIsPKC )
				{
				if( cryptStatusError( \
						cryptlibToPgpAlgo( sessionKeyAlgo, &dummy ) ) )
					return( CRYPT_ERROR_PARAM3 );
				if( sessionKeyMode != CRYPT_MODE_CFB )
					return( CRYPT_ERROR_PARAM3 );
				}
			else
				{
				int exportMode;	/* int vs.enum */

				/* If it's a conventional key export there's no key wrap as 
				   in CMS (the session-key context isn't used), so the 
				   "export context" mode must be CFB */
				status = krnlSendMessage( exportKey, MESSAGE_GETATTRIBUTE, 
										  &exportMode, CRYPT_CTXINFO_MODE );
				if( cryptStatusError( status ) || \
					exportMode != CRYPT_MODE_CFB )
					return( CRYPT_ERROR_PARAM1 );
				}

			return( CRYPT_OK );
			}
#endif /* USE_PGP */
		}
	
	/* It's an invalid/unknown format, we can't check the encodability of 
	   the context data */
	return( CRYPT_ERROR_PARAM4 );
	}

/****************************************************************************
*																			*
*								Import a Session Key						*
*																			*
****************************************************************************/

/* Import an extended encrypted key, either a cryptlib key or a CMS key */

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptImportKeyEx( C_IN void C_PTR encryptedKey,
						C_IN int encryptedKeyLength,
						C_IN CRYPT_CONTEXT importKey,
						C_IN CRYPT_CONTEXT sessionKeyContext,
						C_OUT_OPT CRYPT_CONTEXT C_PTR returnedContext )
	{
	CRYPT_CONTEXT iReturnedContext DUMMY_INIT;
	CRYPT_FORMAT_TYPE formatType;
	CRYPT_ALGO_TYPE importAlgo;
	int owner, originalOwner, status;

	/* Perform basic error checking */
	if( encryptedKeyLength <= MIN_CRYPT_OBJECTSIZE || \
		encryptedKeyLength >= MAX_INTLENGTH_SHORT )
		return( CRYPT_ERROR_PARAM2 );
	if( !isReadPtr( encryptedKey, encryptedKeyLength ) )
		return( CRYPT_ERROR_PARAM1 );
	if( ( formatType = \
			getFormatType( encryptedKey, encryptedKeyLength ) ) == CRYPT_FORMAT_NONE )
		return( CRYPT_ERROR_BADDATA );
	if( !isHandleRangeValid( importKey ) )
		return( CRYPT_ERROR_PARAM3 );
	if( sessionKeyContext != CRYPT_UNUSED && \
		!isHandleRangeValid( sessionKeyContext ) )
		return( CRYPT_ERROR_PARAM4 );

	/* Check the importing key */
	status = checkWrapKey( importKey, &importAlgo, TRUE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ERROR_PARAM3 : status );

	/* Check the session key */
	if( formatType == CRYPT_FORMAT_PGP )
		{
		/* PGP stores the session key information with the encrypted key
		   data, so the user can't provide a context */
		if( sessionKeyContext != CRYPT_UNUSED )
			return( CRYPT_ERROR_PARAM4 );
		if( !isWritePtrConst( returnedContext, sizeof( CRYPT_CONTEXT ) ) )
			return( CRYPT_ERROR_PARAM5 );
		*returnedContext = CRYPT_ERROR;
		}
	else
		{
		int sessionKeyAlgo;	/* int vs.enum */

		if( !isHandleRangeValid( sessionKeyContext ) )
			return( CRYPT_ERROR_PARAM4 );
		status = krnlSendMessage( sessionKeyContext, MESSAGE_GETATTRIBUTE,
								  &sessionKeyAlgo, CRYPT_CTXINFO_ALGO );
		if( cryptStatusOK( status ) )
			{
			status = krnlSendMessage( sessionKeyContext, MESSAGE_CHECK, NULL,
									  isMacAlgo( sessionKeyAlgo ) ? \
										MESSAGE_CHECK_MAC_READY : \
										MESSAGE_CHECK_CRYPT_READY );
			}
		if( cryptStatusError( status ) )
			return( cryptArgError( status ) ? CRYPT_ERROR_PARAM4 : status );
		if( returnedContext != NULL )
			return( CRYPT_ERROR_PARAM5 );
		}

	/* If the importing key is owned, bind the session key context to the same
	   owner before we load a key into it.  We also need to save the original
	   owner so that we can undo the binding later if things fail */
	status = krnlSendMessage( sessionKeyContext, MESSAGE_GETATTRIBUTE,
							  &originalOwner, CRYPT_PROPERTY_OWNER );
	if( cryptStatusError( status ) )
		originalOwner = CRYPT_ERROR;	/* Non-owned object */
	status = krnlSendMessage( importKey, MESSAGE_GETATTRIBUTE, &owner,
							  CRYPT_PROPERTY_OWNER );
	if( cryptStatusOK( status ) )
		{
		/* The importing key is owned, set the imported key's owner if it's
		   present */
		if( sessionKeyContext != CRYPT_UNUSED )
			{
			status = krnlSendMessage( sessionKeyContext, MESSAGE_SETATTRIBUTE, 
									  &owner, CRYPT_PROPERTY_OWNER );
			if( cryptStatusError( status ) )
				return( status );
			}
		}
	else
		{
		/* Don't try and change the session key ownership */
		originalOwner = CRYPT_ERROR;
		}

	/* Import it as appropriate */
	status = iCryptImportKey( encryptedKey, encryptedKeyLength, formatType,
							  importKey, sessionKeyContext, 
							  ( formatType == CRYPT_FORMAT_PGP ) ? \
								&iReturnedContext : NULL );
	if( cryptStatusError( status ) )
		{
		/* The import failed, return the session key context to its
		   original owner.  If this fails there's not much that we can do
		   to recover so we don't do anything with the return value */
		if( originalOwner != CRYPT_ERROR )
			{
			( void ) krnlSendMessage( sessionKeyContext, 
									  MESSAGE_SETATTRIBUTE,
									  &originalOwner, CRYPT_PROPERTY_OWNER );
			}
		if( cryptArgError( status ) )
			{
			/* If we get an argument error from the lower-level code, map the
			   parameter number to the function argument number */
			status = ( status == CRYPT_ARGERROR_NUM1 ) ? \
					 CRYPT_ERROR_PARAM4 : CRYPT_ERROR_PARAM3;
			}
		return( status );
		}

#ifdef USE_PGP
	/* If it's a PGP key import then the session key was recreated from 
	   information stored with the wrapped key so we have to make it 
	   externally visible before it can be used by the caller */
	if( formatType == CRYPT_FORMAT_PGP && isPkcAlgo( importAlgo ) )
		{
		/* If the importing key is owned, set the imported key's owner */
		if( originalOwner != CRYPT_ERROR )
			{
			status = krnlSendMessage( iReturnedContext, 
									  IMESSAGE_SETATTRIBUTE, 
									  &owner, CRYPT_PROPERTY_OWNER );
			if( cryptStatusError( status ) )
				{
				krnlSendNotifier( iReturnedContext, IMESSAGE_DECREFCOUNT );
				return( status );
				}
			}

		/* Make the newly-created context externally visible */
		status = krnlSendMessage( iReturnedContext, IMESSAGE_SETATTRIBUTE, 
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_INTERNAL );
		if( cryptStatusError( status ) )
			{
			krnlSendNotifier( iReturnedContext, IMESSAGE_DECREFCOUNT );
			return( status );
			}
		*returnedContext = iReturnedContext;
		}
#endif /* USE_PGP */
	
	return( CRYPT_OK );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) \
C_RET cryptImportKey( C_IN void C_PTR encryptedKey,
					  C_IN int encryptedKeyLength,
					  C_IN CRYPT_CONTEXT importKey,
					  C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	return( cryptImportKeyEx( encryptedKey, encryptedKeyLength, importKey,
							  sessionKeyContext, NULL ) );
	}

/****************************************************************************
*																			*
*								Export a Session Key						*
*																			*
****************************************************************************/

/* Export an extended encrypted key, either a cryptlib key or a CMS key */

C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) \
C_RET cryptExportKeyEx( C_OUT_OPT void C_PTR encryptedKey,
						C_IN int encryptedKeyMaxLength,
						C_OUT int C_PTR encryptedKeyLength,
						C_IN CRYPT_FORMAT_TYPE formatType,
						C_IN CRYPT_HANDLE exportKey,
						C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	CRYPT_ALGO_TYPE exportAlgo;
	int sessionKeyAlgo, status;	/* int vs.enum */

	/* Perform basic error checking */
	if( encryptedKey != NULL )
		{
		if( encryptedKeyMaxLength <= MIN_CRYPT_OBJECTSIZE || \
			encryptedKeyMaxLength >= MAX_BUFFER_SIZE )
			return( CRYPT_ERROR_PARAM2 );
		if( !isWritePtr( encryptedKey, encryptedKeyMaxLength ) )
			return( CRYPT_ERROR_PARAM1 );
		memset( encryptedKey, 0, MIN_CRYPT_OBJECTSIZE );
		}
	else
		{
		if( encryptedKeyMaxLength != 0 )
			return( CRYPT_ERROR_PARAM2 );
		}
	if( !isWritePtrConst( encryptedKeyLength, sizeof( int ) ) )
		return( CRYPT_ERROR_PARAM3 );
	*encryptedKeyLength = 0;
	if( formatType != CRYPT_FORMAT_CRYPTLIB && \
		formatType != CRYPT_FORMAT_CMS && \
		formatType != CRYPT_FORMAT_SMIME && \
		formatType != CRYPT_FORMAT_PGP )
		return( CRYPT_ERROR_PARAM4 );
	if( !isHandleRangeValid( exportKey ) )
		return( CRYPT_ERROR_PARAM5 );
	if( !isHandleRangeValid( sessionKeyContext ) )
		return( CRYPT_ERROR_PARAM6 );

	/* Check the exporting key */
	status = checkWrapKey( exportKey, &exportAlgo, FALSE );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ERROR_PARAM5 : status );
	status = checkContextsEncodable( exportKey, exportAlgo, 
									 sessionKeyContext, formatType );
	if( cryptStatusError( status ) )
		{
		return( ( status == CRYPT_ERROR_PARAM1 ) ? CRYPT_ERROR_PARAM5 : \
				( status == CRYPT_ERROR_PARAM3 ) ? CRYPT_ERROR_PARAM6 : \
				CRYPT_ERROR_PARAM4 );
		}

	/* Check the exported key */
	status = krnlSendMessage( sessionKeyContext, MESSAGE_GETATTRIBUTE,
							  &sessionKeyAlgo, CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM6 );
	status = krnlSendMessage( sessionKeyContext, MESSAGE_CHECK, NULL,
							  isMacAlgo( sessionKeyAlgo ) ? \
								MESSAGE_CHECK_MAC : MESSAGE_CHECK_CRYPT );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ERROR_PARAM6 : status );

	/* Export the key via the shared export function */
	status = iCryptExportKey( encryptedKey, encryptedKeyMaxLength, 
							  encryptedKeyLength, formatType,
							  sessionKeyContext, exportKey );
	if( cryptArgError( status ) )
		{
		/* If we get an argument error from the lower-level code, map the
		   parameter number to the function argument number */
		status = ( status == CRYPT_ARGERROR_NUM1 ) ? \
				 CRYPT_ERROR_PARAM6 : CRYPT_ERROR_PARAM5;
		}
	return( status );
	}

C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) \
C_RET cryptExportKey( C_OUT_OPT void C_PTR encryptedKey,
					  C_IN int encryptedKeyMaxLength,
					  C_OUT int C_PTR encryptedKeyLength,
					  C_IN CRYPT_HANDLE exportKey,
					  C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	int status;

	status = cryptExportKeyEx( encryptedKey, encryptedKeyMaxLength,
							   encryptedKeyLength, CRYPT_FORMAT_CRYPTLIB,
							   exportKey, sessionKeyContext );
	return( ( status == CRYPT_ERROR_PARAM5 ) ? CRYPT_ERROR_PARAM4 : \
			( status == CRYPT_ERROR_PARAM6 ) ? CRYPT_ERROR_PARAM5 : status );
	}

/****************************************************************************
*																			*
*						Internal Import/Export Functions					*
*																			*
****************************************************************************/

/* Internal versions of the above.  These skip a lot of the explicit 
   checking done by the external versions (e.g. "Is this value really a 
   handle to a valid PKC context?") since they're only called by cryptlib 
   internal functions rather than being passed untrusted user data */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int iCryptImportKey( IN_BUFFER( encryptedKeyLength ) const void *encryptedKey, 
					 IN_LENGTH_SHORT const int encryptedKeyLength,
					 IN_ENUM( CRYPT_FORMAT ) const CRYPT_FORMAT_TYPE formatType,
					 IN_HANDLE const CRYPT_CONTEXT iImportKey,
					 IN_HANDLE_OPT const CRYPT_CONTEXT iSessionKeyContext,
					 OUT_OPT_HANDLE_OPT CRYPT_CONTEXT *iReturnedContext )
	{
	const KEYEX_TYPE keyexType = \
			( formatType == CRYPT_FORMAT_AUTO || \
			  formatType == CRYPT_FORMAT_CRYPTLIB ) ? KEYEX_CRYPTLIB : \
			( formatType == CRYPT_FORMAT_PGP ) ? KEYEX_PGP : KEYEX_CMS;
	int importAlgo,	status;	/* int vs.enum */

	assert( isReadPtr( encryptedKey, encryptedKeyLength ) );
	assert( ( formatType == CRYPT_FORMAT_PGP && \
			  isWritePtr( iReturnedContext, sizeof( CRYPT_CONTEXT ) ) ) || \
			( formatType != CRYPT_FORMAT_PGP && \
			  iReturnedContext == NULL ) );

	REQUIRES( encryptedKeyLength > MIN_CRYPT_OBJECTSIZE && \
			  encryptedKeyLength < MAX_INTLENGTH_SHORT );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST );
	REQUIRES( isHandleRangeValid( iImportKey ) );
	REQUIRES( ( formatType == CRYPT_FORMAT_PGP && \
				iSessionKeyContext == CRYPT_UNUSED ) || \
			  ( formatType != CRYPT_FORMAT_PGP && \
				isHandleRangeValid( iSessionKeyContext ) ) );
	REQUIRES( ( formatType == CRYPT_FORMAT_PGP && \
				iReturnedContext != NULL ) || \
			  ( formatType != CRYPT_FORMAT_PGP && \
				iReturnedContext == NULL ) );

	/* Clear return values */
	if( iReturnedContext != NULL )
		*iReturnedContext = CRYPT_ERROR;

	/* Import it as appropriate.  We don't handle key agreement at this
	   level since it's a protocol-specific mechanism used by SSH and SSL,
	   which are internal-only formats */
	status = krnlSendMessage( iImportKey, IMESSAGE_GETATTRIBUTE, &importAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( status );
	if( isConvAlgo( importAlgo ) )
		{
		if( !isHandleRangeValid( iSessionKeyContext ) )
			return( CRYPT_ARGERROR_NUM2 );
		return( importConventionalKey( encryptedKey, encryptedKeyLength,
									   iSessionKeyContext, iImportKey,
									   keyexType ) );
		}
	return( importPublicKey( encryptedKey, encryptedKeyLength,
							 iSessionKeyContext, iImportKey,
							 iReturnedContext, keyexType ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int iCryptExportKey( OUT_BUFFER_OPT( encryptedKeyMaxLength, *encryptedKeyLength ) \
						void *encryptedKey, 
					 IN_DATALENGTH_Z const int encryptedKeyMaxLength,
					 OUT_DATALENGTH_Z int *encryptedKeyLength,
					 IN_ENUM( CRYPT_FORMAT ) const CRYPT_FORMAT_TYPE formatType,
					 IN_HANDLE_OPT const CRYPT_CONTEXT iSessionKeyContext,
					 IN_HANDLE const CRYPT_CONTEXT iExportKey )
	{
	const KEYEX_TYPE keyexType = \
			( formatType == CRYPT_FORMAT_CRYPTLIB ) ? KEYEX_CRYPTLIB : \
			( formatType == CRYPT_FORMAT_PGP ) ? KEYEX_PGP : KEYEX_CMS;
	DYNBUF auxDB;
	const int encKeyMaxLength = ( encryptedKey == NULL ) ? \
								0 : encryptedKeyMaxLength;
	int exportAlgo, status;	/* int vs.enum */

	assert( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			( encryptedKeyMaxLength >= MIN_CRYPT_OBJECTSIZE && \
			  isWritePtr( encryptedKey, encryptedKeyMaxLength ) ) );
	assert( isWritePtr( encryptedKeyLength, sizeof( int ) ) );

	REQUIRES( ( encryptedKey == NULL && encryptedKeyMaxLength == 0 ) || \
			  ( encryptedKeyMaxLength > MIN_CRYPT_OBJECTSIZE && \
				encryptedKeyMaxLength < MAX_BUFFER_SIZE ) );
	REQUIRES( formatType > CRYPT_FORMAT_NONE && \
			  formatType < CRYPT_FORMAT_LAST );
	REQUIRES( ( formatType == CRYPT_FORMAT_PGP && \
				iSessionKeyContext == CRYPT_UNUSED ) || \
			  isHandleRangeValid( iSessionKeyContext ) );
	REQUIRES( isHandleRangeValid( iExportKey ) );

	ANALYSER_HINT( encryptedKeyLength != NULL );

	/* Clear return value */
	*encryptedKeyLength = 0;

	/* Perform simplified error checking */
	status = krnlSendMessage( iExportKey, IMESSAGE_GETATTRIBUTE, &exportAlgo,
							  CRYPT_CTXINFO_ALGO );
	if( cryptStatusError( status ) )
		return( cryptArgError( status ) ? CRYPT_ARGERROR_NUM2 : status );

	/* If it's a non-PKC export, pass the call down to the low-level export
	   function */
	if( !isPkcAlgo( exportAlgo ) )
		{
		return( exportConventionalKey( encryptedKey, encKeyMaxLength, 
									   encryptedKeyLength, iSessionKeyContext, 
									   iExportKey, keyexType ) );
		}

	REQUIRES( isHandleRangeValid( iSessionKeyContext ) );

	/* If it's a non-CMS/SMIME PKC export, pass the call down to the low-
	   level export function */
	if( formatType != CRYPT_FORMAT_CMS && formatType != CRYPT_FORMAT_SMIME )
		{
		return( exportPublicKey( encryptedKey, encKeyMaxLength, 
								 encryptedKeyLength, iSessionKeyContext, 
								 iExportKey, NULL, 0, keyexType ) );
		}

	/* We're exporting a key in CMS format we need to obtain recipient 
	   information from the certificate associated with the export context.
	   First we lock the certificate for our exclusive use and in case it's 
	   a certificate chain select the first certificate in the chain */
	status = krnlSendMessage( iExportKey, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_TRUE, CRYPT_IATTRIBUTE_LOCKED );
	if( cryptStatusError( status ) )
		return( CRYPT_ERROR_PARAM5 );
	status = krnlSendMessage( iExportKey, IMESSAGE_SETATTRIBUTE,
							  MESSAGE_VALUE_CURSORFIRST, 
							  CRYPT_CERTINFO_CURRENT_CERTIFICATE );
	if( cryptStatusError( status ) )
		{
		/* Unlock the chain before we exit.  If this fails there's not much 
		   that we can do to recover so we don't do anything with the return 
		   value */
		( void ) krnlSendMessage( iExportKey, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( CRYPT_ERROR_PARAM5 );
		}

	/* Next we get the recipient information from the certificate into a 
	   dynbuf */
	status = dynCreate( &auxDB, iExportKey,
						CRYPT_IATTRIBUTE_ISSUERANDSERIALNUMBER );
	if( cryptStatusError( status ) )
		{
		( void ) krnlSendMessage( iExportKey, IMESSAGE_SETATTRIBUTE,
								  MESSAGE_VALUE_FALSE, 
								  CRYPT_IATTRIBUTE_LOCKED );
		return( CRYPT_ERROR_PARAM5 );
		}

	/* We're ready to export the key alongside the key ID as auxiliary 
	   data */
	status = exportPublicKey( encryptedKey, encKeyMaxLength, 
							  encryptedKeyLength, iSessionKeyContext, 
							  iExportKey, dynData( auxDB ), 
							  dynLength( auxDB ), keyexType );

	/* Clean up.  If the unlock fails there's not much that we can do to 
	   recover so we don't do anything with the return value */
	( void ) krnlSendMessage( iExportKey, IMESSAGE_SETATTRIBUTE, 
							  MESSAGE_VALUE_FALSE, CRYPT_IATTRIBUTE_LOCKED );
	dynDestroy( &auxDB );

	return( status );
	}

#else

/****************************************************************************
*																			*
*						Stub Functions for non-CMS/PGP Use					*
*																			*
****************************************************************************/

C_RET cryptImportKeyEx( C_IN void C_PTR encryptedKey,
						C_IN int encryptedKeyLength,
						C_IN CRYPT_CONTEXT importKey,
						C_IN CRYPT_CONTEXT sessionKeyContext,
						C_OUT CRYPT_CONTEXT C_PTR returnedContext )
	{
	UNUSED_ARG( encryptedKey );
	UNUSED_ARG( returnedContext );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptImportKey( C_IN void C_PTR encryptedKey,
					  C_IN int encryptedKeyLength,
					  C_IN CRYPT_CONTEXT importKey,
					  C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	UNUSED_ARG( encryptedKey );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptExportKeyEx( C_OUT_OPT void C_PTR encryptedKey,
						C_IN int encryptedKeyMaxLength,
						C_OUT int C_PTR encryptedKeyLength,
						C_IN CRYPT_FORMAT_TYPE formatType,
						C_IN CRYPT_HANDLE exportKey,
						C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	UNUSED_ARG( encryptedKey );
	UNUSED_ARG( encryptedKeyLength );

	return( CRYPT_ERROR_NOTAVAIL );
	}

C_RET cryptExportKey( C_OUT_OPT void C_PTR encryptedKey,
					  C_IN int encryptedKeyMaxLength,
					  C_OUT int C_PTR encryptedKeyLength,
					  C_IN CRYPT_HANDLE exportKey,
					  C_IN CRYPT_CONTEXT sessionKeyContext )
	{
	UNUSED_ARG( encryptedKey );
	UNUSED_ARG( encryptedKeyLength );

	return( CRYPT_ERROR_NOTAVAIL );
	}
#endif /* USE_INT_CMS */
