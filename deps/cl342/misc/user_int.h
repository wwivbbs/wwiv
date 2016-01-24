/****************************************************************************
*																			*
*						  Interal User Header File							*
*						 Copyright Peter Gutmann 1999-2008					*
*																			*
****************************************************************************/

#ifndef _USER_INT_DEFINED

#define _USER_INT_DEFINED

/* Configuration option types */

typedef enum {
	OPTION_NONE,					/* Non-option */
	OPTION_STRING,					/* Literal string */
	OPTION_NUMERIC,					/* Numeric value */
	OPTION_BOOLEAN					/* Boolean flag */
	} OPTION_TYPE;

/* The configuration options.  Alongside the CRYPT_ATTRIBUTE_TYPE we store a 
   persistent index value for the option that always stays the same even if 
   the attribute type changes.  This avoids the need to change the config 
   file every time that an attribute is added or deleted.  Some options 
   can't be made persistent, for these the index value is set to 
   CRYPT_UNUSED */

typedef struct {
	const CRYPT_ATTRIBUTE_TYPE option;/* Attribute ID */
	const OPTION_TYPE type;			/* Option type */
	const int index;				/* Index value for this option */
	BUFFER_OPT_FIXED( intDefault ) \
	const char FAR_BSS *strDefault;	/* Default if it's a string option */
	const int intDefault;			/* Default if it's a numeric/boolean
									   or length if it's a string */
	} BUILTIN_OPTION_INFO;

typedef struct {
	BUFFER_OPT_FIXED( intValue ) \
	char *strValue;					/* Value if it's a string option */
	int intValue;					/* Value if it's a numeric/boolean
									   or length if it's a string */
	const BUILTIN_OPTION_INFO *builtinOptionInfo;
									/* Pointer to corresponding built-in 
									   option info */
	BOOLEAN dirty;					/* Whether option has been changed */
	} OPTION_INFO;

/* The attribute ID of the last option that's written to disk, and an upper 
   bound on the corresponding persistent index value used for range checking.  
   Further options beyond this one are ephemeral and are never written to 
   disk */

#define LAST_STORED_OPTION			CRYPT_OPTION_MISC_SIDECHANNELPROTECTION
#define LAST_OPTION_INDEX			1000

/* Prototypes for functions in user_cfg.c */

CHECK_RETVAL_PTR \
const BUILTIN_OPTION_INFO *getBuiltinOptionInfoByCode( IN_RANGE( 0, LAST_OPTION_INDEX ) \
														const int optionCode );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN checkConfigChanged( IN_ARRAY( configOptionsCount ) \
								const OPTION_INFO *optionList,
							IN_INT_SHORT const int configOptionsCount );

#endif /* _USER_INT_DEFINED */
