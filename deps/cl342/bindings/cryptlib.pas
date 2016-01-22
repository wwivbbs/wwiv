unit cryptlib;

interface

{****************************************************************************
*                                                                           *
*                     Cryptlib external API interface                       *
*                    Copyright Peter Gutmann 1997-2012                      *
*                                                                           *
*        adapted for Delphi Version 5 (32 bit) and Kylix Version 3          *
*                              by W. Gothier                                *
****************************************************************************}


{------------------------------------------------------------------------------

 This file has been created automatically by a perl script from the file:

 "cryptlib.h" dated Wed Aug 29 15:34:08 2012, filesize = 97645.

 Please check twice that the file matches the version of cryptlib.h
 in your cryptlib source! If this is not the right version, try to download an
 update from "http://cryptlib.sogot.de/". If the filesize or file creation
 date do not match, then please do not complain about problems.

 Published by W. Gothier, 
 mailto: problems@cryptlib.sogot.de if you find errors in this file.

-------------------------------------------------------------------------------}

{$A+}  {Set Alignment on}
{$F+}  {Force function calls to FAR}
{$Z+}  {Force all enumeration values to Integer size}

const
  {$IFDEF WIN32}
    cryptlibname = 'CL32.DLL';  { dynamic linkname for Windows (Delphi) }
  {$ELSE}
    cryptlibname = 'libcl.so';  { library name for Unix/Linux  (Kylix) }
                 { symbolic link should be used for libcl.so -> libcl.so.3.x.y }
  {$ENDIF}


const
  CRYPTLIB_VERSION = 3410;

{****************************************************************************
*                                                                           *
*                           Algorithm and Object Types                      *
*                                                                           *
****************************************************************************}

{  Algorithm and mode types  }


type
 CRYPT_ALGO_TYPE = Integer;
const
  { Algorithms }
  { No encryption }
  CRYPT_ALGO_NONE = 0;  { No encryption }
  
  { Conventional encryption }
  CRYPT_ALGO_DES = 1;  { DES }
  CRYPT_ALGO_3DES = 2;  { Triple DES }
  CRYPT_ALGO_IDEA = 3;  { IDEA (only used for PGP 2.x) }
  CRYPT_ALGO_CAST = 4;  { CAST-128 (only used for OpenPGP) }
  CRYPT_ALGO_RC2 = 5;  { RC2 (disabled by default) }
  CRYPT_ALGO_RC4 = 6;  { RC4 }
  CRYPT_ALGO_RC5 = 7;  { RC5 }
  CRYPT_ALGO_AES = 8;  { AES }
  CRYPT_ALGO_BLOWFISH = 9;  { Blowfish }
  
  { Public-key encryption }
  CRYPT_ALGO_DH = 100;  { Diffie-Hellman }
  CRYPT_ALGO_RSA = 101;  { RSA }
  CRYPT_ALGO_DSA = 102;  { DSA }
  CRYPT_ALGO_ELGAMAL = 103;  { ElGamal }
  CRYPT_ALGO_RESERVED1 = 104;  { Formerly KEA }
  CRYPT_ALGO_ECDSA = 105;  { ECDSA }
  CRYPT_ALGO_ECDH = 106;  { ECDH }
  
  { Hash algorithms }
  CRYPT_ALGO_RESERVED2 = 200;  { Formerly MD2 }
  CRYPT_ALGO_RESERVED3 = 201;  { Formerly MD4 }
  CRYPT_ALGO_MD5 = 202;  { MD5 }
  CRYPT_ALGO_SHA1 = 203;  { SHA/SHA1 }
  CRYPT_ALGO_RIPEMD160 = 204;  { RIPE-MD 160 }
  CRYPT_ALGO_SHA2 = 205;  { SHA-256 }
  CRYPT_ALGO_SHA256 = 205; { = CRYPT_ALGO_SHA2 }  { Alternate name }
  CRYPT_ALGO_SHAng = 206;  { Future SHA-nextgen standard }
  
  { MAC's }
  CRYPT_ALGO_HMAC_MD5 = 300;  { HMAC-MD5 }
  CRYPT_ALGO_HMAC_SHA1 = 301;  { HMAC-SHA }
  CRYPT_ALGO_HMAC_RIPEMD160 = 302;  { HMAC-RIPEMD-160 }
  CRYPT_ALGO_HMAC_SHA2 = 303;  { HMAC-SHA2 }
  CRYPT_ALGO_HMAC_SHAng = 304;  { HMAC-future-SHA-nextgen }
  
  
  { Vendors may want to use their own algorithms that aren't part of the
  general cryptlib suite.  The following values are for vendor-defined
  algorithms, and can be used just like the named algorithm types (it's
  up to the vendor to keep track of what _VENDOR1 actually corresponds
  to) }
  
  CRYPT_ALGO_LAST = 305;  { Last possible crypt algo value }
  
  { In order that we can scan through a range of algorithms with
  cryptQueryCapability(), we define the following boundary points for
  each algorithm class }
  CRYPT_ALGO_FIRST_CONVENTIONAL = 1;  
  CRYPT_ALGO_LAST_CONVENTIONAL = 99;  
  CRYPT_ALGO_FIRST_PKC = 100;  
  CRYPT_ALGO_LAST_PKC = 199;  
  CRYPT_ALGO_FIRST_HASH = 200;  
  CRYPT_ALGO_LAST_HASH = 299;  
  CRYPT_ALGO_FIRST_MAC = 300;  
  CRYPT_ALGO_LAST_MAC = 399;  
  




type
  CRYPT_MODE_TYPE = (                        {  Block cipher modes  }
    CRYPT_MODE_NONE,                {  No encryption mode  }
    CRYPT_MODE_ECB,                 {  ECB  }
    CRYPT_MODE_CBC,                 {  CBC  }
    CRYPT_MODE_CFB,                 {  CFB  }
    CRYPT_MODE_OFB,                 {  OFB  }
    CRYPT_MODE_GCM,                 {  GCM  }
    CRYPT_MODE_LAST                 {  Last possible crypt mode value  }
    
  );


{  Keyset subtypes  }

  CRYPT_KEYSET_TYPE = (                        {  Keyset types  }
    CRYPT_KEYSET_NONE,              {  No keyset type  }
    CRYPT_KEYSET_FILE,              {  Generic flat file keyset  }
    CRYPT_KEYSET_HTTP,              {  Web page containing cert/CRL  }
    CRYPT_KEYSET_LDAP,              {  LDAP directory service  }
    CRYPT_KEYSET_ODBC,              {  Generic ODBC interface  }
    CRYPT_KEYSET_DATABASE,          {  Generic RDBMS interface  }
    CRYPT_KEYSET_ODBC_STORE,        {  ODBC certificate store  }
    CRYPT_KEYSET_DATABASE_STORE,    {  Database certificate store  }
    CRYPT_KEYSET_LAST               {  Last possible keyset type  }

    
  );

{  Device subtypes  }

  CRYPT_DEVICE_TYPE = (                        {  Crypto device types  }
    CRYPT_DEVICE_NONE,              {  No crypto device  }
    CRYPT_DEVICE_FORTEZZA,          {  Fortezza card - Placeholder only  }
    CRYPT_DEVICE_PKCS11,            {  PKCS #11 crypto token  }
    CRYPT_DEVICE_CRYPTOAPI,         {  Microsoft CryptoAPI  }
    CRYPT_DEVICE_HARDWARE,          {  Generic crypo HW plugin  }
    CRYPT_DEVICE_LAST               {  Last possible crypto device type  }
    
  );

{  Certificate subtypes  }

  CRYPT_CERTTYPE_TYPE = (                        {  Certificate object types  }
    CRYPT_CERTTYPE_NONE,            {  No certificate type  }
    CRYPT_CERTTYPE_CERTIFICATE,     {  Certificate  }
    CRYPT_CERTTYPE_ATTRIBUTE_CERT,  {  Attribute certificate  }
    CRYPT_CERTTYPE_CERTCHAIN,       {  PKCS #7 certificate chain  }
    CRYPT_CERTTYPE_CERTREQUEST,     {  PKCS #10 certification request  }
    CRYPT_CERTTYPE_REQUEST_CERT,    {  CRMF certification request  }
    CRYPT_CERTTYPE_REQUEST_REVOCATION,  {  CRMF revocation request  }
    CRYPT_CERTTYPE_CRL,             {  CRL  }
    CRYPT_CERTTYPE_CMS_ATTRIBUTES,  {  CMS attributes  }
    CRYPT_CERTTYPE_RTCS_REQUEST,    {  RTCS request  }
    CRYPT_CERTTYPE_RTCS_RESPONSE,   {  RTCS response  }
    CRYPT_CERTTYPE_OCSP_REQUEST,    {  OCSP request  }
    CRYPT_CERTTYPE_OCSP_RESPONSE,   {  OCSP response  }
    CRYPT_CERTTYPE_PKIUSER,         {  PKI user information  }
    CRYPT_CERTTYPE_LAST             {  Last possible cert.type  }
    
  );

{  Envelope/data format subtypes  }

  CRYPT_FORMAT_TYPE = (  
    CRYPT_FORMAT_NONE,              {  No format type  }
    CRYPT_FORMAT_AUTO,              {  Deenv, auto-determine type  }
    CRYPT_FORMAT_CRYPTLIB,          {  cryptlib native format  }
    CRYPT_FORMAT_CMS,               {  PKCS #7 / CMS / S/MIME fmt.}
    CRYPT_FORMAT_SMIME,             {  As CMS with MSG-style behaviour  }
    CRYPT_FORMAT_PGP,               {  PGP format  }
    CRYPT_FORMAT_LAST               {  Last possible format type  }
    
  );

const
  CRYPT_FORMAT_PKCS7: CRYPT_FORMAT_TYPE = CRYPT_FORMAT_CMS;

{  Session subtypes  }


type
  CRYPT_SESSION_TYPE = (  
    CRYPT_SESSION_NONE,             {  No session type  }
    CRYPT_SESSION_SSH,              {  SSH  }
    CRYPT_SESSION_SSH_SERVER,       {  SSH server  }
    CRYPT_SESSION_SSL,              {  SSL/TLS  }
    CRYPT_SESSION_SSL_SERVER,       {  SSL/TLS server  }
    CRYPT_SESSION_RTCS,             {  RTCS  }
    CRYPT_SESSION_RTCS_SERVER,      {  RTCS server  }
    CRYPT_SESSION_OCSP,             {  OCSP  }
    CRYPT_SESSION_OCSP_SERVER,      {  OCSP server  }
    CRYPT_SESSION_TSP,              {  TSP  }
    CRYPT_SESSION_TSP_SERVER,       {  TSP server  }
    CRYPT_SESSION_CMP,              {  CMP  }
    CRYPT_SESSION_CMP_SERVER,       {  CMP server  }
    CRYPT_SESSION_SCEP,             {  SCEP  }
    CRYPT_SESSION_SCEP_SERVER,      {  SCEP server  }
    CRYPT_SESSION_CERTSTORE_SERVER, {  HTTP cert store interface  }
    CRYPT_SESSION_LAST              {  Last possible session type  }
    
  );

{  User subtypes  }

  CRYPT_USER_TYPE = (  
    CRYPT_USER_NONE,                {  No user type  }
    CRYPT_USER_NORMAL,              {  Normal user  }
    CRYPT_USER_SO,                  {  Security officer  }
    CRYPT_USER_CA,                  {  CA user  }
    CRYPT_USER_LAST                 {  Last possible user type  }
    
  );

{****************************************************************************
*                                                                           *
*                               Attribute Types                             *
*                                                                           *
****************************************************************************}

{  Attribute types.  These are arranged in the following order:

    PROPERTY    - Object property
    ATTRIBUTE   - Generic attributes
    OPTION      - Global or object-specific config.option
    CTXINFO     - Context-specific attribute
    CERTINFO    - Certificate-specific attribute
    KEYINFO     - Keyset-specific attribute
    DEVINFO     - Device-specific attribute
    ENVINFO     - Envelope-specific attribute
    SESSINFO    - Session-specific attribute
    USERINFO    - User-specific attribute  }

 CRYPT_ATTRIBUTE_TYPE = Integer;
const
  
  CRYPT_ATTRIBUTE_NONE = 0;  { Non-value }
  
  { Used internally }
  CRYPT_PROPERTY_FIRST = 1;  
  
  {*******************}
  { Object attributes }
  {*******************}
  
  { Object properties }
  CRYPT_PROPERTY_HIGHSECURITY = 2;  { Owned+non-forwardcount+locked }
  CRYPT_PROPERTY_OWNER = 3;  { Object owner }
  CRYPT_PROPERTY_FORWARDCOUNT = 4;  { No.of times object can be forwarded }
  CRYPT_PROPERTY_LOCKED = 5;  { Whether properties can be chged/read }
  CRYPT_PROPERTY_USAGECOUNT = 6;  { Usage count before object expires }
  CRYPT_PROPERTY_NONEXPORTABLE = 7;  { Whether key is nonexp.from context }
  
  { Used internally }
  CRYPT_PROPERTY_LAST = 8;  CRYPT_GENERIC_FIRST = 9;  
  
  { Extended error information }
  CRYPT_ATTRIBUTE_ERRORTYPE = 10;  { Type of last error }
  CRYPT_ATTRIBUTE_ERRORLOCUS = 11;  { Locus of last error }
  CRYPT_ATTRIBUTE_ERRORMESSAGE = 12;  { Detailed error description }
  
  { Generic information }
  CRYPT_ATTRIBUTE_CURRENT_GROUP = 13;  { Cursor mgt: Group in attribute list }
  CRYPT_ATTRIBUTE_CURRENT = 14;  { Cursor mgt: Entry in attribute list }
  CRYPT_ATTRIBUTE_CURRENT_INSTANCE = 15;  { Cursor mgt: Instance in attribute list }
  CRYPT_ATTRIBUTE_BUFFERSIZE = 16;  { Internal data buffer size }
  
  { User internally }
  CRYPT_GENERIC_LAST = 17;  CRYPT_OPTION_FIRST = 100;  
  
  {**************************}
  { Configuration attributes }
  {**************************}
  
  { cryptlib information (read-only) }
  CRYPT_OPTION_INFO_DESCRIPTION = 101;  { Text description }
  CRYPT_OPTION_INFO_COPYRIGHT = 102;  { Copyright notice }
  CRYPT_OPTION_INFO_MAJORVERSION = 103;  { Major release version }
  CRYPT_OPTION_INFO_MINORVERSION = 104;  { Minor release version }
  CRYPT_OPTION_INFO_STEPPING = 105;  { Release stepping }
  
  { Encryption options }
  CRYPT_OPTION_ENCR_ALGO = 106;  { Encryption algorithm }
  CRYPT_OPTION_ENCR_HASH = 107;  { Hash algorithm }
  CRYPT_OPTION_ENCR_MAC = 108;  { MAC algorithm }
  
  { PKC options }
  CRYPT_OPTION_PKC_ALGO = 109;  { Public-key encryption algorithm }
  CRYPT_OPTION_PKC_KEYSIZE = 110;  { Public-key encryption key size }
  
  { Signature options }
  CRYPT_OPTION_SIG_ALGO = 111;  { Signature algorithm }
  CRYPT_OPTION_SIG_KEYSIZE = 112;  { Signature keysize }
  
  { Keying options }
  CRYPT_OPTION_KEYING_ALGO = 113;  { Key processing algorithm }
  CRYPT_OPTION_KEYING_ITERATIONS = 114;  { Key processing iterations }
  
  { Certificate options }
  CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES = 115;  { Whether to sign unrecog.attrs }
  CRYPT_OPTION_CERT_VALIDITY = 116;  { Certificate validity period }
  CRYPT_OPTION_CERT_UPDATEINTERVAL = 117;  { CRL update interval }
  CRYPT_OPTION_CERT_COMPLIANCELEVEL = 118;  { PKIX compliance level for cert chks.}
  CRYPT_OPTION_CERT_REQUIREPOLICY = 119;  { Whether explicit policy req'd for certs }
  
  { CMS/SMIME options }
  CRYPT_OPTION_CMS_DEFAULTATTRIBUTES = 120;  { Add default CMS attributes }
  CRYPT_OPTION_SMIME_DEFAULTATTRIBUTES = 120; { = CRYPT_OPTION_CMS_DEFAULTATTRIBUTES }  
  
  { LDAP keyset options }
  CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS = 121;  { Object class }
  CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE = 122;  { Object type to fetch }
  CRYPT_OPTION_KEYS_LDAP_FILTER = 123;  { Query filter }
  CRYPT_OPTION_KEYS_LDAP_CACERTNAME = 124;  { CA certificate attribute name }
  CRYPT_OPTION_KEYS_LDAP_CERTNAME = 125;  { Certificate attribute name }
  CRYPT_OPTION_KEYS_LDAP_CRLNAME = 126;  { CRL attribute name }
  CRYPT_OPTION_KEYS_LDAP_EMAILNAME = 127;  { Email attribute name }
  
  { Crypto device options }
  CRYPT_OPTION_DEVICE_PKCS11_DVR01 = 128;  { Name of first PKCS #11 driver }
  CRYPT_OPTION_DEVICE_PKCS11_DVR02 = 129;  { Name of second PKCS #11 driver }
  CRYPT_OPTION_DEVICE_PKCS11_DVR03 = 130;  { Name of third PKCS #11 driver }
  CRYPT_OPTION_DEVICE_PKCS11_DVR04 = 131;  { Name of fourth PKCS #11 driver }
  CRYPT_OPTION_DEVICE_PKCS11_DVR05 = 132;  { Name of fifth PKCS #11 driver }
  CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY = 133;  { Use only hardware mechanisms }
  
  { Network access options }
  CRYPT_OPTION_NET_SOCKS_SERVER = 134;  { Socks server name }
  CRYPT_OPTION_NET_SOCKS_USERNAME = 135;  { Socks user name }
  CRYPT_OPTION_NET_HTTP_PROXY = 136;  { Web proxy server }
  CRYPT_OPTION_NET_CONNECTTIMEOUT = 137;  { Timeout for network connection setup }
  CRYPT_OPTION_NET_READTIMEOUT = 138;  { Timeout for network reads }
  CRYPT_OPTION_NET_WRITETIMEOUT = 139;  { Timeout for network writes }
  
  { Miscellaneous options }
  CRYPT_OPTION_MISC_ASYNCINIT = 140;  { Whether to init cryptlib async'ly }
  CRYPT_OPTION_MISC_SIDECHANNELPROTECTION = 141;  { Protect against side-channel attacks }
  
  { cryptlib state information }
  CRYPT_OPTION_CONFIGCHANGED = 142;  { Whether in-mem.opts match on-disk ones }
  CRYPT_OPTION_SELFTESTOK = 143;  { Whether self-test was completed and OK }
  
  { Used internally }
  CRYPT_OPTION_LAST = 144;  CRYPT_CTXINFO_FIRST = 1000;  
  
  {********************}
  { Context attributes }
  {********************}
  
  { Algorithm and mode information }
  CRYPT_CTXINFO_ALGO = 1001;  { Algorithm }
  CRYPT_CTXINFO_MODE = 1002;  { Mode }
  CRYPT_CTXINFO_NAME_ALGO = 1003;  { Algorithm name }
  CRYPT_CTXINFO_NAME_MODE = 1004;  { Mode name }
  CRYPT_CTXINFO_KEYSIZE = 1005;  { Key size in bytes }
  CRYPT_CTXINFO_BLOCKSIZE = 1006;  { Block size }
  CRYPT_CTXINFO_IVSIZE = 1007;  { IV size }
  CRYPT_CTXINFO_KEYING_ALGO = 1008;  { Key processing algorithm }
  CRYPT_CTXINFO_KEYING_ITERATIONS = 1009;  { Key processing iterations }
  CRYPT_CTXINFO_KEYING_SALT = 1010;  { Key processing salt }
  CRYPT_CTXINFO_KEYING_VALUE = 1011;  { Value used to derive key }
  
  { State information }
  CRYPT_CTXINFO_KEY = 1012;  { Key }
  CRYPT_CTXINFO_KEY_COMPONENTS = 1013;  { Public-key components }
  CRYPT_CTXINFO_IV = 1014;  { IV }
  CRYPT_CTXINFO_HASHVALUE = 1015;  { Hash value }
  
  { Misc.information }
  CRYPT_CTXINFO_LABEL = 1016;  { Label for private/secret key }
  CRYPT_CTXINFO_PERSISTENT = 1017;  { Obj.is backed by device or keyset }
  
  { Used internally }
  CRYPT_CTXINFO_LAST = 1018;  CRYPT_CERTINFO_FIRST = 2000;  
  
  {************************}
  { Certificate attributes }
  {************************}
  
  { Because there are so many cert attributes, we break them down into
  blocks to minimise the number of values that change if a new one is
  added halfway through }
  
  { Pseudo-information on a cert object or meta-information which is used
  to control the way that a cert object is processed }
  CRYPT_CERTINFO_SELFSIGNED = 2001;  { Cert is self-signed }
  CRYPT_CERTINFO_IMMUTABLE = 2002;  { Cert is signed and immutable }
  CRYPT_CERTINFO_XYZZY = 2003;  { Cert is a magic just-works cert }
  CRYPT_CERTINFO_CERTTYPE = 2004;  { Certificate object type }
  CRYPT_CERTINFO_FINGERPRINT = 2005;  { Certificate fingerprints }
  CRYPT_CERTINFO_FINGERPRINT_MD5 = 2005; { = CRYPT_CERTINFO_FINGERPRINT }  
  CRYPT_CERTINFO_FINGERPRINT_SHA1 = 2006;  
  CRYPT_CERTINFO_FINGERPRINT_SHA = 2006; { = CRYPT_CERTINFO_FINGERPRINT_SHA1 }  
  CRYPT_CERTINFO_FINGERPRINT_SHA2 = 2007;  
  CRYPT_CERTINFO_FINGERPRINT_SHAng = 2008;  
  CRYPT_CERTINFO_CURRENT_CERTIFICATE = 2009;  { Cursor mgt: Rel.pos in chain/CRL/OCSP }
  CRYPT_CERTINFO_TRUSTED_USAGE = 2010;  { Usage that cert is trusted for }
  CRYPT_CERTINFO_TRUSTED_IMPLICIT = 2011;  { Whether cert is implicitly trusted }
  CRYPT_CERTINFO_SIGNATURELEVEL = 2012;  { Amount of detail to include in sigs.}
  
  { General certificate object information }
  CRYPT_CERTINFO_VERSION = 2013;  { Cert.format version }
  CRYPT_CERTINFO_SERIALNUMBER = 2014;  { Serial number }
  CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO = 2015;  { Public key }
  CRYPT_CERTINFO_CERTIFICATE = 2016;  { User certificate }
  CRYPT_CERTINFO_USERCERTIFICATE = 2016; { = CRYPT_CERTINFO_CERTIFICATE }  
  CRYPT_CERTINFO_CACERTIFICATE = 2017;  { CA certificate }
  CRYPT_CERTINFO_ISSUERNAME = 2018;  { Issuer DN }
  CRYPT_CERTINFO_VALIDFROM = 2019;  { Cert valid-from time }
  CRYPT_CERTINFO_VALIDTO = 2020;  { Cert valid-to time }
  CRYPT_CERTINFO_SUBJECTNAME = 2021;  { Subject DN }
  CRYPT_CERTINFO_ISSUERUNIQUEID = 2022;  { Issuer unique ID }
  CRYPT_CERTINFO_SUBJECTUNIQUEID = 2023;  { Subject unique ID }
  CRYPT_CERTINFO_CERTREQUEST = 2024;  { Cert.request (DN + public key) }
  CRYPT_CERTINFO_THISUPDATE = 2025;  { CRL/OCSP current-update time }
  CRYPT_CERTINFO_NEXTUPDATE = 2026;  { CRL/OCSP next-update time }
  CRYPT_CERTINFO_REVOCATIONDATE = 2027;  { CRL/OCSP cert-revocation time }
  CRYPT_CERTINFO_REVOCATIONSTATUS = 2028;  { OCSP revocation status }
  CRYPT_CERTINFO_CERTSTATUS = 2029;  { RTCS certificate status }
  CRYPT_CERTINFO_DN = 2030;  { Currently selected DN in string form }
  CRYPT_CERTINFO_PKIUSER_ID = 2031;  { PKI user ID }
  CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD = 2032;  { PKI user issue password }
  CRYPT_CERTINFO_PKIUSER_REVPASSWORD = 2033;  { PKI user revocation password }
  
  { X.520 Distinguished Name components.  This is a composite field, the
  DN to be manipulated is selected through the addition of a
  pseudocomponent, and then one of the following is used to access the
  DN components directly }
  CRYPT_CERTINFO_COUNTRYNAME = 2100;  { countryName }
  CRYPT_CERTINFO_STATEORPROVINCENAME = 2101;  { stateOrProvinceName }
  CRYPT_CERTINFO_LOCALITYNAME = 2102;  { localityName }
  CRYPT_CERTINFO_ORGANIZATIONNAME = 2103;  { organizationName }
  CRYPT_CERTINFO_ORGANISATIONNAME = 2103; { = CRYPT_CERTINFO_ORGANIZATIONNAME }  
  CRYPT_CERTINFO_ORGANIZATIONALUNITNAME = 2104;  { organizationalUnitName }
  CRYPT_CERTINFO_ORGANISATIONALUNITNAME = 2104; { = CRYPT_CERTINFO_ORGANIZATIONALUNITNAME }  
  CRYPT_CERTINFO_COMMONNAME = 2105;  { commonName }
  
  { X.509 General Name components.  These are handled in the same way as
  the DN composite field, with the current GeneralName being selected by
  a pseudo-component after which the individual components can be
  modified through one of the following }
  CRYPT_CERTINFO_OTHERNAME_TYPEID = 2106;  { otherName.typeID }
  CRYPT_CERTINFO_OTHERNAME_VALUE = 2107;  { otherName.value }
  CRYPT_CERTINFO_RFC822NAME = 2108;  { rfc822Name }
  CRYPT_CERTINFO_EMAIL = 2108; { = CRYPT_CERTINFO_RFC822NAME }  
  CRYPT_CERTINFO_DNSNAME = 2109;  { dNSName }
  CRYPT_CERTINFO_DIRECTORYNAME = 2110;  { directoryName }
  CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER = 2111;  { ediPartyName.nameAssigner }
  CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME = 2112;  { ediPartyName.partyName }
  CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER = 2113;  { uniformResourceIdentifier }
  CRYPT_CERTINFO_URL = 2113; { = CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER }  
  CRYPT_CERTINFO_IPADDRESS = 2114;  { iPAddress }
  CRYPT_CERTINFO_REGISTEREDID = 2115;  { registeredID }
  
  { X.509 certificate extensions.  Although it would be nicer to use names
  that match the extensions more closely (e.g.
  CRYPT_CERTINFO_BASICCONSTRAINTS_PATHLENCONSTRAINT), these exceed the
  32-character ANSI minimum length for unique names, and get really
  hairy once you get into the weird policy constraints extensions whose
  names wrap around the screen about three times.
  
  The following values are defined in OID order, this isn't absolutely
  necessary but saves an extra layer of processing when encoding them }
  
  { 1 2 840 113549 1 9 7 challengePassword.  This is here even though it's
  a CMS attribute because SCEP stuffs it into PKCS #10 requests }
  CRYPT_CERTINFO_CHALLENGEPASSWORD = 2200;  
  
  { 1 3 6 1 4 1 3029 3 1 4 cRLExtReason }
  CRYPT_CERTINFO_CRLEXTREASON = 2201;  
  
  { 1 3 6 1 4 1 3029 3 1 5 keyFeatures }
  CRYPT_CERTINFO_KEYFEATURES = 2202;  
  
  { 1 3 6 1 5 5 7 1 1 authorityInfoAccess }
  CRYPT_CERTINFO_AUTHORITYINFOACCESS = 2203;  
  CRYPT_CERTINFO_AUTHORITYINFO_RTCS = 2204;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_AUTHORITYINFO_OCSP = 2205;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS = 2206;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE = 2207;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_AUTHORITYINFO_CRLS = 2208;  { accessDescription.accessLocation }
  
  { 1 3 6 1 5 5 7 1 2 biometricInfo }
  CRYPT_CERTINFO_BIOMETRICINFO = 2209;  
  CRYPT_CERTINFO_BIOMETRICINFO_TYPE = 2210;  { biometricData.typeOfData }
  CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO = 2211;  { biometricData.hashAlgorithm }
  CRYPT_CERTINFO_BIOMETRICINFO_HASH = 2212;  { biometricData.dataHash }
  CRYPT_CERTINFO_BIOMETRICINFO_URL = 2213;  { biometricData.sourceDataUri }
  
  { 1 3 6 1 5 5 7 1 3 qcStatements }
  CRYPT_CERTINFO_QCSTATEMENT = 2214;  
  CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS = 2215;  
  { qcStatement.statementInfo.semanticsIdentifier }
  CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY = 2216;  
  { qcStatement.statementInfo.nameRegistrationAuthorities }
  
  { 1 3 6 1 5 5 7 1 7 ipAddrBlocks }
  CRYPT_CERTINFO_IPADDRESSBLOCKS = 2217;  
  CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY = 2218;  { addressFamily }
  {  CRYPT_CERTINFO_IPADDRESSBLOCKS_INHERIT, // ipAddress.inherit }
  CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX = 2219;  { ipAddress.addressPrefix }
  CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN = 2220;  { ipAddress.addressRangeMin }
  CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX = 2221;  { ipAddress.addressRangeMax }
  
  { 1 3 6 1 5 5 7 1 8 autonomousSysIds }
  CRYPT_CERTINFO_AUTONOMOUSSYSIDS = 2222;  
  {  CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_INHERIT,// asNum.inherit }
  CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID = 2223;  { asNum.id }
  CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN = 2224;  { asNum.min }
  CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX = 2225;  { asNum.max }
  
  { 1 3 6 1 5 5 7 48 1 2 ocspNonce }
  CRYPT_CERTINFO_OCSP_NONCE = 2226;  { nonce }
  
  { 1 3 6 1 5 5 7 48 1 4 ocspAcceptableResponses }
  CRYPT_CERTINFO_OCSP_RESPONSE = 2227;  
  CRYPT_CERTINFO_OCSP_RESPONSE_OCSP = 2228;  { OCSP standard response }
  
  { 1 3 6 1 5 5 7 48 1 5 ocspNoCheck }
  CRYPT_CERTINFO_OCSP_NOCHECK = 2229;  
  
  { 1 3 6 1 5 5 7 48 1 6 ocspArchiveCutoff }
  CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF = 2230;  
  
  { 1 3 6 1 5 5 7 48 1 11 subjectInfoAccess }
  CRYPT_CERTINFO_SUBJECTINFOACCESS = 2231;  
  CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING = 2232;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY = 2233;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY = 2234;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST = 2235;  { accessDescription.accessLocation }
  CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT = 2236;  { accessDescription.accessLocation }
  
  { 1 3 36 8 3 1 siggDateOfCertGen }
  CRYPT_CERTINFO_SIGG_DATEOFCERTGEN = 2237;  
  
  { 1 3 36 8 3 2 siggProcuration }
  CRYPT_CERTINFO_SIGG_PROCURATION = 2238;  
  CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY = 2239;  { country }
  CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION = 2240;  { typeOfSubstitution }
  CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR = 2241;  { signingFor.thirdPerson }
  
  { 1 3 36 8 3 3 siggAdmissions }
  CRYPT_CERTINFO_SIGG_ADMISSIONS = 2242;  
  CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY = 2243;  { authority }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID = 2244;  { namingAuth.iD }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL = 2245;  { namingAuth.uRL }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT = 2246;  { namingAuth.text }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM = 2247;  { professionItem }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID = 2248;  { professionOID }
  CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER = 2249;  { registrationNumber }
  
  { 1 3 36 8 3 4 siggMonetaryLimit }
  CRYPT_CERTINFO_SIGG_MONETARYLIMIT = 2250;  
  CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY = 2251;  { currency }
  CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT = 2252;  { amount }
  CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT = 2253;  { exponent }
  
  { 1 3 36 8 3 5 siggDeclarationOfMajority }
  CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY = 2254;  
  CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY = 2255;  { fullAgeAtCountry }
  
  { 1 3 36 8 3 8 siggRestriction }
  CRYPT_CERTINFO_SIGG_RESTRICTION = 2256;  
  
  { 1 3 36 8 3 13 siggCertHash }
  CRYPT_CERTINFO_SIGG_CERTHASH = 2257;  
  
  { 1 3 36 8 3 15 siggAdditionalInformation }
  CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION = 2258;  
  
  { 1 3 101 1 4 1 strongExtranet }
  CRYPT_CERTINFO_STRONGEXTRANET = 2259;  
  CRYPT_CERTINFO_STRONGEXTRANET_ZONE = 2260;  { sxNetIDList.sxNetID.zone }
  CRYPT_CERTINFO_STRONGEXTRANET_ID = 2261;  { sxNetIDList.sxNetID.id }
  
  { 2 5 29 9 subjectDirectoryAttributes }
  CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES = 2262;  
  CRYPT_CERTINFO_SUBJECTDIR_TYPE = 2263;  { attribute.type }
  CRYPT_CERTINFO_SUBJECTDIR_VALUES = 2264;  { attribute.values }
  
  { 2 5 29 14 subjectKeyIdentifier }
  CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER = 2265;  
  
  { 2 5 29 15 keyUsage }
  CRYPT_CERTINFO_KEYUSAGE = 2266;  
  
  { 2 5 29 16 privateKeyUsagePeriod }
  CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD = 2267;  
  CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE = 2268;  { notBefore }
  CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER = 2269;  { notAfter }
  
  { 2 5 29 17 subjectAltName }
  CRYPT_CERTINFO_SUBJECTALTNAME = 2270;  
  
  { 2 5 29 18 issuerAltName }
  CRYPT_CERTINFO_ISSUERALTNAME = 2271;  
  
  { 2 5 29 19 basicConstraints }
  CRYPT_CERTINFO_BASICCONSTRAINTS = 2272;  
  CRYPT_CERTINFO_CA = 2273;  { cA }
  CRYPT_CERTINFO_AUTHORITY = 2273; { = CRYPT_CERTINFO_CA }  
  CRYPT_CERTINFO_PATHLENCONSTRAINT = 2274;  { pathLenConstraint }
  
  { 2 5 29 20 cRLNumber }
  CRYPT_CERTINFO_CRLNUMBER = 2275;  
  
  { 2 5 29 21 cRLReason }
  CRYPT_CERTINFO_CRLREASON = 2276;  
  
  { 2 5 29 23 holdInstructionCode }
  CRYPT_CERTINFO_HOLDINSTRUCTIONCODE = 2277;  
  
  { 2 5 29 24 invalidityDate }
  CRYPT_CERTINFO_INVALIDITYDATE = 2278;  
  
  { 2 5 29 27 deltaCRLIndicator }
  CRYPT_CERTINFO_DELTACRLINDICATOR = 2279;  
  
  { 2 5 29 28 issuingDistributionPoint }
  CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT = 2280;  
  CRYPT_CERTINFO_ISSUINGDIST_FULLNAME = 2281;  { distributionPointName.fullName }
  CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY = 2282;  { onlyContainsUserCerts }
  CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY = 2283;  { onlyContainsCACerts }
  CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY = 2284;  { onlySomeReasons }
  CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL = 2285;  { indirectCRL }
  
  { 2 5 29 29 certificateIssuer }
  CRYPT_CERTINFO_CERTIFICATEISSUER = 2286;  
  
  { 2 5 29 30 nameConstraints }
  CRYPT_CERTINFO_NAMECONSTRAINTS = 2287;  
  CRYPT_CERTINFO_PERMITTEDSUBTREES = 2288;  { permittedSubtrees }
  CRYPT_CERTINFO_EXCLUDEDSUBTREES = 2289;  { excludedSubtrees }
  
  { 2 5 29 31 cRLDistributionPoint }
  CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT = 2290;  
  CRYPT_CERTINFO_CRLDIST_FULLNAME = 2291;  { distributionPointName.fullName }
  CRYPT_CERTINFO_CRLDIST_REASONS = 2292;  { reasons }
  CRYPT_CERTINFO_CRLDIST_CRLISSUER = 2293;  { cRLIssuer }
  
  { 2 5 29 32 certificatePolicies }
  CRYPT_CERTINFO_CERTIFICATEPOLICIES = 2294;  
  CRYPT_CERTINFO_CERTPOLICYID = 2295;  { policyInformation.policyIdentifier }
  CRYPT_CERTINFO_CERTPOLICY_CPSURI = 2296;  
  { policyInformation.policyQualifiers.qualifier.cPSuri }
  CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION = 2297;  
  { policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.organization }
  CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS = 2298;  
  { policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.noticeNumbers }
  CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT = 2299;  
  { policyInformation.policyQualifiers.qualifier.userNotice.explicitText }
  
  { 2 5 29 33 policyMappings }
  CRYPT_CERTINFO_POLICYMAPPINGS = 2300;  
  CRYPT_CERTINFO_ISSUERDOMAINPOLICY = 2301;  { policyMappings.issuerDomainPolicy }
  CRYPT_CERTINFO_SUBJECTDOMAINPOLICY = 2302;  { policyMappings.subjectDomainPolicy }
  
  { 2 5 29 35 authorityKeyIdentifier }
  CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER = 2303;  
  CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER = 2304;  { keyIdentifier }
  CRYPT_CERTINFO_AUTHORITY_CERTISSUER = 2305;  { authorityCertIssuer }
  CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER = 2306;  { authorityCertSerialNumber }
  
  { 2 5 29 36 policyConstraints }
  CRYPT_CERTINFO_POLICYCONSTRAINTS = 2307;  
  CRYPT_CERTINFO_REQUIREEXPLICITPOLICY = 2308;  { policyConstraints.requireExplicitPolicy }
  CRYPT_CERTINFO_INHIBITPOLICYMAPPING = 2309;  { policyConstraints.inhibitPolicyMapping }
  
  { 2 5 29 37 extKeyUsage }
  CRYPT_CERTINFO_EXTKEYUSAGE = 2310;  
  CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING = 2311;  { individualCodeSigning }
  CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING = 2312;  { commercialCodeSigning }
  CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING = 2313;  { certTrustListSigning }
  CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING = 2314;  { timeStampSigning }
  CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO = 2315;  { serverGatedCrypto }
  CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM = 2316;  { encrypedFileSystem }
  CRYPT_CERTINFO_EXTKEY_SERVERAUTH = 2317;  { serverAuth }
  CRYPT_CERTINFO_EXTKEY_CLIENTAUTH = 2318;  { clientAuth }
  CRYPT_CERTINFO_EXTKEY_CODESIGNING = 2319;  { codeSigning }
  CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION = 2320;  { emailProtection }
  CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM = 2321;  { ipsecEndSystem }
  CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL = 2322;  { ipsecTunnel }
  CRYPT_CERTINFO_EXTKEY_IPSECUSER = 2323;  { ipsecUser }
  CRYPT_CERTINFO_EXTKEY_TIMESTAMPING = 2324;  { timeStamping }
  CRYPT_CERTINFO_EXTKEY_OCSPSIGNING = 2325;  { ocspSigning }
  CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE = 2326;  { directoryService }
  CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE = 2327;  { anyExtendedKeyUsage }
  CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO = 2328;  { serverGatedCrypto }
  CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA = 2329;  { serverGatedCrypto CA }
  
  { 2 5 29 40 crlStreamIdentifier }
  CRYPT_CERTINFO_CRLSTREAMIDENTIFIER = 2330;  
  
  { 2 5 29 46 freshestCRL }
  CRYPT_CERTINFO_FRESHESTCRL = 2331;  
  CRYPT_CERTINFO_FRESHESTCRL_FULLNAME = 2332;  { distributionPointName.fullName }
  CRYPT_CERTINFO_FRESHESTCRL_REASONS = 2333;  { reasons }
  CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER = 2334;  { cRLIssuer }
  
  { 2 5 29 47 orderedList }
  CRYPT_CERTINFO_ORDEREDLIST = 2335;  
  
  { 2 5 29 51 baseUpdateTime }
  CRYPT_CERTINFO_BASEUPDATETIME = 2336;  
  
  { 2 5 29 53 deltaInfo }
  CRYPT_CERTINFO_DELTAINFO = 2337;  
  CRYPT_CERTINFO_DELTAINFO_LOCATION = 2338;  { deltaLocation }
  CRYPT_CERTINFO_DELTAINFO_NEXTDELTA = 2339;  { nextDelta }
  
  { 2 5 29 54 inhibitAnyPolicy }
  CRYPT_CERTINFO_INHIBITANYPOLICY = 2340;  
  
  { 2 5 29 58 toBeRevoked }
  CRYPT_CERTINFO_TOBEREVOKED = 2341;  
  CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER = 2342;  { certificateIssuer }
  CRYPT_CERTINFO_TOBEREVOKED_REASONCODE = 2343;  { reasonCode }
  CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME = 2344;  { revocationTime }
  CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER = 2345;  { certSerialNumber }
  
  { 2 5 29 59 revokedGroups }
  CRYPT_CERTINFO_REVOKEDGROUPS = 2346;  
  CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER = 2347;  { certificateIssuer }
  CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE = 2348;  { reasonCode }
  CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE = 2349;  { invalidityDate }
  CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER = 2350;  { startingNumber }
  CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER = 2351;  { endingNumber }
  
  { 2 5 29 60 expiredCertsOnCRL }
  CRYPT_CERTINFO_EXPIREDCERTSONCRL = 2352;  
  
  { 2 5 29 63 aaIssuingDistributionPoint }
  CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT = 2353;  
  CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME = 2354;  { distributionPointName.fullName }
  CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY = 2355;  { onlySomeReasons }
  CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL = 2356;  { indirectCRL }
  CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS = 2357;  { containsUserAttributeCerts }
  CRYPT_CERTINFO_AAISSUINGDIST_AACERTS = 2358;  { containsAACerts }
  CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS = 2359;  { containsSOAPublicKeyCerts }
  
  { 2 16 840 1 113730 1 x Netscape extensions }
  CRYPT_CERTINFO_NS_CERTTYPE = 2360;  { netscape-cert-type }
  CRYPT_CERTINFO_NS_BASEURL = 2361;  { netscape-base-url }
  CRYPT_CERTINFO_NS_REVOCATIONURL = 2362;  { netscape-revocation-url }
  CRYPT_CERTINFO_NS_CAREVOCATIONURL = 2363;  { netscape-ca-revocation-url }
  CRYPT_CERTINFO_NS_CERTRENEWALURL = 2364;  { netscape-cert-renewal-url }
  CRYPT_CERTINFO_NS_CAPOLICYURL = 2365;  { netscape-ca-policy-url }
  CRYPT_CERTINFO_NS_SSLSERVERNAME = 2366;  { netscape-ssl-server-name }
  CRYPT_CERTINFO_NS_COMMENT = 2367;  { netscape-comment }
  
  { 2 23 42 7 0 SET hashedRootKey }
  CRYPT_CERTINFO_SET_HASHEDROOTKEY = 2368;  
  CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT = 2369;  { rootKeyThumbPrint }
  
  { 2 23 42 7 1 SET certificateType }
  CRYPT_CERTINFO_SET_CERTIFICATETYPE = 2370;  
  
  { 2 23 42 7 2 SET merchantData }
  CRYPT_CERTINFO_SET_MERCHANTDATA = 2371;  
  CRYPT_CERTINFO_SET_MERID = 2372;  { merID }
  CRYPT_CERTINFO_SET_MERACQUIRERBIN = 2373;  { merAcquirerBIN }
  CRYPT_CERTINFO_SET_MERCHANTLANGUAGE = 2374;  { merNames.language }
  CRYPT_CERTINFO_SET_MERCHANTNAME = 2375;  { merNames.name }
  CRYPT_CERTINFO_SET_MERCHANTCITY = 2376;  { merNames.city }
  CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE = 2377;  { merNames.stateProvince }
  CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE = 2378;  { merNames.postalCode }
  CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME = 2379;  { merNames.countryName }
  CRYPT_CERTINFO_SET_MERCOUNTRY = 2380;  { merCountry }
  CRYPT_CERTINFO_SET_MERAUTHFLAG = 2381;  { merAuthFlag }
  
  { 2 23 42 7 3 SET certCardRequired }
  CRYPT_CERTINFO_SET_CERTCARDREQUIRED = 2382;  
  
  { 2 23 42 7 4 SET tunneling }
  CRYPT_CERTINFO_SET_TUNNELING = 2383;  
  CRYPT_CERTINFO_SET_TUNNELLING = 2383; { = CRYPT_CERTINFO_SET_TUNNELING }  
  CRYPT_CERTINFO_SET_TUNNELINGFLAG = 2384;  { tunneling }
  CRYPT_CERTINFO_SET_TUNNELLINGFLAG = 2384; { = CRYPT_CERTINFO_SET_TUNNELINGFLAG }  
  CRYPT_CERTINFO_SET_TUNNELINGALGID = 2385;  { tunnelingAlgID }
  CRYPT_CERTINFO_SET_TUNNELLINGALGID = 2385; { = CRYPT_CERTINFO_SET_TUNNELINGALGID }  
  
  { S/MIME attributes }
  
  { 1 2 840 113549 1 9 3 contentType }
  CRYPT_CERTINFO_CMS_CONTENTTYPE = 2500;  
  
  { 1 2 840 113549 1 9 4 messageDigest }
  CRYPT_CERTINFO_CMS_MESSAGEDIGEST = 2501;  
  
  { 1 2 840 113549 1 9 5 signingTime }
  CRYPT_CERTINFO_CMS_SIGNINGTIME = 2502;  
  
  { 1 2 840 113549 1 9 6 counterSignature }
  CRYPT_CERTINFO_CMS_COUNTERSIGNATURE = 2503;  { counterSignature }
  
  { 1 2 840 113549 1 9 13 signingDescription }
  CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION = 2504;  
  
  { 1 2 840 113549 1 9 15 sMIMECapabilities }
  CRYPT_CERTINFO_CMS_SMIMECAPABILITIES = 2505;  
  CRYPT_CERTINFO_CMS_SMIMECAP_3DES = 2506;  { 3DES encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_AES = 2507;  { AES encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_CAST128 = 2508;  { CAST-128 encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_IDEA = 2509;  { IDEA encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_RC2 = 2510;  { RC2 encryption (w.128 key) }
  CRYPT_CERTINFO_CMS_SMIMECAP_RC5 = 2511;  { RC5 encryption (w.128 key) }
  CRYPT_CERTINFO_CMS_SMIMECAP_SKIPJACK = 2512;  { Skipjack encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_DES = 2513;  { DES encryption }
  CRYPT_CERTINFO_CMS_SMIMECAP_SHAng = 2514;  { SHA2-ng hash }
  CRYPT_CERTINFO_CMS_SMIMECAP_SHA2 = 2515;  { SHA2-256 hash }
  CRYPT_CERTINFO_CMS_SMIMECAP_SHA1 = 2516;  { SHA1 hash }
  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng = 2517;  { HMAC-SHA2-ng MAC }
  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2 = 2518;  { HMAC-SHA2-256 MAC }
  CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1 = 2519;  { HMAC-SHA1 MAC }
  CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256 = 2520;  { AuthEnc w.256-bit key }
  CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128 = 2521;  { AuthEnc w.128-bit key }
  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng = 2522;  { RSA with SHA-ng signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2 = 2523;  { RSA with SHA2-256 signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1 = 2524;  { RSA with SHA1 signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1 = 2525;  { DSA with SHA-1 signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng = 2526;  { ECDSA with SHA-ng signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2 = 2527;  { ECDSA with SHA2-256 signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1 = 2528;  { ECDSA with SHA-1 signing }
  CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA = 2529;  { preferSignedData }
  CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY = 2530;  { canNotDecryptAny }
  CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE = 2531;  { preferBinaryInside }
  
  { 1 2 840 113549 1 9 16 2 1 receiptRequest }
  CRYPT_CERTINFO_CMS_RECEIPTREQUEST = 2532;  
  CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER = 2533;  { contentIdentifier }
  CRYPT_CERTINFO_CMS_RECEIPT_FROM = 2534;  { receiptsFrom }
  CRYPT_CERTINFO_CMS_RECEIPT_TO = 2535;  { receiptsTo }
  
  { 1 2 840 113549 1 9 16 2 2 essSecurityLabel }
  CRYPT_CERTINFO_CMS_SECURITYLABEL = 2536;  
  CRYPT_CERTINFO_CMS_SECLABEL_POLICY = 2537;  { securityPolicyIdentifier }
  CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION = 2538;  { securityClassification }
  CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK = 2539;  { privacyMark }
  CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE = 2540;  { securityCategories.securityCategory.type }
  CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE = 2541;  { securityCategories.securityCategory.value }
  
  { 1 2 840 113549 1 9 16 2 3 mlExpansionHistory }
  CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY = 2542;  
  CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER = 2543;  { mlData.mailListIdentifier.issuerAndSerialNumber }
  CRYPT_CERTINFO_CMS_MLEXP_TIME = 2544;  { mlData.expansionTime }
  CRYPT_CERTINFO_CMS_MLEXP_NONE = 2545;  { mlData.mlReceiptPolicy.none }
  CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF = 2546;  { mlData.mlReceiptPolicy.insteadOf.generalNames.generalName }
  CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO = 2547;  { mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName }
  
  { 1 2 840 113549 1 9 16 2 4 contentHints }
  CRYPT_CERTINFO_CMS_CONTENTHINTS = 2548;  
  CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION = 2549;  { contentDescription }
  CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE = 2550;  { contentType }
  
  { 1 2 840 113549 1 9 16 2 9 equivalentLabels }
  CRYPT_CERTINFO_CMS_EQUIVALENTLABEL = 2551;  
  CRYPT_CERTINFO_CMS_EQVLABEL_POLICY = 2552;  { securityPolicyIdentifier }
  CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION = 2553;  { securityClassification }
  CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK = 2554;  { privacyMark }
  CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE = 2555;  { securityCategories.securityCategory.type }
  CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE = 2556;  { securityCategories.securityCategory.value }
  
  { 1 2 840 113549 1 9 16 2 12 signingCertificate }
  CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE = 2557;  
  CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID = 2558;  { certs.essCertID }
  CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES = 2559;  { policies.policyInformation.policyIdentifier }
  
  { 1 2 840 113549 1 9 16 2 47 signingCertificateV2 }
  CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2 = 2560;  
  CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2 = 2561;  { certs.essCertID }
  CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES = 2562;  { policies.policyInformation.policyIdentifier }
  
  { 1 2 840 113549 1 9 16 2 15 signaturePolicyID }
  CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID = 2563;  
  CRYPT_CERTINFO_CMS_SIGPOLICYID = 2564;  { sigPolicyID }
  CRYPT_CERTINFO_CMS_SIGPOLICYHASH = 2565;  { sigPolicyHash }
  CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI = 2566;  { sigPolicyQualifiers.sigPolicyQualifier.cPSuri }
  CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION = 2567;  
  { sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization }
  CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS = 2568;  
  { sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers }
  CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT = 2569;  
  { sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText }
  
  { 1 2 840 113549 1 9 16 9 signatureTypeIdentifier }
  CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER = 2570;  
  CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG = 2571;  { originatorSig }
  CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG = 2572;  { domainSig }
  CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES = 2573;  { additionalAttributesSig }
  CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG = 2574;  { reviewSig }
  
  { 1 2 840 113549 1 9 25 3 randomNonce }
  CRYPT_CERTINFO_CMS_NONCE = 2575;  { randomNonce }
  
  { SCEP attributes:
  2 16 840 1 113733 1 9 2 messageType
  2 16 840 1 113733 1 9 3 pkiStatus
  2 16 840 1 113733 1 9 4 failInfo
  2 16 840 1 113733 1 9 5 senderNonce
  2 16 840 1 113733 1 9 6 recipientNonce
  2 16 840 1 113733 1 9 7 transID }
  CRYPT_CERTINFO_SCEP_MESSAGETYPE = 2576;  { messageType }
  CRYPT_CERTINFO_SCEP_PKISTATUS = 2577;  { pkiStatus }
  CRYPT_CERTINFO_SCEP_FAILINFO = 2578;  { failInfo }
  CRYPT_CERTINFO_SCEP_SENDERNONCE = 2579;  { senderNonce }
  CRYPT_CERTINFO_SCEP_RECIPIENTNONCE = 2580;  { recipientNonce }
  CRYPT_CERTINFO_SCEP_TRANSACTIONID = 2581;  { transID }
  
  { 1 3 6 1 4 1 311 2 1 10 spcAgencyInfo }
  CRYPT_CERTINFO_CMS_SPCAGENCYINFO = 2582;  
  CRYPT_CERTINFO_CMS_SPCAGENCYURL = 2583;  { spcAgencyInfo.url }
  
  { 1 3 6 1 4 1 311 2 1 11 spcStatementType }
  CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE = 2584;  
  CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING = 2585;  { individualCodeSigning }
  CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING = 2586;  { commercialCodeSigning }
  
  { 1 3 6 1 4 1 311 2 1 12 spcOpusInfo }
  CRYPT_CERTINFO_CMS_SPCOPUSINFO = 2587;  
  CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME = 2588;  { spcOpusInfo.name }
  CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL = 2589;  { spcOpusInfo.url }
  
  { Used internally }
  CRYPT_CERTINFO_LAST = 2590;  CRYPT_KEYINFO_FIRST = 3000;  
  
  {*******************}
  { Keyset attributes }
  {*******************}
  
  CRYPT_KEYINFO_QUERY = 3001;  { Keyset query }
  CRYPT_KEYINFO_QUERY_REQUESTS = 3002;  { Query of requests in cert store }
  
  { Used internally }
  CRYPT_KEYINFO_LAST = 3003;  CRYPT_DEVINFO_FIRST = 4000;  
  
  {*******************}
  { Device attributes }
  {*******************}
  
  CRYPT_DEVINFO_INITIALISE = 4001;  { Initialise device for use }
  CRYPT_DEVINFO_INITIALIZE = 4001; { = CRYPT_DEVINFO_INITIALISE }  
  CRYPT_DEVINFO_AUTHENT_USER = 4002;  { Authenticate user to device }
  CRYPT_DEVINFO_AUTHENT_SUPERVISOR = 4003;  { Authenticate supervisor to dev.}
  CRYPT_DEVINFO_SET_AUTHENT_USER = 4004;  { Set user authent.value }
  CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR = 4005;  { Set supervisor auth.val.}
  CRYPT_DEVINFO_ZEROISE = 4006;  { Zeroise device }
  CRYPT_DEVINFO_ZEROIZE = 4006; { = CRYPT_DEVINFO_ZEROISE }  
  CRYPT_DEVINFO_LOGGEDIN = 4007;  { Whether user is logged in }
  CRYPT_DEVINFO_LABEL = 4008;  { Device/token label }
  
  { Used internally }
  CRYPT_DEVINFO_LAST = 4009;  CRYPT_ENVINFO_FIRST = 5000;  
  
  {*********************}
  { Envelope attributes }
  {*********************}
  
  { Pseudo-information on an envelope or meta-information which is used to
  control the way that data in an envelope is processed }
  CRYPT_ENVINFO_DATASIZE = 5001;  { Data size information }
  CRYPT_ENVINFO_COMPRESSION = 5002;  { Compression information }
  CRYPT_ENVINFO_CONTENTTYPE = 5003;  { Inner CMS content type }
  CRYPT_ENVINFO_DETACHEDSIGNATURE = 5004;  { Detached signature }
  CRYPT_ENVINFO_SIGNATURE_RESULT = 5005;  { Signature check result }
  CRYPT_ENVINFO_INTEGRITY = 5006;  { Integrity-protection level }
  
  { Resources required for enveloping/deenveloping }
  CRYPT_ENVINFO_PASSWORD = 5007;  { User password }
  CRYPT_ENVINFO_KEY = 5008;  { Conventional encryption key }
  CRYPT_ENVINFO_SIGNATURE = 5009;  { Signature/signature check key }
  CRYPT_ENVINFO_SIGNATURE_EXTRADATA = 5010;  { Extra information added to CMS sigs }
  CRYPT_ENVINFO_RECIPIENT = 5011;  { Recipient email address }
  CRYPT_ENVINFO_PUBLICKEY = 5012;  { PKC encryption key }
  CRYPT_ENVINFO_PRIVATEKEY = 5013;  { PKC decryption key }
  CRYPT_ENVINFO_PRIVATEKEY_LABEL = 5014;  { Label of PKC decryption key }
  CRYPT_ENVINFO_ORIGINATOR = 5015;  { Originator info/key }
  CRYPT_ENVINFO_SESSIONKEY = 5016;  { Session key }
  CRYPT_ENVINFO_HASH = 5017;  { Hash value }
  CRYPT_ENVINFO_TIMESTAMP = 5018;  { Timestamp information }
  
  { Keysets used to retrieve keys needed for enveloping/deenveloping }
  CRYPT_ENVINFO_KEYSET_SIGCHECK = 5019;  { Signature check keyset }
  CRYPT_ENVINFO_KEYSET_ENCRYPT = 5020;  { PKC encryption keyset }
  CRYPT_ENVINFO_KEYSET_DECRYPT = 5021;  { PKC decryption keyset }
  
  { Used internally }
  CRYPT_ENVINFO_LAST = 5022;  CRYPT_SESSINFO_FIRST = 6000;  
  
  {********************}
  { Session attributes }
  {********************}
  
  { Pseudo-information about the session }
  CRYPT_SESSINFO_ACTIVE = 6001;  { Whether session is active }
  CRYPT_SESSINFO_CONNECTIONACTIVE = 6002;  { Whether network connection is active }
  
  { Security-related information }
  CRYPT_SESSINFO_USERNAME = 6003;  { User name }
  CRYPT_SESSINFO_PASSWORD = 6004;  { Password }
  CRYPT_SESSINFO_PRIVATEKEY = 6005;  { Server/client private key }
  CRYPT_SESSINFO_KEYSET = 6006;  { Certificate store }
  CRYPT_SESSINFO_AUTHRESPONSE = 6007;  { Session authorisation OK }
  
  { Client/server information }
  CRYPT_SESSINFO_SERVER_NAME = 6008;  { Server name }
  CRYPT_SESSINFO_SERVER_PORT = 6009;  { Server port number }
  CRYPT_SESSINFO_SERVER_FINGERPRINT = 6010;  { Server key fingerprint }
  CRYPT_SESSINFO_CLIENT_NAME = 6011;  { Client name }
  CRYPT_SESSINFO_CLIENT_PORT = 6012;  { Client port number }
  CRYPT_SESSINFO_SESSION = 6013;  { Transport mechanism }
  CRYPT_SESSINFO_NETWORKSOCKET = 6014;  { User-supplied network socket }
  
  { Generic protocol-related information }
  CRYPT_SESSINFO_VERSION = 6015;  { Protocol version }
  CRYPT_SESSINFO_REQUEST = 6016;  { Cert.request object }
  CRYPT_SESSINFO_RESPONSE = 6017;  { Cert.response object }
  CRYPT_SESSINFO_CACERTIFICATE = 6018;  { Issuing CA certificate }
  
  { Protocol-specific information }
  CRYPT_SESSINFO_CMP_REQUESTTYPE = 6019;  { Request type }
  CRYPT_SESSINFO_CMP_PRIVKEYSET = 6020;  { Private-key keyset }
  CRYPT_SESSINFO_SSH_CHANNEL = 6021;  { SSH current channel }
  CRYPT_SESSINFO_SSH_CHANNEL_TYPE = 6022;  { SSH channel type }
  CRYPT_SESSINFO_SSH_CHANNEL_ARG1 = 6023;  { SSH channel argument 1 }
  CRYPT_SESSINFO_SSH_CHANNEL_ARG2 = 6024;  { SSH channel argument 2 }
  CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE = 6025;  { SSH channel active }
  CRYPT_SESSINFO_SSL_OPTIONS = 6026;  { SSL/TLS protocol options }
  CRYPT_SESSINFO_TSP_MSGIMPRINT = 6027;  { TSP message imprint }
  
  { Used internally }
  CRYPT_SESSINFO_LAST = 6028;  CRYPT_USERINFO_FIRST = 7000;  
  
  {********************}
  { User attributes }
  {********************}
  
  { Security-related information }
  CRYPT_USERINFO_PASSWORD = 7001;  { Password }
  
  { User role-related information }
  CRYPT_USERINFO_CAKEY_CERTSIGN = 7002;  { CA cert signing key }
  CRYPT_USERINFO_CAKEY_CRLSIGN = 7003;  { CA CRL signing key }
  CRYPT_USERINFO_CAKEY_RTCSSIGN = 7004;  { CA RTCS signing key }
  CRYPT_USERINFO_CAKEY_OCSPSIGN = 7005;  { CA OCSP signing key }
  
  { Used internally for range checking }
  CRYPT_USERINFO_LAST = 7006;  CRYPT_ATTRIBUTE_LAST = 7006; { = CRYPT_USERINFO_LAST }  
  
  



{****************************************************************************
*                                                                           *
*                       Attribute Subtypes and Related Values               *
*                                                                           *
****************************************************************************}

{  Flags for the X.509 keyUsage extension  }

  CRYPT_KEYUSAGE_NONE = $000;
  CRYPT_KEYUSAGE_DIGITALSIGNATURE = $001;
  CRYPT_KEYUSAGE_NONREPUDIATION = $002;
  CRYPT_KEYUSAGE_KEYENCIPHERMENT = $004;
  CRYPT_KEYUSAGE_DATAENCIPHERMENT = $008;
  CRYPT_KEYUSAGE_KEYAGREEMENT = $010;
  CRYPT_KEYUSAGE_KEYCERTSIGN = $020;
  CRYPT_KEYUSAGE_CRLSIGN = $040;
  CRYPT_KEYUSAGE_ENCIPHERONLY = $080;
  CRYPT_KEYUSAGE_DECIPHERONLY = $100;
  CRYPT_KEYUSAGE_LAST = $200;   {  Last possible value  }

{  X.509 cRLReason and cryptlib cRLExtReason codes  }


  CRYPT_CRLREASON_UNSPECIFIED = 0;
  CRYPT_CRLREASON_KEYCOMPROMISE = 1;
  CRYPT_CRLREASON_CACOMPROMISE = 2;
  CRYPT_CRLREASON_AFFILIATIONCHANGED = 3;
  CRYPT_CRLREASON_SUPERSEDED = 4;
  CRYPT_CRLREASON_CESSATIONOFOPERATION = 5;
  CRYPT_CRLREASON_CERTIFICATEHOLD = 6;
  CRYPT_CRLREASON_REMOVEFROMCRL = 8;
  CRYPT_CRLREASON_PRIVILEGEWITHDRAWN = 9;
  CRYPT_CRLREASON_AACOMPROMISE = 10;
  CRYPT_CRLREASON_LAST = 11;
  { End of standard CRL reasons }
       CRYPT_CRLREASON_NEVERVALID = 20;
  CRYPT_CRLEXTREASON_LAST  = 21;



{  X.509 CRL reason flags.  These identify the same thing as the cRLReason
   codes but allow for multiple reasons to be specified.  Note that these
   don't follow the X.509 naming since in that scheme the enumerated types
   and bitflags have the same names  }

  CRYPT_CRLREASONFLAG_UNUSED = $001;
  CRYPT_CRLREASONFLAG_KEYCOMPROMISE = $002;
  CRYPT_CRLREASONFLAG_CACOMPROMISE = $004;
  CRYPT_CRLREASONFLAG_AFFILIATIONCHANGED = $008;
  CRYPT_CRLREASONFLAG_SUPERSEDED = $010;
  CRYPT_CRLREASONFLAG_CESSATIONOFOPERATION = $020;
  CRYPT_CRLREASONFLAG_CERTIFICATEHOLD = $040;
  CRYPT_CRLREASONFLAG_LAST = $080;   {  Last poss.value  }

{  X.509 CRL holdInstruction codes  }


  CRYPT_HOLDINSTRUCTION_NONE = 0;
  CRYPT_HOLDINSTRUCTION_CALLISSUER = 1;
  CRYPT_HOLDINSTRUCTION_REJECT = 2;
  CRYPT_HOLDINSTRUCTION_PICKUPTOKEN = 3;
  CRYPT_HOLDINSTRUCTION_LAST  = 4;



{  Certificate checking compliance levels  }


  CRYPT_COMPLIANCELEVEL_OBLIVIOUS = 0;
  CRYPT_COMPLIANCELEVEL_REDUCED = 1;
  CRYPT_COMPLIANCELEVEL_STANDARD = 2;
  CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL = 3;
  CRYPT_COMPLIANCELEVEL_PKIX_FULL = 4;
  CRYPT_COMPLIANCELEVEL_LAST  = 5;



{  Flags for the Netscape netscape-cert-type extension  }

  CRYPT_NS_CERTTYPE_SSLCLIENT = $001;
  CRYPT_NS_CERTTYPE_SSLSERVER = $002;
  CRYPT_NS_CERTTYPE_SMIME = $004;
  CRYPT_NS_CERTTYPE_OBJECTSIGNING = $008;
  CRYPT_NS_CERTTYPE_RESERVED = $010;
  CRYPT_NS_CERTTYPE_SSLCA = $020;
  CRYPT_NS_CERTTYPE_SMIMECA = $040;
  CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA = $080;
  CRYPT_NS_CERTTYPE_LAST = $100;   {  Last possible value  }

{  Flags for the SET certificate-type extension  }

  CRYPT_SET_CERTTYPE_CARD = $001;
  CRYPT_SET_CERTTYPE_MER = $002;
  CRYPT_SET_CERTTYPE_PGWY = $004;
  CRYPT_SET_CERTTYPE_CCA = $008;
  CRYPT_SET_CERTTYPE_MCA = $010;
  CRYPT_SET_CERTTYPE_PCA = $020;
  CRYPT_SET_CERTTYPE_GCA = $040;
  CRYPT_SET_CERTTYPE_BCA = $080;
  CRYPT_SET_CERTTYPE_RCA = $100;
  CRYPT_SET_CERTTYPE_ACQ = $200;
  CRYPT_SET_CERTTYPE_LAST = $400;   {  Last possible value  }

{  CMS contentType values  }


type
  CRYPT_CONTENT_TYPE = (   CRYPT_CONTENT_NONE, CRYPT_CONTENT_DATA,
               CRYPT_CONTENT_SIGNEDDATA, CRYPT_CONTENT_ENVELOPEDDATA,
               CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA,
               CRYPT_CONTENT_DIGESTEDDATA, CRYPT_CONTENT_ENCRYPTEDDATA,
               CRYPT_CONTENT_COMPRESSEDDATA, CRYPT_CONTENT_AUTHDATA, 
               CRYPT_CONTENT_AUTHENVDATA, CRYPT_CONTENT_TSTINFO,
               CRYPT_CONTENT_SPCINDIRECTDATACONTEXT,
               CRYPT_CONTENT_RTCSREQUEST, CRYPT_CONTENT_RTCSRESPONSE,
               CRYPT_CONTENT_RTCSRESPONSE_EXT, CRYPT_CONTENT_MRTD, 
               CRYPT_CONTENT_LAST
             
  );

{  ESS securityClassification codes  }



const
  CRYPT_CLASSIFICATION_UNMARKED = 0;
  CRYPT_CLASSIFICATION_UNCLASSIFIED = 1;
  CRYPT_CLASSIFICATION_RESTRICTED = 2;
  CRYPT_CLASSIFICATION_CONFIDENTIAL = 3;
  CRYPT_CLASSIFICATION_SECRET = 4;
  CRYPT_CLASSIFICATION_TOP_SECRET = 5;
  CRYPT_CLASSIFICATION_LAST = 255 ;



{  RTCS certificate status  }


  CRYPT_CERTSTATUS_VALID = 0;
  CRYPT_CERTSTATUS_NOTVALID = 1;
  CRYPT_CERTSTATUS_NONAUTHORITATIVE = 2;
  CRYPT_CERTSTATUS_UNKNOWN  = 3;



{  OCSP revocation status  }


  CRYPT_OCSPSTATUS_NOTREVOKED = 0;
  CRYPT_OCSPSTATUS_REVOKED = 1;
  CRYPT_OCSPSTATUS_UNKNOWN  = 2;



{  The amount of detail to include in signatures when signing certificate
   objects  }


type
  CRYPT_SIGNATURELEVEL_TYPE = (  
    CRYPT_SIGNATURELEVEL_NONE,      {  Include only signature  }
    CRYPT_SIGNATURELEVEL_SIGNERCERT,{  Include signer cert  }
    CRYPT_SIGNATURELEVEL_ALL,       {  Include all relevant info  }
    CRYPT_SIGNATURELEVEL_LAST       {  Last possible sig.level type  }
    
  );

{  The level of integrity protection to apply to enveloped data.  The 
   default envelope protection for an envelope with keying information 
   applied is encryption, this can be modified to use MAC-only protection
   (with no encryption) or hybrid encryption + authentication  }

  CRYPT_INTEGRITY_TYPE = (  
    CRYPT_INTEGRITY_NONE,           {  No integrity protection  }
    CRYPT_INTEGRITY_MACONLY,        {  MAC only, no encryption  }
    CRYPT_INTEGRITY_FULL            {  Encryption + ingerity protection  }
    
  );

{  The certificate export format type, which defines the format in which a
   certificate object is exported  }

  CRYPT_CERTFORMAT_TYPE = (  
    CRYPT_CERTFORMAT_NONE,          {  No certificate format  }
    CRYPT_CERTFORMAT_CERTIFICATE,   {  DER-encoded certificate  }
    CRYPT_CERTFORMAT_CERTCHAIN,     {  PKCS #7 certificate chain  }
    CRYPT_CERTFORMAT_TEXT_CERTIFICATE,  {  base-64 wrapped cert  }
    CRYPT_CERTFORMAT_TEXT_CERTCHAIN,    {  base-64 wrapped cert chain  }
    CRYPT_CERTFORMAT_XML_CERTIFICATE,   {  XML wrapped cert  }
    CRYPT_CERTFORMAT_XML_CERTCHAIN, {  XML wrapped cert chain  }
    CRYPT_CERTFORMAT_LAST           {  Last possible cert.format type  }
    
  );

{  CMP request types  }

  CRYPT_REQUESTTYPE_TYPE = (  
    CRYPT_REQUESTTYPE_NONE,         {  No request type  }
    CRYPT_REQUESTTYPE_INITIALISATION,   {  Initialisation request  }
    CRYPT_REQUESTTYPE_CERTIFICATE,  {  Certification request  }
    CRYPT_REQUESTTYPE_KEYUPDATE,    {  Key update request  }
    CRYPT_REQUESTTYPE_REVOCATION,   {  Cert revocation request  }
    CRYPT_REQUESTTYPE_PKIBOOT,      {  PKIBoot request  }
    CRYPT_REQUESTTYPE_LAST          {  Last possible request type  }
    
  );

const
  CRYPT_REQUESTTYPE_INITIALIZATION: CRYPT_REQUESTTYPE_TYPE = CRYPT_REQUESTTYPE_INITIALISATION;

{  Key ID types  }


type
  CRYPT_KEYID_TYPE = (  
    CRYPT_KEYID_NONE,               {  No key ID type  }
    CRYPT_KEYID_NAME,               {  Key owner name  }
    CRYPT_KEYID_URI,                {  Key owner URI  } {  Synonym: owner email addr.}
    CRYPT_KEYID_LAST                {  Last possible key ID type  }
    
  );

const
  CRYPT_KEYID_EMAIL: CRYPT_KEYID_TYPE = CRYPT_KEYID_URI;

{  The encryption object types  }


type
  CRYPT_OBJECT_TYPE = (  
    CRYPT_OBJECT_NONE,              {  No object type  }
    CRYPT_OBJECT_ENCRYPTED_KEY,     {  Conventionally encrypted key  }
    CRYPT_OBJECT_PKCENCRYPTED_KEY,  {  PKC-encrypted key  }
    CRYPT_OBJECT_KEYAGREEMENT,      {  Key agreement information  }
    CRYPT_OBJECT_SIGNATURE,         {  Signature  }
    CRYPT_OBJECT_LAST               {  Last possible object type  }
    
  );

{  Object/attribute error type information  }

  CRYPT_ERRTYPE_TYPE = (  
    CRYPT_ERRTYPE_NONE,             {  No error information  }
    CRYPT_ERRTYPE_ATTR_SIZE,        {  Attribute data too small or large  }
    CRYPT_ERRTYPE_ATTR_VALUE,       {  Attribute value is invalid  }
    CRYPT_ERRTYPE_ATTR_ABSENT,      {  Required attribute missing  }
    CRYPT_ERRTYPE_ATTR_PRESENT,     {  Non-allowed attribute present  }
    CRYPT_ERRTYPE_CONSTRAINT,       {  Cert: Constraint violation in object  }
    CRYPT_ERRTYPE_ISSUERCONSTRAINT, {  Cert: Constraint viol.in issuing cert  }
    CRYPT_ERRTYPE_LAST              {  Last possible error info type  }
    
  );

{  Cert store management action type  }

  CRYPT_CERTACTION_TYPE = (  
    CRYPT_CERTACTION_NONE,          {  No cert management action  }
    CRYPT_CERTACTION_CREATE,        {  Create cert store  }
    CRYPT_CERTACTION_CONNECT,       {  Connect to cert store  }
    CRYPT_CERTACTION_DISCONNECT,    {  Disconnect from cert store  }
    CRYPT_CERTACTION_ERROR,         {  Error information  }
    CRYPT_CERTACTION_ADDUSER,       {  Add PKI user  }
    CRYPT_CERTACTION_DELETEUSER,    {  Delete PKI user  }
    CRYPT_CERTACTION_REQUEST_CERT,  {  Cert request  }
    CRYPT_CERTACTION_REQUEST_RENEWAL,{  Cert renewal request  }
    CRYPT_CERTACTION_REQUEST_REVOCATION,{  Cert revocation request  }
    CRYPT_CERTACTION_CERT_CREATION, {  Cert creation  }
    CRYPT_CERTACTION_CERT_CREATION_COMPLETE,{  Confirmation of cert creation  }
    CRYPT_CERTACTION_CERT_CREATION_DROP,    {  Cancellation of cert creation  }
    CRYPT_CERTACTION_CERT_CREATION_REVERSE, {  Cancel of creation w.revocation  }
    CRYPT_CERTACTION_RESTART_CLEANUP, {  Delete reqs after restart  }
    CRYPT_CERTACTION_RESTART_REVOKE_CERT, {  Complete revocation after restart  }
    CRYPT_CERTACTION_ISSUE_CERT,    {  Cert issue  }
    CRYPT_CERTACTION_ISSUE_CRL,     {  CRL issue  }
    CRYPT_CERTACTION_REVOKE_CERT,   {  Cert revocation  }
    CRYPT_CERTACTION_EXPIRE_CERT,   {  Cert expiry  }
    CRYPT_CERTACTION_CLEANUP,       {  Clean up on restart  }
    CRYPT_CERTACTION_LAST           {  Last possible cert store log action  }
    
  );

{  SSL/TLS protocol options.  CRYPT_SSLOPTION_MINVER_SSLV3 is the same as 
   CRYPT_SSLOPTION_NONE since this is the default  }


const
  CRYPT_SSLOPTION_NONE = $00;
  CRYPT_SSLOPTION_MINVER_SSLV3 = $00;    {  Min.protocol version  }
  CRYPT_SSLOPTION_MINVER_TLS10 = $01;
  CRYPT_SSLOPTION_MINVER_TLS11 = $02;
  CRYPT_SSLOPTION_MINVER_TLS12 = $03;
  CRYPT_SSLOPTION_SUITEB_128 = $04;    {  SuiteB security levels  }
  CRYPT_SSLOPTION_SUITEB_256 = $08;

{****************************************************************************
*                                                                           *
*                               General Constants                           *
*                                                                           *
****************************************************************************}

{  The maximum user key size - 2048 bits  }

  CRYPT_MAX_KEYSIZE = 256;

{  The maximum IV size - 256 bits  }

  CRYPT_MAX_IVSIZE = 32;

{  The maximum public-key component size - 4096 bits, and maximum component
   size for ECCs - 576 bits (to handle the P521 curve)  }

  CRYPT_MAX_PKCSIZE = 512;
  CRYPT_MAX_PKCSIZE_ECC = 72;

{  The maximum hash size - 512 bits.  Before 3.4 this was 256 bits, in the 
   3.4 release it was increased to 512 bits to accommodate SHA-3  }

  CRYPT_MAX_HASHSIZE = 64;

{  The maximum size of a text string (e.g.key owner name)  }

  CRYPT_MAX_TEXTSIZE = 64;

{  A magic value indicating that the default setting for this parameter
   should be used.  The parentheses are to catch potential erroneous use 
   in an expression  }

  CRYPT_USE_DEFAULT = -100;

{  A magic value for unused parameters  }

  CRYPT_UNUSED = -101;

{  Cursor positioning codes for certificate/CRL extensions.  The parentheses 
   are to catch potential erroneous use in an expression  }

  CRYPT_CURSOR_FIRST = -200;
  CRYPT_CURSOR_PREVIOUS = -201;
  CRYPT_CURSOR_NEXT = -202;
  CRYPT_CURSOR_LAST = -203;

{  The type of information polling to perform to get random seed 
   information.  These values have to be negative because they're used
   as magic length values for cryptAddRandom().  The parentheses are to 
   catch potential erroneous use in an expression  }

  CRYPT_RANDOM_FASTPOLL = -300;
  CRYPT_RANDOM_SLOWPOLL = -301;

{  Whether the PKC key is a public or private key  }

  CRYPT_KEYTYPE_PRIVATE = 0;
  CRYPT_KEYTYPE_PUBLIC = 1;

{  Keyset open options  }


type
  CRYPT_KEYOPT_TYPE = (  
    CRYPT_KEYOPT_NONE,              {  No options  }
    CRYPT_KEYOPT_READONLY,          {  Open keyset in read-only mode  }
    CRYPT_KEYOPT_CREATE,            {  Create a new keyset  }
    CRYPT_KEYOPT_LAST               {  Last possible key option type  }
    
  );

{  The various cryptlib objects - these are just integer handles  }

  CRYPT_CERTIFICATE = Integer;
  CRYPT_CONTEXT = Integer;
  CRYPT_DEVICE = Integer;
  CRYPT_ENVELOPE = Integer;
  CRYPT_KEYSET = Integer;
  CRYPT_SESSION = Integer;
  CRYPT_USER = Integer;

{  Sometimes we don't know the exact type of a cryptlib object, so we use a
   generic handle type to identify it  }

  CRYPT_HANDLE = Integer;

{****************************************************************************
*                                                                           *
*                           Encryption Data Structures                      *
*                                                                           *
****************************************************************************}

{  Results returned from the capability query  }

  CRYPT_QUERY_INFO = record  
    { Algorithm information }
    algoName: array[0 .. CRYPT_MAX_TEXTSIZE-1] of char;{ Algorithm name }
    blockSize: Integer;                  { Block size of the algorithm }
    minKeySize: Integer;                 { Minimum key size in bytes }
    keySize: Integer;                    { Recommended key size in bytes }
    maxKeySize: Integer;                 { Maximum key size in bytes }
    

  end;

{  Results returned from the encoded object query.  These provide
   information on the objects created by cryptExportKey()/
   cryptCreateSignature()  }

  CRYPT_OBJECT_INFO = record  
    { The object type }
    objectType: CRYPT_OBJECT_TYPE;

    { The encryption algorithm and mode }
    cryptAlgo: CRYPT_ALGO_TYPE;
    cryptMode: CRYPT_MODE_TYPE;

    { The hash algorithm for Signature objects }
    hashAlgo: CRYPT_ALGO_TYPE;

    { The salt for derived keys }
    salt: array[0 .. CRYPT_MAX_HASHSIZE-1] of byte;
    saltSize: Integer;
    

  end;

{  Key information for the public-key encryption algorithms.  These fields
   are not accessed directly, but can be manipulated with the init/set/
   destroyComponents() macros  }

  CRYPT_PKCINFO_RSA = record  
    { Status information }
    isPublicKey: Integer;            { Whether this is a public or private key  }

    {  Public components }
    n: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Modulus }
    nLen: Integer;                   { Length of modulus in bits }
    e: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Public exponent }
    eLen: Integer;                   { Length of public exponent in bits  }

    {  Private components }
    d: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Private exponent }
    dLen: Integer;                   { Length of private exponent in bits }
    p: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Prime factor 1 }
    pLen: Integer;                   { Length of prime factor 1 in bits }
    q: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Prime factor 2 }
    qLen: Integer;                   { Length of prime factor 2 in bits }
    u: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Mult.inverse of q, mod p }
    uLen: Integer;                   { Length of private exponent in bits }
    e1: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;  { Private exponent 1 (PKCS) }
    e1Len: Integer;                  { Length of private exponent in bits }
    e2: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;  { Private exponent 2 (PKCS) }
    e2Len: Integer;                  { Length of private exponent in bits }
    

  end;

  CRYPT_PKCINFO_DLP = record  
    { Status information }
    isPublicKey: Integer;            { Whether this is a public or private key  }

    {  Public components }
    p: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Prime modulus }
    pLen: Integer;                   { Length of prime modulus in bits }
    q: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Prime divisor }
    qLen: Integer;                   { Length of prime divisor in bits }
    g: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { h^( ( p - 1 ) / q ) mod p }
    gLen: Integer;                   { Length of g in bits }
    y: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Public random integer }
    yLen: Integer;                   { Length of public integer in bits  }

    {  Private components }
    x: array[0 .. CRYPT_MAX_PKCSIZE-1] of byte;   { Private random integer }
    xLen: Integer;                   { Length of private integer in bits }
    

  end;

  CRYPT_ECCCURVE_TYPE = (  
    {  Named ECC curves.  Since these need to be mapped to all manner of
       protocol- and mechanism-specific identifiers, when updating this list 
       grep for occurrences of CRYPT_ECCCURVE_P256 (the most common one) and
       check whether any related mapping tables need to be updated  }
    CRYPT_ECCCURVE_NONE,        {  No ECC curve type  }
    CRYPT_ECCCURVE_P192,        {  NIST P192/X9.62 P192r1/SECG p192r1 curve  }
    CRYPT_ECCCURVE_P224,        {  NIST P224/X9.62 P224r1/SECG p224r1 curve  }
    CRYPT_ECCCURVE_P256,        {  NIST P256/X9.62 P256v1/SECG p256r1 curve  }
    CRYPT_ECCCURVE_P384,        {  NIST P384, SECG p384r1 curve  }
    CRYPT_ECCCURVE_P521,        {  NIST P521, SECG p521r1  }
    CRYPT_ECCCURVE_LAST         {  Last valid ECC curve type  }
    
  );

  CRYPT_PKCINFO_ECC = record  
    { Status information }
    isPublicKey: Integer;            { Whether this is a public or private key  }

    {  Curve domain parameters.  Either the curveType or the explicit domain
       parameters must be provided }
    curveType: CRYPT_ECCCURVE_TYPE;  { Named curve }
    p: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Prime defining Fq }
    pLen: Integer;                   { Length of prime in bits }
    a: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Element in Fq defining curve }
    aLen: Integer;                   { Length of element a in bits }
    b: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Element in Fq defining curve }
    bLen: Integer;                   { Length of element b in bits }
    gx: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Element in Fq defining point }
    gxLen: Integer;                  { Length of element gx in bits }
    gy: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Element in Fq defining point }
    gyLen: Integer;                  { Length of element gy in bits }
    n: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Order of point }
    nLen: Integer;                   { Length of order in bits }
    h: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Optional cofactor }
    hLen: Integer;                   { Length of cofactor in bits  }

    {  Public components }
    qx: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Point Q on the curve }
    qxLen: Integer;                  { Length of point xq in bits }
    qy: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Point Q on the curve }
    qyLen: Integer;                  { Length of point xy in bits  }

    {  Private components }
    d: array[0 .. CRYPT_MAX_PKCSIZE_ECC-1] of byte;{ Private random integer }
    dLen: Integer;                   { Length of integer in bits }
    

  end;

{  Macros to initialise and destroy the structure that stores the components
   of a public key  }

{ C-macro not translated to Delphi code: 
{   #define cryptInitComponents( componentInfo, componentKeyType ) 
    < memset( ( componentInfo ), 0, sizeof( *componentInfo ) ); 
      ( componentInfo )->isPublicKey = ( ( componentKeyType ) ? 1 : 0 ); > }

{ C-macro not translated to Delphi code: 
{   #define cryptDestroyComponents( componentInfo ) 
    memset( ( componentInfo ), 0, sizeof( *componentInfo ) ) }

{  Macros to set a component of a public key  }

{ C-macro not translated to Delphi code: 
{   #define cryptSetComponent( destination, source, length ) 
    < memcpy( ( destination ), ( source ), ( ( length ) + 7 ) >> 3 ); 
      ( destination##Len ) = length; > }

{****************************************************************************
*                                                                           *
*                               Status Codes                                *
*                                                                           *
****************************************************************************}

{  No error in function call  }


const
  CRYPT_OK = 0;       {  No error  }

{  Error in parameters passed to function.  The parentheses are to catch 
   potential erroneous use in an expression  }

  CRYPT_ERROR_PARAM1 = -1;  {  Bad argument, parameter 1  }
  CRYPT_ERROR_PARAM2 = -2;  {  Bad argument, parameter 2  }
  CRYPT_ERROR_PARAM3 = -3;  {  Bad argument, parameter 3  }
  CRYPT_ERROR_PARAM4 = -4;  {  Bad argument, parameter 4  }
  CRYPT_ERROR_PARAM5 = -5;  {  Bad argument, parameter 5  }
  CRYPT_ERROR_PARAM6 = -6;  {  Bad argument, parameter 6  }
  CRYPT_ERROR_PARAM7 = -7;  {  Bad argument, parameter 7  }

{  Errors due to insufficient resources  }

  CRYPT_ERROR_MEMORY = -10; {  Out of memory  }
  CRYPT_ERROR_NOTINITED = -11; {  Data has not been initialised  }
  CRYPT_ERROR_INITED = -12; {  Data has already been init'd  }
  CRYPT_ERROR_NOSECURE = -13; {  Opn.not avail.at requested sec.level  }
  CRYPT_ERROR_RANDOM = -14; {  No reliable random data available  }
  CRYPT_ERROR_FAILED = -15; {  Operation failed  }
  CRYPT_ERROR_INTERNAL = -16; {  Internal consistency check failed  }

{  Security violations  }

  CRYPT_ERROR_NOTAVAIL = -20; {  This type of opn.not available  }
  CRYPT_ERROR_PERMISSION = -21; {  No permiss.to perform this operation  }
  CRYPT_ERROR_WRONGKEY = -22; {  Incorrect key used to decrypt data  }
  CRYPT_ERROR_INCOMPLETE = -23; {  Operation incomplete/still in progress  }
  CRYPT_ERROR_COMPLETE = -24; {  Operation complete/can't continue  }
  CRYPT_ERROR_TIMEOUT = -25; {  Operation timed out before completion  }
  CRYPT_ERROR_INVALID = -26; {  Invalid/inconsistent information  }
  CRYPT_ERROR_SIGNALLED = -27; {  Resource destroyed by extnl.event  }

{  High-level function errors  }

  CRYPT_ERROR_OVERFLOW = -30; {  Resources/space exhausted  }
  CRYPT_ERROR_UNDERFLOW = -31; {  Not enough data available  }
  CRYPT_ERROR_BADDATA = -32; {  Bad/unrecognised data format  }
  CRYPT_ERROR_SIGNATURE = -33; {  Signature/integrity check failed  }

{  Data access function errors  }

  CRYPT_ERROR_OPEN = -40; {  Cannot open object  }
  CRYPT_ERROR_READ = -41; {  Cannot read item from object  }
  CRYPT_ERROR_WRITE = -42; {  Cannot write item to object  }
  CRYPT_ERROR_NOTFOUND = -43; {  Requested item not found in object  }
  CRYPT_ERROR_DUPLICATE = -44; {  Item already present in object  }

{  Data enveloping errors  }

  CRYPT_ENVELOPE_RESOURCE = -50; {  Need resource to proceed  }

{  Macros to examine return values  }

{ C-macro not translated to Delphi code: 
{   #define cryptStatusError( status )  ( ( status ) < CRYPT_OK ) }
{ C-macro not translated to Delphi code: 
{   #define cryptStatusOK( status )     ( ( status ) == CRYPT_OK ) }

{****************************************************************************
*                                                                           *
*                                   General Functions                       *
*                                                                           *
****************************************************************************}

{  The following is necessary to stop C++ name mangling  }


{  Initialise and shut down cryptlib  }

 
function cryptInit: Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


function cryptEnd: Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;



{  Query cryptlibs capabilities  }

function cryptQueryCapability( const cryptAlgo: CRYPT_ALGO_TYPE;
  var cryptQueryInfo: CRYPT_QUERY_INFO ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Create and destroy an encryption context  }

function cryptCreateContext( var cryptContext: CRYPT_CONTEXT;
  const cryptUser: CRYPT_USER;
  const cryptAlgo: CRYPT_ALGO_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDestroyContext( const cryptContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Generic "destroy an object" function  }

function cryptDestroyObject( const cryptObject: CRYPT_HANDLE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Generate a key into a context  }

function cryptGenerateKey( const cryptContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Encrypt/decrypt/hash a block of memory  }

function cryptEncrypt( const cryptContext: CRYPT_CONTEXT;
  buffer: Pointer;
  const length: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDecrypt( const cryptContext: CRYPT_CONTEXT;
  buffer: Pointer;
  const length: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Get/set/delete attribute functions  }

function cryptSetAttribute( const cryptHandle: CRYPT_HANDLE;
  const attributeType: CRYPT_ATTRIBUTE_TYPE;
  const value: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptSetAttributeString( const cryptHandle: CRYPT_HANDLE;
  const attributeType: CRYPT_ATTRIBUTE_TYPE;
  const value: Pointer;
  const valueLength: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptGetAttribute( const cryptHandle: CRYPT_HANDLE;
  const attributeType: CRYPT_ATTRIBUTE_TYPE;
  var value: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptGetAttributeString( const cryptHandle: CRYPT_HANDLE;
  const attributeType: CRYPT_ATTRIBUTE_TYPE;
  value: Pointer;
  var valueLength: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDeleteAttribute( const cryptHandle: CRYPT_HANDLE;
  const attributeType: CRYPT_ATTRIBUTE_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Oddball functions: Add random data to the pool, query an encoded signature
   or key data.  These are due to be replaced once a suitable alternative can
   be found  }

function cryptAddRandom( const randomData: Pointer;
  const randomDataLength: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptQueryObject( const objectData: Pointer;
  const objectDataLength: Integer;
  var cryptObjectInfo: CRYPT_OBJECT_INFO ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                           Mid-level Encryption Functions                  *
*                                                                           *
****************************************************************************}

{  Export and import an encrypted session key  }

function cryptExportKey( encryptedKey: Pointer;
  const encryptedKeyMaxLength: Integer;
  var encryptedKeyLength: Integer;
  const exportKey: CRYPT_HANDLE;
  const sessionKeyContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptExportKeyEx( encryptedKey: Pointer;
  const encryptedKeyMaxLength: Integer;
  var encryptedKeyLength: Integer;
  const formatType: CRYPT_FORMAT_TYPE;
  const exportKey: CRYPT_HANDLE;
  const sessionKeyContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptImportKey( const encryptedKey: Pointer;
  const encryptedKeyLength: Integer;
  const importKey: CRYPT_CONTEXT;
  const sessionKeyContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptImportKeyEx( const encryptedKey: Pointer;
  const encryptedKeyLength: Integer;
  const importKey: CRYPT_CONTEXT;
  const sessionKeyContext: CRYPT_CONTEXT;
  var returnedContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Create and check a digital signature  }

function cryptCreateSignature( signature: Pointer;
  const signatureMaxLength: Integer;
  var signatureLength: Integer;
  const signContext: CRYPT_CONTEXT;
  const hashContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCreateSignatureEx( signature: Pointer;
  const signatureMaxLength: Integer;
  var signatureLength: Integer;
  const formatType: CRYPT_FORMAT_TYPE;
  const signContext: CRYPT_CONTEXT;
  const hashContext: CRYPT_CONTEXT;
  const extraData: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCheckSignature( const signature: Pointer;
  const signatureLength: Integer;
  const sigCheckKey: CRYPT_HANDLE;
  const hashContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCheckSignatureEx( const signature: Pointer;
  const signatureLength: Integer;
  const sigCheckKey: CRYPT_HANDLE;
  const hashContext: CRYPT_CONTEXT;
  var extraData: CRYPT_HANDLE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                                   Keyset Functions                        *
*                                                                           *
****************************************************************************}

{  Open and close a keyset  }

function cryptKeysetOpen( var keyset: CRYPT_KEYSET;
  const cryptUser: CRYPT_USER;
  const keysetType: CRYPT_KEYSET_TYPE;
  const name: PAnsiChar;
  const options: CRYPT_KEYOPT_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptKeysetClose( const keyset: CRYPT_KEYSET ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Get a key from a keyset or device  }

function cryptGetPublicKey( const keyset: CRYPT_KEYSET;
  var cryptContext: CRYPT_CONTEXT;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptGetPrivateKey( const keyset: CRYPT_KEYSET;
  var cryptContext: CRYPT_CONTEXT;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar;
  const password: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptGetKey( const keyset: CRYPT_KEYSET;
  var cryptContext: CRYPT_CONTEXT;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar;
  const password: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Add/delete a key to/from a keyset or device  }

function cryptAddPublicKey( const keyset: CRYPT_KEYSET;
  const certificate: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptAddPrivateKey( const keyset: CRYPT_KEYSET;
  const cryptKey: CRYPT_HANDLE;
  const password: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDeleteKey( const keyset: CRYPT_KEYSET;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                               Certificate Functions                       *
*                                                                           *
****************************************************************************}

{  Create/destroy a certificate  }

function cryptCreateCert( var certificate: CRYPT_CERTIFICATE;
  const cryptUser: CRYPT_USER;
  const certType: CRYPT_CERTTYPE_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDestroyCert( const certificate: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Get/add/delete certificate extensions.  These are direct data insertion
   functions whose use is discouraged, so they fix the string at char *
   rather than C_STR  }

function cryptGetCertExtension( const certificate: CRYPT_CERTIFICATE;
  const oid: PAnsiChar;
  var criticalFlag: Integer;
  extension: Pointer;
  const extensionMaxLength: Integer;
  var extensionLength: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptAddCertExtension( const certificate: CRYPT_CERTIFICATE;
  const oid: PAnsiChar;
  const criticalFlag: Integer;
  const extension: Pointer;
  const extensionLength: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDeleteCertExtension( const certificate: CRYPT_CERTIFICATE;
  const oid: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Sign/sig.check a certificate/certification request  }

function cryptSignCert( const certificate: CRYPT_CERTIFICATE;
  const signContext: CRYPT_CONTEXT ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCheckCert( const certificate: CRYPT_CERTIFICATE;
  const sigCheckKey: CRYPT_HANDLE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Import/export a certificate/certification request  }

function cryptImportCert( const certObject: Pointer;
  const certObjectLength: Integer;
  const cryptUser: CRYPT_USER;
  var certificate: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptExportCert( certObject: Pointer;
  const certObjectMaxLength: Integer;
  var certObjectLength: Integer;
  const certFormatType: CRYPT_CERTFORMAT_TYPE;
  const certificate: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  CA management functions  }

function cryptCAAddItem( const keyset: CRYPT_KEYSET;
  const certificate: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCAGetItem( const keyset: CRYPT_KEYSET;
  var certificate: CRYPT_CERTIFICATE;
  const certType: CRYPT_CERTTYPE_TYPE;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCADeleteItem( const keyset: CRYPT_KEYSET;
  const certType: CRYPT_CERTTYPE_TYPE;
  const keyIDtype: CRYPT_KEYID_TYPE;
  const keyID: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptCACertManagement( var certificate: CRYPT_CERTIFICATE;
  const action: CRYPT_CERTACTION_TYPE;
  const keyset: CRYPT_KEYSET;
  const caKey: CRYPT_CONTEXT;
  const certRequest: CRYPT_CERTIFICATE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                           Envelope and Session Functions                  *
*                                                                           *
****************************************************************************}

{  Create/destroy an envelope  }

function cryptCreateEnvelope( var envelope: CRYPT_ENVELOPE;
  const cryptUser: CRYPT_USER;
  const formatType: CRYPT_FORMAT_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDestroyEnvelope( const envelope: CRYPT_ENVELOPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Create/destroy a session  }

function cryptCreateSession( var session: CRYPT_SESSION;
  const cryptUser: CRYPT_USER;
  const formatType: CRYPT_SESSION_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDestroySession( const session: CRYPT_SESSION ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Add/remove data to/from and envelope or session  }

function cryptPushData( const envelope: CRYPT_HANDLE;
  const buffer: Pointer;
  const length: Integer;
  var bytesCopied: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptFlushData( const envelope: CRYPT_HANDLE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptPopData( const envelope: CRYPT_HANDLE;
  buffer: Pointer;
  const length: Integer;
  var bytesCopied: Integer ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                               Device Functions                            *
*                                                                           *
****************************************************************************}

{  Open and close a device  }

function cryptDeviceOpen( var device: CRYPT_DEVICE;
  const cryptUser: CRYPT_USER;
  const deviceType: CRYPT_DEVICE_TYPE;
  const name: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptDeviceClose( const device: CRYPT_DEVICE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Query a devices capabilities  }

function cryptDeviceQueryCapability( const device: CRYPT_DEVICE;
  const cryptAlgo: CRYPT_ALGO_TYPE;
  var cryptQueryInfo: CRYPT_QUERY_INFO ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{  Create an encryption context via the device  }

function cryptDeviceCreateContext( const device: CRYPT_DEVICE;
  var cryptContext: CRYPT_CONTEXT;
  const cryptAlgo: CRYPT_ALGO_TYPE ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                           User Management Functions                       *
*                                                                           *
****************************************************************************}

{  Log on and off (create/destroy a user object)  }

function cryptLogin( var user: CRYPT_USER;
  const name: PAnsiChar;
  const password: PAnsiChar ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;

function cryptLogout( const user: CRYPT_USER ): Integer;
{$IFDEF WIN32} stdcall; {$ELSE} cdecl; {$ENDIF} external cryptlibname;


{****************************************************************************
*                                                                           *
*                           User Interface Functions                        *
*                                                                           *
****************************************************************************}




implementation

{ no implementation code now }

end.
