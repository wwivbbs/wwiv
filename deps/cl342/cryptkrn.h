/****************************************************************************
*																			*
*					  cryptlib Kernel Interface Header File 				*
*						Copyright Peter Gutmann 1992-2013					*
*																			*
****************************************************************************/

#ifndef _CRYPTKRN_DEFINED

#define _CRYPTKRN_DEFINED

/* Macros to handle code correctness checking of critical sections of the 
   code such as the kernel and CSPRNG (sed quis custodiet ipsos custodes?), 
   extending the design-by-contract macros in misc/int_api.h */

/* Value of a variable at the start of block scope, used for postcondition 
   predicates.  The pointer is declared as BYTE * rather than the more 
   general void * in order to allow range comparisons, and a BYTE * rather 
   than char * because of compilers that complain about comparisons between 
   signed and unsigned pointer types.  Note that these declarations must be 
   the last in any set of variable declarations since some of the higher-
   overhead checks are only applied in the debug build and the release build 
   expands them to nothing, leaving only the terminating semicolon on the 
   line, which must follow all other declarations */

#define ORIGINAL_VALUE( x )			orig_##x
#define ORIGINAL_INT( x )			const int orig_##x = ( int ) x
#define ORIGINAL_PTR( x )			const BYTE *orig_##x = ( const BYTE * ) x

/* Sometimes we can't use the preprocessor tricks above because the value 
   being saved isn't a primitive type or the variable value isn't available 
   at the start of the block, in which case we have to use the somewhat less 
   transparent macros below.
   
   In a small number of cases the values declared by these macros are only
   used in debug builds, leading to unused-variable warnings in release 
   builds.  This is infrequent enough that there's not much point in adding
   even further special-casing for them */

#define ORIGINAL_INT_VAR( x, y )	const int orig_##x = ( y )
#define DECLARE_ORIGINAL_INT( x )	int orig_##x
#define STORE_ORIGINAL_INT( x, y )	orig_##x = ( y )

/* Some checks have a relatively high overhead (for example for_all and 
   there_exists on arrays) so we only perform them in the debug build */

#if !defined( NDEBUG )

/* Universal qualifiers: for_all, there_exists.  Because of the typical use 
   of 'i' as loop variables we'll get compiler warnings about duplicate 
   variable declarations, unfortunately we can't fix this with something 
   like '_local_##iter' because the loop test condition is a free-form 
   expression and there's no easy way to create a localised form of all 
   references to the loop variable inside the expression */

#define FORALL( iter, start, end, condition ) \
		{ \
		int iter; \
		\
		for( iter = ( start ); iter < ( end ); iter++ ) \
			assert( condition ); \
		}

#define EXISTS( iter, start, end, condition ) \
		{ \
		int iter; \
		\
		for( iter = ( start ); iter < ( end ); iter++ ) \
			{ \
			if( condition ) \
				break; \
			} \
		assert( iter < ( end ) ); \
		}
#else

/* Non-debug version, no-op out the various checks */

#define TEMP_INT( a )
#define TEMP_VAR( a )
#define FORALL( a, b, c, d )
#define EXISTS( a, b, c, d )

#endif /* NDEBUG */

/****************************************************************************
*																			*
*							Object Message Types							*
*																			*
****************************************************************************/

/* The object types */

typedef enum {
	OBJECT_TYPE_NONE,				/* No object type */
	OBJECT_TYPE_CONTEXT,			/* Context */
	OBJECT_TYPE_KEYSET,				/* Keyset */
	OBJECT_TYPE_ENVELOPE,			/* Envelope */
	OBJECT_TYPE_CERTIFICATE,		/* Certificate */
	OBJECT_TYPE_DEVICE,				/* Crypto device */
	OBJECT_TYPE_SESSION,			/* Secure session */
	OBJECT_TYPE_USER,				/* User object */
	OBJECT_TYPE_LAST				/* Last possible object type */
	} OBJECT_TYPE;

/* Object subtypes.  The subtype names aren't needed by the kernel (it just 
   treats the values as an anonymous bitfield during an ACL check) but they 
   are used in the ACL definitions and by the code that calls 
   krnlCreateObject(), so they need to be defined here.

   Because there are so many object subtypes we have to split them across 
   two 32-bit bitfields in order to permit a simple bitwise AND check, if we 
   ordered them by the more obvious major and minor type (that is, object 
   type and subtype) this wouldn't be necessary but it would increase the 
   size of the compiled ACL table (from 2 * 32 bits to NO_OBJECT_TYPES * 
   32 bits) and would make automated consistency checking difficult since 
   it's no longer possible to spot a case where a subtype bit for object A 
   has inadvertently been set for object B.

   To resolve this, we divide the subtype bit field into two smaller bit 
   fields (classes) with the high two bits designating which class the 
   subtype is in (actually we use the bits one below the high bit since 
   this may be interpreted as a sign bit by some preprocessors even if it's 
   declared as a xxxxUL, so in the following discussion we're talking about 
   logical rather than physical high bits).  Class A is always 01xxx..., 
   class B is always 10xxx...  If we get an entry that has 11xxx... we know 
   that the ACL entry is inconsistent.  This isn't pretty, but it's the 
   least ugly way to do it that still allows the ACL table to be built 
   using the preprocessor.

   Note that the device and keyset values must be in the same class, since 
   they're interchangeable for many message types and this simplifies some 
   of the MKACL() macros that only need to initialise one class type.
   
   The different between SUBTYPE_KEYSET_FILE, SUBTYPE_KEYSET_FILE_PARTIAL,
   and SUBTYPE_KEYSET_FILE_READONLY is that SUBTYPE_KEYSET_FILE stores a 
   full index of key ID types and allows storage of any data type, 
   SUBTYPE_KEYSET_FILE_PARTIAL only handles one or two key IDs and a 
   restricted set of data types so that only a single private key and 
   associated public key and certificate can be stored, and 
   SUBTYPE_KEYSET_FILE_READONLY is even more restricted and can't be 
   updated in any sensible manner.
   
   The difference between SUBTYPE_KEYSET_DBMS and SUBTYPE_KEYSET_DBMS_STORE 
   is similar, the former is a simple certificate-and-CRL store while the 
   latter stores assorted CA management data items and supports extended 
   operations */

#define SUBTYPE_CLASS_MASK			0x70000000L
#define SUBTYPE_CLASS_A				0x10000000L
#define SUBTYPE_CLASS_B				0x20000000L
#define SUBTYPE_CLASS_C				0x40000000L

#define MK_SUBTYPE_A( value )		( SUBTYPE_CLASS_A | ( 1L << ( value - 1 ) ) )
#define MK_SUBTYPE_B( value )		( SUBTYPE_CLASS_B | ( 1L << ( value - 1 ) ) )
#define MK_SUBTYPE_C( value )		( SUBTYPE_CLASS_C | ( 1L << ( value - 1 ) ) )

#define SUBTYPE_NONE				0x00000000L

#define SUBTYPE_CTX_CONV			MK_SUBTYPE_A( 1 )
#define SUBTYPE_CTX_PKC				MK_SUBTYPE_A( 2 )
#define SUBTYPE_CTX_HASH			MK_SUBTYPE_A( 3 )
#define SUBTYPE_CTX_MAC				MK_SUBTYPE_A( 4 )
#define SUBTYPE_CTX_GENERIC			MK_SUBTYPE_A( 5 )

#define SUBTYPE_CERT_CERT			MK_SUBTYPE_A( 6 )
#define SUBTYPE_CERT_CERTREQ		MK_SUBTYPE_A( 7 )
#define SUBTYPE_CERT_REQ_CERT		MK_SUBTYPE_A( 8 )
#define SUBTYPE_CERT_REQ_REV		MK_SUBTYPE_A( 9 )
#define SUBTYPE_CERT_CERTCHAIN		MK_SUBTYPE_A( 10 )
#define SUBTYPE_CERT_ATTRCERT		MK_SUBTYPE_A( 11 )
#define SUBTYPE_CERT_CRL			MK_SUBTYPE_A( 12 )
#define SUBTYPE_CERT_CMSATTR		MK_SUBTYPE_A( 13 )
#define SUBTYPE_CERT_RTCS_REQ		MK_SUBTYPE_A( 14 )
#define SUBTYPE_CERT_RTCS_RESP		MK_SUBTYPE_A( 15 )
#define SUBTYPE_CERT_OCSP_REQ		MK_SUBTYPE_A( 16 )
#define SUBTYPE_CERT_OCSP_RESP		MK_SUBTYPE_A( 17 )
#define SUBTYPE_CERT_PKIUSER		MK_SUBTYPE_A( 18 )

#define SUBTYPE_ENV_ENV				MK_SUBTYPE_B( 1 )
#define SUBTYPE_ENV_ENV_PGP			MK_SUBTYPE_B( 2 )
#define SUBTYPE_ENV_DEENV			MK_SUBTYPE_B( 3 )

#define SUBTYPE_KEYSET_FILE			MK_SUBTYPE_B( 4 )
#define SUBTYPE_KEYSET_FILE_PARTIAL	MK_SUBTYPE_B( 5 )
#define SUBTYPE_KEYSET_FILE_READONLY MK_SUBTYPE_B( 6 )
#define SUBTYPE_KEYSET_DBMS			MK_SUBTYPE_B( 7 )
#define SUBTYPE_KEYSET_DBMS_STORE	MK_SUBTYPE_B( 8 )
#define SUBTYPE_KEYSET_HTTP			MK_SUBTYPE_B( 9 )
#define SUBTYPE_KEYSET_LDAP			MK_SUBTYPE_B( 10 )

#define SUBTYPE_DEV_SYSTEM			MK_SUBTYPE_B( 11 )
#define SUBTYPE_DEV_PKCS11			MK_SUBTYPE_B( 12 )
#define SUBTYPE_DEV_CRYPTOAPI		MK_SUBTYPE_B( 13 )
#define SUBTYPE_DEV_HARDWARE		MK_SUBTYPE_B( 14 )

#define SUBTYPE_SESSION_SSH			MK_SUBTYPE_C( 1 )
#define SUBTYPE_SESSION_SSH_SVR		MK_SUBTYPE_C( 2 )
#define SUBTYPE_SESSION_SSL			MK_SUBTYPE_C( 3 )
#define SUBTYPE_SESSION_SSL_SVR		MK_SUBTYPE_C( 4 )
#define SUBTYPE_SESSION_RTCS		MK_SUBTYPE_C( 5 )
#define SUBTYPE_SESSION_RTCS_SVR	MK_SUBTYPE_C( 6 )
#define SUBTYPE_SESSION_OCSP		MK_SUBTYPE_C( 7 )
#define SUBTYPE_SESSION_OCSP_SVR	MK_SUBTYPE_C( 8 )
#define SUBTYPE_SESSION_TSP			MK_SUBTYPE_C( 9 )
#define SUBTYPE_SESSION_TSP_SVR		MK_SUBTYPE_C( 10 )
#define SUBTYPE_SESSION_CMP			MK_SUBTYPE_C( 11 )
#define SUBTYPE_SESSION_CMP_SVR		MK_SUBTYPE_C( 12 )
#define SUBTYPE_SESSION_SCEP		MK_SUBTYPE_C( 13 )
#define SUBTYPE_SESSION_SCEP_SVR	MK_SUBTYPE_C( 14 )
#define SUBTYPE_SESSION_CERT_SVR	MK_SUBTYPE_C( 15 )

#define SUBTYPE_USER_SO				MK_SUBTYPE_C( 16 )
#define SUBTYPE_USER_NORMAL			MK_SUBTYPE_C( 17 )
#define SUBTYPE_USER_CA				MK_SUBTYPE_C( 18 )

#define SUBTYPE_LAST				SUBTYPE_USER_CA

/* The data type used to store subtype values */

#ifdef SYSTEM_16BIT
  typedef long OBJECT_SUBTYPE;
#else
  typedef int OBJECT_SUBTYPE;
#endif /* 16- vs.32-bit systems */

/* Message flags.  Normally messages can only be sent to external objects, 
   however we can also explicitly send them to internal objects which means 
   that we use the internal rather than external access ACL.  This can only 
   be done from inside cryptlib, for example when an object sends a message 
   to a subordinate object */

#define MESSAGE_FLAG_INTERNAL		0x100
#define MKINTERNAL( message )		( message | MESSAGE_FLAG_INTERNAL )

/* A mask to extract the basic message type */

#define MESSAGE_MASK				0xFF

/* The message types that can be sent to an object via krnlSendMessage(). 
   By default messages can only be sent to externally visible objects, there 
   are also internal versions that can be sent to all objects.  The object 
   messages have the following arguments:

	Type								DataPtr			Value
	---------------------------			-------			-----
	MESSAGE_DESTROY						NULL			0
	MESSAGE_INC/DECREFCOUNT				NULL			0
	MESSAGE_GETDEPENDENT				&objectHandle	objectType
	MESSAGE_SETDEPENDENT				&objectHandle	incRefCount
	MESSAGE_CLONE						NULL			cloneContext
	MESSAGE_GET/SETATTRIBUTE			&value			attributeType
	MESSAGE_DELETEATTRIBUTE				NULL			attributeType
	MESSAGE_COMPARE						&value			compareType
	MESSAGE_CHECK						NULL			requestedUse
	MESSAGE_SELFTEST					NULL			0

	MESSAGE_CHANGENOTIFY				&value			attributeType

	MESSAGE_CTX_ENCRYPT/DECRYPT/SIGN/-
		SIGCHECK/HASH					&value			valueLength
	MESSAGE_CTX_GENKEY					NULL			0
	MESSAGE_CTX_GENIV					NULL			0

	MESSAGE_CRT_SIGN,					NULL			sigKey
	MESSAGE_CRT_SIGCHECK,				NULL			verifyObject
	MESSAGE_CRT_EXPORT,					&value			formatType

	MESSAGE_DEV_QUERYCAPABILITY			&queryInfo		algorithm
	MESSAGE_DEV_EXPORT/IMPORT/SIGN/-
		SIGCHECK/DERIVE/KDF				&mechanismInfo	mechanismType
	MESSAGE_DEV_CREATEOBJECT			&createInfo		objectType
	MESSAGE_DEV_CREATEOBJECT_INDIRECT	&createInfo		objectType

	MESSAGE_ENV_PUSH/POPDATA			&value			0

	MESSAGE_KEY_GET/SET/DELETEKEY		&keymgmtInfo	itemType
	MESSAGE_KEY_GETFIRST/NEXTCERT		&keymgmtInfo	itemType
	MESSAGE_KEY_CERTMGMT				&certMgmtInfo	action 
	
	MESSAGE_USER_USERMGMT				&value			action
	MESSAGE_USER_TRUSTMGMT				&value			action */

typedef enum {
	MESSAGE_NONE,				/* No message type */

	/* Control messages to externally visible objects (the internal versions 
	   are defined further down).  These messages are handled directly by 
	   the kernel and don't affect the object itself except for 
	   MESSAGE_DESTROY which is generated by the kernel in response to the 
	   final MESSAGE_DECREFCOUNT sent to an object.  These are forwarded out 
	   to the object to get it to clean up its state before the kernel 
	   destroys it */
	MESSAGE_DESTROY,			/* Destroy the object */
	MESSAGE_INCREFCOUNT,		/* Increment object ref.count */
	MESSAGE_DECREFCOUNT,		/* Decrement object ref.count */
	MESSAGE_GETDEPENDENT,		/* Get dependent object */
	MESSAGE_SETDEPENDENT,		/* Set dependent object (e.g.ctx->dev) */
	MESSAGE_CLONE,				/* Clone the object */

	/* Attribute messages.  The reason for the numeric vs.non-numeric 
	   attribute messages is that for improved error checking the data types 
	   that these work with are explicitly specified by the user based on 
	   which function they call to get/set them rather than being implicitly 
	   specified by the attribute ID.  Because of the explicit typing, the 
	   handlers have to be able to check to make sure that the actual type 
	   matches what the user specified, so we need one message type for 
	   numeric attributes and one for string attributes */
	MESSAGE_GETATTRIBUTE,		/* Get numeric object attribute */
	MESSAGE_GETATTRIBUTE_S,		/* Get string object attribute */
	MESSAGE_SETATTRIBUTE,		/* Set numeric object attribute */
	MESSAGE_SETATTRIBUTE_S,		/* Set string object attribute */
	MESSAGE_DELETEATTRIBUTE,	/* Delete object attribute */

	/* General messages.  The check message is used for informational 
	   purposes only so that problems (e.g. an attempt to use a public key 
	   where a private key is required) can be reported to the user 
	   immediately as a function parameter error rather than appearing much 
	   later as an object use permission error when the kernel blocks the 
	   access.  Final access checking is always still done at the kernel 
	   level to avoid the confused deputy problem */
	MESSAGE_COMPARE,			/* Compare objs. or obj.properties */
	MESSAGE_CHECK,				/* Check object info */
	MESSAGE_SELFTEST,			/* Perform a self-test */

	/* Messages sent from the kernel to object message handlers.  These never 
	   originate from outside the kernel but are generated in response to 
	   other messages to notify an object of a change in its state */
	MESSAGE_CHANGENOTIFY,		/* Notification of obj.status chge.*/

	/* Object-type-specific messages */
	MESSAGE_CTX_ENCRYPT,		/* Context: Action = encrypt */
	MESSAGE_CTX_DECRYPT,		/* Context: Action = decrypt */
	MESSAGE_CTX_SIGN,			/* Context: Action = sign */
	MESSAGE_CTX_SIGCHECK,		/* Context: Action = sigcheck */
	MESSAGE_CTX_HASH,			/* Context: Action = hash */
	MESSAGE_CTX_GENKEY,			/* Context: Generate a key */
	MESSAGE_CTX_GENIV,			/* Context: Generate an IV */
	MESSAGE_CRT_SIGN,			/* Cert: Action = sign cert */
	MESSAGE_CRT_SIGCHECK,		/* Cert: Action = check/verify cert */
	MESSAGE_CRT_EXPORT,			/* Cert: Export encoded cert data */
	MESSAGE_DEV_QUERYCAPABILITY,/* Device: Query capability */
	MESSAGE_DEV_EXPORT,			/* Device: Action = export key */
	MESSAGE_DEV_IMPORT,			/* Device: Action = import key */
	MESSAGE_DEV_SIGN,			/* Device: Action = sign */
	MESSAGE_DEV_SIGCHECK,		/* Device: Action = sig.check */
	MESSAGE_DEV_DERIVE,			/* Device: Action = derive key */
	MESSAGE_DEV_KDF,			/* Device: Action = KDF */
	MESSAGE_DEV_CREATEOBJECT,	/* Device: Create object */
	MESSAGE_DEV_CREATEOBJECT_INDIRECT,	/* Device: Create obj.from data */
	MESSAGE_ENV_PUSHDATA,		/* Envelope: Push data */
	MESSAGE_ENV_POPDATA,		/* Envelope: Pop data */
	MESSAGE_KEY_GETKEY,			/* Keyset: Instantiate ctx/cert */
	MESSAGE_KEY_SETKEY,			/* Keyset: Add ctx/cert */
	MESSAGE_KEY_DELETEKEY,		/* Keyset: Delete key/cert */
	MESSAGE_KEY_GETFIRSTCERT,	/* Keyset: Get first cert in sequence */
	MESSAGE_KEY_GETNEXTCERT,	/* Keyset: Get next cert in sequence */
	MESSAGE_KEY_CERTMGMT,		/* Keyset: Cert management */
	MESSAGE_USER_USERMGMT,		/* User: User management */
	MESSAGE_USER_TRUSTMGMT,		/* User: Trust management */
	MESSAGE_LAST,				/* Last valid message type */

	/* Internal-object versions of the above messages */
	IMESSAGE_DESTROY = MKINTERNAL( MESSAGE_DESTROY ),
	IMESSAGE_INCREFCOUNT = MKINTERNAL( MESSAGE_INCREFCOUNT ),
	IMESSAGE_DECREFCOUNT = MKINTERNAL( MESSAGE_DECREFCOUNT ),
	IMESSAGE_GETDEPENDENT = MKINTERNAL( MESSAGE_GETDEPENDENT ),
	IMESSAGE_SETDEPENDENT = MKINTERNAL( MESSAGE_SETDEPENDENT ),
	IMESSAGE_CLONE = MKINTERNAL( MESSAGE_CLONE ),

	IMESSAGE_GETATTRIBUTE = MKINTERNAL( MESSAGE_GETATTRIBUTE ),
	IMESSAGE_GETATTRIBUTE_S = MKINTERNAL( MESSAGE_GETATTRIBUTE_S ),
	IMESSAGE_SETATTRIBUTE = MKINTERNAL( MESSAGE_SETATTRIBUTE ),
	IMESSAGE_SETATTRIBUTE_S = MKINTERNAL( MESSAGE_SETATTRIBUTE_S ),
	IMESSAGE_DELETEATTRIBUTE = MKINTERNAL( MESSAGE_DELETEATTRIBUTE ),

	IMESSAGE_COMPARE = MKINTERNAL( MESSAGE_COMPARE ),
	IMESSAGE_CHECK = MKINTERNAL( MESSAGE_CHECK ),
	IMESSAGE_SELFTEST = MKINTERNAL( MESSAGE_SELFTEST ),

	IMESSAGE_CHANGENOTIFY = MKINTERNAL( MESSAGE_CHANGENOTIFY ),

	IMESSAGE_CTX_ENCRYPT = MKINTERNAL( MESSAGE_CTX_ENCRYPT ),
	IMESSAGE_CTX_DECRYPT = MKINTERNAL( MESSAGE_CTX_DECRYPT ),
	IMESSAGE_CTX_SIGN = MKINTERNAL( MESSAGE_CTX_SIGN ),
	IMESSAGE_CTX_SIGCHECK = MKINTERNAL( MESSAGE_CTX_SIGCHECK ),
	IMESSAGE_CTX_HASH = MKINTERNAL( MESSAGE_CTX_HASH ),
	IMESSAGE_CTX_GENKEY = MKINTERNAL( MESSAGE_CTX_GENKEY ),
	IMESSAGE_CTX_GENIV = MKINTERNAL( MESSAGE_CTX_GENIV ),
	IMESSAGE_CRT_SIGN = MKINTERNAL( MESSAGE_CRT_SIGN ),
	IMESSAGE_CRT_SIGCHECK = MKINTERNAL( MESSAGE_CRT_SIGCHECK ),
	IMESSAGE_CRT_EXPORT = MKINTERNAL( MESSAGE_CRT_EXPORT ),
	IMESSAGE_DEV_QUERYCAPABILITY = MKINTERNAL( MESSAGE_DEV_QUERYCAPABILITY ),
	IMESSAGE_DEV_EXPORT = MKINTERNAL( MESSAGE_DEV_EXPORT ),
	IMESSAGE_DEV_IMPORT = MKINTERNAL( MESSAGE_DEV_IMPORT ),
	IMESSAGE_DEV_SIGN = MKINTERNAL( MESSAGE_DEV_SIGN ),
	IMESSAGE_DEV_SIGCHECK = MKINTERNAL( MESSAGE_DEV_SIGCHECK ),
	IMESSAGE_DEV_DERIVE = MKINTERNAL( MESSAGE_DEV_DERIVE ),
	IMESSAGE_DEV_KDF = MKINTERNAL( MESSAGE_DEV_KDF ),
	IMESSAGE_DEV_CREATEOBJECT = MKINTERNAL( MESSAGE_DEV_CREATEOBJECT ),
	IMESSAGE_DEV_CREATEOBJECT_INDIRECT = MKINTERNAL( MESSAGE_DEV_CREATEOBJECT_INDIRECT ),
	IMESSAGE_ENV_PUSHDATA = MKINTERNAL( MESSAGE_ENV_PUSHDATA ),
	IMESSAGE_ENV_POPDATA = MKINTERNAL( MESSAGE_ENV_POPDATA ),
	IMESSAGE_KEY_GETKEY = MKINTERNAL( MESSAGE_KEY_GETKEY ),
	IMESSAGE_KEY_SETKEY = MKINTERNAL( MESSAGE_KEY_SETKEY ),
	IMESSAGE_KEY_DELETEKEY = MKINTERNAL( MESSAGE_KEY_DELETEKEY ),
	IMESSAGE_KEY_GETFIRSTCERT = MKINTERNAL( MESSAGE_KEY_GETFIRSTCERT ),
	IMESSAGE_KEY_GETNEXTCERT = MKINTERNAL( MESSAGE_KEY_GETNEXTCERT ),
	IMESSAGE_KEY_CERTMGMT = MKINTERNAL( MESSAGE_KEY_CERTMGMT ),
	IMESSAGE_USER_USERMGMT = MKINTERNAL( MESSAGE_USER_USERMGMT ),
	IMESSAGE_USER_TRUSTMGMT = MKINTERNAL( MESSAGE_USER_TRUSTMGMT ),
	IMESSAGE_LAST = MKINTERNAL( MESSAGE_LAST )
	} MESSAGE_TYPE;

/* The properties that MESSAGE_COMPARE can compare */

typedef enum {
	MESSAGE_COMPARE_NONE,			/* No comparison type */
	MESSAGE_COMPARE_HASH,			/* Compare hash/MAC value */
	MESSAGE_COMPARE_ICV,			/* Compare ICV value */
	MESSAGE_COMPARE_KEYID,			/* Compare key IDs */
	MESSAGE_COMPARE_KEYID_PGP,		/* Compare PGP key IDs */
	MESSAGE_COMPARE_KEYID_OPENPGP,	/* Compare OpenPGP key IDs */
	MESSAGE_COMPARE_SUBJECT,		/* Compare subject */
	MESSAGE_COMPARE_ISSUERANDSERIALNUMBER,	/* Compare iAndS */
	MESSAGE_COMPARE_SUBJECTKEYIDENTIFIER,	/* Compare subjectKeyIdentifier */
	MESSAGE_COMPARE_FINGERPRINT_SHA1,/* Compare cert.fingerprint */
	MESSAGE_COMPARE_FINGERPRINT_SHA2,/* Compare fingerprint, SHA2 */
	MESSAGE_COMPARE_FINGERPRINT_SHAng,/* Compare fingerprint, SHAng */
	MESSAGE_COMPARE_CERTOBJ,		/* Compare cert objects */
	MESSAGE_COMPARE_LAST			/* Last possible compare type */
	} MESSAGE_COMPARE_TYPE;

/* The checks that MESSAGE_CHECK performs.  There are a number of variations 
   of the checking we can perform, either the object is initialised in a 
   state to perform the required action (meaning that it has to be in the 
   high state), the object is ready to be initialised for the required 
   action, for example an encryption context about to have a key loaded for 
   encryption (meaning that it has to be in the low state), or the check is 
   on a passive container object that constrains another object (for example 
   a cert being attached to a context) for which the state isn't important 
   in this instance.  Usually we check to make sure that the cert is in the 
   high state, but when a cert is being created/imported it may not be in 
   the high state yet at the time the check is being carried out */

typedef enum {
	/* Standard checks, for which the object must be initialised in a state 
	   to perform this operation */
	MESSAGE_CHECK_NONE,				/* No check type */
	MESSAGE_CHECK_PKC,				/* Public or private key context */
	MESSAGE_CHECK_PKC_PRIVATE,		/* Private key context */
	MESSAGE_CHECK_PKC_ENCRYPT,		/* Public encryption context */
	MESSAGE_CHECK_PKC_DECRYPT,		/* Private decryption context */
	MESSAGE_CHECK_PKC_SIGCHECK,		/* Public signature check context */
	MESSAGE_CHECK_PKC_SIGN,			/* Private signature context */
	MESSAGE_CHECK_PKC_KA_EXPORT,	/* Key agreement - export context */
	MESSAGE_CHECK_PKC_KA_IMPORT,	/* Key agreement - import context */
	MESSAGE_CHECK_CRYPT,			/* Conventional encryption context */
	MESSAGE_CHECK_HASH,				/* Hash context */
	MESSAGE_CHECK_MAC,				/* MAC context */

	/* Checks that an object is ready to be initialised to perform this 
	   operation */
	MESSAGE_CHECK_CRYPT_READY,		/* Ready for conv.encr. init */
	MESSAGE_CHECK_MAC_READY,		/* Ready for MAC init */
	MESSAGE_CHECK_KEYGEN_READY,		/* Ready for key generation */

	/* Checks on purely passive container objects that constrain action 
	   objects */
	MESSAGE_CHECK_PKC_ENCRYPT_AVAIL,/* Encryption available */
	MESSAGE_CHECK_PKC_DECRYPT_AVAIL,/* Decryption available */
	MESSAGE_CHECK_PKC_SIGCHECK_AVAIL,	/* Signature check available */
	MESSAGE_CHECK_PKC_SIGN_AVAIL,	/* Signature available */
	MESSAGE_CHECK_PKC_KA_EXPORT_AVAIL,	/* Key agreement - export available */
	MESSAGE_CHECK_PKC_KA_IMPORT_AVAIL,	/* Key agreement - import available */

	/* Misc.checks for meta-capabilities not directly connected with object 
	   actions.  The certificate check is used to verify that a certificate 
	   is generally valid (for example not expired) without having to 
	   performing a full signature verification up to a trusted root, this 
	   is used to verify certificates passed in as parameters (for example 
	   server certificates) to ensure that the client doesn't get invalid 
	   data back when it tries to connect to the server.  The CA check 
	   applies to both PKC contexts and certificates, when the message is 
	   forwarded to a dependent CA cert it's forwarded as the internal 
	   MESSAGE_CHECK_CACERT check type, since MESSAGE_CHECK_CA requires both 
	   a PKC context and a certificate */
	MESSAGE_CHECK_CERT,				/* Generic certificate */
	MESSAGE_CHECK_CERTxx,			/* Internal value used for cert.checks */
	MESSAGE_CHECK_CA,				/* Cert signing capability */
	MESSAGE_CHECK_CACERT,			/* Internal value used for CA checks */
	MESSAGE_CHECK_LAST				/* Last possible check type */
	} MESSAGE_CHECK_TYPE;

/* The notifications that a MESSAGE_CHANGENOTIFY can deliver */

typedef enum {
	MESSAGE_CHANGENOTIFY_NONE,		/* No notification type */
	MESSAGE_CHANGENOTIFY_STATE,		/* Object should save/rest.int.state */
	MESSAGE_CHANGENOTIFY_OBJHANDLE,	/* Object cloned, handle changed */
	MESSAGE_CHANGENOTIFY_OWNERHANDLE,	/* Object cloned, owner handle changed */
	MESSAGE_CHANGENOTIFY_LAST		/* Last possible notification type */
	} MESSAGE_CHANGENOTIFY_TYPE;

/* The operations that a MESSAGE_USER_USERMGMT can perform */

typedef enum {
	MESSAGE_USERMGMT_NONE,			/* No operation type */
	MESSAGE_USERMGMT_ZEROISE,		/* Zeroise users */
	MESSAGE_USERMGMT_LAST			/* Last possible operation type */
	} MESSAGE_USERMGMT_TYPE;

/* The operations that a MESSAGE_USER_TRUSTMGMT can perform */

typedef enum {
	MESSAGE_TRUSTMGMT_NONE,			/* No operation type */
	MESSAGE_TRUSTMGMT_ADD,			/* Add trusted cert */
	MESSAGE_TRUSTMGMT_DELETE,		/* Delete trusted cert */
	MESSAGE_TRUSTMGMT_CHECK,		/* Check trust status of cert */
	MESSAGE_TRUSTMGMT_GETISSUER,	/* Get trusted issuer cert */
	MESSAGE_TRUSTMGMT_LAST			/* Last possible operation type */
	} MESSAGE_TRUSTMGMT_TYPE;

/* Options for the MESSAGE_SETDEPENDENT message */

typedef enum {
	SETDEP_OPTION_NONE,				/* No option type */
	SETDEP_OPTION_NOINCREF,			/* Don't inc.dep.objs reference count */
	SETDEP_OPTION_INCREF,			/* Increment dep.objs reference count */
	SETDEP_OPTION_LAST				/* Last possible option type */
	} MESSAGE_SETDEPENDENT_TYPE;

/* When getting/setting string data that consists of (value, length) pairs, 
   we pass a pointer to a value-and-length structure rather than a pointer 
   to the data itself */

typedef struct {
	BUFFER_FIXED( length ) \
	void *data;							/* Data */
	int length;							/* Length */
	} MESSAGE_DATA;

#define setMessageData( msgDataPtr, dataPtr, dataLength ) \
	{ \
	( msgDataPtr )->data = ( dataPtr ); \
	( msgDataPtr )->length = ( dataLength ); \
	}

/* When passing constant data to krnlSendMessage() we get compiler warnings 
   because the function is prototyped as taking a 'void *'.  The following 
   symbolic value for use in casts corrects the parameter mismatch */

typedef void *MESSAGE_CAST;

/* Some messages communicate standard data values that are used again and 
   again, so we predefine values for these that can be used globally */

#define MESSAGE_VALUE_TRUE			( ( MESSAGE_CAST ) &messageValueTrue )
#define MESSAGE_VALUE_FALSE			( ( MESSAGE_CAST ) &messageValueFalse )
#define MESSAGE_VALUE_OK			( ( MESSAGE_CAST ) &messageValueCryptOK )
#define MESSAGE_VALUE_ERROR			( ( MESSAGE_CAST ) &messageValueCryptError )
#define MESSAGE_VALUE_UNUSED		( ( MESSAGE_CAST ) &messageValueCryptUnused )
#define MESSAGE_VALUE_DEFAULT		( ( MESSAGE_CAST ) &messageValueCryptUseDefault )
#define MESSAGE_VALUE_CURSORFIRST	( ( MESSAGE_CAST ) &messageValueCursorFirst )
#define MESSAGE_VALUE_CURSORNEXT	( ( MESSAGE_CAST ) &messageValueCursorNext )
#define MESSAGE_VALUE_CURSORPREVIOUS ( ( MESSAGE_CAST ) &messageValueCursorPrevious )
#define MESSAGE_VALUE_CURSORLAST	( ( MESSAGE_CAST ) &messageValueCursorLast )

extern const int messageValueTrue, messageValueFalse;
extern const int messageValueCryptOK, messageValueCryptError;
extern const int messageValueCryptUnused, messageValueCryptUseDefault;
extern const int messageValueCursorFirst, messageValueCursorNext;
extern const int messageValueCursorPrevious, messageValueCursorLast;

/* Check for membership within an attribute class */

#define isAttribute( attribute ) \
	( ( attribute ) > CRYPT_ATTRIBUTE_NONE && \
	  ( attribute ) < CRYPT_ATTRIBUTE_LAST )
#define isInternalAttribute( attribute ) \
	( ( attribute ) > CRYPT_IATTRIBUTE_FIRST && \
	  ( attribute ) < CRYPT_IATTRIBUTE_LAST )

/* Check whether a message is in a given message class, used in object 
   message handlers */

#define isAttributeMessage( message ) \
	( ( message ) >= MESSAGE_GETATTRIBUTE && \
	  ( message ) <= MESSAGE_DELETEATTRIBUTE )
#define isActionMessage( message ) \
	( ( message ) >= MESSAGE_CTX_ENCRYPT && \
	  ( message ) <= MESSAGE_CTX_HASH )
#define isMechanismActionMessage( message ) \
	( ( message ) >= MESSAGE_DEV_EXPORT && \
	  ( message ) <= MESSAGE_DEV_KDF )

/* The following handles correspond to built-in fixed object types that are 
   available throughout the architecture.  Currently there are two objects, 
   an internal system object that encapsulates the built-in RNG and the 
   built-in mechanism types (if this ever becomes a bottleneck the two can be 
   separated into different objects) and a default user object which is used 
   when there are no explicit user objects being employed */

#define SYSTEM_OBJECT_HANDLE	0	/* Internal system object */
#define DEFAULTUSER_OBJECT_HANDLE 1	/* Default user object */

#define NO_SYSTEM_OBJECTS		2	/* Total number of system objects */

/* We limit the maximum number of objects to a sensible value to prevent 
   deliberate/accidental DoS attacks.  The following represents about 32MB 
   of object data, which should be a good indication that there are more 
   objects present than there should be */

#define MAX_OBJECTS				16384

/****************************************************************************
*																			*
*							Action Message Types							*
*																			*
****************************************************************************/

/* Action messages come in two types, direct action messages and mechanism-
   action messages.  Action messages apply directly to action objects (for 
   example transform a block of data) while mechanism-action messages apply 
   to device objects and involve extra formatting above and beyond the 
   direct action (for example perform PKCS #1 padding and then transform a 
   block of data).

   Each object that processes direct action messages can can have a range of 
   permission settings that control how action messages sent to it are 
   handled.  The most common case is that the action isn't available for 
   this object, ACTION_PERM_NOTAVAIL.  This is an all-zero permission, so 
   the default is deny-all unless the action is explicitly permitted.  The 
   other permissions are ACTION_PERM_NONE, which means that the action is in 
   theory available but has been turned off, ACTION_PERM_NONE_EXTERNAL, 
   which means that the action is only valid if the message is coming from 
   inside cryptlib, and ACTION_PERM_ALL, which means that the action is 
   available for anyone.  In order to set all permissions to a certain value 
   (e.g. NONE_EXTERNAL), we define overall values xxx_ALL that (in 
   combination with the kernel-enforced ratchet) can be used to set all 
   settings to (at most) the given value.

   The order of the action bits is:

	  hash   sign  encr
		|	  |		|
	xx xx xx xx xx xx
	 |	   |	 |
	kgen sigch  decr

    x. .x|x. .x|x. .x	Hex digits

   Common settings are 0xCFF (new PKC, all operations), 0x0F (key-loaded 
   conv., all operations), and 0xAA (key-loaded PKC, internal-only 
   operations).

   The kernel enforces a ratchet for these setting that only allows them to 
   be set to a more restrictive value than their existing one.  If a setting 
   starts out as not available on object creation, it can never be enabled. 
   If a setting starts as 'none-external', it can only be set to a straight 
   'none', but never to 'all' */

#define ACTION_PERM_NOTAVAIL		0x00
#define ACTION_PERM_NONE			0x01
#define ACTION_PERM_NONE_EXTERNAL	0x02
#define ACTION_PERM_ALL				0x03

#define ACTION_PERM_NONE_ALL			0x000
#define ACTION_PERM_NONE_EXTERNAL_ALL	0xAAA
#define ACTION_PERM_ALL_MAX				0xFFF

#define ACTION_PERM_BASE	MESSAGE_CTX_ENCRYPT
#define ACTION_PERM_MASK	0x03
#define ACTION_PERM_BITS	2
#define ACTION_PERM_COUNT	( MESSAGE_CTX_GENKEY - \
							  MESSAGE_CTX_ENCRYPT + 1 )
#define ACTION_PERM_LAST	\
		( 1 << ( ( ( ACTION_PERM_COUNT ) * ACTION_PERM_BITS ) + 1 ) )
#define ACTION_PERM_SHIFT( action ) \
		( ( ( action ) - ACTION_PERM_BASE ) * ACTION_PERM_BITS )
#define MK_ACTION_PERM( action, perm ) \
		( ( perm ) << ACTION_PERM_SHIFT( action ) )
#define MK_ACTION_PERM_NONE_EXTERNAL( action ) \
		( ( action ) & ACTION_PERM_NONE_EXTERNAL_ALL )

/* Symbolic defines to allow the action flags to be range-checked alongside 
   all of the other flag types */

#define ACTION_PERM_FLAG_NONE		0x000
#define ACTION_PERM_FLAG_MAX		0xFFF

/* The mechanism types.  The distinction between the PKCS #1 and raw PKCS #1 
   mechanisms is somewhat artificial in that they do the same thing, however 
   it's easier for the kernel to perform security checks on parameters if 
   the two are distinct */

typedef enum {
	MECHANISM_NONE,				/* No mechanism type */
	MECHANISM_ENC_PKCS1,		/* PKCS #1 en/decrypt */
	MECHANISM_ENC_PKCS1_PGP,	/* PKCS #1 using PGP formatting */
	MECHANISM_ENC_PKCS1_RAW,	/* PKCS #1 returning uninterpreted data */
	MECHANISM_ENC_OAEP,			/* OAEP en/decrypt */
	MECHANISM_ENC_CMS,			/* CMS key wrap */
	MECHANISM_SIG_PKCS1,		/* PKCS #1 sign */
	MECHANISM_SIG_SSL,			/* SSL sign with dual hashes */
	MECHANISM_DERIVE_PKCS5,		/* PKCS #5 derive */
	MECHANISM_DERIVE_PKCS12,	/* PKCS #12 derive */
	MECHANISM_DERIVE_SSL,		/* SSL derive */
	MECHANISM_DERIVE_TLS,		/* TLS derive */
	MECHANISM_DERIVE_TLS12,		/* TLS 1.2 derive */
	MECHANISM_DERIVE_CMP,		/* CMP/Entrust derive */
	MECHANISM_DERIVE_PGP,		/* OpenPGP S2K derive */
	MECHANISM_PRIVATEKEYWRAP,	/* Private key wrap */
	MECHANISM_PRIVATEKEYWRAP_PKCS8,	/* PKCS #8 private key wrap */
	MECHANISM_PRIVATEKEYWRAP_PGP2,	/* PGP 2.x private key wrap */
	MECHANISM_PRIVATEKEYWRAP_OPENPGP_OLD,/* Legacy OpenPGP private key wrap */
	MECHANISM_PRIVATEKEYWRAP_OPENPGP,/* OpenPGP private key wrap */
	MECHANISM_LAST				/* Last possible mechanism type */
	} MECHANISM_TYPE;

/* A structure to hold information needed by the key export/import mechanism. 
   The key can be passed as raw key data or as a context if tied to hardware 
   that doesn't allow keying material outside the hardware's security 
   perimeter:

	PKCS #1,	wrappedData = wrapped key
	PKCS #1 PGP	keyData = -
				keyContext = context containing key
				wrapContext = wrap/unwrap PKC context
				auxContext = CRYPT_UNUSED
				auxInfo = CRYPT_UNUSED
	PKCS #1	raw	wrappedData = wrapped raw data
				keyData = raw data
				keyContext = CRYPT_UNUSED
				wrapContext = wrap/unwrap PKC context
				auxContext = CRYPT_UNUSED
				auxInfo = CRYPT_UNUSED
	OAEP		wrappedData = wrapped key
				keyData = -
				keyContext = context containing key
				wrapContext = wrap/unwrap PKC context
				auxContext = CRYPT_UNUSED
				auxInfo = MGF1 algorithm
	CMS			wrappedData = wrapped key
				keyData = -
				keyContext = context containing key
				wrapContext = wrap/unwrap conventional context
				auxContext = CRYPT_UNUSED
				auxInfo = CRYPT_UNUSED
	Private		wrappedData = padded encrypted private key components
	key wrap	keyData = -
				keyContext = context containing private key
				wrapContext = wrap/unwrap conventional context
				auxContext = CRYPT_UNUSED 
				auxInfo = CRYPT_UNUSED */

typedef struct {
	BUFFER_OPT_FIXED( wrappedDataLength ) \
		void *wrappedData;					/* Wrapped key */
	int wrappedDataLength;
	BUFFER_FIXED( keyDataLength ) \
		void *keyData;						/* Raw key */
	int keyDataLength;
	VALUE_HANDLE CRYPT_HANDLE keyContext;	/* Context containing raw key */
	VALUE_HANDLE CRYPT_HANDLE wrapContext;	/* Wrap/unwrap context */
	VALUE_HANDLE CRYPT_HANDLE auxContext;	/* Auxiliary context */
	int auxInfo;							/* Auxiliary info */
	} MECHANISM_WRAP_INFO;

/* A structure to hold information needed by the sign/sig check mechanism: 

	PKCS #1		signature = signature
				hashContext = hash to sign
				signContext = signing key

	SSL			signature = signature
				hashContext, hashContext2 = dual hashes to sign
				signContext = signing key */

typedef struct {
	BUFFER_OPT_FIXED( signatureLength ) \
		void *signature;					/* Signature */
	int signatureLength;
	VALUE_HANDLE CRYPT_CONTEXT hashContext;	/* Hash context */
	VALUE_HANDLE CRYPT_CONTEXT hashContext2;/* Secondary hash context */
	VALUE_HANDLE CRYPT_HANDLE signContext;	/* Signing context */
	} MECHANISM_SIGN_INFO;

/* A structure to hold information needed by the key derivation mechanism. 
   This differs from the KDF mechanism that follows in that the "Derive" 
   form is used with an iteration count for password-based key derivation 
   while the "KDF" form is used without an iteration count for straight key 
   derivation from a master secret.  An exception to this is the SSL/TLS 
   derivation, which uses the resulting data in so many ways that it's not 
   possible to perform a generic context -> context transformation but 
   requires a generalisation in which the raw output data is available: 

	PKCS #5,	dataOut = key data
	CMP, PGP	dataIn = password
				hashAlgo = hash algorithm
				salt = salt
				iterations = iteration count
	SSL/TLS/	dataOut = key data/master secret
		TLS 1.2	dataIn = master secret/premaster secret
				hashAlgo = CRYPT_USE_DEFAULT
				salt = client || server random/server || client random
				iterations = 1 */

typedef struct {
	BUFFER_OPT_FIXED( dataOutLength ) \
		void *dataOut;						/* Output keying information */
	int dataOutLength;
	BUFFER_FIXED( dataInLength ) \
		const void *dataIn;					/* Input keying information */
	int dataInLength;
	VALUE_HANDLE CRYPT_ALGO_TYPE hashAlgo;	/* Hash algorithm */
	int hashParam;							/* Optional algorithm parameters */
	BUFFER_OPT_FIXED( saltLength ) \
		const void *salt;					/* Salt/randomiser */
	int saltLength;
	int iterations;							/* Iterations of derivation function */
	} MECHANISM_DERIVE_INFO;

/* A structure to hold information needed by the KDF mechanism:

	CMS AuthEnc	keyContext = encryption/MAC context 
				masterKeyContext = context containing master secret 
				hashAlgo = hash algorithm
				salt = salt */

typedef struct {
	VALUE_HANDLE CRYPT_HANDLE keyContext;	/* Context containing derived key */
	VALUE_HANDLE CRYPT_HANDLE masterKeyContext;/* Context containing master key */
	VALUE_HANDLE CRYPT_ALGO_TYPE hashAlgo;	/* Hash algorithm */
	int hashParam;							/* Optional algorithm parameters */
	BUFFER_OPT_FIXED( saltLength ) \
		const void *salt;					/* Salt/randomiser */
	int saltLength;
	} MECHANISM_KDF_INFO;

/* Macros to make it easier to work with the mechanism info types.  The 
   shortened name forms in the macro args are necessary to avoid clashes with 
   the struct members.  The long lines are necessary because older Borland 
   compilers can't handle line breaks at this point in a macro definition */

#define clearMechanismInfo( mechanismInfo ) \
		memset( mechanismInfo, 0, sizeof( *mechanismInfo ) )

#define setMechanismWrapInfo( mechanismInfo, wrapped, wrappedLen, key, keyLen, keyCtx, wrapCtx ) \
		{ \
		memset( mechanismInfo, 0, sizeof( MECHANISM_WRAP_INFO ) ); \
		( mechanismInfo )->wrappedData = ( wrapped ); \
		( mechanismInfo )->wrappedDataLength = ( wrappedLen ); \
		( mechanismInfo )->keyData = ( key ); \
		( mechanismInfo )->keyDataLength = ( keyLen ); \
		( mechanismInfo )->keyContext = ( keyCtx ); \
		( mechanismInfo )->wrapContext = ( wrapCtx ); \
		( mechanismInfo )->auxContext = \
			( mechanismInfo )->auxInfo = CRYPT_UNUSED; \
		ANALYSER_HINT( isHandleRangeValid( ( mechanismInfo )->keyContext ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

#define setMechanismWrapInfoEx( mechanismInfo, wrapped, wrappedLen, key, keyLen, keyCtx, wrapCtx, auxCtx, auxInf ) \
		{ \
		memset( mechanismInfo, 0, sizeof( MECHANISM_WRAP_INFO ) ); \
		( mechanismInfo )->wrappedData = ( wrapped ); \
		( mechanismInfo )->wrappedDataLength = ( wrappedLen ); \
		( mechanismInfo )->keyData = ( key ); \
		( mechanismInfo )->keyDataLength = ( keyLen ); \
		( mechanismInfo )->keyContext = ( keyCtx ); \
		( mechanismInfo )->wrapContext = ( wrapCtx ); \
		( mechanismInfo )->auxContext = ( auxCtx ); \
		( mechanismInfo )->auxInfo = ( auxInf ); \
		}

#define setMechanismSignInfo( mechanismInfo, sig, sigLen, hashCtx, hashCtx2, signCtx ) \
		{ \
		memset( mechanismInfo, 0, sizeof( MECHANISM_SIGN_INFO ) ); \
		( mechanismInfo )->signature = ( sig ); \
		( mechanismInfo )->signatureLength = ( sigLen ); \
		( mechanismInfo )->hashContext = ( hashCtx ); \
		( mechanismInfo )->hashContext2 = ( hashCtx2 ); \
		( mechanismInfo )->signContext = ( signCtx ); \
		}

#define setMechanismDeriveInfo( mechanismInfo, out, outLen, in, inLen, hAlgo, slt, sltLen, iters ) \
		{ \
		memset( mechanismInfo, 0, sizeof( MECHANISM_DERIVE_INFO ) ); \
		( mechanismInfo )->dataOut = ( out ); \
		( mechanismInfo )->dataOutLength = ( outLen ); \
		( mechanismInfo )->dataIn = ( in ); \
		( mechanismInfo )->dataInLength = ( inLen ); \
		( mechanismInfo )->hashAlgo = ( hAlgo ); \
		( mechanismInfo )->salt = ( slt ); \
		( mechanismInfo )->saltLength = ( sltLen ); \
		( mechanismInfo )->iterations = ( iters ); \
		}

#define setMechanismKDFInfo( mechanismInfo, keyCtx, masterKeyCtx, hAlgo, slt, sltLen ) \
		{ \
		memset( mechanismInfo, 0, sizeof( MECHANISM_KDF_INFO ) ); \
		( mechanismInfo )->keyContext = ( keyCtx ); \
		( mechanismInfo )->masterKeyContext = ( masterKeyCtx ); \
		( mechanismInfo )->hashAlgo = ( hAlgo ); \
		( mechanismInfo )->salt = ( slt ); \
		( mechanismInfo )->saltLength = ( sltLen ); \
		}

/****************************************************************************
*																			*
*								Misc Message Types							*
*																			*
****************************************************************************/

/* Beside the general value+length and mechanism messages, we also have a 
   number of special-purposes messages that require their own parameter 
   data structures.  These are:

   Create object messages, used to create objects via a device, either 
   directly or indirectly by instantiating the object from encoded data (for 
   example a certificate object from a certificate).  Usually the device is 
   the system object, but it can also be used to create contexts in hardware 
   devices.  In addition to the creation parameters we also pass in the 
   owner's user object to be stored with the object data for use when 
   needed.
   
   For the create-indirect certificate case, the arguments are:

	arg1		Certificate object type.
	arg2		Optional certificate object ID type, used as an indicator of 
				the primary object in a collection like a certificate chain 
				or set.
	arg3		Optional KEYMGMT_FLAGs.
	strArg1		Certificate object data.
	strArg2		Optional certificate object ID (see arg2) */

typedef struct {
	VALUE_HANDLE CRYPT_HANDLE cryptHandle;	/* Handle to created object */
	VALUE_HANDLE CRYPT_USER cryptOwner;		/* New object's owner */
	int arg1, arg2, arg3;					/* Integer args */
	BUFFER_OPT_FIXED( strArgLen1 ) \
		const void *strArg1;
	BUFFER_OPT_FIXED( strArgLen2 ) \
		const void *strArg2;				/* String args */
	VALUE_INT int strArgLen1;
	VALUE_INT_SHORT int strArgLen2;
	} MESSAGE_CREATEOBJECT_INFO;

#define setMessageCreateObjectInfo( createObjectInfo, a1 ) \
		{ \
		memset( createObjectInfo, 0, sizeof( MESSAGE_CREATEOBJECT_INFO ) ); \
		( createObjectInfo )->cryptHandle = CRYPT_ERROR; \
		( createObjectInfo )->cryptOwner = CRYPT_ERROR; \
		( createObjectInfo )->arg1 = ( a1 ); \
		ANALYSER_HINT( isHandleRangeValid( ( createObjectInfo )->cryptHandle ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

#define setMessageCreateObjectIndirectInfo( createObjectInfo, data, dataLen, type ) \
		{ \
		memset( createObjectInfo, 0, sizeof( MESSAGE_CREATEOBJECT_INFO ) ); \
		( createObjectInfo )->cryptHandle = CRYPT_ERROR; \
		( createObjectInfo )->cryptOwner = CRYPT_ERROR; \
		( createObjectInfo )->strArg1 = ( data ); \
		( createObjectInfo )->strArgLen1 = ( dataLen ); \
		( createObjectInfo )->arg1 = ( type ); \
		ANALYSER_HINT( isHandleRangeValid( ( createObjectInfo )->cryptHandle ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

#define setMessageCreateObjectIndirectInfoEx( createObjectInfo, data, dataLen, type, options ) \
		{ \
		memset( createObjectInfo, 0, sizeof( MESSAGE_CREATEOBJECT_INFO ) ); \
		( createObjectInfo )->cryptHandle = CRYPT_ERROR; \
		( createObjectInfo )->cryptOwner = CRYPT_ERROR; \
		( createObjectInfo )->strArg1 = ( data ); \
		( createObjectInfo )->strArgLen1 = ( dataLen ); \
		( createObjectInfo )->arg1 = ( type ); \
		( createObjectInfo )->arg3 = ( options ); \
		ANALYSER_HINT( isHandleRangeValid( ( createObjectInfo )->cryptHandle ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

#define updateMessageCreateObjectIndirectInfo( createObjectInfo, idType, id, idLength ) \
		{ \
		( createObjectInfo )->arg2 = ( idType ); \
		( createObjectInfo )->strArg2 = ( id ); \
		( createObjectInfo )->strArgLen2 = ( idLength ); \
		}

/* Key management messages, used to set, get and delete keys.  The item type, 
   keyIDtype, keyID, and keyIDlength are mandatory, the aux.info depends on 
   the type of message (optional password for private key get/set, state 
   information for get next cert, null otherwise), and the flags are 
   generally only required where the keyset can hold multiple types of keys 
   (for example a crypto device acting as a keyset, or a PKCS #15 token).

   An itemType of KEYMGMT_ITEM_PUBLICKEY is somewhat more general than its 
   name implies in that keysets/devices that store private key information 
   alongside public-key data may delete both types of items if asked to 
   delete a KEYMGMT_ITEM_PUBLICKEY since the two items are (implicitly) 
   connected.

   An itemType of KEYMGMT_ITEM_REQUEST is distinct from the subtype 
   KEYMGMT_ITEM_REVREQUEST because the former is for certification requests 
   while the latter is for revocation requests, the distinction is made 
   because some protocols only allow certification but not revocation 
   requests and we want to trap these as soon as possible rather than some 
   way down the road when error reporting becomes a lot more nonspecific.

   An itemType of KEYMGMT_ITEM_KEYMETADATA is a KEYMGMT_ITEM_PRIVATEKEY with 
   the context passed in being a dummy context with actual keying data held 
   in a crypto device.  What's stored in the keyset is purely the 
   surrounding metadata like labels, dates, and ID information.  This is 
   distinct from a KEYMGMT_ITEM_PRIVATEKEY both to allow for better checking 
   by the kernel, since a KEYMGMT_ITEM_KEYMETADATA object may not be in the 
   high state yet when it's used and doens't need a password like a 
   KEYMGMT_ITEM_PRIVATEKEY does.  In addition keeping the types distinct is 
   a safety feature since otherwise we'd need to allow a special-case 
   KEYMGMT_ITEM_PRIVATEKEY store without a password, which is just asking 
   for trouble.  Note that KEYMGMT_ITEM_PRIVATEKEY items are write-only, for 
   reads they're treated the same as KEYMGMT_ITEM_PRIVATEKEY except that a 
   dummy context is created.

   In addition to the flags that are used to handle various special-case 
   read accesses, we can also specify a usage preference (e.g. 
   confidentiality vs.signature) for cases where we may have multiple keys 
   with the same keyID that differ only in required usage */

typedef enum {
	KEYMGMT_ITEM_NONE,			/* No item type */
	KEYMGMT_ITEM_PUBLICKEY,		/* Access public key */
	KEYMGMT_ITEM_PRIVATEKEY,	/* Access private key */
	KEYMGMT_ITEM_SECRETKEY,		/* Access secret key */
	KEYMGMT_ITEM_REQUEST,		/* Access cert request */
	KEYMGMT_ITEM_REVREQUEST,	/* Access cert revocation request */
	KEYMGMT_ITEM_PKIUSER,		/* Access PKI user info */
	KEYMGMT_ITEM_REVOCATIONINFO,/* Access revocation info/CRL */
	KEYMGMT_ITEM_KEYMETADATA,	/* Access key metadata for dummy ctx.*/
	KEYMGMT_ITEM_DATA,			/* Other data (for PKCS #15 tokens) */
	KEYMGMT_ITEM_LAST			/* Last possible item type */
	} KEYMGMT_ITEM_TYPE;

#define KEYMGMT_FLAG_NONE			0x0000	/* No flag type */
#define KEYMGMT_FLAG_CHECK_ONLY		0x0001	/* Perform existence check only */
#define KEYMGMT_FLAG_LABEL_ONLY		0x0002	/* Get key label only */
#define KEYMGMT_FLAG_UPDATE			0x0004	/* Update existing (allow dups) */
#define KEYMGMT_FLAG_DATAONLY_CERT	0x0008	/* Create data-only certificate */
#define KEYMGMT_FLAG_CERT_AS_CERTCHAIN 0x010 /* Force creation of cert.chain
												even if single cert.present */
#define KEYMGMT_FLAG_USAGE_CRYPT	0x0020	/* Prefer encryption key */
#define KEYMGMT_FLAG_USAGE_SIGN		0x0040	/* Prefer signature key */
#define KEYMGMT_FLAG_GETISSUER		0x0080	/* Get issuing PKI user for cert */
#define KEYMGMT_FLAG_INITIALOP		0x0100	/* Initial cert issue operation */
#define KEYMGMT_FLAG_MAX			0x01FF	/* Maximum possible flag value */

#define KEYMGMT_MASK_USAGEOPTIONS	( KEYMGMT_FLAG_USAGE_CRYPT | \
									  KEYMGMT_FLAG_USAGE_SIGN )
#define KEYMGMT_MASK_CERTOPTIONS	( KEYMGMT_FLAG_DATAONLY_CERT | \
									  KEYMGMT_FLAG_CERT_AS_CERTCHAIN | \
									  KEYMGMT_FLAG_USAGE_CRYPT | \
									  KEYMGMT_FLAG_USAGE_SIGN )
typedef struct {
	VALUE_HANDLE CRYPT_HANDLE cryptHandle;	/* Returned key */
	VALUE_HANDLE CRYPT_KEYID_TYPE keyIDtype;/* Key ID type */
	BUFFER_OPT_FIXED( keyIDlength ) \
		const void *keyID;					/* Key ID */
	VALUE_INT_SHORT int keyIDlength;
	BUFFER_OPT_FIXED( auxInfoLength ) \
		void *auxInfo;						/* Aux.info (e.g.password for private key) */
	VALUE_INT_SHORT int auxInfoLength;
	int flags;								/* Access options */
	} MESSAGE_KEYMGMT_INFO;

#define setMessageKeymgmtInfo( keymgmtInfo, idType, id, idLength, aux, auxLen, keyFlags ) \
		{ \
		( keymgmtInfo )->cryptHandle = CRYPT_ERROR; \
		( keymgmtInfo )->keyIDtype = ( idType ); \
		( keymgmtInfo )->keyID = ( id ); \
		( keymgmtInfo )->keyIDlength = ( idLength ); \
		( keymgmtInfo )->auxInfo = ( aux ); \
		( keymgmtInfo )->auxInfoLength = ( auxLen ); \
		( keymgmtInfo )->flags = ( keyFlags ); \
		ANALYSER_HINT( isHandleRangeValid( ( keymgmtInfo )->cryptHandle ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

/* Cert management messages used to handle CA operations.  All fields are 
   mandatory, however the cryptCert and request fields may be set to 
   CRYPT_UNUSED to indicate 'don't care' conditions */

typedef struct {
	CRYPT_CERTIFICATE cryptCert;	/* Returned cert */
	CRYPT_CONTEXT caKey;			/* CA key to sign item */
	CRYPT_CERTIFICATE request;		/* Request for operation */
	} MESSAGE_CERTMGMT_INFO;

#define setMessageCertMgmtInfo( certMgmtInfo, mgmtCaKey, mgmtRequest ) \
		{ \
		( certMgmtInfo )->cryptCert = CRYPT_ERROR; \
		( certMgmtInfo )->caKey = ( mgmtCaKey ); \
		( certMgmtInfo )->request = ( mgmtRequest ); \
		ANALYSER_HINT( isHandleRangeValid( ( certMgmtInfo )->cryptCert ) ); \
		}			   /* Hack for unimplemented VALUE() op.in VS 2013 */

/****************************************************************************
*																			*
*								Kernel Functions							*
*																			*
****************************************************************************/

/* cryptlib initialistion/shutdown functions */

CHECK_RETVAL \
int initCryptlib( void );
CHECK_RETVAL \
int endCryptlib( void );
#if defined( __PALMOS__ ) || defined( __WIN32__ ) || defined( __WINCE__ )
  void preInit( void );
  void postShutdown( void );
#endif /* Systems with OS-specific pre-init/post-shutdown facilities */

/* Prototype for an object's message-handling function */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
typedef int ( *MESSAGE_FUNCTION )( INOUT void *objectInfoPtr,
								   IN_ENUM( MESSAGE ) \
										const MESSAGE_TYPE message,
								   void *messageDataPtr,
								   const int messageValue );

/* If the message-handler can unlock an object to allow other threads 
   access, it has to be able to inform the kernel of this.  The following 
   structure and macros allow this information to be passed back to the 
   kernel via the message function's objectInfoPtr */

typedef struct {
	void *objectInfoPtr;					/* Original objectInfoPtr */
	BOOLEAN isUnlocked;						/* Whether obj.is now unlocked */
	} MESSAGE_FUNCTION_EXTINFO;

#define initMessageExtInfo( messageExtInfo, objectInfo ) \
		{ \
		memset( messageExtInfo, 0, sizeof( MESSAGE_FUNCTION_EXTINFO ) ); \
		( messageExtInfo )->objectInfoPtr = objectInfo; \
		}
#define setMessageObjectLocked( messageExtInfo ) \
		( messageExtInfo )->isUnlocked = FALSE
#define setMessageObjectUnlocked( messageExtInfo ) \
		( messageExtInfo )->isUnlocked = TRUE
#define isMessageObjectUnlocked( messageExtInfo ) \
		( ( messageExtInfo )->isUnlocked )

/* Object management functions.  A dummy object is one that exists but 
   doesn't have the capabilities of the actual object, for example an 
   encryption context that just maps to underlying crypto hardware.  This 
   doesn't affect krnlCreateObject(), but is used by the object-type-
   specific routines that decorate the results of krnlCreateObject() with 
   object-specific extras.
   
   Since krnlSendMessage() is the universal entry-point for the kernel API,
   it's heavily annotated to provide compile-time warnings of issues.  Since
   this is checked anyway at runtime, it's not clear whether this is really
   useful or just a gratuitous means of exercising the static analyser's 
   capabilities... */

#define CREATEOBJECT_FLAG_NONE		0x00	/* No create-object flags */
#define CREATEOBJECT_FLAG_SECUREMALLOC \
									0x01	/* Use krnlMemAlloc() to alloc.*/
#define CREATEOBJECT_FLAG_DUMMY		0x02	/* Dummy obj.used as placeholder */
#define CREATEOBJECT_FLAG_PERSISTENT 0x04	/* Obj.backed by key in device */
#define CREATEOBJECT_FLAG_MAX		0x0F	/* Maximum possible flag value */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 9 ) ) \
int krnlCreateObject( OUT_HANDLE_OPT int *objectHandle,
					  OUT_BUFFER_ALLOC_OPT( objectDataSize ) void **objectDataPtr, 
					  IN_LENGTH_SHORT const int objectDataSize,
					  IN_ENUM( OBJECT_TYPE ) const OBJECT_TYPE type, 
					  IN_ENUM( SUBTYPE ) const OBJECT_SUBTYPE subType,
					  IN_FLAGS_Z( CREATEOBJECT ) const int createObjectFlags, 
					  IN_HANDLE_OPT const CRYPT_USER owner,
					  IN_FLAGS_Z( ACTION_PERM ) const int actionFlags,
					  IN MESSAGE_FUNCTION messageFunction );
RETVAL \
PARAMCHECK_MESSAGE( MESSAGE_DESTROY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_INCREFCOUNT, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DECREFCOUNT, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_GETDEPENDENT, OUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_SETDEPENDENT, IN, IN ) \
PARAMCHECK_MESSAGE( MESSAGE_CLONE, PARAM_NULL, IN_HANDLE ) \
PARAMCHECK_MESSAGE( MESSAGE_GETATTRIBUTE, OUT, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_GETATTRIBUTE_S, INOUT, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_SETATTRIBUTE, IN, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_SETATTRIBUTE_S, IN, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_DELETEATTRIBUTE, PARAM_NULL, IN_ATTRIBUTE ) \
PARAMCHECK_MESSAGE( MESSAGE_COMPARE, IN, IN_ENUM( MESSAGE_COMPARE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CHECK, PARAM_NULL, IN_ENUM( MESSAGE_CHECK ) ) \
PARAMCHECK_MESSAGE( MESSAGE_SELFTEST, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CHANGENOTIFY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_ENCRYPT, INOUT, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_DECRYPT, INOUT, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_SIGN, IN, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_SIGCHECK, IN, IN_LENGTH ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_HASH, IN, IN_LENGTH_Z ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_GENKEY, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CTX_GENIV, PARAM_NULL, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_SIGN, PARAM_NULL, IN_HANDLE ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_SIGCHECK, PARAM_NULL, IN_HANDLE_OPT ) \
PARAMCHECK_MESSAGE( MESSAGE_CRT_EXPORT, INOUT, IN_ENUM( CRYPT_CERTFORMAT ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_QUERYCAPABILITY, OUT, IN_ALGO ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_EXPORT, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_IMPORT, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_SIGN, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_SIGCHECK, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_DERIVE, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_KDF, INOUT, IN_ENUM( MECHANISM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_CREATEOBJECT, INOUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_DEV_CREATEOBJECT_INDIRECT, INOUT, IN_ENUM( OBJECT_TYPE ) ) \
PARAMCHECK_MESSAGE( MESSAGE_ENV_PUSHDATA, INOUT, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_ENV_POPDATA, INOUT, PARAM_IS( 0 ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_SETKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_DELETEKEY, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETFIRSTCERT, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_GETNEXTCERT, INOUT, IN_ENUM( KEYMGMT_ITEM ) ) \
PARAMCHECK_MESSAGE( MESSAGE_KEY_CERTMGMT, INOUT, IN_ENUM( CRYPT_CERTACTION ) ) \
PARAMCHECK_MESSAGE( MESSAGE_USER_USERMGMT, INOUT, IN_ENUM( MESSAGE_USERMGMT ) ) \
PARAMCHECK_MESSAGE( MESSAGE_USER_TRUSTMGMT, IN, IN_ENUM( MESSAGE_TRUSTMGMT ) ) \
					/* Actually INOUT for MESSAGE_TRUSTMGMT_GETISSUER, but too \
					   complex to annotate */ \
int krnlSendMessage( IN_HANDLE const int objectHandle, 
					 IN_MESSAGE const MESSAGE_TYPE message,
					 void *messageDataPtr, const int messageValue );

/* Since some messages contain no data but act only as notifiers, we define 
   the following macro to make using them less messy */

#define krnlSendNotifier( handle, message ) \
		krnlSendMessage( handle, message, NULL, 0 )

/* In some rare cases we have to access an object directly without sending 
   it a message.  This happens either with certs where we're already 
   processing a message for one cert and need to access internal data in 
   another cert, when we're working with a crypto device tied to a context 
   where we need access to both context and device internals at the same 
   time, or when we're updating config data in a user object.  This type of 
   access is handled by the following function, which also explicitly 
   disallows any access types apart from the three described here */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3 ) ) \
int krnlAcquireObject( IN_HANDLE const int objectHandle, 
					   IN_ENUM( OBJECT_TYPE ) const OBJECT_TYPE type,
					   OUT_PTR_COND void **objectPtr, 
					   IN_ERROR const int errorCode );
RETVAL \
int krnlReleaseObject( IN_HANDLE const int objectHandle );

/* In even rarer cases, we have to allow a second thread access to an object 
   when another thread has it locked, providing a (somewhat crude) mechanism 
   for making kernel calls interruptible and resumable.  This only occurs in 
   two cases, for the system object when a background polling thread is 
   adding entropy to the system device and for the user object when we're 
   saving configuration data to persistent storage (we temporarily unlock it 
   to allow other threads access, since the act of writing the marshalled 
   data to storage doesn't require the user object) */

RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
int krnlSuspendObject( IN_HANDLE const int objectHandle, 
					   OUT_INT_Z int *refCount );
CHECK_RETVAL \
int krnlResumeObject( IN_HANDLE const int objectHandle, 
					  IN_INT_Z const int refCount );

/* When the kernel is closing down, any cryptlib-internal threads should 
   exit as quickly as possible.  For threads coming in from the outside this 
   happens automatically because any message it tries to send fails, but for 
   internal worker threads (for example the async driver-binding thread or 
   randomness polling threads) that don't perform many kernel calls, the 
   thread has to periodically check whether the kernel is still active.  The 
   following function is used to indicate whether the kernel is shutting 
   down */

CHECK_RETVAL_BOOL \
BOOLEAN krnlIsExiting( void );

/* Semaphores and mutexes */

typedef enum {
	SEMAPHORE_NONE,					/* No semaphore */
	SEMAPHORE_DRIVERBIND,			/* Async driver bind */
	SEMAPHORE_LAST					/* Last possible semaphore */
	} SEMAPHORE_TYPE;

typedef enum {
	MUTEX_NONE,						/* No mutex */
	MUTEX_SCOREBOARD,				/* Session scoreboard */
	MUTEX_SOCKETPOOL,				/* Network socket pool */
	MUTEX_RANDOM,					/* Randomness subsystem */
	MUTEX_LAST						/* Last possible mutex */
	} MUTEX_TYPE;

/* Execute a function in a background thread.  This takes a pointer to the 
   function to execute in the background thread, a block of memory for 
   thread state storage, a set of parameters to pass to the thread function, 
   and an optional semaphore ID to set once the thread is started.  A 
   function is run via a background thread as follows:

	void threadFunction( const THREAD_PARAMS *threadParams )
		{
		}

	krnlDispatchThread( threadFunction, &threadState, ptrParam, intParam, 
						SEMAPHORE_ID );

   Note that the thread parameters must be held in static storage because 
   the caller's stack frame may have long since disappeared before the 
   thread gets to access them */

struct TF;

typedef void ( *THREAD_FUNCTION )( const struct TF *threadParams );

typedef BYTE THREAD_STATE[ 48 ];

typedef struct TF {
	void *ptrParam;					/* Pointer parameter */
	int intParam;					/* Integer parameter */
	} THREAD_PARAMS;

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlDispatchThread( THREAD_FUNCTION threadFunction,
						THREAD_STATE threadState, void *ptrParam, 
						const int intParam, const SEMAPHORE_TYPE semaphore );

/* Wait on a semaphore, enter and exit a mutex */

CHECK_RETVAL_BOOL \
BOOLEAN krnlWaitSemaphore( IN_ENUM( SEMAPHORE ) const SEMAPHORE_TYPE semaphore );
CHECK_RETVAL \
int krnlEnterMutex( IN_ENUM( MUTEX ) const MUTEX_TYPE mutex );
void krnlExitMutex( IN_ENUM( MUTEX ) const MUTEX_TYPE mutex );

/* Secure memory handling functions */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemalloc( OUT_BUFFER_ALLOC_OPT( size ) void **pointer, 
				  IN_LENGTH int size );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int krnlMemfree( INOUT_PTR void **pointer );

#ifdef NEED_ENUMFIX
  #undef OBJECT_TYPE_LAST
  #undef MESSAGE_COMPARE_LAST
  #undef MESSAGE_CHECK_LAST
  #undef MESSAGE_CHANGENOTIFY_LAST
  #undef MECHANISM_LAST
  #undef KEYMGMT_ITEM_LAST
  #undef SEMAPHORE_LAST
  #undef MUTEX_LAST
#endif /* NEED_ENUMFIX */
#endif /* _CRYPTKRN_DEFINED */
