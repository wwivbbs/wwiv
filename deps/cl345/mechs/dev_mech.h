/****************************************************************************
*																			*
*					cryptlib Device Mechanism Header File 					*
*					  Copyright Peter Gutmann 1992-2009						*
*																			*
****************************************************************************/

/* Prototypes for crypto mechanism functions supported by various devices.  
   These are cryptlib-native mechanisms, some devices override these with 
   device-specific implementations */

#ifndef _DEVMECH_DEFINED

#define _DEVMECH_DEFINED

/* Key derivation mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePBKDF2( STDC_UNUSED void *dummy, 
				  INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePKCS12( STDC_UNUSED void *dummy, 
				  INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveSSL( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveTLS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveTLS12( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveCMP( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int derivePGP( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_DERIVE_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int deriveSelftest( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_DERIVE_INFO *mechanismInfo );

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int kdfPBKDF2( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_KDF_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int kdfSelftest( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_DERIVE_INFO *mechanismInfo );

/* Signature mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int signPKCS1( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_SIGN_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int sigcheckPKCS1( STDC_UNUSED void *dummy, 
				   INOUT MECHANISM_SIGN_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int signSSL( STDC_UNUSED void *dummy, 
			 INOUT MECHANISM_SIGN_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int sigcheckSSL( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_SIGN_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int signSelftest( STDC_UNUSED void *dummy, 
				  INOUT MECHANISM_SIGN_INFO *mechanismInfo );

/* Public-key key wrap mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPKCS1( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPKCS1PGP( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPKCS1( STDC_UNUSED void *dummy, 
				 INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPKCS1PGP( STDC_UNUSED void *dummy, 
					INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportOAEP( STDC_UNUSED void *dummy, 
				INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importOAEP( STDC_UNUSED void *dummy, 
				INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int pkcWrapSelftest( STDC_UNUSED void *dummy, 
					 INOUT MECHANISM_WRAP_INFO *mechanismInfo );

/* Symmetric key wrap mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportCMS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importCMS( STDC_UNUSED void *dummy, 
			   INOUT MECHANISM_WRAP_INFO *mechanismInfo );

/* Private key export mechanisms */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPrivateKey( STDC_UNUSED void *dummy, 
					  INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKey( STDC_UNUSED void *dummy, 
					  INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int exportPrivateKeyPKCS8( STDC_UNUSED void *dummy, 
						   INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyPKCS8( STDC_UNUSED void *dummy, 
						   INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyPGP2( STDC_UNUSED void *dummy, 
						  INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyOpenPGPOld( STDC_UNUSED void *dummy, 
								INOUT MECHANISM_WRAP_INFO *mechanismInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int importPrivateKeyOpenPGP( STDC_UNUSED void *dummy, 
							 INOUT MECHANISM_WRAP_INFO *mechanismInfo );

/* A data structure for mapping mechanisms to the functions that implement 
   them.  The device info pointer is optional and used for mechanisms that 
   are tied to particular devices */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
		int ( *MECHANISM_FUNCTION )( IN_OPT void *deviceInfoPtr,
									 INOUT void *mechanismInfo );
typedef struct {
	const MESSAGE_TYPE action;
	const MECHANISM_TYPE mechanism;
	const MECHANISM_FUNCTION function;
	} MECHANISM_FUNCTION_INFO;
#endif /* _DEVMECH_DEFINED */
