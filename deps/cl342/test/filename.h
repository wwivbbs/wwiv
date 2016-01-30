/****************************************************************************
*																			*
*						cryptlib Test Data Filenames						*
*						Copyright Peter Gutmann 1995-2007					*
*																			*
****************************************************************************/

/* The names of the test key and certificate files.  For flat filesystems we
   give the test files names starting with 'z' so they're easier to find */

/****************************************************************************
*																			*
*									AS/400									*
*																			*
****************************************************************************/

#if defined( __OS400__ )

#define TEST_PRIVKEY_FILE			"testlib/zkeytest"
#define TEST_PRIVKEY_TMP_FILE		"testlib/zkeytstt"
#define TEST_PRIVKEY_ALT_FILE		"testlib/zkeytsta"
#define CA_PRIVKEY_FILE				"testlib/zkeyca"
#define ICA_PRIVKEY_FILE			"testlib/zkeycai"
#define SCEPCA_PRIVKEY_FILE			"testlib/zkeycas"
#define USER_PRIVKEY_FILE			"testlib/zkeyuser"
#define DUAL_PRIVKEY_FILE			"testlib/zkeydual"
#define RENEW_PRIVKEY_FILE			"testlib/zkeyrene"
#define P15_FILE_TEMPLATE			"testlib/zkeyp15%d"
#define CMP_PRIVKEY_FILE_TEMPLATE	"testlib/zkeycmp"
#define PNP_PRIVKEY_FILE			"testlib/zkeypnpu"
#define PNPCA_PRIVKEY_FILE			"testlib/zkeypnpc"
#define SERVER_PRIVKEY_FILE_TEMPLATE "testlib/zkeysrv%d"
#define SERVER_ECPRIVKEY_FILE_TEMPLATE "testlib/zkeysrp%d"
#define SSH_PRIVKEY_FILE_TEMPLATE	"testlib/zkeyssh%d"
#define SSL_CLI_PRIVKEY_FILE		"testlib/zkeysslc"
#define TSA_PRIVKEY_FILE			"testlib/zkeytsa"
#define MISC_PRIVKEY_FILE_TEMPLATE	"testlib/zkeymis%d"
#define PKCS12_FILE_TEMPLATE		"testlib/zkeyp12%d"

#define PGP_PUBKEY_FILE				"testlib/zpubring"
#define PGP_PRIVKEY_FILE			"testlib/zsecring"
#define OPENPGP_PUBKEY_FILE			"testlib/zpubringg"
#define OPENPGP_PRIVKEY_HASH_FILE	"testlib/zsechash"
#define OPENPGP_PUBKEY_AES_FILE		"testlib/zpubaes"
#define OPENPGP_PRIVKEY_AES_FILE	"testlib/zsecaes"
#define OPENPGP_PRIVKEY_CAST_FILE	"testlib/zseccast"
#define OPENPGP_PUBKEY_RSA_FILE		"testlib/zpubrsa"
#define OPENPGP_PRIVKEY_RSA_FILE	"testlib/zsecrsa"
#define OPENPGP_PRIVKEY_PART_FILE	"testlib/zsecpart"
#define OPENPGP_PUBKEY_MULT_FILE	"testlib/zpubmult"
#define NAIPGP_PUBKEY_FILE			"testlib/zpubnai"
#define NAIPGP_PRIVKEY_FILE			"testlib/zsecnai"

#define CERT_FILE_TEMPLATE			"testlib/zcert%02d"
#define BASE64CERT_FILE_TEMPLATE	"testlib/zcerta%d"
#define ECC_CERT_FILE_TEMPLATE		"testlib/zeccert%02d"
#define BROKEN_CERT_FILE			"testlib/zcertb"
#define BROKEN_USER_CERT_FILE		"testlib/zcertbus"
#define BROKEN_CA_CERT_FILE			"testlib/zcertbca"
#define CERTREQ_FILE_TEMPLATE		"testlib/zcertreq%d"
#define CRL_FILE_TEMPLATE			"testlib/zcrl%d"
#define CERTCHAIN_FILE_TEMPLATE		"testlib/zcertchn%d"
#define BASE64CERTCHAIN_FILE_TEMPLATE "testlib/zcertcha%d"
#define PATHTEST_FILE_TEMPLATE		"testlib/zntest%d"
#define PADTEST_FILE_TEMPLATE		"testlib/zbadsig%d"
#define SSHKEY_FILE_TEMPLATE		"testlib/zsshkey%d"
#define PGPKEY_FILE_TEMPLATE		"testlib/zpgpkey%d"
#define PGPASCKEY_FILE_TEMPLATE		"testlib/zpgpkea%d"
#define NOCHAIN_EE_FILE				"testlib/znochn_ee"
#define NOCHAIN_CA_FILE				"testlib/znochn_ca"
#define RTCS_OK_FILE				"testlib/zrtcsok"
#define OCSP_OK_FILE				"testlib/zocsprok"
#define OCSP_REV_FILE				"testlib/zocsprre"
#define OCSP_CA_FILE				"testlib/zocspca"
#define CRLCERT_FILE_TEMPLATE		"testlib/zcrlcrt%d"
#define RTCS_FILE_TEMPLATE			"testlib/zrtcsee%do"
#define OCSP_CA_FILE_TEMPLATE		"testlib/zocspca%d"
#define OCSP_EEOK_FILE_TEMPLATE		"testlib/zocspok%d"
#define OCSP_EEREV_FILE_TEMPLATE	"testlib/zocspre%d"
#define CMP_CA_FILE_TEMPLATE		"testlib/zcmpca%d"
#define SCEP_CA_FILE_TEMPLATE		"testlib/zscepca%d"

#define CMS_ENC_FILE_TEMPLATE		"testlib/zspwenc%d"
#define SMIME_SIG_FILE_TEMPLATE		"testlib/zsigned%d"
#define SMIME_ENV_FILE_TEMPLATE		"testlib/zsenvel%d"
#define PGP_ENC_FILE_TEMPLATE		"testlib/zconven%d"
#define PGP_PKE_FILE_TEMPLATE		"testlib/zpgpenc%d"
#define OPENPGP_PKE_FILE_TEMPLATE	"testlib/zgpgenc%d"
#define PGP_SIG_FILE_TEMPLATE		"testlib/zsigned%d"
#define PGP_COPR_FILE_TEMPLATE		"testlib/zcopr%d"

#define TESTDATA_FILE_TEMPLATE		"testlib/ztestd%d"
#define COMPRESS_FILE				"test/filename"

/****************************************************************************
*																			*
*							Macintosh pre-OS X								*
*																			*
****************************************************************************/

#elif defined( __MWERKS__ ) || defined( SYMANTEC_C ) || defined( __MRC__ )

#define TEST_PRIVKEY_FILE			":test:keys:test.p15"
#define TEST_PRIVKEY_TMP_FILE		":test:keys:test_tmp.p15"
#define TEST_PRIVKEY_ALT_FILE		":test:keys:test.p12"
#define CA_PRIVKEY_FILE				":test:keys:ca.p15"
#define ICA_PRIVKEY_FILE			":test:keys:ca_int.p15"
#define SCEPCA_PRIVKEY_FILE			":test:keys:ca_cert.p15"
#define USER_PRIVKEY_FILE			":test:keys:user.p15"
#define DUAL_PRIVKEY_FILE			":test:keys:dual.p15"
#define RENEW_PRIVKEY_FILE			":test:keys:renewed.p15"
#define P15_FILE_TEMPLATE			":test:keys:pkcs15_%d.p15"
#define CMP_PRIVKEY_FILE_TEMPLATE	":test:keys:cmp%d.p15"
#define PNP_PRIVKEY_FILE			":test:keys:pnp_user.p15"
#define PNPCA_PRIVKEY_FILE			":test:keys:pnp_ca.p15"
#define SERVER_PRIVKEY_FILE_TEMPLATE ":test:keys:server%d.p15"
#define SERVER_ECPRIVKEY_FILE_TEMPLATE ":test:keys:serverp%d.p15"
#define SSH_PRIVKEY_FILE_TEMPLATE	":test:keys:ssh%d.p15"
#define SSL_CLI_PRIVKEY_FILE		":test:keys:ssl_cli.p15"
#define TSA_PRIVKEY_FILE			":test:keys:tsa.p15"
#define MISC_PRIVKEY_FILE_TEMPLATE	":test:keys:misc%d.p15"
#define PKCS12_FILE_TEMPLATE		":test:keys:pkcs12_%d.p12"

#define PGP_PUBKEY_FILE				":test:pgp:pubring.pgp"
#define PGP_PRIVKEY_FILE			":test:pgp:secring.pgp"
#define OPENPGP_PUBKEY_HASH_FILE	":test:pgp:pub_hash.gpg"
#define OPENPGP_PRIVKEY_HASH_FILE	":test:pgp:sec_hash.gpg"
#define OPENPGP_PUBKEY_AES_FILE		":test:pgp:pub_aes.pkr"
#define OPENPGP_PRIVKEY_AES_FILE	":test:pgp:sec_aes.skr"
#define OPENPGP_PRIVKEY_CAST_FILE	":test:pgp:sec_cast.gpg"
#define OPENPGP_PUBKEY_RSA_FILE		":test:pgp:pub_rsa.gpg"
#define OPENPGP_PRIVKEY_RSA_FILE	":test:pgp:sec_rsa.gpg"
#define OPENPGP_PRIVKEY_PART_FILE	":test:pgp:sec_part.gpg"
#define OPENPGP_PUBKEY_MULT_FILE	":test:pgp:pub_mult.gpg"
#define NAIPGP_PUBKEY_FILE			":test:pgp:pub_nai.pkr"
#define NAIPGP_PRIVKEY_FILE			":test:pgp:sec_nai.skr"

#define CERT_FILE_TEMPLATE			":test:certs:cert%02d.der"
#define BASE64CERT_FILE_TEMPLATE	":test:certs:cert%d.asc"
#define ECC_CERT_FILE_TEMPLATE		":test:certs:eccert%02d.der"
#define BROKEN_CERT_FILE			":test:certs:broken.der"
#define BROKEN_USER_CERT_FILE		":test:certs:broken_ee.der"
#define BROKEN_CA_CERT_FILE			":test:certs:broken_ca.der"
#define CERTREQ_FILE_TEMPLATE		":test:certs:certreq%d.der"
#define CRL_FILE_TEMPLATE			":test:certs:crl%d.crl"
#define CERTCHAIN_FILE_TEMPLATE		":test:certs:certchn%d.der"
#define BASE64CERTCHAIN_FILE_TEMPLATE ":test:certs:certchn%d.asc"
#define PATHTEST_FILE_TEMPLATE		":test:nist:ntest%d.p7s"
#define PADTEST_FILE_TEMPLATE		":test:certs:badsig%d.der"
#define SSHKEY_FILE_TEMPLATE		":test:misc:sshkey%d.asc"
#define PGPKEY_FILE_TEMPLATE		":test:pgp:pubkey%d.pgp"
#define PGPASCKEY_FILE_TEMPLATE		":test:pgp:pubkey%d.asc"
#define NOCHAIN_EE_FILE				":test:misc:nochn_ee.der"
#define NOCHAIN_CA_FILE				":test:misc:nochn_ca.der"
#define RTCS_OK_FILE				":test:misc:rtcsrok.der"
#define OCSP_OK_FILE				":test:session:ocspr_ok.der"
#define OCSP_REV_FILE				":test:session:ocspr_re.der"
#define OCSP_CA_FILE				":test:session:ocspca.der"
#define CRLCERT_FILE_TEMPLATE		":test:misc:crl_cert%d.der"
#define RTCS_FILE_TEMPLATE			":test:misc:rtcs_ee%do.der"
#define OCSP_CA_FILE_TEMPLATE		":test:session:ocsp_ca%d.der"
#define OCSP_EEOK_FILE_TEMPLATE		":test:session:ocsp_ok%d.der"
#define OCSP_EEREV_FILE_TEMPLATE	":test:session:ocsp_re%d.der"
#define CMP_CA_FILE_TEMPLATE		":test:session:cmp_ca%d.der"
#define SCEP_CA_FILE_TEMPLATE		":test:session:scep_ca%d.der"

#define CMS_ENC_FILE_TEMPLATE		":test:smime:pwenc%d.p7m"
#define SMIME_SIG_FILE_TEMPLATE		":test:smime:signed%d.p7s"
#define SMIME_ENV_FILE_TEMPLATE		":test:smime:envel%d.p7m"
#define PGP_ENC_FILE_TEMPLATE		":test:pgp:conv_enc%d.pgp"
#define PGP_PKE_FILE_TEMPLATE		":test:pgp:pgp_enc%d.pgp"
#define OPENPGP_PKE_FILE_TEMPLATE	":test:pgp:gpg_enc%d.gpg"
#define PGP_SIG_FILE_TEMPLATE		":test:pgp:signed%d.pgp"
#define PGP_COPR_FILE_TEMPLATE		":test:pgp:copr%d.pgp"

#define TESTDATA_FILE_TEMPLATE		":test:misc:testdata%d.dat"
#define COMPRESS_FILE				":test:filename.h"

/****************************************************************************
*																			*
*							MVS with DDNAME I/O								*
*																			*
****************************************************************************/

#elif defined( DDNAME_IO )

#define TEST_PRIVKEY_FILE			"DD:CLBTEST"
#define TEST_PRIVKEY_TMP_FILE		"DD:CLBTESTT"
#define TEST_PRIVKEY_ALT_FILE		"DD:CLBTESTA"
#define CA_PRIVKEY_FILE				"DD:CLBP15(KEYCA)"
#define ICA_PRIVKEY_FILE			"DD:CLBP15(KEYCAI)"
#define SCEPCA_PRIVKEY_FILE			"DD:CLBP15(KEYCAS)"
#define USER_PRIVKEY_FILE			"DD:CLBP15(KEYUSER)"
#define DUAL_PRIVKEY_FILE			"DD:CLBP15(KEYDUAL)"
#define RENEW_PRIVKEY_FILE			"DD:CLBP15(KEYRENE)"
#define P15_FILE_TEMPLATE			"DD:CLBP15(KEYP15%d)"
#define CMP_PRIVKEY_FILE_TEMPLATE	"DD:CLBP15(KEYCMP%d)"
#define PNP_PRIVKEY_FILE			"DD:CLBP15(KEYPNPU)"
#define PNPCA_PRIVKEY_FILE			"DD:CLBP15(KEYPNPC)"
#define SERVER_PRIVKEY_FILE_TEMPLATE "DD:CLBP15(KEYSRV%d)"
#define SERVER_ECPRIVKEY_FILE_TEMPLATE "DD:CLBP15(KEYSRP%d)"
#define SSH_PRIVKEY_FILE_TEMPLATE	"DD:CLBP15(KEYSSH%d)"
#define SSL_CLI_PRIVKEY_FILE		"DD:CLBP15(KEYSSLC)"
#define TSA_PRIVKEY_FILE			"DD:CLBP15(KEYTSA)"
#define MISC_PRIVKEY_FILE_TEMPLATE	"DD:CLBP15(KEYMIS%d)"
#define PKCS12_FILE_TEMPLATE		"DD:CLBP12(KEYP12%d)"

#define PGP_PUBKEY_FILE				"DD:CLBPGP(PUBRING)"
#define PGP_PRIVKEY_FILE			"DD:CLBPGP(SECRING)"
#define OPENPGP_PUBKEY_HASH_FILE	"DD:CLBGPG(PUBHASH)"
#define OPENPGP_PRIVKEY_HASH_FILE	"DD:CLBGPG(SECHASH)"
#define OPENPGP_PUBKEY_AES_FILE		"DD:CLBPKR(PUBAES)"
#define OPENPGP_PRIVKEY_AES_FILE	"DD:CLBSKR(SECAES)"
#define OPENPGP_PRIVKEY_CAST_FILE	"DD:CLBSKR(SECCAS)"
#define OPENPGP_PUBKEY_RSA_FILE		"DD:CLBGPG(PUBRSA)"
#define OPENPGP_PRIVKEY_RSA_FILE	"DD:CLBGPG(SECRSA)"
#define OPENPGP_PRIVKEY_PART_FILE	"DD:CLBGPG(SECPART)"
#define OPENPGP_PUBKEY_MULT_FILE	"DD:CLBGPG(PUBMULT)"
#define NAIPGP_PUBKEY_FILE			"DD:CLBPKR(PUBNAI)"
#define NAIPGP_PRIVKEY_FILE			"DD:CLBSKR(SECNAI)"

#define CERT_FILE_TEMPLATE			"DD:CLBDER(CERT%02d)"
#define BASE64CERT_FILE_TEMPLATE	"DD:CLBDER(CERT%d)"
#define ECC_CERT_FILE_TEMPLATE		"DD:CLBDER(ECCERT%02d)"
#define BROKEN_CERT_FILE			"DD:CLBDER(CERTB)"
#define BROKEN_USER_CERT_FILE		"DD:CLBDER(CERTBUS)"
#define BROKEN_CA_CERT_FILE			"DD:CLBDER(CERTBCA)"
#define CERTREQ_FILE_TEMPLATE		"DD:CLBDER(CERTREQ%d)"
#define CRL_FILE_TEMPLATE			"DD:CLBDER(CRL%d)"
#define CERTCHAIN_FILE_TEMPLATE		"DD:CLBDER(CERTCHN%d)"
#define BASE64CERTCHAIN_FILE_TEMPLATE "DD:CLBDER(CERT%d)"
#define PATHTEST_FILE_TEMPLATE		"DD:CLBDER(NTEST%d)"
#define PADTEST_FILE_TEMPLATE		"DD:CLBDER(BADSIG%d)"
#define SSHKEY_FILE_TEMPLATE		"DD:CLBDER(SSHKEY%d)"
#define PGPKEY_FILE_TEMPLATE		"DD:CLBDER(PGPKEY%d)"
#define PGPASCKEY_FILE_TEMPLATE		"DD:CLBDER(PGPKEA%d)"
#define NOCHAIN_EE_FILE				"DD:CLBDER(NOCHNEE)"
#define NOCHAIN_CA_FILE				"DD:CLBDER(NOCHNCA)"
#define RTCS_OK_FILE				"DD:CLBDER(RTCSROK)"
#define OCSP_OK_FILE				"DD:CLBDER(OCSPROK)"
#define OCSP_REV_FILE				"DD:CLBDER(OCSPRRE)"
#define OCSP_CA_FILE				"DD:CLBDER(OCSPCA)"
#define CRLCERT_FILE_TEMPLATE		"DD:CLBDER(CRLCERT%d)"
#define RTCS_FILE_TEMPLATE			"DD:CLBDER(RTCSEE%dO)"
#define OCSP_CA_FILE_TEMPLATE		"DD:CLBDER(OCSPCA%d)"
#define OCSP_EEOK_FILE_TEMPLATE		"DD:CLBDER(OCSPOK%d)"
#define OCSP_EEREV_FILE_TEMPLATE	"DD:CLBDER(OCSPRE%d)"
#define CMP_CA_FILE_TEMPLATE		"DD:CLBDER(CMPCA%d)"
#define SCEP_CA_FILE_TEMPLATE		"DD:CLBDER(SCEPCA%d)"

#define CMS_ENC_FILE_TEMPLATE		"DD:CLBP7M(PWENC%d)"
#define SMIME_SIG_FILE_TEMPLATE		"DD:CLBP7S(SIGNED%d)"
#define SMIME_ENV_FILE_TEMPLATE		"DD:CLBP7M(ENVEL%d)"
#define PGP_ENC_FILE_TEMPLATE		"DD:CLBPGP(CONVEN%d)"
#define PGP_PKE_FILE_TEMPLATE		"DD:CLBPGP(PGPENC%d)"
#define OPENPGP_PKE_FILE_TEMPLATE	"DD:CLBGPG(GPGENC%d)"
#define PGP_SIG_FILE_TEMPLATE		"DD:CLBPGP(SIGNED%d)"
#define PGP_COPR_FILE_TEMPLATE		"DD:CLBPGP(COPR%d)"

#define TESTDATA_FILE_TEMPLATE		"DD:CLBCMP(TESTD%d)"
#define COMPRESS_FILE				"DD:CLBCMP(FILENAME)"

/****************************************************************************
*																			*
*									Nucleus									*
*																			*
****************************************************************************/

/* Generic embedded system using the FAT filesystem */

#elif defined (__Nucleus__)

#define TEST_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\test.p15" )
#define TEST_PRIVKEY_TMP_FILE		TEXT( "c:\\test\\keys\\test_tmp.p15" )
#define TEST_PRIVKEY_ALT_FILE		TEXT( "c:\\test\\keys\\test.p12" )
#define CA_PRIVKEY_FILE				TEXT( "c:\\test\\keys\\ca.p15" )
#define ICA_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\ca_int.p15" )
#define SCEPCA_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\ca_scep.p15" )
#define USER_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\user.p15" )
#define DUAL_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\dual.p15" )
#define RENEW_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\renewed.p15" )
#define P15_FILE_TEMPLATE			TEXT( "c:\\test\\keys\\pkcs15_%d.p15" )
#define CMP_PRIVKEY_FILE_TEMPLATE	TEXT( "c:\\test\\keys\\cmp%d.p15" )
#define PNP_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\pnp_user.p15" )
#define PNPCA_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\pnp_ca.p15" )
#define SERVER_PRIVKEY_FILE_TEMPLATE TEXT( "c:\\test\\keys\\server%d.p15" )
#define SERVER_ECPRIVKEY_FILE_TEMPLATE TEXT( "c:\\test\\keys\\serverp%d.p15" )
#define SSH_PRIVKEY_FILE_TEMPLATE	TEXT( "c:\\test\\keys\\ssh%d.p15" )
#define SSL_CLI_PRIVKEY_FILE		TEXT( "c:\\test\\keys\\ssl_cli.p15" )
#define TSA_PRIVKEY_FILE			TEXT( "c:\\test\\keys\\tsa.p15" )
#define MISC_PRIVKEY_FILE_TEMPLATE	TEXT( "c:\\test\\keys\\misc%d.p15" )
#define PKCS12_FILE_TEMPLATE		TEXT( "c:\\test\\keys\\pkcs12_%d.p12" )

#define PGP_PUBKEY_FILE				TEXT( "c:\\test\\pgp\\pubring.pgp" )
#define PGP_PRIVKEY_FILE			TEXT( "c:\\test\\pgp\\secring.pgp" )
#define OPENPGP_PUBKEY_HASH_FILE	TEXT( "c:\\test\\pgp\\pub_hash.gpg" )
#define OPENPGP_PRIVKEY_HASH_FILE	TEXT( "c:\\test\\pgp\\sec_hash.gpg" )
#define OPENPGP_PUBKEY_AES_FILE		TEXT( "c:\\test\\pgp\\pub_aes.pkr" )
#define OPENPGP_PRIVKEY_AES_FILE	TEXT( "c:\\test\\pgp\\sec_aes.skr" )
#define OPENPGP_PUBKEY_RSA_FILE		TEXT( "c:\\test\\pgp\\pub_rsa.gpg" )
#define OPENPGP_PRIVKEY_RSA_FILE	TEXT( "c:\\test\\pgp\\sec_rsa.gpg" )
#define OPENPGP_PRIVKEY_PART_FILE	TEXT( "c:\\test\\pgp\\sec_part.gpg" )
#define NAIPGP_PUBKEY_FILE			TEXT( "c:\\test\\pgp\\pub_nai.pkr" )
#define NAIPGP_PRIVKEY_FILE			TEXT( "c:\\test\\pgp\\sec_nai.skr" )

#define CERT_FILE_TEMPLATE			TEXT( "c:\\test\\certs\\cert%02d.der" )
#define BASE64CERT_FILE_TEMPLATE	TEXT( "c:\\test\\certs\\cert%d.asc" )
#define ECC_CERT_FILE_TEMPLATE		TEXT( "c:\\test\\certs\\eccert%02d.der" )
#define BROKEN_CERT_FILE			TEXT( "c:\\test\\certs\\broken.der" )
#define BROKEN_USER_CERT_FILE		TEXT( "c:\\test\\certs\\broken_ee.der" )
#define BROKEN_CA_CERT_FILE			TEXT( "c:\\test\\certs\\broken_ca.der" )
#define CERTREQ_FILE_TEMPLATE		TEXT( "c:\\test\\certs\\certreq%d.der" )
#define CRL_FILE_TEMPLATE			TEXT( "c:\\test\\certs\\crl%d.crl" )
#define CERTCHAIN_FILE_TEMPLATE		TEXT( "c:\\test\\certs\\certchn%d.der" )
#define BASE64CERTCHAIN_FILE_TEMPLATE TEXT( "c:\\test\\certs\\certchn%d.asc" )
#define PATHTEST_FILE_TEMPLATE		TEXT( "c:\\test\\nist\\test%d.p7s" )
#define PADTEST_FILE_TEMPLATE		TEXT( "c:\\test\\certs\\bad_sig%d.der" )
#define SSHKEY_FILE_TEMPLATE		TEXT( "c:\\test\\misc\\sshkey%d.asc" )
#define PGPKEY_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\pubkey%d.pgp" )
#define PGPASCKEY_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\pubkey%d.asc" )
#define NOCHAIN_EE_FILE				TEXT( "c:\\test\\misc\\nochn_ee.der" )
#define NOCHAIN_CA_FILE				TEXT( "c:\\test\\misc\\nochn_ca.der" )
#define RTCS_OK_FILE				TEXT( "c:\\test\\misc\\rtcsrok.der" )
#define OCSP_OK_FILE				TEXT( "c:\\test\\session\\ocspr_ok.der" )
#define OCSP_REV_FILE				TEXT( "c:\\test\\session\\ocspr_re.der" )
#define OCSP_CA_FILE				TEXT( "c:\\test\\session\\ocspca.der" )
#define CRLCERT_FILE_TEMPLATE		TEXT( "c:\\test\\misc\\crl_cert%d.der" )
#define RTCS_FILE_TEMPLATE			TEXT( "c:\\test\\misc\\rtcs_ee%do.der" )
#define OCSP_CA_FILE_TEMPLATE		TEXT( "c:\\test\\session\\ocsp_ca%d.der" )
#define OCSP_EEOK_FILE_TEMPLATE		TEXT( "c:\\test\\session\\ocsp_ok%d.der" )
#define OCSP_EEREV_FILE_TEMPLATE	TEXT( "c:\\test\\session\\ocsp_re%d.der" )
#define CMP_CA_FILE_TEMPLATE		TEXT( "c:\\test\\session\\cmp_ca%d.der" )
#define SCEP_CA_FILE_TEMPLATE		TEXT( "c:\\test\\session\\scep_ca%d.der" )

#define CMS_ENC_FILE_TEMPLATE		TEXT( "c:\\test\\smime\\pw_enc%d.p7m" )
#define SMIME_SIG_FILE_TEMPLATE		TEXT( "c:\\test\\smime\\signed%d.p7s" )
#define SMIME_ENV_FILE_TEMPLATE		TEXT( "c:\\test\\smime\\envel%d.p7m" )
#define PGP_ENC_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\conv_enc%d.pgp" )
#define PGP_PKE_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\pgp_enc%d.pgp" )
#define OPENPGP_PKE_FILE_TEMPLATE	TEXT( "c:\\test\\pgp\\gpg_enc%d.gpg" )
#define PGP_SIG_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\signed%d.pgp" )
#define PGP_COPR_FILE_TEMPLATE		TEXT( "c:\\test\\pgp\\copr%d.pgp" )

#define TESTDATA_FILE_TEMPLATE		TEXT( "c:\\test\\misc\\testdata%d.dat" )
#define COMPRESS_FILE				TEXT( "c:\\test\\filename.h" )

/****************************************************************************
*																			*
*									VM/CMS									*
*																			*
****************************************************************************/

#elif defined( __VMCMS__ )

#define TEST_PRIVKEY_FILE			"zkeytest.p15"
#define TEST_PRIVKEY_TMP_FILE		"zkeytstt.p15"
#define TEST_PRIVKEY_ALT_FILE		"zkeytest.p12"
#define CA_PRIVKEY_FILE				"zkeyca.p15"
#define ICA_PRIVKEY_FILE			"zkeycai.p15"
#define SCEPCA_PRIVKEY_FILE			"zkeycas.p15"
#define USER_PRIVKEY_FILE			"zkeyuser.p15"
#define DUAL_PRIVKEY_FILE			"zkeydual.p15"
#define RENEW_PRIVKEY_FILE			"zkeyren.p15"
#define P15_FILE_TEMPLATE			"zkeyp15%d.p15"
#define CMP_PRIVKEY_FILE_TEMPLATE	"zkeycmp.p15"
#define PNP_PRIVKEY_FILE			"zkeypnpu.p15"
#define PNPCA_PRIVKEY_FILE			"zkeypnpc.p15"
#define SERVER_PRIVKEY_FILE_TEMPLATE "zkeysrv%d.p15"
#define SERVER_ECPRIVKEY_FILE_TEMPLATE "zkeysrp%d.p15"
#define SSH_PRIVKEY_FILE_TEMPLATE	"zkeyssh%d.p15"
#define SSL_CLI_PRIVKEY_FILE		"zkeysslc.p15"
#define TSA_PRIVKEY_FILE			"zkeytsa.p15"
#define MISC_PRIVKEY_FILE_TEMPLATE	"zkeymis%d.p15"
#define PKCS12_FILE_TEMPLATE		"zkeyp12%d.p12"

#define PGP_PUBKEY_FILE				"zpubring.pgp"
#define PGP_PRIVKEY_FILE			"zsecring.pgp"
#define OPENPGP_PUBKEY_HASH_FILE	"zpubhash.gpg"
#define OPENPGP_PRIVKEY_HASH_FILE	"zsechash.gpg"
#define OPENPGP_PUBKEY_AES_FILE		"zpubaes.pkr"
#define OPENPGP_PRIVKEY_AES_FILE	"zsecaes.skr"
#define OPENPGP_PRIVKEY_CAST_FILE	"zseccas.gpg"
#define OPENPGP_PUBKEY_RSA_FILE		"zpubrsa.gpg"
#define OPENPGP_PRIVKEY_RSA_FILE	"zsecrsa.gpg"
#define OPENPGP_PRIVKEY_PART_FILE	"zsecpart.gpg"
#define OPENPGP_PUBKEY_MULT_FILE	"zpubmult.gpg"
#define NAIPGP_PUBKEY_FILE			"zpubnai.pkr"
#define NAIPGP_PRIVKEY_FILE			"zsecnai.skr"

#define CERT_FILE_TEMPLATE			"zcert%02d.der"
#define BASE64CERT_FILE_TEMPLATE	"zcert%d.asc"
#define ECC_CERT_FILE_TEMPLATE		"zeccert%02d.der"
#define BROKEN_CERT_FILE			"zcertb.der"
#define BROKEN_USER_CERT_FILE		"zcertbus.der"
#define BROKEN_CA_CERT_FILE			"zcertbca.der"
#define CERTREQ_FILE_TEMPLATE		"zcertreq%d.der"
#define CRL_FILE_TEMPLATE			"zcrl%d.crl"
#define CERTCHAIN_FILE_TEMPLATE		"zcertchn%d.der"
#define BASE64CERTCHAIN_FILE_TEMPLATE "zcertchn%d.asc"
#define PATHTEST_FILE_TEMPLATE		"zntest%d.p7s"
#define PADTEST_FILE_TEMPLATE		"zbadsig%d.der"
#define SSHKEY_FILE_TEMPLATE		"zsshkey%d.asc"
#define PGPKEY_FILE_TEMPLATE		"zpgpkey%d.pgp"
#define PGPASCKEY_FILE_TEMPLATE		"zpgpkey%d.asc"
#define NOCHAIN_EE_FILE				"znochn_ee.der"
#define NOCHAIN_CA_FILE				"znochn_ca.der"
#define RTCS_OK_FILE				"zrtcsrok.der"
#define OCSP_OK_FILE				"zocsprok.der"
#define OCSP_REV_FILE				"zocsprre.der"
#define OCSP_CA_FILE				"zocspca.der"
#define CRLCERT_FILE_TEMPLATE		"zcrlcrt%d.der"
#define RTCS_FILE_TEMPLATE			"zrtcsee%do.der"
#define OCSP_CA_FILE_TEMPLATE		"zocspca%d.der"
#define OCSP_EEOK_FILE_TEMPLATE		"zocspok%d.der"
#define OCSP_EEREV_FILE_TEMPLATE	"zocspre%d.der"
#define CMP_CA_FILE_TEMPLATE		"zcmpca%d.der"
#define SCEP_CA_FILE_TEMPLATE		"zscepca%d.der"

#define CMS_ENC_FILE_TEMPLATE		"zpwenc%d.p7m"
#define SMIME_SIG_FILE_TEMPLATE		"zsigned%d.p7s"
#define SMIME_ENV_FILE_TEMPLATE		"zenvel%d.p7m"
#define PGP_ENC_FILE_TEMPLATE		"zconven%d.pgp"
#define PGP_PKE_FILE_TEMPLATE		"zpgpenc%d.pgp"
#define OPENPGP_PKE_FILE_TEMPLATE	"zgpgenc%d.gpg"
#define PGP_SIG_FILE_TEMPLATE		"zsigned%d.pgp"
#define PGP_COPR_FILE_TEMPLATE		"zcopr%d.pgp"

#define TESTDATA_FILE_TEMPLATE		"ztestd%d.dat"
#define COMPRESS_FILE				"filename.h"

/****************************************************************************
*																			*
*									Windows CE								*
*																			*
****************************************************************************/

#elif defined( _WIN32_WCE )

#define TEST_PRIVKEY_FILE			L"\\Storage Card\\keys\\test.p15"
#define TEST_PRIVKEY_TMP_FILE		L"\\Storage Card\\keys\\test_tmp.p15"
#define TEST_PRIVKEY_ALT_FILE		L"\\Storage Card\\keys\\test.p12"
#define CA_PRIVKEY_FILE				L"\\Storage Card\\keys\\ca.p15"
#define ICA_PRIVKEY_FILE			L"\\Storage Card\\keys\\ca_int.p15"
#define SCEPCA_PRIVKEY_FILE			L"\\Storage Card\\keys\\ca_scep.p15"
#define USER_PRIVKEY_FILE			L"\\Storage Card\\keys\\user.p15"
#define DUAL_PRIVKEY_FILE			L"\\Storage Card\\keys\\dual.p15"
#define RENEW_PRIVKEY_FILE			L"\\Storage Card\\keys\\renewed.p15"
#define P15_FILE_TEMPLATE			L"\\Storage Card\\keys\\pkcs15_%d.p15"
#define CMP_PRIVKEY_FILE_TEMPLATE	L"\\Storage Card\\keys\\cmp%d.p15"
#define PNP_PRIVKEY_FILE			L"\\Storage Card\\keys\\pnp_user.p15"
#define PNPCA_PRIVKEY_FILE			L"\\Storage Card\\keys\\pnp_ca.p15"
#define SERVER_PRIVKEY_FILE_TEMPLATE L"\\Storage Card\\keys\\server%d.p15"
#define SERVER_ECPRIVKEY_FILE_TEMPLATE L"\\Storage Card\\keys\\serverp%d.p15"
#define SSH_PRIVKEY_FILE_TEMPLATE	L"\\Storage Card\\keys\\ssh%d.p15"
#define SSL_CLI_PRIVKEY_FILE		L"\\Storage Card\\keys\\ssl_cli.p15"
#define TSA_PRIVKEY_FILE			L"\\Storage Card\\keys\\tsa.p15"
#define MISC_PRIVKEY_FILE_TEMPLATE	L"\\Storage Card\\keys\\misc%d.p15"
#define PKCS12_FILE_TEMPLATE		L"\\Storage Card\\keys\\pkcs12_%d.p12"

#define PGP_PUBKEY_FILE				L"\\Storage Card\\pgp\\pubring.pgp"
#define PGP_PRIVKEY_FILE			L"\\Storage Card\\pgp\\secring.pgp"
#define OPENPGP_PUBKEY_HASH_FILE	L"\\Storage Card\\pgp\\pub_hash.gpg"
#define OPENPGP_PRIVKEY_HASH_FILE	L"\\Storage Card\\pgp\\sec_hash.gpg"
#define OPENPGP_PUBKEY_AES_FILE		L"\\Storage Card\\pgp\\pub_aes.pkr"
#define OPENPGP_PRIVKEY_AES_FILE	L"\\Storage Card\\pgp\\sec_aes.skr"
#define OPENPGP_PRIVKEY_CAST_FILE	L"\\Storage Card\\pgp\\sec_cast.gpg"
#define OPENPGP_PUBKEY_RSA_FILE		L"\\Storage Card\\pgp\\pub_rsa.gpg"
#define OPENPGP_PRIVKEY_RSA_FILE	L"\\Storage Card\\pgp\\sec_rsa.gpg"
#define OPENPGP_PRIVKEY_PART_FILE	L"\\Storage Card\\pgp\\sec_part.gpg"
#define OPENPGP_PUBKEY_MULT_FILE	L"\\Storage Card\\pgp\\pub_mult.gpg"
#define NAIPGP_PUBKEY_FILE			L"\\Storage Card\\pgp\\pub_nai.pkr"
#define NAIPGP_PRIVKEY_FILE			L"\\Storage Card\\pgp\\sec_nai.skr"

#define CERT_FILE_TEMPLATE			L"\\Storage Card\\certs\\cert%02d.der"
#define BASE64CERT_FILE_TEMPLATE	L"\\Storage Card\\certs\\cert%d.asc"
#define ECC_CERT_FILE_TEMPLATE		L"\\Storage Card\\certs\\eccert%02d.der"
#define BROKEN_CERT_FILE			L"\\Storage Card\\certs\\broken.der"
#define BROKEN_USER_CERT_FILE		L"\\Storage Card\\certs\\broken_ee.der"
#define BROKEN_CA_CERT_FILE			L"\\Storage Card\\certs\\broken_ca.der"
#define CERTREQ_FILE_TEMPLATE		L"\\Storage Card\\certs\\certreq%d.der"
#define CRL_FILE_TEMPLATE			L"\\Storage Card\\certs\\crl%d.crl"
#define CERTCHAIN_FILE_TEMPLATE		L"\\Storage Card\\certs\\certchn%d.der"
#define BASE64CERTCHAIN_FILE_TEMPLATE L"\\Storage Card\\certs\\certchn%d.asc"
#define PATHTEST_FILE_TEMPLATE		L"\\Storage Card\\nist\\test%d.p7s"
#define PADTEST_FILE_TEMPLATE		L"\\Storage Card\\certs\\badsig%d.der"
#define SSHKEY_FILE_TEMPLATE		L"\\Storage Card\\misc\\sshkey%d.asc"
#define PGPKEY_FILE_TEMPLATE		L"\\Storage Card\\pgp\\pubkey%d.pgp"
#define PGPASCKEY_FILE_TEMPLATE		L"\\Storage Card\\pgp\\pubkey%d.asc"
#define NOCHAIN_EE_FILE				L"\\Storage Card\\misc\\nochn_ee.der"
#define NOCHAIN_CA_FILE				L"\\Storage Card\\misc\\nochn_ca.der"
#define RTCS_OK_FILE				L"\\Storage Card\\misc\\rtcsrok.der"
#define OCSP_OK_FILE				L"\\Storage Card\\session\\ocspr_ok.der"
#define OCSP_REV_FILE				L"\\Storage Card\\session\\ocspr_re.der"
#define OCSP_CA_FILE				L"\\Storage Card\\session\\ocspca.der"
#define CRLCERT_FILE_TEMPLATE		L"\\Storage Card\\misc\\crl_cert%d.der"
#define RTCS_FILE_TEMPLATE			L"\\Storage Card\\misc\\rtcs_ee%do.der"
#define OCSP_CA_FILE_TEMPLATE		L"\\Storage Card\\session\\ocsp_ca%d.der"
#define OCSP_EEOK_FILE_TEMPLATE		L"\\Storage Card\\session\\ocsp_ok%d.der"
#define OCSP_EEREV_FILE_TEMPLATE	L"\\Storage Card\\session\\ocsp_re%d.der"
#define CMP_CA_FILE_TEMPLATE		L"\\Storage Card\\session\\cmp_ca%d.der"
#define SCEP_CA_FILE_TEMPLATE		L"\\Storage Card\\session\\scep_ca%d.der"

#define CMS_ENC_FILE_TEMPLATE		L"\\Storage Card\\smime\\pw_enc%d.p7m"
#define SMIME_SIG_FILE_TEMPLATE		L"\\Storage Card\\smime\\signed%d.p7s"
#define SMIME_ENV_FILE_TEMPLATE		L"\\Storage Card\\smime\\envel%d.p7m"
#define PGP_ENC_FILE_TEMPLATE		L"\\Storage Card\\pgp\\conv_enc%d.pgp"
#define PGP_PKE_FILE_TEMPLATE		L"\\Storage Card\\pgp\\pgp_enc%d.pgp"
#define OPENPGP_PKE_FILE_TEMPLATE	L"\\Storage Card\\pgp\\gpg_enc%d.gpg"
#define PGP_SIG_FILE_TEMPLATE		L"\\Storage Card\\pgp\\signed%d.pgp"
#define PGP_COPR_FILE_TEMPLATE		L"\\Storage Card\\pgp\\copr%d.pgp"

#define TESTDATA_FILE_TEMPLATE		L"\\Storage Card\\misc\\testdata%d.dat"
#define COMPRESS_FILE				L"\\Storage Card\\filename.h"

/****************************************************************************
*																			*
*								Generic Filesystem							*
*																			*
****************************************************************************/

#else

#define TEST_PRIVKEY_FILE			TEXT( "test/keys/test.p15" )
#define TEST_PRIVKEY_TMP_FILE		TEXT( "test/keys/test_tmp.p15" )
#define TEST_PRIVKEY_ALT_FILE		TEXT( "test/keys/test.p12" )
#define CA_PRIVKEY_FILE				TEXT( "test/keys/ca.p15" )
#define ICA_PRIVKEY_FILE			TEXT( "test/keys/ca_int.p15" )
#define SCEPCA_PRIVKEY_FILE			TEXT( "test/keys/ca_scep.p15" )
#define USER_PRIVKEY_FILE			TEXT( "test/keys/user.p15" )
#define DUAL_PRIVKEY_FILE			TEXT( "test/keys/dual.p15" )
#define RENEW_PRIVKEY_FILE			TEXT( "test/keys/renewed.p15" )
#define P15_FILE_TEMPLATE			TEXT( "test/keys/pkcs15_%d.p15" )
#define CMP_PRIVKEY_FILE_TEMPLATE	TEXT( "test/keys/cmp%d.p15" )
#define PNP_PRIVKEY_FILE			TEXT( "test/keys/pnp_user.p15" )
#define PNPCA_PRIVKEY_FILE			TEXT( "test/keys/pnp_ca.p15" )
#define SERVER_PRIVKEY_FILE_TEMPLATE TEXT( "test/keys/server%d.p15" )
#define SERVER_ECPRIVKEY_FILE_TEMPLATE TEXT( "test/keys/serverp%d.p15" )
#define SSH_PRIVKEY_FILE_TEMPLATE	TEXT( "test/keys/ssh%d.p15" )
#define SSL_CLI_PRIVKEY_FILE		TEXT( "test/keys/ssl_cli.p15" )
#define TSA_PRIVKEY_FILE			TEXT( "test/keys/tsa.p15" )
#define MISC_PRIVKEY_FILE_TEMPLATE	TEXT( "test/keys/misc%d.p15" )
#define PKCS12_FILE_TEMPLATE		TEXT( "test/keys/pkcs12_%d.p12" )

#define PGP_PUBKEY_FILE				TEXT( "test/pgp/pubring.pgp" )
#define PGP_PRIVKEY_FILE			TEXT( "test/pgp/secring.pgp" )
#define OPENPGP_PUBKEY_HASH_FILE	TEXT( "test/pgp/pub_hash.gpg" )
#define OPENPGP_PRIVKEY_HASH_FILE	TEXT( "test/pgp/sec_hash.gpg" )
#define OPENPGP_PUBKEY_AES_FILE		TEXT( "test/pgp/pub_aes.pkr" )
#define OPENPGP_PRIVKEY_AES_FILE	TEXT( "test/pgp/sec_aes.skr" )
#define OPENPGP_PRIVKEY_CAST_FILE	TEXT( "test/pgp/sec_cast.gpg" )
#define OPENPGP_PUBKEY_RSA_FILE		TEXT( "test/pgp/pub_rsa.gpg" )
#define OPENPGP_PRIVKEY_RSA_FILE	TEXT( "test/pgp/sec_rsa.gpg" )
#define OPENPGP_PRIVKEY_PART_FILE	TEXT( "test/pgp/sec_part.gpg" )
#define OPENPGP_PUBKEY_MULT_FILE	TEXT( "test/pgp/pub_mult.gpg" )
#define NAIPGP_PUBKEY_FILE			TEXT( "test/pgp/pub_nai.pkr" )
#define NAIPGP_PRIVKEY_FILE			TEXT( "test/pgp/sec_nai.skr" )

#define CERT_FILE_TEMPLATE			TEXT( "test/certs/cert%02d.der" )
#define BASE64CERT_FILE_TEMPLATE	TEXT( "test/certs/cert%d.asc" )
#define ECC_CERT_FILE_TEMPLATE		TEXT( "test/certs/eccert%02d.der" )
#define BROKEN_CERT_FILE			TEXT( "test/certs/broken.der" )
#define BROKEN_USER_CERT_FILE		TEXT( "test/certs/broken_ee.der" )
#define BROKEN_CA_CERT_FILE			TEXT( "test/certs/broken_ca.der" )
#define CERTREQ_FILE_TEMPLATE		TEXT( "test/certs/certreq%d.der" )
#define CRL_FILE_TEMPLATE			TEXT( "test/certs/crl%d.crl" )
#define CERTCHAIN_FILE_TEMPLATE		TEXT( "test/certs/certchn%d.der" )
#define BASE64CERTCHAIN_FILE_TEMPLATE TEXT( "test/certs/certchn%d.asc" )
#define PATHTEST_FILE_TEMPLATE		TEXT( "test/nist/test%d.p7s" )
#define PADTEST_FILE_TEMPLATE		TEXT( "test/certs/bad_sig%d.der" )
#define SSHKEY_FILE_TEMPLATE		TEXT( "test/misc/sshkey%d.asc" )
#define PGPKEY_FILE_TEMPLATE		TEXT( "test/pgp/pubkey%d.pgp" )
#define PGPASCKEY_FILE_TEMPLATE		TEXT( "test/pgp/pubkey%d.asc" )
#define NOCHAIN_EE_FILE				TEXT( "test/misc/nochn_ee.der" )
#define NOCHAIN_CA_FILE				TEXT( "test/misc/nochn_ca.der" )
#define RTCS_OK_FILE				TEXT( "test/misc/rtcsrok.der" )
#define OCSP_OK_FILE				TEXT( "test/session/ocspr_ok.der" )
#define OCSP_REV_FILE				TEXT( "test/session/ocspr_re.der" )
#define OCSP_CA_FILE				TEXT( "test/session/ocspca.der" )
#define CRLCERT_FILE_TEMPLATE		TEXT( "test/misc/crl_cert%d.der" )
#define RTCS_FILE_TEMPLATE			TEXT( "test/misc/rtcs_ee%do.der" )
#define OCSP_CA_FILE_TEMPLATE		TEXT( "test/session/ocsp_ca%d.der" )
#define OCSP_EEOK_FILE_TEMPLATE		TEXT( "test/session/ocsp_ok%d.der" )
#define OCSP_EEREV_FILE_TEMPLATE	TEXT( "test/session/ocsp_re%d.der" )
#define CMP_CA_FILE_TEMPLATE		TEXT( "test/session/cmp_ca%d.der" )
#define SCEP_CA_FILE_TEMPLATE		TEXT( "test/session/scep_ca%d.der" )

#define CMS_ENC_FILE_TEMPLATE		TEXT( "test/smime/pw_enc%d.p7m" )
#define SMIME_SIG_FILE_TEMPLATE		TEXT( "test/smime/signed%d.p7s" )
#define SMIME_ENV_FILE_TEMPLATE		TEXT( "test/smime/envel%d.p7m" )
#define PGP_ENC_FILE_TEMPLATE		TEXT( "test/pgp/conv_enc%d.pgp" )
#define PGP_PKE_FILE_TEMPLATE		TEXT( "test/pgp/pgp_enc%d.pgp" )
#define OPENPGP_PKE_FILE_TEMPLATE	TEXT( "test/pgp/gpg_enc%d.gpg" )
#define PGP_SIG_FILE_TEMPLATE		TEXT( "test/pgp/signed%d.pgp" )
#define PGP_COPR_FILE_TEMPLATE		TEXT( "test/pgp/copr%d.pgp" )

#define TESTDATA_FILE_TEMPLATE		TEXT( "test/misc/testdata%d.dat" )
#define COMPRESS_FILE				TEXT( "test/filename.h" )

#endif /* OS-specific naming */
