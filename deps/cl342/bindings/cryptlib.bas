Attribute VB_Name = "CRYPTLIB"

Option Explicit

'*****************************************************************************
'*                                                                           *
'*                        cryptlib External API Interface                    *
'*                       Copyright Peter Gutmann 1997-2012                   *
'*                                                                           *
'*                 adapted for Visual Basic Version 6  by W. Gothier         *
'*****************************************************************************


'-----------------------------------------------------------------------------

'This file has been created automatically by a perl script from the file:
'
'"cryptlib.h" dated Wed Aug 29 15:34:08 2012, filesize = 97645.
'
'Please check twice that the file matches the version of cryptlib.h
'in your cryptlib source! If this is not the right version, try to download an
'update from "http://cryptlib.sogot.de/". If the filesize or file creation
'date do not match, then please do not complain about problems.
'
'Examples using Visual Basic are available on the same web address.
'
'Published by W. Gothier, 
'mailto: problems@cryptlib.sogot.de if you find errors in this file.

'-----------------------------------------------------------------------------

  Public Const CRYPTLIB_VERSION As Long = 3410

'****************************************************************************
'*                                                                           *
'*                           Algorithm and Object Types                      *
'*                                                                           *
'****************************************************************************

' Algorithm and mode types 

Public Enum CRYPT_ALGO_TYPE
                      ' Algorithms 
    ' No encryption 
    CRYPT_ALGO_NONE                 ' No encryption 

    ' Conventional encryption 
    CRYPT_ALGO_DES                  ' DES 
    CRYPT_ALGO_3DES                 ' Triple DES 
    CRYPT_ALGO_IDEA                 ' IDEA (only used for PGP 2.x) 
    CRYPT_ALGO_CAST                 ' CAST-128 (only used for OpenPGP) 
    CRYPT_ALGO_RC2                  ' RC2 (disabled by default) 
    CRYPT_ALGO_RC4                  ' RC4 
    CRYPT_ALGO_RC5                  ' RC5 
    CRYPT_ALGO_AES                  ' AES 
    CRYPT_ALGO_BLOWFISH             ' Blowfish 

    ' Public-key encryption 
    CRYPT_ALGO_DH = 100             ' Diffie-Hellman 
    CRYPT_ALGO_RSA                  ' RSA 
    CRYPT_ALGO_DSA                  ' DSA 
    CRYPT_ALGO_ELGAMAL              ' ElGamal 
    CRYPT_ALGO_RESERVED1            ' Formerly KEA 
    CRYPT_ALGO_ECDSA                ' ECDSA 
    CRYPT_ALGO_ECDH                 ' ECDH 

    ' Hash algorithms 
    CRYPT_ALGO_RESERVED2 = 200      ' Formerly MD2 
    CRYPT_ALGO_RESERVED3            ' Formerly MD4 
    CRYPT_ALGO_MD5                  ' MD5 
    CRYPT_ALGO_SHA1                 ' SHA/SHA1 
    CRYPT_ALGO_RIPEMD160            ' RIPE-MD 160 
    CRYPT_ALGO_SHA2                 ' SHA-256 
        CRYPT_ALGO_SHA256 = CRYPT_ALGO_SHA2 ' Alternate name 
    CRYPT_ALGO_SHAng                ' Future SHA-nextgen standard 

    ' MAC's 
    CRYPT_ALGO_HMAC_MD5 = 300       ' HMAC-MD5 
    CRYPT_ALGO_HMAC_SHA1            ' HMAC-SHA 
    CRYPT_ALGO_HMAC_RIPEMD160       ' HMAC-RIPEMD-160 
    CRYPT_ALGO_HMAC_SHA2            ' HMAC-SHA2 
    CRYPT_ALGO_HMAC_SHAng           ' HMAC-future-SHA-nextgen 


'      Vendors may want to use their own algorithms that aren't part of the
'      general cryptlib suite.  The following values are for vendor-defined
'      algorithms, and can be used just like the named algorithm types (it's
'      up to the vendor to keep track of what _VENDOR1 actually corresponds
'      to) 

    CRYPT_ALGO_LAST                 ' Last possible crypt algo value 

'      In order that we can scan through a range of algorithms with
'      cryptQueryCapability(), we define the following boundary points for
'      each algorithm class 
    CRYPT_ALGO_FIRST_CONVENTIONAL = 1
    CRYPT_ALGO_LAST_CONVENTIONAL = 99
    CRYPT_ALGO_FIRST_PKC = 100
    CRYPT_ALGO_LAST_PKC = 199
    CRYPT_ALGO_FIRST_HASH = 200
    CRYPT_ALGO_LAST_HASH = 299
    CRYPT_ALGO_FIRST_MAC = 300
    CRYPT_ALGO_LAST_MAC = 399
    

End Enum

Public Enum CRYPT_MODE_TYPE
                      ' Block cipher modes 
    CRYPT_MODE_NONE                 ' No encryption mode 
    CRYPT_MODE_ECB                  ' ECB 
    CRYPT_MODE_CBC                  ' CBC 
    CRYPT_MODE_CFB                  ' CFB 
    CRYPT_MODE_OFB                  ' OFB 
    CRYPT_MODE_GCM                  ' GCM 
    CRYPT_MODE_LAST                 ' Last possible crypt mode value 
    

End Enum


' Keyset subtypes 

Public Enum CRYPT_KEYSET_TYPE
                      ' Keyset types 
    CRYPT_KEYSET_NONE               ' No keyset type 
    CRYPT_KEYSET_FILE               ' Generic flat file keyset 
    CRYPT_KEYSET_HTTP               ' Web page containing cert/CRL 
    CRYPT_KEYSET_LDAP               ' LDAP directory service 
    CRYPT_KEYSET_ODBC               ' Generic ODBC interface 
    CRYPT_KEYSET_DATABASE           ' Generic RDBMS interface 
    CRYPT_KEYSET_ODBC_STORE         ' ODBC certificate store 
    CRYPT_KEYSET_DATABASE_STORE     ' Database certificate store 
    CRYPT_KEYSET_LAST               ' Last possible keyset type 

    

End Enum

' Device subtypes 

Public Enum CRYPT_DEVICE_TYPE
                      ' Crypto device types 
    CRYPT_DEVICE_NONE               ' No crypto device 
    CRYPT_DEVICE_FORTEZZA           ' Fortezza card - Placeholder only 
    CRYPT_DEVICE_PKCS11             ' PKCS #11 crypto token 
    CRYPT_DEVICE_CRYPTOAPI          ' Microsoft CryptoAPI 
    CRYPT_DEVICE_HARDWARE           ' Generic crypo HW plugin 
    CRYPT_DEVICE_LAST               ' Last possible crypto device type 
    

End Enum

' Certificate subtypes 

Public Enum CRYPT_CERTTYPE_TYPE
                      ' Certificate object types 
    CRYPT_CERTTYPE_NONE             ' No certificate type 
    CRYPT_CERTTYPE_CERTIFICATE      ' Certificate 
    CRYPT_CERTTYPE_ATTRIBUTE_CERT   ' Attribute certificate 
    CRYPT_CERTTYPE_CERTCHAIN        ' PKCS #7 certificate chain 
    CRYPT_CERTTYPE_CERTREQUEST      ' PKCS #10 certification request 
    CRYPT_CERTTYPE_REQUEST_CERT     ' CRMF certification request 
    CRYPT_CERTTYPE_REQUEST_REVOCATION   ' CRMF revocation request 
    CRYPT_CERTTYPE_CRL              ' CRL 
    CRYPT_CERTTYPE_CMS_ATTRIBUTES   ' CMS attributes 
    CRYPT_CERTTYPE_RTCS_REQUEST     ' RTCS request 
    CRYPT_CERTTYPE_RTCS_RESPONSE    ' RTCS response 
    CRYPT_CERTTYPE_OCSP_REQUEST     ' OCSP request 
    CRYPT_CERTTYPE_OCSP_RESPONSE    ' OCSP response 
    CRYPT_CERTTYPE_PKIUSER          ' PKI user information 
    CRYPT_CERTTYPE_LAST             ' Last possible cert.type 
    

End Enum

' Envelope/data format subtypes 

Public Enum CRYPT_FORMAT_TYPE

    CRYPT_FORMAT_NONE               ' No format type 
    CRYPT_FORMAT_AUTO               ' Deenv, auto-determine type 
    CRYPT_FORMAT_CRYPTLIB           ' cryptlib native format 
    CRYPT_FORMAT_CMS                ' PKCS #7 / CMS / S/MIME fmt.
        CRYPT_FORMAT_PKCS7 = CRYPT_FORMAT_CMS
    CRYPT_FORMAT_SMIME              ' As CMS with MSG-style behaviour 
    CRYPT_FORMAT_PGP                ' PGP format 
    CRYPT_FORMAT_LAST               ' Last possible format type 
    

End Enum

' Session subtypes 

Public Enum CRYPT_SESSION_TYPE

    CRYPT_SESSION_NONE              ' No session type 
    CRYPT_SESSION_SSH               ' SSH 
    CRYPT_SESSION_SSH_SERVER        ' SSH server 
    CRYPT_SESSION_SSL               ' SSL/TLS 
    CRYPT_SESSION_SSL_SERVER        ' SSL/TLS server 
    CRYPT_SESSION_RTCS              ' RTCS 
    CRYPT_SESSION_RTCS_SERVER       ' RTCS server 
    CRYPT_SESSION_OCSP              ' OCSP 
    CRYPT_SESSION_OCSP_SERVER       ' OCSP server 
    CRYPT_SESSION_TSP               ' TSP 
    CRYPT_SESSION_TSP_SERVER        ' TSP server 
    CRYPT_SESSION_CMP               ' CMP 
    CRYPT_SESSION_CMP_SERVER        ' CMP server 
    CRYPT_SESSION_SCEP              ' SCEP 
    CRYPT_SESSION_SCEP_SERVER       ' SCEP server 
    CRYPT_SESSION_CERTSTORE_SERVER  ' HTTP cert store interface 
    CRYPT_SESSION_LAST              ' Last possible session type 
    

End Enum

' User subtypes 

Public Enum CRYPT_USER_TYPE

    CRYPT_USER_NONE                 ' No user type 
    CRYPT_USER_NORMAL               ' Normal user 
    CRYPT_USER_SO                   ' Security officer 
    CRYPT_USER_CA                   ' CA user 
    CRYPT_USER_LAST                 ' Last possible user type 
    

End Enum

'****************************************************************************
'*                                                                           *
'*                               Attribute Types                             *
'*                                                                           *
'****************************************************************************

'  Attribute types.  These are arranged in the following order:
'
'   PROPERTY    - Object property
'   ATTRIBUTE   - Generic attributes
'   OPTION      - Global or object-specific config.option
'   CTXINFO     - Context-specific attribute
'   CERTINFO    - Certificate-specific attribute
'   KEYINFO     - Keyset-specific attribute
'   DEVINFO     - Device-specific attribute
'   ENVINFO     - Envelope-specific attribute
'   SESSINFO    - Session-specific attribute
'   USERINFO    - User-specific attribute 

Public Enum CRYPT_ATTRIBUTE_TYPE

    CRYPT_ATTRIBUTE_NONE            ' Non-value 

    ' Used internally 
    CRYPT_PROPERTY_FIRST

    '*******************
    ' Object attributes 
    '*******************

    ' Object properties 
    CRYPT_PROPERTY_HIGHSECURITY     ' Owned+non-forwardcount+locked 
    CRYPT_PROPERTY_OWNER            ' Object owner 
    CRYPT_PROPERTY_FORWARDCOUNT     ' No.of times object can be forwarded 
    CRYPT_PROPERTY_LOCKED           ' Whether properties can be chged/read 
    CRYPT_PROPERTY_USAGECOUNT       ' Usage count before object expires 
    CRYPT_PROPERTY_NONEXPORTABLE    ' Whether key is nonexp.from context 

    ' Used internally 
    CRYPT_PROPERTY_LAST
    CRYPT_GENERIC_FIRST

    ' Extended error information 
    CRYPT_ATTRIBUTE_ERRORTYPE       ' Type of last error 
    CRYPT_ATTRIBUTE_ERRORLOCUS      ' Locus of last error 
    CRYPT_ATTRIBUTE_ERRORMESSAGE    ' Detailed error description 

    ' Generic information 
    CRYPT_ATTRIBUTE_CURRENT_GROUP   ' Cursor mgt: Group in attribute list 
    CRYPT_ATTRIBUTE_CURRENT         ' Cursor mgt: Entry in attribute list 
    CRYPT_ATTRIBUTE_CURRENT_INSTANCE    ' Cursor mgt: Instance in attribute list 
    CRYPT_ATTRIBUTE_BUFFERSIZE      ' Internal data buffer size 

    ' User internally 
    CRYPT_GENERIC_LAST
    CRYPT_OPTION_FIRST = 100

    '**************************
    ' Configuration attributes 
    '**************************

    ' cryptlib information (read-only) 
    CRYPT_OPTION_INFO_DESCRIPTION   ' Text description 
    CRYPT_OPTION_INFO_COPYRIGHT     ' Copyright notice 
    CRYPT_OPTION_INFO_MAJORVERSION  ' Major release version 
    CRYPT_OPTION_INFO_MINORVERSION  ' Minor release version 
    CRYPT_OPTION_INFO_STEPPING      ' Release stepping 

    ' Encryption options 
    CRYPT_OPTION_ENCR_ALGO          ' Encryption algorithm 
    CRYPT_OPTION_ENCR_HASH          ' Hash algorithm 
    CRYPT_OPTION_ENCR_MAC           ' MAC algorithm 

    ' PKC options 
    CRYPT_OPTION_PKC_ALGO           ' Public-key encryption algorithm 
    CRYPT_OPTION_PKC_KEYSIZE        ' Public-key encryption key size 

    ' Signature options 
    CRYPT_OPTION_SIG_ALGO           ' Signature algorithm 
    CRYPT_OPTION_SIG_KEYSIZE        ' Signature keysize 

    ' Keying options 
    CRYPT_OPTION_KEYING_ALGO        ' Key processing algorithm 
    CRYPT_OPTION_KEYING_ITERATIONS  ' Key processing iterations 

    ' Certificate options 
    CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES    ' Whether to sign unrecog.attrs 
    CRYPT_OPTION_CERT_VALIDITY      ' Certificate validity period 
    CRYPT_OPTION_CERT_UPDATEINTERVAL    ' CRL update interval 
    CRYPT_OPTION_CERT_COMPLIANCELEVEL   ' PKIX compliance level for cert chks.
    CRYPT_OPTION_CERT_REQUIREPOLICY     ' Whether explicit policy req'd for certs 

    ' CMS/SMIME options 
    CRYPT_OPTION_CMS_DEFAULTATTRIBUTES  ' Add default CMS attributes 
        CRYPT_OPTION_SMIME_DEFAULTATTRIBUTES = CRYPT_OPTION_CMS_DEFAULTATTRIBUTES

    ' LDAP keyset options 
    CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS  ' Object class 
    CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE   ' Object type to fetch 
    CRYPT_OPTION_KEYS_LDAP_FILTER       ' Query filter 
    CRYPT_OPTION_KEYS_LDAP_CACERTNAME   ' CA certificate attribute name 
    CRYPT_OPTION_KEYS_LDAP_CERTNAME     ' Certificate attribute name 
    CRYPT_OPTION_KEYS_LDAP_CRLNAME      ' CRL attribute name 
    CRYPT_OPTION_KEYS_LDAP_EMAILNAME    ' Email attribute name 

    ' Crypto device options 
    CRYPT_OPTION_DEVICE_PKCS11_DVR01    ' Name of first PKCS #11 driver 
    CRYPT_OPTION_DEVICE_PKCS11_DVR02    ' Name of second PKCS #11 driver 
    CRYPT_OPTION_DEVICE_PKCS11_DVR03    ' Name of third PKCS #11 driver 
    CRYPT_OPTION_DEVICE_PKCS11_DVR04    ' Name of fourth PKCS #11 driver 
    CRYPT_OPTION_DEVICE_PKCS11_DVR05    ' Name of fifth PKCS #11 driver 
    CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY ' Use only hardware mechanisms 

    ' Network access options 
    CRYPT_OPTION_NET_SOCKS_SERVER       ' Socks server name 
    CRYPT_OPTION_NET_SOCKS_USERNAME     ' Socks user name 
    CRYPT_OPTION_NET_HTTP_PROXY         ' Web proxy server 
    CRYPT_OPTION_NET_CONNECTTIMEOUT     ' Timeout for network connection setup 
    CRYPT_OPTION_NET_READTIMEOUT        ' Timeout for network reads 
    CRYPT_OPTION_NET_WRITETIMEOUT       ' Timeout for network writes 

    ' Miscellaneous options 
    CRYPT_OPTION_MISC_ASYNCINIT     ' Whether to init cryptlib async'ly 
    CRYPT_OPTION_MISC_SIDECHANNELPROTECTION  ' Protect against side-channel attacks 

    ' cryptlib state information 
    CRYPT_OPTION_CONFIGCHANGED      ' Whether in-mem.opts match on-disk ones 
    CRYPT_OPTION_SELFTESTOK         ' Whether self-test was completed and OK 

    ' Used internally 
    CRYPT_OPTION_LAST
    CRYPT_CTXINFO_FIRST = 1000

    '********************
    ' Context attributes 
    '********************

    ' Algorithm and mode information 
    CRYPT_CTXINFO_ALGO              ' Algorithm 
    CRYPT_CTXINFO_MODE              ' Mode 
    CRYPT_CTXINFO_NAME_ALGO         ' Algorithm name 
    CRYPT_CTXINFO_NAME_MODE         ' Mode name 
    CRYPT_CTXINFO_KEYSIZE           ' Key size in bytes 
    CRYPT_CTXINFO_BLOCKSIZE         ' Block size 
    CRYPT_CTXINFO_IVSIZE            ' IV size 
    CRYPT_CTXINFO_KEYING_ALGO       ' Key processing algorithm 
    CRYPT_CTXINFO_KEYING_ITERATIONS ' Key processing iterations 
    CRYPT_CTXINFO_KEYING_SALT       ' Key processing salt 
    CRYPT_CTXINFO_KEYING_VALUE      ' Value used to derive key 

    ' State information 
    CRYPT_CTXINFO_KEY               ' Key 
    CRYPT_CTXINFO_KEY_COMPONENTS    ' Public-key components 
    CRYPT_CTXINFO_IV                ' IV 
    CRYPT_CTXINFO_HASHVALUE         ' Hash value 

    ' Misc.information 
    CRYPT_CTXINFO_LABEL             ' Label for private/secret key 
    CRYPT_CTXINFO_PERSISTENT        ' Obj.is backed by device or keyset 

    ' Used internally 
    CRYPT_CTXINFO_LAST
    CRYPT_CERTINFO_FIRST = 2000

    '************************
    ' Certificate attributes 
    '************************

'      Because there are so many cert attributes, we break them down into
'      blocks to minimise the number of values that change if a new one is
'      added halfway through 

'      Pseudo-information on a cert object or meta-information which is used
'      to control the way that a cert object is processed 
    CRYPT_CERTINFO_SELFSIGNED       ' Cert is self-signed 
    CRYPT_CERTINFO_IMMUTABLE        ' Cert is signed and immutable 
    CRYPT_CERTINFO_XYZZY            ' Cert is a magic just-works cert 
    CRYPT_CERTINFO_CERTTYPE         ' Certificate object type 
    CRYPT_CERTINFO_FINGERPRINT      ' Certificate fingerprints 
        CRYPT_CERTINFO_FINGERPRINT_MD5 = CRYPT_CERTINFO_FINGERPRINT
    CRYPT_CERTINFO_FINGERPRINT_SHA1
        CRYPT_CERTINFO_FINGERPRINT_SHA = CRYPT_CERTINFO_FINGERPRINT_SHA1
    CRYPT_CERTINFO_FINGERPRINT_SHA2
    CRYPT_CERTINFO_FINGERPRINT_SHAng
    CRYPT_CERTINFO_CURRENT_CERTIFICATE ' Cursor mgt: Rel.pos in chain/CRL/OCSP 
    CRYPT_CERTINFO_TRUSTED_USAGE    ' Usage that cert is trusted for 
    CRYPT_CERTINFO_TRUSTED_IMPLICIT ' Whether cert is implicitly trusted 
    CRYPT_CERTINFO_SIGNATURELEVEL   ' Amount of detail to include in sigs.

    ' General certificate object information 
    CRYPT_CERTINFO_VERSION          ' Cert.format version 
    CRYPT_CERTINFO_SERIALNUMBER     ' Serial number 
    CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO     ' Public key 
    CRYPT_CERTINFO_CERTIFICATE      ' User certificate 
        CRYPT_CERTINFO_USERCERTIFICATE = CRYPT_CERTINFO_CERTIFICATE
    CRYPT_CERTINFO_CACERTIFICATE    ' CA certificate 
    CRYPT_CERTINFO_ISSUERNAME       ' Issuer DN 
    CRYPT_CERTINFO_VALIDFROM        ' Cert valid-from time 
    CRYPT_CERTINFO_VALIDTO          ' Cert valid-to time 
    CRYPT_CERTINFO_SUBJECTNAME      ' Subject DN 
    CRYPT_CERTINFO_ISSUERUNIQUEID   ' Issuer unique ID 
    CRYPT_CERTINFO_SUBJECTUNIQUEID  ' Subject unique ID 
    CRYPT_CERTINFO_CERTREQUEST      ' Cert.request (DN + public key) 
    CRYPT_CERTINFO_THISUPDATE       ' CRL/OCSP current-update time 
    CRYPT_CERTINFO_NEXTUPDATE       ' CRL/OCSP next-update time 
    CRYPT_CERTINFO_REVOCATIONDATE   ' CRL/OCSP cert-revocation time 
    CRYPT_CERTINFO_REVOCATIONSTATUS ' OCSP revocation status 
    CRYPT_CERTINFO_CERTSTATUS       ' RTCS certificate status 
    CRYPT_CERTINFO_DN               ' Currently selected DN in string form 
    CRYPT_CERTINFO_PKIUSER_ID       ' PKI user ID 
    CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD    ' PKI user issue password 
    CRYPT_CERTINFO_PKIUSER_REVPASSWORD      ' PKI user revocation password 

'      X.520 Distinguished Name components.  This is a composite field, the
'      DN to be manipulated is selected through the addition of a
'      pseudocomponent, and then one of the following is used to access the
'      DN components directly 
    CRYPT_CERTINFO_COUNTRYNAME = CRYPT_CERTINFO_FIRST + 100     ' countryName 
    CRYPT_CERTINFO_STATEORPROVINCENAME  ' stateOrProvinceName 
    CRYPT_CERTINFO_LOCALITYNAME         ' localityName 
    CRYPT_CERTINFO_ORGANIZATIONNAME     ' organizationName 
        CRYPT_CERTINFO_ORGANISATIONNAME = CRYPT_CERTINFO_ORGANIZATIONNAME
    CRYPT_CERTINFO_ORGANIZATIONALUNITNAME   ' organizationalUnitName 
        CRYPT_CERTINFO_ORGANISATIONALUNITNAME = CRYPT_CERTINFO_ORGANIZATIONALUNITNAME
    CRYPT_CERTINFO_COMMONNAME       ' commonName 

'      X.509 General Name components.  These are handled in the same way as
'      the DN composite field, with the current GeneralName being selected by
'      a pseudo-component after which the individual components can be
'      modified through one of the following 
    CRYPT_CERTINFO_OTHERNAME_TYPEID         ' otherName.typeID 
    CRYPT_CERTINFO_OTHERNAME_VALUE          ' otherName.value 
    CRYPT_CERTINFO_RFC822NAME               ' rfc822Name 
        CRYPT_CERTINFO_EMAIL = CRYPT_CERTINFO_RFC822NAME
    CRYPT_CERTINFO_DNSNAME                  ' dNSName 
    CRYPT_CERTINFO_DIRECTORYNAME            ' directoryName 
    CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER    ' ediPartyName.nameAssigner 
    CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME   ' ediPartyName.partyName 
    CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER    ' uniformResourceIdentifier 
        CRYPT_CERTINFO_URL = CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER
    CRYPT_CERTINFO_IPADDRESS                ' iPAddress 
    CRYPT_CERTINFO_REGISTEREDID             ' registeredID 

'      X.509 certificate extensions.  Although it would be nicer to use names
'      that match the extensions more closely (e.g.
'      CRYPT_CERTINFO_BASICCONSTRAINTS_PATHLENCONSTRAINT), these exceed the
'      32-character ANSI minimum length for unique names, and get really
'      hairy once you get into the weird policy constraints extensions whose
'      names wrap around the screen about three times.
'
'      The following values are defined in OID order, this isn't absolutely
'      necessary but saves an extra layer of processing when encoding them 

'      1 2 840 113549 1 9 7 challengePassword.  This is here even though it's
'      a CMS attribute because SCEP stuffs it into PKCS #10 requests 
    CRYPT_CERTINFO_CHALLENGEPASSWORD = CRYPT_CERTINFO_FIRST + 200

    ' 1 3 6 1 4 1 3029 3 1 4 cRLExtReason 
    CRYPT_CERTINFO_CRLEXTREASON

    ' 1 3 6 1 4 1 3029 3 1 5 keyFeatures 
    CRYPT_CERTINFO_KEYFEATURES

    ' 1 3 6 1 5 5 7 1 1 authorityInfoAccess 
    CRYPT_CERTINFO_AUTHORITYINFOACCESS
    CRYPT_CERTINFO_AUTHORITYINFO_RTCS       ' accessDescription.accessLocation 
    CRYPT_CERTINFO_AUTHORITYINFO_OCSP       ' accessDescription.accessLocation 
    CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS  ' accessDescription.accessLocation 
    CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE  ' accessDescription.accessLocation 
    CRYPT_CERTINFO_AUTHORITYINFO_CRLS       ' accessDescription.accessLocation 

    ' 1 3 6 1 5 5 7 1 2 biometricInfo 
    CRYPT_CERTINFO_BIOMETRICINFO
    CRYPT_CERTINFO_BIOMETRICINFO_TYPE       ' biometricData.typeOfData 
    CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO   ' biometricData.hashAlgorithm 
    CRYPT_CERTINFO_BIOMETRICINFO_HASH       ' biometricData.dataHash 
    CRYPT_CERTINFO_BIOMETRICINFO_URL        ' biometricData.sourceDataUri 

    ' 1 3 6 1 5 5 7 1 3 qcStatements 
    CRYPT_CERTINFO_QCSTATEMENT
    CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS
                    ' qcStatement.statementInfo.semanticsIdentifier 
    CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY
                    ' qcStatement.statementInfo.nameRegistrationAuthorities 

    ' 1 3 6 1 5 5 7 1 7 ipAddrBlocks 
    CRYPT_CERTINFO_IPADDRESSBLOCKS
    CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY    ' addressFamily 
'  CRYPT_CERTINFO_IPADDRESSBLOCKS_INHERIT, // ipAddress.inherit 
    CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX   ' ipAddress.addressPrefix 
    CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN      ' ipAddress.addressRangeMin 
    CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX      ' ipAddress.addressRangeMax 

    ' 1 3 6 1 5 5 7 1 8 autonomousSysIds 
    CRYPT_CERTINFO_AUTONOMOUSSYSIDS
'  CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_INHERIT,// asNum.inherit 
    CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID    ' asNum.id 
    CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN   ' asNum.min 
    CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX   ' asNum.max 

    ' 1 3 6 1 5 5 7 48 1 2 ocspNonce 
    CRYPT_CERTINFO_OCSP_NONCE               ' nonce 

    ' 1 3 6 1 5 5 7 48 1 4 ocspAcceptableResponses 
    CRYPT_CERTINFO_OCSP_RESPONSE
    CRYPT_CERTINFO_OCSP_RESPONSE_OCSP       ' OCSP standard response 

    ' 1 3 6 1 5 5 7 48 1 5 ocspNoCheck 
    CRYPT_CERTINFO_OCSP_NOCHECK

    ' 1 3 6 1 5 5 7 48 1 6 ocspArchiveCutoff 
    CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF

    ' 1 3 6 1 5 5 7 48 1 11 subjectInfoAccess 
    CRYPT_CERTINFO_SUBJECTINFOACCESS
    CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING ' accessDescription.accessLocation 
    CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY ' accessDescription.accessLocation 
    CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY ' accessDescription.accessLocation 
    CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST ' accessDescription.accessLocation 
    CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT ' accessDescription.accessLocation 

    ' 1 3 36 8 3 1 siggDateOfCertGen 
    CRYPT_CERTINFO_SIGG_DATEOFCERTGEN

    ' 1 3 36 8 3 2 siggProcuration 
    CRYPT_CERTINFO_SIGG_PROCURATION
    CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY     ' country 
    CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION  ' typeOfSubstitution 
    CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR  ' signingFor.thirdPerson 

    ' 1 3 36 8 3 3 siggAdmissions 
    CRYPT_CERTINFO_SIGG_ADMISSIONS
    CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY    ' authority 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID     ' namingAuth.iD 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL    ' namingAuth.uRL 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT   ' namingAuth.text 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM   ' professionItem 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID    ' professionOID 
    CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER   ' registrationNumber 

    ' 1 3 36 8 3 4 siggMonetaryLimit 
    CRYPT_CERTINFO_SIGG_MONETARYLIMIT
    CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY   ' currency 
    CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT     ' amount 
    CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT   ' exponent 

    ' 1 3 36 8 3 5 siggDeclarationOfMajority 
    CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY
    CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY   ' fullAgeAtCountry 

    ' 1 3 36 8 3 8 siggRestriction 
    CRYPT_CERTINFO_SIGG_RESTRICTION

    ' 1 3 36 8 3 13 siggCertHash 
    CRYPT_CERTINFO_SIGG_CERTHASH

    ' 1 3 36 8 3 15 siggAdditionalInformation 
    CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION

    ' 1 3 101 1 4 1 strongExtranet 
    CRYPT_CERTINFO_STRONGEXTRANET
    CRYPT_CERTINFO_STRONGEXTRANET_ZONE      ' sxNetIDList.sxNetID.zone 
    CRYPT_CERTINFO_STRONGEXTRANET_ID        ' sxNetIDList.sxNetID.id 

    ' 2 5 29 9 subjectDirectoryAttributes 
    CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES
    CRYPT_CERTINFO_SUBJECTDIR_TYPE          ' attribute.type 
    CRYPT_CERTINFO_SUBJECTDIR_VALUES        ' attribute.values 

    ' 2 5 29 14 subjectKeyIdentifier 
    CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER

    ' 2 5 29 15 keyUsage 
    CRYPT_CERTINFO_KEYUSAGE

    ' 2 5 29 16 privateKeyUsagePeriod 
    CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD
    CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE     ' notBefore 
    CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER      ' notAfter 

    ' 2 5 29 17 subjectAltName 
    CRYPT_CERTINFO_SUBJECTALTNAME

    ' 2 5 29 18 issuerAltName 
    CRYPT_CERTINFO_ISSUERALTNAME

    ' 2 5 29 19 basicConstraints 
    CRYPT_CERTINFO_BASICCONSTRAINTS
    CRYPT_CERTINFO_CA                       ' cA 
        CRYPT_CERTINFO_AUTHORITY = CRYPT_CERTINFO_CA
    CRYPT_CERTINFO_PATHLENCONSTRAINT        ' pathLenConstraint 

    ' 2 5 29 20 cRLNumber 
    CRYPT_CERTINFO_CRLNUMBER

    ' 2 5 29 21 cRLReason 
    CRYPT_CERTINFO_CRLREASON

    ' 2 5 29 23 holdInstructionCode 
    CRYPT_CERTINFO_HOLDINSTRUCTIONCODE

    ' 2 5 29 24 invalidityDate 
    CRYPT_CERTINFO_INVALIDITYDATE

    ' 2 5 29 27 deltaCRLIndicator 
    CRYPT_CERTINFO_DELTACRLINDICATOR

    ' 2 5 29 28 issuingDistributionPoint 
    CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT
    CRYPT_CERTINFO_ISSUINGDIST_FULLNAME     ' distributionPointName.fullName 
    CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY    ' onlyContainsUserCerts 
    CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY  ' onlyContainsCACerts 
    CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY  ' onlySomeReasons 
    CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL  ' indirectCRL 

    ' 2 5 29 29 certificateIssuer 
    CRYPT_CERTINFO_CERTIFICATEISSUER

    ' 2 5 29 30 nameConstraints 
    CRYPT_CERTINFO_NAMECONSTRAINTS
    CRYPT_CERTINFO_PERMITTEDSUBTREES        ' permittedSubtrees 
    CRYPT_CERTINFO_EXCLUDEDSUBTREES         ' excludedSubtrees 

    ' 2 5 29 31 cRLDistributionPoint 
    CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT
    CRYPT_CERTINFO_CRLDIST_FULLNAME         ' distributionPointName.fullName 
    CRYPT_CERTINFO_CRLDIST_REASONS          ' reasons 
    CRYPT_CERTINFO_CRLDIST_CRLISSUER        ' cRLIssuer 

    ' 2 5 29 32 certificatePolicies 
    CRYPT_CERTINFO_CERTIFICATEPOLICIES
    CRYPT_CERTINFO_CERTPOLICYID         ' policyInformation.policyIdentifier 
    CRYPT_CERTINFO_CERTPOLICY_CPSURI
        ' policyInformation.policyQualifiers.qualifier.cPSuri 
    CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION
        ' policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.organization 
    CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS
        ' policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.noticeNumbers 
    CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT
        ' policyInformation.policyQualifiers.qualifier.userNotice.explicitText 

    ' 2 5 29 33 policyMappings 
    CRYPT_CERTINFO_POLICYMAPPINGS
    CRYPT_CERTINFO_ISSUERDOMAINPOLICY   ' policyMappings.issuerDomainPolicy 
    CRYPT_CERTINFO_SUBJECTDOMAINPOLICY  ' policyMappings.subjectDomainPolicy 

    ' 2 5 29 35 authorityKeyIdentifier 
    CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER
    CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER  ' keyIdentifier 
    CRYPT_CERTINFO_AUTHORITY_CERTISSUER     ' authorityCertIssuer 
    CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER   ' authorityCertSerialNumber 

    ' 2 5 29 36 policyConstraints 
    CRYPT_CERTINFO_POLICYCONSTRAINTS
    CRYPT_CERTINFO_REQUIREEXPLICITPOLICY    ' policyConstraints.requireExplicitPolicy 
    CRYPT_CERTINFO_INHIBITPOLICYMAPPING     ' policyConstraints.inhibitPolicyMapping 

    ' 2 5 29 37 extKeyUsage 
    CRYPT_CERTINFO_EXTKEYUSAGE
    CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING  ' individualCodeSigning 
    CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING  ' commercialCodeSigning 
    CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING   ' certTrustListSigning 
    CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING   ' timeStampSigning 
    CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO  ' serverGatedCrypto 
    CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM    ' encrypedFileSystem 
    CRYPT_CERTINFO_EXTKEY_SERVERAUTH        ' serverAuth 
    CRYPT_CERTINFO_EXTKEY_CLIENTAUTH        ' clientAuth 
    CRYPT_CERTINFO_EXTKEY_CODESIGNING       ' codeSigning 
    CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION   ' emailProtection 
    CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM    ' ipsecEndSystem 
    CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL       ' ipsecTunnel 
    CRYPT_CERTINFO_EXTKEY_IPSECUSER         ' ipsecUser 
    CRYPT_CERTINFO_EXTKEY_TIMESTAMPING      ' timeStamping 
    CRYPT_CERTINFO_EXTKEY_OCSPSIGNING       ' ocspSigning 
    CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE  ' directoryService 
    CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE       ' anyExtendedKeyUsage 
    CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO  ' serverGatedCrypto 
    CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA   ' serverGatedCrypto CA 

    ' 2 5 29 40 crlStreamIdentifier 
    CRYPT_CERTINFO_CRLSTREAMIDENTIFIER

    ' 2 5 29 46 freshestCRL 
    CRYPT_CERTINFO_FRESHESTCRL
    CRYPT_CERTINFO_FRESHESTCRL_FULLNAME     ' distributionPointName.fullName 
    CRYPT_CERTINFO_FRESHESTCRL_REASONS      ' reasons 
    CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER    ' cRLIssuer 

    ' 2 5 29 47 orderedList 
    CRYPT_CERTINFO_ORDEREDLIST

    ' 2 5 29 51 baseUpdateTime 
    CRYPT_CERTINFO_BASEUPDATETIME

    ' 2 5 29 53 deltaInfo 
    CRYPT_CERTINFO_DELTAINFO
    CRYPT_CERTINFO_DELTAINFO_LOCATION       ' deltaLocation 
    CRYPT_CERTINFO_DELTAINFO_NEXTDELTA      ' nextDelta 

    ' 2 5 29 54 inhibitAnyPolicy 
    CRYPT_CERTINFO_INHIBITANYPOLICY

    ' 2 5 29 58 toBeRevoked 
    CRYPT_CERTINFO_TOBEREVOKED
    CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER   ' certificateIssuer 
    CRYPT_CERTINFO_TOBEREVOKED_REASONCODE   ' reasonCode 
    CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME   ' revocationTime 
    CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER ' certSerialNumber 

    ' 2 5 29 59 revokedGroups 
    CRYPT_CERTINFO_REVOKEDGROUPS
    CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER ' certificateIssuer 
    CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE ' reasonCode 
    CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE ' invalidityDate 
    CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER ' startingNumber 
    CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER   ' endingNumber 

    ' 2 5 29 60 expiredCertsOnCRL 
    CRYPT_CERTINFO_EXPIREDCERTSONCRL

    ' 2 5 29 63 aaIssuingDistributionPoint 
    CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT
    CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME   ' distributionPointName.fullName 
    CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY ' onlySomeReasons 
    CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL    ' indirectCRL 
    CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS  ' containsUserAttributeCerts 
    CRYPT_CERTINFO_AAISSUINGDIST_AACERTS    ' containsAACerts 
    CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS   ' containsSOAPublicKeyCerts 

    ' 2 16 840 1 113730 1 x Netscape extensions 
    CRYPT_CERTINFO_NS_CERTTYPE              ' netscape-cert-type 
    CRYPT_CERTINFO_NS_BASEURL               ' netscape-base-url 
    CRYPT_CERTINFO_NS_REVOCATIONURL         ' netscape-revocation-url 
    CRYPT_CERTINFO_NS_CAREVOCATIONURL       ' netscape-ca-revocation-url 
    CRYPT_CERTINFO_NS_CERTRENEWALURL        ' netscape-cert-renewal-url 
    CRYPT_CERTINFO_NS_CAPOLICYURL           ' netscape-ca-policy-url 
    CRYPT_CERTINFO_NS_SSLSERVERNAME         ' netscape-ssl-server-name 
    CRYPT_CERTINFO_NS_COMMENT               ' netscape-comment 

    ' 2 23 42 7 0 SET hashedRootKey 
    CRYPT_CERTINFO_SET_HASHEDROOTKEY
    CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT    ' rootKeyThumbPrint 

    ' 2 23 42 7 1 SET certificateType 
    CRYPT_CERTINFO_SET_CERTIFICATETYPE

    ' 2 23 42 7 2 SET merchantData 
    CRYPT_CERTINFO_SET_MERCHANTDATA
    CRYPT_CERTINFO_SET_MERID                ' merID 
    CRYPT_CERTINFO_SET_MERACQUIRERBIN       ' merAcquirerBIN 
    CRYPT_CERTINFO_SET_MERCHANTLANGUAGE     ' merNames.language 
    CRYPT_CERTINFO_SET_MERCHANTNAME         ' merNames.name 
    CRYPT_CERTINFO_SET_MERCHANTCITY         ' merNames.city 
    CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE ' merNames.stateProvince 
    CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE   ' merNames.postalCode 
    CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME  ' merNames.countryName 
    CRYPT_CERTINFO_SET_MERCOUNTRY           ' merCountry 
    CRYPT_CERTINFO_SET_MERAUTHFLAG          ' merAuthFlag 

    ' 2 23 42 7 3 SET certCardRequired 
    CRYPT_CERTINFO_SET_CERTCARDREQUIRED

    ' 2 23 42 7 4 SET tunneling 
    CRYPT_CERTINFO_SET_TUNNELING
        CRYPT_CERTINFO_SET_TUNNELLING = CRYPT_CERTINFO_SET_TUNNELING
    CRYPT_CERTINFO_SET_TUNNELINGFLAG        ' tunneling 
        CRYPT_CERTINFO_SET_TUNNELLINGFLAG = CRYPT_CERTINFO_SET_TUNNELINGFLAG
    CRYPT_CERTINFO_SET_TUNNELINGALGID       ' tunnelingAlgID 
        CRYPT_CERTINFO_SET_TUNNELLINGALGID = CRYPT_CERTINFO_SET_TUNNELINGALGID

    ' S/MIME attributes 

    ' 1 2 840 113549 1 9 3 contentType 
    CRYPT_CERTINFO_CMS_CONTENTTYPE = CRYPT_CERTINFO_FIRST + 500

    ' 1 2 840 113549 1 9 4 messageDigest 
    CRYPT_CERTINFO_CMS_MESSAGEDIGEST

    ' 1 2 840 113549 1 9 5 signingTime 
    CRYPT_CERTINFO_CMS_SIGNINGTIME

    ' 1 2 840 113549 1 9 6 counterSignature 
    CRYPT_CERTINFO_CMS_COUNTERSIGNATURE     ' counterSignature 

    ' 1 2 840 113549 1 9 13 signingDescription 
    CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION

    ' 1 2 840 113549 1 9 15 sMIMECapabilities 
    CRYPT_CERTINFO_CMS_SMIMECAPABILITIES
    CRYPT_CERTINFO_CMS_SMIMECAP_3DES        ' 3DES encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_AES         ' AES encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_CAST128     ' CAST-128 encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_IDEA        ' IDEA encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_RC2         ' RC2 encryption (w.128 key) 
    CRYPT_CERTINFO_CMS_SMIMECAP_RC5         ' RC5 encryption (w.128 key) 
    CRYPT_CERTINFO_CMS_SMIMECAP_SKIPJACK    ' Skipjack encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_DES         ' DES encryption 
    CRYPT_CERTINFO_CMS_SMIMECAP_SHAng       ' SHA2-ng hash 
    CRYPT_CERTINFO_CMS_SMIMECAP_SHA2        ' SHA2-256 hash 
    CRYPT_CERTINFO_CMS_SMIMECAP_SHA1        ' SHA1 hash 
    CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng  ' HMAC-SHA2-ng MAC 
    CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2   ' HMAC-SHA2-256 MAC 
    CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1   ' HMAC-SHA1 MAC 
    CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256  ' AuthEnc w.256-bit key 
    CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128  ' AuthEnc w.128-bit key 
    CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng   ' RSA with SHA-ng signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2    ' RSA with SHA2-256 signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1    ' RSA with SHA1 signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1    ' DSA with SHA-1 signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng ' ECDSA with SHA-ng signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2  ' ECDSA with SHA2-256 signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1  ' ECDSA with SHA-1 signing 
    CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA    ' preferSignedData 
    CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY    ' canNotDecryptAny 
    CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE  ' preferBinaryInside 

    ' 1 2 840 113549 1 9 16 2 1 receiptRequest 
    CRYPT_CERTINFO_CMS_RECEIPTREQUEST
    CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER  ' contentIdentifier 
    CRYPT_CERTINFO_CMS_RECEIPT_FROM         ' receiptsFrom 
    CRYPT_CERTINFO_CMS_RECEIPT_TO           ' receiptsTo 

    ' 1 2 840 113549 1 9 16 2 2 essSecurityLabel 
    CRYPT_CERTINFO_CMS_SECURITYLABEL
    CRYPT_CERTINFO_CMS_SECLABEL_POLICY      ' securityPolicyIdentifier 
    CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION  ' securityClassification 
    CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK ' privacyMark 
    CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE     ' securityCategories.securityCategory.type 
    CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE    ' securityCategories.securityCategory.value 

    ' 1 2 840 113549 1 9 16 2 3 mlExpansionHistory 
    CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY
    CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER  ' mlData.mailListIdentifier.issuerAndSerialNumber 
    CRYPT_CERTINFO_CMS_MLEXP_TIME           ' mlData.expansionTime 
    CRYPT_CERTINFO_CMS_MLEXP_NONE           ' mlData.mlReceiptPolicy.none 
    CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF      ' mlData.mlReceiptPolicy.insteadOf.generalNames.generalName 
    CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO   ' mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName 

    ' 1 2 840 113549 1 9 16 2 4 contentHints 
    CRYPT_CERTINFO_CMS_CONTENTHINTS
    CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION  ' contentDescription 
    CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE     ' contentType 

    ' 1 2 840 113549 1 9 16 2 9 equivalentLabels 
    CRYPT_CERTINFO_CMS_EQUIVALENTLABEL
    CRYPT_CERTINFO_CMS_EQVLABEL_POLICY      ' securityPolicyIdentifier 
    CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION  ' securityClassification 
    CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK ' privacyMark 
    CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE     ' securityCategories.securityCategory.type 
    CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE    ' securityCategories.securityCategory.value 

    ' 1 2 840 113549 1 9 16 2 12 signingCertificate 
    CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE
    CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID  ' certs.essCertID 
    CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES ' policies.policyInformation.policyIdentifier 

    ' 1 2 840 113549 1 9 16 2 47 signingCertificateV2 
    CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2
    CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2  ' certs.essCertID 
    CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES ' policies.policyInformation.policyIdentifier 

    ' 1 2 840 113549 1 9 16 2 15 signaturePolicyID 
    CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID
    CRYPT_CERTINFO_CMS_SIGPOLICYID          ' sigPolicyID 
    CRYPT_CERTINFO_CMS_SIGPOLICYHASH        ' sigPolicyHash 
    CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI     ' sigPolicyQualifiers.sigPolicyQualifier.cPSuri 
    CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION
        ' sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization 
    CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS
        ' sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers 
    CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT
        ' sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText 

    ' 1 2 840 113549 1 9 16 9 signatureTypeIdentifier 
    CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER
    CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG  ' originatorSig 
    CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG  ' domainSig 
    CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES  ' additionalAttributesSig 
    CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG  ' reviewSig 

    ' 1 2 840 113549 1 9 25 3 randomNonce 
    CRYPT_CERTINFO_CMS_NONCE                ' randomNonce 

'      SCEP attributes:
'      2 16 840 1 113733 1 9 2 messageType
'      2 16 840 1 113733 1 9 3 pkiStatus
'      2 16 840 1 113733 1 9 4 failInfo
'      2 16 840 1 113733 1 9 5 senderNonce
'      2 16 840 1 113733 1 9 6 recipientNonce
'      2 16 840 1 113733 1 9 7 transID 
    CRYPT_CERTINFO_SCEP_MESSAGETYPE         ' messageType 
    CRYPT_CERTINFO_SCEP_PKISTATUS           ' pkiStatus 
    CRYPT_CERTINFO_SCEP_FAILINFO            ' failInfo 
    CRYPT_CERTINFO_SCEP_SENDERNONCE         ' senderNonce 
    CRYPT_CERTINFO_SCEP_RECIPIENTNONCE      ' recipientNonce 
    CRYPT_CERTINFO_SCEP_TRANSACTIONID       ' transID 

    ' 1 3 6 1 4 1 311 2 1 10 spcAgencyInfo 
    CRYPT_CERTINFO_CMS_SPCAGENCYINFO
    CRYPT_CERTINFO_CMS_SPCAGENCYURL         ' spcAgencyInfo.url 

    ' 1 3 6 1 4 1 311 2 1 11 spcStatementType 
    CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE
    CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING    ' individualCodeSigning 
    CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING    ' commercialCodeSigning 

    ' 1 3 6 1 4 1 311 2 1 12 spcOpusInfo 
    CRYPT_CERTINFO_CMS_SPCOPUSINFO
    CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME     ' spcOpusInfo.name 
    CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL      ' spcOpusInfo.url 

    ' Used internally 
    CRYPT_CERTINFO_LAST
    CRYPT_KEYINFO_FIRST = 3000

    '*******************
    ' Keyset attributes 
    '*******************

    CRYPT_KEYINFO_QUERY             ' Keyset query 
    CRYPT_KEYINFO_QUERY_REQUESTS    ' Query of requests in cert store 

    ' Used internally 
    CRYPT_KEYINFO_LAST
    CRYPT_DEVINFO_FIRST = 4000

    '*******************
    ' Device attributes 
    '*******************

    CRYPT_DEVINFO_INITIALISE    ' Initialise device for use 
        CRYPT_DEVINFO_INITIALIZE = CRYPT_DEVINFO_INITIALISE
    CRYPT_DEVINFO_AUTHENT_USER  ' Authenticate user to device 
    CRYPT_DEVINFO_AUTHENT_SUPERVISOR    ' Authenticate supervisor to dev.
    CRYPT_DEVINFO_SET_AUTHENT_USER  ' Set user authent.value 
    CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR    ' Set supervisor auth.val.
    CRYPT_DEVINFO_ZEROISE   ' Zeroise device 
        CRYPT_DEVINFO_ZEROIZE = CRYPT_DEVINFO_ZEROISE
    CRYPT_DEVINFO_LOGGEDIN      ' Whether user is logged in 
    CRYPT_DEVINFO_LABEL         ' Device/token label 

    ' Used internally 
    CRYPT_DEVINFO_LAST
    CRYPT_ENVINFO_FIRST = 5000

    '*********************
    ' Envelope attributes 
    '*********************

'      Pseudo-information on an envelope or meta-information which is used to
'      control the way that data in an envelope is processed 
    CRYPT_ENVINFO_DATASIZE          ' Data size information 
    CRYPT_ENVINFO_COMPRESSION       ' Compression information 
    CRYPT_ENVINFO_CONTENTTYPE       ' Inner CMS content type 
    CRYPT_ENVINFO_DETACHEDSIGNATURE ' Detached signature 
    CRYPT_ENVINFO_SIGNATURE_RESULT  ' Signature check result 
    CRYPT_ENVINFO_INTEGRITY         ' Integrity-protection level 

    ' Resources required for enveloping/deenveloping 
    CRYPT_ENVINFO_PASSWORD          ' User password 
    CRYPT_ENVINFO_KEY               ' Conventional encryption key 
    CRYPT_ENVINFO_SIGNATURE         ' Signature/signature check key 
    CRYPT_ENVINFO_SIGNATURE_EXTRADATA   ' Extra information added to CMS sigs 
    CRYPT_ENVINFO_RECIPIENT         ' Recipient email address 
    CRYPT_ENVINFO_PUBLICKEY         ' PKC encryption key 
    CRYPT_ENVINFO_PRIVATEKEY        ' PKC decryption key 
    CRYPT_ENVINFO_PRIVATEKEY_LABEL  ' Label of PKC decryption key 
    CRYPT_ENVINFO_ORIGINATOR        ' Originator info/key 
    CRYPT_ENVINFO_SESSIONKEY        ' Session key 
    CRYPT_ENVINFO_HASH              ' Hash value 
    CRYPT_ENVINFO_TIMESTAMP         ' Timestamp information 

    ' Keysets used to retrieve keys needed for enveloping/deenveloping 
    CRYPT_ENVINFO_KEYSET_SIGCHECK   ' Signature check keyset 
    CRYPT_ENVINFO_KEYSET_ENCRYPT    ' PKC encryption keyset 
    CRYPT_ENVINFO_KEYSET_DECRYPT    ' PKC decryption keyset 

    ' Used internally 
    CRYPT_ENVINFO_LAST
    CRYPT_SESSINFO_FIRST = 6000

    '********************
    ' Session attributes 
    '********************

    ' Pseudo-information about the session 
    CRYPT_SESSINFO_ACTIVE           ' Whether session is active 
    CRYPT_SESSINFO_CONNECTIONACTIVE ' Whether network connection is active 

    ' Security-related information 
    CRYPT_SESSINFO_USERNAME         ' User name 
    CRYPT_SESSINFO_PASSWORD         ' Password 
    CRYPT_SESSINFO_PRIVATEKEY       ' Server/client private key 
    CRYPT_SESSINFO_KEYSET           ' Certificate store 
    CRYPT_SESSINFO_AUTHRESPONSE     ' Session authorisation OK 

    ' Client/server information 
    CRYPT_SESSINFO_SERVER_NAME      ' Server name 
    CRYPT_SESSINFO_SERVER_PORT      ' Server port number 
    CRYPT_SESSINFO_SERVER_FINGERPRINT ' Server key fingerprint 
    CRYPT_SESSINFO_CLIENT_NAME      ' Client name 
    CRYPT_SESSINFO_CLIENT_PORT      ' Client port number 
    CRYPT_SESSINFO_SESSION          ' Transport mechanism 
    CRYPT_SESSINFO_NETWORKSOCKET    ' User-supplied network socket 

    ' Generic protocol-related information 
    CRYPT_SESSINFO_VERSION          ' Protocol version 
    CRYPT_SESSINFO_REQUEST          ' Cert.request object 
    CRYPT_SESSINFO_RESPONSE         ' Cert.response object 
    CRYPT_SESSINFO_CACERTIFICATE    ' Issuing CA certificate 

    ' Protocol-specific information 
    CRYPT_SESSINFO_CMP_REQUESTTYPE  ' Request type 
    CRYPT_SESSINFO_CMP_PRIVKEYSET   ' Private-key keyset 
    CRYPT_SESSINFO_SSH_CHANNEL      ' SSH current channel 
    CRYPT_SESSINFO_SSH_CHANNEL_TYPE ' SSH channel type 
    CRYPT_SESSINFO_SSH_CHANNEL_ARG1 ' SSH channel argument 1 
    CRYPT_SESSINFO_SSH_CHANNEL_ARG2 ' SSH channel argument 2 
    CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE ' SSH channel active 
    CRYPT_SESSINFO_SSL_OPTIONS      ' SSL/TLS protocol options 
    CRYPT_SESSINFO_TSP_MSGIMPRINT   ' TSP message imprint 

    ' Used internally 
    CRYPT_SESSINFO_LAST
    CRYPT_USERINFO_FIRST = 7000

    '********************
    ' User attributes 
    '********************

    ' Security-related information 
    CRYPT_USERINFO_PASSWORD         ' Password 

    ' User role-related information 
    CRYPT_USERINFO_CAKEY_CERTSIGN   ' CA cert signing key 
    CRYPT_USERINFO_CAKEY_CRLSIGN    ' CA CRL signing key 
    CRYPT_USERINFO_CAKEY_RTCSSIGN   ' CA RTCS signing key 
    CRYPT_USERINFO_CAKEY_OCSPSIGN   ' CA OCSP signing key 

    ' Used internally for range checking 
    CRYPT_USERINFO_LAST
    CRYPT_ATTRIBUTE_LAST = CRYPT_USERINFO_LAST

    

End Enum

'****************************************************************************
'*                                                                           *
'*                       Attribute Subtypes and Related Values               *
'*                                                                           *
'****************************************************************************

' Flags for the X.509 keyUsage extension 

  Public Const CRYPT_KEYUSAGE_NONE As Long = &H000
  Public Const CRYPT_KEYUSAGE_DIGITALSIGNATURE As Long = &H001
  Public Const CRYPT_KEYUSAGE_NONREPUDIATION As Long = &H002
  Public Const CRYPT_KEYUSAGE_KEYENCIPHERMENT As Long = &H004
  Public Const CRYPT_KEYUSAGE_DATAENCIPHERMENT As Long = &H008
  Public Const CRYPT_KEYUSAGE_KEYAGREEMENT As Long = &H010
  Public Const CRYPT_KEYUSAGE_KEYCERTSIGN As Long = &H020
  Public Const CRYPT_KEYUSAGE_CRLSIGN As Long = &H040
  Public Const CRYPT_KEYUSAGE_ENCIPHERONLY As Long = &H080
  Public Const CRYPT_KEYUSAGE_DECIPHERONLY As Long = &H100
  Public Const CRYPT_KEYUSAGE_LAST As Long = &H200   ' Last possible value 

' X.509 cRLReason and cryptlib cRLExtReason codes 

  Public Const CRYPT_CRLREASON_UNSPECIFIED As Long = 0
  Public Const CRYPT_CRLREASON_KEYCOMPROMISE As Long = 1
  Public Const CRYPT_CRLREASON_CACOMPROMISE As Long = 2
  Public Const CRYPT_CRLREASON_AFFILIATIONCHANGED As Long = 3
  Public Const CRYPT_CRLREASON_SUPERSEDED As Long = 4
  Public Const CRYPT_CRLREASON_CESSATIONOFOPERATION As Long = 5
  Public Const CRYPT_CRLREASON_CERTIFICATEHOLD As Long = 6
  Public Const CRYPT_CRLREASON_REMOVEFROMCRL As Long = 8
  Public Const CRYPT_CRLREASON_PRIVILEGEWITHDRAWN As Long = 9
  Public Const CRYPT_CRLREASON_AACOMPROMISE As Long = 10
  Public Const CRYPT_CRLREASON_LAST As Long = 11
  Public Const CRYPT_CRLREASON_NEVERVALID As Long = 20
  Public Const CRYPT_CRLEXTREASON_LAST  As Long = 21


'  X.509 CRL reason flags.  These identify the same thing as the cRLReason
'  codes but allow for multiple reasons to be specified.  Note that these
'  don't follow the X.509 naming since in that scheme the enumerated types
'  and bitflags have the same names 

  Public Const CRYPT_CRLREASONFLAG_UNUSED As Long = &H001
  Public Const CRYPT_CRLREASONFLAG_KEYCOMPROMISE As Long = &H002
  Public Const CRYPT_CRLREASONFLAG_CACOMPROMISE As Long = &H004
  Public Const CRYPT_CRLREASONFLAG_AFFILIATIONCHANGED As Long = &H008
  Public Const CRYPT_CRLREASONFLAG_SUPERSEDED As Long = &H010
  Public Const CRYPT_CRLREASONFLAG_CESSATIONOFOPERATION As Long = &H020
  Public Const CRYPT_CRLREASONFLAG_CERTIFICATEHOLD As Long = &H040
  Public Const CRYPT_CRLREASONFLAG_LAST As Long = &H080   ' Last poss.value 

' X.509 CRL holdInstruction codes 

  Public Const CRYPT_HOLDINSTRUCTION_NONE As Long = 0
  Public Const CRYPT_HOLDINSTRUCTION_CALLISSUER As Long = 1
  Public Const CRYPT_HOLDINSTRUCTION_REJECT As Long = 2
  Public Const CRYPT_HOLDINSTRUCTION_PICKUPTOKEN As Long = 3
  Public Const CRYPT_HOLDINSTRUCTION_LAST  As Long = 4


' Certificate checking compliance levels 

  Public Const CRYPT_COMPLIANCELEVEL_OBLIVIOUS As Long = 0
  Public Const CRYPT_COMPLIANCELEVEL_REDUCED As Long = 1
  Public Const CRYPT_COMPLIANCELEVEL_STANDARD As Long = 2
  Public Const CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL As Long = 3
  Public Const CRYPT_COMPLIANCELEVEL_PKIX_FULL As Long = 4
  Public Const CRYPT_COMPLIANCELEVEL_LAST  As Long = 5


' Flags for the Netscape netscape-cert-type extension 

  Public Const CRYPT_NS_CERTTYPE_SSLCLIENT As Long = &H001
  Public Const CRYPT_NS_CERTTYPE_SSLSERVER As Long = &H002
  Public Const CRYPT_NS_CERTTYPE_SMIME As Long = &H004
  Public Const CRYPT_NS_CERTTYPE_OBJECTSIGNING As Long = &H008
  Public Const CRYPT_NS_CERTTYPE_RESERVED As Long = &H010
  Public Const CRYPT_NS_CERTTYPE_SSLCA As Long = &H020
  Public Const CRYPT_NS_CERTTYPE_SMIMECA As Long = &H040
  Public Const CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA As Long = &H080
  Public Const CRYPT_NS_CERTTYPE_LAST As Long = &H100   ' Last possible value 

' Flags for the SET certificate-type extension 

  Public Const CRYPT_SET_CERTTYPE_CARD As Long = &H001
  Public Const CRYPT_SET_CERTTYPE_MER As Long = &H002
  Public Const CRYPT_SET_CERTTYPE_PGWY As Long = &H004
  Public Const CRYPT_SET_CERTTYPE_CCA As Long = &H008
  Public Const CRYPT_SET_CERTTYPE_MCA As Long = &H010
  Public Const CRYPT_SET_CERTTYPE_PCA As Long = &H020
  Public Const CRYPT_SET_CERTTYPE_GCA As Long = &H040
  Public Const CRYPT_SET_CERTTYPE_BCA As Long = &H080
  Public Const CRYPT_SET_CERTTYPE_RCA As Long = &H100
  Public Const CRYPT_SET_CERTTYPE_ACQ As Long = &H200
  Public Const CRYPT_SET_CERTTYPE_LAST As Long = &H400   ' Last possible value 

' CMS contentType values 

Public Enum CRYPT_CONTENT_TYPE
 CRYPT_CONTENT_NONE
 CRYPT_CONTENT_DATA
               CRYPT_CONTENT_SIGNEDDATA
               CRYPT_CONTENT_ENVELOPEDDATA
               CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA
               CRYPT_CONTENT_DIGESTEDDATA
               CRYPT_CONTENT_ENCRYPTEDDATA
               CRYPT_CONTENT_COMPRESSEDDATA
               CRYPT_CONTENT_AUTHDATA
               CRYPT_CONTENT_AUTHENVDATA
               CRYPT_CONTENT_TSTINFO
               CRYPT_CONTENT_SPCINDIRECTDATACONTEXT
               CRYPT_CONTENT_RTCSREQUEST
               CRYPT_CONTENT_RTCSRESPONSE
               CRYPT_CONTENT_RTCSRESPONSE_EXT
               CRYPT_CONTENT_MRTD
               CRYPT_CONTENT_LAST
             

End Enum

' ESS securityClassification codes 

  Public Const CRYPT_CLASSIFICATION_UNMARKED As Long = 0
  Public Const CRYPT_CLASSIFICATION_UNCLASSIFIED As Long = 1
  Public Const CRYPT_CLASSIFICATION_RESTRICTED As Long = 2
  Public Const CRYPT_CLASSIFICATION_CONFIDENTIAL As Long = 3
  Public Const CRYPT_CLASSIFICATION_SECRET As Long = 4
  Public Const CRYPT_CLASSIFICATION_TOP_SECRET As Long = 5
  Public Const CRYPT_CLASSIFICATION_LAST As Long = 255


' RTCS certificate status 

  Public Const CRYPT_CERTSTATUS_VALID As Long = 0
  Public Const CRYPT_CERTSTATUS_NOTVALID As Long = 1
  Public Const CRYPT_CERTSTATUS_NONAUTHORITATIVE As Long = 2
  Public Const CRYPT_CERTSTATUS_UNKNOWN  As Long = 3


' OCSP revocation status 

  Public Const CRYPT_OCSPSTATUS_NOTREVOKED As Long = 0
  Public Const CRYPT_OCSPSTATUS_REVOKED As Long = 1
  Public Const CRYPT_OCSPSTATUS_UNKNOWN  As Long = 2


'  The amount of detail to include in signatures when signing certificate
'  objects 

Public Enum CRYPT_SIGNATURELEVEL_TYPE

    CRYPT_SIGNATURELEVEL_NONE       ' Include only signature 
    CRYPT_SIGNATURELEVEL_SIGNERCERT ' Include signer cert 
    CRYPT_SIGNATURELEVEL_ALL        ' Include all relevant info 
    CRYPT_SIGNATURELEVEL_LAST       ' Last possible sig.level type 
    

End Enum

'  The level of integrity protection to apply to enveloped data.  The 
'  default envelope protection for an envelope with keying information 
'  applied is encryption, this can be modified to use MAC-only protection
'  (with no encryption) or hybrid encryption + authentication 

Public Enum CRYPT_INTEGRITY_TYPE

    CRYPT_INTEGRITY_NONE            ' No integrity protection 
    CRYPT_INTEGRITY_MACONLY         ' MAC only, no encryption 
    CRYPT_INTEGRITY_FULL            ' Encryption + ingerity protection 
    

End Enum

'  The certificate export format type, which defines the format in which a
'  certificate object is exported 

Public Enum CRYPT_CERTFORMAT_TYPE

    CRYPT_CERTFORMAT_NONE           ' No certificate format 
    CRYPT_CERTFORMAT_CERTIFICATE    ' DER-encoded certificate 
    CRYPT_CERTFORMAT_CERTCHAIN      ' PKCS #7 certificate chain 
    CRYPT_CERTFORMAT_TEXT_CERTIFICATE   ' base-64 wrapped cert 
    CRYPT_CERTFORMAT_TEXT_CERTCHAIN     ' base-64 wrapped cert chain 
    CRYPT_CERTFORMAT_XML_CERTIFICATE    ' XML wrapped cert 
    CRYPT_CERTFORMAT_XML_CERTCHAIN  ' XML wrapped cert chain 
    CRYPT_CERTFORMAT_LAST           ' Last possible cert.format type 
    

End Enum

' CMP request types 

Public Enum CRYPT_REQUESTTYPE_TYPE

    CRYPT_REQUESTTYPE_NONE          ' No request type 
    CRYPT_REQUESTTYPE_INITIALISATION    ' Initialisation request 
        CRYPT_REQUESTTYPE_INITIALIZATION = CRYPT_REQUESTTYPE_INITIALISATION
    CRYPT_REQUESTTYPE_CERTIFICATE   ' Certification request 
    CRYPT_REQUESTTYPE_KEYUPDATE     ' Key update request 
    CRYPT_REQUESTTYPE_REVOCATION    ' Cert revocation request 
    CRYPT_REQUESTTYPE_PKIBOOT       ' PKIBoot request 
    CRYPT_REQUESTTYPE_LAST          ' Last possible request type 
    

End Enum

' Key ID types 

Public Enum CRYPT_KEYID_TYPE

    CRYPT_KEYID_NONE                ' No key ID type 
    CRYPT_KEYID_NAME                ' Key owner name 
    CRYPT_KEYID_URI                 ' Key owner URI 
        CRYPT_KEYID_EMAIL = CRYPT_KEYID_URI  ' Synonym: owner email addr.
    CRYPT_KEYID_LAST                ' Last possible key ID type 
    

End Enum

' The encryption object types 

Public Enum CRYPT_OBJECT_TYPE

    CRYPT_OBJECT_NONE               ' No object type 
    CRYPT_OBJECT_ENCRYPTED_KEY      ' Conventionally encrypted key 
    CRYPT_OBJECT_PKCENCRYPTED_KEY   ' PKC-encrypted key 
    CRYPT_OBJECT_KEYAGREEMENT       ' Key agreement information 
    CRYPT_OBJECT_SIGNATURE          ' Signature 
    CRYPT_OBJECT_LAST               ' Last possible object type 
    

End Enum

' Object/attribute error type information 

Public Enum CRYPT_ERRTYPE_TYPE

    CRYPT_ERRTYPE_NONE              ' No error information 
    CRYPT_ERRTYPE_ATTR_SIZE         ' Attribute data too small or large 
    CRYPT_ERRTYPE_ATTR_VALUE        ' Attribute value is invalid 
    CRYPT_ERRTYPE_ATTR_ABSENT       ' Required attribute missing 
    CRYPT_ERRTYPE_ATTR_PRESENT      ' Non-allowed attribute present 
    CRYPT_ERRTYPE_CONSTRAINT        ' Cert: Constraint violation in object 
    CRYPT_ERRTYPE_ISSUERCONSTRAINT  ' Cert: Constraint viol.in issuing cert 
    CRYPT_ERRTYPE_LAST              ' Last possible error info type 
    

End Enum

' Cert store management action type 

Public Enum CRYPT_CERTACTION_TYPE

    CRYPT_CERTACTION_NONE           ' No cert management action 
    CRYPT_CERTACTION_CREATE         ' Create cert store 
    CRYPT_CERTACTION_CONNECT        ' Connect to cert store 
    CRYPT_CERTACTION_DISCONNECT     ' Disconnect from cert store 
    CRYPT_CERTACTION_ERROR          ' Error information 
    CRYPT_CERTACTION_ADDUSER        ' Add PKI user 
    CRYPT_CERTACTION_DELETEUSER     ' Delete PKI user 
    CRYPT_CERTACTION_REQUEST_CERT   ' Cert request 
    CRYPT_CERTACTION_REQUEST_RENEWAL ' Cert renewal request 
    CRYPT_CERTACTION_REQUEST_REVOCATION ' Cert revocation request 
    CRYPT_CERTACTION_CERT_CREATION  ' Cert creation 
    CRYPT_CERTACTION_CERT_CREATION_COMPLETE ' Confirmation of cert creation 
    CRYPT_CERTACTION_CERT_CREATION_DROP     ' Cancellation of cert creation 
    CRYPT_CERTACTION_CERT_CREATION_REVERSE  ' Cancel of creation w.revocation 
    CRYPT_CERTACTION_RESTART_CLEANUP  ' Delete reqs after restart 
    CRYPT_CERTACTION_RESTART_REVOKE_CERT  ' Complete revocation after restart 
    CRYPT_CERTACTION_ISSUE_CERT     ' Cert issue 
    CRYPT_CERTACTION_ISSUE_CRL      ' CRL issue 
    CRYPT_CERTACTION_REVOKE_CERT    ' Cert revocation 
    CRYPT_CERTACTION_EXPIRE_CERT    ' Cert expiry 
    CRYPT_CERTACTION_CLEANUP        ' Clean up on restart 
    CRYPT_CERTACTION_LAST           ' Last possible cert store log action 
    

End Enum

'  SSL/TLS protocol options.  CRYPT_SSLOPTION_MINVER_SSLV3 is the same as 
'  CRYPT_SSLOPTION_NONE since this is the default 

  Public Const CRYPT_SSLOPTION_NONE As Long = &H00
  Public Const CRYPT_SSLOPTION_MINVER_SSLV3 As Long = &H00    ' Min.protocol version 
  Public Const CRYPT_SSLOPTION_MINVER_TLS10 As Long = &H01
  Public Const CRYPT_SSLOPTION_MINVER_TLS11 As Long = &H02
  Public Const CRYPT_SSLOPTION_MINVER_TLS12 As Long = &H03
  Public Const CRYPT_SSLOPTION_SUITEB_128 As Long = &H04    ' SuiteB security levels 
  Public Const CRYPT_SSLOPTION_SUITEB_256 As Long = &H08

'****************************************************************************
'*                                                                           *
'*                               General Constants                           *
'*                                                                           *
'****************************************************************************

' The maximum user key size - 2048 bits 

  Public Const CRYPT_MAX_KEYSIZE As Long = 256

' The maximum IV size - 256 bits 

  Public Const CRYPT_MAX_IVSIZE As Long = 32

'  The maximum public-key component size - 4096 bits, and maximum component
'  size for ECCs - 576 bits (to handle the P521 curve) 

  Public Const CRYPT_MAX_PKCSIZE As Long = 512
  Public Const CRYPT_MAX_PKCSIZE_ECC As Long = 72

'  The maximum hash size - 512 bits.  Before 3.4 this was 256 bits, in the 
'  3.4 release it was increased to 512 bits to accommodate SHA-3 

  Public Const CRYPT_MAX_HASHSIZE As Long = 64

' The maximum size of a text string (e.g.key owner name) 

  Public Const CRYPT_MAX_TEXTSIZE As Long = 64

'  A magic value indicating that the default setting for this parameter
'  should be used.  The parentheses are to catch potential erroneous use 
'  in an expression 

  Public Const CRYPT_USE_DEFAULT As Long = -100

' A magic value for unused parameters 

  Public Const CRYPT_UNUSED As Long = -101

'  Cursor positioning codes for certificate/CRL extensions.  The parentheses 
'  are to catch potential erroneous use in an expression 

  Public Const CRYPT_CURSOR_FIRST As Long = -200
  Public Const CRYPT_CURSOR_PREVIOUS As Long = -201
  Public Const CRYPT_CURSOR_NEXT As Long = -202
  Public Const CRYPT_CURSOR_LAST As Long = -203

'  The type of information polling to perform to get random seed 
'  information.  These values have to be negative because they're used
'  as magic length values for cryptAddRandom().  The parentheses are to 
'  catch potential erroneous use in an expression 

  Public Const CRYPT_RANDOM_FASTPOLL As Long = -300
  Public Const CRYPT_RANDOM_SLOWPOLL As Long = -301

' Whether the PKC key is a public or private key 

  Public Const CRYPT_KEYTYPE_PRIVATE As Long = 0
  Public Const CRYPT_KEYTYPE_PUBLIC As Long = 1

' Keyset open options 

Public Enum CRYPT_KEYOPT_TYPE

    CRYPT_KEYOPT_NONE               ' No options 
    CRYPT_KEYOPT_READONLY           ' Open keyset in read-only mode 
    CRYPT_KEYOPT_CREATE             ' Create a new keyset 
    CRYPT_KEYOPT_LAST               ' Last possible key option type 
    

End Enum

' The various cryptlib objects - these are just integer handles 

REM  CRYPT_CERTIFICATE = Long
REM  CRYPT_CONTEXT = Long
REM  CRYPT_DEVICE = Long
REM  CRYPT_ENVELOPE = Long
REM  CRYPT_KEYSET = Long
REM  CRYPT_SESSION = Long
REM  CRYPT_USER = Long

'  Sometimes we don't know the exact type of a cryptlib object, so we use a
'  generic handle type to identify it 

REM  CRYPT_HANDLE = Long

'****************************************************************************
'*                                                                           *
'*                           Encryption Data Structures                      *
'*                                                                           *
'****************************************************************************

' Results returned from the capability query 

Public Type CRYPT_QUERY_INFO 
    ' Algorithm information 
    algoName(CRYPT_MAX_TEXTSIZE-1) As Byte' Algorithm name 
    blockSize As Long                  ' Block size of the algorithm 
    minKeySize As Long                 ' Minimum key size in bytes 
    keySize As Long                    ' Recommended key size in bytes 
    maxKeySize As Long                 ' Maximum key size in bytes 
    

End Type

'  Results returned from the encoded object query.  These provide
'  information on the objects created by cryptExportKey()/
'  cryptCreateSignature() 

Public Type CRYPT_OBJECT_INFO 
    ' The object type 
    objectType As CRYPT_OBJECT_TYPE

    ' The encryption algorithm and mode 
    cryptAlgo As CRYPT_ALGO_TYPE
    cryptMode As CRYPT_MODE_TYPE

    ' The hash algorithm for Signature objects 
    hashAlgo As CRYPT_ALGO_TYPE

    ' The salt for derived keys 
    salt(CRYPT_MAX_HASHSIZE-1) As Byte
    saltSize As Long
    

End Type

'  Key information for the public-key encryption algorithms.  These fields
'  are not accessed directly, but can be manipulated with the init/set/
'  destroyComponents() macros 

Public Type CRYPT_PKCINFO_RSA 
    ' Status information 
    isPublicKey As Long            ' Whether this is a public or private key 

    ' Public components 
    n(CRYPT_MAX_PKCSIZE-1) As Byte   ' Modulus 
    nLen As Long                   ' Length of modulus in bits 
    e(CRYPT_MAX_PKCSIZE-1) As Byte   ' Public exponent 
    eLen As Long                   ' Length of public exponent in bits 

    ' Private components 
    d(CRYPT_MAX_PKCSIZE-1) As Byte   ' Private exponent 
    dLen As Long                   ' Length of private exponent in bits 
    p(CRYPT_MAX_PKCSIZE-1) As Byte   ' Prime factor 1 
    pLen As Long                   ' Length of prime factor 1 in bits 
    q(CRYPT_MAX_PKCSIZE-1) As Byte   ' Prime factor 2 
    qLen As Long                   ' Length of prime factor 2 in bits 
    u(CRYPT_MAX_PKCSIZE-1) As Byte   ' Mult.inverse of q, mod p 
    uLen As Long                   ' Length of private exponent in bits 
    e1(CRYPT_MAX_PKCSIZE-1) As Byte  ' Private exponent 1 (PKCS) 
    e1Len As Long                  ' Length of private exponent in bits 
    e2(CRYPT_MAX_PKCSIZE-1) As Byte  ' Private exponent 2 (PKCS) 
    e2Len As Long                  ' Length of private exponent in bits 
    

End Type

Public Type CRYPT_PKCINFO_DLP 
    ' Status information 
    isPublicKey As Long            ' Whether this is a public or private key 

    ' Public components 
    p(CRYPT_MAX_PKCSIZE-1) As Byte   ' Prime modulus 
    pLen As Long                   ' Length of prime modulus in bits 
    q(CRYPT_MAX_PKCSIZE-1) As Byte   ' Prime divisor 
    qLen As Long                   ' Length of prime divisor in bits 
    g(CRYPT_MAX_PKCSIZE-1) As Byte   ' h^( ( p - 1 ) / q ) mod p 
    gLen As Long                   ' Length of g in bits 
    y(CRYPT_MAX_PKCSIZE-1) As Byte   ' Public random integer 
    yLen As Long                   ' Length of public integer in bits 

    ' Private components 
    x(CRYPT_MAX_PKCSIZE-1) As Byte   ' Private random integer 
    xLen As Long                   ' Length of private integer in bits 
    

End Type

Public Enum CRYPT_ECCCURVE_TYPE

'      Named ECC curves.  Since these need to be mapped to all manner of
'      protocol- and mechanism-specific identifiers, when updating this list 
'      grep for occurrences of CRYPT_ECCCURVE_P256 (the most common one) and
'      check whether any related mapping tables need to be updated 
    CRYPT_ECCCURVE_NONE         ' No ECC curve type 
    CRYPT_ECCCURVE_P192         ' NIST P192/X9.62 P192r1/SECG p192r1 curve 
    CRYPT_ECCCURVE_P224         ' NIST P224/X9.62 P224r1/SECG p224r1 curve 
    CRYPT_ECCCURVE_P256         ' NIST P256/X9.62 P256v1/SECG p256r1 curve 
    CRYPT_ECCCURVE_P384         ' NIST P384, SECG p384r1 curve 
    CRYPT_ECCCURVE_P521         ' NIST P521, SECG p521r1 
    CRYPT_ECCCURVE_LAST         ' Last valid ECC curve type 
    

End Enum

Public Type CRYPT_PKCINFO_ECC 
    ' Status information 
    isPublicKey As Long            ' Whether this is a public or private key 

'      Curve domain parameters.  Either the curveType or the explicit domain
'      parameters must be provided 
    curveType As CRYPT_ECCCURVE_TYPE  ' Named curve 
    p(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Prime defining Fq 
    pLen As Long                   ' Length of prime in bits 
    a(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Element in Fq defining curve 
    aLen As Long                   ' Length of element a in bits 
    b(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Element in Fq defining curve 
    bLen As Long                   ' Length of element b in bits 
    gx(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Element in Fq defining point 
    gxLen As Long                  ' Length of element gx in bits 
    gy(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Element in Fq defining point 
    gyLen As Long                  ' Length of element gy in bits 
    n(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Order of point 
    nLen As Long                   ' Length of order in bits 
    h(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Optional cofactor 
    hLen As Long                   ' Length of cofactor in bits 

    ' Public components 
    qx(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Point Q on the curve 
    qxLen As Long                  ' Length of point xq in bits 
    qy(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Point Q on the curve 
    qyLen As Long                  ' Length of point xy in bits 

    ' Private components 
    d(CRYPT_MAX_PKCSIZE_ECC-1) As Byte' Private random integer 
    dLen As Long                   ' Length of integer in bits 
    

End Type

'  Macros to initialise and destroy the structure that stores the components
'  of a public key 

' C-macro not translated to Visual Basic code: 
'   #define cryptInitComponents( componentInfo, componentKeyType ) 
'    { memset( ( componentInfo ), 0, sizeof( *componentInfo ) ); 
'      ( componentInfo )->isPublicKey = ( ( componentKeyType ) ? 1 : 0 ); }
'

' C-macro not translated to Visual Basic code: 
'   #define cryptDestroyComponents( componentInfo ) 
'    memset( ( componentInfo ), 0, sizeof( *componentInfo ) )
'

' Macros to set a component of a public key 

' C-macro not translated to Visual Basic code: 
'   #define cryptSetComponent( destination, source, length ) 
'    { memcpy( ( destination ), ( source ), ( ( length ) + 7 ) >> 3 ); 
'      ( destination##Len ) = length; }
'

'****************************************************************************
'*                                                                           *
'*                               Status Codes                                *
'*                                                                           *
'****************************************************************************

' No error in function call 

  Public Const CRYPT_OK As Long = 0       ' No error 

'  Error in parameters passed to function.  The parentheses are to catch 
'  potential erroneous use in an expression 

  Public Const CRYPT_ERROR_PARAM1 As Long = -1  ' Bad argument, parameter 1 
  Public Const CRYPT_ERROR_PARAM2 As Long = -2  ' Bad argument, parameter 2 
  Public Const CRYPT_ERROR_PARAM3 As Long = -3  ' Bad argument, parameter 3 
  Public Const CRYPT_ERROR_PARAM4 As Long = -4  ' Bad argument, parameter 4 
  Public Const CRYPT_ERROR_PARAM5 As Long = -5  ' Bad argument, parameter 5 
  Public Const CRYPT_ERROR_PARAM6 As Long = -6  ' Bad argument, parameter 6 
  Public Const CRYPT_ERROR_PARAM7 As Long = -7  ' Bad argument, parameter 7 

' Errors due to insufficient resources 

  Public Const CRYPT_ERROR_MEMORY As Long = -10 ' Out of memory 
  Public Const CRYPT_ERROR_NOTINITED As Long = -11 ' Data has not been initialised 
  Public Const CRYPT_ERROR_INITED As Long = -12 ' Data has already been init'd 
  Public Const CRYPT_ERROR_NOSECURE As Long = -13 ' Opn.not avail.at requested sec.level 
  Public Const CRYPT_ERROR_RANDOM As Long = -14 ' No reliable random data available 
  Public Const CRYPT_ERROR_FAILED As Long = -15 ' Operation failed 
  Public Const CRYPT_ERROR_INTERNAL As Long = -16 ' Internal consistency check failed 

' Security violations 

  Public Const CRYPT_ERROR_NOTAVAIL As Long = -20 ' This type of opn.not available 
  Public Const CRYPT_ERROR_PERMISSION As Long = -21 ' No permiss.to perform this operation 
  Public Const CRYPT_ERROR_WRONGKEY As Long = -22 ' Incorrect key used to decrypt data 
  Public Const CRYPT_ERROR_INCOMPLETE As Long = -23 ' Operation incomplete/still in progress 
  Public Const CRYPT_ERROR_COMPLETE As Long = -24 ' Operation complete/can't continue 
  Public Const CRYPT_ERROR_TIMEOUT As Long = -25 ' Operation timed out before completion 
  Public Const CRYPT_ERROR_INVALID As Long = -26 ' Invalid/inconsistent information 
  Public Const CRYPT_ERROR_SIGNALLED As Long = -27 ' Resource destroyed by extnl.event 

' High-level function errors 

  Public Const CRYPT_ERROR_OVERFLOW As Long = -30 ' Resources/space exhausted 
  Public Const CRYPT_ERROR_UNDERFLOW As Long = -31 ' Not enough data available 
  Public Const CRYPT_ERROR_BADDATA As Long = -32 ' Bad/unrecognised data format 
  Public Const CRYPT_ERROR_SIGNATURE As Long = -33 ' Signature/integrity check failed 

' Data access function errors 

  Public Const CRYPT_ERROR_OPEN As Long = -40 ' Cannot open object 
  Public Const CRYPT_ERROR_READ As Long = -41 ' Cannot read item from object 
  Public Const CRYPT_ERROR_WRITE As Long = -42 ' Cannot write item to object 
  Public Const CRYPT_ERROR_NOTFOUND As Long = -43 ' Requested item not found in object 
  Public Const CRYPT_ERROR_DUPLICATE As Long = -44 ' Item already present in object 

' Data enveloping errors 

  Public Const CRYPT_ENVELOPE_RESOURCE As Long = -50 ' Need resource to proceed 

' Macros to examine return values 

' C-macro not translated to Visual Basic code: 
'   #define cryptStatusError( status )  ( ( status ) < CRYPT_OK )
'
' C-macro not translated to Visual Basic code: 
'   #define cryptStatusOK( status )     ( ( status ) == CRYPT_OK )
'

'****************************************************************************
'*                                                                           *
'*                                   General Functions                       *
'*                                                                           *
'****************************************************************************

' The following is necessary to stop C++ name mangling 


' Initialise and shut down cryptlib 

 
Public Declare Function cryptInit Lib "CL32.DLL" () As Long


Public Declare Function cryptEnd Lib "CL32.DLL" () As Long



' Query cryptlibs capabilities 

Public Declare Function cryptQueryCapability Lib "CL32.DLL" ( ByVal cryptAlgo As CRYPT_ALGO_TYPE, _
 ByRef cryptQueryInfo As CRYPT_QUERY_INFO) As Long


' Create and destroy an encryption context 

Public Declare Function cryptCreateContext Lib "CL32.DLL" ( ByRef cryptContext As Long, _
 ByVal cryptUser As Long, _
 ByVal cryptAlgo As CRYPT_ALGO_TYPE) As Long

Public Declare Function cryptDestroyContext Lib "CL32.DLL" ( ByVal cryptContext As Long) As Long


' Generic "destroy an object" function 

Public Declare Function cryptDestroyObject Lib "CL32.DLL" ( ByVal cryptObject As Long) As Long


' Generate a key into a context 

Public Declare Function cryptGenerateKey Lib "CL32.DLL" ( ByVal cryptContext As Long) As Long


' Encrypt/decrypt/hash a block of memory 

' ***Warning: function 'cryptEncrypt' will replace the String 'buffer'
Public Declare Function cryptEncrypt Lib "CL32.DLL" ( ByVal cryptContext As Long, _
 ByVal buffer As String, _
 ByVal length As Long) As Long

' ***Warning: function 'cryptDecrypt' will replace the String 'buffer'
Public Declare Function cryptDecrypt Lib "CL32.DLL" ( ByVal cryptContext As Long, _
 ByVal buffer As String, _
 ByVal length As Long) As Long


' Get/set/delete attribute functions 

Public Declare Function cryptSetAttribute Lib "CL32.DLL" ( ByVal cryptHandle As Long, _
 ByVal attributeType As CRYPT_ATTRIBUTE_TYPE, _
 ByVal value As Long) As Long

Public Declare Function cryptSetAttributeString Lib "CL32.DLL" ( ByVal cryptHandle As Long, _
 ByVal attributeType As CRYPT_ATTRIBUTE_TYPE, _
 ByVal value As String, _
 ByVal valueLength As Long) As Long

Public Declare Function cryptGetAttribute Lib "CL32.DLL" ( ByVal cryptHandle As Long, _
 ByVal attributeType As CRYPT_ATTRIBUTE_TYPE, _
 ByRef value As Long) As Long

' ***Warning: function 'cryptGetAttributeString' will modify the String 'value'
Public Declare Function cryptGetAttributeString Lib "CL32.DLL" ( ByVal cryptHandle As Long, _
 ByVal attributeType As CRYPT_ATTRIBUTE_TYPE, _
 ByVal value As String, _
 ByRef valueLength As Long) As Long

Public Declare Function cryptDeleteAttribute Lib "CL32.DLL" ( ByVal cryptHandle As Long, _
 ByVal attributeType As CRYPT_ATTRIBUTE_TYPE) As Long


'  Oddball functions: Add random data to the pool, query an encoded signature
'  or key data.  These are due to be replaced once a suitable alternative can
'  be found 

Public Declare Function cryptAddRandom Lib "CL32.DLL" ( ByVal randomData As String, _
 ByVal randomDataLength As Long) As Long

Public Declare Function cryptQueryObject Lib "CL32.DLL" ( ByVal objectData As String, _
 ByVal objectDataLength As Long, _
 ByRef cryptObjectInfo As CRYPT_OBJECT_INFO) As Long


'****************************************************************************
'*                                                                           *
'*                           Mid-level Encryption Functions                  *
'*                                                                           *
'****************************************************************************

' Export and import an encrypted session key 

' ***Warning: function 'cryptExportKey' will modify the String 'encryptedKey'
Public Declare Function cryptExportKey Lib "CL32.DLL" ( ByVal encryptedKey As String, _
 ByVal encryptedKeyMaxLength As Long, _
 ByRef encryptedKeyLength As Long, _
 ByVal exportKey As Long, _
 ByVal sessionKeyContext As Long) As Long

' ***Warning: function 'cryptExportKeyEx' will modify the String 'encryptedKey'
Public Declare Function cryptExportKeyEx Lib "CL32.DLL" ( ByVal encryptedKey As String, _
 ByVal encryptedKeyMaxLength As Long, _
 ByRef encryptedKeyLength As Long, _
 ByVal formatType As CRYPT_FORMAT_TYPE, _
 ByVal exportKey As Long, _
 ByVal sessionKeyContext As Long) As Long

Public Declare Function cryptImportKey Lib "CL32.DLL" ( ByVal encryptedKey As String, _
 ByVal encryptedKeyLength As Long, _
 ByVal importKey As Long, _
 ByVal sessionKeyContext As Long) As Long

Public Declare Function cryptImportKeyEx Lib "CL32.DLL" ( ByVal encryptedKey As String, _
 ByVal encryptedKeyLength As Long, _
 ByVal importKey As Long, _
 ByVal sessionKeyContext As Long, _
 ByRef returnedContext As Long) As Long


' Create and check a digital signature 

' ***Warning: function 'cryptCreateSignature' will modify the String 'signature'
Public Declare Function cryptCreateSignature Lib "CL32.DLL" ( ByVal signature As String, _
 ByVal signatureMaxLength As Long, _
 ByRef signatureLength As Long, _
 ByVal signContext As Long, _
 ByVal hashContext As Long) As Long

' ***Warning: function 'cryptCreateSignatureEx' will modify the String 'signature'
Public Declare Function cryptCreateSignatureEx Lib "CL32.DLL" ( ByVal signature As String, _
 ByVal signatureMaxLength As Long, _
 ByRef signatureLength As Long, _
 ByVal formatType As CRYPT_FORMAT_TYPE, _
 ByVal signContext As Long, _
 ByVal hashContext As Long, _
 ByVal extraData As Long) As Long

Public Declare Function cryptCheckSignature Lib "CL32.DLL" ( ByVal signature As String, _
 ByVal signatureLength As Long, _
 ByVal sigCheckKey As Long, _
 ByVal hashContext As Long) As Long

Public Declare Function cryptCheckSignatureEx Lib "CL32.DLL" ( ByVal signature As String, _
 ByVal signatureLength As Long, _
 ByVal sigCheckKey As Long, _
 ByVal hashContext As Long, _
 ByRef extraData As Long) As Long


'****************************************************************************
'*                                                                           *
'*                                   Keyset Functions                        *
'*                                                                           *
'****************************************************************************

' Open and close a keyset 

Public Declare Function cryptKeysetOpen Lib "CL32.DLL" ( ByRef keyset As Long, _
 ByVal cryptUser As Long, _
 ByVal keysetType As CRYPT_KEYSET_TYPE, _
 ByVal name As String, _
 ByVal options As CRYPT_KEYOPT_TYPE) As Long

Public Declare Function cryptKeysetClose Lib "CL32.DLL" ( ByVal keyset As Long) As Long


' Get a key from a keyset or device 

Public Declare Function cryptGetPublicKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByRef cryptContext As Long, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String) As Long

Public Declare Function cryptGetPrivateKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByRef cryptContext As Long, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String, _
 ByVal password As String) As Long

Public Declare Function cryptGetKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByRef cryptContext As Long, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String, _
 ByVal password As String) As Long


' Add/delete a key to/from a keyset or device 

Public Declare Function cryptAddPublicKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByVal certificate As Long) As Long

Public Declare Function cryptAddPrivateKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByVal cryptKey As Long, _
 ByVal password As String) As Long

Public Declare Function cryptDeleteKey Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String) As Long


'****************************************************************************
'*                                                                           *
'*                               Certificate Functions                       *
'*                                                                           *
'****************************************************************************

' Create/destroy a certificate 

Public Declare Function cryptCreateCert Lib "CL32.DLL" ( ByRef certificate As Long, _
 ByVal cryptUser As Long, _
 ByVal certType As CRYPT_CERTTYPE_TYPE) As Long

Public Declare Function cryptDestroyCert Lib "CL32.DLL" ( ByVal certificate As Long) As Long


'  Get/add/delete certificate extensions.  These are direct data insertion
'  functions whose use is discouraged, so they fix the string at char *
'  rather than C_STR 

' ***Warning: function 'cryptGetCertExtension' will modify the String 'extension'
Public Declare Function cryptGetCertExtension Lib "CL32.DLL" ( ByVal certificate As Long, _
 ByVal oid As String, _
 ByRef criticalFlag As Long, _
 ByVal extension As String, _
 ByVal extensionMaxLength As Long, _
 ByRef extensionLength As Long) As Long

Public Declare Function cryptAddCertExtension Lib "CL32.DLL" ( ByVal certificate As Long, _
 ByVal oid As String, _
 ByVal criticalFlag As Long, _
 ByVal extension As String, _
 ByVal extensionLength As Long) As Long

Public Declare Function cryptDeleteCertExtension Lib "CL32.DLL" ( ByVal certificate As Long, _
 ByVal oid As String) As Long


' Sign/sig.check a certificate/certification request 

Public Declare Function cryptSignCert Lib "CL32.DLL" ( ByVal certificate As Long, _
 ByVal signContext As Long) As Long

Public Declare Function cryptCheckCert Lib "CL32.DLL" ( ByVal certificate As Long, _
 ByVal sigCheckKey As Long) As Long


' Import/export a certificate/certification request 

Public Declare Function cryptImportCert Lib "CL32.DLL" ( ByVal certObject As String, _
 ByVal certObjectLength As Long, _
 ByVal cryptUser As Long, _
 ByRef certificate As Long) As Long

' ***Warning: function 'cryptExportCert' will modify the String 'certObject'
Public Declare Function cryptExportCert Lib "CL32.DLL" ( ByVal certObject As String, _
 ByVal certObjectMaxLength As Long, _
 ByRef certObjectLength As Long, _
 ByVal certFormatType As CRYPT_CERTFORMAT_TYPE, _
 ByVal certificate As Long) As Long


' CA management functions 

Public Declare Function cryptCAAddItem Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByVal certificate As Long) As Long

Public Declare Function cryptCAGetItem Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByRef certificate As Long, _
 ByVal certType As CRYPT_CERTTYPE_TYPE, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String) As Long

Public Declare Function cryptCADeleteItem Lib "CL32.DLL" ( ByVal keyset As Long, _
 ByVal certType As CRYPT_CERTTYPE_TYPE, _
 ByVal keyIDtype As CRYPT_KEYID_TYPE, _
 ByVal keyID As String) As Long

Public Declare Function cryptCACertManagement Lib "CL32.DLL" ( ByRef certificate As Long, _
 ByVal action As CRYPT_CERTACTION_TYPE, _
 ByVal keyset As Long, _
 ByVal caKey As Long, _
 ByVal certRequest As Long) As Long


'****************************************************************************
'*                                                                           *
'*                           Envelope and Session Functions                  *
'*                                                                           *
'****************************************************************************

' Create/destroy an envelope 

Public Declare Function cryptCreateEnvelope Lib "CL32.DLL" ( ByRef envelope As Long, _
 ByVal cryptUser As Long, _
 ByVal formatType As CRYPT_FORMAT_TYPE) As Long

Public Declare Function cryptDestroyEnvelope Lib "CL32.DLL" ( ByVal envelope As Long) As Long


' Create/destroy a session 

Public Declare Function cryptCreateSession Lib "CL32.DLL" ( ByRef session As Long, _
 ByVal cryptUser As Long, _
 ByVal formatType As CRYPT_SESSION_TYPE) As Long

Public Declare Function cryptDestroySession Lib "CL32.DLL" ( ByVal session As Long) As Long


' Add/remove data to/from and envelope or session 

Public Declare Function cryptPushData Lib "CL32.DLL" ( ByVal envelope As Long, _
 ByVal buffer As String, _
 ByVal length As Long, _
 ByRef bytesCopied As Long) As Long

Public Declare Function cryptFlushData Lib "CL32.DLL" ( ByVal envelope As Long) As Long

' ***Warning: function 'cryptPopData' will modify the String 'buffer'
Public Declare Function cryptPopData Lib "CL32.DLL" ( ByVal envelope As Long, _
 ByVal buffer As String, _
 ByVal length As Long, _
 ByRef bytesCopied As Long) As Long


'****************************************************************************
'*                                                                           *
'*                               Device Functions                            *
'*                                                                           *
'****************************************************************************

' Open and close a device 

Public Declare Function cryptDeviceOpen Lib "CL32.DLL" ( ByRef device As Long, _
 ByVal cryptUser As Long, _
 ByVal deviceType As CRYPT_DEVICE_TYPE, _
 ByVal name As String) As Long

Public Declare Function cryptDeviceClose Lib "CL32.DLL" ( ByVal device As Long) As Long


' Query a devices capabilities 

Public Declare Function cryptDeviceQueryCapability Lib "CL32.DLL" ( ByVal device As Long, _
 ByVal cryptAlgo As CRYPT_ALGO_TYPE, _
 ByRef cryptQueryInfo As CRYPT_QUERY_INFO) As Long


' Create an encryption context via the device 

Public Declare Function cryptDeviceCreateContext Lib "CL32.DLL" ( ByVal device As Long, _
 ByRef cryptContext As Long, _
 ByVal cryptAlgo As CRYPT_ALGO_TYPE) As Long


'****************************************************************************
'*                                                                           *
'*                           User Management Functions                       *
'*                                                                           *
'****************************************************************************

' Log on and off (create/destroy a user object) 

Public Declare Function cryptLogin Lib "CL32.DLL" ( ByRef user As Long, _
 ByVal name As String, _
 ByVal password As String) As Long

Public Declare Function cryptLogout Lib "CL32.DLL" ( ByVal user As Long) As Long


'****************************************************************************
'*                                                                           *
'*                           User Interface Functions                        *
'*                                                                           *
'****************************************************************************



'*****************************************************************************
'*                                                                           *
'*                    End of Visual Basic Functions                          *
'*                                                                           *
'****************************************************************************}

