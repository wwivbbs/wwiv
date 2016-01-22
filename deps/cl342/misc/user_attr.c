/****************************************************************************
*																			*
*						cryptlib User Attribute Routines					*
*						Copyright Peter Gutmann 1999-2007					*
*																			*
****************************************************************************/

#include <stdio.h>		/* For snprintf_s() */
#include "crypt.h"
#ifdef INC_ALL
  #include "trustmgr.h"
  #include "user.h"
#else
  #include "cert/trustmgr.h"
  #include "misc/user.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Exit after setting extended error information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitError( INOUT USER_INFO *userInfoPtr,
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus,
					  IN_ENUM( CRYPT_ERRTYPE ) const CRYPT_ERRTYPE_TYPE errorType, 
					  IN_ERROR const int status )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );
	REQUIRES( errorType > CRYPT_ERRTYPE_NONE && \
			  errorType < CRYPT_ERRTYPE_LAST );
	REQUIRES( cryptStatusError( status ) );

	setErrorInfo( userInfoPtr, errorLocus, errorType );
	return( status );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorInited( INOUT USER_INFO *userInfoPtr,
							IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( userInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_PRESENT, 
					   CRYPT_ERROR_INITED ) );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int exitErrorNotFound( INOUT USER_INFO *userInfoPtr,
							  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE errorLocus )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( isAttribute( errorLocus ) || \
			  isInternalAttribute( errorLocus ) );

	return( exitError( userInfoPtr, errorLocus, CRYPT_ERRTYPE_ATTR_ABSENT, 
					   CRYPT_ERROR_NOTFOUND ) );
	}

/* Process a set-attribute operation that initiates an operation that's 
   performed in two phases.  The reason for the split is that the second 
   phase doesn't require the use of the user object data any more and can be 
   a somewhat lengthy process due to disk accesses or lengthy crypto 
   operations.  Because of this we unlock the user object between the two 
   phases to ensure that the second phase doesn't stall all other operations 
   that require this user object */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int twoPhaseConfigUpdate( INOUT USER_INFO *userInfoPtr, 
								 IN_INT_Z const int value )
	{
	CRYPT_USER iTrustedCertUserObject = CRYPT_UNUSED;
	CONFIG_DISPOSITION_TYPE disposition;
	char userFileName[ 16 + 8 ];
	void *data = NULL;
	int length = 0, commitStatus, refCount, status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( value == FALSE );

	/* Set up the filename that we want to access */
	if( userInfoPtr->userFileInfo.fileRef == CRYPT_UNUSED )
		strlcpy_s( userFileName, 16, "cryptlib" );
	else
		{
		sprintf_s( userFileName, 16, "u%06x", 
				   userInfoPtr->userFileInfo.fileRef );
		}

	/* Determine what we have to do with the configuation information */
	status = getConfigDisposition( userInfoPtr->configOptions, 
								   userInfoPtr->configOptionsCount, 
								   userInfoPtr->trustInfoPtr, 
								   &disposition );
	if( cryptStatusError( status ) )
		return( status );
	switch( disposition )
		{
		case CONFIG_DISPOSITION_NO_CHANGE:
			/* There's nothing to do, we're done */
			return( CRYPT_OK );

		case CONFIG_DISPOSITION_EMPTY_CONFIG_FILE:
			/* There's nothing to write, delete the configuration file */
			return( deleteConfig( userFileName ) );

		case CONFIG_DISPOSITION_TRUSTED_CERTS_ONLY:
			/* There are only trusted certificates present, if they're 
			   unchanged from earlier then there's nothing to do */
			if( !userInfoPtr->trustInfoChanged )
				return( CRYPT_OK );

			/* Remember where the trusted certificates are coming from */
			iTrustedCertUserObject = userInfoPtr->objectHandle;
			break;

		case CONFIG_DISPOSITION_DATA_ONLY:
		case CONFIG_DISPOSITION_BOTH:
			/* There's configuration data to write, prepare it */
			status = prepareConfigData( userInfoPtr->configOptions, 
										userInfoPtr->configOptionsCount, 
										&data, &length );
			if( cryptStatusError( status ) )
				return( status );
			if( disposition == CONFIG_DISPOSITION_BOTH )
				{
				/* There are certificates present as well, remember where 
				   they're coming from */
				iTrustedCertUserObject = userInfoPtr->objectHandle;
				}
			break;

		default:
			retIntError();
		}

	/* We've got the configuration data (if there is any) in a memory 
	   buffer, we can unlock the user object to allow external access while 
	   we commit the in-memory data to disk.  This also sends any trusted
	   certificates in the user object to the configuration file alongside
	   the data */
	status = krnlSuspendObject( userInfoPtr->objectHandle, &refCount );
	ENSURES( cryptStatusOK( status ) );
	commitStatus = commitConfigData( userFileName, data, length, 
									 iTrustedCertUserObject );
	if( disposition == CONFIG_DISPOSITION_DATA_ONLY || \
		disposition == CONFIG_DISPOSITION_BOTH )
		clFree( "userMessageFunction", data );
	status = krnlResumeObject( userInfoPtr->objectHandle, refCount );
	if( cryptStatusError( status ) )
		{
		/* Handling errors at this point is rather tricky because an error 
		   response from krnlResumeObject() is a can't-occur condition.  In 
		   particular this will mean that we return to the kernel with the 
		   user object released, which will cause it to throw an exception 
		   due to the inconsistent object state.  On the other hand we can 
		   only get to this point because of an exception condition anyway 
		   so it's just going to propagate the exception back */
		retIntError();
		}
	if( cryptStatusOK( commitStatus ) )
		userInfoPtr->trustInfoChanged = FALSE;

	return( commitStatus );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
static int twoPhaseSelftest( INOUT USER_INFO *userInfoPtr, 
							 IN_INT_Z const int value )
	{
	const CRYPT_USER iCryptUser = userInfoPtr->objectHandle;
	int refCount, selfTestStatus, status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( value == TRUE );

	/* It's a self-test, forward the message to the system object with 
	   the user object unlocked, tell the system object to perform its self-
	   test, and then re-lock the user object and set the self-test result 
	   value.  Since the self-test configuration setting will be marked as 
	   in-use at this point (to avoid having another thread update it while 
	   the user object was unlocked) it can't be written to directly so we 
	   have to update it via setOptionSpecial().  In addition since this is 
	   now an action message we forward it as a MESSAGE_SELFTEST since the 
	   CRYPT_OPTION_SELFTESTOK attribute only applies to the user object.
	   
	   An alternative way to handle this would be implement an advisory-
	   locking mechanism for configuration options, but this adds a great 
	   deal of complexity just to handle this one single case so until 
	   there's a wider need for general-purpose configuration option locking 
	   the current approach will do */
	status = krnlSuspendObject( iCryptUser, &refCount );
	ENSURES( cryptStatusOK( status ) );
	selfTestStatus = krnlSendNotifier( SYSTEM_OBJECT_HANDLE, 
									   IMESSAGE_SELFTEST );
	status = krnlResumeObject( iCryptUser, refCount );
			 /* See comment above on krnlResumeObject() error handling */
	if( cryptStatusError( status ) )
		return( status );
	return( setOptionSpecial( userInfoPtr->configOptions, 
							  userInfoPtr->configOptionsCount,
							  CRYPT_OPTION_SELFTESTOK,
							  cryptStatusOK( selfTestStatus ) ? \
								TRUE : FALSE ) );
	}

/****************************************************************************
*																			*
*								Get Attributes								*
*																			*
****************************************************************************/

/* Get a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getUserAttribute( INOUT USER_INFO *userInfoPtr,
					  OUT_INT_Z int *valuePtr, 
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	int status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isWritePtr( valuePtr, sizeof( int ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* Clear return value */
	*valuePtr = 0;

	switch( attribute )
		{
		case CRYPT_USERINFO_CAKEY_CERTSIGN:
		case CRYPT_USERINFO_CAKEY_CRLSIGN:
		case CRYPT_USERINFO_CAKEY_OCSPSIGN:
			{
			CRYPT_CERTIFICATE caCert;

			/* Make sure that the key type that we're after is present in 
			   the object */
			if( userInfoPtr->iCryptContext == CRYPT_UNUSED )
				return( exitErrorNotFound( userInfoPtr, attribute ) );

			/* Since the CA signing key tied to the user object is meant to 
			   be used only through cryptlib-internal means we shouldn't 
			   really be returning it to the caller.  We can return the 
			   ssociated CA cert, but this may be an internal-only object 
			   that the caller can't do anything with.  To avoid this 
			   problem we isolate the cert by returning a copy of the
			   associated certificate object */
			status = krnlSendMessage( userInfoPtr->iCryptContext, 
									  IMESSAGE_GETATTRIBUTE, &caCert,
									  CRYPT_IATTRIBUTE_CERTCOPY );
			if( cryptStatusOK( status ) )
				*valuePtr = caCert;
			return( status );
			}

#ifdef USE_CERTIFICATES
		case CRYPT_IATTRIBUTE_CTL:
			{
			MESSAGE_CREATEOBJECT_INFO createInfo;

			/* Check whether there are actually trusted certs present */
			if( !trustedCertsPresent( userInfoPtr->trustInfoPtr ) )
				return( CRYPT_ERROR_NOTFOUND );

			/* Create a cert chain meta-object to hold the overall set of
			   certs */
			setMessageCreateObjectInfo( &createInfo,
										CRYPT_CERTTYPE_CERTCHAIN );
			status = krnlSendMessage( SYSTEM_OBJECT_HANDLE,
									  IMESSAGE_DEV_CREATEOBJECT,
									  &createInfo, OBJECT_TYPE_CERTIFICATE );
			if( cryptStatusError( status ) )
				return( status );

			/* Assemble the trusted certs into the cert chain */
			status = enumTrustedCerts( userInfoPtr->trustInfoPtr,
									   createInfo.cryptHandle, CRYPT_UNUSED );
			if( cryptStatusOK( status ) )
				*valuePtr = createInfo.cryptHandle;
			else
				krnlSendNotifier( createInfo.cryptHandle, IMESSAGE_DECREFCOUNT );
			return( status );
			}
#endif /* USE_CERTIFICATES */
		}

	/* Anything else has to be a configuration option */
	REQUIRES( attribute > CRYPT_OPTION_FIRST && \
			  attribute < CRYPT_OPTION_LAST );

	/* A numeric-value get can never fail because we always have default 
	   values present */
	return( getOption( userInfoPtr->configOptions, 
					   userInfoPtr->configOptionsCount, attribute, 
					   valuePtr ) );
	}

/* Get a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int getUserAttributeS( INOUT USER_INFO *userInfoPtr,
					   INOUT MESSAGE_DATA *msgData, 
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	const void *string;
	int stringLen, status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isWritePtr( msgData, sizeof( MESSAGE_DATA ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	/* This can only be a configuration option */
	REQUIRES( attribute > CRYPT_OPTION_FIRST && \
			  attribute < CRYPT_OPTION_LAST );

	/* Check whether there's a configuration value of this type present */
	status = getOptionString( userInfoPtr->configOptions, 
							  userInfoPtr->configOptionsCount, attribute, 
							  &string, &stringLen );
	if( cryptStatusError( status ) )
		return( status );
	return( attributeCopy( msgData, string, stringLen ) );
	}

/****************************************************************************
*																			*
*								Set Attributes								*
*																			*
****************************************************************************/

/* Set a numeric/boolean attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int setUserAttribute( INOUT USER_INFO *userInfoPtr,
					  IN_INT_Z const int value, 
					  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	int status;

	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( ( attribute == CRYPT_IATTRUBUTE_CERTKEYSET && \
				value == CRYPT_UNUSED ) || \
			  ( value >= 0 && value < MAX_INTLENGTH ) );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_USERINFO_CAKEY_CERTSIGN:
		case CRYPT_USERINFO_CAKEY_CRLSIGN:
		case CRYPT_USERINFO_CAKEY_OCSPSIGN:
			{
			const int requiredKeyUsage = \
				( attribute == CRYPT_USERINFO_CAKEY_CERTSIGN ) ? \
					CRYPT_KEYUSAGE_KEYCERTSIGN : \
				( attribute == CRYPT_USERINFO_CAKEY_CRLSIGN ) ? \
					CRYPT_KEYUSAGE_CRLSIGN : KEYUSAGE_SIGN;
			int attributeValue;

			/* Make sure that this key type isn't already present in the 
			   object */
			if( userInfoPtr->iCryptContext != CRYPT_UNUSED )
				return( exitErrorInited( userInfoPtr, attribute ) );

			/* Make sure that we've been given a signing key */
			status = krnlSendMessage( value, IMESSAGE_CHECK, NULL, 
									  MESSAGE_CHECK_PKC_SIGN );
			if( cryptStatusError( status ) )
				return( CRYPT_ARGERROR_NUM1 );

			/* Make sure that the object has an initialised cert of the
			   correct type associated with it */
			status = krnlSendMessage( value, IMESSAGE_GETATTRIBUTE,
									  &attributeValue, 
									  CRYPT_CERTINFO_IMMUTABLE );
			if( cryptStatusError( status ) || !attributeValue )
				return( CRYPT_ARGERROR_NUM1 );
			status = krnlSendMessage( value, IMESSAGE_GETATTRIBUTE,
									  &attributeValue, 
									  CRYPT_CERTINFO_CERTTYPE );
			if( cryptStatusError( status ) ||
				( attributeValue != CRYPT_CERTTYPE_CERTIFICATE && \
				  attributeValue != CRYPT_CERTTYPE_CERTCHAIN ) )
				return( CRYPT_ARGERROR_NUM1 );

			/* Make sure that the key usage required for this action is
			   permitted.  OCSP is a bit difficult since the key may or may
			   not have an OCSP extended usage (depending on whether the CA
			   bothers to set it or not, even if they do they may delegate
			   the functionality to a short-term generic signing key) and the
			   signing ability may be indicated by either a digital signature
			   flag or a nonrepudiation flag depending on whether the CA
			   considers an OCSP signature to be short or long-term, so we
			   just check for a generic signing ability */
			status = krnlSendMessage( value, IMESSAGE_GETATTRIBUTE,
									  &attributeValue, 
									  CRYPT_CERTINFO_KEYUSAGE );
			if( cryptStatusError( status ) || \
				!( attributeValue & requiredKeyUsage ) )
				return( CRYPT_ARGERROR_NUM1 );

			/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
			/* Save key in the keyset at some point.  Also handle get */
			/* (gets public key) and delete (removes key), this */
			/* functionality is only needed for CA users so is left for */
			/* the full implementation of user roles */
			/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

			return( status );
			}

		case CRYPT_IATTRUBUTE_CERTKEYSET:
			/* If it's a presence check, handle it specially */
			if( value == CRYPT_UNUSED )
				{
				return( trustedCertsPresent( userInfoPtr->trustInfoPtr ) ? \
						CRYPT_OK : CRYPT_ERROR_NOTFOUND );
				}

			/* Send all trusted certs to the keyset */
			return( enumTrustedCerts( userInfoPtr->trustInfoPtr, 
									  CRYPT_UNUSED, value ) );

#ifdef USE_CERTIFICATES
		case CRYPT_IATTRIBUTE_CTL:
			/* Add the certs via the trust list */
			status = addTrustEntry( userInfoPtr->trustInfoPtr,
									value, NULL, 0, FALSE );
			if( cryptStatusOK( status ) )
				userInfoPtr->trustInfoChanged = TRUE;
			return( status );
#endif /* USE_CERTIFICATES */
		}

	/* Anything else has to be a configuration option */
	REQUIRES( attribute > CRYPT_OPTION_FIRST && \
			  attribute < CRYPT_OPTION_LAST );

	/* Set the option.  If it's not one of the two special options with 
	   side-effects, we're done */
	status = setOption( userInfoPtr->configOptions, 
						userInfoPtr->configOptionsCount, attribute, value );
	if( attribute != CRYPT_OPTION_CONFIGCHANGED && \
		attribute != CRYPT_OPTION_SELFTESTOK )
		return( status );

	/* If there was a problem setting a side-effects option, don't go any 
	   further */
	if( status != OK_SPECIAL )
		return( status );

	/* Complete the processing of the special options */
	if( attribute == CRYPT_OPTION_CONFIGCHANGED )
		return( twoPhaseConfigUpdate( userInfoPtr, value ) );
	return( twoPhaseSelftest( userInfoPtr, value ) );
	}

/* Set a string attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int setUserAttributeS( INOUT USER_INFO *userInfoPtr,
					   IN_BUFFER( dataLength ) const void *data,
					   IN_LENGTH const int dataLength,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );
	assert( isReadPtr( data, dataLength ) );

	REQUIRES( dataLength > 0 && dataLength < MAX_INTLENGTH );
	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_USERINFO_PASSWORD:
			return( setUserPassword( userInfoPtr, data, dataLength ) );
		}

	/* Anything else has to be a configuration option */
	REQUIRES( attribute > CRYPT_OPTION_FIRST && \
			  attribute < CRYPT_OPTION_LAST );
	return( setOptionString( userInfoPtr->configOptions, 
							 userInfoPtr->configOptionsCount, attribute, 
							 data, dataLength ) );
	}

/****************************************************************************
*																			*
*								Delete Attributes							*
*																			*
****************************************************************************/

/* Delete an attribute */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteUserAttribute( INOUT USER_INFO *userInfoPtr,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE attribute )
	{
	assert( isWritePtr( userInfoPtr, sizeof( USER_INFO ) ) );

	REQUIRES( isAttribute( attribute ) || \
			  isInternalAttribute( attribute ) );

	switch( attribute )
		{
		case CRYPT_USERINFO_CAKEY_CERTSIGN:
		case CRYPT_USERINFO_CAKEY_CRLSIGN:
		case CRYPT_USERINFO_CAKEY_OCSPSIGN:
			return( CRYPT_ERROR_NOTFOUND );
		}

	/* Anything else has to be a configuration option */
	REQUIRES( attribute > CRYPT_OPTION_FIRST && \
			  attribute < CRYPT_OPTION_LAST );

	return( deleteOption( userInfoPtr->configOptions, 
						  userInfoPtr->configOptionsCount, attribute ) );
	}
