/****************************************************************************
*																			*
*							cryptlib RPC Header File						*
*						Copyright Peter Gutmann 1997-2001					*
*																			*
****************************************************************************/

#ifndef _RPC_DEFINED

#define _RPC_DEFINED

/* Each message when encoded looks as follows:

	type			: 8
	flags			: 8
	noArgs			: 8
	noStringArgs	: 8
	length			: 32
	arg * 0..n		: 32 * n
	stringArg * 0..n: 32 + data * n

   The fixed header consists of a 32-bit type+format information value and
   length (to allow the entire message to be read using only two read calls)
   followed by 1 - MAX_ARGS integer args and 0 - MAX_STRING_ARGS variable-
   length data args.  The protocol is completely stateless, the client sends
   COMMAND_xxx requests to the server and the server responds with
   COMMAND_RESULT messages */

/* cryptlib API commands */

typedef enum {
	COMMAND_NONE,				/* No command type */
	COMMAND_RESULT,				/* Result from server */
	COMMAND_SERVERQUERY,		/* Get info on server */
	COMMAND_CREATEOBJECT,		/* Create an object */
	COMMAND_CREATEOBJECT_INDIRECT,	/* Create an object indirectly */
	COMMAND_EXPORTOBJECT,		/* Export object in encoded form */
	COMMAND_DESTROYOBJECT,		/* Destroy an object */
	COMMAND_QUERYCAPABILITY,	/* Query capabilities */
	COMMAND_GENKEY,				/* Generate key */
	COMMAND_ENCRYPT,			/* Encrypt/sign/hash */
	COMMAND_DECRYPT,			/* Decrypt/sig check/hash */
	COMMAND_GETATTRIBUTE,		/* Get/set/delete attribute */
	COMMAND_SETATTRIBUTE,
	COMMAND_DELETEATTRIBUTE,
	COMMAND_GETKEY,				/* Get/set/delete key */
	COMMAND_SETKEY,
	COMMAND_DELETEKEY,
	COMMAND_PUSHDATA,
	COMMAND_POPDATA,
	COMMAND_FLUSHDATA,			/* Push/pop/flush data */
	COMMAND_CERTSIGN,			/* Sign certificate */
	COMMAND_CERTCHECK,			/* Check signature on certificate */
	COMMAND_CERTMGMT,			/* CA cert management operation */
	COMMAND_LAST				/* Last command type */
	} COMMAND_TYPE;

/* Database shim commands */

typedef enum {
	DBX_COMMAND_NONE,			/* No command type */
	DBX_COMMAND_RESULT,			/* Result from server (== COMAND_RESULT) */
	DBX_COMMAND_OPEN,			/* Open session with database */
	DBX_COMMAND_CLOSE,			/* Close session with database */
	DBX_COMMAND_QUERY,			/* Perform data fetch/check */
	DBX_COMMAND_UPDATE,			/* Perform data update */
	DBX_COMMAND_GETERRORINFO,	/* Sent if another command fails */
	DBX_COMMAND_LAST			/* Last command type */
	} DBX_COMMAND_TYPE;

/* The command formats are as follows (arguments in square brackets are
   implied arguments whose values are supplied at the C function level but
   that aren't passed over the wire, this is used to handle reads of string
   values):

	COMMAND_SERVERQUERY
		<none>						word: status
									word: protocol version
									word: max.fragment size
	COMMAND_CREATEOBJECT
		word: handle				word: status
		word: object type			word: new handle
		word(s) | str(s): params
	COMMAND_CREATEOBJECT_INDIRECT
		word: handle				word: status
		word: object type			word: new handle
		str : encoded object data
	COMMAND_EXPORTOBJECT
		word: handle				word: status
		word(s): params				word: str_length | str: data
	COMMAND_DESTROYOBJECT
		word: handle				word: status
	COMMAND_QUERYCAPABILITY
		word: handle				word: status
		word: algo					word: str_length | str : data
		word: mode
		[str: return buffer]
	COMMAND_GENKEY
		word: handle				word: status
		word: is_async (optional)
	COMMAND_ENCRYPT
		word: handle				word: status
		str : data					str : data
	COMMAND_DECRYPT
		word: handle				word: status
		str : data					str : data
	COMMAND_GETATTRIBUTE
		word: handle				word: status
		word: attribute type		word: value | word: str_length | str: data
		word: get_str_data (optional)
		[str: return buffer for str_data]
	COMMAND_SETATTRIBUTE
		word: handle				word: status
		word: attribute type
		word: value | str : value
	COMMAND_DELETEATTRIBUTE
		word: handle				word: status
		word: attribute type
	COMMAND_GETKEY
		word: handle				word: status
		word: itemType				word: handle
		word: key ID type	
		str : key ID (optional)
		str : password (optional)
	COMMAND_SETKEY
		word: handle				word: status
		word: key handle
		word: caItemType (optional)
		str : password (optional)
	COMMAND_DELETEKEY
		word: handle				word: status
		word: key ID type
		word: caItemType (optional)
		str : key ID
	COMMAND_PUSHDATA
		word: handle				word: status
		str : data					word: length
	COMMAND_POPDATA
		word: handle				word: status
		word: length				str : data
		[str: return buffer]
	COMMAND_CERTSIGN
		word: handle				word: status
		word: sig.key handle
	COMMAND_CERTCHECK
		word: handle				word: status
		word: check key handle
	COMMAND_CERTMGMT
		word: handle				word: status
		word: caKey					word: new cert (optional)
		word: certRequest

	DBX_COMMAND_OPEN:
		word: options				word: status
		str : name					word: featureFlags
	DBX_COMMAND_CLOSE
		<none>
	DBX_COMMAND_UPDATE:
		word: type					word: status
		str : command
		str : date (optional)
		str : data (optional)
	DBX_COMMAND_QUERY:
		word: type					word: status
		word: query entry (opt.)	str : data
		str : command			
		str : date (optional)
		str : data (optional)
		[str: return buffer]
	DBX_COMMAND_GETERRORINFO
		<none>						word: errorCode
									str : errorMessage */

/* The maximum number of integer and string args, and the amount of space to
   allocate in the COMMAND_INFO to store all possible args */

#define MAX_ARGS				4
#define MAX_STRING_ARGS			2
#define DBX_MAX_ARGS			2
#define DBX_MAX_STRING_ARGS		3
#define ALLOC_MAX_ARGS			MAX_ARGS
#define ALLOC_MAX_STRING_ARGS	DBX_MAX_STRING_ARGS

/* The possible command flags */

#define COMMAND_FLAG_NONE		0x00	/* No command flag */
#define COMMAND_FLAG_RET_NONE	0x01	/* Don't return any data */
#define COMMAND_FLAG_RET_LENGTH	0x02	/* Return only length of string arg */

/* The size of an integer as encoded in a message, the size of the fixed-
   length fields, and the offsets of the data fields in the message */

#define COMMAND_WORDSIZE		4
#define COMMAND_FIXED_DATA_SIZE	( COMMAND_WORDSIZE * 2 )
#define COMMAND_WORD1_OFFSET	COMMAND_FIXED_DATA_SIZE
#define COMMAND_WORD2_OFFSET	( COMMAND_FIXED_DATA_SIZE + COMMAND_WORDSIZE )
#define COMMAND_WORD3_OFFSET	( COMMAND_FIXED_DATA_SIZE + ( COMMAND_WORDSIZE * 2 ) )
#define COMMAND_WORD4_OFFSET	( COMMAND_FIXED_DATA_SIZE + ( COMMAND_WORDSIZE * 3 ) )

/* Macros to encode/decode a message type value */

#define putMessageType( buffer, type, flags, noInt, noString ) \
		{ \
		buffer[ 0 ] = ( BYTE ) ( type & 0xFF ); \
		buffer[ 1 ] = ( BYTE ) ( flags & 0xFF ); \
		buffer[ 2 ] = noInt; \
		buffer[ 3 ] = noString; \
		}
#define getMessageType( buffer, type, flags, noInt, noString ) \
		type = buffer[ 0 ]; flags = buffer[ 1 ]; \
		noInt = buffer[ 2 ]; noString = buffer[ 3 ]

/* Macros to encode/decode an integer value and a length */

#define putMessageWord( buffer, word ) \
		{ \
		( buffer )[ 0 ] = ( BYTE ) ( ( ( word ) >> 24 ) & 0xFF ); \
		( buffer )[ 1 ] = ( BYTE ) ( ( ( word ) >> 16 ) & 0xFF ); \
		( buffer )[ 2 ] = ( BYTE ) ( ( ( word ) >> 8 ) & 0xFF ); \
		( buffer )[ 3 ] = ( BYTE ) ( ( word ) & 0xFF ); \
		}
#define getMessageWord( buffer ) \
		( ( ( ( long ) ( buffer )[ 0 ] ) << 24 ) | \
		  ( ( ( long ) ( buffer )[ 1 ] ) << 16 ) | \
		  ( ( ( long ) ( buffer )[ 2 ] ) << 8 ) | \
			  ( long ) ( buffer )[ 3 ] )

#define getMessageLength	getMessageWord
#define putMessageLength	putMessageWord

/* A structure to contain the command elements */

typedef struct {
	COMMAND_TYPE type;					/* Command type */
	int flags;							/* Command flags */
	int noArgs, noStrArgs;				/* Number of int, string args */
	int arg[ ALLOC_MAX_ARGS ];			/* Integer arguments */
	void *strArg[ ALLOC_MAX_STRING_ARGS ];	/* String args */
	int strArgLen[ ALLOC_MAX_STRING_ARGS ];
	} COMMAND_INFO;

/* Function pointers for a generic dispatch function that dispatches the
   marshalled data to a receiver, and command handlers that process each
   command type */

typedef void ( *DISPATCH_FUNCTION )( void *stateInfo, BYTE *buffer );
typedef int ( *COMMAND_HANDLER )( void *stateInfo, COMMAND_INFO *cmd );

/* The full RPC interface (with marshalling and everything) provides complete
   isolation of input and output, however it resuls in a slight performance
   decrease due to copying, and can't handle large objects atomically due to
   limits on message size (this specifically applies to mega-CRLs).  Because 
   of this, we also allow a direct interface that just forwards the data 
   without marshalling/unmarshalling.
   
   Because of the change in arg handling for returned data in RPC vs. direct 
   calls (the RPC returns the data in the return message, the direct call 
   requires an extra parameter to specify the location of the returned data) 
   we also need a macro RETURN_VALUE() to no-op out the extra parameter in 
   case we're using the RPC form */

#ifdef USE_RPCAPI
  #define DISPATCH_COMMAND( function, command ) \
		  dispatchCommand( &command )
  #define DISPATCH_COMMAND_DBX( function, command, dbmsInfo ) \
		  dispatchCommand( &command, ( dbmsInfo )->stateInfo, ( dbmsInfo )->dispatchFunction )
  #define RETURN_VALUE( value )		0
#else
  #define DISPATCH_COMMAND( function, command ) \
		  function( &command )
  #define DISPATCH_COMMAND_DBX( function, command, dbmsInfo ) \
		  function( ( dbmsInfo )->stateInfo, &command )
  #define RETURN_VALUE( value )		value
#endif /* USE_RPCAPI */

/* Check whether a decoded command header contains valid data for the
   different RPC types */

#define checkCommandInfo( cmd, length ) \
		( ( cmd )->type > COMMAND_NONE && \
		  ( cmd )->type < COMMAND_LAST && \
		  ( ( cmd )->flags == COMMAND_FLAG_NONE || \
		    ( cmd )->flags == COMMAND_FLAG_RET_NONE || \
			( cmd )->flags == COMMAND_FLAG_RET_LENGTH ) && \
		  ( cmd )->noArgs >= 1 && ( cmd )->noArgs <= MAX_ARGS && \
		  ( cmd )->noStrArgs >= 0 && ( cmd )->noStrArgs <= MAX_STRING_ARGS && \
		  ( cmd )->strArgLen[ 0 ] >= 0 && \
		  ( cmd )->strArgLen[ 1 ] >= 0 && \
		  ( length ) >= 0 && ( length ) <= RPC_IO_BUFSIZE )

#define checkCommandConsistency( cmd, length ) \
		( ( ( cmd )->strArgLen[ 0 ] + ( cmd )->strArgLen[ 1 ] ) == \
			( length - ( COMMAND_WORDSIZE * ( ( cmd )->noArgs + ( cmd )->noStrArgs ) ) ) )

#define dbxCheckCommandInfo( cmd, length ) \
		( ( cmd )->type > DBX_COMMAND_NONE && \
		  ( cmd )->type < DBX_COMMAND_LAST && \
		  ( cmd )->flags == COMMAND_FLAG_NONE && \
		  ( cmd )->noArgs >= 0 && ( cmd )->noArgs <= DBX_MAX_ARGS && \
		  ( cmd )->noStrArgs >= 0 && ( cmd )->noStrArgs <= DBX_MAX_STRING_ARGS && \
		  ( cmd )->strArgLen[ 0 ] >= 0 && \
		  ( cmd )->strArgLen[ 1 ] >= 0 && \
		  ( cmd )->strArgLen[ 2 ] >= 0 && \
		  ( length ) >= 0 && ( length ) <= DBX_IO_BUFSIZE )

#define dbxCheckCommandConsistency( cmd, length ) \
		( ( ( cmd )->strArgLen[ 0 ] + ( cmd )->strArgLen[ 1 ] + ( cmd )->strArgLen[ 2 ] ) == \
			( length - ( COMMAND_WORDSIZE * ( ( cmd )->noArgs + ( cmd )->noStrArgs ) ) ) )

/* The maximum size of a message fragment.  Messages containing more data
   than this are broken up into fragments.  On systems with very restricted
   amounts of memory we make the size rather small to limit the size of
   the intermediate buffers used */

#ifdef CONFIG_CONSERVE_MEMORY
  #define MAX_FRAGMENT_SIZE		8192
#else
  #define MAX_FRAGMENT_SIZE		32768
#endif /* CONSERVER_MEMORY */

/* The size of the I/O buffer used to assemble messages.  This is equal to
   the maximum fragment size plus the maximum header size for commands that
   require fragmentation (COMMAND_ENCRYPT/COMMAND_DECRYPT and
   COMMAND_PUSHDATA/COMMAND_POPDATA).  We define a separate version for 
   database RPC since this uses much smaller buffers */

#define RPC_IO_BUFSIZE			MAX_FRAGMENT_SIZE + 32
#define DBX_IO_BUFSIZE			4096

#endif /* _RPC_DEFINED */
