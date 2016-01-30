/****************************************************************************
*																			*
*						  cryptlib Device Test Routines						*
*						Copyright Peter Gutmann 1997-2009					*
*																			*
****************************************************************************/

#include "cryptlib.h"
#include "test/test.h"

#if defined( __MVS__ ) || defined( __VMCMS__ )
  /* Suspend conversion of literals to ASCII. */
  #pragma convlit( suspend )
#endif /* IBM big iron */
#if defined( __ILEC400__ )
  #pragma convert( 0 )
#endif /* IBM medium iron */

/* Set the following to a nonzero value to test cryptlib's device init
   capability.  THIS WILL ZEROISE/ERASE THE DEVICE BEING TESTED AS A PART
   OF THE PROCESS.  All data contained in it will be destroyed.
   
   A number of buggy PKCS #11 devices can't be properly initialised through 
   the PKCS #11 API but require a vendor-supplied init program to be run.
   If they're initialised via the PKCS #11 API then everything appears to
   be fine but the device will then fail in a variety of strange ways when
   an attempt is made to use it.  The PKCS #11 code will perform some sanity
   checks to try and detect obvious cases of not-really-intitialised 
   initialised devices, but it can't catch every possible kind of brokenness.

   Device initialisation is always performed for native cryptographic 
   hardware devices since these are typically being subject to development
   self-tests and need to be reset to their ground state as part of the
   testing process.  In other words TEST_INITIALISE_DEVICE has an implicit
   value of '1' for this device type */

#define TEST_INITIALISE_DEVICE	0

/* If the device is very slow (e.g. a smart card), clear the following to
   not test the keygen capabilities of the device.  You can set this once
   initially to generate the test keys and then re-clear it to use the
   initially-generated keys from then on */

#define TEST_KEYGEN
#if ( TEST_INITIALISE_DEVICE > 0 ) && !defined( TEST_KEYGEN )
  #define TEST_KEYGEN			1	/* Must be 1 if initialising card */
#endif /* TEST_INITIALISE_DEVICE && !TEST_KEYGEN */

/* When testing high-level functionality, it's useful to be able to disable
   the low-level algorithm tests and go straight to the high-level tests.
   The following define can be used to disable the algorithm tests */

#define TEST_ALGORITHMS

/* The default PKC algorithm that we use when generating keys in the device.
   This is usually the universal-standard RSA, but can be switched to other
   algorithms when we're testing different device capabilities */

#define DEVICE_PKC_ALGO			CRYPT_ALGO_RSA
/* #define DEVICE_PKC_ALGO			CRYPT_ALGO_ECDSA */

#ifdef TEST_DEVICE

/* Note that Fortezza support was removed as of cryptlib 3.4.0, the Fortezza 
   test code is still present here for historical purposes but it's no 
   longer supported in cryptlib itself */

/* The device code will produce a large number of warnings because of ASCII
   <-> Unicode issues, since there aren't any PKCS #11 drivers for WinCE
   it's not worth adding a mountain of special-case code to handle this so
   we no-op it out under WinCE */

#ifndef _WIN32_WCE

/****************************************************************************
*																			*
*								Device Information							*
*																			*
****************************************************************************/

/* Device information tables for PKCS #11 device types.  This lists all the
   devices we know about and can check for.  If you have a PKCS #11 device
   that isn't listed below, you need to add an entry with its name and a
   password and key object label usable for testing to the table, and also
   add the name of the driver as a CRYPT_OPTION_DEVICE_PKCS11_DVRxx entry so
   cryptlib can load the appropriate driver for it.  To add this, use the
   updateConfig() function in testlib.c, see the code comments there for more
   details.

   The ActivCard driver is so broken that it's incredible it works at all,
   it's more of a PKCS #11-like API that only works if you use it in exactly
   the same way as the single test case that ActivCard must have used to
   evaluate it.  The Telesec driver is even more broken than that (it's so
   bad that it doesn't even work with Netscape), it just fakes a PKCS #11
   API while doing something completely different.

   The SEIS EID cards name their private key objects slightly differently
   from the name used in the software-only eID driver, if you're using a
   card-based version you need to switch the commented lines below to the
   alternate name.

   The Rainbow iKey uses Datakey drivers, so the Datakey test below will work
   for both Datakey cards/keys and iKeys.  The newer Rainbow USB tokens
   (using DataKey 232 drivers) can't be usefully initialised via PKCS #11
   but have to be initialised using the vendor utility or operations fail in
   strange and illogical ways.  In addition the driver partially ignores
   user-specified key attributes such as encrypt-only or sign-only and uses
   its own internal defaults.  Finally, operations with these keys then fail
   with a key-type-inconsistent error even though there's nothing wrong with
   them.  The solution to these problems is to use the Datakey 201 drivers
   (intended for Entrust compatibility, which means they actually test them
   properly), which work properly.

   The iD2 driver implements multiple virtual slots, one for each key type,
   so the entry is given in the extended driver::slot name format to tell
   cryptlib which slot to use.

   To reset the Rainbow card after it locks up and stops responding to
   commands, run /samples/cryptoki20/sample.exe, enter 1 CR, 4 CR, 5 CR,
   7 CR 2 CR "rainbow" CR, g CR "test" CR q CR (you need to follow that
   sequence exactly for it to work).

   To (try to) get the Eracom 3.09 CProv to work, delete the /cryptoki
   directory (where it dumps all its config data, ignoring the settings in
   cryptoki.ini), run ctconf.exe to set up a new device config, then
   run ctconf -v -n0 (optionally 'ctconf -v -r0' to reinitialise the token
   if you get an error about it already being initialised).  Then set 
   TEST_INITIALISE_DEVICE to a nonzero value and things should run OK (as 
   with the Rainbow tests, you need to follow this exactly for it to work).  
   Re-running the test with the initialised device, or trying to re-run the 
   initialisation, both fail.  The re-init reports that no login is required 
   for the token, returns an already-logged-in error if an attempt is made 
   to log in, and returns a not-logged-in error of an attempt is made to do 
   anything that needs a login.  The re-use of the initialised device fails 
   with an invalid object handle for every object that's created (that is, 
   it's possible to create any object, but any attempt to use it returns an 
   invalid object handle error).  However, retrying this on a different 
   machine with a fresh install of the 3.09 CProv jumped immediately to the 
   not-logged-in error state in which it's possible to log in, but trying to 
   perform any operation that needs a login results in a not-logged-in 
   error.
   
   The Eracom 2.10 CProv is usually a lot less problematic, although even
   with that it's not possible to initialise the device in software (in
   other words never enable TEST_INITIALISE_DEVICE), you have to run 
   'ctinit -ptest' otherwise the device will end up in a state where any 
   attempt to use an object results in a CKR_KEY_HANDLE_INVALID (so even a 
   C_CreateObject() followed immediately by a C_EncryptInit() using the
   handle returned from C_CreateObject() returns CKR_KEY_HANDLE_INVALID).

   The Spyrus USB drivers don't get on with various drivers for other USB
   devices such as USB printers (basic devices like USB storage keys are
   OK).  To get the Spyrus driver to see the USB token, it may be necessary
   to completely remove (not just disable) other USB device drivers.  The
   Rosetta cards/USB tokens are very flaky, in general it's only possible
   to safely generate new keys and add certs to a freshly-initialised token,
   trying to add/update objects when there are already existing objects
   present tends to fail with internal errors, and deleting the existing
   objects isn't possible, the delete call succeeds but the object isn't
   touched.  In addition with most versions of the Spyrus drivers data
   isn't persisted to the token correctly, so some information (and even
   complete objects) are only visible for the duration of the session that
   created them.

   The Netscape soft-token (softokn3.dll, i.e. NSS) is a bit problematic to 
   use, it requires the presence of three additional libraries (nspr4.dll, 
   plc4.dll, and plds4.dll) in the same directory, and even then requires 
   that C_Initialize() be called with proprietary and only partially-
   documented nonstandard arguments, see the comment in device/pkcs11_init.c 
   for more on this.  The easiest way to arrange to have all the files in 
   the right place is to chdir to "C:/Program Files/Mozilla Firefox" before 
   loading the DLL, but even this doesn't mean that it'll work properly 
   (this behaviour isn't really a bug, softokn3.dll was only ever meant to 
   be used as a crypto layer for NSS, it was never intended to be a general-
   purpose PKCS #11 soft-token).

   The presence of a device entry in this table doesn't necessarily mean 
   that the PKCS #11 driver that it comes with functions correctly, or at 
   all.  In particular the iButton driver never really got out of beta so it 
   has some features unimplemented, and the Utimaco driver apparently has 
   some really strange bugs, as well as screwing up Windows power management 
   so that suspends either aren't possible any more or will crash apps.  At 
   the other end of the scale the Datakey (before Rainbow got to them), 
   Eracom (usually, for older versions of the driver), iD2, and nCipher 
   drivers are pretty good */

typedef struct {
	const char *name;
	const char *description;
	const char *password;
	const char *keyLabel;	/* Existing-key name, otherwise 'Test user key' */
	} DEVICE_CONFIG_INFO;

static const DEVICE_CONFIG_INFO pkcs11DeviceInfo[] = {
	{ "[Autodetect]", "Automatically detect device", "test", "Test user key" },
	{ "ActivCard Cryptoki Library", "ActivCard", "test", "Test user key" },
	{ "Bloomberg PKCS#11 Library", "Bloomberg", "test1234", "Test user key" },
	{ "Chrystoki", "Chrysalis Luna", "test", "Test user key" },
	{ "CryptoFlex", "CryptoFlex", "ABCD1234", "012345678901234567890123456789ME" },
	{ "Cryptographic Token Interface", "AET SafeSign", "test", "Test user key" },
	{ "Cryptoki for CardMan API", "Utimaco", "test", "Test user key" },
	{ "Cryptoki for eID", "Nexus soft-token", "1234", "Private key" },
	{ "Cryptoki for eID", "Nexus signature token", "1234", "eID private nonrepudiation key" },
	{ "Cryptoki for eID", "Nexus signature token", "1234", "eID private key encipherment key" },
	{ "Cryptoki PKCS-11", "Gemplus", "test", "Test user key" },
	{ "CryptoKit Extended Version", "Eutron (via Cylink)", "12345678", "Test user key" },
	{ "Datakey Cryptoki DLL - NETSCAPE", "Datakey pre-4.1, post-4.4 driver", "test", "Test user key" },
	{ "Datakey Cryptoki DLL - Version", "Datakey 4.1-4.4 driver", "test", "Test user key" },
	{ "Eracom Cryptoki", "Eracom", "test", "Test user key" },
	{ "ERACOM Software Only", "Eracom 1.x soft-token", "test", "Test user key" },
	{ "Software Only", "Eracom 2.x soft-token", "test", "Test user key" },
	{ "eToken PKCS#11", "Aladdin eToken", "test", "Test user key" },
	{ "G&D PKCS#11 Library", "Giesecke and Devrient", "test", "Test user key" },
	{ "FTSmartCard", "Feitian", "test", "1234" },
	{ "iButton", "Dallas iButton", "test", "Test user key" },
	{ "iD2 Cryptographic Library::iD2 Smart Card (PIN1)", "iD2 signature token::Slot 1", "1234", "Digital Signature" },
	{ "iD2 Cryptographic Library::iD2 Smart Card (PIN2)", "iD2 signature token::Slot 2", "5678", "Non Repudiation" },
	{ "ISG", "CryptoSwift HSM", "test", "Test user key" },
	{ "ISG Cryptoki API library", "CryptoSwift card", "test", "Test user key" },
	{ "Lynks/EES Token in SpyrusNATIVE", "Spyrus Lynks/EES", "test", "Test user key" },
	{ "NShield 75", "nCipher", "test", "Test user key" },
	{ "NSS Generic Crypto Services", "Netscape", "test", "Test user key" },
	{ "PKCS#11 Private Cryptoki", "GemSAFE", "1234", "Test user key" },
	{ "Safelayer PKCS#11", "Safelayer", "test", "Test user key" },
	{ "Schlumberger", "Schlumberger", "QWERTYUI", "Test user key" },
	{ "SignLite security module", "IBM SignLite", "test", "Test user key" },
	{ "SmartCard-HSM (UserPIN)", "CardContact", "123456", "rsa2k" },
	{ "Spyrus Rosetta", "Spyrus Rosetta", "test", "Test user key" },
	{ "Spyrus Lynks", "Spyrus Lynks", "test", "Test user key" },
	{ "Sun Metaslot", "nCipher on Solaris", "test", "Test user key" },
	{ "TCrypt", "Telesec", "123456", "Test user key" },
	{ "TrustCenter PKCS#11 Library", "GPKCS11", "12345678", "Test user key" },
	{ NULL, NULL, NULL }
	};

/* Device information for Fortezza cards */

#define FORTEZZA_ZEROISE_PIN		"ZeroizedCard"
#define FORTEZZA_SSO_DEFAULT_PIN	"Mosaic"
#define FORTEZZA_SSO_PIN			"test"
#define FORTEZZA_USER_PIN			"test"

static const DEVICE_CONFIG_INFO fortezzaDeviceInfo = \
	{ "[Autodetect]", "Automatically detect device", FORTEZZA_USER_PIN, "Test user key" };

/* Device information for CryptoAPI */

static const DEVICE_CONFIG_INFO capiDeviceInfo[] = {
	{ "[Autodetect]", "Automatically detect device", "", "Encryption key" },
	{ "Microsoft Base Cryptographic Provider v1.0::MY", "Microsoft Base Cryptographic Provider", "", "Encryption key" },
	{ NULL, NULL, NULL }
	};

/* Device information for generic crypto hardware */

static const DEVICE_CONFIG_INFO hardwareDeviceInfo[] = {
	{ "[Autodetect]", "Automatically detect device", "test", "Test user key" },
	{ "Dummy device", "Dummy test device", "test", "Test user key" },
	{ NULL, NULL, NULL }
	};

/* Data used to create certs in the device */

static const CERT_DATA paaCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "NZ" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "Honest Dave's PAA" },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, "Certification Policy Division" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "Dave the PAA" },

	/* Self-signed X.509v3 CA certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA cACertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "NZ" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "Dave's Wetaburgers and CA" },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, "Certification Division" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "Dave Himself" },

	/* Self-signed X.509v3 CA certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA userCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "NZ" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "Dave's Wetaburgers" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "Dave's key" },
	{ CRYPT_CERTINFO_EMAIL, IS_STRING, 0, TEXT( "dave@wetaburgers.com" ) },
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_SUBJECTNAME },	/* Re-select subject DN */

	/* X.509v3 general-purpose certificate */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_DIGITALSIGNATURE | CRYPT_KEYUSAGE_KEYENCIPHERMENT },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA userSigOnlyCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "NZ" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "Dave's Wetaburgers" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "Dave's signing key" },

	/* X.509v3 signature-only certificate */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_DIGITALSIGNATURE },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static const CERT_DATA userKeyAgreeCertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "NZ" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "Dave's Wetaburgers" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "Dave's key agreement key" },

	/* X.509v3 key agreement certificate */
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC, CRYPT_KEYUSAGE_KEYAGREEMENT },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Delete leftover keys created during testing */

static void deleteTestKey( const CRYPT_DEVICE cryptDevice,
						   const C_STR keyName, const char *keyDescription )
	{
	if( cryptDeleteKey( cryptDevice, CRYPT_KEYID_NAME, \
						keyName ) == CRYPT_OK && keyDescription != NULL )
		{
		printf( "(Deleted a %s key object, presumably a leftover from a "
				"previous run).\n", keyDescription );
		}
	}

/* Create a key and certificate in a device */

static BOOLEAN createKey( const CRYPT_DEVICE cryptDevice,
						  const CRYPT_ALGO_TYPE cryptAlgo,
						  const char *description, const char *dumpName,
						  const CRYPT_CONTEXT signingKey )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_CERTIFICATE cryptCert;
	BYTE certBuffer[ BUFFER_SIZE ], labelBuffer[ CRYPT_MAX_TEXTSIZE ];
	const BOOLEAN isCA = ( signingKey == CRYPT_UNUSED ) ? TRUE : FALSE;
	const CERT_DATA *certData = ( isCA ) ? cACertData : \
			( cryptAlgo == CRYPT_ALGO_RSA ) ? userCertData : \
			( cryptAlgo == CRYPT_ALGO_DSA ) ? userSigOnlyCertData : \
			userKeyAgreeCertData;
	int certificateLength, status;

	sprintf( labelBuffer, "Test %s key", description );

	/* Generate a key in the device */
	printf( "Generating a %s key in the device...", description );
	status = cryptDeviceCreateContext( cryptDevice, &cryptContext,
									   cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptDeviceCreateContext() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL, labelBuffer,
							 strlen( labelBuffer ) );
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		{
		cryptDestroyContext( cryptContext );
		printf( "\ncryptGenerateKey() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* Create a certificate for the key */
	printf( "Generating a certificate for the key..." );
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED, ( isCA ) ? \
								CRYPT_CERTTYPE_CERTIFICATE : \
								CRYPT_CERTTYPE_CERTCHAIN );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptSetAttribute( cryptCert,
						CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusOK( status ) && \
		!addCertFields( cryptCert, certData, __LINE__ ) )
		return( FALSE );
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptCert, isCA ? cryptContext : signingKey );
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( status ) )
		{
		cryptDestroyCert( cryptCert );
		printf( "\nCreation of certificate failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* Dump the resulting certificate for debugging */
	if( dumpName != NULL )
		{
		status = cryptExportCert( certBuffer, BUFFER_SIZE, &certificateLength,
				isCA ? CRYPT_CERTFORMAT_CERTIFICATE : CRYPT_CERTFORMAT_CERTCHAIN,
				cryptCert );
		if( cryptStatusOK( status ) )
			debugDump( dumpName, certBuffer, certificateLength );
		}

	/* Update the key with the certificate */
	printf( "Updating device with certificate..." );
	status = cryptAddPublicKey( cryptDevice, cryptCert );
	cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptAddPublicKey() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	return( TRUE );
	}

/****************************************************************************
*																			*
*							Device Logon/Initialisation						*
*																			*
****************************************************************************/

/* Print information about a device and log in if necessary */

static const DEVICE_CONFIG_INFO *checkLogonDevice( const CRYPT_DEVICE cryptDevice,
												   const DEVICE_CONFIG_INFO *deviceInfo,
												   const BOOLEAN isAutoDetect,
												   const BOOLEAN willInitialise )
	{
	char tokenLabel[ CRYPT_MAX_TEXTSIZE + 1 ];
	int loggedOn, tokenLabelSize, status;

	/* Tell the user what we're talking to */
	status = cryptGetAttributeString( cryptDevice, CRYPT_DEVINFO_LABEL,
									  tokenLabel, &tokenLabelSize );
	if( cryptStatusError( status ) )
		puts( "(Device doesn't appear to have a label)." );
	else
		{
		tokenLabel[ tokenLabelSize ] = '\0';
		printf( "Device label is '%s'.\n", tokenLabel );
		}

	/* Check whether the device corresponds to a known device.  We do this
	   because some devices require specific test passwords and whatnot in
	   order to work */
	if( isAutoDetect )
		{
		int i;

		for( i = 1; deviceInfo[ i ].name != NULL; i++ )
			{
			if( tokenLabelSize == \
							( int ) strlen( deviceInfo[ i ].name ) && \
				!memcmp( deviceInfo[ i ].name, tokenLabel,
						 tokenLabelSize ) )
				{
				printf( "Found a match for pre-defined device '%s',\n"
						"  using pre-set parameters.\n",
						deviceInfo[ i ].description );
				deviceInfo = &deviceInfo[ i ];
				break;
				}
			}
		}

	/* See if we need to authenticate ourselves */
	status = cryptGetAttribute( cryptDevice, CRYPT_DEVINFO_LOGGEDIN,
								&loggedOn );
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't obtain device login status." );
		return( NULL );
		}
	if( loggedOn )
		{
		/* Device may not require a login, or has already been logged in
		   via a keypad or similar mechanism */
		puts( "Device is already logged in, skipping login." );
		return( deviceInfo );
		}

	/* Try and log in */
	printf( "Logging on to the device..." );
	status = cryptSetAttributeString( cryptDevice,
							CRYPT_DEVINFO_AUTHENT_USER, deviceInfo->password,
							strlen( deviceInfo->password ) );
	if( status == CRYPT_ERROR_INITED )
		{
		/* Some devices may not require any login, in which case we're
		   done */
		puts( " device is already logged in." );
		return( deviceInfo );
		}
	if( status == CRYPT_ERROR_NOTINITED )
		{
		/* It's an uninitialised device, tell the user and exit */
		puts( " device needs to be initialised." );
		if( willInitialise )
			return( deviceInfo );
		printf( "cryptlib will not automatically initialise the device "
				"during the self-test\n  in case it contains data that "
				"needs to be preserved or requires special\n  steps to be "
				"taken before the initialisation is performed.  If you want "
				"to\n  initialise it, set TEST_INITIALISE_DEVICE at the top "
				"of\n  " __FILE__ " to a nonzero value.\n" );
		return( NULL );
		}
	if( cryptStatusError( status ) )
		{
		printf( "\nDevice login%s failed with error code %d, line %d.\n",
				( status == CRYPT_ERROR_WRONGKEY ) ? \
					"" : " for initialisation/setup purposes", 
				status, __LINE__ );
		if( status == CRYPT_ERROR_WRONGKEY && willInitialise )
			{
			/* If we're going to initialise the card, being in the wrong (or
			   even totally uninitialised) state isn't an error */
			puts( "This may be because the device isn't in the user-"
				  "initialised state, in which\n  case the standard user "
				  "PIN can't be used to log on to it." );
			return( deviceInfo );
			}
		if( isAutoDetect )
			{
			puts( "This may be because the auto-detection test uses a fixed "
				  "login value rather\n  than one specific to the device "
				  "type." );
			}
		return( NULL );
		}
	puts( " succeeded." );
	return( deviceInfo );
	}

/* Initialise a device.  Note that when doing this with a Fortezza card,
   these operations have to be done in a more or less continuous sequence
   (i.e. without an intervening device open call) because it's not possible
   to escape from some of the states if the card is closed and reopened in
   between.  In addition the PKCS #11 interface maps some of the
   initialisation steps differently than the CI interface, so we have to
   special-case this below */

static BOOLEAN initialiseDevice( const CRYPT_DEVICE cryptDevice,
								 const CRYPT_DEVICE_TYPE deviceType,
								 const DEVICE_CONFIG_INFO *deviceInfo )
	{
	const char *defaultSSOPIN = ( deviceType == CRYPT_DEVICE_FORTEZZA ) ? \
								FORTEZZA_SSO_DEFAULT_PIN : \
								deviceInfo->password;
	const char *ssoPIN = ( deviceType == CRYPT_DEVICE_FORTEZZA ) ? \
						 FORTEZZA_SSO_PIN : deviceInfo->password;
	const char *userPIN = deviceInfo->password;
	int status;

	/* PKCS #11 doesn't distinguish between zeroisation and initialisation,
	   so we only perform the zeroise test if it's a Fortezza card or built-
	   in hardware.  The latter doesn't really distinguish between the two
	   either but we perform the separate zeroise/initialise to make sure 
	   that the two work as required */
	if( deviceType == CRYPT_DEVICE_FORTEZZA || \
		deviceType == CRYPT_DEVICE_HARDWARE )
		{
		printf( "Zeroising device..." );
		status = cryptSetAttributeString( cryptDevice,
						CRYPT_DEVINFO_ZEROISE, FORTEZZA_ZEROISE_PIN,
						strlen( FORTEZZA_ZEROISE_PIN ) );
		if( cryptStatusError( status ) )
			{
			printf( "\nZeroise failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		puts( " succeeded." );
		}

	/* Initialise the device and set the SO PIN.   */
	printf( "Initialising device..." );
	status = cryptSetAttributeString( cryptDevice, CRYPT_DEVINFO_INITIALISE,
									  defaultSSOPIN, strlen( defaultSSOPIN ) );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't initialise device, status = %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );
	printf( "Setting SO PIN to '%s'...", ssoPIN );
	status = cryptSetAttributeString( cryptDevice,
									  CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR,
									  ssoPIN, strlen( ssoPIN ) );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't set SO PIN, status = %d, line %d.\n", status,
				__LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* If it's a Fortezza card, create a CA root key and install its 
	   certificate.  We have to do it at this point because the operation is 
	   only allowed in the SSO initialised state.  In addition we can't use 
	   the card for this operation because certificate slot 0 is a data-only 
	   slot (that is, it can't correspond to a key held on the card), so we 
	   create a dummy external certificate and use that */
	if( deviceType == CRYPT_DEVICE_FORTEZZA )
		{
		CRYPT_CERTIFICATE cryptCert;
		CRYPT_CONTEXT signContext;

		printf( "Loading PAA certificate..." );
		if( !loadDSAContexts( CRYPT_UNUSED, &signContext, NULL ) )
			return( FALSE );
		status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
								  CRYPT_CERTTYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = cryptSetAttribute( cryptCert,
						CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, signContext );
		if( cryptStatusOK( status ) && \
			!addCertFields( cryptCert, paaCertData, __LINE__ ) )
			return( FALSE );
		if( cryptStatusOK( status ) )
			status = cryptSignCert( cryptCert, signContext );
		cryptDestroyContext( signContext );
		if( cryptStatusError( status ) )
			{
			cryptDestroyCert( cryptCert );
			printf( "\nCreation of certificate failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptAddPublicKey( cryptDevice, cryptCert );
		cryptDestroyCert( cryptCert );
		if( cryptStatusError( status ) )
			{
			printf( "\ncryptAddPublicKey() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		puts( " succeeded." );
		}

	/* Set the user PIN and log on as the user */
	printf( "Setting user PIN to '%s'...", userPIN );
	status = cryptSetAttributeString( cryptDevice,
									  CRYPT_DEVINFO_SET_AUTHENT_USER,
									  userPIN, strlen( userPIN ) );
	if( cryptStatusOK( status ) )
		{
		int loggedOn;

		/* Some devices automatically log the user in when they set the user
		   password, so we check to see if it's necessary to log in before we
		   actually do it */
		status = cryptGetAttribute( cryptDevice, CRYPT_DEVINFO_LOGGEDIN,
									&loggedOn );
		if( cryptStatusError( status ) )
			{
			puts( "Couldn't obtain device login status." );
			return( FALSE );
			}
		if( !loggedOn )
			{
			status = cryptSetAttributeString( cryptDevice,
											  CRYPT_DEVINFO_AUTHENT_USER,
											  userPIN, strlen( userPIN ) );
			}
		}
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't set user PIN/log on as user, status = %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	return( TRUE );
	}

/****************************************************************************
*																			*
*									Device Tests							*
*																			*
****************************************************************************/

/* Test the general capabilities of a device */

static BOOLEAN testDeviceCapabilities( const CRYPT_DEVICE cryptDevice,
									   const char *deviceName,
									   const BOOLEAN isWriteProtected )
	{
	CRYPT_ALGO_TYPE cryptAlgo;
	int testCount = 0, failCount = 0;

	printf( "Checking %s capabilities...\n", deviceName );
	for( cryptAlgo = CRYPT_ALGO_FIRST_CONVENTIONAL;
		 cryptAlgo <= CRYPT_ALGO_LAST; cryptAlgo++ )
		{
		if( cryptStatusOK( cryptDeviceQueryCapability( cryptDevice,
													   cryptAlgo, NULL ) ) )
			{
			testCount++;
			if( !testLowlevel( cryptDevice, cryptAlgo, isWriteProtected ) )
				{
				/* The test failed, we don't exit at this point but only
				   remember that there was a problem since we want to test
				   every possible algorithm */
				failCount++;
				}
			}
		}

	if( isWriteProtected )
		puts( "No tests were performed since the device is write-protected." );
	else
		{
		if( failCount )
			printf( "%d of %d test%s failed.\n", failCount, testCount,
					( testCount > 1 ) ? "s" : "" );
		else
			puts( "Device capabilities test succeeded." );
		}

	return( ( failCount == testCount ) ? FALSE : TRUE );
	}

/* Test the high-level functionality provided by a device */

static BOOLEAN testPersistentObject( const CRYPT_DEVICE cryptDevice )
	{
	CRYPT_ALGO_TYPE cryptAlgo = CRYPT_ALGO_HMAC_SHA1;
	CRYPT_CONTEXT cryptContext;
	int status;

	printf( "Loading a persistent symmetric key into the device..." );

	/* Find an encryption algorithm that we can use and create a context in
	   the device */
	status = cryptDeviceQueryCapability( cryptDevice, cryptAlgo, NULL );
	if( cryptStatusError( status ) )
		{
		cryptAlgo = CRYPT_ALGO_3DES;
		status = cryptDeviceQueryCapability( cryptDevice, cryptAlgo, NULL );
		}
	if( cryptStatusOK( status ) )
		status = cryptDeviceCreateContext( cryptDevice, &cryptContext, 
										   cryptAlgo );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't create conventional-encryption context in "
				"device, status = %d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Make it a persistent object and load a key */
	status = cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL, 
									  SYMMETRIC_KEY_LABEL, 
									  strlen( SYMMETRIC_KEY_LABEL ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptContext, CRYPT_CTXINFO_PERSISTENT, 
									TRUE );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't make device context persistent, status = %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't load key into persistent context, status = %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* Destroy the context.  Since it's persistent, it'll still be present 
	   in the device */
	cryptDestroyContext( cryptContext );

	/* Recreate the object from the device */
	printf( "Reading back symmetric key..." );
	status = cryptGetKey( cryptDevice, &cryptContext, CRYPT_KEYID_NAME,
						  SYMMETRIC_KEY_LABEL, NULL );
	if( cryptStatusError( status ) )
		{
		printf( "\nRead of symmetric key failed, status = %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* Re-destroy it and make sure that it's still there afterwards.  This 
	   tests the persistence of read-back objects vs. created objects */
	cryptDestroyContext( cryptContext );
	printf( "Re-reading back symmetric key..." );
	status = cryptGetKey( cryptDevice, &cryptContext, CRYPT_KEYID_NAME,
						  SYMMETRIC_KEY_LABEL, NULL );
	if( cryptStatusError( status ) )
		{
		printf( "\nRe-read of symmetric key failed, status = %d, line %d.\n", 
				status, __LINE__ );
		return( FALSE );
		}
	puts( " succeeded." );

	/* Perform an encrypt/decrypt test with the recreated object */
	printf( "Performing encryption test with recovered key..." );
	status = testCrypt( cryptContext, cryptContext, NULL, TRUE, FALSE );
	if( cryptStatusError( status ) )
		return( FALSE );
	puts( " succeeded." );

	/* Clean up.  Unlike the public/private keys which are reused for 
	   various tests, this object isn't really useful for anything else so 
	   we always clean it up once we're done */
	cryptDestroyContext( cryptContext );
	deleteTestKey( cryptDevice, SYMMETRIC_KEY_LABEL, NULL );
	return( TRUE );
	}

static BOOLEAN testDeviceHighlevel( const CRYPT_DEVICE cryptDevice,
									const CRYPT_DEVICE_TYPE deviceType,
									const char *keyLabel,
									const char *password,
									const BOOLEAN isWriteProtected )
	{
	CRYPT_CONTEXT pubKeyContext, privKeyContext, sigKeyContext;
	int status;

#ifdef TEST_KEYGEN
	if( !isWriteProtected )
		{
		const CRYPT_ALGO_TYPE cryptAlgo = \
						( deviceType == CRYPT_DEVICE_FORTEZZA ) ? \
						CRYPT_ALGO_DSA : DEVICE_PKC_ALGO;

		if( deviceType != CRYPT_DEVICE_CRYPTOAPI )
			{
			/* Create a CA key in the device */
			if( !createKey( cryptDevice, cryptAlgo, "CA",
							( deviceType == CRYPT_DEVICE_PKCS11 ) ? \
							"dp_cacert" : "df_cacert", CRYPT_UNUSED ) )
				return( FALSE );

			/* Read back the CA key for use in generating end entity certs */
			status = cryptGetPrivateKey( cryptDevice, &sigKeyContext,
										 CRYPT_KEYID_NAME,
										 TEXT( "Test CA key" ), NULL );
			if( status == CRYPT_ERROR_NOTFOUND )
				{
				/* If we're using a PKCS #11 device that doesn't support
				   object labels then we have to use the alternate read
				   method via the certificate CN.  This requires enabling 
				   PKCS11_FIND_VIA_CRYPTLIB in device/pkcs11_rw.c */
				assert( cACertData[ 3 ].type == CRYPT_CERTINFO_COMMONNAME );
				status = cryptGetPrivateKey( cryptDevice, &sigKeyContext, 
											 CRYPT_KEYID_NAME, 
											 cACertData[ 3 ].stringValue, 
											 NULL );
				}
			}
		else
			{
			/* CryptoAPI can only store one private key per provider so we
			   can't have both a CA key and user key in the same "device".
			   Because of this we have to use the fixed CA key to issue the
			   certificate */
			status = getPrivateKey( &sigKeyContext, CA_PRIVKEY_FILE,
									CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
			}
		if( cryptStatusError( status ) )
			{
			printf( "\nRead of CA key failed with error code %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}

		/* Create end-entity certificate(s) for keys using the previously-
		   generated CA key.  If it's a Fortezza card and we're using KEA we
		   have to generate two sets of keys/certs, one for signing and one
		   for encryption */
		status = createKey( cryptDevice, cryptAlgo, "user",
							( deviceType == CRYPT_DEVICE_PKCS11 ) ? \
							"dp_usrcert" : "df_usrcert", sigKeyContext );
#ifdef USE_KEA
		if( status && deviceType == CRYPT_DEVICE_FORTEZZA )
			status = createKey( cryptDevice, CRYPT_ALGO_KEA, "KEA",
								"df_keacert", sigKeyContext );
#endif /* USE_KEA */
		cryptDestroyContext( sigKeyContext );
		if( !status )
			return( FALSE );
		}
	else
#endif /* TEST_KEYGEN */
		{
		puts( "Skipping key generation test, this assumes that the device "
			  "contains pre-\n  existing keys." );
		}

	/* See whether there are any existing keys or certs.  Some tokens have
	   these built in and don't allow anything new to be created, after this
	   point the handling is somewhat special-case but we can at least report
	   their presence.  Although generally we can re-use a private key
	   context for both public and private operations, some devices or drivers
	   (and by logical extension thereof the cryptlib kernel) don't allow
	   public-key ops with private keys so we have to eplicitly handle public
	   and private keys.  This gets somewhat messy because some devices don't
	   have public keys but allow public-key ops with their private keys,
	   while others separate public and private keys and don't allow the
	   private key to do public-key ops */
	status = cryptGetPublicKey( cryptDevice, &pubKeyContext,
								CRYPT_KEYID_NAME, keyLabel );
	if( cryptStatusOK( status ) )
		{
		int value;

		puts( "Found a public key in the device, details follow..." );
		printCertChainInfo( pubKeyContext );
		if( cryptStatusOK( \
				cryptGetAttribute( pubKeyContext,
								   CRYPT_CERTINFO_SELFSIGNED, &value ) ) && \
			value )
			{
			/* It's a self-signed certificate/certificate chain, make sure 
			   that it's valid.  Because it's probably not trusted, we make 
			   it temporarily implicitly trusted in order for the sig.check 
			   to succeed */
			status = cryptGetAttribute( pubKeyContext,
								CRYPT_CERTINFO_TRUSTED_IMPLICIT, &value );
			if( cryptStatusOK( status ) )
				status = cryptSetAttribute( pubKeyContext,
									CRYPT_CERTINFO_TRUSTED_IMPLICIT, 1 );
			if( cryptStatusOK( status ) )
				status = cryptCheckCert( pubKeyContext, CRYPT_UNUSED );
			if( cryptStatusError( status ) )
				{
				printf( "Signature on certificate is invalid, status %d, "
						"line %d.\n", status, __LINE__ );
				return( FALSE );
				}
			cryptSetAttribute( pubKeyContext,
							   CRYPT_CERTINFO_TRUSTED_IMPLICIT, value );
			}
		}
	else
		{
		puts( "Error: Couldn't locate public key in device." );
		pubKeyContext = CRYPT_UNUSED;
		}
	status = cryptGetPrivateKey( cryptDevice, &privKeyContext,
								 CRYPT_KEYID_NAME, keyLabel, NULL );
	if( cryptStatusOK( status ) )
		{
		puts( "Found a private key in the device, details follow..." );
		printCertChainInfo( privKeyContext );
		if( pubKeyContext == CRYPT_UNUSED )
			{
			/* No explicit public key found, try using the private key for
			   both key types */
			puts( "No public key found, attempting to continue using the "
				  "private key as both a\n  public and a private key." );
			pubKeyContext = privKeyContext;
			}
		}
	else
		{
		puts( "Error: Couldn't locate private key in device." );
		privKeyContext = CRYPT_UNUSED;
		}
	sigKeyContext = privKeyContext;
	if( deviceType == CRYPT_DEVICE_FORTEZZA )
		{
		cryptDestroyContext( pubKeyContext );	/* pubK is sig.only */
		status = cryptGetPrivateKey( cryptDevice, &privKeyContext,
									 CRYPT_KEYID_NAME, "Test KEA key", NULL );
		if( cryptStatusOK( status ) )
			{
			puts( "Found a key agreement key in the device, details follow..." );
			printCertChainInfo( privKeyContext );
			pubKeyContext = privKeyContext;		/* Fortezza allows both uses */
			}
		else
			{
			pubKeyContext = CRYPT_UNUSED;
			privKeyContext = CRYPT_UNUSED;
			}
		}

	/* If we got something, try some simple operations with it */
	if( pubKeyContext != CRYPT_UNUSED )
		{
		char emailAddress[ CRYPT_MAX_TEXTSIZE + 1 ];
		int length;

		if( !testEnvelopePKCCryptEx( pubKeyContext, cryptDevice ) )
			{
			if( deviceType == CRYPT_DEVICE_HARDWARE )
				{
				puts( "\nEnveloping test failed when using the built-in "
					  "cryptographic hardware device.\nIf this is an "
					  "emulated device that doesn't fully implement "
					  "public/private-key\nencryption and the test failed "
					  "with a CRYPT_ERROR_BADDATA then this isn't\na fatal "
					  "error, it simply means that cryptlib has detected "
					  "that the emulation\nisn't performing a genuine "
					  "crypto operation." );
				}
			return( FALSE );
			}
		if( !testCMSEnvelopePKCCryptEx( pubKeyContext, cryptDevice, 
										password, NULL ) )
			return( FALSE );
		cryptSetAttribute( pubKeyContext, CRYPT_ATTRIBUTE_CURRENT, 
						   CRYPT_CERTINFO_SUBJECTALTNAME );
		status = cryptGetAttributeString( pubKeyContext, CRYPT_CERTINFO_EMAIL,
										  emailAddress, &length );
		if( cryptStatusError( status ) )
			{
			printf( "Couldn't read recipient address from certificate, "
					"status %d, line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		emailAddress[ length ] = '\0';
		if( !testCMSEnvelopePKCCryptEx( CRYPT_UNUSED, cryptDevice, 
										password, emailAddress ) )
			return( FALSE );
		}
	else
		{
		puts( "Public-key enveloping tests skipped because no key was "
			  "available.\n" );
		}
	if( sigKeyContext != CRYPT_UNUSED )
		{
		if( !testCMSEnvelopeSignEx( sigKeyContext ) )
			return( FALSE );
		}
	else
		{
		puts( "Signed enveloping tests skipped because no key was "
			  "available." );
		}

	/* Test persistent (non-public-key) object creation */
#ifdef TEST_KEYGEN
	if( !isWriteProtected )
		{
		if( !testPersistentObject( cryptDevice ) )
			return( FALSE );
		}
#endif /* TEST_KEYGEN */

	/* Clean up */
	if( pubKeyContext == CRYPT_UNUSED && sigKeyContext == CRYPT_UNUSED )
		return( FALSE );
	if( privKeyContext != CRYPT_UNUSED )
		cryptDestroyContext( privKeyContext );
	if( sigKeyContext != CRYPT_UNUSED && privKeyContext != sigKeyContext )
		cryptDestroyContext( sigKeyContext );
	if( pubKeyContext != CRYPT_UNUSED && pubKeyContext != privKeyContext )
		cryptDestroyContext( pubKeyContext );
	return( TRUE );
	}

/* General device test routine */

static int testCryptoDevice( const CRYPT_DEVICE_TYPE deviceType,
							 const char *deviceName,
							 const DEVICE_CONFIG_INFO *deviceInfo )
	{
	CRYPT_DEVICE cryptDevice;
	BOOLEAN isWriteProtected = FALSE, isAutoDetect = FALSE;
	BOOLEAN testResult = FALSE, partialSuccess = FALSE;
	int status;

	/* Open a connection to the device */
	if( deviceType == CRYPT_DEVICE_PKCS11 || \
		deviceType == CRYPT_DEVICE_CRYPTOAPI || \
		deviceType == CRYPT_DEVICE_HARDWARE )
		{
		if( !memcmp( deviceInfo->name, "[A", 2 ) )
			{
			printf( "\nTesting %s with autodetection...\n", deviceName );
			isAutoDetect = TRUE;
			}
		else
			printf( "\nTesting %s %s...\n", deviceInfo->description, deviceName );
		status = cryptDeviceOpen( &cryptDevice, CRYPT_UNUSED, deviceType,
								  deviceInfo->name );
		}
	else
		{
		printf( "\nTesting %s...\n", deviceName );
		status = cryptDeviceOpen( &cryptDevice, CRYPT_UNUSED, deviceType,
								  deviceName );
		}
	if( status == CRYPT_ERROR_PARAM2 )
		{
		puts( "Support for this device type isn't enabled in this build of "
			  "cryptlib." );
		return( CRYPT_ERROR_NOTAVAIL );	/* Device access not available */
		}
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_PARAM3 || status == CRYPT_ERROR_NOTFOUND )
			puts( "Crypto device not detected, skipping test." );
		else
			printf( "cryptDeviceOpen() failed with error code %d, line %d.\n",
					status, __LINE__ );
		return( FALSE );
		}

	/* If it's one of the smarter classes of device, authenticate ourselves to
	   the device, which is usually required in order to allow it to be used
	   fully */
	if( deviceType == CRYPT_DEVICE_PKCS11 || \
		deviceType == CRYPT_DEVICE_FORTEZZA || \
		deviceType == CRYPT_DEVICE_HARDWARE )
		{
		deviceInfo = checkLogonDevice( cryptDevice, deviceInfo, isAutoDetect, 
									   TEST_INITIALISE_DEVICE );
		if( deviceInfo == NULL )
			{
			cryptDeviceClose( cryptDevice );
			return( FALSE );
			}
		}

	/* Write-protected devices won't allow contexts to be created in them,
	   before we try the general device capabilities test we make sure that
	   we can actually perform the operation */
	if( deviceType == CRYPT_DEVICE_PKCS11 )
		{
		CRYPT_CONTEXT cryptContext;

		/* Try and create a DES object.  The following check for read-only
		   devices always works because the device object ACL is applied at
		   a much higher level than any device capability checking, the
		   device will never even see the create object message if it's
		   write-protected so all we have to do is make sure that whatever
		   we create is ephemeral */
		status = cryptDeviceCreateContext( cryptDevice, &cryptContext,
										   CRYPT_ALGO_3DES );
		if( cryptStatusOK( status ) )
			cryptDestroyContext( cryptContext );
		if( status == CRYPT_ERROR_PERMISSION )
			isWriteProtected = TRUE;
		}

	/* To force the code not to try to create keys and certs in a writeable
	   device, uncomment the following line of code.  This requires that keys/
	   certs of the required type are already present in the device */
/*	KLUDGE_WARN( "write-protect status" );
	isWriteProtected = TRUE; */
#ifdef TEST_KEYGEN
	if( !isWriteProtected )
		{
		/* If it's a device that we can initialise, go through a full
		   initialisation.  Note that if this we're using built-in 
		   cryptographic hardware then we always perform the initialisation
		   because we're typically doing this as a development self-test and
		   need to restore the hardware to the ground state before we can
		   continue */
		if( deviceType != CRYPT_DEVICE_CRYPTOAPI && \
			( TEST_INITIALISE_DEVICE || deviceType == CRYPT_DEVICE_HARDWARE ) )
			{
			status = initialiseDevice( cryptDevice, deviceType,
									   deviceInfo );
			if( status == FALSE )
				{
				cryptDeviceClose( cryptDevice );
				return( FALSE );
				}
			}
		else
			{
			/* There may be test keys lying around from an earlier run, in
			   which case we try to delete them to make sure they won't
			   interfere with the current one */
			deleteTestKey( cryptDevice, "Test CA key", "CA" );
			deleteTestKey( cryptDevice, deviceInfo->keyLabel, "user" );
			if( deviceType == CRYPT_DEVICE_PKCS11 )
				{
				deleteTestKey( cryptDevice, RSA_PUBKEY_LABEL, "RSA public" );
				deleteTestKey( cryptDevice, RSA_PRIVKEY_LABEL, "RSA private" );
				deleteTestKey( cryptDevice, DSA_PUBKEY_LABEL, "DSA public" );
				deleteTestKey( cryptDevice, DSA_PRIVKEY_LABEL, "DSA private" );
				deleteTestKey( cryptDevice, SYMMETRIC_KEY_LABEL, "symmetric" );
				assert( cACertData[ 3 ].type == CRYPT_CERTINFO_COMMONNAME );
				deleteTestKey( cryptDevice, cACertData[ 3 ].stringValue,
							   "CA-cert" );
				assert( userCertData[ 2 ].type == CRYPT_CERTINFO_COMMONNAME );
				deleteTestKey( cryptDevice, userCertData[ 2 ].stringValue,
							   "user-cert" );
				assert( userSigOnlyCertData[ 2 ].type == CRYPT_CERTINFO_COMMONNAME );
				deleteTestKey( cryptDevice, 
							   userSigOnlyCertData[ 2 ].stringValue,
							   "sig-only-cert" );
				assert( userKeyAgreeCertData[ 2 ].type == CRYPT_CERTINFO_COMMONNAME );
				deleteTestKey( cryptDevice, 
							   userKeyAgreeCertData[ 2 ].stringValue,
							   "keyagree-only-cert" );
				}
			if( deviceType == CRYPT_DEVICE_FORTEZZA )
				deleteTestKey( cryptDevice, "Test KEA key", "KEA" );
			if( deviceType == CRYPT_DEVICE_CRYPTOAPI )
				{
				deleteTestKey( cryptDevice, "Encryption key", "RSA private" );
				deleteTestKey( cryptDevice, "Signature key", "secondary RSA private" );
				}
			}
		}
#endif /* TEST_KEYGEN */

#ifdef TEST_DH
	return( testLowlevel( cryptDevice, CRYPT_ALGO_DH, FALSE ) );
#endif /* TEST_DH */

	/* Report what the device can do.  This is intended mostly for simple
	   crypto accelerators and may fail with for devices that work only
	   with the higher-level functions centered around certificates,
	   signatures,and key wrapping, so we skip the tests for devices that
	   allow only high-level access */
#ifdef TEST_ALGORITHMS
	if( deviceType != CRYPT_DEVICE_FORTEZZA )
		testResult = testDeviceCapabilities( cryptDevice, deviceName,
											 isWriteProtected );
#else
	puts( "Skipping device algorithm tests." );
#endif /* TEST_ALGORITHMS */

	/* If it's a smart device, try various device-specific operations */
	if( deviceType == CRYPT_DEVICE_FORTEZZA || \
		deviceType == CRYPT_DEVICE_PKCS11 || \
		deviceType == CRYPT_DEVICE_CRYPTOAPI || \
		deviceType == CRYPT_DEVICE_HARDWARE )
		{
		partialSuccess = testDeviceHighlevel( cryptDevice, deviceType,
								deviceInfo->keyLabel, deviceInfo->password,
								isWriteProtected );
		}

	/* Clean up */
	status = cryptDeviceClose( cryptDevice );
	if( cryptStatusError( status ) )
		{
		printf( "cryptDeviceClose() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	if( !testResult && !partialSuccess )
		return( FALSE );
	if( testResult && partialSuccess )
		printf( "\n%s tests succeeded.\n\n", deviceName );
	else
		printf( "\nSome %s tests succeeded.\n\n", deviceName );
	return( TRUE );
	}

int testDevices( void )
	{
	int i, status;

	/* Test PKCS #11 devices */
#if 1
	for( i = 0; pkcs11DeviceInfo[ i ].name != NULL; i++ )
		{
		status = testCryptoDevice( CRYPT_DEVICE_PKCS11, "PKCS #11 crypto token",
								   &pkcs11DeviceInfo[ i ] );
		if( cryptStatusError( status ) && \
			!( status == CRYPT_ERROR_NOTAVAIL || \
			   ( i == 0 && status == CRYPT_ERROR_WRONGKEY ) ) )
			return( status );
		}
#endif /* 0 */

	/* Test generic crypto hardware interface */
#if 0
	for( i = 0; hardwareDeviceInfo[ i ].name != NULL; i++ )
		{
		status = testCryptoDevice( CRYPT_DEVICE_HARDWARE, "generic crypto hardware",
								   &hardwareDeviceInfo[ i ] );
		if( cryptStatusError( status ) && \
			!( status == CRYPT_ERROR_NOTAVAIL || \
			   ( i == 0 && status == CRYPT_ERROR_WRONGKEY ) ) )
			return( status );
		}
#endif /* 0 */

#if 0	/* For test purposes only to check CAPI data, don't use the CAPI code */
#ifdef __WINDOWS__
	for( i = 0; capiDeviceInfo[ i ].name != NULL; i++ )
		{
		status = testCryptoDevice( CRYPT_DEVICE_CRYPTOAPI, "Microsoft CryptoAPI",
								   &capiDeviceInfo[ i ] );
		if( cryptStatusError( status ) && \
			!( status == CRYPT_ERROR_NOTAVAIL || \
			   ( i == 0 && status == CRYPT_ERROR_WRONGKEY ) ) )
			return( status );
		}
#endif /* __WINDOWS__ */
#endif /* 0 */
	putchar( '\n' );
	return( TRUE );
	}

/****************************************************************************
*																			*
*						Full Device Initialisation Test						*
*																			*
****************************************************************************/

/* The following code takes two unitialised devices and turns one into a
   fully initialised CA device, which then runs a PnP PKI session that turns
   the other into a fully initialised user device.

   The following configuration options can be used to change the beaviour of
   the self-test, for example to run it on the local machine in loopback mode
   vs. running on two distinct machines.  Defining an IP address (or host
   name) for SERVER_MACHINE_ADDRESS will have the client connect to that
   address instead of running a local loopback test */

#if 0
  #define SERVER_MACHINE_ADDRESS	"161.5.99.22"
  #define SERVER_MACHINE_PORT		4080
  #define CLIENT_DEVICE_TYPE		CRYPT_DEVICE_FORTEZZA
  #define SERVER_DEVICE_TYPE		CRYPT_DEVICE_FORTEZZA
  #define CLIENT_ID					"25CHS-UDQBU-BPASM"
  #define CLIENT_AUTHENTICATOR		"5ZCJ8-34A5C-YSXRD-C9EME"
  #define CLIENT_TOKEN_SLOT			CRYPT_USE_DEFAULT
  #define NET_TIMEOUT				300
#else
  #define SERVER_MACHINE_ADDRESS	"localhost"
  #define SERVER_MACHINE_PORT		80
  #define CLIENT_DEVICE_TYPE		CRYPT_DEVICE_FORTEZZA
  #define SERVER_DEVICE_TYPE		CRYPT_DEVICE_FORTEZZA
  #define CLIENT_TOKEN_SLOT			1
  #define NET_TIMEOUT				CRYPT_USE_DEFAULT
#endif /* Loopback vs. general test */

/* Default PIN values */

#if CLIENT_DEVICE_TYPE == CRYPT_DEVICE_FORTEZZA
  #define DEFAULT_SSO_PIN	FORTEZZA_SSO_DEFAULT_PIN
#else
  #define DEFAULT_SSO_PIN	"0000"
#endif /* Fortezza vs. PKCS #11 default SSO PINs */
#define SSO_PIN			"0000"
#define USER_PIN		"0000"

/* Set up a client/server to connect locally (usually) or to a custom
   address and port if we're running on distinct machines.  For the client
   this simply tells it where to connect, for the server in loopback mode
   this binds it to the local address so that we don't inadvertently open
   up outside ports */

static BOOLEAN setConnectInfo( const CRYPT_SESSION cryptSession,
							   const char *address, const int port,
							   const int timeout )
	{
	int status;

	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_SERVER_NAME,
									  address,  strlen( address ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_SERVER_PORT, port );
	if( cryptStatusOK( status ) && timeout != CRYPT_USE_DEFAULT )
		{
		cryptSetAttribute( cryptSession, CRYPT_OPTION_NET_READTIMEOUT,
						   NET_TIMEOUT );
		status = cryptSetAttribute( cryptSession,
									CRYPT_OPTION_NET_WRITETIMEOUT,
									NET_TIMEOUT );
		}
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttribute/AttributeString() failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	return( TRUE );
	}

/* Create a CA certificate in a device */

static const CERT_DATA rootCACertData[] = {
	/* Identification information */
	{ CRYPT_CERTINFO_COUNTRYNAME, IS_STRING, 0, "AT" },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, IS_STRING, 0, "IAEA" },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, IS_STRING, 0, "SGTIE" },
	{ CRYPT_CERTINFO_COMMONNAME, IS_STRING, 0, "IAEA CA root" },

	/* Self-signed X.509v3 CA certificate */
	{ CRYPT_CERTINFO_SELFSIGNED, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_CA, IS_NUMERIC, TRUE },
	{ CRYPT_CERTINFO_KEYUSAGE, IS_NUMERIC,
	  CRYPT_KEYUSAGE_KEYCERTSIGN | CRYPT_KEYUSAGE_CRLSIGN },

	/* Access information */
	{ CRYPT_ATTRIBUTE_CURRENT, IS_NUMERIC, CRYPT_CERTINFO_AUTHORITYINFO_RTCS },
	{ CRYPT_CERTINFO_UNIFORMRESOURCEIDENTIFIER, IS_STRING, 0, "http://localhost" },

	{ CRYPT_ATTRIBUTE_NONE, IS_VOID }
	};

static int createCACert( const CRYPT_DEVICE cryptDevice )
	{
	CRYPT_CONTEXT cryptContext;
	CRYPT_CERTIFICATE cryptCert;
	int status;

	/* Generate a key in the device */
	printf( "Generating a CA key in the device..." );
	status = cryptDeviceCreateContext( cryptDevice, &cryptContext,
						( SERVER_DEVICE_TYPE == CRYPT_DEVICE_FORTEZZA ) ? \
							CRYPT_ALGO_DSA : CRYPT_ALGO_RSA );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptDeviceCreateContext() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttributeString( cryptContext, CRYPT_CTXINFO_LABEL,
									  "CA key", strlen( "CA key" ) );
	if( cryptStatusOK( status ) )
		status = cryptGenerateKey( cryptContext );
	if( cryptStatusError( status ) )
		{
		cryptDestroyContext( cryptContext );
		printf( "\ncryptGenerateKey() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	/* Create a certificate for the key */
	printf( "Generating a CA certificate for the key..." );
	status = cryptCreateCert( &cryptCert, CRYPT_UNUSED, 
							  CRYPT_CERTTYPE_CERTIFICATE );
	if( cryptStatusError( status ) )
		return( FALSE );
	status = cryptSetAttribute( cryptCert,
						CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, cryptContext );
	if( cryptStatusOK( status ) && \
		!addCertFields( cryptCert, rootCACertData, __LINE__ ) )
		return( FALSE );
	if( cryptStatusOK( status ) )
		status = cryptSignCert( cryptCert, cryptContext );
	cryptDestroyContext( cryptContext );
	if( cryptStatusError( status ) )
		{
		cryptDestroyCert( cryptCert );
		printf( "\nCreation of certificate failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	/* Update the key with the CA certificate */
	printf( "Updating device with certificate..." );
	status = cryptAddPublicKey( cryptDevice, cryptCert );
	cryptDestroyCert( cryptCert );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptAddPublicKey() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	return( TRUE );
	}

/* Connect to a device */

static int connectDevice( CRYPT_DEVICE *cryptDevice, CRYPT_DEVICE_TYPE type,
						  const int slotNo )
	{
	char buffer[ 128 ];
	int status;

	/* Clear return value */
	*cryptDevice = -1;

	/* Connect to the device */
	if( slotNo == CRYPT_USE_DEFAULT )
		{
		printf( "Connecting to crypto device in default slot..." );
		strcpy( buffer, "[Autodetect]" );
		}
	else
		{
		printf( "Connecting to crypto device in slot %d...", slotNo );
		sprintf( buffer, "[Autodetect]::%d", slotNo );
		}
	status = cryptDeviceOpen( cryptDevice, CRYPT_UNUSED, type, buffer );
	if( cryptStatusError( status ) )
		{
		if( status == CRYPT_ERROR_PARAM3 || status == CRYPT_ERROR_NOTFOUND )
			puts( "\nDevice not detected, skipping test." );
		else
			printf( "\ncryptDeviceOpen() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	return( TRUE );
	}

/* Log on to a device */

static int logonDevice( const CRYPT_DEVICE cryptDevice, const char *userPIN )
	{
	char tokenLabel[ CRYPT_MAX_TEXTSIZE + 1 ];
	int loggedOn, tokenLabelSize, status;

	/* Tell the user what we're talking to */
	status = cryptGetAttributeString( cryptDevice, CRYPT_DEVINFO_LABEL,
									  tokenLabel, &tokenLabelSize );
	if( cryptStatusError( status ) )
		puts( "(Device doesn't appear to have a label)." );
	else
		{
		tokenLabel[ tokenLabelSize ] = '\0';
		printf( "Device label is '%s'.\n", tokenLabel );
		}

	/* See if we need to authenticate ourselves */
	status = cryptGetAttribute( cryptDevice, CRYPT_DEVINFO_LOGGEDIN,
								&loggedOn );
	if( cryptStatusError( status ) )
		{
		puts( "Couldn't obtain device login status." );
		return( FALSE );
		}
	if( loggedOn )
		{
		/* Device may not require a login, or has already been logged in
		   via a keypad or similar mechanism */
		puts( "Device is already logged in, skipping login." );
		return( TRUE );
		}

	/* Try and log in */
	printf( "Logging on to the device..." );
	status = cryptSetAttributeString( cryptDevice,
								CRYPT_DEVINFO_AUTHENT_USER, userPIN,
								strlen( userPIN ) );
	if( status == CRYPT_ERROR_INITED )
		{
		/* Some devices may not require any login, in which case we're
		   done */
		puts( " device is already logged in." );
		return( TRUE );
		}
	if( status == CRYPT_ERROR_NOTINITED )
		{
		/* It's an uninitialised device, tell the user and exit */
		puts( " device needs to be initialised." );
		return( FALSE );
		}
	if( cryptStatusError( status ) )
		{
		printf( "\nDevice login failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );
	return( TRUE );
	}

/* Initialise a device */

static int initDevice( const CRYPT_DEVICE cryptDevice,
					   const char *defaultSSOPIN, const char *ssoPIN,
					   const char *userPIN )
	{
	int status;

	/* Most devices don't distinguish between zeroisation and initialisation
	   so we only need to zeroise the device if it's a Fortezza card */
	if( CLIENT_DEVICE_TYPE == CRYPT_DEVICE_FORTEZZA )
		{
		printf( "Zeroising device..." );
		status = cryptSetAttributeString( cryptDevice,
						CRYPT_DEVINFO_ZEROISE, FORTEZZA_ZEROISE_PIN,
						strlen( FORTEZZA_ZEROISE_PIN ) );
		if( cryptStatusError( status ) )
			{
			printf( "\nCouldn't zeroise device, status = %d, line %d.\n",
					status, __LINE__ );
			return( FALSE );
			}
		puts( " done." );
		}

	/* Initialise the device and set the SO PIN */
	printf( "Initialising device with default SO PIN '%s'...",
			defaultSSOPIN );
	status = cryptSetAttributeString( cryptDevice, CRYPT_DEVINFO_INITIALISE,
									  defaultSSOPIN, strlen( defaultSSOPIN ) );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't initialise device, status = %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );
	printf( "Setting SO PIN to '%s'...", ssoPIN );
	status = cryptSetAttributeString( cryptDevice,
									  CRYPT_DEVINFO_SET_AUTHENT_SUPERVISOR,
									  ssoPIN, strlen( ssoPIN ) );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't set SO PIN, status = %d, line %d.\n", status,
				__LINE__ );
		return( FALSE );
		}
	puts( " done." );

	/* If it's a Fortezza card, create a dummy PAA key and install its 
	   certificate.  We have to do it at this point because the operation is 
	   only allowed in the SSO initialised state.  In addition we can't use 
	   the card for this operation because certificate slot 0 is a data-only 
	   slot (that is, it can't correspond to a key held on the card) so we 
	   create a dummy external certificate and use that */
	if( CLIENT_DEVICE_TYPE == CRYPT_DEVICE_FORTEZZA )
		{
		CRYPT_CERTIFICATE cryptCert;
		CRYPT_CONTEXT signContext;

		printf( "Loading PAA certificate..." );
		if( !loadDSAContexts( CRYPT_UNUSED, &signContext, NULL ) )
			return( FALSE );
		status = cryptCreateCert( &cryptCert, CRYPT_UNUSED,
								  CRYPT_CERTTYPE_CERTIFICATE );
		if( cryptStatusError( status ) )
			return( FALSE );
		status = cryptSetAttribute( cryptCert,
						CRYPT_CERTINFO_SUBJECTPUBLICKEYINFO, signContext );
		if( cryptStatusOK( status ) && \
			!addCertFields( cryptCert, paaCertData, __LINE__ ) )
			return( FALSE );
		if( cryptStatusOK( status ) )
			status = cryptSignCert( cryptCert, signContext );
		cryptDestroyContext( signContext );
		if( cryptStatusError( status ) )
			{
			cryptDestroyCert( cryptCert );
			printf( "\nCreation of certificate failed with error code %d, "
					"line %d.\n", status, __LINE__ );
			return( FALSE );
			}
		status = cryptAddPublicKey( cryptDevice, cryptCert );
		cryptDestroyCert( cryptCert );
		if( cryptStatusError( status ) )
			{
			printf( "\ncryptAddPublicKey() failed with error code %d, line "
					"%d.\n", status, __LINE__ );
			return( FALSE );
			}
		puts( " done." );
		}

	/* Set the user PIN and log on as the user.  Some devices automatically
	   log the user in when they set the user password, however, for some
	   devices this is a pseudo-login for which any subsequent operations
	   fail with a not-logged-in error or something similar, so rather than
	   relying on the existing login we always (re-)log in, which performs
	   an explicit logoff if we're already logged in at the time */
	printf( "Setting user PIN to '%s'...", userPIN );
	status = cryptSetAttributeString( cryptDevice,
									  CRYPT_DEVINFO_SET_AUTHENT_USER,
									  userPIN, strlen( userPIN ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptDevice,
										  CRYPT_DEVINFO_AUTHENT_USER,
										  userPIN, strlen( userPIN ) );
	if( cryptStatusError( status ) )
		{
		printf( "\nCouldn't set user PIN/log on as user, status = %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	return( TRUE );
	}

/* Open a certificate store */

static int openCertStore( CRYPT_KEYSET *cryptCertStore )
	{
	int status;

	/* Clear return value */
	*cryptCertStore = -1;

	/* Open the certificate store */
	printf( "Opening CA certificate store..." );
	status = cryptKeysetOpen( cryptCertStore, CRYPT_UNUSED,
							  CRYPT_KEYSET_ODBC_STORE, "testcertstore",
							  CRYPT_KEYOPT_CREATE );
	if( status == CRYPT_ERROR_DUPLICATE )
		status = cryptKeysetOpen( cryptCertStore, CRYPT_UNUSED,
								  CRYPT_KEYSET_ODBC_STORE, "testcertstore",
								  CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	puts( " done." );

	return( TRUE );
	}

/* Add a PKI user to a certificate store */

static int initUserInfo( const CRYPT_KEYSET cryptCertStore,
						 const char *userName )
	{
	CRYPT_CERTIFICATE cryptPKIUser;
	int length, status;

	/* Create the PKI user object and add the user's identification
	   information */
	printf( "Creating PKI user..." );
	status = cryptCreateCert( &cryptPKIUser, CRYPT_UNUSED,
							  CRYPT_CERTTYPE_PKIUSER );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptCreateCert() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttributeString( cryptPKIUser,
									  CRYPT_CERTINFO_COMMONNAME, userName,
									  strlen( userName ) );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptPKIUser, "cryptSetAttributeString()",
							   status, __LINE__ ) );
	puts( " done." );

	/* Add the user info to the certificate store */
	printf( "Adding PKI user to CA certificate store..." );
	status = cryptCAAddItem( cryptCertStore, cryptPKIUser );
	if( status == CRYPT_ERROR_DUPLICATE )
		{
		char userCN[ CRYPT_MAX_TEXTSIZE + 1 ];

		/* The PKI user info is already present from a previous run, get the
		   existing info */
		printf( "\nPKI user information is already present from a previous "
				"run, re-using existing\n  PKI user data..." );
		status = cryptGetAttributeString( cryptPKIUser,
										  CRYPT_CERTINFO_COMMONNAME,
										  userCN, &length );
		if( cryptStatusError( status ) )
			return( attrErrorExit( cryptPKIUser, "cryptGetAttribute()",
								   status, __LINE__ ) );
		userCN[ length ] = '\0';
		cryptDestroyCert( cryptPKIUser );
		status = cryptCAGetItem( cryptCertStore, &cryptPKIUser,
								 CRYPT_CERTTYPE_PKIUSER, CRYPT_KEYID_NAME,
								 userCN );
		}
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAAddItem()", status,
							  __LINE__ ) );
	puts( " done." );

	/* Display the information for the user */
	if( !printCertInfo( cryptPKIUser ) )
		return( FALSE );

	/* Clean up */
	cryptDestroyCert( cryptPKIUser );
	return( TRUE );
	}

/* Get details for a PKI user */

static int getUserInfo( char *userID, char *issuePW )
	{
#ifndef CLIENT_ID
	CRYPT_KEYSET cryptCertStore;
	CRYPT_CERTIFICATE cryptPKIUser;
	int length, status;

	/* Get the PKIUser object from the certificate store */
	status = cryptKeysetOpen( &cryptCertStore, CRYPT_UNUSED,
							  CRYPT_KEYSET_ODBC_STORE, "testcertstore",
							  CRYPT_KEYOPT_NONE );
	if( cryptStatusError( status ) )
		{
		printf( "cryptKeysetOpen() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptCAGetItem( cryptCertStore, &cryptPKIUser,
							 CRYPT_CERTTYPE_PKIUSER, CRYPT_KEYID_NAME,
							 "Test PKI user" );
	cryptKeysetClose( cryptCertStore );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptCertStore, "cryptCAGetItem()", status,
							  __LINE__ ) );

	/* Extract the information from the PKIUser object */
	status = cryptGetAttributeString( cryptPKIUser,
									  CRYPT_CERTINFO_PKIUSER_ID,
									  userID, &length );
	if( cryptStatusOK( status ) )
		{
		userID[ length ] = '\0';
		status = cryptGetAttributeString( cryptPKIUser,
									CRYPT_CERTINFO_PKIUSER_ISSUEPASSWORD,
									issuePW, &length );
		}
	if( cryptStatusOK( status ) )
		issuePW[ length ] = '\0';
	cryptDestroyCert( cryptPKIUser );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptPKIUser, "cryptGetAttribute()", status,
							   __LINE__ ) );
#else
	strcpy( userID, CLIENT_ID );
	strcpy( issuePW, CLIENT_AUTHENTICATOR );
#endif /* CLIENT_ID */

	/* We've got what we need, tell the user what we're doing */
	printf( "Using user name %s, password %s.\n", userID, issuePW );
	return( TRUE );
	}

/* Run the PnP PKI server */

static int pnpServer( const CRYPT_DEVICE cryptDevice,
					  const CRYPT_KEYSET cryptCertStore )
	{
	CRYPT_SESSION cryptSession;
	CRYPT_CONTEXT cryptPrivateKey;
	int caCertTrusted, status;

	/* Perform a cleanup action to remove any leftover requests from
	   previous runs */
	status = cryptCACertManagement( NULL, CRYPT_CERTACTION_CLEANUP, 
									cryptCertStore, CRYPT_UNUSED, 
									CRYPT_UNUSED );
	if( cryptStatusError( status ) )
		{
		printf( "\nCA certificate store cleanup failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Get the CA's key from the device and make it trusted for PKIBoot
	   functionality.  If we're running the test in loopback mode with the
	   Fortezza interface we can't have both the client and server using
	   Fortezza cards due to Spyrus driver bugs, and also can't have the
	   client and server as PKCS #11 and Fortezza due to other driver bugs,
	   so we have to fake the CA key using a software-only implementation */
	printf( "Making CA certificate trusted for PKIBoot..." );
#ifndef CLIENT_ID
	if( CLIENT_DEVICE_TYPE == CRYPT_DEVICE_FORTEZZA )
		status = getPrivateKey( &cryptPrivateKey, CA_PRIVKEY_FILE,
								CA_PRIVKEY_LABEL, TEST_PRIVKEY_PASSWORD );
	else
#endif /* CLIENT_ID */
		status = cryptGetPrivateKey( cryptDevice, &cryptPrivateKey,
									 CRYPT_KEYID_NAME, "CA key", NULL );
	if( cryptStatusError( status ) )
		{
		printf( "\nCA private key read failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	cryptGetAttribute( cryptPrivateKey, CRYPT_CERTINFO_TRUSTED_IMPLICIT,
					   &caCertTrusted );
	cryptSetAttribute( cryptPrivateKey, CRYPT_CERTINFO_TRUSTED_IMPLICIT, 1 );
	puts( " done." );

	/* Create the CMP session and add the CA key and certificate store */
	printf( "Creating CMP server session..." );
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP_SERVER );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptCreateSession() failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_PRIVATEKEY, cryptPrivateKey );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
							CRYPT_SESSINFO_KEYSET, cryptCertStore );
	if( cryptStatusError( status ) )
		return( attrErrorExit( cryptSession, "cryptSetAttribute()",
							   status, __LINE__ ) );
	if( !setConnectInfo( cryptSession, SERVER_MACHINE_ADDRESS,
						 SERVER_MACHINE_PORT, NET_TIMEOUT ) )
		return( FALSE );
	puts( " done." );

	/* Activate the session */
	status = activatePersistentServerSession( cryptSession, TRUE );
	if( cryptStatusError( status ) )
		return( extErrorExit( cryptSession, "Attempt to activate CMP "
							  "server session", status, __LINE__ ) );

	/* Clean up */
	cryptDestroySession( cryptSession );
	if( !caCertTrusted )
		cryptSetAttribute( cryptPrivateKey,
						   CRYPT_CERTINFO_TRUSTED_IMPLICIT, 0 );

	return( TRUE );
	}

/* Run the PnP PKI client */

static int pnpClient( const CRYPT_DEVICE cryptDevice, const char *userID,
					  const char *issuePW )
	{
	CRYPT_SESSION cryptSession;
	int status;

	/* Create the CMP session and set up the information we need for the
	   plug-and-play PKI process */
	printf( "Creating CMP client session..." );
	status = cryptCreateSession( &cryptSession, CRYPT_UNUSED,
								 CRYPT_SESSION_CMP );
	if( cryptStatusError( status ) )
		{
		printf( "\ncryptCreateSession() failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}
	status = cryptSetAttributeString( cryptSession,
									  CRYPT_SESSINFO_USERNAME, userID,
									  paramStrlen( userID ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttributeString( cryptSession,
										  CRYPT_SESSINFO_PASSWORD,
										  issuePW, paramStrlen( issuePW ) );
	if( cryptStatusOK( status ) )
		status = cryptSetAttribute( cryptSession,
									CRYPT_SESSINFO_CMP_PRIVKEYSET,
									cryptDevice );
	if( cryptStatusError( status ) )
		{
		printf( "\nAddition of session information failed with error code "
				"%d, line %d.\n", status, __LINE__ );
		return( FALSE );
		}
	if( !setConnectInfo( cryptSession, SERVER_MACHINE_ADDRESS,
						 SERVER_MACHINE_PORT, NET_TIMEOUT ) )
		return( FALSE );
	puts( " done." );

	/* Activate the session */
	printf( "Obtaining keys and certs..." );
	status = cryptSetAttribute( cryptSession, CRYPT_SESSINFO_ACTIVE, TRUE );
	if( cryptStatusError( status ) )
		{
		printExtError( cryptSession, "\nAttempt to activate plug-and-play "
					   "PKI client session", status, __LINE__ );
		cryptDestroySession( cryptSession );
		return( FALSE );
		}
	puts( " done." );

	/* Clean up */
	cryptDestroySession( cryptSession );
	return( TRUE );
	}

/* Client/server test harness */

static int testServer( void )
	{
	CRYPT_KEYSET cryptCertStore;
	CRYPT_DEVICE cryptDevice;
	int status;

	/* Open the device and certificate store */
	status = connectDevice( &cryptDevice, SERVER_DEVICE_TYPE, 0 );
	if( status )
		status = openCertStore( &cryptCertStore );
	if( !status )
		return( FALSE );

	/* Create a CA key in the device.  Due to Spyrus bugs it's necessary to 
	   disconnect from the device and then re-connect after initialising it 
	   or crypto operations fail in various ways.  In older (and even 
	   buggier) devices crypto ops simply fail after initialisation and it 
	   may be necessary to eject and re-insert the card before they'll work 
	   properly */
#if 0
	status = initDevice( cryptDevice, DEFAULT_SSO_PIN, SSO_PIN, USER_PIN );
	cryptDeviceClose( cryptDevice );
	if( status )
		status = connectDevice( &cryptDevice, SERVER_DEVICE_TYPE, 0 );
	if( status )
		status = logonDevice( cryptDevice, USER_PIN );
	if( status )
		status = createCACert( cryptDevice );
#else
	status = logonDevice( cryptDevice, USER_PIN );
#endif /* 0 */
	if( !status )
		return( FALSE );

	/* Init the PKI user */
	status = initUserInfo( cryptCertStore, "Test user #1" );
	if( !status )
		return( FALSE );

	/* Run the PnP PKI server */
	status = pnpServer( cryptDevice, cryptCertStore );

	/* Clean up */
	cryptDeviceClose( cryptDevice );
	cryptKeysetClose( cryptCertStore );

	return( status );
	}

static int testClient( void )
	{
	CRYPT_DEVICE cryptDevice;
	char userID[ CRYPT_MAX_TEXTSIZE + 1 ], issuePW[ CRYPT_MAX_TEXTSIZE + 1 ];
	int status;

	/* Open the device and get the user ID and password from the certificate 
	   store.  Normally this would be communicated directly, for our test 
	   purposes we cheat and get it from the certificate store */
	status = connectDevice( &cryptDevice, CLIENT_DEVICE_TYPE,
							CLIENT_TOKEN_SLOT );
	if( status )
		status = getUserInfo( userID, issuePW );
	if( !status )
		return( FALSE );

	/* Initialise the device */
#if 1
	status = initDevice( cryptDevice, DEFAULT_SSO_PIN, SSO_PIN, USER_PIN );
	cryptDeviceClose( cryptDevice );
	if( status )
		status = connectDevice( &cryptDevice, CLIENT_DEVICE_TYPE,
								CLIENT_TOKEN_SLOT );
	if( status )
		status = logonDevice( cryptDevice, USER_PIN );
#else
	status = logonDevice( cryptDevice, USER_PIN );
#endif /* 0 */
	if( !status )
		return( FALSE );

	/* Run the PnP PKI client */
	status = pnpClient( cryptDevice, userID, issuePW );

	/* Clean up */
	cryptDeviceClose( cryptDevice );

	return( status );
	}

/* Perform a client/server loopback test */

#ifdef WINDOWS_THREADS

unsigned __stdcall serverThread( void *dummy )
	{
	testServer();
	_endthreadex( 0 );
	return( 0 );
	}

int testDeviceLifeCycle( void )
	{
#ifdef CLIENT_ID
#if 1
	return( testClient() );
#else
	return( testServer() );
#endif /* 0 */
#else
	HANDLE hThread;
	unsigned threadID;
	int status;

	/* Start the server and wait 15s for it to initialise */
	hThread = ( HANDLE ) _beginthreadex( NULL, 0, serverThread,
										 NULL, 0, &threadID );
	Sleep( 15000 );

	/* Connect to the local server with PKIBoot enabled */
	status = testClient();
	if( WaitForSingleObject( hThread, 15000 ) == WAIT_TIMEOUT )
		{
		puts( "Warning: Server thread is still active due to session "
			  "negotiation failure,\n         this will cause an error "
			  "condition when cryptEnd() is called due\n         to "
			  "resources remaining allocated.  Press a key to continue." );
		getchar();
		}
	CloseHandle( hThread );

	return( status );
#endif /* CLIENT_ID */
	}
#endif /* WINDOWS_THREADS */
#endif /* WinCE */

#endif /* TEST_DEVICE */

/****************************************************************************
*																			*
*							User Management Routines Test					*
*																			*
****************************************************************************/

#ifdef TEST_USER

int testUser( void )
	{
	CRYPT_USER cryptUser;
	int status;

	puts( "Testing (minimal) user management functions..." );

	/* Perform a zeroise.  This currently isn't done because (a) it would
	   zeroise all user data whenever anyone runs the self-test and (b) the
	   external API to trigger this isn't defined yet */
/*	status = cryptZeroise( ... ); */

	/* Log in as primary SO using the zeroisation password.  Because of the
	   above situation this currently performs an implicit zeroise */
	status = cryptLogin( &cryptUser, TEXT( "Security officer" ),
						 TEXT( "zeroised" ) );
	if( cryptStatusError( status ) )
		{
		printf( "cryptLogin() (Primary SO) failed with error code %d, line "
				"%d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Set the SO password */
	status = cryptSetAttributeString( cryptUser, CRYPT_USERINFO_PASSWORD,
									  TEST_PASSWORD, paramStrlen( TEST_PASSWORD ) );
	if( cryptStatusError( status ) )
		{
		printf( "cryptSetAttributeString() failed with error code %d, "
				"line %d.\n", status, __LINE__ );
		return( FALSE );
		}

	/* Log out and log in again with the new password.  At the moment it's
	   possible to use any password until the PKCS #15 attribute situation
	   is resolved */
	status = cryptLogout( cryptUser );
	if( cryptStatusError( status ) )
		{
		printf( "cryptLogout() failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}
	status = cryptLogin( &cryptUser, TEXT( "Security officer" ),
						 TEST_PASSWORD );
	if( cryptStatusError( status ) )
		{
		printf( "cryptLogin() (SO) failed with error code %d, line %d.\n",
				status, __LINE__ );
		return( FALSE );
		}

	/* Clean up */
	cryptLogout( cryptUser );
	puts( "User management tests succeeded.\n" );
	return( TRUE );
	}

#endif /* TEST_USER */
