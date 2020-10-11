/****************************************************************************
*																			*
*						PGP Keyset Definitions Header File					*
*						Copyright Peter Gutmann 1996-2014					*
*																			*
****************************************************************************/

#ifndef _PGPKEY_DEFINED

#define _PGPKEY_DEFINED

#ifndef _PGP_DEFINED
  #if defined( INC_ALL )
	#include "pgp.h"
  #else
	#include "misc/pgp.h"
  #endif /* Compiler-specific includes */
#endif /* _PGP_DEFINED */

/****************************************************************************
*																			*
*								PGP Keyring Constants						*
*																			*
****************************************************************************/

/* Each PGP key can contain an arbitrary number of user IDs, we only track
   the following maximum number.  Further IDs are read and stored, but not
   indexed or searched on */

#define MAX_PGP_USERIDS		16

/* When reading a PGP keyring we implement a sliding window that reads a
   certain amount of data into a lookahead buffer and then tries to identify
   a key packet group in the buffer.  The following value determines the size
   of the lookahead.  Unfortunately we have to keep this above a certain
   minimum size in order to handle PGP 8.x's inclusion of photo IDs in 
   keyrings, which means that the smallest size that we can safely use is 
   about 8kb */

#define KEYRING_BUFSIZE		8192

/****************************************************************************
*																			*
*						PGP Keyring Types and Structures					*
*																			*
****************************************************************************/

/* Key-related information needed to create a cryptlib context from PGP key
   data */

typedef struct {
	/* Key data information */
	CRYPT_ALGO_TYPE pkcAlgo;		/* Key algorithm */
	CRYPT_ECCCURVE_TYPE curveType;	/* Curve type for ECC */
	int usageFlags;					/* Keymgmt flags permitted usage */
	BUFFER_FIXED( PGP_KEYID_SIZE ) \
	BYTE pgp2KeyID[ PGP_KEYID_SIZE + 8 ];
	BUFFER_FIXED( PGP_KEYID_SIZE ) \
	BYTE openPGPkeyID[ PGP_KEYID_SIZE + 8 ];
	BUFFER_FIXED( pubKeyDataLen ) \
	void *pubKeyData;
	BUFFER_OPT_FIXED( privKeyDataLen ) \
	void *privKeyData;	/* Pointer to encoded pub/priv key data */
	int pubKeyDataLen, privKeyDataLen;

	/* Key data protection information */
	CRYPT_ALGO_TYPE cryptAlgo;		/* Key wrap algorithm */
	int cryptAlgoParam;				/* Optional parameter for key wrap algo */
	BUFFER( CRYPT_MAX_IVSIZE, ivSize ) \
	BYTE iv[ CRYPT_MAX_IVSIZE + 8 ];/* Key wrap IV */
	int ivSize;
	CRYPT_ALGO_TYPE hashAlgo;		/* Password hashing algo */
	int hashAlgoParam;				/* Optional parameter for hash algo */
	BUFFER( PGP_SALTSIZE, saltSize ) \
	BYTE salt[ PGP_SALTSIZE + 8 ];	/* Password hashing salt */
	int saltSize;
	int keySetupIterations;			/* Password hashing iterations */
	BOOLEAN hashedChecksum;			/* Key checksum is SHA-1 hash */
	} PGP_KEYINFO;

/* The following structure contains the the information for one personality,
   which covers one or more of a private key, public key, and subkeys.  PGP
   encodes keys in a complex manner by writing them as groups of (implicitly)
   connected packets that require arbitrary amounts of lookahead to parse.  
   To handle this we read the overall encoded key data as a single unit and
   store it in a dynamically-allocated buffer, then set up pointers to
   locations of relevant data (public and private keys and user IDs) within
   the overall key data.  To further complicate matters, there can be a key
   and subkey associated with the same information, so we have to maintain
   two lots of physical keying information for each logical key.
   
   Writing keys is no easier, because a single logical key can consist of 
   one or more keys and subkeys we can potentially need to add data from 
   multiple contexts to produce a single logical key.  We restrict what can
   be added to a single signature+encryption key or a signature key with an
   encryption key as a subkey.  The isComplete flag records whether further
   key data can be added or not */

typedef struct {
	/* Key and subkey data */
	BUFFER_FIXED( keyDataLen ) \
	void *keyData;					/* Encoded key data */
	int keyDataLen;
	PGP_KEYINFO key, subKey;		/* Key and subkey information */

	/* User ID(s) */
	ARRAY( MAX_PGP_USERIDS, lastUserID ) \
	char *userID[ MAX_PGP_USERIDS + 8 ];/* UserIDs */
	ARRAY( MAX_PGP_USERIDS, lastUserID ) \
	int userIDlen[ MAX_PGP_USERIDS + 8 ];
	int lastUserID;					/* Last used userID */

	/* Miscellaneous data */
	BOOLEAN isOpenPGP;				/* Whether data is PGP 2.x or OpenPGP */
	BOOLEAN isComplete;				/* Whether all key information is present */
	} PGP_INFO;

#define resetPGPInfo( pgpInfo ) \
		memset( &( pgpInfo )->key, 0, sizeof( PGP_KEYINFO ) ); \
		memset( &( pgpInfo )->subKey, 0, sizeof( PGP_KEYINFO ) ); \
		memset( ( pgpInfo )->userID, 0, sizeof( char * ) * MAX_PGP_USERIDS ); \
		memset( ( pgpInfo )->userIDlen, 0, sizeof( int ) * MAX_PGP_USERIDS ); \
		( pgpInfo )->lastUserID = 0; \
		( pgpInfo )->isOpenPGP = ( pgpInfo )->isComplete = FALSE

/* When we're searching for a key, we need to compare each one against a
   collection of match criteria.  The following structure contains the 
   information that we match against */

typedef struct {
	CONST_INIT CRYPT_KEYID_TYPE keyIDtype;/* Key ID type */
	BUFFER_FIXED( keyIDlength ) \
	const void *keyID;
	CONST_INIT int keyIDlength;		/* Key ID */
	CONST_INIT int flags;			/* Key usage flags */
	} KEY_MATCH_INFO;

/****************************************************************************
*																			*
*							PGP Keyring Functions							*
*																			*
****************************************************************************/

/* Utility functions in pgp.c */

STDC_NONNULL_ARG( ( 1 ) ) \
void pgpFreeEntry( INOUT PGP_INFO *pgpInfo );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN pgpCheckKeyMatch( const PGP_INFO *pgpInfo, 
						  const PGP_KEYINFO *keyInfo, 
						  const KEY_MATCH_INFO *keyMatchInfo );

/* Prototypes for functions in pgp_rd.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
int pgpScanPubKeyring( INOUT STREAM *stream, 
					   INOUT PGP_INFO *pgpInfo, 
					   const KEY_MATCH_INFO *keyMatchInfo,
					   OUT_PTR_OPT PGP_KEYINFO **matchedKeyInfoPtrPtr,
					   INOUT ERROR_INFO *errorInfo );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int pgpReadPrivKeyring( INOUT STREAM *stream, 
						IN_ARRAY( maxNoPgpObjects ) PGP_INFO *pgpInfo, 
						IN_LENGTH_SHORT const int maxNoPgpObjects,
						INOUT ERROR_INFO *errorInfo );

/* Prototypes for functions in pgp_wr.c */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWritePubkey( INOUT PGP_INFO *pgpInfoPtr,
					IN_HANDLE const CRYPT_HANDLE cryptHandle );

#endif /* _PGPKEY_DEFINED */
