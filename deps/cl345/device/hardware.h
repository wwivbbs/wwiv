/****************************************************************************
*																			*
*					cryptlib Generic Crypto HW Header						*
*					Copyright Peter Gutmann 1998-2009						*
*																			*
****************************************************************************/

/* The access functions that must be provided by each HAL module */

int hwGetCapabilities( const CAPABILITY_INFO **capabilityInfo,
					   int *noCapabilities );
int hwGetRandom( void *buffer, const int length );
int hwLookupItem( const void *keyID, const int keyIDlength, int *keyHandle );
int hwDeleteItem( const int keyHandle );
int hwInitialise( void );

/* Helper functions in hardware.c that may be used by HAL modules */

int setPersonalityMapping( CONTEXT_INFO *contextInfoPtr, const int keyHandle,
						   void *storageID, const int storageIDlength );
int generatePKCcomponents( CONTEXT_INFO *contextInfoPtr, void *keyInfo, 
						   const int keySizeBits );
int setPKCinfo( CONTEXT_INFO *contextInfoPtr, 
				const CRYPT_ALGO_TYPE cryptAlgo, const void *keyInfo );
int setConvInfo( const CRYPT_CONTEXT iCryptContext, const int keySize );
int cleanupHardwareContext( const CONTEXT_INFO *contextInfoPtr );
