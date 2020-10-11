/****************************************************************************
*																			*
*							Certificate DN Routines							*
*						Copyright Peter Gutmann 1996-2016					*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "cert.h"
  #include "dn.h"
#else
  #include "cert/cert.h"
  #include "cert/dn.h"
#endif /* Compiler-specific includes */

#ifdef USE_CERTIFICATES

/****************************************************************************
*																			*
*							DN Information Tables							*
*																			*
****************************************************************************/

/* A macro to make make declaring DN OIDs simpler */

#define MKDNOID( value )			MKOID( "\x06\x03" value )

/* Type information for DN components.  If the OID doesn't correspond to a 
   valid cryptlib component (i.e. it's one of the 1,001 other odd things that 
   can be crammed into a DN) then we can't directly identify it with a type 
   but instead return a simple integer value in the information table.  This 
   works because the certificate component values don't start until x000 */

static const DN_COMPONENT_INFO certInfoOIDs[] = {
	/* Useful components, identified by attribute */
	{ CRYPT_CERTINFO_COMMONNAME, MKDNOID( "\x55\x04\x03" ), 
	  "cn", 2, "oid.2.5.4.3", 11, CRYPT_MAX_TEXTSIZE, FALSE, TRUE },
	{ CRYPT_CERTINFO_COUNTRYNAME, MKDNOID( "\x55\x04\x06" ), 
	  "c", 1, "oid.2.5.4.6", 11, 2, FALSE, FALSE },
	{ CRYPT_CERTINFO_LOCALITYNAME, MKDNOID( "\x55\x04\x07" ), 
	  "l", 1, "oid.2.5.4.7", 11, 128, FALSE, TRUE },
	{ CRYPT_CERTINFO_STATEORPROVINCENAME, MKDNOID( "\x55\x04\x08" ), 
	  "sp", 2, "oid.2.5.4.8", 11, 128, FALSE, TRUE },
	{ CRYPT_CERTINFO_ORGANIZATIONNAME, MKDNOID( "\x55\x04\x0A" ), 
	  "o", 1, "oid.2.5.4.10", 12, CRYPT_MAX_TEXTSIZE, FALSE, TRUE },
	{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, MKDNOID( "\x55\x04\x0B" ), 
	  "ou", 2, "oid.2.5.4.11", 12, CRYPT_MAX_TEXTSIZE, FALSE, TRUE },

	/* Non-useful components, identified by index value */
	{ 1, MKDNOID( "\x55\x04\x01" ),		/* aliasObjectName (2 5 4 1) */
	  "oid.2.5.4.1", 11, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 2, MKDNOID( "\x55\x04\x02" ),		/* knowledgeInformation (2 5 4 2) */
	  "oid.2.5.4.2", 11, NULL, 0, MAX_ATTRIBUTE_SIZE /*32768*/, FALSE, FALSE },
	{ 3, MKDNOID( "\x55\x04\x04" ),		/* surname (2 5 4 4) */
	  "s", 1, "oid.2.5.4.4", 11, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 4, MKDNOID( "\x55\x04\x05" ),		/* serialNumber (2 5 4 5) */
	  "sn", 2, "oid.2.5.4.5", 11, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 5, MKDNOID( "\x55\x04\x09" ),		/* streetAddress (2 5 4 9) */
	  "st", 2, "oid.2.5.4.9", 11, 128, FALSE, FALSE },
	{ 6, MKDNOID( "\x55\x04\x0C" ),		/* title (2 5 4 12) */
	  "t", 1, "oid.2.5.4.12", 12, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 7, MKDNOID( "\x55\x04\x0D" ),		/* description (2 5 4 13) */
	  "d", 1, "oid.2.5.4.13", 12, 1024, FALSE, FALSE },
	{ 8, MKDNOID( "\x55\x04\x0E" ),		/* searchGuide (2 5 4 14) */
	  "oid.2.5.4.14", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 9, MKDNOID( "\x55\x04\x0F" ),		/* businessCategory (2 5 4 15) */
	  "bc", 2, "oid.2.5.4.15", 12, 128, FALSE, FALSE },
	{ 10, MKDNOID( "\x55\x04\x10" ),	/* postalAddress (2 5 4 16) */
	  "oid.2.5.4.16", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 11, MKDNOID( "\x55\x04\x11" ),	/* postalCode (2 5 4 17) */
	  "oid.2.5.4.17", 12, NULL, 0, 40, FALSE, FALSE },
	{ 12, MKDNOID( "\x55\x04\x12" ),	/* postOfficeBox (2 5 4 18) */
	  "oid.2.5.4.18", 12, NULL, 0, 40, FALSE, FALSE },
	{ 13, MKDNOID( "\x55\x04\x13" ),	/* physicalDeliveryOfficeName (2 5 4 19) */
	  "oid.2.5.4.19", 12, NULL, 0, 128, FALSE, FALSE },
	{ 14, MKDNOID( "\x55\x04\x14" ),	/* telephoneNumber (2 5 4 20) */
	  "oid.2.5.4.20", 12, NULL, 0, 32, FALSE, FALSE },
	{ 15, MKDNOID( "\x55\x04\x15" ),	/* telexNumber (2 5 4 21) */
	  "oid.2.5.4.21", 12, NULL, 0, 14, FALSE, FALSE },
	{ 16, MKDNOID( "\x55\x04\x16" ),	/* teletexTerminalIdentifier (2 5 4 22) */
	  "oid.2.5.4.22", 12, NULL, 0, 24, FALSE, FALSE },
	{ 17, MKDNOID( "\x55\x04\x17" ),	/* facsimileTelephoneNumber (2 5 4 23) */
	  "oid.2.5.4.23", 12, NULL, 0, 32, FALSE, FALSE },
	{ 18, MKDNOID( "\x55\x04\x18" ),	/* x121Address (2 5 4 24) */
	  "oid.2.5.4.24", 12, NULL, 0, 15, FALSE, FALSE },
	{ 19, MKDNOID( "\x55\x04\x19" ),	/* internationalISDNNumber (2 5 4 25) */
	  "isdn", 4, "oid.2.5.4.25", 12, 16, FALSE, FALSE },
	{ 20, MKDNOID( "\x55\x04\x1A" ),	/* registeredAddress (2 5 4 26) */
	  "oid.2.5.4.26", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 21, MKDNOID( "\x55\x04\x1B" ),	/* destinationIndicator (2 5 4 27) */
	  "oid.2.5.4.27", 12, NULL, 0, 128, FALSE, FALSE },
	{ 22, MKDNOID( "\x55\x04\x1C" ),	/* preferredDeliveryMethod (2 5 4 28) */
	  "oid.2.5.4.28", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 23, MKDNOID( "\x55\x04\x1D" ),	/* presentationAddress (2 5 4 29) */
	  "oid.2.5.4.29", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 24, MKDNOID( "\x55\x04\x1E" ),	/* supportedApplicationContext (2 5 4 30) */
	  "oid.2.5.4.30", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 25, MKDNOID( "\x55\x04\x1F" ),	/* member (2 5 4 31) */
	  "oid.2.5.4.31", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 26, MKDNOID( "\x55\x04\x20" ),	/* owner (2 5 4 32) */
	  "oid.2.5.4.32", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 27, MKDNOID( "\x55\x04\x21" ),	/* roleOccupant (2 5 4 33) */
	  "oid.2.5.4.33", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 28, MKDNOID( "\x55\x04\x22" ),	/* seeAlso (2 5 4 34) */
	  "oid.2.5.4.34", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	  /* 0x23-0x28 are certificates/CRLs and some weird encrypted directory components */
	{ 29, MKDNOID( "\x55\x04\x29" ),	/* name (2 5 4 41) */
	  "oid.2.5.4.41", 12, NULL, 0, MAX_ATTRIBUTE_SIZE /*32768*/, FALSE, FALSE },
	{ 30, MKDNOID( "\x55\x04\x2A" ),	/* givenName (2 5 4 42) */
	  "g", 1, "oid.2.5.4.42", 12, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 31, MKDNOID( "\x55\x04\x2B" ),	/* initials (2 5 4 43) */
	  "i", 1, "oid.2.5.4.43", 12, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 32, MKDNOID( "\x55\x04\x2C" ),	/* generationQualifier (2 5 4 44) */
	  "oid.2.5.4.44", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 33, MKDNOID( "\x55\x04\x2D" ),	/* uniqueIdentifier (2 5 4 45) */
	  "oid.2.5.4.45", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 34, MKDNOID( "\x55\x04\x2E" ),	/* dnQualifier (2 5 4 46) */
	  "oid.2.5.4.46", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	  /* 0x2F-0x30 are directory components */
	{ 35, MKDNOID( "\x55\x04\x31" ),	/* distinguishedName (2 5 4 49) */
	  "oid.2.5.4.49", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 36, MKDNOID( "\x55\x04\x32" ),	/* uniqueMember (2 5 4 50) */
	  "oid.2.5.4.50", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 37, MKDNOID( "\x55\x04\x33" ),	/* houseIdentifier (2 5 4 51) */
	  "oid.2.5.4.51", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	  /* 0x34-0x3A are more certificates and weird encrypted directory components */
	{ 38, MKDNOID( "\x55\x04\x41" ),	/* pseudonym (2 5 4 65) */
	  "oid.2.5.4.65", 12, NULL, 0, 128, FALSE, FALSE },
	{ 39, MKDNOID( "\x55\x04\x42" ),	/* communicationsService (2 5 4 66) */
	  "oid.2.5.4.66", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	{ 40, MKDNOID( "\x55\x04\x43" ),	/* communicationsNetwork (2 5 4 67) */
	  "oid.2.5.4.67", 12, NULL, 0, CRYPT_MAX_TEXTSIZE, FALSE, FALSE },
	  /* 0x44-0x49 are more PKI-related attributes */
	{ 41, MKOID( "\x06\x0A\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x01" ),	/* userid (0 9 2342 19200300 100 1 1) */
	  "uid", 3, NULL, 0, CRYPT_MAX_TEXTSIZE, TRUE, FALSE },
	{ 42, MKOID( "\x06\x0A\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x03" ),	/* rfc822Mailbox (0 9 2342 19200300 100 1 3) */
	  "oid.0.9.2342.19200300.100.1.3", 29, NULL, 0, CRYPT_MAX_TEXTSIZE, TRUE, FALSE },
	{ 43, MKOID( "\x06\x0A\x09\x92\x26\x89\x93\xF2\x2C\x64\x01\x19" ),	/* domainComponent (0 9 2342 19200300 100 1 25) */
	  "dc", 2, "oid.0.9.2342.19200300.100.1.25", 30, CRYPT_MAX_TEXTSIZE, TRUE, FALSE },
	{ 44, MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01" ),		/* emailAddress (1 2 840 113549 1 9 1) */
	  "email", 5, "oid.1.2.840.113549.1.9.1", 24, CRYPT_MAX_TEXTSIZE, TRUE, FALSE },
	{ 45, MKOID( "\x06\x07\x02\x82\x06\x01\x0A\x07\x14" ),				/* nameDistinguisher (0 2 262 1 10 7 20) */
	  "oid.0.2.262.1.10.7.20", 21, NULL, 0, CRYPT_MAX_TEXTSIZE, TRUE, FALSE },

	{ CRYPT_ATTRIBUTE_NONE, NULL }, { CRYPT_ATTRIBUTE_NONE, NULL }
	};

/* Check that a country code is valid */

#define xA	( 1 << 0 )
#define xB	( 1 << 1 )
#define xC	( 1 << 2 )
#define xD	( 1 << 3 )
#define xE	( 1 << 4 )
#define xF	( 1 << 5 )
#define xG	( 1 << 6 )
#define xH	( 1 << 7 )
#define xI	( 1 << 8 )
#define xJ	( 1 << 9 )
#define xK	( 1 << 10 )
#define xL	( 1 << 11 )
#define xM	( 1 << 12 )
#define xN	( 1 << 13 )
#define xO	( 1 << 14 )
#define xP	( 1 << 15 )
#define xQ	( 1 << 16 )
#define xR	( 1 << 17 )
#define xS	( 1 << 18 )
#define xT	( 1 << 19 )
#define xU	( 1 << 20 )
#define xV	( 1 << 21 )
#define xW	( 1 << 22 )
#define xX	( 1 << 23 )
#define xY	( 1 << 24 )
#define xZ	( 1 << 25 )

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN checkCountryCode( IN_BUFFER_C( 2 ) const BYTE *countryCode )
	{
	static const long countryCodes[] = {	/* ISO 3166 code table */
	/*	 A  B  C  D  E  F  G  H  I  J  K  L  M  N  O  P  Q  R  S  T  U  V  W  X  Y  Z */
  /*A*/			 xD|xE|xF|xG|	xI|		 xL|xM|xN|xO|	xQ|xR|xS|xT|xU|	  xW|	   xZ,
  /*B*/	xA|xB|	 xD|xE|xF|xG|xH|xI|xJ|		xM|xN|xO|	   xR|xS|xT|   xV|xW|	xY|xZ,
  /*C*/	xA|	  xC|xD|   xF|xG|xH|xI|	  xK|xL|xM|xN|xO|	   xR|		xU|xV|	 xX|xY|xZ,
  /*D*/				xE|			   xJ|xK|	xM|	  xO|							   xZ,
  /*E*/		  xC|	xE|	  xG|xH|						   xR|xS|xT,
  /*F*/							xI|xJ|xK|	xM|	  xO|	   xR,
  /*G*/	xA|xB|	 xD|xE|xF|	 xH|xI|		 xL|xM|xN|	 xP|xQ|xR|xS|xT|xU|	  xW|	xY,
  /*H*/								  xK|	xM|xN|		   xR|	 xT|xU,
  /*I*/			 xD|xE|					 xL|   xN|xO|	xQ|xR|xS|xT,
  /*J*/										xM|	  xO|xP,
  /*K*/				xE|	  xG|xH|xI|			xM|xN|	 xP|   xR|			  xW|	xY|xZ,
  /*L*/	xA|xB|xC|				xI|	  xK|				   xR|xS|xT|xU|xV|		xY,
  /*M*/	xA|	  xC|xD|	  xG|xH|	  xK|xL|xM|xN|xO|xP|xQ|xR|xS|xT|xU|xV|xW|xX|xY|xZ,
  /*N*/	xA|	  xC|	xE|xF|xG|	xI|		 xL|	  xO|xP|   xR|		xU|			   xZ,
  /*O*/										xM,
  /*P*/	xA|			xE|xF|xG|xH|	  xK|xL|xM|xN|		   xR|xS|xT|	  xW|	xY,
  /*Q*/	xA,
  /*R*/				xE|							  xO|				xU|	  xW,
  /*S*/	xA|xB|xC|xD|xE|	  xG|xH|xI|xJ|xK|xL|xM|xN|xO|	   xR|	 xT|   xV|		xY|xZ,
  /*T*/		  xC|xD|   xF|xG|xH|   xJ|xK|xL|xM|xN|xO|	   xR|	 xT|   xV|xW|	   xZ,
  /*U*/	xA|				  xG|				xM|				  xS|				xY|xZ,
  /*V*/	xA|	  xC|	xE|	  xG|	xI|			   xN|					xU,
  /*W*/				   xF|									  xS,
  /*X*/	0,
  /*Y*/				xE|											 xT|xU,
  /*Z*/	xA|									xM|							  xW,
		0, 0	/* Catch overflows */
		};
	const int cc0 = countryCode[ 0 ] - 'A';
	const int cc1 = countryCode[ 1 ] - 'A';

	assert( isReadPtr( countryCode, 2 ) );

	/* Check that the country code is present in the table of valid ISO 3166
	   codes.  Note the explicit declaration of the one-bit as '1L', this is
	   required because the shift amount can be greater than the word size on
	   16-bit systems */
	if( cc0 < 0 || cc0 > 25 || cc1 < 0 || cc1 > 25 )
		return( FALSE );
	return( ( countryCodes[ cc0 ] & ( 1L << cc1 ) ) ? TRUE : FALSE );
	}

/* Determine the sort priority for DN components */

CHECK_RETVAL_RANGE( 0, 10 ) \
static int dnSortOrder( IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type )
	{
	static const MAP_TABLE dnSortOrderTbl[] = {
		{ CRYPT_CERTINFO_COUNTRYNAME, 0 },
		{ CRYPT_CERTINFO_STATEORPROVINCENAME, 1 },
		{ CRYPT_CERTINFO_LOCALITYNAME, 2 },
		{ CRYPT_CERTINFO_ORGANIZATIONNAME, 3 },
		{ CRYPT_CERTINFO_ORGANIZATIONALUNITNAME, 4 },
		{ CRYPT_CERTINFO_COMMONNAME, 5 },
		{ CRYPT_ERROR, 0 }, { CRYPT_ERROR, 0 }
		};
	int status, value;

	REQUIRES( type >= CRYPT_CERTINFO_FIRST_DN && \
			  type <= CRYPT_CERTINFO_LAST_DN );

	status = mapValue( type, &value, dnSortOrderTbl, 
					   FAILSAFE_ARRAYSIZE( dnSortOrderTbl, MAP_TABLE ) );
	ENSURES( cryptStatusOK( status ) );

	return( value );
	}

/****************************************************************************
*																			*
*								Utility Functions							*
*																			*
****************************************************************************/

/* Sanity-check a DN component */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN sanityCheckDNComponent( IN const DN_COMPONENT *dnComponent )
	{
	assert( isReadPtr( dnComponent, sizeof( DN_COMPONENT ) ) );

	REQUIRES_B( dnComponent != NULL );

	/* Check general DN component data */
	if( dnComponent->type < 1 || \
		( dnComponent->type > 50 && \
		  dnComponent->type < CRYPT_CERTINFO_FIRST_DN ) || \
		dnComponent->type > CRYPT_CERTINFO_LAST_DN || \
		dnComponent->typeInfo == NULL )
		{
		DEBUG_PUTS(( "sanityCheckDNComponent: General info" ));
		return( FALSE );
		}
	if( !CHECK_FLAGS( dnComponent->flags, DN_FLAG_NONE, DN_FLAG_MAX ) )
		{
		DEBUG_PUTS(( "sanityCheckDNComponent: Flags" ));
		return( FALSE );
		}

	/* Check safe pointers */
	if( !DATAPTR_ISVALID( dnComponent->prev ) || \
		!DATAPTR_ISVALID( dnComponent->next ) )
		{
		DEBUG_PUTS(( "sanityCheckDNComponent: Safe pointers" ));
		return( FALSE );
		}
	
	/* Check encoding information */
	if( dnComponent->value != dnComponent->storage || \
		!isShortIntegerRange( dnComponent->valueLength ) )
		{
		DEBUG_PUTS(( "sanityCheckDNComponent: Value" ));
		return( FALSE );
		}
	if( dnComponent->asn1EncodedStringType < 0 || \
		dnComponent->asn1EncodedStringType > 0xFF || \
		!isShortIntegerRange( dnComponent->encodedRDNdataSize ) || \
		!isShortIntegerRange( dnComponent->encodedAVAdataSize ) )
		{
		DEBUG_PUTS(( "sanityCheckDNComponent: Encoding info" ));
		return( FALSE );
		}

	return( TRUE );
	}

/* Find a DN component in a DN component list by type and by OID.  This also
   takes a count parameter to return the n'th occurrence of a DN component.
   Note that we use counted-access rather than the usual getFirst()/getNext()
   enumeration mechanism because some of the accesses are coming from outside
   cryptlib and may be non-atomic.  In other words there's no guarantee that
   the caller won't perform a delete between getNext()'s, leaving a dangling
   reference to trip up the next getNext() call */

CHECK_RETVAL_PTR \
static DN_COMPONENT *findDNComponent( const DATAPTR_DN dnComponentList,
									  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
									  IN_RANGE( 0, 100 ) const int count,
									  IN_BUFFER_OPT( valueLength ) const void *value,
									  IN_LENGTH_SHORT_Z const int valueLength )
	{
	DN_COMPONENT *listPtr;
	int matchCount = 0, LOOP_ITERATOR;

	assert( ( value == NULL && valueLength == 0 ) || \
			isReadPtrDynamic( value, valueLength ) );
			/* We may be doing the lookup purely by type */

	REQUIRES_N( DATAPTR_ISVALID( dnComponentList ) );
	REQUIRES_N( type >= CRYPT_CERTINFO_FIRST_DN && \
				type <= CRYPT_CERTINFO_LAST_DN );
	REQUIRES_N( count >= 0 && count <= 100 );
	REQUIRES_N( ( value == NULL && valueLength == 0 ) || \
				( value != NULL && \
				  isShortIntegerRangeNZ( valueLength ) ) );
	REQUIRES_N( ( value == NULL && count >= 0 ) || \
				( value != NULL && count == 0 ) );

	/* Find the position of this component in the list */
	LOOP_MED( listPtr = DATAPTR_GET( dnComponentList ), listPtr != NULL,
			  listPtr = DATAPTR_GET( listPtr->next ) )
		{
		REQUIRES_N( sanityCheckDNComponent( listPtr ) );

		/* If it's of a different type then it can't be a match */
		if( listPtr->type != type )
			continue;
		
		/* If we're doing the lookup purely by type then we've found a 
		   match */
		if( value == NULL )
			{
			/* If we're looking for the n-th match rather than the first
			   matching item, continue if we haven't reached the n-th item 
			   yet */
			if( matchCount < count )
				{
				matchCount++;
				continue;
				}
			return( ( DN_COMPONENT * ) listPtr );
			}

		/* Check for a match by value */
		if( listPtr->valueLength == valueLength && \
			!memcmp( listPtr->value, value, valueLength ) ) 
			return( ( DN_COMPONENT * ) listPtr );
		}
	ENSURES_N( LOOP_BOUND_OK );

	return( NULL );
	}

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 2 ) ) \
static DN_COMPONENT *findDNComponentByOID( const DATAPTR_DN dnComponentList,
										   IN_BUFFER( oidLength ) const BYTE *oid, 
										   IN_LENGTH_OID const int oidLength )
	{
	DN_COMPONENT *listPtr;
	int LOOP_ITERATOR;

	assert( isReadPtrDynamic( oid, oidLength ) );

	REQUIRES_N( DATAPTR_ISVALID( dnComponentList ) );
	REQUIRES_N( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	/* Find the position of this component in the list */
	LOOP_MED( listPtr = DATAPTR_GET( dnComponentList ), listPtr != NULL, 
			  listPtr = DATAPTR_GET( listPtr->next ) )
		{
		const DN_COMPONENT_INFO *dnComponentInfo;

		REQUIRES_N( sanityCheckDNComponent( listPtr ) );

		dnComponentInfo = listPtr->typeInfo;
		if( oidLength == sizeofOID( dnComponentInfo->oid ) && \
			!memcmp( dnComponentInfo->oid, oid, oidLength ) )
			return( listPtr );
		}
	ENSURES_N( LOOP_BOUND_OK );

	return( NULL );
	}

/* Find DN information by type, by OID, and by string label */

CHECK_RETVAL_PTR \
static const DN_COMPONENT_INFO *findDNInfo( IN_INT const int type )
	{
	int i, LOOP_ITERATOR;

	REQUIRES_N( ( type >= CRYPT_CERTINFO_FIRST_DN && \
				  type <= CRYPT_CERTINFO_LAST_DN ) || \
				( type > 0 && type < 50 ) );

	/* Find the type information for this component */
	LOOP_LARGE( i = 0, 
				certInfoOIDs[ i ].oid != NULL && \
					i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ),
				i++ )
		{
		if( certInfoOIDs[ i ].type == type )
			return( &certInfoOIDs[ i ] );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ) );

	retIntError_Null();
	}	

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const DN_COMPONENT_INFO *findDNInfoByOID( IN_BUFFER( oidLength ) const BYTE *oid, 
										  IN_LENGTH_OID const int oidLength )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( oid, oidLength ) );

	REQUIRES_N( oidLength >= MIN_OID_SIZE && oidLength <= MAX_OID_SIZE && \
				oidLength == sizeofOID( oid ) );

	LOOP_LARGE( i = 0,
				certInfoOIDs[ i ].oid != NULL && \
					i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ), 
				i++ )
		{
		const DN_COMPONENT_INFO *certInfoOID = &certInfoOIDs[ i ];

		/* Perform a quick check of the OID.  The majority of all DN OIDs
		   are of the form (2 5 4 n), encoded as 0x06 0x03 0x55 0x04 0xnn,
		   so we compare the byte at offset 4 for a quick-reject match 
		   before we go for the full OID match */
		if( oidLength == sizeofOID( certInfoOID->oid ) && \
			certInfoOID->oid[ 4 ] == oid[ 4 ] && \
			!memcmp( certInfoOID->oid, oid, oidLength ) )
			return( certInfoOID );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ) );

	return( NULL );
	}

#ifdef USE_CERT_DNSTRING

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
const DN_COMPONENT_INFO *findDNInfoByLabel( IN_BUFFER( labelLength ) const char *label, 
											IN_LENGTH_SHORT const int labelLength )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( label, labelLength ) );

	REQUIRES_N( isShortIntegerRangeNZ( labelLength ) );

	LOOP_LARGE( i = 0,
				certInfoOIDs[ i ].oid != NULL && \
					i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ), 
				i++ )
		{
		const DN_COMPONENT_INFO *certInfoOID = &certInfoOIDs[ i ];

		if( certInfoOID->nameLen == labelLength && \
			!strCompare( certInfoOID->name, label, labelLength ) )
			return( certInfoOID );
		if( certInfoOID->altName != NULL && \
			certInfoOID->altNameLen == labelLength && \
			!strCompare( certInfoOID->altName, label, labelLength ) )
			return( certInfoOID );
		}
	ENSURES_N( LOOP_BOUND_OK );
	ENSURES_N( i < FAILSAFE_ARRAYSIZE( certInfoOIDs, DN_COMPONENT_INFO ) );

	return( NULL );
	}
#endif /* USE_CERT_DNSTRING */

/* Find the appropriate location in a DN to insert a new component */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 4, 5 ) ) \
static int findDNInsertPoint( const DN_COMPONENT *listHeadPtr,
							  IN_INT const int type,
							  const BOOLEAN fromExternalSource,
							  OUT_PTR_COND DN_COMPONENT **insertPointPtrPtr,
							  OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
									CRYPT_ERRTYPE_TYPE *errorType )	
	{
	const DN_COMPONENT *listPtr, *prevElement = NULL;
	int newValueSortOrder, status, LOOP_ITERATOR;

	assert( isReadPtr( listHeadPtr, sizeof( DN_COMPONENT ) ) );
	assert( isWritePtr( insertPointPtrPtr, sizeof( DN_COMPONENT * ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( ( type >= CRYPT_CERTINFO_FIRST_DN && \
				type <= CRYPT_CERTINFO_LAST_DN ) || \
			  ( type > 0 && type < 50 ) );
	REQUIRES( fromExternalSource == TRUE || fromExternalSource == FALSE );

	/* Clear return values */
	*insertPointPtrPtr = NULL;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If it's being read from encoded certificate data just append it to 
	   the end of the list */
	if( fromExternalSource )
		{
		LOOP_MED( ( prevElement = NULL, listPtr = listHeadPtr ), 
				  listPtr != NULL,
				  ( prevElement = listPtr,
					listPtr = DATAPTR_GET( listPtr->next ) ) )
			{
			REQUIRES( sanityCheckDNComponent( listPtr ) );
			}
		ENSURES( LOOP_BOUND_OK );
		*insertPointPtrPtr = ( DN_COMPONENT * ) prevElement;

		return( CRYPT_OK );
		}

	/* Get the sort order for the new value and make sure that it's valid */
	status = newValueSortOrder = dnSortOrder( type );
	if( cryptStatusError( status ) )
		return( status );

	/* Find the location to insert it */			
	LOOP_MED( ( prevElement = NULL, listPtr = listHeadPtr ), 
			  listPtr != NULL && \
				newValueSortOrder >= dnSortOrder( listPtr->type ),
			  ( prevElement = listPtr,
				listPtr = DATAPTR_GET( listPtr->next ) ) )
		{
		REQUIRES( sanityCheckDNComponent( listPtr ) );

		/* Make sure that this component isn't already present.  We only 
		   allow a single DN component of any type to keep things simple for 
		   the user and to avoid potential problems with implementations
		   that don't handle more than one instance of a given DN component
		   too well */
		if( listPtr->type == type )
			{
			*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
			return( CRYPT_ERROR_INITED );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	*insertPointPtrPtr = ( DN_COMPONENT * ) prevElement;

	return( CRYPT_OK );
	}

/****************************************************************************
*																			*
*								Add DN Components							*
*																			*
****************************************************************************/

/* Insert a DN component into a list.  The type can be either a 
   CRYPT_CERTINFO_xxx value, indicating that it's a standard DN component,
   or a small integer denoting a recognised but nonstandard DN component.  
   In the latter case we don't try to sort the component into the correct 
   position */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 7 ) ) \
int insertDNstring( INOUT DATAPTR_DN *dnListHeadPtr, 
					IN_INT const int type,
					IN_BUFFER( valueLength ) const void *value, 
					IN_LENGTH_SHORT const int valueLength,
					IN_RANGE( 1, 20 ) const int valueStringType,
					IN_FLAGS_Z( DN ) const int flags, 
					OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
						CRYPT_ERRTYPE_TYPE *errorType )
	{
	const DN_COMPONENT_INFO *dnComponentInfo = NULL;
	DATAPTR_ATTRIBUTE listHead = *dnListHeadPtr;
	DN_COMPONENT *newElement, *listHeadPtr, *insertPoint;
	BYTE countryCode[ 8 + 8 ];

	assert( isWritePtr( dnListHeadPtr, sizeof( DATAPTR_DN ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( listHead ) );
	REQUIRES( ( type >= CRYPT_CERTINFO_FIRST_DN && \
				type <= CRYPT_CERTINFO_LAST_DN ) || \
			  ( type > 0 && type < 50 ) );
	REQUIRES( isShortIntegerRangeNZ( valueLength ) );
	REQUIRES( valueStringType >= 1 && valueStringType <= 20 );
	REQUIRES( isFlagRangeZ( flags, DN ) );

	/* Clear return value */
	*errorType = CRYPT_ERRTYPE_NONE;

	/* If the DN is locked against modification we can't make any further
	   updates */
	listHeadPtr = DATAPTR_GET( listHead );
	if( listHeadPtr != NULL && \
		TEST_FLAG( listHeadPtr->flags, DN_FLAG_LOCKED ) )
		return( CRYPT_ERROR_INITED );

	/* Find the type information for this component */
	dnComponentInfo = findDNInfo( type );
	ENSURES( dnComponentInfo != NULL );

	/* Make sure that the length is valid.  If it's being read from an
	   encoded form we allow abnormally-long lengths (although we still keep
	   them within a sensible limit) since this is better than failing to
	   read a certificate because it contains a broken DN.  In addition if a 
	   widechar string is OK we allow a range up to the maximum byte count
	   defined by the widechar size */
#ifdef USE_WIDECHARS
	if( valueLength > ( ( flags & DN_FLAG_NOCHECK ) ? \
							MAX_ATTRIBUTE_SIZE : \
						( dnComponentInfo->wcsOK ) ? \
							( WCSIZE * dnComponentInfo->maxLength ) : \
							dnComponentInfo->maxLength ) )
#else
	if( valueLength > ( ( flags & DN_FLAG_NOCHECK ) ? \
							MAX_ATTRIBUTE_SIZE : dnComponentInfo->maxLength ) )
#endif /* USE_WIDECHARS */
		{
		*errorType = CRYPT_ERRTYPE_ATTR_SIZE;
		return( CRYPT_ARGERROR_NUM1 );
		}

	/* Find the correct place in the list to insert the new element */
	if( listHeadPtr != NULL )
		{
		int status;

		status = findDNInsertPoint( listHeadPtr, type, 
									( flags & DN_FLAG_NOCHECK ) ? \
										TRUE : FALSE,
									&insertPoint, errorType );
		if( cryptStatusError( status ) )
			return( status );
		}
	else
		{
		/* It's an empty list, insert the new element at the start */
		insertPoint = NULL;
		}

	/* If it's a country code, force it to uppercase as per ISO 3166 */
	if( type == CRYPT_CERTINFO_COUNTRYNAME )
		{
		const BYTE *dnStrPtr = value;
		int i, LOOP_ITERATOR;

		/* An ISO 3166 country code must be exactly two characters long, 
		   however some implementations stuff things like abbreviated 
		   country names into the "country code" (e.g. "US" -> "USA") so if 
		   the DN is coming from an external source then we allow longer 
		   values */
		if( flags & DN_FLAG_NOCHECK )
			{
			if( valueLength < 2 || valueLength > 8 )
				return( CRYPT_ERROR_BADDATA );
			}
		else
			{
			if( valueLength != 2 )
				return( CRYPT_ERROR_BADDATA );
			}

		/* Note: When this code is run under BoundsChecker 6.x the toUpper() 
		   conversion will produce garbage on any call after the first one 
		   resulting in the following checks failing */
		LOOP_SMALL( i = 0, i < valueLength, i++ )
			countryCode[ i ] = intToByte( toUpper( dnStrPtr[ i ] ) );
		ENSURES( LOOP_BOUND_OK );
		if( flags & DN_FLAG_NOCHECK )
			{
			/* 'UK' isn't an ISO 3166 country code but may be found in some
			   certificates.  If we find this we quietly convert it to the
			   correct value */
			if( !memcmp( countryCode, "UK", 2 ) )
				memcpy( countryCode, "GB", 2 );
			}
		else
			{
			/* Make sure that the country code is valid */
			if( !checkCountryCode( countryCode ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				return( CRYPT_ERROR_INVALID );
				}
			}

		/* We've fixed up the coutry code information if required, make sure 
		   that we add the fixed form rather than the original */
		value = countryCode;
		}

	/* Allocate memory for the new element and copy over the information */
	REQUIRES( rangeCheck( valueLength, 1, MAX_INTLENGTH_SHORT ) );
	if( ( newElement = ( DN_COMPONENT * ) \
				clAlloc( "insertDNstring", sizeof( DN_COMPONENT ) + \
										   valueLength ) ) == NULL )
		return( CRYPT_ERROR_MEMORY );
	initVarStruct( newElement, DN_COMPONENT, valueLength, value );
	newElement->type = type;
	newElement->typeInfo = dnComponentInfo;
	memcpy( newElement->value, value, valueLength );
	newElement->valueLength = valueLength;
	newElement->valueStringType = valueStringType;
	INIT_FLAGS( newElement->flags, flags );
	DATAPTR_SET( newElement->prev, NULL );
	DATAPTR_SET( newElement->next, NULL );

	ENSURES( sanityCheckDNComponent( newElement ) );

	/* Link it into the list */
	insertDoubleListElement( dnListHeadPtr, insertPoint, newElement, 
							 DN_COMPONENT );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3, 5 ) ) \
int insertDNComponent( INOUT DATAPTR_DN *dnListHeadPtr,
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE componentType,
					   IN_BUFFER( valueLength ) const void *value, 
					   IN_LENGTH_SHORT const int valueLength,
					   OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
							CRYPT_ERRTYPE_TYPE *errorType )
	{
	int valueStringType, dummy1, dummy2, status;

	assert( isWritePtr( dnListHeadPtr, sizeof( DATAPTR_DN ) ) );
	assert( isReadPtrDynamic( value, valueLength ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( componentType >= CRYPT_CERTINFO_FIRST_DN && \
			  componentType <= CRYPT_CERTINFO_LAST_DN );
	REQUIRES( isShortIntegerRangeNZ( valueLength ) );

	/* The value is coming from an external source, make sure that it's
	   representable as a certificate string type.  All that we care
	   about here is the validity so we ignore the returned encoding
	   information */
	status = getAsn1StringInfo( value, valueLength, &valueStringType, 
								&dummy1, &dummy2, TRUE );
	if( cryptStatusError( status ) )
		{
		*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
		return( CRYPT_ARGERROR_STR1 );
		}

	return( insertDNstring( dnListHeadPtr, componentType, value, 
							valueLength, valueStringType, 0, errorType ) );
	}

/****************************************************************************
*																			*
*								Delete DN Components						*
*																			*
****************************************************************************/

/* Delete a DN component from a list */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
static int deleteComponent( INOUT DATAPTR_DN *dnPtr, 
							INOUT DN_COMPONENT *theElement )
	{
	assert( isWritePtr( dnPtr, sizeof( DATAPTR_DN ) ) );
	assert( isWritePtr( theElement, sizeof( DN_COMPONENT ) ) );

	REQUIRES( sanityCheckDNComponent( theElement ) );

	/* Remove the item from the list */
	deleteDoubleListElement( dnPtr, theElement, DN_COMPONENT );

	/* Clear all data in the list item and free the memory */
	endVarStruct( theElement, DN_COMPONENT );
	clFree( "deleteComponent", theElement );

	return( CRYPT_OK );
	}

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int deleteDNComponent( INOUT DATAPTR_DN *dnPtr, 
					   IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
					   IN_BUFFER_OPT( valueLength ) const void *value, 
					   IN_LENGTH_SHORT_Z const int valueLength )
	{
	DATAPTR_DN dn = *dnPtr;
	DN_COMPONENT *listHeadPtr, *itemToDelete;

	assert( isWritePtr( dnPtr, sizeof( DATAPTR_DN ) ) );
	assert( ( value == NULL && valueLength == 0 ) ||
			isReadPtrDynamic( value, valueLength ) );
			/* We may be doing the delete purely by type */

	REQUIRES( DATAPTR_ISVALID( dn ) );
	REQUIRES( type > CRYPT_CERTINFO_FIRST && type < CRYPT_CERTINFO_LAST );
	REQUIRES( ( value == NULL && valueLength == 0 ) || \
			  ( value != NULL && \
				isShortIntegerRangeNZ( valueLength ) ) );

	/* Trying to delete from an empty DN always fails */
	if( DATAPTR_ISNULL( dn ) )
		return( CRYPT_ERROR_NOTFOUND );

	listHeadPtr = DATAPTR_GET( dn );
	REQUIRES( listHeadPtr != NULL );

	/* If the DN is locked against modification then we can't make any 
	   further updates */
	if( TEST_FLAG( listHeadPtr->flags, DN_FLAG_LOCKED ) )
		return( CRYPT_ERROR_PERMISSION );

	/* Find the component in the list and delete it */
	itemToDelete = findDNComponent( dn, type, 0, value, valueLength );
	if( itemToDelete == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	return( deleteComponent( dnPtr, itemToDelete ) );
	}

/* Delete a DN */

STDC_NONNULL_ARG( ( 1 ) ) \
void deleteDN( INOUT_PTR DATAPTR_DN *dnPtr )
	{
	DATAPTR_DN dn = *dnPtr;
	DN_COMPONENT *dnComponentListPtr;
	int LOOP_ITERATOR;

	assert( isWritePtr( dnPtr, sizeof( DATAPTR_DN *) ) );

	REQUIRES_V( DATAPTR_ISSET( dn ) );

	/* Destroy all DN items */
	dnComponentListPtr = DATAPTR_GET( dn );
	ENSURES_V( dnComponentListPtr != NULL );
	ENSURES_V( sanityCheckDNComponent( dnComponentListPtr ) );
	LOOP_MED_CHECK( dnComponentListPtr != NULL )
		{
		DATAPTR_DN itemToFree;
		DN_COMPONENT *itemToFreePtr;
		int status;

		DATAPTR_SET( itemToFree, dnComponentListPtr );
		itemToFreePtr = dnComponentListPtr;
		REQUIRES_V( sanityCheckDNComponent( itemToFreePtr ) );
		REQUIRES_V( DATAPTR_ISVALID( dnComponentListPtr->next ) );
		dnComponentListPtr = DATAPTR_GET( dnComponentListPtr->next );

		/* Another gcc bug, this time in gcc 4.x for 64-bit architectures 
		   (confirmed for x86-64 and ppc64) in which it removes an empty-
		   list check in deleteDoubleListElement() (in fact the emitted 
		   code bears only a passing resemblance to the actual source code, 
		   this appears to be a variant of the broken handling of the
		   nonnull attribute described in misc/analyse.h). The only possible 
		   workaround seems to be to omit the call to deleteComponent() and 
		   just delete the item directly */
#if defined( __GNUC__ ) && ( __GNUC__ == 4 ) && 0
			/* The addition of the REQUIRES() preconditions to 
			   deleteDoubleListElement() seem to have fixed the gcc problem,
			   either that or it was only present in some early 4.0.x versions */
		deleteDoubleListElementSafe( dnComponentListPtrPtr, itemToFree, 
									 DN_COMPONENT );
		endVarStruct( itemToFree, DN_COMPONENT );
		clFree( "deleteComponent", itemToFree );
#else
		status = deleteComponent( &itemToFree, itemToFreePtr );
		if( cryptStatusError( status ) )
			return;
#endif /* gcc 4.x on 64-bit architectures bug workaround */
		}
	ENSURES_V( LOOP_BOUND_OK );

	/* Mark the list as being empty */
	DATAPTR_SET_PTR( dnPtr, NULL );
	}

/****************************************************************************
*																			*
*						Miscellaneous DN Component Functions				*
*																			*
****************************************************************************/

/* Get DN component information */

CHECK_RETVAL STDC_NONNULL_ARG( ( 2, 3 ) ) \
int getDNComponentInfo( IN const DATAPTR_DN dn,
						OUT_ATTRIBUTE_Z CRYPT_ATTRIBUTE_TYPE *type,
						OUT_BOOL BOOLEAN *dnContinues )
	{
	const DN_COMPONENT *dnComponent;

	assert( isWritePtr( type, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( dnContinues, sizeof( BOOLEAN ) ) );

	REQUIRES( DATAPTR_ISSET( dn ) );

	/* Clear return values */
	*type = CRYPT_ATTRIBUTE_NONE;
	*dnContinues = FALSE;

	dnComponent = DATAPTR_GET( dn );
	REQUIRES( dnComponent != NULL );
	REQUIRES( sanityCheckDNComponent( dnComponent ) );

	/* Return the component information to the caller.  The dnContinues 
	   value is a somewhat oddball piece of information that's used in 
	   conjunction with compareDN() when checking subsets of DNs, for 
	   example to see if a DN subset contains more than one element */
	if( dnComponent->type >= CRYPT_CERTINFO_FIRST_DN && \
		dnComponent->type <= CRYPT_CERTINFO_LAST_DN )
		*type = dnComponent->type;
	REQUIRES( DATAPTR_ISVALID( dnComponent->next ) );
	if( DATAPTR_ISSET( dnComponent->next ) )
		*dnContinues = TRUE;

	return( CRYPT_OK );
	}

/* Get the value of a DN component.  This also takes a count parameter to 
   return the n'th occurrence of a DN component, see the comment for
   findDNComponent() for details */

CHECK_RETVAL STDC_NONNULL_ARG( ( 6 ) ) \
int getDNComponentValue( IN_DATAPTR_OPT const DATAPTR_DN dn,
						 IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE type,
						 IN_RANGE( 0, 100 ) const int count,
						 OUT_BUFFER_OPT( valueMaxLength, \
										 *valueLength ) void *value, 
						 IN_LENGTH_SHORT_Z const int valueMaxLength, 
						 OUT_LENGTH_BOUNDED_Z( valueMaxLength ) \
							int *valueLength )
	{
	const DN_COMPONENT *dnComponent;

	assert( ( value == NULL && valueMaxLength == 0 ) || \
			( isWritePtrDynamic( value, valueMaxLength ) ) );
	assert( isWritePtr( valueLength, sizeof( int ) ) );

	REQUIRES( DATAPTR_ISVALID( dn ) );
	REQUIRES( type >= CRYPT_CERTINFO_FIRST_DN && \
			  type <= CRYPT_CERTINFO_LAST_DN );
	REQUIRES( count >= 0 && count <= 100 );
	REQUIRES( ( value == NULL && valueMaxLength == 0 ) || \
			  ( value != NULL && \
				isShortIntegerRangeNZ( valueMaxLength ) ) );

	/* Clear return values */
	*valueLength = 0;
	if( value != NULL )
		memset( value, 0, min( 16, valueMaxLength ) );

	/* Trying to get data from an empty DN always fails */
	if( DATAPTR_ISNULL( dn ) )
		return( CRYPT_ERROR_NOTFOUND );

	dnComponent = findDNComponent( dn, type, count, NULL, 0 );
	if( dnComponent == NULL )
		return( CRYPT_ERROR_NOTFOUND );
	return( attributeCopyParams( value, valueMaxLength, valueLength,
								 dnComponent->value, 
								 dnComponent->valueLength ) );
	}

/* Compare two DNs.  Since this is used for constraint comparisons as well
   as just strict equality checks we provide a flag that, if set, returns
   a match if the first DN is a proper substring of the second DN.  We
   optionally return a pointer to the first mis-matching element in the 
   first DN in case the caller wants to perform further actions with it */

CHECK_RETVAL_BOOL \
BOOLEAN compareDN( IN_DATAPTR_OPT const DATAPTR_DN dn1,
				   IN_DATAPTR_OPT const DATAPTR_DN dn2,
				   const BOOLEAN dn1substring,
				   OUT_DATAPTR_xCOND DATAPTR_DN *mismatchPointPtr )
	{
	DN_COMPONENT *dn1ptr, *dn2ptr;
	int LOOP_ITERATOR;

	assert( mismatchPointPtr == NULL || \
			isWritePtr( mismatchPointPtr, sizeof( DATAPTR_DN * ) ) );

	REQUIRES_B( DATAPTR_ISVALID( dn1 ) && DATAPTR_ISVALID( dn2 ) );
	REQUIRES_B( dn1substring == TRUE || dn1substring == FALSE );
	REQUIRES_B( !( dn1substring == FALSE && mismatchPointPtr != NULL ) );

	/* Clear return value */
	if( mismatchPointPtr != NULL )
		DATAPTR_SET_PTR( mismatchPointPtr, NULL );

	/* Check each DN component for equality */
	LOOP_MED( ( dn1ptr = DATAPTR_GET( dn1 ), \
				dn2ptr = DATAPTR_GET( dn2 ) ),
			  dn1ptr != NULL && dn2ptr != NULL,
			  ( dn1ptr = DATAPTR_GET( dn1ptr->next ), \
				dn2ptr = DATAPTR_GET( dn2ptr->next ) ) )
		{
		REQUIRES_B( sanityCheckDNComponent( dn1ptr ) );
		REQUIRES_B( sanityCheckDNComponent( dn2ptr ) );

		/* If the RDN types differ then the DNs don't match */
		if( dn1ptr->type != dn2ptr->type )
			{
			if( mismatchPointPtr != NULL )
				DATAPTR_SET_PTR( mismatchPointPtr, dn1ptr );
			return( FALSE );
			}

		/* Compare the current RDNs.  In theory we should be using the
		   complex and arcane X.500 name comparison rules but no-one in 
		   their right mind actually does this since they're almost 
		   impossible to get right.  Since everyone else uses memcpy()/
		   memcmp() to handle DN components it's safe to use it here (sic 
		   faciunt omnes).  This also avoids any security problems arising 
		   from the complexity of the code necessary to implement the X.500 
		   matching rules */
		if( dn1ptr->valueLength != dn2ptr->valueLength || \
			memcmp( dn1ptr->value, dn2ptr->value, dn1ptr->valueLength ) ||
			GET_FLAG( dn1ptr->flags, DN_FLAGS_COMPARE_MASK ) != \
				GET_FLAG( dn2ptr->flags, DN_FLAGS_COMPARE_MASK ) )
			{
			if( mismatchPointPtr != NULL )
				DATAPTR_SET_PTR( mismatchPointPtr, dn1ptr );
			return( FALSE );
			}
		}
	ENSURES_B( LOOP_BOUND_OK );

	/* If we've reached the end of both DNs or we're looking for a substring
	   match (which means that the match ended when it reached the end of 
	   the first DN, making it a proper substring of the second) then the 
	   two match */
	if( dn1ptr == NULL && dn2ptr == NULL )
		return( TRUE );
	if( dn1substring && dn1ptr == NULL )
		return( TRUE );

	/* We've reached the end of the DN without finding a match */
	if( mismatchPointPtr != NULL )
		DATAPTR_SET_PTR( mismatchPointPtr, dn1ptr );
	return( FALSE );
	}

/* Copy a DN */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int copyDN( OUT_DATAPTR DATAPTR_DN *dnDestPtr, 
			IN const DATAPTR_DN dnSrc )
	{
	DN_COMPONENT *srcCursor, *destCursor = NULL;
	int LOOP_ITERATOR;

	assert( isWritePtr( dnDestPtr, sizeof( DATAPTR_DN ) ) );

	REQUIRES( DATAPTR_ISSET( dnSrc ) );

	/* Clear return value */
	DATAPTR_SET_PTR( dnDestPtr, NULL );

	/* Copy each element in the source DN */
	LOOP_MED( srcCursor = DATAPTR_GET( dnSrc ), 
			  srcCursor != NULL, 
			  srcCursor = DATAPTR_GET( srcCursor->next ) )
		{
		DN_COMPONENT *newElement;

		REQUIRES( sanityCheckDNComponent( srcCursor ) );

		/* Allocate memory for the new element and copy over the 
		   information.  Since we're copying over the contents of an 
		   existing DN_COMPONENT structure we have to zero the list links 
		   after the copy */
		if( ( newElement = ( DN_COMPONENT * ) \
				clAlloc( "copyDN", \
						 sizeofVarStruct( srcCursor, DN_COMPONENT ) ) ) == NULL )
			{
			deleteDN( dnDestPtr );

			return( CRYPT_ERROR_MEMORY );
			}
		copyVarStruct( newElement, srcCursor, DN_COMPONENT, value );
		DATAPTR_SET( newElement->prev, NULL );
		DATAPTR_SET( newElement->next, NULL );

		ENSURES( sanityCheckDNComponent( newElement ) );

		/* Link it into the list */
		insertDoubleListElement( dnDestPtr, destCursor, newElement, 
								 DN_COMPONENT );
		destCursor = newElement;
		}
	ENSURES( LOOP_BOUND_OK );

	return( CRYPT_OK );
	}

/* Check the validity and well-formedness of a DN.  The check for the bottom 
   of the DN (common name) and top (country) are made configurable, DNs that 
   act as filters (e.g. path constraints) may not have the lower DN parts 
   present and certificate requests submitted to CAs that set the country 
   themselves may not have the country present.

   We also check that there's only one country present (the use of DN 
   strings allows more than one to be set).  In addition if an explicit 
   check for well-formedness is being done then we check for no more than 
   one CN and no more than one AVA per RDN because the behaviour of 
   different implementations when they encounter these is more or less 
   random */

CHECK_RETVAL STDC_NONNULL_ARG( ( 3, 4 ) ) \
int checkDN( IN const DATAPTR_DN dn,
			 IN_FLAGS( CHECKDN ) const int checkFlags,
			 OUT_ENUM_OPT( CRYPT_ATTRIBUTE ) \
				CRYPT_ATTRIBUTE_TYPE *errorLocus,
			 OUT_ENUM_OPT( CRYPT_ERRTYPE ) \
				CRYPT_ERRTYPE_TYPE *errorType )
	{
	DN_COMPONENT *dnComponentListPtr;
	BOOLEAN seenCountry = FALSE, seenCommonName = FALSE;
	int LOOP_ITERATOR;

	assert( isWritePtr( errorLocus, sizeof( CRYPT_ATTRIBUTE_TYPE ) ) );
	assert( isWritePtr( errorType, sizeof( CRYPT_ERRTYPE_TYPE ) ) );

	REQUIRES( DATAPTR_ISVALID( dn ) );
	REQUIRES( isFlagRange( checkFlags, CHECKDN ) );

	/* Clear return values */
	*errorLocus = CRYPT_ATTRIBUTE_NONE;
	*errorType = CRYPT_ERRTYPE_NONE;

	/* Perform a special-case check for a null DN */
	if( DATAPTR_ISNULL( dn ) )
		return( CRYPT_ERROR_NOTINITED );

	/* Make sure that certain critical components are present */
	LOOP_MED( dnComponentListPtr = DATAPTR_GET( dn ), 
			  dnComponentListPtr != NULL,
			  dnComponentListPtr = DATAPTR_GET( dnComponentListPtr->next ) )
		{
		REQUIRES( sanityCheckDNComponent( dnComponentListPtr ) );

		if( dnComponentListPtr->type == CRYPT_CERTINFO_COUNTRYNAME )
			{
			if( !checkCountryCode( dnComponentListPtr->value ) )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_VALUE;
				*errorLocus = CRYPT_CERTINFO_COUNTRYNAME;
				return( CRYPT_ERROR_INVALID );
				}
			if( seenCountry )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				*errorLocus = CRYPT_CERTINFO_COUNTRYNAME;
				return( CRYPT_ERROR_DUPLICATE );
				}
			seenCountry = TRUE;
			}
		if( dnComponentListPtr->type == CRYPT_CERTINFO_COMMONNAME )
			{
			if( ( checkFlags & CHECKDN_FLAG_WELLFORMED ) && seenCommonName )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				*errorLocus = CRYPT_CERTINFO_COMMONNAME;
				return( CRYPT_ERROR_DUPLICATE );
				}
			seenCommonName = TRUE;
			}

		/* Multi-AVA RDNs are handled more or less arbitrarily by 
		   implementations, we don't allow them if the DN is supposed to
		   be well-formed */
		if( ( checkFlags & CHECKDN_FLAG_WELLFORMED ) && \
			TEST_FLAG( dnComponentListPtr->flags, DN_FLAG_CONTINUED ) )
			{
			if( dnComponentListPtr->type >= CRYPT_CERTINFO_FIRST && \
				dnComponentListPtr->type <= CRYPT_CERTINFO_LAST )
				{
				*errorType = CRYPT_ERRTYPE_ATTR_PRESENT;
				*errorLocus = dnComponentListPtr->type;
				}
			return( CRYPT_ERROR_INVALID );
			}
		}
	ENSURES( LOOP_BOUND_OK );
	if( ( ( checkFlags & CHECKDN_FLAG_COUNTRY ) && !seenCountry ) || \
		( ( checkFlags & CHECKDN_FLAG_COMMONNAME ) && !seenCommonName ) )
		{
		*errorType = CRYPT_ERRTYPE_ATTR_ABSENT;
		*errorLocus = ( seenCountry ) ? CRYPT_CERTINFO_COMMONNAME : \
										CRYPT_CERTINFO_COUNTRYNAME;
		return( CRYPT_ERROR_NOTINITED );
		}

	return( CRYPT_OK );
	}

/* Convert a DN component containing a PKCS #9 emailAddress or an RFC 1274
   rfc822Mailbox into an rfc822Name */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int convertEmail( INOUT CERT_INFO *certInfoPtr, 
				  INOUT DATAPTR_DN *dnPtr,
				  IN_ATTRIBUTE const CRYPT_ATTRIBUTE_TYPE altNameType )
	{
	DATAPTR_DN dn = *dnPtr;
	DATAPTR certData, subjectDNData, issuerDNData, publicKeyData;
	DN_COMPONENT *emailComponent;
	SELECTION_STATE selectionState;
	int certDataSize, publicKeyDataSize, status;

	assert( isWritePtr( certInfoPtr, sizeof( CERT_INFO ) ) );
	assert( isWritePtr( dnPtr, sizeof( DATAPTR_DN ) ) );

	REQUIRES( altNameType == CRYPT_CERTINFO_SUBJECTALTNAME || \
			  altNameType == CRYPT_CERTINFO_ISSUERALTNAME );

	/* If there's no PKCS #9 email address present, try for an RFC 1274 one.
	   If that's not present either, exit */
	if( DATAPTR_ISNULL( dn ) )
		{
		/* If there's an empty subject/issuer DN present, there's nothing to 
		   do */
		return( CRYPT_OK );
		}
	emailComponent = findDNComponentByOID( dn,
			MKOID( "\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01" ), 11 );
	if( emailComponent == NULL )
		{
		emailComponent = findDNComponentByOID( dn,
			MKOID( "\x06\x09\x09\x92\x26\x89\x93\xF2\x2C\x01\x03" ), 11 );
		}
	if( emailComponent == NULL )
		return( CRYPT_OK );

	/* Try and add the email address component as an rfc822Name.  Since this
	   changes the current GeneralName selection we have to be careful about
	   saving and restoring the state.  In addition since we're changing the
	   internal state of an object which is technically in the high state we 
	   have to temporarily disconnect all of the certificate data from the 
	   certificate object to make it appear as a mutable object.  This is an 
	   unfortunate consequence of the fact that what we're doing is a 
	   behind-the-scenes switch to move an initialised certificate component 
	   from where it is to where it really should be */
	saveSelectionState( selectionState, certInfoPtr );
	DATAPTR_SET( certData, certInfoPtr->certificate );
	certDataSize = certInfoPtr->certificateSize;
	DATAPTR_SET( subjectDNData, certInfoPtr->subjectDNptr );
	DATAPTR_SET( issuerDNData, certInfoPtr->issuerDNptr );
	DATAPTR_SET( publicKeyData, certInfoPtr->publicKeyInfo );
	publicKeyDataSize = certInfoPtr->publicKeyInfoSize;
	certInfoPtr->certificate = NULL;
	certInfoPtr->certificateSize = 0;
	certInfoPtr->subjectDNptr = certInfoPtr->issuerDNptr = NULL;
	certInfoPtr->publicKeyInfo = NULL;
	certInfoPtr->publicKeyInfoSize = 0;
	ENSURES( sanityCheckCert( certInfoPtr ) );
	status = addCertComponent( certInfoPtr, CRYPT_ATTRIBUTE_CURRENT, 
							   altNameType );
	assert( cryptStatusOK( status ) );	/* Should never fail, warn in debug mode */
	if( cryptStatusOK( status ) )
		{
		status = addCertComponentString( certInfoPtr, 
										 CRYPT_CERTINFO_RFC822NAME,
										 emailComponent->value,
										 emailComponent->valueLength );
		}
	if( cryptStatusOK( status ) )
		{
		/* It was successfully copied over, delete the copy in the DN */
		( void ) deleteComponent( dnPtr, emailComponent );
		}
	else
		{
		/* If it's already present (which is somewhat odd since the presence
		   of an email address in the DN implies that the implementation
		   doesn't know about rfc822Name), we can't do anything about it */
		if( status == CRYPT_ERROR_INITED )
			status = CRYPT_OK;
		else
			{
			/* Some certificates can contain garbage in the (supposed) email 
			   address, normally the certificate would be rejected because 
			   of this but if we're running in oblivious mode then we can 
			   import it successfully but get an internal error code when we 
			   try and perform this sideways add.  To catch this we check 
			   for invalid email addresses here and ignore an error status 
			   if we get one */
			if( cryptArgError( status ) )
				status = CRYPT_OK;
			}
		}
	certInfoPtr->certificate = DATAPTR_GET( certData );
	certInfoPtr->certificateSize = certDataSize;
	certInfoPtr->subjectDNptr = DATAPTR_GET( subjectDNData );
	certInfoPtr->issuerDNptr = DATAPTR_GET( issuerDNData );
	certInfoPtr->publicKeyInfo = DATAPTR_GET( publicKeyData );
	certInfoPtr->publicKeyInfoSize = publicKeyDataSize;
	ENSURES( sanityCheckCert( certInfoPtr ) );
	restoreSelectionState( selectionState, certInfoPtr );

	return( status );
	}
#endif /* USE_CERTIFICATES */
