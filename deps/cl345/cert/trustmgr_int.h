/****************************************************************************
*																			*
*				Certificate Trust Manger Internal Interface 				*
*						Copyright Peter Gutmann 1998-2016					*
*																			*
****************************************************************************/

#ifndef _TRUSTMGR_INT_DEFINED

#define _TRUSTMGR_INT_DEFINED

/* The size of the table of trust information.  This must be a power of 2 */

#ifdef CONFIG_CONSERVE_MEMORY
  #define TRUSTINFO_SIZE		16
#else
  #define TRUSTINFO_SIZE		256
#endif /* CONFIG_CONSERVE_MEMORY */

/* Trusted certificate information */

typedef struct TI {
	/* Identification information, the checksum and hash of the 
	   certificate's subjectName and subjectKeyIdentifier */
	int sCheck;
	BYTE sHash[ HASH_DATA_SIZE + 4 ];
#if 0	/* sKID lookup isn't used at present */
	int kCheck;
	BYTE kHash[ HASH_DATA_SIZE + 4 ];
#endif /* 0 */

	/* The trusted certificate.  When we read trusted certificates from a 
	   configuration file the certificate is stored in the encoded form to 
	   save creating a pile of certificate objects that'll never be used.  
	   When it's needed the certificate is created on the fly from the 
	   encoded form.  Conversely, when we get the trust information directly 
	   from the user the certificate object already exists and the encoded 
	   form isn't used */
	DATAPTR certObject;
	int certObjectLength;
	int certChecksum;
	CRYPT_CERTIFICATE iCryptCert;

	/* Pointer to the next entry */
	DATAPTR next, prev;			/* Next trustInfo record in the chain */
	} TRUST_INFO;

/* The data structure that stores the trust management information */

typedef struct {
	/* Array of pointers to certificate trust information */
	DATAPTR trustInfo[ TRUSTINFO_SIZE ];	

	/* Checksum for the trust information array */
	int trustInfoChecksum;
	} TRUST_INFO_CONTAINER;

/* Trust manager access function */

CHECK_RETVAL_PTR_NONNULL \
void *getTrustMgrStorage( void );

#endif /* _TRUSTMGR_INT_DEFINED */
