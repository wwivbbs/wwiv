/****************************************************************************
*																			*
*							cryptlib WebSockets Header						*
*						Copyright Peter Gutmann 2015-2017					*
*																			*
****************************************************************************/

#ifndef _WEBSOCKETS_DEFINED

#define _WEBSOCKETS_DEFINED

/****************************************************************************
*																			*
*								WebSockets Packet Types						*
*																			*
****************************************************************************/

/* WebSockets packet types */

#define WS_PACKET_TEXT			0x81
#define WS_PACKET_BINARY		0x82
#define WS_PACKET_TEXT_PARTIAL	0x01
#define WS_PACKET_BINARY_PARTIAL 0x02
#define WS_PACKET_CLOSE			0x88
#define WS_PACKET_PING			0x89
#define	WS_PACKET_PONG			0x8A

/* A WebSockets Pong packet, sent in response to a received Ping packet */

#define WS_PONG_DATA_CLIENT		"\x8A\x80"
#define WS_PONG_DATA_SERVER		"\x8A\x00"
#define WS_PONG_SIZE			2

/* A WebSockets Close packet, consisting of the standard Close packet and a
   status code of WS_CLOSE_NORMAL sent as payload data.  Note that this 
   then needs to be masked if it's sent by the client */

#define WS_CLOSE_DATA_CLIENT	"\x88\x82\x03\xE8"
#define WS_CLOSE_DATA_SERVER	"\x88\x02\x03\xE8"
#define WS_CLOSE_SIZE			2
#define WS_CLOSE_DATA_SIZE		2

/* WebSockets Close packet status codes.  God knows how some of these are 
   supposed to be used, for example the various Clayton's codes can't be set
   but have defined meanings, and the policy code is a kind of catch-all 
   that's meant to be used if other codes like overflow error aren't 
   suitable, so it has nothing to do with actual policies but is more a 
   catch-all for when there's nothing else available */

#define WS_CLOSE_NORMAL			1000	/* Normal close */
#define WS_CLOSE_SHUTDOWN		1001	/* Client/server is closing down */
#define WS_CLOSE_PROTOCOL_ERROR	1002	/* Protocol error */
#define WS_CLOSE_UNACCEPTABLE_DATA 1003	/* Unacceptable data, e.g.txt vs.bin */
#define WS_CLOSE_RESERVED		1004	/* RFU */
#define WS_CLOSE_CLAYTONS_NORMAL 1005	/* Clayton's version of 1000 */
#define WS_CLOSE_CLAYTONS_ERROR	1006	/* Claytons version of 1002 etc */
#define WS_CLOSE_INVALID_DATA	1007	/* Invalid data, e.g. bin in txt packet */
#define WS_CLOSE_POLICY			1008	/* Catch-all, see above */
#define WS_CLOSE_OVERFLOW		1009	/* Message too large */
#define WS_CLOSE_MISSING_EXTENSION 1010	/* Client expected extension from server */
#define WS_CLOSE_UNEXPECTED		1011	/* Server encountered unexpected condition */
#define WS_CLOSE_CLAYTONS_TLS_ERROR	1015 /* Clayton's TLS error */

/****************************************************************************
*																			*
*								WebSockets Constants						*
*																			*
****************************************************************************/

/* The payload length at which we need to move to an explicit length field 
   rather than having the length encoded as as length-code in the header, 
   and the magic values to indicate the size of the length field: 0x7E = 
   16-bit length follows, 0x7F = 64-bit length follows */

#define WS_LENGTH_THRESHOLD		0x7D	/* 0 - 125 = encoded in header */

#define WS_LENGTH_16BIT			0x7E	/* 16-bit length follows */
#define WS_LENGTH_64BIT			0x7F	/* 64-bit length follows */

#define WS_LENGTH_MASK			0x7F	/* Mask for length in length-code field */

/* The flag indicating that data is masked, encoded as part of the length-
   code field, and the size of the masking value that the client has to use 
   when sending data to the server */

#define WS_MASK_FLAG			0x80	/* Flag for masking in length-code field */
#define WS_MASK_SIZE			4		/* Size of masking value */

/* WebSockets requires that client-to-server messages be masked, this 
   mechanism was added to deal with cache-poisoning attacks on proxies
   via maliciously-structured payloads, a specific thing that seems to 
   have grabbed the RFC authors' attention at the time it was written.
   In theory the masking value is supposed to be random and different for 
   each packet, however this only applies for plaintext messaging, when
   running inside a TLS tunnel there's no need for masking (it could be
   argued that there's no need for masking outside a TLS tunnel either, or
   at least that the one very specific attack that's mitigated, if it's 
   even a concern, could be replaced with any number of other attacks that 
   aren't addressed).  To simplify the code we don't bother with the 
   overhead of using cryptographically strong sources of entropy but just
   use a fixed value that satisfies the requirement for masking */

#define WS_MASK_VALUE			"\x6E\x2A\xF7\xA4"

/* The position of various fields within the WebSockets header: the packet
   type, the length code (containing either a 7-bit length or a length 
   extension indicator), the optional length field, and the optional mask 
   for client->server data masking assuming a default 16-bit explicit length
   is present */

#define WS_TYPE_OFFSET			0
#define WS_LENGTHCODE_OFFSET	1
#define WS_LENGTH_OFFSET		( 1 + 1 )
#define WS_MASK_OFFSET			( 1 + 1 + UINT16_SIZE )

/* WebSockets header lengths, the basic (minimal) size header with a single-
   byte length and the typical size header with a UINT16 length.  Since the 
   client has to mask the payload, the client length differs from the server 
   length */

#define WS_BASE_HEADER_LENGTH_CLIENT ( 1 + 1 + WS_MASK_SIZE )
#define WS_BASE_HEADER_LENGTH_SERVER ( 1 + 1 )

#define WS_HEADER_LENGTH_CLIENT	( 1 + 1 + UINT16_SIZE + WS_MASK_SIZE )
#define WS_HEADER_LENGTH_SERVER	( 1 + 1 + UINT16_SIZE )

/****************************************************************************
*																			*
*							WebSockets Handshake Constants					*
*																			*
****************************************************************************/

/* The sizes of various data values used to perform the WebSockets 
   HTTP upgrade authentication/authorisation process */

#define WS_KEY_SIZE				16		/* Key used to generate auth.response */
#define WS_ENCODED_KEY_SIZE		22		/* base64-encoded key size */

#define WS_KEY_GUID				"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_KEY_GUID_SIZE		36

#define WS_ENCODED_AUTH_SIZE	28		/* base64-encoded auth.response size */

#endif /* _WEBSOCKETS_DEFINED */
