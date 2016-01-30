/****************************************************************************
*																			*
*							Cert Management ACLs							*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "crypt.h"
  #include "acl.h"
  #include "kernel.h"
#else
  #include "crypt.h"
  #include "kernel/acl.h"
  #include "kernel/kernel.h"
#endif /* Compiler-specific includes */

/* Macro to access the secondary parameter ACL information for a given
   parameter in a list of parameter ACLs */

#define secParamInfo( parentACL, paramNo )	parentACL->secParamACL[ paramNo ]

#if defined( USE_CERTIFICATES ) && defined( USE_KEYSETS )

/* A pointer to the kernel data block */

static KERNEL_DATA *krnlData = NULL;

/****************************************************************************
*																			*
*								Cert Management ACLs						*
*																			*
****************************************************************************/

/* The ACL tables for each cert management action */

static const CERTMGMT_ACL FAR_BSS certMgmtACLTbl[] = {
	/* Create cert store */
	{ CRYPT_CERTACTION_CREATE,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Connect to cert store */
	{ CRYPT_CERTACTION_CONNECT,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Disconnect from cert store */
	{ CRYPT_CERTACTION_DISCONNECT,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Error information */
	{ CRYPT_CERTACTION_ERROR,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Add PKI user */
	{ CRYPT_CERTACTION_ADDUSER,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Cert request */
	{ CRYPT_CERTACTION_REQUEST_CERT,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Cert renewal request */
	{ CRYPT_CERTACTION_REQUEST_RENEWAL,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Cert revocation request */
	{ CRYPT_CERTACTION_REQUEST_REVOCATION,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Cert creation */
	{ CRYPT_CERTACTION_CERT_CREATION,
	  ACTION_PERM_NONE_EXTERNAL,		/* Cert mgmt.use only */
	  { MKACP_O( ST_CTX_PKC,			/* CA key w/cert (see below) */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CERT_CERTREQ | ST_CERT_REQ_CERT,/* Cert request */
				 ACL_FLAG_HIGH_STATE ) },
	  { MKACP_O( ST_CERT_CERT | ST_CERT_CERTCHAIN,	/* CA cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* Confirmation of cert creation */
	{ CRYPT_CERTACTION_CERT_CREATION_COMPLETE,
	  ACTION_PERM_NONE_EXTERNAL,		/* Cert mgmt.use only */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CERT_CERT,			/* Completed cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* Cancellation of cert creation */
	{ CRYPT_CERTACTION_CERT_CREATION_DROP,
	  ACTION_PERM_NONE_EXTERNAL,		/* Cert mgmt.use only */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CERT_CERT,			/* Completed cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* Cancel of creation w.revocation */
	{ CRYPT_CERTACTION_CERT_CREATION_REVERSE,
	  ACTION_PERM_NONE_EXTERNAL,		/* Cert mgmt.use only */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CERT_CERT,			/* Completed cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* Delete reqs after restart */
	{ CRYPT_CERTACTION_RESTART_CLEANUP,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Complete revocation after restart */
	{ CRYPT_CERTACTION_RESTART_REVOKE_CERT,
	  ACTION_PERM_NONE,					/* Cert dbx.use only */
	  { MKACP_END() } },

	/* Cert issue */
	{ CRYPT_CERTACTION_ISSUE_CERT,
	  ACTION_PERM_ALL,					/* Any access */
	  { MKACP_O( ST_CTX_PKC,			/* CA key w/cert (see below) */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_O( ST_CERT_CERTREQ | ST_CERT_REQ_CERT,/* Cert request */
				 ACL_FLAG_HIGH_STATE ) },
	  { MKACP_O( ST_CERT_CERT | ST_CERT_CERTCHAIN,	/* CA cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* CRL issue */
	{ CRYPT_CERTACTION_ISSUE_CRL,
	  ACTION_PERM_ALL,					/* Any access */
	  { MKACP_O( ST_CTX_PKC,			/* CA key w/cert (see below) */
				 ACL_FLAG_HIGH_STATE ),
		MKACP_N_FIXED( CRYPT_UNUSED ) },
	  { MKACP_O( ST_CERT_CERT | ST_CERT_CERTCHAIN,	/* CA cert */
				 ACL_FLAG_HIGH_STATE ) } },

	/* Cert revocation */
	{ CRYPT_CERTACTION_REVOKE_CERT,
	  ACTION_PERM_ALL,					/* Any access */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_O( ST_CERT_REQ_REV,		/* Rev.request.  Rev.reqs are usually */
				 ACL_FLAG_ANY_STATE ) } },/* unsigned, but may be in the high
										   state if imported from an external
										   source */

	/* Cert expiry */
	{ CRYPT_CERTACTION_EXPIRE_CERT,
	  ACTION_PERM_ALL,					/* Any access */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* Clean up on restart */
	{ CRYPT_CERTACTION_CLEANUP,
	  ACTION_PERM_ALL,					/* Any access */
	  { MKACP_N_FIXED( CRYPT_UNUSED ),
		MKACP_N_FIXED( CRYPT_UNUSED ) } },

	/* End-of-ACL marker */
	{ CRYPT_CERTACTION_NONE,
	  ACTION_PERM_NONE,
	  { MKACP_END() } },
	{ CRYPT_CERTACTION_NONE,
	  ACTION_PERM_NONE,
	  { MKACP_END() } }
	};

/****************************************************************************
*																			*
*							Init/Shutdown Functions							*
*																			*
****************************************************************************/

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int initCertMgmtACL( INOUT KERNEL_DATA *krnlDataPtr )
	{
	int i;

	assert( isWritePtr( krnlDataPtr, sizeof( KERNEL_DATA ) ) );

	/* If we're running a fuzzing build, skip the lengthy self-checks */
#ifdef CONFIG_FUZZ
	krnlData = krnlDataPtr;
	return( CRYPT_OK );
#endif /* CONFIG_FUZZ */

	/* Perform a consistency check on the cert management ACLs */
	for( i = 0; certMgmtACLTbl[ i ].action != CRYPT_CERTACTION_NONE && \
				i < FAILSAFE_ARRAYSIZE( certMgmtACLTbl, CERTMGMT_ACL ); i++ )
		{
		const CERTMGMT_ACL *certMgmtACL = &certMgmtACLTbl[ i ];

		/* Actions and permissions are consistent */
		ENSURES( certMgmtACL->action > CRYPT_CERTACTION_NONE && \
				 certMgmtACL->action < CRYPT_CERTACTION_LAST );
		ENSURES( certMgmtACL->access == ACTION_PERM_NONE || \
				 certMgmtACL->access == ACTION_PERM_NONE_EXTERNAL || \
				 certMgmtACL->access == ACTION_PERM_ALL );

		/* If it's a no-access ACL, all mechanisms should be blocked */
		if( certMgmtACL->access == ACTION_PERM_NONE )
			{
			ENSURES( paramInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_NONE );
			continue;
			}

		/* If it's an internal-only ACL, it always needs a request
		   parameter */
		if( certMgmtACL->access == ACTION_PERM_NONE_EXTERNAL )
			{
			if( paramInfo( certMgmtACL, 1 ).valueType != PARAM_VALUE_OBJECT || \
				( paramInfo( certMgmtACL, 1 ).subTypeA & \
					~( ST_CERT_CERTREQ | ST_CERT_REQ_CERT | \
					   ST_CERT_REQ_REV | ST_CERT_CERT ) ) || \
				paramInfo( certMgmtACL, 1 ).subTypeB != ST_NONE || \
				paramInfo( certMgmtACL, 1 ).subTypeC != ST_NONE )
				{
				DEBUG_DIAG(( "Certificate management ACLs inconsistent" ));
				retIntError();
				}
			}

		/* If it requires a CA key parameter, it must be a private-key
		   context with the key loaded and an attached CA certificate */
		if( paramInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_OBJECT )
			{
			ENSURES( paramInfo( certMgmtACL, 0 ).subTypeA == ST_CTX_PKC && \
					 paramInfo( certMgmtACL, 0 ).subTypeB == ST_NONE && \
					 paramInfo( certMgmtACL, 0 ).subTypeC == ST_NONE && \
					 paramInfo( certMgmtACL, 0 ).flags == ACL_FLAG_HIGH_STATE );
			if( ( secParamInfo( certMgmtACL, 0 ).subTypeA & \
					~( ST_CERT_CERT | ST_CERT_CERTCHAIN ) ) || \
				secParamInfo( certMgmtACL, 0 ).subTypeB != ST_NONE || \
				secParamInfo( certMgmtACL, 0 ).subTypeC != ST_NONE || \
				secParamInfo( certMgmtACL, 0 ).flags != ACL_FLAG_HIGH_STATE )
				{
				DEBUG_DIAG(( "Certificate management ACLs inconsistent" ));
				retIntError();
				}
			continue;
			}
		ENSURES( paramInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_NUMERIC && \
				 paramInfo( certMgmtACL, 0 ).lowRange == CRYPT_UNUSED );
		}
	ENSURES( i < FAILSAFE_ARRAYSIZE( certMgmtACLTbl, CERTMGMT_ACL ) );

	/* Set up the reference to the kernel data block */
	krnlData = krnlDataPtr;

	return( CRYPT_OK );
	}

void endCertMgmtACL( void )
	{
	krnlData = NULL;
	}

/****************************************************************************
*																			*
*						Cert Management ACL Check Functions					*
*																			*
****************************************************************************/

/* Functions to implement the checks in the cert management ACL tables */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckCertMgmtAccess( IN_HANDLE const int objectHandle,
									IN_MESSAGE const MESSAGE_TYPE message,
									IN_BUFFER_C( sizeof( MESSAGE_CERTMGMT_INFO ) ) \
										const void *messageDataPtr,
									IN_ENUM( CRYPT_CERTACTION ) \
										const int messageValue,
									STDC_UNUSED const void *dummy )
	{
	const MESSAGE_CERTMGMT_INFO *mechanismInfo = \
		  ( MESSAGE_CERTMGMT_INFO * ) messageDataPtr;
	const CERTMGMT_ACL *certMgmtACL = certMgmtACLTbl;
	const OBJECT_INFO *objectTable = krnlData->objectTable;
	int i;

	assert( isReadPtr( messageDataPtr, sizeof( MESSAGE_CERTMGMT_INFO ) ) );

	/* Precondition */
	REQUIRES( isValidObject( objectHandle ) );
	REQUIRES( message == MESSAGE_KEY_CERTMGMT || message == IMESSAGE_KEY_CERTMGMT );
	REQUIRES( messageValue > CRYPT_CERTACTION_NONE && \
			  messageValue < CRYPT_CERTACTION_LAST );

	/* Find the appropriate ACL for this mechanism */
	for( i = 0; certMgmtACL[ i ].action != messageValue && \
				certMgmtACL[ i ].action != CRYPT_CERTACTION_NONE && \
				i < FAILSAFE_ARRAYSIZE( certMgmtACLTbl, CERTMGMT_ACL ); 
		 i++ );
	ENSURES( i < FAILSAFE_ARRAYSIZE( certMgmtACLTbl, CERTMGMT_ACL ) );
	ENSURES( certMgmtACL[ i ].action != CRYPT_CERTACTION_NONE );
	certMgmtACL = &certMgmtACL[ i ];

	/* Make sure that the access is valid.  Most cert management actions can
	   never be initiated explicitly (they're only used internally by the
	   cert management code), a few can be initiated explicitly but only
	   internally by some cert management protocols, and an even smaller
	   number can be initiated externally */
	switch( certMgmtACL->access )
		{
		case ACTION_PERM_ALL:
			/* Any access is valid */
			break;

		case ACTION_PERM_NONE_EXTERNAL:
			/* Only internal access (e.g. from a cert management protocol)
			   is permitted */
			if( !isInternalMessage( message ) )
				return( CRYPT_ARGERROR_VALUE );
			break;

		case ACTION_PERM_NONE:
			/* No access is permitted, it's a value used only by the cert
			   management code */
			return( CRYPT_ARGERROR_VALUE );

		default:
			retIntError();
		}

	/* Check the mechanism parameters */
	if( paramInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_OBJECT )
		{
		if( !fullObjectCheck( mechanismInfo->caKey, message ) || \
			!isSameOwningObject( objectHandle, mechanismInfo->caKey ) )
			return( CRYPT_ARGERROR_NUM1 );
		if( !checkParamObject( paramInfo( certMgmtACL, 0 ), \
							   mechanismInfo->caKey ) )
			return( CRYPT_ARGERROR_NUM1 );

		/* If there's a secondary parameter present, check it agains the
		   dependent object.  We perform a basic isValidObject() check
		   rather than a fullObjectCheck() since the dependent object is
		   usually internal, and this would fail with an external message */
		if( secParamInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_OBJECT )
			{
			const int dependentObject = \
						objectTable[ mechanismInfo->caKey ].dependentObject;

			if( !isValidObject( dependentObject ) )
				return( CRYPT_ARGERROR_NUM1 );
			if( !checkParamObject( secParamInfo( certMgmtACL, 0 ), \
								   dependentObject ) )
				return( CRYPT_ARGERROR_NUM1 );
			}
		}
	else
		{
		REQUIRES( paramInfo( certMgmtACL, 0 ).valueType == PARAM_VALUE_NUMERIC && \
				  paramInfo( certMgmtACL, 0 ).lowRange == CRYPT_UNUSED );

		if( mechanismInfo->caKey != CRYPT_UNUSED )
			return( CRYPT_ARGERROR_NUM1 );
		}
	if( paramInfo( certMgmtACL, 1 ).valueType == PARAM_VALUE_OBJECT )
		{
		if( !fullObjectCheck( mechanismInfo->request, message ) || \
			!isSameOwningObject( objectHandle, mechanismInfo->request ) )
			return( CRYPT_ARGERROR_NUM2 );
		if( !checkParamObject( paramInfo( certMgmtACL, 1 ), \
							   mechanismInfo->request ) )
			return( CRYPT_ARGERROR_NUM2 );
		}
	else
		{
		REQUIRES( paramInfo( certMgmtACL, 1 ).valueType == PARAM_VALUE_NUMERIC && \
				  paramInfo( certMgmtACL, 1 ).lowRange == CRYPT_UNUSED );

		if( mechanismInfo->request != CRYPT_UNUSED )
			return( CRYPT_ARGERROR_NUM2 );
		}

	return( CRYPT_OK );
	}
#else

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int preDispatchCheckCertMgmtAccess( IN_HANDLE const int objectHandle,
									IN_MESSAGE const MESSAGE_TYPE message,
									IN_BUFFER_C( sizeof( MESSAGE_CERTMGMT_INFO ) ) \
										const void *messageDataPtr,
									IN_ENUM( CRYPT_CERTACTION ) \
										const int messageValue,
									STDC_UNUSED const void *dummy )
	{
	UNUSED_ARG( messageDataPtr );

	return( CRYPT_ERROR_PERMISSION );
	}
#endif /* USE_CERTIFICATES && USE_KEYSETS */
