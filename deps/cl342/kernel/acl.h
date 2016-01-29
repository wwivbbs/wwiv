/****************************************************************************
*																			*
*								ACL Definitions								*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

#ifndef _ACL_DEFINED

#define _ACL_DEFINED

/****************************************************************************
*																			*
*							Object Type Information							*
*																			*
****************************************************************************/

/* Bit flags for specifying valid object subtypes.  Since the full field names
   are rather long, we define a shortened form (only visible within the ACL
   definitions) that reduces the space required to define them */

#define ST_CTX_CONV				SUBTYPE_CTX_CONV
#define ST_CTX_PKC				SUBTYPE_CTX_PKC
#define ST_CTX_HASH				SUBTYPE_CTX_HASH
#define ST_CTX_MAC				SUBTYPE_CTX_MAC
#define ST_CTX_GENERIC			SUBTYPE_CTX_GENERIC
#define ST_CTX_ANY				( ST_CTX_CONV | ST_CTX_PKC | ST_CTX_HASH | \
								  ST_CTX_MAC | ST_CTX_GENERIC )

#define ST_CERT_CERT			SUBTYPE_CERT_CERT
#define ST_CERT_CERTREQ			SUBTYPE_CERT_CERTREQ
#define ST_CERT_REQ_CERT		SUBTYPE_CERT_REQ_CERT
#define ST_CERT_REQ_REV			SUBTYPE_CERT_REQ_REV
#define ST_CERT_CERTCHAIN		SUBTYPE_CERT_CERTCHAIN
#define ST_CERT_ATTRCERT		SUBTYPE_CERT_ATTRCERT
#define ST_CERT_CRL				SUBTYPE_CERT_CRL
#define ST_CERT_CMSATTR			SUBTYPE_CERT_CMSATTR
#define ST_CERT_RTCS_REQ		SUBTYPE_CERT_RTCS_REQ
#define ST_CERT_RTCS_RESP		SUBTYPE_CERT_RTCS_RESP
#define ST_CERT_OCSP_REQ		SUBTYPE_CERT_OCSP_REQ
#define ST_CERT_OCSP_RESP		SUBTYPE_CERT_OCSP_RESP
#define ST_CERT_PKIUSER			SUBTYPE_CERT_PKIUSER
#define ST_CERT_ANY_CERT		( ST_CERT_CERT | ST_CERT_CERTREQ | \
								  SUBTYPE_CERT_REQ_CERT | ST_CERT_CERTCHAIN )
#define ST_CERT_ANY				( ST_CERT_ANY_CERT | ST_CERT_ATTRCERT | \
								  ST_CERT_REQ_REV | ST_CERT_CRL | \
								  ST_CERT_CMSATTR | ST_CERT_RTCS_REQ | \
								  ST_CERT_RTCS_RESP | ST_CERT_OCSP_REQ | \
								  ST_CERT_OCSP_RESP | ST_CERT_PKIUSER )

#define ST_KEYSET_FILE			SUBTYPE_KEYSET_FILE
#define ST_KEYSET_FILE_PARTIAL	SUBTYPE_KEYSET_FILE_PARTIAL
#define ST_KEYSET_FILE_RO		SUBTYPE_KEYSET_FILE_READONLY
#define ST_KEYSET_DBMS			SUBTYPE_KEYSET_DBMS
#define ST_KEYSET_DBMS_STORE	SUBTYPE_KEYSET_DBMS_STORE
#define ST_KEYSET_HTTP			SUBTYPE_KEYSET_HTTP
#define ST_KEYSET_LDAP			SUBTYPE_KEYSET_LDAP
#define ST_KEYSET_ANY			( ST_KEYSET_FILE | SUBTYPE_KEYSET_FILE_PARTIAL | \
								  ST_KEYSET_FILE_RO | ST_KEYSET_DBMS | \
								  ST_KEYSET_DBMS_STORE | ST_KEYSET_HTTP | \
								  ST_KEYSET_LDAP )

#define ST_ENV_ENV				SUBTYPE_ENV_ENV
#define ST_ENV_ENV_PGP			SUBTYPE_ENV_ENV_PGP
#define ST_ENV_DEENV			SUBTYPE_ENV_DEENV
#define ST_ENV_ANY				( ST_ENV_ENV | ST_ENV_ENV_PGP | ST_ENV_DEENV )

#define ST_DEV_SYSTEM			SUBTYPE_DEV_SYSTEM
#define ST_DEV_P11				SUBTYPE_DEV_PKCS11
#define ST_DEV_CAPI				SUBTYPE_DEV_CRYPTOAPI
#define ST_DEV_HW				SUBTYPE_DEV_HARDWARE
#define ST_DEV_ANY_STD			( ST_DEV_P11 | ST_DEV_CAPI | ST_DEV_HW )
#define ST_DEV_ANY				( ST_DEV_ANY_STD | ST_DEV_SYSTEM )

#define ST_SESS_SSH				SUBTYPE_SESSION_SSH
#define ST_SESS_SSH_SVR			SUBTYPE_SESSION_SSH_SVR
#define ST_SESS_SSL				SUBTYPE_SESSION_SSL
#define ST_SESS_SSL_SVR			SUBTYPE_SESSION_SSL_SVR
#define ST_SESS_RTCS			SUBTYPE_SESSION_RTCS
#define ST_SESS_RTCS_SVR		SUBTYPE_SESSION_RTCS_SVR
#define ST_SESS_OCSP			SUBTYPE_SESSION_OCSP
#define ST_SESS_OCSP_SVR		SUBTYPE_SESSION_OCSP_SVR
#define ST_SESS_TSP				SUBTYPE_SESSION_TSP
#define ST_SESS_TSP_SVR			SUBTYPE_SESSION_TSP_SVR
#define ST_SESS_CMP				SUBTYPE_SESSION_CMP
#define ST_SESS_CMP_SVR			SUBTYPE_SESSION_CMP_SVR
#define ST_SESS_SCEP			SUBTYPE_SESSION_SCEP
#define ST_SESS_SCEP_SVR		SUBTYPE_SESSION_SCEP_SVR
#define ST_SESS_CERT_SVR		SUBTYPE_SESSION_CERT_SVR
#define ST_SESS_ANY_SVR			( ST_SESS_SSH_SVR | ST_SESS_SSL_SVR | \
								  ST_SESS_RTCS_SVR | ST_SESS_OCSP_SVR | \
								  ST_SESS_TSP_SVR | ST_SESS_CMP_SVR | \
								  ST_SESS_SCEP_SVR | ST_SESS_CERT_SVR )
#define ST_SESS_ANY_CLIENT		( ST_SESS_SSH | ST_SESS_SSL | ST_SESS_RTCS | \
								  ST_SESS_OCSP | ST_SESS_TSP | ST_SESS_CMP | \
								  ST_SESS_SCEP )
#define ST_SESS_ANY_DATA		( ST_SESS_SSH | ST_SESS_SSH_SVR | \
								  ST_SESS_SSL | ST_SESS_SSL_SVR )
#define ST_SESS_ANY_REQRESP		( ST_SESS_RTCS | ST_SESS_RTCS_SVR | \
								  ST_SESS_OCSP | ST_SESS_OCSP_SVR | \
								  ST_SESS_TSP | ST_SESS_TSP_SVR | \
								  ST_SESS_CMP | ST_SESS_CMP_SVR | \
								  ST_SESS_SCEP | ST_SESS_SCEP_SVR | \
								  ST_SESS_CERT_SVR )
#define ST_SESS_ANY_SEC			( ST_SESS_ANY_DATA | \
								  ST_SESS_CMP | ST_SESSION_CMP_SVR )
#define ST_SESS_ANY				( ST_SESS_ANY_CLIENT | ST_SESS_ANY_SVR )

#define ST_USER_NORMAL			SUBTYPE_USER_NORMAL
#define ST_USER_SO				SUBTYPE_USER_SO
#define ST_USER_CA				SUBTYPE_USER_CA
#define ST_USER_ANY				( ST_USER_NORMAL | ST_USER_SO | ST_USER_CA )

/* Subtype values that allow access for any object subtype and for no
   object subtypes */

#define ST_ANY_A				( ST_CTX_ANY | ST_CERT_ANY )
#define ST_ANY_B				( ST_ENV_ANY | ST_KEYSET_ANY | ST_DEV_ANY )
#define ST_ANY_C				( ST_SESS_ANY | ST_USER_ANY )
#define ST_NONE					0

/****************************************************************************
*																			*
*							Access Permission Information					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "acl_perm.h"
#else
  #include "kernel/acl_perm.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Routing Information							*
*																			*
****************************************************************************/

/* Routing types, which specify the routing used for the message.  This
   routing applies not only for attribute manipulation messages but for all
   messages in general, so that some of the routing types defined below only
   apply for non-attribute messages.  The routing types are:

	ROUTE_NONE
		Not routed (the message or attribute is valid for any object type).

	ROUTE( target )
	ROUTE_ALT( target, altTarget )
	ROUTE_ALT2( target, altTarget1, altTarget2 )
		Fixed-target messages always routed to a particular object type or
		set of types (e.g. a certificate attribute is always routed to a
		certificate object; a generate key message is always routed to a
		context).  In some cases alternative targets are possible, e.g. a
		get-key message can be sent to a keyset or a device.

	ROUTE_FIXED( target )
	ROUTE_FIXED_ALT( target, altTarget )
		Not routed, but checked to make sure that they're addressed to the
		required target type.  These message types aren't routed because
		they're specific to a particular object and are explicitly
		unroutable.  For example, a get key message sent to a cert or
		context tied to a device shouldn't be forwarded on to the device,
		since it would result in the cert acting as a keyset.  This is
		theoretically justifiable - "Get me another cert from the same place
		that this one came from" - but it's stretching the orthogonality of
		objects a bit far.

	ROUTE_IMPLICIT
		For object attribute manipulation messages, implicitly routed by
		attribute type.

	ROUTE_SPECIAL( routingFunction )
		Special-case, message-dependent routing */

#define ROUTE_NONE \
		OBJECT_TYPE_NONE, NULL
#define ROUTE( target ) \
		( target ), findTargetType
#define ROUTE_ALT( target, altTarget ) \
		( target ) | ( ( altTarget ) << 8 ), findTargetType
#define ROUTE_ALT2( target, altTarget1, altTarget2 ) \
		( target ) | ( ( altTarget1 ) << 8 ) | ( ( altTarget2 ) << 16 ), findTargetType
#define ROUTE_FIXED( target ) \
		( target ), checkTargetType
#define ROUTE_FIXED_ALT( target, altTarget ) \
		( target ) | ( ( altTarget ) << 8 ), checkTargetType
#define ROUTE_IMPLICIT \
		OBJECT_TYPE_LAST, findTargetType
#define ROUTE_SPECIAL( function ) \
		OBJECT_TYPE_NONE, ( route##function )

/* Macros to determine which type of routing to apply */

#define isImplicitRouting( target )		( ( target ) == OBJECT_TYPE_LAST )
#define isExplicitRouting( target )		( ( target ) == OBJECT_TYPE_NONE )

/****************************************************************************
*																			*
*								Value Range Information						*
*																			*
****************************************************************************/

/* The value range (for numeric or boolean values) or length range (for
   variable-length data).  Some values aren't amenable to a simple range
   check so we also allow various extended types of checking.  To denote that
   an extended check needs to be performed, we set the low range value to
   RANGE_EXT_MARKER and the high range value to an indicator of the type of
   check to be performed.  The range types are:

	RANGE_ANY
		Allow any value
	RANGE_ALLOWEDVALUES
		extendedInfo contains int [] of allowed values, terminated by
		CRYPT_ERROR
	RANGE_SUBRANGES
		extendedInfo contains subrange [] of allowed subranges, terminated
		by { CRYPT_ERROR, CRYPT_ERROR }
	RANGE_SUBTYPED
		extendedInfo contains sub-acl [] of object type-specific sub-ACLs.
		The main ACL is only used as a general template for checks, the real
		checking is done via recursive application of the sub-ACL for the
		specific object sub-type */

typedef enum {
	RANGEVAL_NONE,					/* No range type */
	RANGEVAL_ANY,					/* Any value allowed */
	RANGEVAL_ALLOWEDVALUES,			/* List of permissible values */
	RANGEVAL_SUBRANGES,				/* List of permissible subranges */
	RANGEVAL_SUBTYPED,				/* Object-subtype-specific sub-ACL */
	RANGEVAL_LAST					/* Last valid range type */
	} RANGEVAL_TYPE;

#define RANGE_EXT_MARKER	( -1000 )/* Marker to denote extended range value */

#define RANGE_ANY			RANGE_EXT_MARKER, RANGEVAL_ANY
#define RANGE_ALLOWEDVALUES	RANGE_EXT_MARKER, RANGEVAL_ALLOWEDVALUES
#define RANGE_SUBRANGES		RANGE_EXT_MARKER, RANGEVAL_SUBRANGES
#define RANGE_SUBTYPED		RANGE_EXT_MARKER, RANGEVAL_SUBTYPED
#define RANGE( low, high )	( low ), ( high )

/* The maximum possible integer value, used to indicate that any value is
   allowed (e.g. when returning device-specific error codes).  Note that
   this differs from the MAX_INTLENGTH value defined in crypt.h, which
   defines the maximum data length value that can be safely specified by a
   signed integer */

#define RANGE_MAX			( INT_MAX - 128 )

/* Data structures to contain special-case range information */

typedef struct { const int lowRange, highRange; } RANGE_SUBRANGE_TYPE;

/* Macro to check whether it's an extended range and to extract the special
   range type */

#define isSpecialRange( attributeACL ) \
		( ( attributeACL )->lowRange == RANGE_EXT_MARKER )
#define getSpecialRangeType( attributeACL )	( ( attributeACL )->highRange )
#define getSpecialRangeInfo( attributeACL )	( ( attributeACL )->extendedInfo )

/****************************************************************************
*																			*
*									ACL Flags								*
*																			*
****************************************************************************/

/* Flags for attribute ACLs:

	FLAG_OBJECTPROPERTY
		This is an object property attribute which is handled by the kernel
		rather than being forwarded to the object.

	FLAG_TRIGGER
		Successfully setting this attribute triggers a change from the low to
		the high state */

#define ATTRIBUTE_FLAG_NONE		0x00
#define ATTRIBUTE_FLAG_PROPERTY	0x01
#define ATTRIBUTE_FLAG_TRIGGER	0x02
#define ATTRIBUTE_FLAG_LAST		0x04

/* Miscellaneous ACL flags:

	FLAG_LOW_STATE
	FLAG_HIGH_STATE
	FLAG_ANY_STATE
		Whether the object should be in a particular state.

	FLAG_ROUTE_TO_CTX
	FLAG_ROUTE_TO_CERT
		Whether routing should be applied to an object to locate an
		underlying object (e.g. a PKC object for a certificate or a
		certificate for a PKC object).  The need to apply routing is
		unfortunate but is required in order to apply the subtype check to
		PKC/cert objects, sorting out which (pre-routed) object types are
		permissible is beyond the scope of the ACL validation routines,
		which would have to take into consideration the intricacies of all
		manner of certificate objects paired with public and private keys */

#define ACL_FLAG_NONE			0x00
#define ACL_FLAG_LOW_STATE		0x01
#define ACL_FLAG_HIGH_STATE		0x02
#define ACL_FLAG_ANY_STATE		0x03
#define ACL_FLAG_ROUTE_TO_CTX	0x04
#define ACL_FLAG_ROUTE_TO_CERT	0x08

#define ACL_FLAG_MASK			0x0F
#define ACL_FLAG_STATE_MASK		0x03

/* Macros to check the misc.ACL flags */

#define checkObjectState( flags, objectHandle ) \
		( ( ( flags & ACL_FLAG_LOW_STATE ) && \
			  !isInHighState( objectHandle ) ) || \
		  ( ( flags & ACL_FLAG_HIGH_STATE ) && \
			  isInHighState( objectHandle ) ) )

/****************************************************************************
*																			*
*							Attribute ACL Definitions						*
*																			*
****************************************************************************/

/* The attribute's type, for attribute ACLs.  The basic values are boolean,
   numeric, or byte string, there are also some special types such as object
   handles that place extra constraints on the attribute */

typedef enum {
	ATTRIBUTE_VALUE_NONE,			/* Non-value */
	ATTRIBUTE_VALUE_BOOLEAN,		/* Boolean flag */
	ATTRIBUTE_VALUE_NUMERIC,		/* Numeric value */
	ATTRIBUTE_VALUE_STRING,			/* Byte string */
	ATTRIBUTE_VALUE_WCSTRING,		/* (Possible) widechar string */
	ATTRIBUTE_VALUE_OBJECT,			/* Object handle */
	ATTRIBUTE_VALUE_TIME,			/* Timestamp */
	ATTRIBUTE_VALUE_SPECIAL,		/* Special-case value with sub-ACLs */
	ATTRIBUTE_VALUE_LAST			/* Last attribute value type */
	} ATTRIBUTE_VALUE_TYPE;

/* Attribute ACL entry.  If the code is compiled in debug mode, we also add
   the attribute type, which is used for an internal consistency check */

typedef CHECK_RETVAL STDC_NONNULL_ARG( ( 2 ) ) \
	int ( *ROUTING_FUNCTION )( IN_HANDLE \
									const CRYPT_HANDLE originalObjectHandle, 
							   OUT_HANDLE_OPT \
									CRYPT_HANDLE *targetObjectHandle,
							   const long targets );

typedef struct {
#ifndef NDEBUG
	/* The attribute type, used for consistency checking */
	const CRYPT_ATTRIBUTE_TYPE attribute;/* Attribute */
#endif /* NDEBUG */

	/* Attribute type checking information: The attribute value type and
	   object subtypes for which the attribute is valid */
	const ATTRIBUTE_VALUE_TYPE valueType;/* Attribute value type */
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
									/* Object subtypes for which attr.valid */

	/* Access information: The type of access and object states that are
	   permitted, and attribute flags for this attribute */
	const int access;				/* Permitted access type */
	const int flags;				/* Attribute flags */

	/* Routing information: The object type (or types, packed into a single
	   value if there are more than one type) that the attribute applies to,
	   and the routing function applied to the attribute message */
	const long routingTarget;		/* Target type(s) if routable */
	ROUTING_FUNCTION routingFunction;

	/* Attribute value checking information */
	const int lowRange;				/* Min/max allowed if numeric/boolean, */
#ifdef SYSTEM_16BIT
	const long highRange;			/*	length if string */
#else
	const int highRange;			/*	length if string */
#endif /* 16- vs. 32-bit systems */
	const void *extendedInfo;		/* Extended ACL/checking information */
	} ATTRIBUTE_ACL;

/* Macros to set up attribute ACL's.  We have one for each of the basic types
   and two general-purpose ones that provide more control over the values */

#ifndef NDEBUG
  /* Standard ACL entries */
  #define MKACL_B( attribute, subTypeA, subTypeB, subTypeC, access, routing ) \
			{ attribute, ATTRIBUTE_VALUE_BOOLEAN, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, FALSE, TRUE, NULL }
  #define MKACL_N( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_NUMERIC, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, range, NULL }
  #define MKACL_S( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, range, NULL }
  #define MKACL_WCS( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_WCSTRING, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, range, NULL }
  #define MKACL_O( attribute, subTypeA, subTypeB, subTypeC, access, routing, type ) \
			{ attribute, ATTRIBUTE_VALUE_OBJECT, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, 0, 0, type }
  #define MKACL_T( attribute, subTypeA, subTypeB, subTypeC, access, routing ) \
			{ attribute, ATTRIBUTE_VALUE_TIME, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, 0, 0, NULL }
  #define MKACL_X( attribute, subTypeA, subTypeB, subTypeC, access, routing, subACL ) \
			{ attribute, ATTRIBUTE_VALUE_SPECIAL, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, RANGE_SUBTYPED, subACL }

  /* Extended types */
  #define MKACL_B_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing ) \
			{ attribute, ATTRIBUTE_VALUE_BOOLEAN, subTypeA, subTypeB, subTypeC, access, \
			  flags, routing, FALSE, TRUE, NULL }
  #define MKACL_N_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_NUMERIC, subTypeA, subTypeB, subTypeC, access, \
			  flags, routing, range, NULL }
  #define MKACL_S_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, \
			  flags, routing, range, NULL }
  #define MKACL_O_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, type ) \
			{ attribute, ATTRIBUTE_VALUE_OBJECT, subTypeA, subTypeB, subTypeC, access, \
			  flags, routing, 0, 0, type }
  #define MKACL_X_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, subACL ) \
			{ attribute, ATTRIBUTE_VALUE_SPECIAL, subTypeA, subTypeB, subTypeC, access, \
			  flags, routing, RANGE_SUBTYPED, subACL }

  /* General-purpose ACL macros */
  #define MKACL( attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, range, NULL }
  #define MKACL_EX( attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range, allowed ) \
			{ attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, range, allowed }

  /* End-of-ACL canary */
  #define MKACL_END() \
			{ CRYPT_ERROR, ATTRIBUTE_VALUE_NONE, 0, 0, 0, ACCESS_xxx_xxx, \
			  0, 0, NULL, 0, 0, NULL }

  /* End-of-ACL marker, used to terminate variable-length sub-ACL lists.  The
     ST_ANY_A/B/C match ensures that it matches any object types */
  #define MKACL_END_SUBACL() \
			{ CRYPT_ERROR, ATTRIBUTE_VALUE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, ACCESS_xxx_xxx, \
			  0, 0, NULL, 0, 0, NULL }
#else
  /* Standard ACL entries */
  #define MKACL_B( attribute, subTypeA, subTypeB, subTypeC, access, routing ) \
			{ ATTRIBUTE_VALUE_BOOLEAN, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, FALSE, TRUE, NULL }
  #define MKACL_N( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ ATTRIBUTE_VALUE_NUMERIC, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, range, NULL }
  #define MKACL_S( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, range, NULL }
  #define MKACL_WCS( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ ATTRIBUTE_VALUE_WCSTRING, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, range, NULL }
  #define MKACL_O( attribute, subTypeA, subTypeB, subTypeC, access, routing, type ) \
			{ ATTRIBUTE_VALUE_OBJECT, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, 0, 0, type }
  #define MKACL_T( attribute, subTypeA, subTypeB, subTypeC, access, routing ) \
			{ ATTRIBUTE_VALUE_TIME, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, 0, 0, NULL }
  #define MKACL_X( attribute, subTypeA, subTypeB, subTypeC, access, routing, subACL ) \
			{ ATTRIBUTE_VALUE_SPECIAL, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, RANGE_SUBTYPED, subACL }

  /* Extended types */
  #define MKACL_B_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing ) \
			{ ATTRIBUTE_VALUE_BOOLEAN, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, FALSE, TRUE, NULL }
  #define MKACL_N_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ ATTRIBUTE_VALUE_NUMERIC, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, range, NULL }
  #define MKACL_S_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, range, NULL }
  #define MKACL_O_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, type ) \
			{ ATTRIBUTE_VALUE_OBJECT, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, 0, 0, type }
  #define MKACL_X_EX( attribute, subTypeA, subTypeB, subTypeC, access, flags, routing, subACL ) \
			{ ATTRIBUTE_VALUE_SPECIAL, subTypeA, subTypeB, subTypeC, access, flags, \
			  routing, RANGE_SUBTYPED, subACL }

  /* General-purpose ACL macros */
  #define MKACL( attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range ) \
			{ valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range, NULL }
  #define MKACL_EX( attribute, valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range, allowed ) \
			{ valueType, subTypeA, subTypeB, subTypeC, access, flags, routing, range, allowed }

  /* End-of-ACL canary */
  #define MKACL_END() \
			{ ATTRIBUTE_VALUE_NONE, 0, 0, 0, ACCESS_xxx_xxx, \
			  0, 0, NULL, 0, 0, NULL }

  /* End-of-ACL marker, used to terminate variable-length sub-ACL lists.  The
     ST_ANY_A/B/C match ensures that it matches any object types */
  #define MKACL_END_SUBACL() \
			{ ATTRIBUTE_VALUE_NONE, ST_ANY_A, ST_ANY_B, ST_ANY_C, ACCESS_xxx_xxx, \
			  0, 0, NULL, 0, 0, NULL }
#endif /* NDEBUG */

/* The attribute ACLs are usually implemented as a lookup table, but in some
   cases we may use sparse ACLs to handle special-case situations where only
   a few attributes need to be checked.  In order to locate the appropriate
   ACL entry, the 'attribute' member (normally only used for debugging
   purposes) needs to be present.  To handle this, we define an alternative
   ACL entry entry type (and associated setup macros) that differs from the
   standard one in that the attribute member is present unconditionally.

   In addition to this, the "attribute" being checked may not be a standard 
   attribute but some other enumerated type, so we have to make the 
   supposed attribute a typeless 'int' to avoid type-conversion issues 
   (CRYPT_ATTRIBUTE_TYPE has an artificial member set to INT_MAX to make 
   sure that it's the same size as an integer for compilers with variable-
   width enums).

   We have to be especially careful here because the parent type differs
   depending on whether it's a normal or debug build.  For the debug build
   the 'attribute' member is present at the start, for the release build
   it's absent so we place it at the end where it doesn't interfere with the
   other struct members */

typedef struct {
#ifndef NDEBUG
	const int /*CRYPT_ATTRIBUTE_TYPE*/ attribute;/* Attribute */
#endif /* !NDEBUG */
	const ATTRIBUTE_VALUE_TYPE valueType;/* Attribute value type */
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
	const int access;				/* Permitted access type */
	const int flags;				/* Attribute flags */
	const long routingTarget;		/* Target type if routable */
	ROUTING_FUNCTION routingFunction;
	const int lowRange;				/* Min/max allowed if numeric/boolean, */
	const int highRange;			/*	length if string */
	const void *extendedInfo;		/* Extended access information */
#ifdef NDEBUG
	const int /*CRYPT_ATTRIBUTE_TYPE*/ attribute;/* Attribute */
#endif /* NDEBUG */
	} ATTRIBUTE_ACL_ALT;

#ifndef NDEBUG
  #define MKACL_S_ALT( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ attribute, ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, \
			  0, routing, range, NULL }
#else
  #define MKACL_S_ALT( attribute, subTypeA, subTypeB, subTypeC, access, routing, range ) \
			{ ATTRIBUTE_VALUE_STRING, subTypeA, subTypeB, subTypeC, access, 0, \
			  routing, range, NULL, attribute }
#endif /* !NDEBUG */

/****************************************************************************
*																			*
*								Keyset ACL Definitions						*
*																			*
****************************************************************************/

/* Key management ACL entry */

typedef struct {
	/* The item type */
	const KEYMGMT_ITEM_TYPE itemType;/* Key management item type */

	/* Valid keyset types and access types for this item type.  This is a 
	   matrix giving keyset types for which read/write/delete (R/W/D), 
	   getFirst/Next (FN), and query (Q) access are valid */
	const OBJECT_SUBTYPE keysetR_subTypeA, keysetR_subTypeB, keysetR_subTypeC;
	const OBJECT_SUBTYPE keysetW_subTypeA, keysetW_subTypeB, keysetW_subTypeC;
	const OBJECT_SUBTYPE keysetD_subTypeA, keysetD_subTypeB, keysetD_subTypeC;
	const OBJECT_SUBTYPE keysetFN_subTypeA, keysetFN_subTypeB, keysetFN_subTypeC;
	const OBJECT_SUBTYPE keysetQ_subTypeA, keysetQ_subTypeB, keysetQ_subTypeC;

	/* Permitted object types, key IDs, and key management flags for this 
	   item type */
	const OBJECT_SUBTYPE objSubTypeA, objSubTypeB, objSubTypeC;
									/* Permitted object types for item */
	const CRYPT_KEYID_TYPE *allowedKeyIDs;	/* Permitted key IDs */
	const int allowedFlags;			/* Permitted key management flags */

	/* Parameter flags for the mechanism information.  These define which
	   types of optional/mandatory parameters can and can't be present,
	   using an extended form of the ACCESS_xxx flags to indicate whether
	   the parameter is required or not/permitted or not for read, write,
	   and delete messages */
	const int idUseFlags;			/* ID required/not permitted */
	const int pwUseFlags;			/* Password required/not permitted */

	/* In the case of public/private keys the general-purpose ACL entries
	   aren't quite specific enough since some keysets require specific
	   types of certificates while others require generic public-key objects.
	   In the latter case they can have almost any kind of certificate object
	   attached, which doesn't matter when we're interested only in the
	   public key but does matter if we want a specific type of cert.  If we
	   want a specific cert type, we specify the subset of keysets that this
	   applies to and the cert type(s) here */
	const OBJECT_SUBTYPE specificKeysetSubTypeA, specificKeysetSubTypeB, \
						 specificKeysetSubTypeC;
	const OBJECT_SUBTYPE specificObjSubTypeA, specificObjSubTypeB, \
						 specificObjSubTypeC;
	} KEYMGMT_ACL;

/* Macros to set up key management ACLs.  The basic form treats the RWD and
   FnQ groups as one value, the _RWD form specifies individual RWD and FnQ
   values, and the _EX form adds special-case checking for specific object
   types that must be written to some keyset types */

#define MK_KEYACL( itemType, keysetRWDSubType, keysetFNQSubType, \
				   objectSubType, keyIDs, flags, idUseFlags, pwUseFlags ) \
			{ itemType, ST_NONE, keysetRWDSubType, ST_NONE, \
			  ST_NONE, keysetRWDSubType, ST_NONE, \
			  ST_NONE, keysetRWDSubType, ST_NONE, \
			  ST_NONE, keysetFNQSubType, ST_NONE, \
			  ST_NONE, keysetFNQSubType, ST_NONE, \
			  objectSubType, ST_NONE, ST_NONE, \
			  keyIDs, flags, idUseFlags, pwUseFlags, \
			  ST_NONE, ST_NONE, ST_NONE, ST_NONE, ST_NONE, ST_NONE }
#define MK_KEYACL_RWD( itemType, keysetR_SubType, keysetW_SubType, keysetD_SubType, \
  					   keysetFN_SubType, keysetQ_SubType, objectSubType, keyIDs, \
					   flags, idUseFlags, pwUseFlags ) \
			{ itemType, ST_NONE, keysetR_SubType, ST_NONE, \
			  ST_NONE, keysetW_SubType, ST_NONE, \
			  ST_NONE, keysetD_SubType, ST_NONE, \
			  ST_NONE, keysetFN_SubType, ST_NONE, \
			  ST_NONE, keysetQ_SubType, ST_NONE, \
			  objectSubType, ST_NONE, ST_NONE, \
			  keyIDs, flags, idUseFlags, pwUseFlags, \
			  ST_NONE, ST_NONE, ST_NONE, ST_NONE, ST_NONE, ST_NONE }
#define MK_KEYACL_EX( itemType, keysetR_SubType, keysetW_SubType, keysetD_SubType, \
  					  keysetFN_SubType, keysetQ_SubType, objectSubType, keyIDs, \
					  flags, idUseFlags, pwUseFlags, specificKeysetType, \
					  specificObjectType ) \
			{ itemType, ST_NONE, keysetR_SubType, ST_NONE, \
			  ST_NONE, keysetW_SubType, ST_NONE, \
			  ST_NONE, keysetD_SubType, ST_NONE, \
			  ST_NONE, keysetFN_SubType, ST_NONE, \
			  ST_NONE, keysetQ_SubType, ST_NONE, \
			  objectSubType, ST_NONE, ST_NONE, \
			  keyIDs, flags, idUseFlags, pwUseFlags, \
			  ST_NONE, specificKeysetType, ST_NONE, \
			  specificObjectType, ST_NONE, ST_NONE }

/****************************************************************************
*																			*
*							Parameter ACL Definitions						*
*																			*
****************************************************************************/

/* The parameter's type.  The basic values are boolean, numeric, or byte
   string, there are also some special types such as object handles that
   place extra constraints on the attribute */

typedef enum {
	PARAM_VALUE_NONE,				/* Non-value */
	PARAM_VALUE_NUMERIC,			/* Numeric value */
	PARAM_VALUE_STRING,				/* Byte string */
	PARAM_VALUE_STRING_OPT,			/* Byte string or (NULL, 0) */
	PARAM_VALUE_STRING_NONE,		/* Empty (NULL, 0) string */
	PARAM_VALUE_OBJECT,				/* Object handle */
	PARAM_VALUE_LAST				/* Last valid parameter type */
	} PARAM_VALUE_TYPE;

/* Parameter ACL entry, which defines the type and valid values for a
   message parameter.  This ACL type is used as a sub-ACL in a variety of
   other kernel ACLs */

typedef struct {
	const PARAM_VALUE_TYPE valueType;/* Parameter value type */
	const int lowRange, highRange;	/* Min/max value or length */
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
									/* Object subtypes for which param.valid */
	const int flags;				/* ACL flags */
	} PARAM_ACL;

/* Macros to set up parameter ACLs */

#define MKACP_B() \
			{ PARAM_VALUE_NUMERIC, FALSE, TRUE, 0, 0, 0, 0 }
#define MKACP_N( min, max ) \
			{ PARAM_VALUE_NUMERIC, min, max, 0, 0, 0, 0 }
#define MKACP_N_FIXED( value ) \
			{ PARAM_VALUE_NUMERIC, value, value, 0, 0, 0, 0 }
#define MKACP_S( minLen, maxLen ) \
			{ PARAM_VALUE_STRING, minLen, maxLen, 0, 0, 0, 0 }
#define MKACP_S_OPT( minLen, maxLen ) \
			{ PARAM_VALUE_STRING_OPT, minLen, maxLen, 0, 0, 0, 0 }
#define MKACP_S_NONE() \
			{ PARAM_VALUE_STRING_NONE, 0, 0, 0, 0, 0, 0 }
#define MKACP_O( subTypeA, flags ) \
			{ PARAM_VALUE_OBJECT, 0, 0, subTypeA, ST_NONE, ST_NONE, flags }

/* End-of-mechanism-ACL marker */

#define MKACP_END() \
			{ PARAM_VALUE_NONE, 0, 0, 0, 0, 0 }

/* Macro to access the parameter ACL information for a given parameter in a
   list of parameter ACLs */

#define paramInfo( parentACL, paramNo )		parentACL->paramACL[ paramNo ]

/* Macro to get the subtype of an object */

#define objectST( objectHandle )			objectTable[ objectHandle ].subType

/* Macros to check each parameter against a parameter ACL entry */

#define checkParamNumeric( paramACL, value ) \
		( paramACL.valueType == PARAM_VALUE_NUMERIC && \
		  ( value >= paramACL.lowRange && value <= paramACL.highRange ) )

#define checkParamString( paramACL, data, dataLen ) \
		( ( ( paramACL.valueType == PARAM_VALUE_STRING_NONE || \
			  paramACL.valueType == PARAM_VALUE_STRING_OPT ) && \
			data == NULL && dataLen == 0 ) || \
		  ( ( paramACL.valueType == PARAM_VALUE_STRING || \
			  paramACL.valueType == PARAM_VALUE_STRING_OPT ) && \
			( dataLen >= paramACL.lowRange && \
			  dataLen <= paramACL.highRange ) && \
			isReadPtr( data, dataLen ) ) )

#define checkParamObject( paramACL, objectHandle ) \
		( ( paramACL.valueType == PARAM_VALUE_NUMERIC && \
			paramACL.lowRange == CRYPT_UNUSED && \
			objectHandle == CRYPT_UNUSED ) || \
		  ( paramACL.valueType == PARAM_VALUE_OBJECT && \
			( ( paramACL.subTypeA & objectST( objectHandle ) ) == \
									objectST( objectHandle ) || \
			  ( paramACL.subTypeB & objectST( objectHandle ) ) == \
									objectST( objectHandle ) || \
			  ( paramACL.subTypeC & objectST( objectHandle ) ) == \
									objectST( objectHandle ) ) && \
			checkObjectState( paramACL.flags, objectHandle ) ) )

/****************************************************************************
*																			*
*								Misc.ACL Definitions						*
*																			*
****************************************************************************/

/* Object ACL entry for object parameters for messages.  This is used as a
   composite entry in various ACLs that apply to objects */

typedef struct {
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
									/* Object subtypes for which attr.valid */
	const int flags;				/* ACL flags */
	} OBJECT_ACL;

/* Message ACL entry */

typedef struct {
	const MESSAGE_TYPE type;		/* Message type */
	const OBJECT_ACL objectACL;		/* Valid objects for message type */
	} MESSAGE_ACL;

/* Mechanism ACL entry */

typedef struct {
	const MECHANISM_TYPE type;		/* Mechanism type */
	const PARAM_ACL paramACL[ 6 ];	/* Parameter ACL information */
	} MECHANISM_ACL;

/* Create-object ACL entry */

typedef struct CRA {
	const OBJECT_TYPE type;			/* Object type */
	const PARAM_ACL paramACL[ 5 ];	/* Parameter ACL information */
	const int exceptions[ 4 ];		/* Subtypes that need special handling */
	const struct CRA *exceptionACL;	/* Special-handling ACL */
	} CREATE_ACL;

/* Cert mgmt.ACL entry.  These have parameters that work similarly to the
   mechanism ACLs, except that only a small subset (objects and unused) are
   used in practice.  In addition some objects require the presence of
   secondary objects (dependent objects for the main object), for example
   a CA's PKC context requires an attached CA certificate.  This is
   specified in the secondary parameter ACL, which mirrors the main
   parameter ACL */

typedef struct {
	const CRYPT_CERTACTION_TYPE action;	/* Cert mgmt.action */
	const int access;				/* Permitted access type */
	const PARAM_ACL paramACL[ 3 ];	/* Parameter ACL information */
	const PARAM_ACL secParamACL[ 3 ];/* Parameter ACL for dep.objects */
	} CERTMGMT_ACL;

/* Compare-message ACL entry */

typedef struct {
	const MESSAGE_COMPARE_TYPE compareType;	/* Compare message type */
	const OBJECT_ACL objectACL;		/* Valid objects for message type */
	const PARAM_ACL paramACL[ 1 ];	/* Parameter ACL information */
	} COMPARE_ACL;

/* Macros to access the parameter ACLs as an array of PARAM_ACL entries */

#define getParamACL( aclInfo )		( &( aclInfo )->paramACL[ 0 ] )
#define getParamACLSize( aclInfo )	( sizeof( ( aclInfo )->paramACL ) / sizeof( PARAM_ACL ) )

/* Macros to set up compare ACLs */

#define MK_CMPACL_S( objSTA, lowRange, highRange ) \
			{ objSTA, ST_NONE, ST_NONE, ACL_FLAG_HIGH_STATE }, \
			{ MKACP_S( lowRange, highRange ) }
#define MK_CMPACL_O( objSTA, pObjSTA ) \
			{ objSTA, ST_NONE, ST_NONE, ACL_FLAG_HIGH_STATE }, \
			{ MKACP_O( pObjSTA, ACL_FLAG_HIGH_STATE ) }
#define MK_CMPACL_END() \
			{ ST_NONE, ST_NONE, ST_NONE, ACL_FLAG_NONE }, \
			{ MKACP_END() }

/* Check-message ACL entry.  Most checks are for the same capability in a
   single object type (e.g. encryption capability in a PKC context and/or
   cert), for which we check the object type and action corresponding to the
   check.  However, some checks apply to dependent but object pairs but with
   different capabilities (for example a PKC context and a CA certificate).
   In this case the check can be applied to either of the two object types,
   so we allow for a secondary ACL entry for the related object type.  In
   addition once we've applied the check to the primary object, we have to
   forward it to the secondary object, however in order to avoid message
   loops the forwarded check type applies only to the secondary object
   rather than being the same as the one that was applied to the primary
   object */

typedef struct {
	const MESSAGE_CHECK_TYPE checkType;	/* Check message type */
	const MESSAGE_TYPE actionType;	/* Action corresponding to check */
	const OBJECT_ACL objectACL;		/* Valid objects for message type */
	const struct CAA *altACL;		/* Ptr. to alt.ACL */
	} CHECK_ACL;

typedef struct CAA {
	const OBJECT_TYPE object;		/* Object type this entry applies to */
	const MESSAGE_CHECK_TYPE checkType;	/* Check message type */
	const OBJECT_TYPE depObject;	/* Dependent object type */
	const OBJECT_ACL depObjectACL;	/* Valid objects for message type */
	const MESSAGE_CHECK_TYPE fdCheckType; /* Fwded check type for related obj.*/
	} CHECK_ALT_ACL;

/* Macros to set up check ACLs.  For the standard check ACL the first
   parameter, the check type, is supplied explicitly and isn't present in
   the macro */

#define MK_CHKACL( action, objSTA ) \
			action, { objSTA, ST_NONE, ST_NONE, ACL_FLAG_HIGH_STATE }, NULL
#define MK_CHKACL_EX( action, objSTA, objSTB, flags ) \
			action, { objSTA, objSTB, ST_NONE, flags }
#define MK_CHKACL_EXT( action, objSTA, extACL ) \
			action, { objSTA, ST_NONE, ST_NONE, ACL_FLAG_HIGH_STATE }, extACL
#define MK_CHKACL_END() \
			MESSAGE_NONE, { ST_NONE, ST_NONE, ST_NONE, ACL_FLAG_NONE }

#define MK_CHKACL_ALT( depObj, depObjSTA, fdCheck ) \
			depObj, { depObjSTA, ST_NONE, ST_NONE, ACL_FLAG_HIGH_STATE }, fdCheck
#define MK_CHKACL_ALT_END() \
			MESSAGE_CHECK_NONE, \
			OBJECT_TYPE_NONE, { ST_NONE, ST_NONE, ST_NONE, ACL_FLAG_NONE }, MESSAGE_CHECK_NONE

/* Object dependency ACL entry, used when making one object dependent on
   another */

typedef struct {
	const OBJECT_TYPE type;			/* Object type and subtype */
	const OBJECT_SUBTYPE subTypeA, subTypeB, subTypeC;
	const OBJECT_TYPE dType;		/* Dependent object type and subtype */
	const OBJECT_SUBTYPE dSubTypeA, dSubTypeB, dSubTypeC;
	const int flags;				/* Dependency flags */
	} DEPENDENCY_ACL;

/* Macros to set up dependency ACLs */

#define MK_DEPACL( objType, objSTA, objSTB, objSTC, dObjType, dObjSTA, dObjSTB, dObjSTC ) \
			{ objType, objSTA, objSTB, objSTC, dObjType, dObjSTA, dObjSTB, dObjSTC, DEP_FLAG_NONE }
#define MK_DEPACL_EX( objType, objSTA, objSTB, objSTC, dObjType, dObjSTA, dObjSTB, dObjSTC, flags ) \
			{ objType, objSTA, objSTB, objSTC, dObjType, dObjSTA, dObjSTB, dObjSTC, flags }
#define MK_DEPACL_END() \
			{ OBJECT_TYPE_NONE, 0, 0, 0, OBJECT_TYPE_NONE, 0, 0, 0, DEP_FLAG_NONE }

/* Flags for the dependency ACLs */

#define DEP_FLAG_NONE		0x00	/* No dependency flag */
#define DEP_FLAG_UPDATEDEP	0x01	/* Update dependent object */

#endif /* _ACL_DEFINED */
