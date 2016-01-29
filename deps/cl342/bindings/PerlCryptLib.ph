
# *****************************************************************************
# *                                                                           *
# *                        cryptlib External API Interface                    *
# *                       Copyright Peter Gutmann 1997-2015                   *
# *                                                                           *
# *                 adapted for Perl Version 5.x  by Alvaro Livraghi          *
# *****************************************************************************
#
#
# ----------------------------------------------------------------------------
#
# This file has been created automatically by a perl script from the file:
#
# "cryptlib.h" dated Mon Dec  7 02:33:30 2015, filesize = 98078.
#
# Please check twice that the file matches the version of cryptlib.h
# in your cryptlib source! If this is not the right version, try to download an
# update from CPAN web site. If the filesize or file creation date do not match,
# then please do not complain about problems.
#
# Published by Alvaro Livraghi, 
# mailto: perlcryptlib@gmail.com if you find errors in this file.
#
# -----------------------------------------------------------------------------
#

	sub CRYPTLIB_VERSION { 3430 }

#  Additional defines for compilers that provide extended function and 
#  function-parameter checking 



#****************************************************************************
#*                                                                           *
#*                           Algorithm and Object Types                      *
#*                                                                           *
#****************************************************************************

# Algorithm and mode types 

##### BEGIN ENUM CRYPT_ALGO_TYPE 	# Algorithms
	# No encryption
	# No encryption
	sub CRYPT_ALGO_NONE { 0 }

	# Conventional encryption
	# DES
	sub CRYPT_ALGO_DES { 1 }
	# Triple DES
	sub CRYPT_ALGO_3DES { 2 }
	# IDEA (only used for PGP 2.x)
	sub CRYPT_ALGO_IDEA { 3 }
	# CAST-128 (only used for OpenPGP)
	sub CRYPT_ALGO_CAST { 4 }
	# RC2 (disabled by default, used for PKCS #12)
	sub CRYPT_ALGO_RC2 { 5 }
	# RC4 (insecure, deprecated)
	sub CRYPT_ALGO_RC4 { 6 }
	# Formerly RC5
	sub CRYPT_ALGO_RESERVED1 { 7 }
	# AES
	sub CRYPT_ALGO_AES { 8 }
	# Formerly Blowfish
	sub CRYPT_ALGO_RESERVED2 { 9 }

	# Public-key encryption
	# Diffie-Hellman
	sub CRYPT_ALGO_DH { 100 }
	# RSA
	sub CRYPT_ALGO_RSA { 101 }
	# DSA
	sub CRYPT_ALGO_DSA { 102 }
	# ElGamal
	sub CRYPT_ALGO_ELGAMAL { 103 }
	# Formerly KEA
	sub CRYPT_ALGO_RESERVED3 { 104 }
	# ECDSA
	sub CRYPT_ALGO_ECDSA { 105 }
	# ECDH
	sub CRYPT_ALGO_ECDH { 106 }

	# Hash algorithms
	# Formerly MD2
	sub CRYPT_ALGO_RESERVED4 { 200 }
	# Formerly MD4
	sub CRYPT_ALGO_RESERVED5 { 201 }
	# MD5 (only used for TLS 1.0/1.1)
	sub CRYPT_ALGO_MD5 { 202 }
	# SHA/SHA1
	sub CRYPT_ALGO_SHA1 { 203 }
	# Formerly RIPE-MD 160
	sub CRYPT_ALGO_RESERVED6 { 204 }
	# SHA-256
	sub CRYPT_ALGO_SHA2 { 205 }
	# Alternate name
	sub CRYPT_ALGO_SHA256 { 205 }
	# Future SHA-nextgen standard
	sub CRYPT_ALGO_SHAng { 206 }

	# MAC's
	# Formerly HMAC-MD5
	sub CRYPT_ALGO_RESREVED_7 { 300 }
	# HMAC-SHA
	sub CRYPT_ALGO_HMAC_SHA1 { 301 }
	# Formerly HMAC-RIPEMD-160
	sub CRYPT_ALGO_RESERVED8 { 302 }
	# HMAC-SHA2
	sub CRYPT_ALGO_HMAC_SHA2 { 303 }
	# HMAC-future-SHA-nextgen
	sub CRYPT_ALGO_HMAC_SHAng { 304 }


	# Vendors may want to use their own algorithms that aren't part of the
	# general cryptlib suite.  The following values are for vendor-defined
	# algorithms, and can be used just like the named algorithm types (it's
	# up to the vendor to keep track of what _VENDOR1 actually corresponds
	# to)

	# Last possible crypt algo value
	sub CRYPT_ALGO_LAST { 305 }

	# In order that we can scan through a range of algorithms with
	# cryptQueryCapability(), we define the following boundary points for
	# each algorithm class

	sub CRYPT_ALGO_FIRST_CONVENTIONAL { 1 }

	sub CRYPT_ALGO_LAST_CONVENTIONAL { 99 }

	sub CRYPT_ALGO_FIRST_PKC { 100 }

	sub CRYPT_ALGO_LAST_PKC { 199 }

	sub CRYPT_ALGO_FIRST_HASH { 200 }

	sub CRYPT_ALGO_LAST_HASH { 299 }

	sub CRYPT_ALGO_FIRST_MAC { 300 }

	sub CRYPT_ALGO_LAST_MAC { 399 }


##### END ENUM CRYPT_ALGO_TYPE

##### BEGIN ENUM CRYPT_MODE_TYPE
	# Block cipher modes
	# No encryption mode
	sub CRYPT_MODE_NONE { 0 }
	# ECB
	sub CRYPT_MODE_ECB { 1 }
	# CBC
	sub CRYPT_MODE_CBC { 2 }
	# CFB
	sub CRYPT_MODE_CFB { 3 }
	# GCM
	sub CRYPT_MODE_GCM { 4 }
	# Last possible crypt mode value
	sub CRYPT_MODE_LAST { 5 }


##### END ENUM CRYPT_MODE_TYPE


# Keyset subtypes 

##### BEGIN ENUM CRYPT_KEYSET_TYPE
	# Keyset types
	# No keyset type
	sub CRYPT_KEYSET_NONE { 0 }
	# Generic flat file keyset
	sub CRYPT_KEYSET_FILE { 1 }
	# Web page containing cert/CRL
	sub CRYPT_KEYSET_HTTP { 2 }
	# LDAP directory service
	sub CRYPT_KEYSET_LDAP { 3 }
	# Generic ODBC interface
	sub CRYPT_KEYSET_ODBC { 4 }
	# Generic RDBMS interface
	sub CRYPT_KEYSET_DATABASE { 5 }
	# ODBC certificate store
	sub CRYPT_KEYSET_ODBC_STORE { 6 }
	# Database certificate store
	sub CRYPT_KEYSET_DATABASE_STORE { 7 }
	# Last possible keyset type
	sub CRYPT_KEYSET_LAST { 8 }



##### END ENUM CRYPT_KEYSET_TYPE

# Device subtypes 

##### BEGIN ENUM CRYPT_DEVICE_TYPE
	# Crypto device types
	# No crypto device
	sub CRYPT_DEVICE_NONE { 0 }
	# Fortezza card - Placeholder only
	sub CRYPT_DEVICE_FORTEZZA { 1 }
	# PKCS #11 crypto token
	sub CRYPT_DEVICE_PKCS11 { 2 }
	# Microsoft CryptoAPI
	sub CRYPT_DEVICE_CRYPTOAPI { 3 }
	# Generic crypo HW plugin
	sub CRYPT_DEVICE_HARDWARE { 4 }
	# Last possible crypto device type
	sub CRYPT_DEVICE_LAST { 5 }


##### END ENUM CRYPT_DEVICE_TYPE

# Certificate subtypes 

##### BEGIN ENUM CRYPT_CERTTYPE_TYPE
	# Certificate object types
	# No certificate type
	sub CRYPT_CERTTYPE_NONE { 0 }
	# Certificate
	sub CRYPT_CERTTYPE_CERTIFICATE { 1 }
	# Attribute certificate
	sub CRYPT_CERTTYPE_ATTRIBUTE_CERT { 2 }
	# PKCS #7 certificate chain
	sub CRYPT_CERTTYPE_CERTCHAIN { 3 }
	# PKCS #10 certification request
	sub CRYPT_CERTTYPE_CERTREQUEST { 4 }
	# CRMF certification request
	sub CRYPT_CERTTYPE_REQUEST_CERT { 5 }
	# CRMF revocation request
	sub CRYPT_CERTTYPE_REQUEST_REVOCATION { 6 }
	# CRL
	sub CRYPT_CERTTYPE_CRL { 7 }
	# CMS attributes
	sub CRYPT_CERTTYPE_CMS_ATTRIBUTES { 8 }
	# RTCS request
	sub CRYPT_CERTTYPE_RTCS_REQUEST { 9 }
	# RTCS response
	sub CRYPT_CERTTYPE_RTCS_RESPONSE { 10 }
	# OCSP request
	sub CRYPT_CERTTYPE_OCSP_REQUEST { 11 }
	# OCSP response
	sub CRYPT_CERTTYPE_OCSP_RESPONSE { 12 }
	# PKI user information
	sub CRYPT_CERTTYPE_PKIUSER { 13 }
	# Last possible cert.type
	sub CRYPT_CERTTYPE_LAST { 14 }


##### END ENUM CRYPT_CERTTYPE_TYPE

# Envelope/data format subtypes 

##### BEGIN ENUM CRYPT_FORMAT_TYPE

	# No format type
	sub CRYPT_FORMAT_NONE { 0 }
	# Deenv, auto-determine type
	sub CRYPT_FORMAT_AUTO { 1 }
	# cryptlib native format
	sub CRYPT_FORMAT_CRYPTLIB { 2 }
	# PKCS #7 / CMS / S/MIME fmt.
	sub CRYPT_FORMAT_CMS { 3 }

	sub CRYPT_FORMAT_PKCS7 { 3 }
	# As CMS with MSG-style behaviour
	sub CRYPT_FORMAT_SMIME { 4 }
	# PGP format
	sub CRYPT_FORMAT_PGP { 5 }
	# Last possible format type
	sub CRYPT_FORMAT_LAST { 6 }


##### END ENUM CRYPT_FORMAT_TYPE

# Session subtypes 

##### BEGIN ENUM CRYPT_SESSION_TYPE

	# No session type
	sub CRYPT_SESSION_NONE { 0 }
	# SSH
	sub CRYPT_SESSION_SSH { 1 }
	# SSH server
	sub CRYPT_SESSION_SSH_SERVER { 2 }
	# SSL/TLS
	sub CRYPT_SESSION_SSL { 3 }

	sub CRYPT_SESSION_TLS { 3 }
	# SSL/TLS server
	sub CRYPT_SESSION_SSL_SERVER { 4 }

	sub CRYPT_SESSION_TLS_SERVER { 4 }
	# RTCS
	sub CRYPT_SESSION_RTCS { 5 }
	# RTCS server
	sub CRYPT_SESSION_RTCS_SERVER { 6 }
	# OCSP
	sub CRYPT_SESSION_OCSP { 7 }
	# OCSP server
	sub CRYPT_SESSION_OCSP_SERVER { 8 }
	# TSP
	sub CRYPT_SESSION_TSP { 9 }
	# TSP server
	sub CRYPT_SESSION_TSP_SERVER { 10 }
	# CMP
	sub CRYPT_SESSION_CMP { 11 }
	# CMP server
	sub CRYPT_SESSION_CMP_SERVER { 12 }
	# SCEP
	sub CRYPT_SESSION_SCEP { 13 }
	# SCEP server
	sub CRYPT_SESSION_SCEP_SERVER { 14 }
	# HTTP cert store interface
	sub CRYPT_SESSION_CERTSTORE_SERVER { 15 }
	# Last possible session type
	sub CRYPT_SESSION_LAST { 16 }


##### END ENUM CRYPT_SESSION_TYPE

# User subtypes 

##### BEGIN ENUM CRYPT_USER_TYPE

	# No user type
	sub CRYPT_USER_NONE { 0 }
	# Normal user
	sub CRYPT_USER_NORMAL { 1 }
	# Security officer
	sub CRYPT_USER_SO { 2 }
	# CA user
	sub CRYPT_USER_CA { 3 }
	# Last possible user type
	sub CRYPT_USER_LAST { 4 }


##### END ENUM CRYPT_USER_TYPE

#****************************************************************************
#*                                                                           *
#*                               Attribute Types                             *
#*                                                                           *
#****************************************************************************

#  Attribute types.  These are arranged in the following order:
#
#   PROPERTY    - Object property
#   ATTRIBUTE   - Generic attributes
#   OPTION      - Global or object-specific config.option
#   CTXINFO     - Context-specific attribute
#   CERTINFO    - Certificate-specific attribute
#   KEYINFO     - Keyset-specific attribute
#   DEVINFO     - Device-specific attribute
#   ENVINFO     - Envelope-specific attribute
#   SESSINFO    - Session-specific attribute
#   USERINFO    - User-specific attribute 

##### BEGIN ENUM CRYPT_ATTRIBUTE_TYPE 
	# Non-value
	sub CRYPT_ATTRIBUTE_NONE { 0 }

	# Used internally

	sub CRYPT_PROPERTY_FIRST { 1 }

	# *******************
	# Object attributes
	# *******************

	# Object properties
	# Owned+non-forwardcount+locked
	sub CRYPT_PROPERTY_HIGHSECURITY { 2 }
	# Object owner
	sub CRYPT_PROPERTY_OWNER { 3 }
	# No.of times object can be forwarded
	sub CRYPT_PROPERTY_FORWARDCOUNT { 4 }
	# Whether properties can be chged/read
	sub CRYPT_PROPERTY_LOCKED { 5 }
	# Usage count before object expires
	sub CRYPT_PROPERTY_USAGECOUNT { 6 }
	# Whether key is nonexp.from context
	sub CRYPT_PROPERTY_NONEXPORTABLE { 7 }

	# Used internally

	sub CRYPT_PROPERTY_LAST { 8 }
	sub CRYPT_GENERIC_FIRST { 9 }

	# Extended error information
	# Type of last error
	sub CRYPT_ATTRIBUTE_ERRORTYPE { 10 }
	# Locus of last error
	sub CRYPT_ATTRIBUTE_ERRORLOCUS { 11 }
	# Detailed error description
	sub CRYPT_ATTRIBUTE_ERRORMESSAGE { 12 }

	# Generic information
	# Cursor mgt: Group in attribute list
	sub CRYPT_ATTRIBUTE_CURRENT_GROUP { 13 }
	# Cursor mgt: Entry in attribute list
	sub CRYPT_ATTRIBUTE_CURRENT { 14 }
	# Cursor mgt: Instance in attribute list
	sub CRYPT_ATTRIBUTE_CURRENT_INSTANCE { 15 }
	# Internal data buffer size
	sub CRYPT_ATTRIBUTE_BUFFERSIZE { 16 }

	# Used internally

	sub CRYPT_GENERIC_LAST { 17 }
	sub CRYPT_OPTION_FIRST { 100 }

	# **************************
	# Configuration attributes
	# **************************

	# cryptlib information (read-only)
	# Text description
	sub CRYPT_OPTION_INFO_DESCRIPTION { 101 }
	# Copyright notice
	sub CRYPT_OPTION_INFO_COPYRIGHT { 102 }
	# Major release version
	sub CRYPT_OPTION_INFO_MAJORVERSION { 103 }
	# Minor release version
	sub CRYPT_OPTION_INFO_MINORVERSION { 104 }
	# Release stepping
	sub CRYPT_OPTION_INFO_STEPPING { 105 }

	# Encryption options
	# Encryption algorithm
	sub CRYPT_OPTION_ENCR_ALGO { 106 }
	# Hash algorithm
	sub CRYPT_OPTION_ENCR_HASH { 107 }
	# MAC algorithm
	sub CRYPT_OPTION_ENCR_MAC { 108 }

	# PKC options
	# Public-key encryption algorithm
	sub CRYPT_OPTION_PKC_ALGO { 109 }
	# Public-key encryption key size
	sub CRYPT_OPTION_PKC_KEYSIZE { 110 }

	# Signature options
	# Signature algorithm
	sub CRYPT_OPTION_SIG_ALGO { 111 }
	# Signature keysize
	sub CRYPT_OPTION_SIG_KEYSIZE { 112 }

	# Keying options
	# Key processing algorithm
	sub CRYPT_OPTION_KEYING_ALGO { 113 }
	# Key processing iterations
	sub CRYPT_OPTION_KEYING_ITERATIONS { 114 }

	# Certificate options
	# Whether to sign unrecog.attrs
	sub CRYPT_OPTION_CERT_SIGNUNRECOGNISEDATTRIBUTES { 115 }
	# Certificate validity period
	sub CRYPT_OPTION_CERT_VALIDITY { 116 }
	# CRL update interval
	sub CRYPT_OPTION_CERT_UPDATEINTERVAL { 117 }
	# PKIX compliance level for cert chks.
	sub CRYPT_OPTION_CERT_COMPLIANCELEVEL { 118 }
	# Whether explicit policy req'd for certs
	sub CRYPT_OPTION_CERT_REQUIREPOLICY { 119 }

	# CMS/SMIME options
	# Add default CMS attributes
	sub CRYPT_OPTION_CMS_DEFAULTATTRIBUTES { 120 }

	sub CRYPT_OPTION_SMIME_DEFAULTATTRIBUTES { 120 }

	# LDAP keyset options
	# Object class
	sub CRYPT_OPTION_KEYS_LDAP_OBJECTCLASS { 121 }
	# Object type to fetch
	sub CRYPT_OPTION_KEYS_LDAP_OBJECTTYPE { 122 }
	# Query filter
	sub CRYPT_OPTION_KEYS_LDAP_FILTER { 123 }
	# CA certificate attribute name
	sub CRYPT_OPTION_KEYS_LDAP_CACERTNAME { 124 }
	# Certificate attribute name
	sub CRYPT_OPTION_KEYS_LDAP_CERTNAME { 125 }
	# CRL attribute name
	sub CRYPT_OPTION_KEYS_LDAP_CRLNAME { 126 }
	# Email attribute name
	sub CRYPT_OPTION_KEYS_LDAP_EMAILNAME { 127 }

	# Crypto device options
	# Name of first PKCS #11 driver
	sub CRYPT_OPTION_DEVICE_PKCS11_DVR01 { 128 }
	# Name of second PKCS #11 driver
	sub CRYPT_OPTION_DEVICE_PKCS11_DVR02 { 129 }
	# Name of third PKCS #11 driver
	sub CRYPT_OPTION_DEVICE_PKCS11_DVR03 { 130 }
	# Name of fourth PKCS #11 driver
	sub CRYPT_OPTION_DEVICE_PKCS11_DVR04 { 131 }
	# Name of fifth PKCS #11 driver
	sub CRYPT_OPTION_DEVICE_PKCS11_DVR05 { 132 }
	# Use only hardware mechanisms
	sub CRYPT_OPTION_DEVICE_PKCS11_HARDWAREONLY { 133 }

	# Network access options
	# Socks server name
	sub CRYPT_OPTION_NET_SOCKS_SERVER { 134 }
	# Socks user name
	sub CRYPT_OPTION_NET_SOCKS_USERNAME { 135 }
	# Web proxy server
	sub CRYPT_OPTION_NET_HTTP_PROXY { 136 }
	# Timeout for network connection setup
	sub CRYPT_OPTION_NET_CONNECTTIMEOUT { 137 }
	# Timeout for network reads
	sub CRYPT_OPTION_NET_READTIMEOUT { 138 }
	# Timeout for network writes
	sub CRYPT_OPTION_NET_WRITETIMEOUT { 139 }

	# Miscellaneous options
	# Whether to init cryptlib async'ly
	sub CRYPT_OPTION_MISC_ASYNCINIT { 140 }
	# Protect against side-channel attacks
	sub CRYPT_OPTION_MISC_SIDECHANNELPROTECTION { 141 }

	# cryptlib state information
	# Whether in-mem.opts match on-disk ones
	sub CRYPT_OPTION_CONFIGCHANGED { 142 }
	# Whether self-test was completed and OK
	sub CRYPT_OPTION_SELFTESTOK { 143 }

	# Used internally

	sub CRYPT_OPTION_LAST { 144 }
	sub CRYPT_CTXINFO_FIRST { 1000 }

	# ********************
	# Context attributes
	# ********************

	# Algorithm and mode information
	# Algorithm
	sub CRYPT_CTXINFO_ALGO { 1001 }
	# Mode
	sub CRYPT_CTXINFO_MODE { 1002 }
	# Algorithm name
	sub CRYPT_CTXINFO_NAME_ALGO { 1003 }
	# Mode name
	sub CRYPT_CTXINFO_NAME_MODE { 1004 }
	# Key size in bytes
	sub CRYPT_CTXINFO_KEYSIZE { 1005 }
	# Block size
	sub CRYPT_CTXINFO_BLOCKSIZE { 1006 }
	# IV size
	sub CRYPT_CTXINFO_IVSIZE { 1007 }
	# Key processing algorithm
	sub CRYPT_CTXINFO_KEYING_ALGO { 1008 }
	# Key processing iterations
	sub CRYPT_CTXINFO_KEYING_ITERATIONS { 1009 }
	# Key processing salt
	sub CRYPT_CTXINFO_KEYING_SALT { 1010 }
	# Value used to derive key
	sub CRYPT_CTXINFO_KEYING_VALUE { 1011 }

	# State information
	# Key
	sub CRYPT_CTXINFO_KEY { 1012 }
	# Public-key components
	sub CRYPT_CTXINFO_KEY_COMPONENTS { 1013 }
	# IV
	sub CRYPT_CTXINFO_IV { 1014 }
	# Hash value
	sub CRYPT_CTXINFO_HASHVALUE { 1015 }

	# Misc.information
	# Label for private/secret key
	sub CRYPT_CTXINFO_LABEL { 1016 }
	# Obj.is backed by device or keyset
	sub CRYPT_CTXINFO_PERSISTENT { 1017 }

	# Used internally

	sub CRYPT_CTXINFO_LAST { 1018 }
	sub CRYPT_CERTINFO_FIRST { 2000 }

	# ************************
	# Certificate attributes
	# ************************

	# Because there are so many cert attributes, we break them down into
	# blocks to minimise the number of values that change if a new one is
	# added halfway through

	# Pseudo-information on a cert object or meta-information which is used
	# to control the way that a cert object is processed
	# Cert is self-signed
	sub CRYPT_CERTINFO_SELFSIGNED { 2001 }
	# Cert is signed and immutable
	sub CRYPT_CERTINFO_IMMUTABLE { 2002 }
	# Cert is a magic just-works cert
	sub CRYPT_CERTINFO_XYZZY { 2003 }
	# Certificate object type
	sub CRYPT_CERTINFO_CERTTYPE { 2004 }
	# Certificate fingerprints
	sub CRYPT_CERTINFO_FINGERPRINT_SHA1 { 2005 }

	sub CRYPT_CERTINFO_FINGERPRINT_SHA2 { 2006 }

	sub CRYPT_CERTINFO_FINGERPRINT_SHAng { 2007 }
	# Cursor mgt: Rel.pos in chain/CRL/OCSP
	sub CRYPT_CERTINFO_CURRENT_CERTIFICATE { 2008 }
	# Usage that cert is trusted for
	sub CRYPT_CERTINFO_TRUSTED_USAGE { 2009 }
	# Whether cert is implicitly trusted
	sub CRYPT_CERTINFO_TRUSTED_IMPLICIT { 2010 }
	# Amount of detail to include in sigs.
	sub CRYPT_CERTINFO_SIGNATURELEVEL { 2011 }

	# General certificate object information
	# Cert.format version
	sub CRYPT_CERTINFO_VERSION { 2012 }
	# Serial number
	sub CRYPT_CERTINFO_SERIALNUMBER { 2013 }
	# Public key
	sub CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO { 2014 }
	# User certificate
	sub CRYPT_CERTINFO_CERTIFICATE { 2015 }

	sub CRYPT_CERTINFO_USERCERTIFICATE { 2015 }
	# CA certificate
	sub CRYPT_CERTINFO_CACERTIFICATE { 2016 }
	# Issuer DN
	sub CRYPT_CERTINFO_ISSUERNAME { 2017 }
	# Cert valid-from time
	sub CRYPT_CERTINFO_VALIDFROM { 2018 }
	# Cert valid-to time
	sub CRYPT_CERTINFO_VALIDTO { 2019 }
	# Subject DN
	sub CRYPT_CERTINFO_SUBJECTNAME { 2020 }
	# Issuer unique ID
	sub CRYPT_CERTINFO_ISSUERUNIQUEID { 2021 }
	# Subject unique ID
	sub CRYPT_CERTINFO_SUBJECTUNIQUEID { 2022 }
	# Cert.request (DN + public key)
	sub CRYPT_CERTINFO_CERTREQUEST { 2023 }
	# CRL/OCSP current-update time
	sub CRYPT_CERTINFO_THISUPDATE { 2024 }
	# CRL/OCSP next-update time
	sub CRYPT_CERTINFO_NEXTUPDATE { 2025 }
	# CRL/OCSP cert-revocation time
	sub CRYPT_CERTINFO_REVOCATIONDATE { 2026 }
	# OCSP revocation status
	sub CRYPT_CERTINFO_REVOCATIONSTATUS { 2027 }
	# RTCS certificate status
	sub CRYPT_CERTINFO_CERTSTATUS { 2028 }
	# Currently selected DN in string form
	sub CRYPT_CERTINFO_DN { 2029 }
	# PKI user ID
	sub CRYPT_CERTINFO_PKIUSER_ID { 2030 }
	# PKI user issue password
	sub CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD { 2031 }
	# PKI user revocation password
	sub CRYPT_CERTINFO_PKIUSER_REVPASSWORD { 2032 }
	# PKI user is an RA
	sub CRYPT_CERTINFO_PKIUSER_RA { 2033 }

	# X.520 Distinguished Name components.  This is a composite field, the
	# DN to be manipulated is selected through the addition of a
	# pseudocomponent, and then one of the following is used to access the
	# DN components directly
	# countryName
	sub CRYPT_CERTINFO_COUNTRYNAME { 2100 }
	# stateOrProvinceName
	sub CRYPT_CERTINFO_STATEORPROVINCENAME { 2101 }
	# localityName
	sub CRYPT_CERTINFO_LOCALITYNAME { 2102 }
	# organizationName
	sub CRYPT_CERTINFO_ORGANIZATIONNAME { 2103 }

	sub CRYPT_CERTINFO_ORGANISATIONNAME { 2103 }
	# organizationalUnitName
	sub CRYPT_CERTINFO_ORGANIZATIONALUNITNAME { 2104 }

	sub CRYPT_CERTINFO_ORGANISATIONALUNITNAME { 2104 }
	# commonName
	sub CRYPT_CERTINFO_COMMONNAME { 2105 }

	# X.509 General Name components.  These are handled in the same way as
	# the DN composite field, with the current GeneralName being selected by
	# a pseudo-component after which the individual components can be
	# modified through one of the following
	# otherName.typeID
	sub CRYPT_CERTINFO_OTHERNAME_TYPEID { 2106 }
	# otherName.value
	sub CRYPT_CERTINFO_OTHERNAME_VALUE { 2107 }
	# rfc822Name
	sub CRYPT_CERTINFO_RFC822NAME { 2108 }

	sub CRYPT_CERTINFO_EMAIL { 2108 }
	# dNSName
	sub CRYPT_CERTINFO_DNSNAME { 2109 }
	# directoryName
	sub CRYPT_CERTINFO_DIRECTORYNAME { 2110 }
	# ediPartyName.nameAssigner
	sub CRYPT_CERTINFO_EDIPARTYNAME_NAMEASSIGNER { 2111 }
	# ediPartyName.partyName
	sub CRYPT_CERTINFO_EDIPARTYNAME_PARTYNAME { 2112 }
	# uniformResourceIdentifier
	sub CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER { 2113 }

	sub CRYPT_CERTINFO_URL { 2113 }
	# iPAddress
	sub CRYPT_CERTINFO_IPADDRESS { 2114 }
	# registeredID
	sub CRYPT_CERTINFO_REGISTEREDID { 2115 }

	# X.509 certificate extensions.  Although it would be nicer to use names
	# that match the extensions more closely (e.g.
	# CRYPT_CERTINFO_BASICCONSTRAINTS_PATHLENCONSTRAINT), these exceed the
	# 32-character ANSI minimum length for unique names, and get really
	# hairy once you get into the weird policy constraints extensions whose
	# names wrap around the screen about three times.

	# The following values are defined in OID order, this isn't absolutely
	# necessary but saves an extra layer of processing when encoding them

	# 1 2 840 113549 1 9 7 challengePassword.  This is here even though it's
	# a CMS attribute because SCEP stuffs it into PKCS #10 requests

	sub CRYPT_CERTINFO_CHALLENGEPASSWORD { 2200 }

	# 1 3 6 1 4 1 3029 3 1 4 cRLExtReason

	sub CRYPT_CERTINFO_CRLEXTREASON { 2201 }

	# 1 3 6 1 4 1 3029 3 1 5 keyFeatures

	sub CRYPT_CERTINFO_KEYFEATURES { 2202 }

	# 1 3 6 1 5 5 7 1 1 authorityInfoAccess

	sub CRYPT_CERTINFO_AUTHORITYINFOACCESS { 2203 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_AUTHORITYINFO_RTCS { 2204 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_AUTHORITYINFO_OCSP { 2205 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_AUTHORITYINFO_CAISSUERS { 2206 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_AUTHORITYINFO_CERTSTORE { 2207 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_AUTHORITYINFO_CRLS { 2208 }

	# 1 3 6 1 5 5 7 1 2 biometricInfo

	sub CRYPT_CERTINFO_BIOMETRICINFO { 2209 }
	# biometricData.typeOfData
	sub CRYPT_CERTINFO_BIOMETRICINFO_TYPE { 2210 }
	# biometricData.hashAlgorithm
	sub CRYPT_CERTINFO_BIOMETRICINFO_HASHALGO { 2211 }
	# biometricData.dataHash
	sub CRYPT_CERTINFO_BIOMETRICINFO_HASH { 2212 }
	# biometricData.sourceDataUri
	sub CRYPT_CERTINFO_BIOMETRICINFO_URL { 2213 }

	# 1 3 6 1 5 5 7 1 3 qcStatements

	sub CRYPT_CERTINFO_QCSTATEMENT { 2214 }

	sub CRYPT_CERTINFO_QCSTATEMENT_SEMANTICS { 2215 }
	# qcStatement.statementInfo.semanticsIdentifier

	sub CRYPT_CERTINFO_QCSTATEMENT_REGISTRATIONAUTHORITY { 2216 }
	# qcStatement.statementInfo.nameRegistrationAuthorities

	# 1 3 6 1 5 5 7 1 7 ipAddrBlocks

	sub CRYPT_CERTINFO_IPADDRESSBLOCKS { 2217 }
	# addressFamily
	sub CRYPT_CERTINFO_IPADDRESSBLOCKS_ADDRESSFAMILY { 2218 }
	# CRYPT_CERTINFO_IPADDRESSBLOCKS_INHERIT, // ipAddress.inherit
	# ipAddress.addressPrefix
	sub CRYPT_CERTINFO_IPADDRESSBLOCKS_PREFIX { 2219 }
	# ipAddress.addressRangeMin
	sub CRYPT_CERTINFO_IPADDRESSBLOCKS_MIN { 2220 }
	# ipAddress.addressRangeMax
	sub CRYPT_CERTINFO_IPADDRESSBLOCKS_MAX { 2221 }

	# 1 3 6 1 5 5 7 1 8 autonomousSysIds

	sub CRYPT_CERTINFO_AUTONOMOUSSYSIDS { 2222 }
	# CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_INHERIT,// asNum.inherit
	# asNum.id
	sub CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_ID { 2223 }
	# asNum.min
	sub CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MIN { 2224 }
	# asNum.max
	sub CRYPT_CERTINFO_AUTONOMOUSSYSIDS_ASNUM_MAX { 2225 }

	# 1 3 6 1 5 5 7 48 1 2 ocspNonce
	# nonce
	sub CRYPT_CERTINFO_OCSP_NONCE { 2226 }

	# 1 3 6 1 5 5 7 48 1 4 ocspAcceptableResponses

	sub CRYPT_CERTINFO_OCSP_RESPONSE { 2227 }
	# OCSP standard response
	sub CRYPT_CERTINFO_OCSP_RESPONSE_OCSP { 2228 }

	# 1 3 6 1 5 5 7 48 1 5 ocspNoCheck

	sub CRYPT_CERTINFO_OCSP_NOCHECK { 2229 }

	# 1 3 6 1 5 5 7 48 1 6 ocspArchiveCutoff

	sub CRYPT_CERTINFO_OCSP_ARCHIVECUTOFF { 2230 }

	# 1 3 6 1 5 5 7 48 1 11 subjectInfoAccess

	sub CRYPT_CERTINFO_SUBJECTINFOACCESS { 2231 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_SUBJECTINFO_TIMESTAMPING { 2232 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_SUBJECTINFO_CAREPOSITORY { 2233 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECTREPOSITORY { 2234 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_SUBJECTINFO_RPKIMANIFEST { 2235 }
	# accessDescription.accessLocation
	sub CRYPT_CERTINFO_SUBJECTINFO_SIGNEDOBJECT { 2236 }

	# 1 3 36 8 3 1 siggDateOfCertGen

	sub CRYPT_CERTINFO_SIGG_DATEOFCERTGEN { 2237 }

	# 1 3 36 8 3 2 siggProcuration

	sub CRYPT_CERTINFO_SIGG_PROCURATION { 2238 }
	# country
	sub CRYPT_CERTINFO_SIGG_PROCURE_COUNTRY { 2239 }
	# typeOfSubstitution
	sub CRYPT_CERTINFO_SIGG_PROCURE_TYPEOFSUBSTITUTION { 2240 }
	# signingFor.thirdPerson
	sub CRYPT_CERTINFO_SIGG_PROCURE_SIGNINGFOR { 2241 }

	# 1 3 36 8 3 3 siggAdmissions

	sub CRYPT_CERTINFO_SIGG_ADMISSIONS { 2242 }
	# authority
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_AUTHORITY { 2243 }
	# namingAuth.iD
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHID { 2244 }
	# namingAuth.uRL
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHURL { 2245 }
	# namingAuth.text
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_NAMINGAUTHTEXT { 2246 }
	# professionItem
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONITEM { 2247 }
	# professionOID
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_PROFESSIONOID { 2248 }
	# registrationNumber
	sub CRYPT_CERTINFO_SIGG_ADMISSIONS_REGISTRATIONNUMBER { 2249 }

	# 1 3 36 8 3 4 siggMonetaryLimit

	sub CRYPT_CERTINFO_SIGG_MONETARYLIMIT { 2250 }
	# currency
	sub CRYPT_CERTINFO_SIGG_MONETARY_CURRENCY { 2251 }
	# amount
	sub CRYPT_CERTINFO_SIGG_MONETARY_AMOUNT { 2252 }
	# exponent
	sub CRYPT_CERTINFO_SIGG_MONETARY_EXPONENT { 2253 }

	# 1 3 36 8 3 5 siggDeclarationOfMajority

	sub CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY { 2254 }
	# fullAgeAtCountry
	sub CRYPT_CERTINFO_SIGG_DECLARATIONOFMAJORITY_COUNTRY { 2255 }

	# 1 3 36 8 3 8 siggRestriction

	sub CRYPT_CERTINFO_SIGG_RESTRICTION { 2256 }

	# 1 3 36 8 3 13 siggCertHash

	sub CRYPT_CERTINFO_SIGG_CERTHASH { 2257 }

	# 1 3 36 8 3 15 siggAdditionalInformation

	sub CRYPT_CERTINFO_SIGG_ADDITIONALINFORMATION { 2258 }

	# 1 3 101 1 4 1 strongExtranet

	sub CRYPT_CERTINFO_STRONGEXTRANET { 2259 }
	# sxNetIDList.sxNetID.zone
	sub CRYPT_CERTINFO_STRONGEXTRANET_ZONE { 2260 }
	# sxNetIDList.sxNetID.id
	sub CRYPT_CERTINFO_STRONGEXTRANET_ID { 2261 }

	# 2 5 29 9 subjectDirectoryAttributes

	sub CRYPT_CERTINFO_SUBJECTDIRECTORYATTRIBUTES { 2262 }
	# attribute.type
	sub CRYPT_CERTINFO_SUBJECTDIR_TYPE { 2263 }
	# attribute.values
	sub CRYPT_CERTINFO_SUBJECTDIR_VALUES { 2264 }

	# 2 5 29 14 subjectKeyIdentifier

	sub CRYPT_CERTINFO_SUBJECTKEYIDENTIFIER { 2265 }

	# 2 5 29 15 keyUsage

	sub CRYPT_CERTINFO_KEYUSAGE { 2266 }

	# 2 5 29 16 privateKeyUsagePeriod

	sub CRYPT_CERTINFO_PRIVATEKEYUSAGEPERIOD { 2267 }
	# notBefore
	sub CRYPT_CERTINFO_PRIVATEKEY_NOTBEFORE { 2268 }
	# notAfter
	sub CRYPT_CERTINFO_PRIVATEKEY_NOTAFTER { 2269 }

	# 2 5 29 17 subjectAltName

	sub CRYPT_CERTINFO_SUBJECTALTNAME { 2270 }

	# 2 5 29 18 issuerAltName

	sub CRYPT_CERTINFO_ISSUERALTNAME { 2271 }

	# 2 5 29 19 basicConstraints

	sub CRYPT_CERTINFO_BASICCONSTRAINTS { 2272 }
	# cA
	sub CRYPT_CERTINFO_CA { 2273 }

	sub CRYPT_CERTINFO_AUTHORITY { 2273 }
	# pathLenConstraint
	sub CRYPT_CERTINFO_PATHLENCONSTRAINT { 2274 }

	# 2 5 29 20 cRLNumber

	sub CRYPT_CERTINFO_CRLNUMBER { 2275 }

	# 2 5 29 21 cRLReason

	sub CRYPT_CERTINFO_CRLREASON { 2276 }

	# 2 5 29 23 holdInstructionCode

	sub CRYPT_CERTINFO_HOLDINSTRUCTIONCODE { 2277 }

	# 2 5 29 24 invalidityDate

	sub CRYPT_CERTINFO_INVALIDITYDATE { 2278 }

	# 2 5 29 27 deltaCRLIndicator

	sub CRYPT_CERTINFO_DELTACRLINDICATOR { 2279 }

	# 2 5 29 28 issuingDistributionPoint

	sub CRYPT_CERTINFO_ISSUINGDISTRIBUTIONPOINT { 2280 }
	# distributionPointName.fullName
	sub CRYPT_CERTINFO_ISSUINGDIST_FULLNAME { 2281 }
	# onlyContainsUserCerts
	sub CRYPT_CERTINFO_ISSUINGDIST_USERCERTSONLY { 2282 }
	# onlyContainsCACerts
	sub CRYPT_CERTINFO_ISSUINGDIST_CACERTSONLY { 2283 }
	# onlySomeReasons
	sub CRYPT_CERTINFO_ISSUINGDIST_SOMEREASONSONLY { 2284 }
	# indirectCRL
	sub CRYPT_CERTINFO_ISSUINGDIST_INDIRECTCRL { 2285 }

	# 2 5 29 29 certificateIssuer

	sub CRYPT_CERTINFO_CERTIFICATEISSUER { 2286 }

	# 2 5 29 30 nameConstraints

	sub CRYPT_CERTINFO_NAMECONSTRAINTS { 2287 }
	# permittedSubtrees
	sub CRYPT_CERTINFO_PERMITTEDSUBTREES { 2288 }
	# excludedSubtrees
	sub CRYPT_CERTINFO_EXCLUDEDSUBTREES { 2289 }

	# 2 5 29 31 cRLDistributionPoint

	sub CRYPT_CERTINFO_CRLDISTRIBUTIONPOINT { 2290 }
	# distributionPointName.fullName
	sub CRYPT_CERTINFO_CRLDIST_FULLNAME { 2291 }
	# reasons
	sub CRYPT_CERTINFO_CRLDIST_REASONS { 2292 }
	# cRLIssuer
	sub CRYPT_CERTINFO_CRLDIST_CRLISSUER { 2293 }

	# 2 5 29 32 certificatePolicies

	sub CRYPT_CERTINFO_CERTIFICATEPOLICIES { 2294 }
	# policyInformation.policyIdentifier
	sub CRYPT_CERTINFO_CERTPOLICYID { 2295 }

	sub CRYPT_CERTINFO_CERTPOLICY_CPSURI { 2296 }
	# policyInformation.policyQualifiers.qualifier.cPSuri

	sub CRYPT_CERTINFO_CERTPOLICY_ORGANIZATION { 2297 }
	# policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.organization

	sub CRYPT_CERTINFO_CERTPOLICY_NOTICENUMBERS { 2298 }
	# policyInformation.policyQualifiers.qualifier.userNotice.noticeRef.noticeNumbers

	sub CRYPT_CERTINFO_CERTPOLICY_EXPLICITTEXT { 2299 }
	# policyInformation.policyQualifiers.qualifier.userNotice.explicitText

	# 2 5 29 33 policyMappings

	sub CRYPT_CERTINFO_POLICYMAPPINGS { 2300 }
	# policyMappings.issuerDomainPolicy
	sub CRYPT_CERTINFO_ISSUERDOMAINPOLICY { 2301 }
	# policyMappings.subjectDomainPolicy
	sub CRYPT_CERTINFO_SUBJECTDOMAINPOLICY { 2302 }

	# 2 5 29 35 authorityKeyIdentifier

	sub CRYPT_CERTINFO_AUTHORITYKEYIDENTIFIER { 2303 }
	# keyIdentifier
	sub CRYPT_CERTINFO_AUTHORITY_KEYIDENTIFIER { 2304 }
	# authorityCertIssuer
	sub CRYPT_CERTINFO_AUTHORITY_CERTISSUER { 2305 }
	# authorityCertSerialNumber
	sub CRYPT_CERTINFO_AUTHORITY_CERTSERIALNUMBER { 2306 }

	# 2 5 29 36 policyConstraints

	sub CRYPT_CERTINFO_POLICYCONSTRAINTS { 2307 }
	# policyConstraints.requireExplicitPolicy
	sub CRYPT_CERTINFO_REQUIREEXPLICITPOLICY { 2308 }
	# policyConstraints.inhibitPolicyMapping
	sub CRYPT_CERTINFO_INHIBITPOLICYMAPPING { 2309 }

	# 2 5 29 37 extKeyUsage

	sub CRYPT_CERTINFO_EXTKEYUSAGE { 2310 }
	# individualCodeSigning
	sub CRYPT_CERTINFO_EXTKEY_MS_INDIVIDUALCODESIGNING { 2311 }
	# commercialCodeSigning
	sub CRYPT_CERTINFO_EXTKEY_MS_COMMERCIALCODESIGNING { 2312 }
	# certTrustListSigning
	sub CRYPT_CERTINFO_EXTKEY_MS_CERTTRUSTLISTSIGNING { 2313 }
	# timeStampSigning
	sub CRYPT_CERTINFO_EXTKEY_MS_TIMESTAMPSIGNING { 2314 }
	# serverGatedCrypto
	sub CRYPT_CERTINFO_EXTKEY_MS_SERVERGATEDCRYPTO { 2315 }
	# encrypedFileSystem
	sub CRYPT_CERTINFO_EXTKEY_MS_ENCRYPTEDFILESYSTEM { 2316 }
	# serverAuth
	sub CRYPT_CERTINFO_EXTKEY_SERVERAUTH { 2317 }
	# clientAuth
	sub CRYPT_CERTINFO_EXTKEY_CLIENTAUTH { 2318 }
	# codeSigning
	sub CRYPT_CERTINFO_EXTKEY_CODESIGNING { 2319 }
	# emailProtection
	sub CRYPT_CERTINFO_EXTKEY_EMAILPROTECTION { 2320 }
	# ipsecEndSystem
	sub CRYPT_CERTINFO_EXTKEY_IPSECENDSYSTEM { 2321 }
	# ipsecTunnel
	sub CRYPT_CERTINFO_EXTKEY_IPSECTUNNEL { 2322 }
	# ipsecUser
	sub CRYPT_CERTINFO_EXTKEY_IPSECUSER { 2323 }
	# timeStamping
	sub CRYPT_CERTINFO_EXTKEY_TIMESTAMPING { 2324 }
	# ocspSigning
	sub CRYPT_CERTINFO_EXTKEY_OCSPSIGNING { 2325 }
	# directoryService
	sub CRYPT_CERTINFO_EXTKEY_DIRECTORYSERVICE { 2326 }
	# anyExtendedKeyUsage
	sub CRYPT_CERTINFO_EXTKEY_ANYKEYUSAGE { 2327 }
	# serverGatedCrypto
	sub CRYPT_CERTINFO_EXTKEY_NS_SERVERGATEDCRYPTO { 2328 }
	# serverGatedCrypto CA
	sub CRYPT_CERTINFO_EXTKEY_VS_SERVERGATEDCRYPTO_CA { 2329 }

	# 2 5 29 40 crlStreamIdentifier

	sub CRYPT_CERTINFO_CRLSTREAMIDENTIFIER { 2330 }

	# 2 5 29 46 freshestCRL

	sub CRYPT_CERTINFO_FRESHESTCRL { 2331 }
	# distributionPointName.fullName
	sub CRYPT_CERTINFO_FRESHESTCRL_FULLNAME { 2332 }
	# reasons
	sub CRYPT_CERTINFO_FRESHESTCRL_REASONS { 2333 }
	# cRLIssuer
	sub CRYPT_CERTINFO_FRESHESTCRL_CRLISSUER { 2334 }

	# 2 5 29 47 orderedList

	sub CRYPT_CERTINFO_ORDEREDLIST { 2335 }

	# 2 5 29 51 baseUpdateTime

	sub CRYPT_CERTINFO_BASEUPDATETIME { 2336 }

	# 2 5 29 53 deltaInfo

	sub CRYPT_CERTINFO_DELTAINFO { 2337 }
	# deltaLocation
	sub CRYPT_CERTINFO_DELTAINFO_LOCATION { 2338 }
	# nextDelta
	sub CRYPT_CERTINFO_DELTAINFO_NEXTDELTA { 2339 }

	# 2 5 29 54 inhibitAnyPolicy

	sub CRYPT_CERTINFO_INHIBITANYPOLICY { 2340 }

	# 2 5 29 58 toBeRevoked

	sub CRYPT_CERTINFO_TOBEREVOKED { 2341 }
	# certificateIssuer
	sub CRYPT_CERTINFO_TOBEREVOKED_CERTISSUER { 2342 }
	# reasonCode
	sub CRYPT_CERTINFO_TOBEREVOKED_REASONCODE { 2343 }
	# revocationTime
	sub CRYPT_CERTINFO_TOBEREVOKED_REVOCATIONTIME { 2344 }
	# certSerialNumber
	sub CRYPT_CERTINFO_TOBEREVOKED_CERTSERIALNUMBER { 2345 }

	# 2 5 29 59 revokedGroups

	sub CRYPT_CERTINFO_REVOKEDGROUPS { 2346 }
	# certificateIssuer
	sub CRYPT_CERTINFO_REVOKEDGROUPS_CERTISSUER { 2347 }
	# reasonCode
	sub CRYPT_CERTINFO_REVOKEDGROUPS_REASONCODE { 2348 }
	# invalidityDate
	sub CRYPT_CERTINFO_REVOKEDGROUPS_INVALIDITYDATE { 2349 }
	# startingNumber
	sub CRYPT_CERTINFO_REVOKEDGROUPS_STARTINGNUMBER { 2350 }
	# endingNumber
	sub CRYPT_CERTINFO_REVOKEDGROUPS_ENDINGNUMBER { 2351 }

	# 2 5 29 60 expiredCertsOnCRL

	sub CRYPT_CERTINFO_EXPIREDCERTSONCRL { 2352 }

	# 2 5 29 63 aaIssuingDistributionPoint

	sub CRYPT_CERTINFO_AAISSUINGDISTRIBUTIONPOINT { 2353 }
	# distributionPointName.fullName
	sub CRYPT_CERTINFO_AAISSUINGDIST_FULLNAME { 2354 }
	# onlySomeReasons
	sub CRYPT_CERTINFO_AAISSUINGDIST_SOMEREASONSONLY { 2355 }
	# indirectCRL
	sub CRYPT_CERTINFO_AAISSUINGDIST_INDIRECTCRL { 2356 }
	# containsUserAttributeCerts
	sub CRYPT_CERTINFO_AAISSUINGDIST_USERATTRCERTS { 2357 }
	# containsAACerts
	sub CRYPT_CERTINFO_AAISSUINGDIST_AACERTS { 2358 }
	# containsSOAPublicKeyCerts
	sub CRYPT_CERTINFO_AAISSUINGDIST_SOACERTS { 2359 }

	# 2 16 840 1 113730 1 x Netscape extensions
	# netscape-cert-type
	sub CRYPT_CERTINFO_NS_CERTTYPE { 2360 }
	# netscape-base-url
	sub CRYPT_CERTINFO_NS_BASEURL { 2361 }
	# netscape-revocation-url
	sub CRYPT_CERTINFO_NS_REVOCATIONURL { 2362 }
	# netscape-ca-revocation-url
	sub CRYPT_CERTINFO_NS_CAREVOCATIONURL { 2363 }
	# netscape-cert-renewal-url
	sub CRYPT_CERTINFO_NS_CERTRENEWALURL { 2364 }
	# netscape-ca-policy-url
	sub CRYPT_CERTINFO_NS_CAPOLICYURL { 2365 }
	# netscape-ssl-server-name
	sub CRYPT_CERTINFO_NS_SSLSERVERNAME { 2366 }
	# netscape-comment
	sub CRYPT_CERTINFO_NS_COMMENT { 2367 }

	# 2 23 42 7 0 SET hashedRootKey

	sub CRYPT_CERTINFO_SET_HASHEDROOTKEY { 2368 }
	# rootKeyThumbPrint
	sub CRYPT_CERTINFO_SET_ROOTKEYTHUMBPRINT { 2369 }

	# 2 23 42 7 1 SET certificateType

	sub CRYPT_CERTINFO_SET_CERTIFICATETYPE { 2370 }

	# 2 23 42 7 2 SET merchantData

	sub CRYPT_CERTINFO_SET_MERCHANTDATA { 2371 }
	# merID
	sub CRYPT_CERTINFO_SET_MERID { 2372 }
	# merAcquirerBIN
	sub CRYPT_CERTINFO_SET_MERACQUIRERBIN { 2373 }
	# merNames.language
	sub CRYPT_CERTINFO_SET_MERCHANTLANGUAGE { 2374 }
	# merNames.name
	sub CRYPT_CERTINFO_SET_MERCHANTNAME { 2375 }
	# merNames.city
	sub CRYPT_CERTINFO_SET_MERCHANTCITY { 2376 }
	# merNames.stateProvince
	sub CRYPT_CERTINFO_SET_MERCHANTSTATEPROVINCE { 2377 }
	# merNames.postalCode
	sub CRYPT_CERTINFO_SET_MERCHANTPOSTALCODE { 2378 }
	# merNames.countryName
	sub CRYPT_CERTINFO_SET_MERCHANTCOUNTRYNAME { 2379 }
	# merCountry
	sub CRYPT_CERTINFO_SET_MERCOUNTRY { 2380 }
	# merAuthFlag
	sub CRYPT_CERTINFO_SET_MERAUTHFLAG { 2381 }

	# 2 23 42 7 3 SET certCardRequired

	sub CRYPT_CERTINFO_SET_CERTCARDREQUIRED { 2382 }

	# 2 23 42 7 4 SET tunneling

	sub CRYPT_CERTINFO_SET_TUNNELING { 2383 }

	sub CRYPT_CERTINFO_SET_TUNNELLING { 2383 }
	# tunneling
	sub CRYPT_CERTINFO_SET_TUNNELINGFLAG { 2384 }

	sub CRYPT_CERTINFO_SET_TUNNELLINGFLAG { 2384 }
	# tunnelingAlgID
	sub CRYPT_CERTINFO_SET_TUNNELINGALGID { 2385 }

	sub CRYPT_CERTINFO_SET_TUNNELLINGALGID { 2385 }

	# S/MIME attributes

	# 1 2 840 113549 1 9 3 contentType

	sub CRYPT_CERTINFO_CMS_CONTENTTYPE { 2500 }

	# 1 2 840 113549 1 9 4 messageDigest

	sub CRYPT_CERTINFO_CMS_MESSAGEDIGEST { 2501 }

	# 1 2 840 113549 1 9 5 signingTime

	sub CRYPT_CERTINFO_CMS_SIGNINGTIME { 2502 }

	# 1 2 840 113549 1 9 6 counterSignature
	# counterSignature
	sub CRYPT_CERTINFO_CMS_COUNTERSIGNATURE { 2503 }

	# 1 2 840 113549 1 9 13 signingDescription

	sub CRYPT_CERTINFO_CMS_SIGNINGDESCRIPTION { 2504 }

	# 1 2 840 113549 1 9 15 sMIMECapabilities

	sub CRYPT_CERTINFO_CMS_SMIMECAPABILITIES { 2505 }
	# 3DES encryption
	sub CRYPT_CERTINFO_CMS_SMIMECAP_3DES { 2506 }
	# AES encryption
	sub CRYPT_CERTINFO_CMS_SMIMECAP_AES { 2507 }
	# CAST-128 encryption
	sub CRYPT_CERTINFO_CMS_SMIMECAP_CAST128 { 2508 }
	# SHA2-ng hash
	sub CRYPT_CERTINFO_CMS_SMIMECAP_SHAng { 2509 }
	# SHA2-256 hash
	sub CRYPT_CERTINFO_CMS_SMIMECAP_SHA2 { 2510 }
	# SHA1 hash
	sub CRYPT_CERTINFO_CMS_SMIMECAP_SHA1 { 2511 }
	# HMAC-SHA2-ng MAC
	sub CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHAng { 2512 }
	# HMAC-SHA2-256 MAC
	sub CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA2 { 2513 }
	# HMAC-SHA1 MAC
	sub CRYPT_CERTINFO_CMS_SMIMECAP_HMAC_SHA1 { 2514 }
	# AuthEnc w.256-bit key
	sub CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC256 { 2515 }
	# AuthEnc w.128-bit key
	sub CRYPT_CERTINFO_CMS_SMIMECAP_AUTHENC128 { 2516 }
	# RSA with SHA-ng signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHAng { 2517 }
	# RSA with SHA2-256 signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA2 { 2518 }
	# RSA with SHA1 signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_RSA_SHA1 { 2519 }
	# DSA with SHA-1 signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_DSA_SHA1 { 2520 }
	# ECDSA with SHA-ng signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHAng { 2521 }
	# ECDSA with SHA2-256 signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA2 { 2522 }
	# ECDSA with SHA-1 signing
	sub CRYPT_CERTINFO_CMS_SMIMECAP_ECDSA_SHA1 { 2523 }
	# preferSignedData
	sub CRYPT_CERTINFO_CMS_SMIMECAP_PREFERSIGNEDDATA { 2524 }
	# canNotDecryptAny
	sub CRYPT_CERTINFO_CMS_SMIMECAP_CANNOTDECRYPTANY { 2525 }
	# preferBinaryInside
	sub CRYPT_CERTINFO_CMS_SMIMECAP_PREFERBINARYINSIDE { 2526 }

	# 1 2 840 113549 1 9 16 2 1 receiptRequest

	sub CRYPT_CERTINFO_CMS_RECEIPTREQUEST { 2527 }
	# contentIdentifier
	sub CRYPT_CERTINFO_CMS_RECEIPT_CONTENTIDENTIFIER { 2528 }
	# receiptsFrom
	sub CRYPT_CERTINFO_CMS_RECEIPT_FROM { 2529 }
	# receiptsTo
	sub CRYPT_CERTINFO_CMS_RECEIPT_TO { 2530 }

	# 1 2 840 113549 1 9 16 2 2 essSecurityLabel

	sub CRYPT_CERTINFO_CMS_SECURITYLABEL { 2531 }
	# securityPolicyIdentifier
	sub CRYPT_CERTINFO_CMS_SECLABEL_POLICY { 2532 }
	# securityClassification
	sub CRYPT_CERTINFO_CMS_SECLABEL_CLASSIFICATION { 2533 }
	# privacyMark
	sub CRYPT_CERTINFO_CMS_SECLABEL_PRIVACYMARK { 2534 }
	# securityCategories.securityCategory.type
	sub CRYPT_CERTINFO_CMS_SECLABEL_CATTYPE { 2535 }
	# securityCategories.securityCategory.value
	sub CRYPT_CERTINFO_CMS_SECLABEL_CATVALUE { 2536 }

	# 1 2 840 113549 1 9 16 2 3 mlExpansionHistory

	sub CRYPT_CERTINFO_CMS_MLEXPANSIONHISTORY { 2537 }
	# mlData.mailListIdentifier.issuerAndSerialNumber
	sub CRYPT_CERTINFO_CMS_MLEXP_ENTITYIDENTIFIER { 2538 }
	# mlData.expansionTime
	sub CRYPT_CERTINFO_CMS_MLEXP_TIME { 2539 }
	# mlData.mlReceiptPolicy.none
	sub CRYPT_CERTINFO_CMS_MLEXP_NONE { 2540 }
	# mlData.mlReceiptPolicy.insteadOf.generalNames.generalName
	sub CRYPT_CERTINFO_CMS_MLEXP_INSTEADOF { 2541 }
	# mlData.mlReceiptPolicy.inAdditionTo.generalNames.generalName
	sub CRYPT_CERTINFO_CMS_MLEXP_INADDITIONTO { 2542 }

	# 1 2 840 113549 1 9 16 2 4 contentHints

	sub CRYPT_CERTINFO_CMS_CONTENTHINTS { 2543 }
	# contentDescription
	sub CRYPT_CERTINFO_CMS_CONTENTHINT_DESCRIPTION { 2544 }
	# contentType
	sub CRYPT_CERTINFO_CMS_CONTENTHINT_TYPE { 2545 }

	# 1 2 840 113549 1 9 16 2 9 equivalentLabels

	sub CRYPT_CERTINFO_CMS_EQUIVALENTLABEL { 2546 }
	# securityPolicyIdentifier
	sub CRYPT_CERTINFO_CMS_EQVLABEL_POLICY { 2547 }
	# securityClassification
	sub CRYPT_CERTINFO_CMS_EQVLABEL_CLASSIFICATION { 2548 }
	# privacyMark
	sub CRYPT_CERTINFO_CMS_EQVLABEL_PRIVACYMARK { 2549 }
	# securityCategories.securityCategory.type
	sub CRYPT_CERTINFO_CMS_EQVLABEL_CATTYPE { 2550 }
	# securityCategories.securityCategory.value
	sub CRYPT_CERTINFO_CMS_EQVLABEL_CATVALUE { 2551 }

	# 1 2 840 113549 1 9 16 2 12 signingCertificate

	sub CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATE { 2552 }
	# certs.essCertID
	sub CRYPT_CERTINFO_CMS_SIGNINGCERT_ESSCERTID { 2553 }
	# policies.policyInformation.policyIdentifier
	sub CRYPT_CERTINFO_CMS_SIGNINGCERT_POLICIES { 2554 }

	# 1 2 840 113549 1 9 16 2 47 signingCertificateV2

	sub CRYPT_CERTINFO_CMS_SIGNINGCERTIFICATEV2 { 2555 }
	# certs.essCertID
	sub CRYPT_CERTINFO_CMS_SIGNINGCERTV2_ESSCERTIDV2 { 2556 }
	# policies.policyInformation.policyIdentifier
	sub CRYPT_CERTINFO_CMS_SIGNINGCERTV2_POLICIES { 2557 }

	# 1 2 840 113549 1 9 16 2 15 signaturePolicyID

	sub CRYPT_CERTINFO_CMS_SIGNATUREPOLICYID { 2558 }
	# sigPolicyID
	sub CRYPT_CERTINFO_CMS_SIGPOLICYID { 2559 }
	# sigPolicyHash
	sub CRYPT_CERTINFO_CMS_SIGPOLICYHASH { 2560 }
	# sigPolicyQualifiers.sigPolicyQualifier.cPSuri
	sub CRYPT_CERTINFO_CMS_SIGPOLICY_CPSURI { 2561 }

	sub CRYPT_CERTINFO_CMS_SIGPOLICY_ORGANIZATION { 2562 }
	# sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.organization

	sub CRYPT_CERTINFO_CMS_SIGPOLICY_NOTICENUMBERS { 2563 }
	# sigPolicyQualifiers.sigPolicyQualifier.userNotice.noticeRef.noticeNumbers

	sub CRYPT_CERTINFO_CMS_SIGPOLICY_EXPLICITTEXT { 2564 }
	# sigPolicyQualifiers.sigPolicyQualifier.userNotice.explicitText

	# 1 2 840 113549 1 9 16 9 signatureTypeIdentifier

	sub CRYPT_CERTINFO_CMS_SIGTYPEIDENTIFIER { 2565 }
	# originatorSig
	sub CRYPT_CERTINFO_CMS_SIGTYPEID_ORIGINATORSIG { 2566 }
	# domainSig
	sub CRYPT_CERTINFO_CMS_SIGTYPEID_DOMAINSIG { 2567 }
	# additionalAttributesSig
	sub CRYPT_CERTINFO_CMS_SIGTYPEID_ADDITIONALATTRIBUTES { 2568 }
	# reviewSig
	sub CRYPT_CERTINFO_CMS_SIGTYPEID_REVIEWSIG { 2569 }

	# 1 2 840 113549 1 9 25 3 randomNonce
	# randomNonce
	sub CRYPT_CERTINFO_CMS_NONCE { 2570 }

	# SCEP attributes:
	# 2 16 840 1 113733 1 9 2 messageType
	# 2 16 840 1 113733 1 9 3 pkiStatus
	# 2 16 840 1 113733 1 9 4 failInfo
	# 2 16 840 1 113733 1 9 5 senderNonce
	# 2 16 840 1 113733 1 9 6 recipientNonce
	# 2 16 840 1 113733 1 9 7 transID
	# messageType
	sub CRYPT_CERTINFO_SCEP_MESSAGETYPE { 2571 }
	# pkiStatus
	sub CRYPT_CERTINFO_SCEP_PKISTATUS { 2572 }
	# failInfo
	sub CRYPT_CERTINFO_SCEP_FAILINFO { 2573 }
	# senderNonce
	sub CRYPT_CERTINFO_SCEP_SENDERNONCE { 2574 }
	# recipientNonce
	sub CRYPT_CERTINFO_SCEP_RECIPIENTNONCE { 2575 }
	# transID
	sub CRYPT_CERTINFO_SCEP_TRANSACTIONID { 2576 }

	# 1 3 6 1 4 1 311 2 1 10 spcAgencyInfo

	sub CRYPT_CERTINFO_CMS_SPCAGENCYINFO { 2577 }
	# spcAgencyInfo.url
	sub CRYPT_CERTINFO_CMS_SPCAGENCYURL { 2578 }

	# 1 3 6 1 4 1 311 2 1 11 spcStatementType

	sub CRYPT_CERTINFO_CMS_SPCSTATEMENTTYPE { 2579 }
	# individualCodeSigning
	sub CRYPT_CERTINFO_CMS_SPCSTMT_INDIVIDUALCODESIGNING { 2580 }
	# commercialCodeSigning
	sub CRYPT_CERTINFO_CMS_SPCSTMT_COMMERCIALCODESIGNING { 2581 }

	# 1 3 6 1 4 1 311 2 1 12 spcOpusInfo

	sub CRYPT_CERTINFO_CMS_SPCOPUSINFO { 2582 }
	# spcOpusInfo.name
	sub CRYPT_CERTINFO_CMS_SPCOPUSINFO_NAME { 2583 }
	# spcOpusInfo.url
	sub CRYPT_CERTINFO_CMS_SPCOPUSINFO_URL { 2584 }

	# Used internally

	sub CRYPT_CERTINFO_LAST { 2585 }
	sub CRYPT_KEYINFO_FIRST { 3000 }

	# *******************
	# Keyset attributes
	# *******************

	# Keyset query
	sub CRYPT_KEYINFO_QUERY { 3001 }
	# Query of requests in cert store
	sub CRYPT_KEYINFO_QUERY_REQUESTS { 3002 }

	# Used internally

	sub CRYPT_KEYINFO_LAST { 3003 }
	sub CRYPT_DEVINFO_FIRST { 4000 }

	# *******************
	# Device attributes
	# *******************

	# Initialise device for use
	sub CRYPT_DEVINFO_INITIALISE { 4001 }

	sub CRYPT_DEVINFO_INITIALIZE { 4001 }
	# Authenticate user to device
	sub CRYPT_DEVINFO_AUTHENT_USER { 4002 }
	# Authenticate supervisor to dev.
	sub CRYPT_DEVINFO_AUTHENT_SUPERVISOR { 4003 }
	# Set user authent.value
	sub CRYPT_DEVINFO_SET_AUTHENT_USER { 4004 }
	# Set supervisor auth.val.
	sub CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR { 4005 }
	# Zeroise device
	sub CRYPT_DEVINFO_ZEROISE { 4006 }

	sub CRYPT_DEVINFO_ZEROIZE { 4006 }
	# Whether user is logged in
	sub CRYPT_DEVINFO_LOGGEDIN { 4007 }
	# Device/token label
	sub CRYPT_DEVINFO_LABEL { 4008 }

	# Used internally

	sub CRYPT_DEVINFO_LAST { 4009 }
	sub CRYPT_ENVINFO_FIRST { 5000 }

	# *********************
	# Envelope attributes
	# *********************

	# Pseudo-information on an envelope or meta-information which is used to
	# control the way that data in an envelope is processed
	# Data size information
	sub CRYPT_ENVINFO_DATASIZE { 5001 }
	# Compression information
	sub CRYPT_ENVINFO_COMPRESSION { 5002 }
	# Inner CMS content type
	sub CRYPT_ENVINFO_CONTENTTYPE { 5003 }
	# Detached signature
	sub CRYPT_ENVINFO_DETACHEDSIGNATURE { 5004 }
	# Signature check result
	sub CRYPT_ENVINFO_SIGNATURE_RESULT { 5005 }
	# Integrity-protection level
	sub CRYPT_ENVINFO_INTEGRITY { 5006 }

	# Resources required for enveloping/deenveloping
	# User password
	sub CRYPT_ENVINFO_PASSWORD { 5007 }
	# Conventional encryption key
	sub CRYPT_ENVINFO_KEY { 5008 }
	# Signature/signature check key
	sub CRYPT_ENVINFO_SIGNATURE { 5009 }
	# Extra information added to CMS sigs
	sub CRYPT_ENVINFO_SIGNATURE_EXTRADATA { 5010 }
	# Recipient email address
	sub CRYPT_ENVINFO_RECIPIENT { 5011 }
	# PKC encryption key
	sub CRYPT_ENVINFO_PUBLICKEY { 5012 }
	# PKC decryption key
	sub CRYPT_ENVINFO_PRIVATEKEY { 5013 }
	# Label of PKC decryption key
	sub CRYPT_ENVINFO_PRIVATEKEY_LABEL { 5014 }
	# Originator info/key
	sub CRYPT_ENVINFO_ORIGINATOR { 5015 }
	# Session key
	sub CRYPT_ENVINFO_SESSIONKEY { 5016 }
	# Hash value
	sub CRYPT_ENVINFO_HASH { 5017 }
	# Timestamp information
	sub CRYPT_ENVINFO_TIMESTAMP { 5018 }

	# Keysets used to retrieve keys needed for enveloping/deenveloping
	# Signature check keyset
	sub CRYPT_ENVINFO_KEYSET_SIGCHECK { 5019 }
	# PKC encryption keyset
	sub CRYPT_ENVINFO_KEYSET_ENCRYPT { 5020 }
	# PKC decryption keyset
	sub CRYPT_ENVINFO_KEYSET_DECRYPT { 5021 }

	# Used internally

	sub CRYPT_ENVINFO_LAST { 5022 }
	sub CRYPT_SESSINFO_FIRST { 6000 }

	# ********************
	# Session attributes
	# ********************

	# Pseudo-information about the session
	# Whether session is active
	sub CRYPT_SESSINFO_ACTIVE { 6001 }
	# Whether network connection is active
	sub CRYPT_SESSINFO_CONNECTIONACTIVE { 6002 }

	# Security-related information
	# User name
	sub CRYPT_SESSINFO_USERNAME { 6003 }
	# Password
	sub CRYPT_SESSINFO_PASSWORD { 6004 }
	# Server/client private key
	sub CRYPT_SESSINFO_PRIVATEKEY { 6005 }
	# Certificate store
	sub CRYPT_SESSINFO_KEYSET { 6006 }
	# Session authorisation OK
	sub CRYPT_SESSINFO_AUTHRESPONSE { 6007 }

	# Client/server information
	# Server name
	sub CRYPT_SESSINFO_SERVER_NAME { 6008 }
	# Server port number
	sub CRYPT_SESSINFO_SERVER_PORT { 6009 }
	# Server key fingerprint
	sub CRYPT_SESSINFO_SERVER_FINGERPRINT_SHA1 { 6010 }
	# Client name
	sub CRYPT_SESSINFO_CLIENT_NAME { 6011 }
	# Client port number
	sub CRYPT_SESSINFO_CLIENT_PORT { 6012 }
	# Transport mechanism
	sub CRYPT_SESSINFO_SESSION { 6013 }
	# User-supplied network socket
	sub CRYPT_SESSINFO_NETWORKSOCKET { 6014 }

	# Generic protocol-related information
	# Protocol version
	sub CRYPT_SESSINFO_VERSION { 6015 }
	# Cert.request object
	sub CRYPT_SESSINFO_REQUEST { 6016 }
	# Cert.response object
	sub CRYPT_SESSINFO_RESPONSE { 6017 }
	# Issuing CA certificate
	sub CRYPT_SESSINFO_CACERTIFICATE { 6018 }

	# Protocol-specific information
	# Request type
	sub CRYPT_SESSINFO_CMP_REQUESTTYPE { 6019 }
	# Private-key keyset
	sub CRYPT_SESSINFO_CMP_PRIVKEYSET { 6020 }
	# SSH current channel
	sub CRYPT_SESSINFO_SSH_CHANNEL { 6021 }
	# SSH channel type
	sub CRYPT_SESSINFO_SSH_CHANNEL_TYPE { 6022 }
	# SSH channel argument 1
	sub CRYPT_SESSINFO_SSH_CHANNEL_ARG1 { 6023 }
	# SSH channel argument 2
	sub CRYPT_SESSINFO_SSH_CHANNEL_ARG2 { 6024 }
	# SSH channel active
	sub CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE { 6025 }
	# SSL/TLS protocol options
	sub CRYPT_SESSINFO_SSL_OPTIONS { 6026 }
	# TSP message imprint
	sub CRYPT_SESSINFO_TSP_MSGIMPRINT { 6027 }

	# Used internally

	sub CRYPT_SESSINFO_LAST { 6028 }
	sub CRYPT_USERINFO_FIRST { 7000 }

	# ********************
	# User attributes
	# ********************

	# Security-related information
	# Password
	sub CRYPT_USERINFO_PASSWORD { 7001 }

	# User role-related information
	# CA cert signing key
	sub CRYPT_USERINFO_CAKEY_CERTSIGN { 7002 }
	# CA CRL signing key
	sub CRYPT_USERINFO_CAKEY_CRLSIGN { 7003 }
	# CA RTCS signing key
	sub CRYPT_USERINFO_CAKEY_RTCSSIGN { 7004 }
	# CA OCSP signing key
	sub CRYPT_USERINFO_CAKEY_OCSPSIGN { 7005 }

	# Used internally for range checking

	sub CRYPT_USERINFO_LAST { 7006 }
	sub CRYPT_ATTRIBUTE_LAST { 7006 }



##### END ENUM CRYPT_ATTRIBUTE_TYPE

#****************************************************************************
#*                                                                           *
#*                       Attribute Subtypes and Related Values               *
#*                                                                           *
#****************************************************************************

# Flags for the X.509 keyUsage extension 

	sub CRYPT_KEYUSAGE_NONE { 0x000 }
	sub CRYPT_KEYUSAGE_DIGITALSIGNATURE { 0x001 }
	sub CRYPT_KEYUSAGE_NONREPUDIATION { 0x002 }
	sub CRYPT_KEYUSAGE_KEYENCIPHERMENT { 0x004 }
	sub CRYPT_KEYUSAGE_DATAENCIPHERMENT { 0x008 }
	sub CRYPT_KEYUSAGE_KEYAGREEMENT { 0x010 }
	sub CRYPT_KEYUSAGE_KEYCERTSIGN { 0x020 }
	sub CRYPT_KEYUSAGE_CRLSIGN { 0x040 }
	sub CRYPT_KEYUSAGE_ENCIPHERONLY { 0x080 }
	sub CRYPT_KEYUSAGE_DECIPHERONLY { 0x100 }
	sub CRYPT_KEYUSAGE_LAST { 0x200 }
# Last possible value 

# X.509 cRLReason and cryptlib cRLExtReason codes 

  sub CRYPT_CRLREASON_UNSPECIFIED { 0 }
  sub CRYPT_CRLREASON_KEYCOMPROMISE { 1 }
  sub CRYPT_CRLREASON_CACOMPROMISE { 2 }
  sub CRYPT_CRLREASON_AFFILIATIONCHANGED { 3 }
  sub CRYPT_CRLREASON_SUPERSEDED { 4 }
  sub CRYPT_CRLREASON_CESSATIONOFOPERATION { 5 }
  sub CRYPT_CRLREASON_CERTIFICATEHOLD { 6 }
  sub CRYPT_CRLREASON_REMOVEFROMCRL { 8 }
  sub CRYPT_CRLREASON_PRIVILEGEWITHDRAWN { 9 }
  sub CRYPT_CRLREASON_AACOMPROMISE { 10 }
  sub CRYPT_CRLREASON_LAST { 11 }
  sub CRYPT_CRLREASON_NEVERVALID { 20 }
  sub CRYPT_CRLEXTREASON_LAST  { 21 }


#  X.509 CRL reason flags.  These identify the same thing as the cRLReason
#  codes but allow for multiple reasons to be specified.  Note that these
#  don't follow the X.509 naming since in that scheme the enumerated types
#  and bitflags have the same names 

	sub CRYPT_CRLREASONFLAG_UNUSED { 0x001 }
	sub CRYPT_CRLREASONFLAG_KEYCOMPROMISE { 0x002 }
	sub CRYPT_CRLREASONFLAG_CACOMPROMISE { 0x004 }
	sub CRYPT_CRLREASONFLAG_AFFILIATIONCHANGED { 0x008 }
	sub CRYPT_CRLREASONFLAG_SUPERSEDED { 0x010 }
	sub CRYPT_CRLREASONFLAG_CESSATIONOFOPERATION { 0x020 }
	sub CRYPT_CRLREASONFLAG_CERTIFICATEHOLD { 0x040 }
	sub CRYPT_CRLREASONFLAG_LAST { 0x080 }
# Last poss.value 

# X.509 CRL holdInstruction codes 

  sub CRYPT_HOLDINSTRUCTION_NONE { 0 }
  sub CRYPT_HOLDINSTRUCTION_CALLISSUER { 1 }
  sub CRYPT_HOLDINSTRUCTION_REJECT { 2 }
  sub CRYPT_HOLDINSTRUCTION_PICKUPTOKEN { 3 }
  sub CRYPT_HOLDINSTRUCTION_LAST  { 4 }


# Certificate checking compliance levels 

  sub CRYPT_COMPLIANCELEVEL_OBLIVIOUS { 0 }
  sub CRYPT_COMPLIANCELEVEL_REDUCED { 1 }
  sub CRYPT_COMPLIANCELEVEL_STANDARD { 2 }
  sub CRYPT_COMPLIANCELEVEL_PKIX_PARTIAL { 3 }
  sub CRYPT_COMPLIANCELEVEL_PKIX_FULL { 4 }
  sub CRYPT_COMPLIANCELEVEL_LAST  { 5 }


# Flags for the Netscape netscape-cert-type extension 

	sub CRYPT_NS_CERTTYPE_SSLCLIENT { 0x001 }
	sub CRYPT_NS_CERTTYPE_SSLSERVER { 0x002 }
	sub CRYPT_NS_CERTTYPE_SMIME { 0x004 }
	sub CRYPT_NS_CERTTYPE_OBJECTSIGNING { 0x008 }
	sub CRYPT_NS_CERTTYPE_RESERVED { 0x010 }
	sub CRYPT_NS_CERTTYPE_SSLCA { 0x020 }
	sub CRYPT_NS_CERTTYPE_SMIMECA { 0x040 }
	sub CRYPT_NS_CERTTYPE_OBJECTSIGNINGCA { 0x080 }
	sub CRYPT_NS_CERTTYPE_LAST { 0x100 }
# Last possible value 

# Flags for the SET certificate-type extension 

	sub CRYPT_SET_CERTTYPE_CARD { 0x001 }
	sub CRYPT_SET_CERTTYPE_MER { 0x002 }
	sub CRYPT_SET_CERTTYPE_PGWY { 0x004 }
	sub CRYPT_SET_CERTTYPE_CCA { 0x008 }
	sub CRYPT_SET_CERTTYPE_MCA { 0x010 }
	sub CRYPT_SET_CERTTYPE_PCA { 0x020 }
	sub CRYPT_SET_CERTTYPE_GCA { 0x040 }
	sub CRYPT_SET_CERTTYPE_BCA { 0x080 }
	sub CRYPT_SET_CERTTYPE_RCA { 0x100 }
	sub CRYPT_SET_CERTTYPE_ACQ { 0x200 }
	sub CRYPT_SET_CERTTYPE_LAST { 0x400 }
# Last possible value 

# CMS contentType values 

##### BEGIN ENUM CRYPT_CONTENT_TYPE

	sub CRYPT_CONTENT_NONE { 0 }
	sub CRYPT_CONTENT_DATA { 1 }

	sub CRYPT_CONTENT_SIGNEDDATA { 2 }
	sub CRYPT_CONTENT_ENVELOPEDDATA { 3 }

	sub CRYPT_CONTENT_SIGNEDANDENVELOPEDDATA { 4 }

	sub CRYPT_CONTENT_DIGESTEDDATA { 5 }
	sub CRYPT_CONTENT_ENCRYPTEDDATA { 6 }

	sub CRYPT_CONTENT_COMPRESSEDDATA { 7 }
	sub CRYPT_CONTENT_AUTHDATA { 8 }

	sub CRYPT_CONTENT_AUTHENVDATA { 9 }
	sub CRYPT_CONTENT_TSTINFO { 10 }

	sub CRYPT_CONTENT_SPCINDIRECTDATACONTEXT { 11 }

	sub CRYPT_CONTENT_RTCSREQUEST { 12 }
	sub CRYPT_CONTENT_RTCSRESPONSE { 13 }

	sub CRYPT_CONTENT_RTCSRESPONSE_EXT { 14 }
	sub CRYPT_CONTENT_MRTD { 15 }

	sub CRYPT_CONTENT_LAST { 16 }


##### END ENUM CRYPT_CONTENT_TYPE

# ESS securityClassification codes 

  sub CRYPT_CLASSIFICATION_UNMARKED { 0 }
  sub CRYPT_CLASSIFICATION_UNCLASSIFIED { 1 }
  sub CRYPT_CLASSIFICATION_RESTRICTED { 2 }
  sub CRYPT_CLASSIFICATION_CONFIDENTIAL { 3 }
  sub CRYPT_CLASSIFICATION_SECRET { 4 }
  sub CRYPT_CLASSIFICATION_TOP_SECRET { 5 }
  sub CRYPT_CLASSIFICATION_LAST { 255 }


# RTCS certificate status 

  sub CRYPT_CERTSTATUS_VALID { 0 }
  sub CRYPT_CERTSTATUS_NOTVALID { 1 }
  sub CRYPT_CERTSTATUS_NONAUTHORITATIVE { 2 }
  sub CRYPT_CERTSTATUS_UNKNOWN  { 3 }


# OCSP revocation status 

  sub CRYPT_OCSPSTATUS_NOTREVOKED { 0 }
  sub CRYPT_OCSPSTATUS_REVOKED { 1 }
  sub CRYPT_OCSPSTATUS_UNKNOWN  { 2 }


#  The amount of detail to include in signatures when signing certificate
#  objects 

##### BEGIN ENUM CRYPT_SIGNATURELEVEL_TYPE

	# Include only signature
	sub CRYPT_SIGNATURELEVEL_NONE { 0 }
	# Include signer cert
	sub CRYPT_SIGNATURELEVEL_SIGNERCERT { 1 }
	# Include all relevant info
	sub CRYPT_SIGNATURELEVEL_ALL { 2 }
	# Last possible sig.level type
	sub CRYPT_SIGNATURELEVEL_LAST { 3 }


##### END ENUM CRYPT_SIGNATURELEVEL_TYPE

#  The level of integrity protection to apply to enveloped data.  The 
#  default envelope protection for an envelope with keying information 
#  applied is encryption, this can be modified to use MAC-only protection
#  (with no encryption) or hybrid encryption + authentication 

##### BEGIN ENUM CRYPT_INTEGRITY_TYPE

	# No integrity protection
	sub CRYPT_INTEGRITY_NONE { 0 }
	# MAC only, no encryption
	sub CRYPT_INTEGRITY_MACONLY { 1 }
	# Encryption + ingerity protection
	sub CRYPT_INTEGRITY_FULL { 2 }


##### END ENUM CRYPT_INTEGRITY_TYPE

#  The certificate export format type, which defines the format in which a
#  certificate object is exported 

##### BEGIN ENUM CRYPT_CERTFORMAT_TYPE

	# No certificate format
	sub CRYPT_CERTFORMAT_NONE { 0 }
	# DER-encoded certificate
	sub CRYPT_CERTFORMAT_CERTIFICATE { 1 }
	# PKCS #7 certificate chain
	sub CRYPT_CERTFORMAT_CERTCHAIN { 2 }
	# base-64 wrapped cert
	sub CRYPT_CERTFORMAT_TEXT_CERTIFICATE { 3 }
	# base-64 wrapped cert chain
	sub CRYPT_CERTFORMAT_TEXT_CERTCHAIN { 4 }
	# XML wrapped cert
	sub CRYPT_CERTFORMAT_XML_CERTIFICATE { 5 }
	# XML wrapped cert chain
	sub CRYPT_CERTFORMAT_XML_CERTCHAIN { 6 }
	# Last possible cert.format type
	sub CRYPT_CERTFORMAT_LAST { 7 }


##### END ENUM CRYPT_CERTFORMAT_TYPE

# CMP request types 

##### BEGIN ENUM CRYPT_REQUESTTYPE_TYPE

	# No request type
	sub CRYPT_REQUESTTYPE_NONE { 0 }
	# Initialisation request
	sub CRYPT_REQUESTTYPE_INITIALISATION { 1 }

	sub CRYPT_REQUESTTYPE_INITIALIZATION { 1 }
	# Certification request
	sub CRYPT_REQUESTTYPE_CERTIFICATE { 2 }
	# Key update request
	sub CRYPT_REQUESTTYPE_KEYUPDATE { 3 }
	# Cert revocation request
	sub CRYPT_REQUESTTYPE_REVOCATION { 4 }
	# PKIBoot request
	sub CRYPT_REQUESTTYPE_PKIBOOT { 5 }
	# Last possible request type
	sub CRYPT_REQUESTTYPE_LAST { 6 }


##### END ENUM CRYPT_REQUESTTYPE_TYPE

# Key ID types 

##### BEGIN ENUM CRYPT_KEYID_TYPE

	# No key ID type
	sub CRYPT_KEYID_NONE { 0 }
	# Key owner name
	sub CRYPT_KEYID_NAME { 1 }
	# Key owner URI
	sub CRYPT_KEYID_URI { 2 }
	# Synonym: owner email addr.
	sub CRYPT_KEYID_EMAIL { 2 }
	# Last possible key ID type
	sub CRYPT_KEYID_LAST { 3 }


##### END ENUM CRYPT_KEYID_TYPE

# The encryption object types 

##### BEGIN ENUM CRYPT_OBJECT_TYPE

	# No object type
	sub CRYPT_OBJECT_NONE { 0 }
	# Conventionally encrypted key
	sub CRYPT_OBJECT_ENCRYPTED_KEY { 1 }
	# PKC-encrypted key
	sub CRYPT_OBJECT_PKCENCRYPTED_KEY { 2 }
	# Key agreement information
	sub CRYPT_OBJECT_KEYAGREEMENT { 3 }
	# Signature
	sub CRYPT_OBJECT_SIGNATURE { 4 }
	# Last possible object type
	sub CRYPT_OBJECT_LAST { 5 }


##### END ENUM CRYPT_OBJECT_TYPE

# Object/attribute error type information 

##### BEGIN ENUM CRYPT_ERRTYPE_TYPE

	# No error information
	sub CRYPT_ERRTYPE_NONE { 0 }
	# Attribute data too small or large
	sub CRYPT_ERRTYPE_ATTR_SIZE { 1 }
	# Attribute value is invalid
	sub CRYPT_ERRTYPE_ATTR_VALUE { 2 }
	# Required attribute missing
	sub CRYPT_ERRTYPE_ATTR_ABSENT { 3 }
	# Non-allowed attribute present
	sub CRYPT_ERRTYPE_ATTR_PRESENT { 4 }
	# Cert: Constraint violation in object
	sub CRYPT_ERRTYPE_CONSTRAINT { 5 }
	# Cert: Constraint viol.in issuing cert
	sub CRYPT_ERRTYPE_ISSUERCONSTRAINT { 6 }
	# Last possible error info type
	sub CRYPT_ERRTYPE_LAST { 7 }


##### END ENUM CRYPT_ERRTYPE_TYPE

# Cert store management action type 

##### BEGIN ENUM CRYPT_CERTACTION_TYPE

	# No cert management action
	sub CRYPT_CERTACTION_NONE { 0 }
	# Create cert store
	sub CRYPT_CERTACTION_CREATE { 1 }
	# Connect to cert store
	sub CRYPT_CERTACTION_CONNECT { 2 }
	# Disconnect from cert store
	sub CRYPT_CERTACTION_DISCONNECT { 3 }
	# Error information
	sub CRYPT_CERTACTION_ERROR { 4 }
	# Add PKI user
	sub CRYPT_CERTACTION_ADDUSER { 5 }
	# Delete PKI user
	sub CRYPT_CERTACTION_DELETEUSER { 6 }
	# Cert request
	sub CRYPT_CERTACTION_REQUEST_CERT { 7 }
	# Cert renewal request
	sub CRYPT_CERTACTION_REQUEST_RENEWAL { 8 }
	# Cert revocation request
	sub CRYPT_CERTACTION_REQUEST_REVOCATION { 9 }
	# Cert creation
	sub CRYPT_CERTACTION_CERT_CREATION { 10 }
	# Confirmation of cert creation
	sub CRYPT_CERTACTION_CERT_CREATION_COMPLETE { 11 }
	# Cancellation of cert creation
	sub CRYPT_CERTACTION_CERT_CREATION_DROP { 12 }
	# Cancel of creation w.revocation
	sub CRYPT_CERTACTION_CERT_CREATION_REVERSE { 13 }
	# Delete reqs after restart
	sub CRYPT_CERTACTION_RESTART_CLEANUP { 14 }
	# Complete revocation after restart
	sub CRYPT_CERTACTION_RESTART_REVOKE_CERT { 15 }
	# Cert issue
	sub CRYPT_CERTACTION_ISSUE_CERT { 16 }
	# CRL issue
	sub CRYPT_CERTACTION_ISSUE_CRL { 17 }
	# Cert revocation
	sub CRYPT_CERTACTION_REVOKE_CERT { 18 }
	# Cert expiry
	sub CRYPT_CERTACTION_EXPIRE_CERT { 19 }
	# Clean up on restart
	sub CRYPT_CERTACTION_CLEANUP { 20 }
	# Last possible cert store log action
	sub CRYPT_CERTACTION_LAST { 21 }


##### END ENUM CRYPT_CERTACTION_TYPE

#  SSL/TLS protocol options.  CRYPT_SSLOPTION_MINVER_SSLV3 is the same as 
#  CRYPT_SSLOPTION_NONE since this is the baseline, although it's generally
#  never encountered since SSLv3 is disabled 

	sub CRYPT_SSLOPTION_NONE { 0x000 }
	sub CRYPT_SSLOPTION_MINVER_SSLV3 { 0x000 }
# Min.protocol version 
	sub CRYPT_SSLOPTION_MINVER_TLS10 { 0x001 }
	sub CRYPT_SSLOPTION_MINVER_TLS11 { 0x002 }
	sub CRYPT_SSLOPTION_MINVER_TLS12 { 0x003 }
	sub CRYPT_SSLOPTION_MINVER_TLS13 { 0x004 }
	sub CRYPT_SSLOPTION_MANUAL_CERTCHECK { 0x008 }
# Require manual cert.verif.
	sub CRYPT_SSLOPTION_DISABLE_NAMEVERIFY { 0x010 }
# Disable cert hostname check 
	sub CRYPT_SSLOPTION_DISABLE_CERTVERIFY { 0x020 }
# Disable certificate check 
	sub CRYPT_SSLOPTION_SUITEB_128 { 0x100 }
# SuiteB security levels (may 
	sub CRYPT_SSLOPTION_SUITEB_256 { 0x200 }
#  vanish in future releases) 

#****************************************************************************
#*                                                                           *
#*                               General Constants                           *
#*                                                                           *
#****************************************************************************

# The maximum user key size - 2048 bits 

	sub CRYPT_MAX_KEYSIZE { 256 }

# The maximum IV/cipher block size - 256 bits 

	sub CRYPT_MAX_IVSIZE { 32 }

#  The maximum public-key component size - 4096 bits, and maximum component
#  size for ECCs - 576 bits (to handle the P521 curve) 

	sub CRYPT_MAX_PKCSIZE { 512 }
	sub CRYPT_MAX_PKCSIZE_ECC { 72 }

#  The maximum hash size - 512 bits.  Before 3.4 this was 256 bits, in the 
#  3.4 release it was increased to 512 bits to accommodate SHA-3 

	sub CRYPT_MAX_HASHSIZE { 64 }

# The maximum size of a text string (e.g.key owner name) 

	sub CRYPT_MAX_TEXTSIZE { 64 }

#  A magic value indicating that the default setting for this parameter
#  should be used.  The parentheses are to catch potential erroneous use 
#  in an expression 

	sub CRYPT_USE_DEFAULT { -100 }

# A magic value for unused parameters 

	sub CRYPT_UNUSED { -101 }

#  Cursor positioning codes for certificate/CRL extensions.  The parentheses 
#  are to catch potential erroneous use in an expression 

	sub CRYPT_CURSOR_FIRST { -200 }
	sub CRYPT_CURSOR_PREVIOUS { -201 }
	sub CRYPT_CURSOR_NEXT { -202 }
	sub CRYPT_CURSOR_LAST { -203 }

#  The type of information polling to perform to get random seed 
#  information.  These values have to be negative because they're used
#  as magic length values for cryptAddRandom().  The parentheses are to 
#  catch potential erroneous use in an expression 

	sub CRYPT_RANDOM_FASTPOLL { -300 }
	sub CRYPT_RANDOM_SLOWPOLL { -301 }

# Whether the PKC key is a public or private key 

	sub CRYPT_KEYTYPE_PRIVATE { 0 }
	sub CRYPT_KEYTYPE_PUBLIC { 1 }

# Keyset open options 

##### BEGIN ENUM CRYPT_KEYOPT_TYPE

	# No options
	sub CRYPT_KEYOPT_NONE { 0 }
	# Open keyset in read-only mode
	sub CRYPT_KEYOPT_READONLY { 1 }
	# Create a new keyset
	sub CRYPT_KEYOPT_CREATE { 2 }
	# Last possible key option type
	sub CRYPT_KEYOPT_LAST { 3 }


##### END ENUM CRYPT_KEYOPT_TYPE

# The various cryptlib objects - these are just integer handles 

sub CRYPT_CERTIFICATE { 0 }
sub CRYPT_CONTEXT { 0 }
sub CRYPT_DEVICE { 0 }
sub CRYPT_ENVELOPE { 0 }
sub CRYPT_KEYSET { 0 }
sub CRYPT_SESSION { 0 }
sub CRYPT_USER { 0 }

#  Sometimes we don't know the exact type of a cryptlib object, so we use a
#  generic handle type to identify it 

sub CRYPT_HANDLE { 0 }

#****************************************************************************
#*                                                                           *
#*                           Encryption Data Structures                      *
#*                                                                           *
#****************************************************************************

# Results returned from the capability query 

sub CRYPT_QUERY_INFO
{
	{
	#  Algorithm information 
     algoName => ' ' x CRYPT_MAX_TEXTSIZE	#  Algorithm name 
    ,blockSize => 0	#  Block size of the algorithm 
    ,minKeySize => 0	#  Minimum key size in bytes 
    ,keySize => 0	#  Recommended key size in bytes 
    ,maxKeySize => 0	#  Maximum key size in bytes 
    
	}
}

#  Results returned from the encoded object query.  These provide
#  information on the objects created by cryptExportKey()/
#  cryptCreateSignature() 

sub CRYPT_OBJECT_INFO
{
	{
	#  The object type 
     objectType => 0	#  The encryption algorithm and mode 
    ,cryptAlgo => 0
    ,cryptMode => 0	#  The hash algorithm for Signature objects 
    ,hashAlgo => 0	#  The salt for derived keys 
    ,salt => ' ' x CRYPT_MAX_HASHSIZE
    ,saltSize => 0
    
	}
}

#  Key information for the public-key encryption algorithms.  These fields
#  are not accessed directly, but can be manipulated with the init/set/
#  destroyComponents() macros 

sub CRYPT_PKCINFO_RSA
{
	{
	#  Status information 
     isPublicKey => 0	#  Whether this is a public or private key 
	#  Public components 
    ,n => ' ' x CRYPT_MAX_PKCSIZE	#  Modulus 
    ,nLen => 0	#  Length of modulus in bits 
    ,e => ' ' x CRYPT_MAX_PKCSIZE	#  Public exponent 
    ,eLen => 0	#  Length of public exponent in bits 
	#  Private components 
    ,d => ' ' x CRYPT_MAX_PKCSIZE	#  Private exponent 
    ,dLen => 0	#  Length of private exponent in bits 
    ,p => ' ' x CRYPT_MAX_PKCSIZE	#  Prime factor 1 
    ,pLen => 0	#  Length of prime factor 1 in bits 
    ,q => ' ' x CRYPT_MAX_PKCSIZE	#  Prime factor 2 
    ,qLen => 0	#  Length of prime factor 2 in bits 
    ,u => ' ' x CRYPT_MAX_PKCSIZE	#  Mult.inverse of q, mod p 
    ,uLen => 0	#  Length of private exponent in bits 
    ,e1 => ' ' x CRYPT_MAX_PKCSIZE	#  Private exponent 1 (PKCS) 
    ,e1Len => 0	#  Length of private exponent in bits 
    ,e2 => ' ' x CRYPT_MAX_PKCSIZE	#  Private exponent 2 (PKCS) 
    ,e2Len => 0	#  Length of private exponent in bits 
    
	}
}

sub CRYPT_PKCINFO_DLP
{
	{
	#  Status information 
     isPublicKey => 0	#  Whether this is a public or private key 
	#  Public components 
    ,p => ' ' x CRYPT_MAX_PKCSIZE	#  Prime modulus 
    ,pLen => 0	#  Length of prime modulus in bits 
    ,q => ' ' x CRYPT_MAX_PKCSIZE	#  Prime divisor 
    ,qLen => 0	#  Length of prime divisor in bits 
    ,g => ' ' x CRYPT_MAX_PKCSIZE	#  h^( ( p - 1 ) / q ) mod p 
    ,gLen => 0	#  Length of g in bits 
    ,y => ' ' x CRYPT_MAX_PKCSIZE	#  Public random integer 
    ,yLen => 0	#  Length of public integer in bits 
	#  Private components 
    ,x => ' ' x CRYPT_MAX_PKCSIZE	#  Private random integer 
    ,xLen => 0	#  Length of private integer in bits 
    
	}
}

##### BEGIN ENUM CRYPT_ECCCURVE_TYPE

	# Named ECC curves.  Since these need to be mapped to all manner of
	# protocol- and mechanism-specific identifiers, when updating this list
	# grep for occurrences of the string "P256" (the most common one) and
	# check whether any related mapping tables need to be updated
	# No ECC curve type
	sub CRYPT_ECCCURVE_NONE { 0 }
	# NIST P256/X9.62 P256v1/SECG p256r1 curve
	sub CRYPT_ECCCURVE_P256 { 1 }
	# NIST P384, SECG p384r1 curve
	sub CRYPT_ECCCURVE_P384 { 2 }
	# NIST P521, SECG p521r1
	sub CRYPT_ECCCURVE_P521 { 3 }
	# Brainpool p256r1
	sub CRYPT_ECCCURVE_BRAINPOOL_P256 { 4 }
	# Brainpool p384r1
	sub CRYPT_ECCCURVE_BRAINPOOL_P384 { 5 }
	# Brainpool p512r1
	sub CRYPT_ECCCURVE_BRAINPOOL_P512 { 6 }
	# Last valid ECC curve type
	sub CRYPT_ECCCURVE_LAST { 7 }


##### END ENUM CRYPT_ECCCURVE_TYPE

sub CRYPT_PKCINFO_ECC
{
	{
	#  Status information 
     isPublicKey => 0	#  Whether this is a public or private key 
	#       Curve domain parameters.  Either the curveType or the explicit domain
	#       parameters must be provided 
    ,curveType => 0	#  Named curve 
    ,p => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Prime defining Fq 
    ,pLen => 0	#  Length of prime in bits 
    ,a => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Element in Fq defining curve 
    ,aLen => 0	#  Length of element a in bits 
    ,b => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Element in Fq defining curve 
    ,bLen => 0	#  Length of element b in bits 
    ,gx => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Element in Fq defining point 
    ,gxLen => 0	#  Length of element gx in bits 
    ,gy => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Element in Fq defining point 
    ,gyLen => 0	#  Length of element gy in bits 
    ,n => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Order of point 
    ,nLen => 0	#  Length of order in bits 
    ,h => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Optional cofactor 
    ,hLen => 0	#  Length of cofactor in bits 
	#  Public components 
    ,qx => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Point Q on the curve 
    ,qxLen => 0	#  Length of point xq in bits 
    ,qy => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Point Q on the curve 
    ,qyLen => 0	#  Length of point xy in bits 
	#  Private components 
    ,d => ' ' x CRYPT_MAX_PKCSIZE_ECC	#  Private random integer 
    ,dLen => 0	#  Length of integer in bits 
    
	}
}

#  Macros to initialise and destroy the structure that stores the components
#  of a public key 

# C-macro not translated to Perl code but implemented apart: 
#   #define cryptInitComponents( componentInfo, componentKeyType ) 
#    { memset( ( componentInfo ), 0, sizeof( *componentInfo ) ); 
#      ( componentInfo )->isPublicKey = ( ( componentKeyType ) ? 1 : 0 ); }
#

# C-macro not translated to Perl code but implemented apart: 
#   #define cryptDestroyComponents( componentInfo ) 
#    memset( ( componentInfo ), 0, sizeof( *componentInfo ) )
#

# Macros to set a component of a public key 

# C-macro not translated to Perl code but implemented apart: 
#   #define cryptSetComponent( destination, source, length ) 
#    { memcpy( ( destination ), ( source ), ( ( length ) + 7 ) >> 3 ); 
#      ( destination##Len ) = length; }
#

#****************************************************************************
#*                                                                           *
#*                               Status Codes                                *
#*                                                                           *
#****************************************************************************

# No error in function call 

	sub CRYPT_OK { 0 }
# No error 

#  Error in parameters passed to function.  The parentheses are to catch 
#  potential erroneous use in an expression 

	sub CRYPT_ERROR_PARAM1 { -1 }
# Bad argument, parameter 1 
	sub CRYPT_ERROR_PARAM2 { -2 }
# Bad argument, parameter 2 
	sub CRYPT_ERROR_PARAM3 { -3 }
# Bad argument, parameter 3 
	sub CRYPT_ERROR_PARAM4 { -4 }
# Bad argument, parameter 4 
	sub CRYPT_ERROR_PARAM5 { -5 }
# Bad argument, parameter 5 
	sub CRYPT_ERROR_PARAM6 { -6 }
# Bad argument, parameter 6 
	sub CRYPT_ERROR_PARAM7 { -7 }
# Bad argument, parameter 7 

# Errors due to insufficient resources 

	sub CRYPT_ERROR_MEMORY { -10 }
# Out of memory 
	sub CRYPT_ERROR_NOTINITED { -11 }
# Data has not been initialised 
	sub CRYPT_ERROR_INITED { -12 }
# Data has already been init'd 
	sub CRYPT_ERROR_NOSECURE { -13 }
# Opn.not avail.at requested sec.level 
	sub CRYPT_ERROR_RANDOM { -14 }
# No reliable random data available 
	sub CRYPT_ERROR_FAILED { -15 }
# Operation failed 
	sub CRYPT_ERROR_INTERNAL { -16 }
# Internal consistency check failed 

# Security violations 

	sub CRYPT_ERROR_NOTAVAIL { -20 }
# This type of opn.not available 
	sub CRYPT_ERROR_PERMISSION { -21 }
# No permiss.to perform this operation 
	sub CRYPT_ERROR_WRONGKEY { -22 }
# Incorrect key used to decrypt data 
	sub CRYPT_ERROR_INCOMPLETE { -23 }
# Operation incomplete/still in progress 
	sub CRYPT_ERROR_COMPLETE { -24 }
# Operation complete/can't continue 
	sub CRYPT_ERROR_TIMEOUT { -25 }
# Operation timed out before completion 
	sub CRYPT_ERROR_INVALID { -26 }
# Invalid/inconsistent information 
	sub CRYPT_ERROR_SIGNALLED { -27 }
# Resource destroyed by extnl.event 

# High-level function errors 

	sub CRYPT_ERROR_OVERFLOW { -30 }
# Resources/space exhausted 
	sub CRYPT_ERROR_UNDERFLOW { -31 }
# Not enough data available 
	sub CRYPT_ERROR_BADDATA { -32 }
# Bad/unrecognised data format 
	sub CRYPT_ERROR_SIGNATURE { -33 }
# Signature/integrity check failed 

# Data access function errors 

	sub CRYPT_ERROR_OPEN { -40 }
# Cannot open object 
	sub CRYPT_ERROR_READ { -41 }
# Cannot read item from object 
	sub CRYPT_ERROR_WRITE { -42 }
# Cannot write item to object 
	sub CRYPT_ERROR_NOTFOUND { -43 }
# Requested item not found in object 
	sub CRYPT_ERROR_DUPLICATE { -44 }
# Item already present in object 

# Data enveloping errors 

	sub CRYPT_ENVELOPE_RESOURCE { -50 }
# Need resource to proceed 

# Macros to examine return values 

# C-macro not translated to Perl code but implemented apart: 
#   #define cryptStatusError( status )  ( ( status ) < CRYPT_OK )
#
# C-macro not translated to Perl code but implemented apart: 
#   #define cryptStatusOK( status )     ( ( status ) == CRYPT_OK )
#

#****************************************************************************
#*                                                                           *
#*                                   General Functions                       *
#*                                                                           *
#****************************************************************************

# The following is necessary to stop C++ name mangling 


# Initialise and shut down cryptlib 

# C_CHECK_RETVAL 
###C_RET cryptInit( void );
###C_RET cryptEnd( void );
#
# Query cryptlibs capabilities 

# C_CHECK_RETVAL 
###C_RET cryptQueryCapability( C_IN CRYPT_ALGO_TYPE cryptAlgo,
##                            C_OUT_OPT CRYPT_QUERY_INFO C_PTR cryptQueryInfo );
##
# Create and destroy an encryption context 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCreateContext( C_OUT CRYPT_CONTEXT C_PTR cryptContext,
##                          C_IN CRYPT_USER cryptUser,
##                          C_IN CRYPT_ALGO_TYPE cryptAlgo );
###C_RET cryptDestroyContext( C_IN CRYPT_CONTEXT cryptContext );
#
# Generic "destroy an object" function 

#C_RET cryptDestroyObject( C_IN CRYPT_HANDLE cryptObject );
#
# Generate a key into a context 

# C_CHECK_RETVAL 
###C_RET cryptGenerateKey( C_IN CRYPT_CONTEXT cryptContext );
##
# Encrypt/decrypt/hash a block of memory 

# C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptEncrypt( C_IN CRYPT_CONTEXT cryptContext, C_INOUT void C_PTR buffer,
##                    C_IN int length );
### C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptDecrypt( C_IN CRYPT_CONTEXT cryptContext, C_INOUT void C_PTR buffer,
##                    C_IN int length );
##
# Get/set/delete attribute functions 

#C_RET cryptSetAttribute( C_IN CRYPT_HANDLE cryptHandle,
#                         C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
#                         C_IN int value );
## C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptSetAttributeString( C_IN CRYPT_HANDLE cryptHandle,
##                               C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
##                               C_IN void C_PTR value, C_IN int valueLength );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptGetAttribute( C_IN CRYPT_HANDLE cryptHandle,
##                         C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
##                         C_OUT int C_PTR value );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 4 ) ) 
###C_RET cryptGetAttributeString( C_IN CRYPT_HANDLE cryptHandle,
##                               C_IN CRYPT_ATTRIBUTE_TYPE attributeType,
##                               C_OUT_OPT void C_PTR value,
##                               C_OUT int C_PTR valueLength );
###C_RET cryptDeleteAttribute( C_IN CRYPT_HANDLE cryptHandle,
#                            C_IN CRYPT_ATTRIBUTE_TYPE attributeType );
#
#  Oddball functions: Add random data to the pool, query an encoded signature
#  or key data.  These are due to be replaced once a suitable alternative can
#  be found 

#C_RET cryptAddRandom( C_IN void C_PTR randomData, C_IN int randomDataLength );
## C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 3 ) ) 
###C_RET cryptQueryObject( C_IN void C_PTR objectData,
##                        C_IN int objectDataLength,
##                        C_OUT CRYPT_OBJECT_INFO C_PTR cryptObjectInfo );
##
#****************************************************************************
#*                                                                           *
#*                           Mid-level Encryption Functions                  *
#*                                                                           *
#****************************************************************************

# Export and import an encrypted session key 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptExportKey( C_OUT_OPT void C_PTR encryptedKey,
##                      C_IN int encryptedKeyMaxLength,
##                      C_OUT int C_PTR encryptedKeyLength,
##                      C_IN CRYPT_HANDLE exportKey,
##                      C_IN CRYPT_CONTEXT sessionKeyContext );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptExportKeyEx( C_OUT_OPT void C_PTR encryptedKey,
##                        C_IN int encryptedKeyMaxLength,
##                        C_OUT int C_PTR encryptedKeyLength,
##                        C_IN CRYPT_FORMAT_TYPE formatType,
##                        C_IN CRYPT_HANDLE exportKey,
##                        C_IN CRYPT_CONTEXT sessionKeyContext );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptImportKey( C_IN void C_PTR encryptedKey,
##                      C_IN int encryptedKeyLength,
##                      C_IN CRYPT_CONTEXT importKey,
##                      C_IN CRYPT_CONTEXT sessionKeyContext );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptImportKeyEx( C_IN void C_PTR encryptedKey,
##                        C_IN int encryptedKeyLength,
##                        C_IN CRYPT_CONTEXT importKey,
##                        C_IN CRYPT_CONTEXT sessionKeyContext,
##                        C_OUT_OPT CRYPT_CONTEXT C_PTR returnedContext );
##
# Create and check a digital signature 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptCreateSignature( C_OUT_OPT void C_PTR signature,
##                            C_IN int signatureMaxLength,
##                            C_OUT int C_PTR signatureLength,
##                            C_IN CRYPT_CONTEXT signContext,
##                            C_IN CRYPT_CONTEXT hashContext );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptCreateSignatureEx( C_OUT_OPT void C_PTR signature,
##                              C_IN int signatureMaxLength,
##                              C_OUT int C_PTR signatureLength,
##                              C_IN CRYPT_FORMAT_TYPE formatType,
##                              C_IN CRYPT_CONTEXT signContext,
##                              C_IN CRYPT_CONTEXT hashContext,
##                              C_IN CRYPT_CERTIFICATE extraData );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCheckSignature( C_IN void C_PTR signature,
##                           C_IN int signatureLength,
##                           C_IN CRYPT_HANDLE sigCheckKey,
##                           C_IN CRYPT_CONTEXT hashContext );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCheckSignatureEx( C_IN void C_PTR signature,
##                             C_IN int signatureLength,
##                             C_IN CRYPT_HANDLE sigCheckKey,
##                             C_IN CRYPT_CONTEXT hashContext,
##                             C_OUT_OPT CRYPT_HANDLE C_PTR extraData );
##
#****************************************************************************
#*                                                                           *
#*                                   Keyset Functions                        *
#*                                                                           *
#****************************************************************************

# Open and close a keyset 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 4 ) ) 
###C_RET cryptKeysetOpen( C_OUT CRYPT_KEYSET C_PTR keyset,
##                       C_IN CRYPT_USER cryptUser,
##                       C_IN CRYPT_KEYSET_TYPE keysetType,
##                       C_IN C_STR name, C_IN CRYPT_KEYOPT_TYPE options );
###C_RET cryptKeysetClose( C_IN CRYPT_KEYSET keyset );
#
# Get a key from a keyset or device 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptGetPublicKey( C_IN CRYPT_KEYSET keyset,
##                         C_OUT CRYPT_CONTEXT C_PTR cryptContext,
##                         C_IN CRYPT_KEYID_TYPE keyIDtype,
##                         C_IN_OPT C_STR keyID );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) 
###C_RET cryptGetPrivateKey( C_IN CRYPT_KEYSET keyset,
##                          C_OUT CRYPT_CONTEXT C_PTR cryptContext,
##                          C_IN CRYPT_KEYID_TYPE keyIDtype,
##                          C_IN C_STR keyID, C_IN_OPT C_STR password );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) 
#C_RET cryptGetKey( C_IN CRYPT_KEYSET keyset,
#                   C_OUT CRYPT_CONTEXT C_PTR cryptContext,
#                   C_IN CRYPT_KEYID_TYPE keyIDtype, C_IN C_STR keyID, 
#                   C_IN_OPT C_STR password );

# Add/delete a key to/from a keyset or device 

# C_CHECK_RETVAL 
###C_RET cryptAddPublicKey( C_IN CRYPT_KEYSET keyset,
##                         C_IN CRYPT_CERTIFICATE certificate );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptAddPrivateKey( C_IN CRYPT_KEYSET keyset,
##                          C_IN CRYPT_HANDLE cryptKey,
##                          C_IN C_STR password );
### C_NONNULL_ARG( ( 3 ) ) 
###C_RET cryptDeleteKey( C_IN CRYPT_KEYSET keyset,
##                      C_IN CRYPT_KEYID_TYPE keyIDtype,
##                      C_IN C_STR keyID );
##
#****************************************************************************
#*                                                                           *
#*                               Certificate Functions                       *
#*                                                                           *
#****************************************************************************

# Create/destroy a certificate 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCreateCert( C_OUT CRYPT_CERTIFICATE C_PTR certificate,
##                       C_IN CRYPT_USER cryptUser,
##                       C_IN CRYPT_CERTTYPE_TYPE certType );
###C_RET cryptDestroyCert( C_IN CRYPT_CERTIFICATE certificate );
#
#  Get/add/delete certificate extensions.  These are direct data insertion
#  functions whose use is discouraged, so they fix the string at char *
#  rather than C_STR 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 3, 6 ) ) 
###C_RET cryptGetCertExtension( C_IN CRYPT_CERTIFICATE certificate,
##                             C_IN char C_PTR oid,
##                             C_OUT int C_PTR criticalFlag,
##                             C_OUT_OPT void C_PTR extension,
##                             C_IN int extensionMaxLength,
##                             C_OUT int C_PTR extensionLength );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) 
###C_RET cryptAddCertExtension( C_IN CRYPT_CERTIFICATE certificate,
##                             C_IN char C_PTR oid, C_IN int criticalFlag,
##                             C_IN void C_PTR extension,
##                             C_IN int extensionLength );
### C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptDeleteCertExtension( C_IN CRYPT_CERTIFICATE certificate,
##                                C_IN char C_PTR oid );
##
# Sign/sig.check a certificate/certification request 

# C_CHECK_RETVAL 
###C_RET cryptSignCert( C_IN CRYPT_CERTIFICATE certificate,
##                     C_IN CRYPT_CONTEXT signContext );
### C_CHECK_RETVAL 
###C_RET cryptCheckCert( C_IN CRYPT_CERTIFICATE certificate,
##                      C_IN CRYPT_HANDLE sigCheckKey );
##
# Import/export a certificate/certification request 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 4 ) ) 
###C_RET cryptImportCert( C_IN void C_PTR certObject,
##                       C_IN int certObjectLength,
##                       C_IN CRYPT_USER cryptUser,
##                       C_OUT CRYPT_CERTIFICATE C_PTR certificate );
### C_CHECK_RETVAL 
###C_RET cryptExportCert( C_OUT_OPT void C_PTR certObject,
##                       C_IN int certObjectMaxLength,
##                       C_OUT int C_PTR certObjectLength,
##                       C_IN CRYPT_CERTFORMAT_TYPE certFormatType,
##                       C_IN CRYPT_CERTIFICATE certificate );
##
# CA management functions 

# C_CHECK_RETVAL 
###C_RET cryptCAAddItem( C_IN CRYPT_KEYSET keyset,
##                      C_IN CRYPT_CERTIFICATE certificate );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptCAGetItem( C_IN CRYPT_KEYSET keyset,
##                      C_OUT CRYPT_CERTIFICATE C_PTR certificate,
##                      C_IN CRYPT_CERTTYPE_TYPE certType,
##                      C_IN CRYPT_KEYID_TYPE keyIDtype,
##                      C_IN_OPT C_STR keyID );
### C_NONNULL_ARG( ( 4 ) ) 
###C_RET cryptCADeleteItem( C_IN CRYPT_KEYSET keyset,
##                         C_IN CRYPT_CERTTYPE_TYPE certType,
##                         C_IN CRYPT_KEYID_TYPE keyIDtype,
##                         C_IN C_STR keyID );
### C_CHECK_RETVAL 
###C_RET cryptCACertManagement( C_OUT_OPT CRYPT_CERTIFICATE C_PTR certificate,
##                             C_IN CRYPT_CERTACTION_TYPE action,
##                             C_IN CRYPT_KEYSET keyset,
##                             C_IN CRYPT_CONTEXT caKey,
##                             C_IN CRYPT_CERTIFICATE certRequest );
##
#****************************************************************************
#*                                                                           *
#*                           Envelope and Session Functions                  *
#*                                                                           *
#****************************************************************************

# Create/destroy an envelope 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCreateEnvelope( C_OUT CRYPT_ENVELOPE C_PTR envelope,
##                           C_IN CRYPT_USER cryptUser,
##                           C_IN CRYPT_FORMAT_TYPE formatType );
###C_RET cryptDestroyEnvelope( C_IN CRYPT_ENVELOPE envelope );
#
# Create/destroy a session 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptCreateSession( C_OUT CRYPT_SESSION C_PTR session,
##                          C_IN CRYPT_USER cryptUser,
##                          C_IN CRYPT_SESSION_TYPE formatType );
###C_RET cryptDestroySession( C_IN CRYPT_SESSION session );
#
# Add/remove data to/from and envelope or session 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) 
###C_RET cryptPushData( C_IN CRYPT_HANDLE envelope, C_IN void C_PTR buffer,
##                     C_IN int length, C_OUT int C_PTR bytesCopied );
### C_CHECK_RETVAL 
###C_RET cryptFlushData( C_IN CRYPT_HANDLE envelope );
### C_CHECK_RETVAL C_NONNULL_ARG( ( 2, 4 ) ) 
###C_RET cryptPopData( C_IN CRYPT_HANDLE envelope, C_OUT void C_PTR buffer,
##                    C_IN int length, C_OUT int C_PTR bytesCopied );
##
#****************************************************************************
#*                                                                           *
#*                               Device Functions                            *
#*                                                                           *
#****************************************************************************

# Open and close a device 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1 ) ) 
###C_RET cryptDeviceOpen( C_OUT CRYPT_DEVICE C_PTR device,
##                       C_IN CRYPT_USER cryptUser,
##                       C_IN CRYPT_DEVICE_TYPE deviceType,
##                       C_IN_OPT C_STR name );
###C_RET cryptDeviceClose( C_IN CRYPT_DEVICE device );
#
# Query a devices capabilities 

# C_CHECK_RETVAL 
###C_RET cryptDeviceQueryCapability( C_IN CRYPT_DEVICE device,
##                                  C_IN CRYPT_ALGO_TYPE cryptAlgo,
##                                  C_OUT_OPT CRYPT_QUERY_INFO C_PTR cryptQueryInfo );
##
# Create an encryption context via the device 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 2 ) ) 
###C_RET cryptDeviceCreateContext( C_IN CRYPT_DEVICE device,
##                                C_OUT CRYPT_CONTEXT C_PTR cryptContext,
##                                C_IN CRYPT_ALGO_TYPE cryptAlgo );
##
#****************************************************************************
#*                                                                           *
#*                           User Management Functions                       *
#*                                                                           *
#****************************************************************************

# Log on and off (create/destroy a user object) 

# C_CHECK_RETVAL C_NONNULL_ARG( ( 1, 2, 3 ) ) 
###C_RET cryptLogin( C_OUT CRYPT_USER C_PTR user,
##                  C_IN C_STR name, C_IN C_STR password );
###C_RET cryptLogout( C_IN CRYPT_USER user );
#
#****************************************************************************
#*                                                                           *
#*                           User Interface Functions                        *
#*                                                                           *
#****************************************************************************



#
# *****************************************************************************
# *                                                                           *
# *                    End of Perl Functions                                  *
# *                                                                           *
# *****************************************************************************
#

1; ##### End-of perl header file!

