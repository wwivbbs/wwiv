/****************************************************************************
*																			*
*							ACL Permission Definitions						*
*						Copyright Peter Gutmann 1997-2005					*
*																			*
****************************************************************************/

#ifndef _ACL_PERM_DEFINED

#define _ACL_PERM_DEFINED

/****************************************************************************
*																			*
*							Access Permission Flags							*
*																			*
****************************************************************************/

/* Read/write/delete permission flags.  Each object can have two modes, "low"
   and "high", whose exact definition depends on the object type.  At some
   point an operation on an object (loading a key for a context, signing a
   cert) will move it from the low to the high state, at which point a much
   more restrictive set of permissions apply.  The permissions are given as
   RWD_RWD with the first set being for the object in the high state and the
   second for the object in the low state.

   In addition to the usual external-access permssions, some attributes are
   only visible internally.  Normal attributes have matching internal-access
   and external-access permssions but the internal-access-only ones have the
   external-access permissions turned off.

   Some of the odder combinations arise from ACLs with sub-ACLs, for which
   the overall access permission is the union of the permissions in all the
   sub-ACLs.  For example if one sub-ACL has xxx_RWx and another has xWD_xxx,
   the parent ACL will have xWD_RWx.  Finally, there are a small number of
   special-case permissions in which internal access differs from external
   access.  This is used for attributes that are used for control purposes
   (e.g. identifier information in cert requests) and can be set internally
   but are read-only externally.

			  Internal low ----++---- External high
			  Internal high --+||+--- External low */
#define ACCESS_xxx_xxx		0x0000	/* No access */
#define ACCESS_xxx_xWx		0x0202	/* Low: Write-only */
#define ACCESS_xxx_xWD		0x0303	/* Low: Write/delete */
#define ACCESS_xxx_Rxx		0x0404	/* Low: Read-only */
#define ACCESS_xxx_RWx		0x0606	/* Low: Read/write */
#define ACCESS_xxx_RWD		0x0707	/* Low: All access */
#define ACCESS_xWx_xWx		0x2222	/* High: Write-only, Low: Write-only */
#define ACCESS_xWD_xWD		0x3333	/* High: Write/delete, Low: Write/delete */
#define ACCESS_xWx_xxx		0x2020	/* High: Write-only, Low: None */
#define ACCESS_Rxx_xxx		0x4040	/* High: Read-only, Low: None */
#define ACCESS_Rxx_xWx		0x4242	/* High: Read-only, Low: Write-only */
#define ACCESS_Rxx_Rxx		0x4444	/* High: Read-only, Low: Read-only */
#define ACCESS_Rxx_RxD		0x4545	/* High: Read-only, Low: Read/delete */
#define ACCESS_Rxx_RWx		0x4646	/* High: Read-only, Low: Read/write */
#define ACCESS_Rxx_RWD		0x4747	/* High: Read-only, Low: All access */
#define ACCESS_RxD_RxD		0x5555	/* High: Read/delete, Low: Read/delete */
#define ACCESS_RWx_xxx		0x6060	/* High: Read/write, Low: None */
#define ACCESS_RWx_xWx		0x6262	/* High: Read/write, Low: Write-only */
#define ACCESS_RWx_Rxx		0x6464	/* High: Read/write, Low: Read-only */
#define ACCESS_RWx_RWx		0x6666	/* High: Read/write, Low: Read/write */
#define ACCESS_RWx_RWD		0x6767	/* High: Read/write, Low: All access */
#define ACCESS_RWD_xxx		0x7070	/* High: All access, Low: None */
#define ACCESS_RWD_xWD		0x7373	/* High: All access, Low: Write/delete */
#define ACCESS_RWD_RWD		0x7777	/* High: All access, Low: All access */

#define ACCESS_INT_xxx_xxx	0x0000	/* Internal: No access */
#define ACCESS_INT_xxx_xWx	0x0200	/* Internal: None, write-only */
#define ACCESS_INT_xxx_Rxx	0x0400	/* Internal: None, read-only */
#define ACCESS_INT_xWx_xxx	0x2000	/* Internal: Write-only, none */
#define ACCESS_INT_xWx_xWx	0x2200	/* Internal: Write-only, write-only */
#define ACCESS_INT_Rxx_xxx	0x4000	/* Internal: Read-only, none */
#define ACCESS_INT_Rxx_xWx	0x4200	/* Internal: Read-only, write-only */
#define ACCESS_INT_Rxx_Rxx	0x4400	/* Internal: Read-only, read-only */
#define ACCESS_INT_Rxx_RWx	0x4600	/* Internal: Read-only, read/write */
#define ACCESS_INT_RWx_xxx	0x6000	/* Internal: Read/write, none */
#define ACCESS_INT_RWx_xWx	0x6200	/* Internal: Read/write, write-only */
#define ACCESS_INT_RWx_RWx	0x6600	/* Internal: Read/write, read/write */

#define ACCESS_SPECIAL_Rxx_RWx_Rxx_Rxx \
							0x4644	/* Internal = Read-only, read/write,
									   External = Read-only, read-only */
#define ACCESS_SPECIAL_Rxx_RWD_Rxx_Rxx \
							0x4744	/* Internal = Read-only, all access,
									   External = Read-only, read-only */

#define ACCESS_FLAG_x		0x0000	/* No access permitted */
#define ACCESS_FLAG_R		0x0004	/* Read access permitted */
#define ACCESS_FLAG_W		0x0002	/* Write access permitted */
#define ACCESS_FLAG_D		0x0001	/* Delete access permitted */
#define ACCESS_FLAG_H_R		0x0040	/* Read access permitted in high mode */
#define ACCESS_FLAG_H_W		0x0020	/* Write access permitted in high mode */
#define ACCESS_FLAG_H_D		0x0010	/* Delete access permitted in high mode */

#define ACCESS_MASK_EXTERNAL 0x0077	/* External-access flags mask */
#define ACCESS_MASK_INTERNAL 0x7700	/* Internal-access flags mask */

#define MK_ACCESS_INTERNAL( value )	( ( value ) << 8 )

/* The basic RWD access flags above are also used for checking some
   parameters passed with keyset mechanism messages, however in addition to
   these we have flags for getFirst/getNext functions that are only used
   with keysets.  Note that although these partially overlap with the high-
   mode access flags for attributes this isn't a problem since keysets don't
   distinguish between high and low states.  In addition some of the
   combinations may seem a bit odd, but that's because they're for mechanism
   parameters such as key ID information which is needed for reads and
   deletes but not writes, since it's implicitly included with the key which
   is being written.  Finally, one type of mechanism has parameter semantics
   that are too complex to express via a simple ACL entry, these are given a
   different-looking ACL entry xxXXxx to indicate to readers that this isn't
   the same as a normal entry with the same value.  In addition to this, the
   semantics of some of the getFirst/Next accesses are complex enough that
   we need to hardcode them into the ACL checking, leaving only a
   representative entry on the ACL definition itself (see key_acl.c for more
   details) */

#define ACCESS_KEYSET_xxxxx	0x0000	/* No access */
#define ACCESS_KEYSET_xxXXx	0x0006	/* Special-case values (params optional) */
#define ACCESS_KEYSET_xxRxD	0x0005	/* Read and delete */
#define ACCESS_KEYSET_xxRWx	0x0006	/* Read/write */
#define ACCESS_KEYSET_xxRWD	0x0007	/* Read/write and delete */
#define ACCESS_KEYSET_FxRxD	0x0015	/* GetFirst, read, and delete */
#define ACCESS_KEYSET_FNxxx	0x0018	/* GetFirst/Next */
#define ACCESS_KEYSET_FNRWD	0x001F	/* All access */

#define ACCESS_FLAG_F		0x0010	/* GetFirst access permitted */
#define ACCESS_FLAG_N		0x0008	/* GetNext access permitted */

/****************************************************************************
*																			*
*					Conditional Access Permission Macros 					*
*																			*
****************************************************************************/

/* Many of cryptlib's capabilities can be selectively disabled, in which 
   case we also disable the use of the attributes that go with them.  The 
   following macros take care of this selective disabling */

#define MKPERM( perm )					ACCESS_##perm
#define MKPERM_INT( perm )				ACCESS_INT_##perm

/* Certificate ACL macros */

#ifdef USE_CERTIFICATES
  #define MKPERM_CERTIFICATES( perm )	ACCESS_##perm
  #define MKPERM_INT_CERTIFICATES( perm ) ACCESS_INT_##perm
  #define MKPERM_SPECIAL_CERTIFICATES( perm ) ACCESS_SPECIAL_##perm
#else
  #define MKPERM_CERTIFICATES( perm )	ACCESS_xxx_xxx
  #define MKPERM_INT_CERTIFICATES( perm ) ACCESS_INT_xxx_xxx
  #define MKPERM_SPECIAL_CERTIFICATES( perm ) ACCESS_xxx_xxx
#endif /* USE_CERTIFICATES */

#ifdef USE_CERTREQ
  #define MKPERM_CERTREQ( perm )		ACCESS_##perm
  #define MKPERM_INT_CERTREQ( perm )	ACCESS_INT_##perm
#else
  #define MKPERM_CERTREQ( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_CERTREQ( perm )	ACCESS_INT_xxx_xxx
#endif /* USE_CERTREQ */

#ifdef USE_CERTREV
  #define MKPERM_CERTREV( perm )		ACCESS_##perm
  #define MKPERM_INT_CERTREV( perm )	ACCESS_INT_##perm
#else
  #define MKPERM_CERTREV( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_CERTREV( perm )	ACCESS_INT_xxx_xxx
#endif /* USE_CERTREV */

#if defined( USE_CERTREQ ) || defined( USE_CERTREV )
  #define MKPERM_CERTREQ_REV( perm )	ACCESS_##perm
#else
  #define MKPERM_CERTREQ_REV( perm )	ACCESS_xxx_xxx
#endif /* USE_CERTREQ || USE_CERTREV */

#ifdef USE_CERTVAL
  #define MKPERM_CERTVAL( perm )		ACCESS_##perm
  #define MKPERM_INT_CERTVAL( perm )	ACCESS_INT_##perm
#else
  #define MKPERM_CERTVAL( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_CERTVAL( perm )	ACCESS_INT_xxx_xxx
#endif /* USE_CERTVAL */

#if defined( USE_CERTREV ) || defined( USE_CERTVAL )
  #define MKPERM_CERTREV_VAL( perm )	ACCESS_##perm
  #define MKPERM_INT_CERTREV_VAL( perm ) ACCESS_INT_##perm
#else
  #define MKPERM_CERTREV_VAL( perm )	ACCESS_xxx_xxx
  #define MKPERM_INT_CERTREV_VAL( perm ) ACCESS_INT_xxx_xxx
#endif /* USE_CERTREV || USE_CERTVAL */

#ifdef USE_CMSATTR
  #define MKPERM_CMSATTR( perm )		ACCESS_##perm
  #define MKPERM_INT_CMSATTR( perm )	ACCESS_INT_##perm
  #define MKPERM_SPECIAL_CMSATTR( perm ) ACCESS_SPECIAL_##perm
#else
  #define MKPERM_CMSATTR( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_CMSATTR( perm )	ACCESS_INT_xxx_xxx
  #define MKPERM_SPECIAL_CMSATTR( perm ) ACCESS_xxx_xxx
#endif /* USE_CMSATTR */

#ifdef USE_PKIUSER
  #define MKPERM_PKIUSER( perm )		ACCESS_##perm
  #define MKPERM_INT_PKIUSER( perm )	ACCESS_INT_##perm
#else
  #define MKPERM_PKIUSER( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_PKIUSER( perm )	ACCESS_INT_xxx_xxx
#endif /* USE_PKIUSER */

#ifdef USE_CERT_OBSOLETE
  #define MKPERM_CERT_OBSOLETE( perm )	ACCESS_##perm
#else
  #define MKPERM_CERT_OBSOLETE( perm )	ACCESS_xxx_xxx
#endif /* USE_CERT_OBSOLETE */

#ifdef USE_CERT_DNSTRING
  #define MKPERM_DNSTRING( perm )		ACCESS_##perm
#else
  #define MKPERM_DNSTRING( perm )		ACCESS_xxx_xxx
#endif /* USE_CERT_DNSTRING */

#ifdef USE_CMSATTR_OBSCURE
  #define MKPERM_CMSATTR_OBSCURE( perm ) ACCESS_##perm
#else
  #define MKPERM_CMSATTR_OBSCURE( perm ) ACCESS_xxx_xxx
#endif /* USE_CMSATTR_OBSCURE */

#if defined( USE_CMSATTR_OBSCURE ) || defined( USE_TSP )
  #define MKPERM_CMSATTR_OBSCURE_TSP( perm )	ACCESS_##perm
#else
  #define MKPERM_CMSATTR_OBSCURE_TSP( perm )	ACCESS_xxx_xxx
#endif /* USE_CMSATTR_OBSCURE || USE_TSP */

#ifdef USE_CERTLEVEL_PKIX_PARTIAL
  #define MKPERM_CERT_PKIX_PARTIAL( perm ) ACCESS_##perm
#else
  #define MKPERM_CERT_PKIX_PARTIAL( perm ) ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_PARTIAL */

#if defined( USE_CERTLEVEL_PKIX_PARTIAL ) && defined( USE_CERTREV )
  #define MKPERM_CERTREV_PKIX_PARTIAL( perm )	ACCESS_##perm
#else
  #define MKPERM_CERTREV_PKIX_PARTIAL( perm )	ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_PARTIAL && USE_CERTREV */
#if defined( USE_CERTLEVEL_PKIX_FULL ) && defined( USE_CERTREV )
  #define MKPERM_CERTREV_PKIX_FULL( perm )		ACCESS_##perm
#else
  #define MKPERM_CERTREV_PKIX_FULL( perm )		ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_FULL && USE_CERTREV */

#if defined( USE_CERTLEVEL_PKIX_PARTIAL ) && defined( USE_CERT_OBSCURE )
  #define MKPERM_PKIX_PARTIAL_OBSCURE( perm )	ACCESS_##perm
#else
  #define MKPERM_PKIX_PARTIAL_OBSCURE( perm )	ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_PARTIAL && USE_CERT_OBSCURE */

#ifdef USE_CERTLEVEL_PKIX_FULL
  #define MKPERM_CERT_PKIX_FULL( perm )	ACCESS_##perm
#else
  #define MKPERM_CERT_PKIX_FULL( perm )	ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_FULL */

#if defined( USE_CERTLEVEL_PKIX_FULL ) && \
	( defined( USE_CERTREQ ) || defined( USE_CERTREV ) )
  #define MKPERM_CERTREQ_REV_PKIX_FULL( perm )	ACCESS_##perm
#else
  #define MKPERM_CERTREQ_REV_PKIX_FULL( perm )	ACCESS_xxx_xxx
#endif /* USE_CERTLEVEL_PKIX_FULL && ( USE_CERTREQ || USE_CERTREV ) */

/* Device ACL macros */

#ifdef USE_PKCS11
  #define MKPERM_PKCS11( perm )			ACCESS_##perm
  #define MKPERM_INT_PKCS11( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_PKCS11( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_PKCS11( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_PKCS11 */

/* Envelope ACL macros */

#ifdef USE_ENVELOPES
  #define MKPERM_ENVELOPE( perm )		ACCESS_##perm
  #define MKPERM_INT_ENVELOPE( perm )	ACCESS_INT_##perm
#else
  #define MKPERM_ENVELOPE( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_ENVELOPE( perm )	ACCESS_INT_xxx_xxx
#endif /* USE_ENVELOPES */

#ifdef USE_COMPRESSION
  #define MKPERM_COMPRESSION( perm )	ACCESS_##perm
  #define MKPERM_INT_COMPRESSION( perm ) ACCESS_INT_##perm
#else
  #define MKPERM_COMPRESSION( perm )	ACCESS_xxx_xxx
  #define MKPERM_INT_COMPRESSION( perm ) ACCESS_INT_xxx_xxx
#endif /* USE_COMPRESSION */

#if defined( USE_PGP ) || defined( USE_PGPKEYS )
  #define MKPERM_PGP( perm )			ACCESS_##perm
  #define MKPERM_INT_PGP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_PGP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_PGP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_PGP || USE_PGPKEYS */

/* Keyset ACL macros */

#ifdef USE_DBMS
  #define MKPERM_DBMS( perm )			ACCESS_##perm
  #define MKPERM_INT_DBMS( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_DBMS( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_DBMS( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_DBMS */

#ifdef USE_LDAP
  #define MKPERM_LDAP( perm )			ACCESS_##perm
  #define MKPERM_INT_LDAP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_LDAP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_LDAP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_LDAP */

#ifdef USE_HTTP
  #define MKPERM_HTTP( perm )			ACCESS_##perm
  #define MKPERM_INT_HTTP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_HTTP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_HTTP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_HTTP */

/* Session ACL macros */

#ifdef USE_TCP
  #define MKPERM_TCP( perm )			ACCESS_##perm
  #define MKPERM_INT_TCP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_TCP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_TCP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_TCP */

#ifdef USE_SESSIONS
  #define MKPERM_SESSIONS( perm )		ACCESS_##perm
  #define MKPERM_INT_SESSIONS( perm ) ACCESS_INT_##perm
#else
  #define MKPERM_SESSIONS( perm )		ACCESS_xxx_xxx
  #define MKPERM_INT_SESSIONS( perm ) ACCESS_INT_xxx_xxx
#endif /* USE_SESSIONS */

#ifdef USE_CMP
  #define MKPERM_CMP( perm )			ACCESS_##perm
  #define MKPERM_INT_CMP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_CMP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_CMP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_CMP */

#ifdef USE_SCEP
  #define MKPERM_SCEP( perm )			ACCESS_##perm
#else
  #define MKPERM_SCEP( perm )			ACCESS_xxx_xxx
#endif /* USE_SCEP */

#ifdef USE_SSH
  #define MKPERM_SSH( perm )			ACCESS_##perm
  #define MKPERM_INT_SSH( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_SSH( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_SSH( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_SSH */

#ifdef USE_SSH_EXTENDED
  #define MKPERM_SSH_EXT( perm )		ACCESS_##perm
#else
  #define MKPERM_SSH_EXT( perm )		ACCESS_xxx_xxx
#endif /* USE_SSH_EXTENDED */

#ifdef USE_SSL
  #define MKPERM_SSL( perm )			ACCESS_##perm
  #define MKPERM_INT_SSL( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_SSL( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_SSL( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_SSL */

#ifdef USE_TSP
  #define MKPERM_TSP( perm )			ACCESS_##perm
  #define MKPERM_INT_TSP( perm )		ACCESS_INT_##perm
#else
  #define MKPERM_TSP( perm )			ACCESS_xxx_xxx
  #define MKPERM_INT_TSP( perm )		ACCESS_INT_xxx_xxx
#endif /* USE_TSP */

#endif /* _ACL_PERM_DEFINED */
