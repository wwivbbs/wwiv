/****************************************************************************
*																			*
*					  cryptlib Source Analysis Header File 					*
*						Copyright Peter Gutmann 1997-2011					*
*																			*
****************************************************************************/

#ifndef _ANALYSE_DEFINED

#define _ANALYSE_DEFINED

/* A symbolic define for the maximum possible error value when we're 
   checking ranges for status codes */

#define MAX_ERROR		CRYPT_ENVELOPE_RESOURCE

/****************************************************************************
*																			*
*							PREfast Analysis Support						*
*																			*
****************************************************************************/

#if defined( _MSC_VER ) && defined( _PREFAST_ ) 

/* Enable strict checking of PREfast annotations.  This gets a bit ugly 
   because if a standard header that's included before us already 
   includes sal.h then we'll have __SPECSTRINGS_STRICT_LEVEL already
   defined (the default is level 1, minimal warnings), so we undefine it
   if necessary before setting it to the maximum warning level */

#ifdef __SPECSTRINGS_STRICT_LEVEL
  #undef __SPECSTRINGS_STRICT_LEVEL
#endif /* __SPECSTRINGS_STRICT_LEVEL */
#define __SPECSTRINGS_STRICT_LEVEL	3

/* Include the PREfast code analysis header.  Microsoft changed the notation
   used from declspec SAL to attribute SAL some time between VC++ 2005 and
   VC++ 2008, with all subsequent development effort going into attribute 
   SAL.  The following annotations work with both declspec SAL and attribute
   SAL, depending on which variant is in use.

   This is complicated somewhat by the fixer-upper nature of the SAL 
   material, many of the annotations aren't documented in MSDN, some are 
   only available in MS-internal versions of PREfast, and some are defined
   to no-ops.  In particular several of the more interesting annotations from
   declspec SAL like __field_bcount, used to describe the extent of a fixed-
   size buffer in a struct or on the stack (which were no-oped out in all 
   known MS-external versions) are no longer defined as no-ops in attribute 
   SAL, so we have to explicitly turn them into no-ops.  Conversely, the 
   formerly no-oped __in_range in declspec SAL may now exist as _In_range_ 
   in attribute SAL and appears to be used, or at least it's defined as 
   atributes rather than just being no-oped out */

#include <specstrings.h>

/* Function return value information.  RETVAL_xxx annotations simply indicate
   what the function returns, CHECK_RETVAL_xxx indicates that the caller must
   check the return value:

	RETVAL			Function returns a standard cryptlib status code.
	RETVAL_BOOL		As CHECK_RETVAL but result is a boolean value.  This is 
					used for 'pure' boolean functions that simply return a 
					yes-or-no response about their input, for which there's 
					no specific failure or success code.
	RETVAL_ENUM		As CHECK_RETVAL but result is an enum following the 
					rules for IN_ENUM further down.
	RETVAL_LENGTH	As CHECK_RETVAL but result is a LENGTH following the 
					rules for IN_LENGTH further down.
	RETVAL_PTR		As CHECK_RETVAL but result is a pointer, non-null on
					success.
	RETVAL_RANGE	As CHECK_RETVAL but result must be in the range 
					{ low...high } inclusive.
	RETVAL_SPECIAL	As RETVAL but OK_SPECIAL is treated as CRYPT_OK.
	RETVAL_STRINGOP	Function returns either an error code or a string 
					position/index, used for the strXYZ() functions.

   This is additionally complicated by the fact that attribute SAL doesn't 
   allow RETVAL in typedefs and function pointers (since they're not valid
   as attributes there) so we have to use a special-case typedef/function-
   pointer version that's no-oped out in attribute SAL but left in in 
   declspec SAL */

#ifdef __ATTR_SAL				/* Attribute SAL */

#define CHECK_RETVAL			_Check_return_ \
								_Success_( result == CRYPT_OK )
#define CHECK_RETVAL_BOOL		_Check_return_ \
								_Success_( result == TRUE )
#define CHECK_RETVAL_ENUM( name ) \
								_Check_return_ \
								_Success_( result >= name##_NONE && result < name##_LAST )
#define CHECK_RETVAL_LENGTH		_Check_return_ \
								_Success_( result >= 0 && result <= MAX_INTLENGTH - 1 )
#define CHECK_RETVAL_PTR		_Check_return_ \
								_Success_( result != NULL )
#define CHECK_RETVAL_RANGE( low, high ) \
								_Check_return_ \
								_Success_( result >= ( low ) && result <= ( high ) )
#define CHECK_RETVAL_SPECIAL	_Check_return_ \
								_Success_( result == CRYPT_OK || result == OK_SPECIAL )
#define CHECK_RETVAL_STRINGOP( length ) \
								_Check_return_ \
								_Success_( result >= 0 )

#define CHECK_RETVAL_FNPTR
#define CHECK_RETVAL_BOOL_FNPTR
#define CHECK_RETVAL_LENGTH_FNPTR
#define CHECK_RETVAL_PTR_FNPTR
#define CHECK_RETVAL_SPECIAL_FNPTR

#define RETVAL					_Success_( result == CRYPT_OK )
#define RETVAL_BOOL				_Success_( result == TRUE )
#define RETVAL_RANGE( low, high ) _Success_( result >= ( low ) && result <= ( high ) )
#define RETVAL_SPECIAL			_Success_( result == CRYPT_OK || result == OK_SPECIAL )

#define RETVAL_FNPTR

#else							/* Declspec SAL */

#define CHECK_RETVAL			__checkReturn \
								__success( result == CRYPT_OK ) \
								__range( MAX_ERROR, CRYPT_OK )
#define CHECK_RETVAL_BOOL		__checkReturn \
								__success( result == TRUE ) \
								__range( FALSE, TRUE )
#define CHECK_RETVAL_ENUM( name ) \
								__checkReturn \
								__success( result >= name##_NONE && result < name##_LAST ) \
								__range( name##_NONE, name##_LAST - 1 )
#define CHECK_RETVAL_LENGTH		__checkReturn \
								__success( result >= 0 && result <= MAX_INTLENGTH - 1 ) \
								__range( MAX_ERROR, MAX_INTLENGTH - 1 )
#define CHECK_RETVAL_PTR		__checkReturn \
								__success( result != NULL )
#define CHECK_RETVAL_RANGE( low, high ) \
								__checkReturn \
								__success( result >= ( low ) && result <= ( high ) ) \
								__range( MAX_ERROR, ( high ) )
#define CHECK_RETVAL_SPECIAL	__checkReturn \
								__success( result == CRYPT_OK || result == OK_SPECIAL ) \
								__range( MAX_ERROR, CRYPT_OK )
#define CHECK_RETVAL_STRINGOP( length ) \
								__checkReturn \
								__success( result >= 0 ) \
								__range( MAX_ERROR, length )

#define CHECK_RETVAL_FNPTR		__checkReturn \
								__success( result == CRYPT_OK ) \
								__range( MAX_ERROR, CRYPT_OK )
#define CHECK_RETVAL_BOOL_FNPTR	__checkReturn \
								__success( result == TRUE ) \
								__range( FALSE, TRUE )
#define CHECK_RETVAL_LENGTH_FNPTR __checkReturn \
								__success( result >= 0 && result <= MAX_INTLENGTH - 1 ) \
								__range( MAX_ERROR, MAX_INTLENGTH - 1 )
#define CHECK_RETVAL_PTR_FNPTR	__checkReturn \
								__success( result != NULL )
#define CHECK_RETVAL_SPECIAL_FNPTR __checkReturn \
								__success( result == CRYPT_OK || result == OK_SPECIAL ) \
								__range( MAX_ERROR, CRYPT_OK )

#define RETVAL					__success( result == CRYPT_OK ) \
								__range( MAX_ERROR, CRYPT_OK )
#define RETVAL_BOOL				__success( result == TRUE ) \
								__range( FALSE, TRUE )
#define RETVAL_RANGE( low, high ) __success( result >= ( low ) && result <= ( high ) ) \
								__range( MAX_ERROR, ( high ) )
#define RETVAL_SPECIAL			__success( result == CRYPT_OK || result == OK_SPECIAL ) \
								__range( MAX_ERROR, CRYPT_OK )

#define RETVAL_FNPTR			__success( result == CRYPT_OK ) \
								__range( MAX_ERROR, CRYPT_OK )

#endif /* Declspec vs. Attribute SAL */

/* When we return from an error handler, returning an error status means 
   that the function succeeded, since it's forwarding a status that it was
   given rather than reporting its own status.  In this case returning an
   error code indicates success */

#ifdef __ATTR_SAL				/* Attribute SAL */

#define CHECK_RETVAL_ERROR		_Check_return_ \
								_Success_( result < CRYPT_OK - 1 )

#else							/* Declspec SAL */

#define CHECK_RETVAL_ERROR		__checkReturn \
								__success( result < CRYPT_OK - 1 ) \
								__range( MAX_ERROR, CRYPT_OK - 1 )

#endif /* Declspec vs. Attribute SAL */

/* Numeric parameter checking:

	INT			Value must be between 1 and MAX_INTLENGTH - 1
	INT_SHORT	As INT but upper bound is MAX_INTLENGTH_SHORT - 1 rather 
				than MAX_INTLENGTH - 1.

   In addition to these we allow the OPT specifier to indicate that the 
   value may be NULL for OUT parameters, the OPT suffix to indicate that the 
   value may be CRYPT_UNUSED, and the Z suffix to indicate that the lower 
   bound may be zero.  This is used for two cases, either when the lower
   bound is explicitly zero or when it's set on error to xyz_NONE, which 
   would normally be an invalid value for the non-_Z parameter */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_INT					_In_ _In_range_( 1, MAX_INTLENGTH - 1 ) 
#define IN_INT_OPT				_In_ _In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 ) 
#define IN_INT_Z				_In_ _In_range_( 0, MAX_INTLENGTH - 1 ) 
#define IN_INT_SHORT			_In_ _In_range_( 1, MAX_INTLENGTH_SHORT - 1 ) 
#define IN_INT_SHORT_Z			_In_ _In_range_( 0, MAX_INTLENGTH_SHORT - 1 ) 

#define OUT_INT					_Out_ _Out_range_( 1, MAX_INTLENGTH - 1 ) 
#define OUT_INT_Z				_Out_ _Out_range_( 0, MAX_INTLENGTH - 1 ) 
#define OUT_INT_SHORT_Z			_Out_ _Out_range_( 0, MAX_INTLENGTH_SHORT - 1 ) 
#define OUT_OPT_INT_Z			_Out_opt_ _Out_range_( 0, MAX_INTLENGTH - 1 ) 

#else							/* Declspec SAL */

#define IN_INT					__in __in_range( 1, MAX_INTLENGTH - 1 ) 
#define IN_INT_OPT				__in __in_range( CRYPT_UNUSED, MAX_INTLENGTH - 1 ) 
#define IN_INT_Z				__in __in_range( 0, MAX_INTLENGTH - 1 ) 
#define IN_INT_SHORT			__in __in_range( 1, MAX_INTLENGTH_SHORT - 1 ) 
#define IN_INT_SHORT_Z			__in __in_range( 0, MAX_INTLENGTH_SHORT - 1 ) 

#define OUT_INT					__out __out_range( 1, MAX_INTLENGTH - 1 ) 
#define OUT_INT_Z				__out __out_range( 0, MAX_INTLENGTH - 1 ) 
#define OUT_INT_SHORT_Z			__out __out_range( 0, MAX_INTLENGTH_SHORT - 1 ) 
#define OUT_OPT_INT_Z			__out_opt __out_range( 0, MAX_INTLENGTH - 1 ) 

#endif /* Declspec vs. Attribute SAL */

/* Special-case parameter checking:

	ALGO		Value must be a cryptlib encryption algorithm.
	ATTRIBUTE	Value must be a cryptlib attribute.
	BOOL		Value must be boolean.
	BYTE		Value must be a single-byte value.
	CHAR		Value must be a 7-bit ASCII character.
	ERROR		Value must be a cryptlib error status code.
	HANDLE		Value must be a cryptlib handle.
	MESSAGE		Value must be a cryptlib message type.
	MODE		Value must be a cryptlib encryption mode.
	PORT		Value must be a network port.
	RANGE		User-specified range check.
	STATUS		Value must be cryptlib status code

   In addition to these we allow the OPT specifier to indicate that the 
   value may be NULL for OUT parameters.
   
   We also allow an OPT suffix to indicate the use of don't-care values, 
   typically CRYPT_UNUSED */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_ALGO					_In_ _In_range_( CRYPT_ALGO_NONE + 1, CRYPT_ALGO_LAST - 1 )
#define IN_ALGO_OPT				_In_ _In_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define IN_ATTRIBUTE			_In_ _In_range_( CRYPT_ATTRIBUTE_NONE + 1, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_ATTRIBUTE_OPT		_In_ _In_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_BYTE					_In_ _In_range_( 0, 0xFF )
#define IN_CHAR					_In_ _In_range_( 0, 0x7F )
#define IN_ERROR				_In_ _In_range_( MAX_ERROR, -1 )
#define IN_HANDLE				_In_ _In_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define IN_HANDLE_OPT			_In_ _In_range_( CRYPT_UNUSED, MAX_OBJECTS - 1 )
#define IN_KEYID				_In_ _In_range_( CRYPT_KEYID_NONE + 1, CRYPT_KEYID_LAST - 1 )
#define IN_KEYID_OPT			_In_ _In_range_( CRYPT_KEYID_NONE, CRYPT_KEYID_LAST - 1 )
#define IN_MESSAGE				_In_ _In_range_( MESSAGE_NONE + 1, IMESSAGE_LAST - 1 )
#define IN_MODE					_In_ _In_range_( CRYPT_MODE_NONE + 1, CRYPT_MODE_LAST - 1 )
#define IN_MODE_OPT				_In_ _In_range_( CRYPT_MODE_NONE, CRYPT_MODE_LAST - 1 )
#define IN_PORT					_In_ _In_range_( 22, 65535L )
#define IN_PORT_OPT				_In_ _In_range_( CRYPT_UNUSED, 65535L )
#define IN_RANGE( min, max )	_In_ _In_range_( ( min ), ( max ) )
#define IN_RANGE_FIXED( value )	_In_ _In_range_( ( value ), ( value ) )
#define IN_STATUS				_In_ _In_range_( MAX_ERROR, CRYPT_OK )

#define INOUT_HANDLE			_Inout_ \
								_In_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 ) \
								_Out_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define INOUT_RANGE( min, max )	_Inout_ \
								_In_range_( ( min ), ( max ) ) \
								_Out_range_( ( min ), ( max ) )

#define OUT_ALGO_Z				_Out_ _Out_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_OPT_ALGO_Z			_Out_opt_ _Out_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_ATTRIBUTE_Z			_Out_ _Out_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_OPT_ATTRIBUTE_Z		_Out_opt_ _Out_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_BOOL				_Out_ _Out_range_( FALSE, TRUE )
#define OUT_OPT_BOOL			_Out_opt_ _Out_range_( FALSE, TRUE )
#define OUT_OPT_BYTE			_Out_opt_ _Out_range_( 0, 0xFF )
#define OUT_ERROR				_Out_ _Out_range_( MAX_ERROR, -1 )
#define OUT_HANDLE_OPT			_Out_ _Out_range_( CRYPT_ERROR, MAX_OBJECTS - 1 )
#define OUT_OPT_HANDLE_OPT		_Out_opt_ _Out_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define OUT_PORT_Z				_Out_ _Out_range_( 0, 65535L )
#define OUT_RANGE( min, max )	_Out_ _Out_range_( ( min ), ( max ) )
#define OUT_OPT_RANGE( min, max ) _Out_opt_ _Out_range_( ( min ), ( max ) )
#define OUT_STATUS				_Out_ _Out_range_( MAX_ERROR, CRYPT_OK )

#else							/* Declspec SAL */

#define IN_ALGO					__in __in_range( CRYPT_ALGO_NONE + 1, CRYPT_ALGO_LAST - 1 )
#define IN_ALGO_OPT				__in __in_range( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define IN_ATTRIBUTE			__in __in_range( CRYPT_ATTRIBUTE_NONE + 1, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_ATTRIBUTE_OPT		__in __in_range( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_BYTE					__in __in_range( 0, 0xFF )
#define IN_CHAR					__in __in_range( 0, 0x7F )
#define IN_ERROR				__in __in_range( MAX_ERROR, -1 )
#define IN_HANDLE				__in __in_range( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define IN_HANDLE_OPT			__in __in_range( CRYPT_UNUSED, MAX_OBJECTS - 1 )
#define IN_KEYID				__in __in_range( CRYPT_KEYID_NONE + 1, CRYPT_KEYID_LAST - 1 )
#define IN_KEYID_OPT			__in __in_range( CRYPT_KEYID_NONE, CRYPT_KEYID_LAST - 1 )
#define IN_MESSAGE				__in __in_range( MESSAGE_NONE + 1, IMESSAGE_LAST - 1 )
#define IN_MODE					__in __in_range( CRYPT_MODE_NONE + 1, CRYPT_MODE_LAST - 1 )
#define IN_MODE_OPT				__in __in_range( CRYPT_MODE_NONE, CRYPT_MODE_LAST - 1 )
#define IN_PORT					__in __in_range( 22, 65535L )
#define IN_PORT_OPT				__in __in_range( CRYPT_UNUSED, 65535L )
#define IN_RANGE( min, max )	__in __in_range( ( min ), ( max ) )
#define IN_RANGE_FIXED( value )	__in __in_range( ( value ), ( value ) )
#define IN_STATUS				__in __in_range( MAX_ERROR, CRYPT_OK )

#define INOUT_HANDLE			__inout \
								__in_range( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 ) \
								__out_range( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define INOUT_RANGE( min, max )	__inout \
								__in_range( ( min ), ( max ) ) \
								__out_range( ( min ), ( max ) )

#define OUT_ALGO_Z				__out __out_range( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_OPT_ALGO_Z			__out_opt __out_range( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_ATTRIBUTE_Z			__out __out_range( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_OPT_ATTRIBUTE_Z		__out_opt __out_range( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_BOOL				__out __out_range( FALSE, TRUE )
#define OUT_OPT_BOOL			__out_opt __out_range( FALSE, TRUE )
#define OUT_OPT_BYTE			__out_opt __out_range( 0, 0xFF )
#define OUT_ERROR				__out __out_range( MAX_ERROR, -1 )
#define OUT_HANDLE_OPT			__out __out_range( CRYPT_ERROR, MAX_OBJECTS - 1 )
#define OUT_OPT_HANDLE_OPT		__out_opt __out_range( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define OUT_PORT_Z				__out __out_range( 0, 65535L )
#define OUT_RANGE( min, max )	__out __out_range( ( min ), ( max ) )
#define OUT_OPT_RANGE( min, max ) __out_opt __out_range( ( min ), ( max ) )
#define OUT_STATUS				__out __out_range( MAX_ERROR, CRYPT_OK )

#endif /* Declspec vs. Attribute SAL */

/* Length parameter checking:

	LENGTH			Value must be between 1 and MAX_INTLENGTH - 1.
	LENGTH_FIXED	Value must be a single fixed length, used for polymorphic
					functions of which this specific instance takes a fixed-
					size parameter, or otherwise in cases where a length 
					parameter is supplied mostly just to allow bounds 
					checking.
	LENGTH_MIN		As LENGTH but lower bound is user-specified.
	LENGTH_Z		As LENGTH but lower bound may be zero.

	LENGTH_SHORT	As LENGTH but upper bound is MAX_INTLENGTH_SHORT - 1 
					rather than MAX_INTLENGTH - 1.
	LENGTH_SHORT_MIN As LENGTH_SHORT but lower bound is user-specified.
	LENGTH_SHORT_Z	As LENGTH_SHORT but lower bound may be non-zero.

   In addition to these we allow the OPT specifier and OPT and Z suffixes as
   before */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_LENGTH				_In_ _In_range_( 1, MAX_INTLENGTH - 1 )
#define IN_LENGTH_OPT			_In_ _In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_FIXED( size )	_In_ _In_range_( ( size ), ( size ) )
#define IN_LENGTH_MIN( min )	_In_ _In_range_( ( min ), MAX_INTLENGTH - 1 )
#define IN_LENGTH_Z				_In_ _In_range_( 0, MAX_INTLENGTH - 1 )

#define IN_LENGTH_SHORT			_In_ _In_range_( 1, MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_OPT		_In_ _In_range_( CRYPT_UNUSED, MAX_INTLENGTH_SHORT - 1 )
								/* This really is a _OPT and not a _INDEF in 
								   the one place where it's used */
#define IN_LENGTH_SHORT_MIN( min ) _In_ _In_range_( ( min ) , MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_Z		_In_ _In_range_( 0, MAX_INTLENGTH_SHORT - 1 )

#define INOUT_LENGTH_Z			_Inout_ \
								_In_range_( 0, MAX_INTLENGTH - 1 ) \
								_Out_range_( 0, MAX_INTLENGTH - 1 )
#define INOUT_LENGTH_SHORT_Z	_Inout_ \
								_In_range_( 0, MAX_INTLENGTH_SHORT - 1 ) \
								_Out_range_( 0, MAX_INTLENGTH_SHORT - 1 )

#define OUT_LENGTH				_Out_ _Out_range_( 1, MAX_INTLENGTH - 1 )
#define OUT_LENGTH_MIN( min )	_Out_ _Out_range_( ( min ), MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_MIN( min ) _Out_opt_ _Out_range_( ( min ), MAX_INTLENGTH - 1 )
#define OUT_LENGTH_Z			_Out_ _Out_range_( 0, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_Z		_Out_opt_ _Out_range_( 0, MAX_INTLENGTH - 1 )

#define OUT_LENGTH_SHORT		_Out_ _Out_range_( 1, MAX_INTLENGTH_SHORT - 1 )
#define OUT_LENGTH_SHORT_Z		_Out_ _Out_range_( 0, MAX_INTLENGTH_SHORT - 1 )
#define OUT_OPT_LENGTH_SHORT_Z	_Out_opt_ _Out_range_( 0, MAX_INTLENGTH_SHORT - 1 )

#else							/* Declspec SAL */

#define IN_LENGTH				__in __in_range( 1, MAX_INTLENGTH - 1 )
#define IN_LENGTH_OPT			__in __in_range( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_FIXED( size )	__in __in_range( ( size ), ( size ) )
#define IN_LENGTH_MIN( min )	__in __in_range( ( min ), MAX_INTLENGTH - 1 )
#define IN_LENGTH_Z				__in __in_range( 0, MAX_INTLENGTH - 1 )

#define IN_LENGTH_SHORT			__in __in_range( 1, MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_OPT		__in __in_range( CRYPT_UNUSED, MAX_INTLENGTH_SHORT - 1 )
								/* This really is a _OPT and not a _INDEF in 
								   the one place where it's used */
#define IN_LENGTH_SHORT_MIN( min ) __in __in_range( ( min ) , MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_Z		__in __in_range( 0, MAX_INTLENGTH_SHORT - 1 )

#define INOUT_LENGTH_Z			__inout \
								__in_range( 0, MAX_INTLENGTH - 1 ) \
								__out_range( 0, MAX_INTLENGTH - 1 )
#define INOUT_LENGTH_SHORT_Z	__inout \
								__in_range( 0, MAX_INTLENGTH_SHORT - 1 ) \
								__out_range( 0, MAX_INTLENGTH_SHORT - 1 )

#define OUT_LENGTH				__out __out_range( 1, MAX_INTLENGTH - 1 )
#define OUT_LENGTH_MIN( min )	__out __out_range( ( min ), MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_MIN( min ) __out_opt __out_range( ( min ), MAX_INTLENGTH - 1 )
#define OUT_LENGTH_Z			__out __out_range( 0, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_Z		__out_opt __out_range( 0, MAX_INTLENGTH - 1 )

#define OUT_LENGTH_SHORT		__out __out_range( 1, MAX_INTLENGTH_SHORT - 1 )
#define OUT_LENGTH_SHORT_Z		__out __out_range( 0, MAX_INTLENGTH_SHORT - 1 )
#define OUT_OPT_LENGTH_SHORT_Z	__out_opt __out_range( 0, MAX_INTLENGTH_SHORT - 1 )

#endif /* Declspec vs. Attribute SAL */

/* Special-case length checking:

	LENGTH_ATTRIBUTE Value must be a valid length for an attribute.
	LENGTH_DNS		Value must be a valid length for DNS data or a URL.
	LENGTH_DNS_Z	Value must be a valid length for DNS data or a URL,
					including zero-length output, which is returned on error.
	LENGTH_ERRORMESSAGE	Value must be a valid length for an extended error 
					string.
	LENGTH_HASH		Value must be a valid length for a hash.
	LENGTH_INDEF	As LENGTH but may also be CRYPT_UNUSED for indefinite-
					length encoded data.
	LENGTH_IV		Value must be a valid length for a cipher block.  
					Technically this isn't always an IV, but this is about
					the best name to give it.
	LENGTH_KEY		Value must be a valid length for a conventional 
					encryption key.
	LENGTH_KEYID	Value must be a valid length for a key ID lookup.
	LENGTH_NAME		Value must be a valid length for an object name or
					similar type of string, e.g. a database name or a 
					password (this is a bit of a catch-all value for
					strings, unfortunately there's no easily descriptive
					name for the function it performs).
	LENGTH_OID		Value must be a valid length for an OID.
	LENGTH_PKC		Value must be a valid length for PKC data.
	LENGTH_PKC_Z	Value must be a valid length for PKC data, including 
					zero-length output, which is returned on error.
	LENGTH_TEXT		Value must be a valid length for a text string (length
					up to CRYPT_MAX_TEXTSIZE).

   In addition to these we allow the OPT specifier and Z suffix as before */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_LENGTH_ATTRIBUTE		_In_ _In_range_( 1, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_DNS			_In_ _In_range_( 1, MAX_DNS_SIZE )
#define IN_LENGTH_DNS_Z			_In_ _In_range_( 0, MAX_DNS_SIZE )
#define IN_LENGTH_ERRORMESSAGE	_In_ _In_range_( 1, MAX_ERRORMESSAGE_SIZE )
#define IN_LENGTH_HASH			_In_ _In_range_( 16, CRYPT_MAX_HASHSIZE )
#define IN_LENGTH_INDEF			_In_ _In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_IV			_In_ _In_range_( 1, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_IV_Z			_In_ _In_range_( 0, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_KEY			_In_ _In_range_( MIN_KEYSIZE, CRYPT_MAX_KEYSIZE )
#define IN_LENGTH_KEYID			_In_ _In_range_( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_KEYID_Z		_In_ _In_range_( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME			_In_ _In_range_( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME_Z		_In_ _In_range_( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_OID			_In_ _In_range_( 7, MAX_OID_SIZE )
#define IN_LENGTH_PKC			_In_ _In_range_( 1, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_PKC_Z			_In_ _In_range_( 0, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_TEXT			_In_ _In_range_( 1, CRYPT_MAX_TEXTSIZE )
#define IN_LENGTH_TEXT_Z		_In_ _In_range_( 0, CRYPT_MAX_TEXTSIZE )

#define OUT_LENGTH_DNS_Z		_Out_ _Out_range_( 0, MAX_DNS_SIZE )
#define OUT_OPT_LENGTH_HASH_Z	_Out_opt_ _Out_range_( 0, CRYPT_MAX_HASHSIZE )
#define OUT_LENGTH_PKC_Z		_Out_ _Out_range_( 0, CRYPT_MAX_PKCSIZE )
#define OUT_LENGTH_INDEF		_Out_ _Out_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_INDEF	_Out_opt_ _Out_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )

#else							/* Declspec SAL */

#define IN_LENGTH_ATTRIBUTE		__in __in_range( 1, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_DNS			__in __in_range( 1, MAX_DNS_SIZE )
#define IN_LENGTH_DNS_Z			__in __in_range( 0, MAX_DNS_SIZE )
#define IN_LENGTH_ERRORMESSAGE	__in __in_range( 1, MAX_ERRORMESSAGE_SIZE )
#define IN_LENGTH_HASH			__in __in_range( 16, CRYPT_MAX_HASHSIZE )
#define IN_LENGTH_INDEF			__in __in_range( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_IV			__in __in_range( 1, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_IV_Z			__in __in_range( 0, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_KEY			__in __in_range( MIN_KEYSIZE, CRYPT_MAX_KEYSIZE )
#define IN_LENGTH_KEYID			__in __in_range( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_KEYID_Z		__in __in_range( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME			__in __in_range( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME_Z		__in __in_range( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_OID			__in __in_range( 7, MAX_OID_SIZE )
#define IN_LENGTH_PKC			__in __in_range( 1, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_PKC_Z			__in __in_range( 0, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_TEXT			__in __in_range( 1, CRYPT_MAX_TEXTSIZE )
#define IN_LENGTH_TEXT_Z		__in __in_range( 0, CRYPT_MAX_TEXTSIZE )

#define OUT_LENGTH_DNS_Z		__out __out_range( 0, MAX_DNS_SIZE )
#define OUT_OPT_LENGTH_HASH_Z	__out_opt __out_range( 0, CRYPT_MAX_HASHSIZE )
#define OUT_LENGTH_PKC_Z		__out __out_range( 0, CRYPT_MAX_PKCSIZE )
#define OUT_LENGTH_INDEF		__out __out_range( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_INDEF	__out_opt __out_range( CRYPT_UNUSED, MAX_INTLENGTH - 1 )

#endif /* Declspec vs. Attribute SAL */

/* ASN.1 parameter checking, enabled if ANALYSE_ASN1 is defined:

	TAG				Value must be a valid tag or DEFAULT_TAG.  Note that 
					this is the raw tag value before any DER encoding, 
					e.g. '0' rather than 'MAKE_CTAG( 0 )'.
	TAG_EXT			As IN_TAG but may also be ANY_TAG, is used internally 
					by the ASN.1 code to indicate a don't-care tag value.
	TAG_ENCODED		Value must be a valid DER-encoded tag value 

   In addition to these we allow the EXT specifier to indicate that the 
   value may also be ANY_TAG (a don't-care value) or the hidden NO_TAG used 
   to handle reading of data when the tag has already been processed */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_TAG					_In_ _In_range_( DEFAULT_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_EXT				_In_ _In_range_( ANY_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_ENCODED			_In_ _In_range_( NO_TAG, MAX_TAG )
#define IN_TAG_ENCODED_EXT		_In_ _In_range_( ANY_TAG, MAX_TAG )

#define OUT_TAG_Z				_Out_ _Out_range_( 0, MAX_TAG_VALUE - 1 )
#define OUT_TAG_ENCODED_Z		_Out_ _Out_range_( 0, MAX_TAG )

#else							/* Declspec SAL */

#define IN_TAG					__in __in_range( DEFAULT_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_EXT				__in __in_range( ANY_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_ENCODED			__in __in_range( NO_TAG, MAX_TAG )
#define IN_TAG_ENCODED_EXT		__in __in_range( ANY_TAG, MAX_TAG )

#define OUT_TAG_Z				__out __out_range( 0, MAX_TAG_VALUE - 1 )
#define OUT_TAG_ENCODED_Z		__out __out_range( 0, MAX_TAG )

#endif /* Declspec vs. Attribute SAL */

/* Enumerated type checking.  Due to the duality of 'int' and 'enum' under C
   these can normally be mixed freely until it comes back to bite on systems
   where the compiler doesn't treat them quite the same.  The following 
   provides more strict type checking of enums than the compiler would 
   normally provide and relies on the enum being bracketed by xxx_NONE and
   xxx_LAST.

   We also allow an OPT suffix to indicate the use of don't-care values, 
   denoted by xxx_NONE */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_ENUM( name )			_In_ _In_range_( name##_NONE + 1, name##_LAST - 1 )
#define IN_ENUM_OPT( name )		_In_ _In_range_( name##_NONE, name##_LAST - 1 )

#define INOUT_ENUM( name )		_Inout_ \
								_In_range_( name##_NONE + 1, name##_LAST - 1 ) \
								_Out_range_( name##_NONE + 1, name##_LAST - 1 )

#define OUT_ENUM( name )		_Out_ _Out_range_( name##_NONE + 1, name##_LAST - 1 )
#define OUT_ENUM_OPT( name )	_Out_ _Out_range_( name##_NONE, name##_LAST - 1 )

#else							/* Declspec SAL */

#define IN_ENUM( name )			__in __in_range( name##_NONE + 1, name##_LAST - 1 )
#define IN_ENUM_OPT( name )		__in __in_range( name##_NONE, name##_LAST - 1 )

#define INOUT_ENUM( name )		__inout \
								__in_range( name##_NONE + 1, name##_LAST - 1 ) \
								__out_range( name##_NONE + 1, name##_LAST - 1 )

#define OUT_ENUM( name )		__out __out_range( name##_NONE + 1, name##_LAST - 1 )
#define OUT_ENUM_OPT( name )	__out __out_range( name##_NONE, name##_LAST - 1 )

#endif /* Declspec vs. Attribute SAL */

/* Binary flag checking.  This works as for enumerated type checking and 
   relies on the flag definition being bracketed by xxx_FLAG_NONE and 
   xxx_FLAG_MAX */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_FLAGS( name )		_In_ _In_range_( 1, flags##_FLAG_MAX )
#define IN_FLAGS_Z( name )		_In_ _In_range_( flags##_FLAG_NONE, flags##_FLAG_MAX )

#define INOUT_FLAGS( name )		_Inout_ _Inout_range_( flags##_FLAG_NONE, flags##_FLAG_MAX )

#define OUT_FLAGS_Z( name )		_Out_ _Out_range_( flags##_FLAG_NONE, flags##_FLAG_MAX )

#else							/* Declspec SAL */

#define IN_FLAGS( name )		__in __in_range( 1, flags##_FLAG_MAX )
#define IN_FLAGS_Z( name )		__in __in_range( flags##_FLAG_NONE, flags##_FLAG_MAX )

#define INOUT_FLAGS( name )		__inout __inout_range( flags##_FLAG_NONE, flags##_FLAG_MAX )

#define OUT_FLAGS_Z( name )		__out __out_range( flags##_FLAG_NONE, flags##_FLAG_MAX )

#endif /* Declspec vs. Attribute SAL */

/* Buffer parameter checking:

	IN_BUFFER		For { buffer, length } values.
	INOUT_BUFFER	For in-place processing of { buffer, maxLength, *length } 
					values, e.g. decryption with padding removal.
	OUT_BUFFER		For filling of { buffer, maxLength, *length } values.

	INOUT_BUFFER_FIXED For in-place processing of { buffer, length } values, 
					e.g.filtering control chars in a string.
	OUT_BUFFER_FIXED For filling of { buffer, length } values.

   In addition to these we allow the OPT specifier as before */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

/* Attribute SAL doesn't have any definitions covering various types of 
   ( max, count ) declarations so we have to define them ourselves */

#define _Inout_bytecap_post_bytecount_(cap,count)		_Pre_valid_bytecap_(cap) \
														_Post_bytecount_(count)
#define _Inout_bytecap_post_bytecount_c_(cap,count)		_Pre_valid_bytecap_(cap) \
														_Post_bytecount_c_(count)

#define _Out_bytecapcount_c_(count)						_Pre_bytecap_c_(count) _Pre_invalid_ \
														_Post_bytecount_c_(count)
#define _Out_bytecap_post_bytecount_c_(cap,count)		_Pre_bytecap_(cap) _Pre_invalid_ \
														_Post_bytecount_c_(count)
#define _Out_opt_bytecap_post_bytecount_c_(cap,count)	_Pre_opt_bytecap_(cap) _Pre_invalid_ \
														_Post_bytecount_c_(count)

#define IN_BUFFER( count )		_In_bytecount_( count )
#define IN_BUFFER_C( count )	_In_bytecount_c_( count )
#define IN_BUFFER_OPT( count )	_In_opt_bytecount_( count )
#define IN_BUFFER_OPT_C( count ) _In_opt_bytecount_c_( count )

#define INOUT_BUFFER( max, count ) _Inout_bytecap_post_bytecount_( max, count )
#define INOUT_BUFFER_C( max, count ) _Inout_bytecap_post_bytecount_c_( max, count )
#define INOUT_BUFFER_FIXED( count ) _Inout_bytecount_( count )
#define INOUT_BUFFER_OPT( max, count ) _Inout_opt_bytecap_( max, count )

#define OUT_BUFFER( max, count ) _Out_bytecap_post_bytecount_( max, count ) 
#define OUT_BUFFER_C( max, count ) _Out_bytecap_post_bytecount_c_( max, count ) 
#define OUT_BUFFER_OPT( max, count ) _Out_opt_bytecap_post_bytecount_( max, count ) 
#define OUT_BUFFER_OPT_C( max, count ) _Out_opt_bytecap_post_bytecount_c_( max, count ) 
#define OUT_BUFFER_FIXED( count ) _Out_bytecapcount_( count )
#define OUT_BUFFER_FIXED_C( count ) _Out_bytecapcount_c_( count )
#define OUT_BUFFER_OPT_FIXED( count ) _Out_opt_bytecapcount_( count )

#else							/* Declspec SAL */

#define IN_BUFFER( count )		__in_bcount( count )
#define IN_BUFFER_C( count )	__in_bcount( count )
#define IN_BUFFER_OPT( count )	__in_bcount_opt( count )
#define IN_BUFFER_OPT_C( count ) __in_bcount_opt( count )

#define INOUT_BUFFER( max, count ) __inout_bcount( max, count )
#define INOUT_BUFFER_C( max, count ) __inout_bcount( max, count )
#define INOUT_BUFFER_FIXED( count ) __inout_bcount_full( count )
#define INOUT_BUFFER_OPT( max, count ) __inout_bcount_opt( max, count )

#define OUT_BUFFER( max, count ) __out_bcount_part( max, count ) 
#define OUT_BUFFER_C( max, count ) __out_bcount_part( max, count ) 
#define OUT_BUFFER_OPT( max, count ) __out_bcount_part_opt( max, count ) 
#define OUT_BUFFER_OPT_C( max, count ) __out_bcount_part_opt( max, count ) 
#define OUT_BUFFER_FIXED( count ) __out_bcount_full( count )
#define OUT_BUFFER_FIXED_C( count ) __out_bcount_full( count )
#define OUT_BUFFER_OPT_FIXED( count ) __out_bcount_full_opt( count )

#endif /* Declspec vs. Attribute SAL */

/* Array parameter checking:

	ARRAY			For { fooType foo[], int fooCount } values */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define IN_ARRAY( count )		_In_count_( count )
#define IN_ARRAY_C( count )		_In_count_c_( count )
#define IN_ARRAY_OPT( count )	_In_opt_count_( count )
#define IN_ARRAY_OPT_C( count )	_In_opt_count_c_( count )

#define INOUT_ARRAY( count )	_Inout_cap_( count )
#define INOUT_ARRAY_C( count )	_Inout_cap_c_( count )

#define OUT_ARRAY( count )		_Out_cap_( count )
#define OUT_ARRAY_C( count )	_Out_cap_c_( count )
#define OUT_ARRAY_OPT( count )	_Out_opt_cap_( count )
#define OUT_ARRAY_OPT_C( count ) _Out_opt_cap_c_( count )

#else							/* Declspec SAL */

#define IN_ARRAY( count )		__in_ecount( count )
#define IN_ARRAY_C( count )		__in_ecount( count )
#define IN_ARRAY_OPT( count )	__in_ecount_opt( count )
#define IN_ARRAY_OPT_C( count )	__in_ecount_opt( count )

#define INOUT_ARRAY( count )	__inout_ecount( count )
#define INOUT_ARRAY_C( count )	__inout_ecount( count )

#define OUT_ARRAY( count )		__out_ecount( count )
#define OUT_ARRAY_C( count )	__out_ecount( count )
#define OUT_ARRAY_OPT( count )	__out_ecount_opt( count )
#define OUT_ARRAY_OPT_C( count ) __out_ecount_opt( count )

#endif /* Declspec vs. Attribute SAL */

/* Structures that encapsulate data-handling operations:

	ARRAY			Array of total allocated size 'max', currently filled
					to 'count' elements.
	BUFFER			Buffer of total allocated size 'max', currently filled 
					to 'count' bytes.
	BUFFER_FIXED	Buffer of total allocated and filled size 'max'.

   In addition to these we allow the OPT specifier as before, and the 
   UNSPECIFIED specifier to indicate that the fill state of the buffer 
   isn't specified or at least is too complex to describe to PREfast, for 
   example an I/O buffer that acts as BUFFER on read but BUFFER_FIXED on 
   write */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define ARRAY( max, count )		/* No equivalent in attribute SAL */
#define ARRAY_FIXED( max )		/* No equivalent in attribute SAL */
#define BUFFER( max, count )	/* No equivalent in attribute SAL */
#define BUFFER_OPT( max, count ) /* No equivalent in attribute SAL */
#define BUFFER_FIXED( max )		/* No equivalent in attribute SAL */
#define BUFFER_OPT_FIXED( max )	/* No equivalent in attribute SAL */
#define BUFFER_UNSPECIFIED( max ) /* No equivalent in attribute SAL */

#else							/* Declspec SAL */

#define ARRAY( max, count )		__field_ecount_part( ( max ), ( count ) )
#define ARRAY_FIXED( max )		__field_ecount_full( max )
#define BUFFER( max, count )	__field_bcount_part( ( max ), ( count ) )
#define BUFFER_OPT( max, count ) __field_bcount_part_opt( ( max ), ( count ) )
#define BUFFER_FIXED( max )		__field_bcount_full( max )
#define BUFFER_OPT_FIXED( max )	__field_bcount_full_opt( max )
#define BUFFER_UNSPECIFIED( max ) __field_bcount( max )

#endif /* Declspec vs. Attribute SAL */

/* Memory-allocation functions that allocate and return a block of 
   initialised memory.  Although this is documented as being usable for 
   functions that take parameters "void **ptr, int *length', this doesn't 
   actually work because when you get down to the attribute level you've
   got 
   "([Pre(ValidElements="*length")] void **ptr, [Pre(Valid=SA_No)]int *length", 
   and since the *length value isn't valid at this point PREfast produces an 
   error 6525 "Invalid size specification".  The way to make it valid is to 
   switch to 
   "[Pre(ValidElements="length")] void **ptr, [Pre(Valid=SA_Yes)]int length)",
   but we don't know the length when the function is called.  Although we
   could break the annotations down into two special cases, one for 
   "int length" (Valid=SA_Yes) and the other for "int *length" (Valid=SA_No), 
   this is getting kind of messy, and PREfast doesn't seem to produce any 
   better diagnostics for this than a more generic __deref_out/_Deref_out_, 
   so we just use __deref_out for now */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#if 0	/* See comment above */
#define OUT_BUFFER_ALLOC( length )		_Out_capcount_( length )
#define OUT_BUFFER_ALLOC_OPT( length )	_Out_opt_capcount_( length )
#else
#define OUT_BUFFER_ALLOC( length )		_Deref_out_
#define OUT_BUFFER_ALLOC_OPT( length )	_Deref_out_opt_
#endif /* 0 */

#else							/* Declspec SAL */

#define OUT_BUFFER_ALLOC( length )		__deref_out_bcount_full( length )
#define OUT_BUFFER_ALLOC_OPT( length )	__deref_out_bcount_full_opt( length )

#endif /* Declspec vs. Attribute SAL */

/* Typeless annotations used in situations where the size or type is 
   implicit, e.g. a pointer to a structure.  The annotation 'IN' is 
   implied by 'const' so we wouldn't normally use it, but it can be useful 
   when we're re-prototyping a non-cryptlib function that doesn't use 
   'const's */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#ifdef IN		/* Defined to no-ops in some versions of windef.h */
  #undef IN
  #undef OUT
#endif

#define IN						_In_
#define IN_OPT					_In_opt_
#define INOUT					_Inout_
#define INOUT_OPT				_Inout_opt_
#define OUT						_Out_
#define OUT_OPT					_Out_opt_

#else							/* Declspec SAL */

#define IN						__in
#define IN_OPT					__in_opt
#define INOUT					__inout
#define INOUT_OPT				__inout_opt
#define OUT						__out
#define OUT_OPT					__out_opt

#endif /* Declspec vs. Attribute SAL */

/* Pointer annotations */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define INOUT_PTR				_Deref_inout_bound_
								/* There's no _Deref_inout_, the _bound_ 
								   veriant seems to work though */
#define OUT_PTR					_Deref_out_
#define OUT_OPT_PTR				_Deref_out_opt_
#define OUT_OPT_PTR_OPT			_Deref_opt_out_opt_

#else							/* Declspec SAL */

#define INOUT_PTR				__deref_inout
#define OUT_PTR					__deref_out
#define OUT_OPT_PTR				__deref_out_opt
#define OUT_OPT_PTR_OPT			__deref_opt_out_opt

#endif /* Declspec vs. Attribute SAL */

/* Other annotations:

	CALLBACK_FUNCTION Function is a callback function (no-one seems to know 
					what this annotation actually does).
	FORMAT_STRING	Argument is a printf-style format string.
	IN_STRING		Argument is a null-terminated string.
	TYPECAST		Type cast, e.g. from void * to struct foo * */

#if VC_GE_2008( _MSC_VER )		/* Attribute SAL */

#define ANALYSER_HINT( expr )	__analysis_assume( expr )
								/* Attribute SAL has no equivalent for this, 
								   but __analysis_assume() still works */
#define CALLBACK_FUNCTION		/* No equivalent in attribute SAL */
#define FORMAT_STRING			_Printf_format_string_
#define IN_STRING				_In_z_
#define IN_STRING_OPT			_In_opt_z_
#define TYPECAST( type )		/* No equivalent in attribute SAL */

#else							/* Declspec SAL */

#define ANALYSER_HINT( expr )	__analysis_assume( expr )
#define CALLBACK_FUNCTION		__callback
#define FORMAT_STRING			__format_string
#define IN_STRING				__in_z
#define IN_STRING_OPT			__in_z_opt
#define TYPECAST( type )		__typefix( type )

#endif /* Declspec vs. Attribute SAL */

#endif /* PREfast */

/* Handling of C'0x analysis */

#if defined( _MSC_VER ) && !VC_16BIT( _MSC_VER )

#define STDC_NONNULL_ARG( argIndex )
#define STDC_PRINTF_FN( formatIndex, argIndex )
#define STDC_PURE		__declspec( noalias )
#if defined( _MSC_VER ) && defined( _PREFAST_ ) 
  /* The use of the declspec annotation cancels the ability to use attribute 
     SAL, so if we're using that we have to no-op out this definition since
	 there's no attribute SAL equivalent */
  #ifdef __ATTR_SAL
	#define STDC_UNUSED
  #else
	#define STDC_UNUSED	__reserved
  #endif /* Declspec vs. attribute SAL */
#else
  #define STDC_UNUSED
#endif /* VC++ with/without PREfast */

#endif /* VC++ */

/****************************************************************************
*																			*
*							gcc/C'0x Analysis Support 						*
*																			*
****************************************************************************/

#if defined( __GNUC__ ) && ( __GNUC__ >= 4 )

/* Future versions of the C standard will support the ability to annotate 
   functions to allow the compiler to perform extra checking and assist with
   code generation.  Currently only gcc supports this annotation (and even
   that in a gcc-specific manner), in order to handle this we define macros
   for the proposed C-standard annotation, which defines attributes of the
   form "stdc_<name>", although how they'll be applied is still undecided.

   Unfortunately neither gcc's STDC_NONNULL_ARG nor its CHECK_RETVAL 
   checking work properly.  CHECK_RETVAL is the least broken since it merely 
   fails to report a return value being unchecked in many cases while
   reporting unnecessary false positives in others, but STDC_NONNULL_ARG is 
   downright dangerous since it'll break correctly functioning code.
   
   The problem with CHECK_RETVAL is that it regards any "use" of the return 
   value of a function, for example assigning it to a variable that's never 
   used, as fulfilling the conditions for "use", and therefore issues no 
   warning.  On the other hand the standard "(void)" cast that's been used 
   to indicate that you genuinely want to ignore the return value of a 
   function since circa 1979 with lint is ignored, resulting in warnings 
   where there shouldn't be any.

   STDC_NONNULL_ARG on the other hand is far more broken since the warnings
   are issued by the front-end before data flow analysis occurs (so many
   cases of NULL pointer use are missed) but then the optimiser takes the
   annotation to mean that that value can never be NULL and *removes any
   code that might check for a NULL pointer*!  This is made even worse by
   the awkward way that the annotation works, requiring hand-counting the
   parameters and providing an index into the parameter list instead of
   placing it next to the parameter as for STDC_UNUSED.

   For both of these issues the gcc maintainers' response was "not our 
   problem/it's behaving as intended".
   
   (This isn't the only time that gcc maintainers have introduced faults 
   and/or security flaws into gcc-generated code.  When they changed gcc to 
   optimise away integer-overflow checks like 'a*b/b != a' (see "IntPatch: 
   Automatically Fix Integer-Overflow-to-Buffer-Overflow Vulnerability at 
   Compile Time", Zhang et al, ESORICS'10) or 'a + 100 > 0' they spent a 55-
   message thread arguing over why it was OK to do this (see 
   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=30475) rather than actually 
   fixing it (although in this case the lack of tact of the bug submitter 
   can't have helped either) */

#define STDC_NONNULL_ARG( argIndex ) \
		__attribute__(( nonnull argIndex ))
#define STDC_PRINTF_FN( formatIndex, argIndex ) \
		__attribute__(( format( printf, formatIndex, argIndex ) ))
#define STDC_PURE		__attribute__(( pure ))
#define STDC_UNUSED		__attribute__(( unused ))

/* The return-value-checking annotation should really be 
   STDC_WARN_UNUSED_RESULT but since the PREfast attributes are defined and 
   widely used within cryptlib while the C standard ones don't even 
   officially exist yet, we allow the PREfast naming to take precedence */

#define CHECK_RETVAL \
		__attribute__(( warn_unused_result ))
#define CHECK_RETVAL_BOOL				CHECK_RETVAL
#define CHECK_RETVAL_ENUM( name )		CHECK_RETVAL
#define CHECK_RETVAL_LENGTH				CHECK_RETVAL
#define CHECK_RETVAL_PTR				CHECK_RETVAL
#define CHECK_RETVAL_RANGE( low, high )	CHECK_RETVAL
#define CHECK_RETVAL_SPECIAL			CHECK_RETVAL
#define CHECK_RETVAL_STRINGOP( length )	CHECK_RETVAL

#define CHECK_RETVAL_FNPTR				CHECK_RETVAL
#define CHECK_RETVAL_BOOL_FNPTR			CHECK_RETVAL
#define CHECK_RETVAL_LENGTH_FNPTR		CHECK_RETVAL
#define CHECK_RETVAL_PTR_FNPTR			CHECK_RETVAL
#define CHECK_RETVAL_SPECIAL_FNPTR		CHECK_RETVAL

#define CHECK_RETVAL_ERROR				CHECK_RETVAL

/* gcc's handling of both warn_unused_result and nonnull is just too broken
   to safely use it in any production code, because of this we require the 
   use to be explicitly enabled.  Since clang/LLVM share the gcc front-end
   and do actually get things right, we don't disable them if we're using
   clang */

#if !( defined( USE_GCC_ATTRIBUTES ) || defined( __clang_analyzer__ ) )
  #undef STDC_NONNULL_ARG
  #define STDC_NONNULL_ARG( argIndex )
  #undef CHECK_RETVAL
  #define CHECK_RETVAL
#endif /* gcc with use of dangerous attributes disabled */

/* clang/LLVM uses gcc as a front-end so we can also end up in the gcc case 
   if we're using clang.  The following defines are for clang with gcc as a
   front-end */

#ifdef __clang_analyzer__

/* Since clang, unlike gcc, gets CHECK_RETVAL and STDC_NONNULL_ARG right, we 
   can enable it not only for cryptlib functions but also for C library 
   ones.  To do this we override at-risk functions with our own ones whose 
   prototypes allow for extended checking with clang */

CHECK_RETVAL \
	void *_checked_malloc( size_t size );
STDC_NONNULL_ARG( ( 1 ) ) \
	void _checked_free( void *ptr );

#define malloc( x )			_checked_malloc( x )
#define free( x )			_checked_free( x )

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
	int _checked_memcmp( const void *ptr1, const void *ptr2, size_t num );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
	void *_checked_memcpy( void *dest, const void *src, size_t num );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
	void *_checked_memmove( void *dest, const void *src, size_t num );
STDC_NONNULL_ARG( ( 1 ) ) \
	void *_checked_memset( void *ptr, int value, size_t num );

#define memcmp( x, y, z )	_checked_memcmp( x, y, z )
#define memcpy( x, y, z )	_checked_memcpy( x, y, z )
#define memmove( x, y, z )	_checked_memmove( x, y, z )
#define memset( x, y, z )	_checked_memset( x, y, z )

/* Unfortunately clang doesn't provide any real means of guiding the 
   analyser, the best that we can do is use an ENSURES() */

#define ANALYSER_HINT( expr )	ENSURES( expr )

#endif /* clang/LLVM */

/* gcc can also provide approximate guessing at buffer overflows.  This is 
   sufficiently hit-and-miss, and subject to platform-specific false 
   positives and other glitches, that we only enable it for the test build 
   in case it catches something there.  clang does a much better job at 
   finding issues without all of the accompanying problems */

#ifdef USE_GCC_ATTRIBUTES

#define bos( x ) __builtin_object_size( x, 0 )

#undef memcpy
#define memcpy( dest, src, count ) \
		__builtin___memcpy_chk( dest, src, count, bos( dest ) )
#undef memmove
#define memmove( dest, src, count ) \
		__builtin___memmove_chk( dest, src, count, bos( dest ) )
#undef memset
#define memset( ptr, value, count ) \
		__builtin___memset_chk( ptr, value, count, bos( ptr ) )
#undef strcpy
#define strcpy( dest, src ) \
		__builtin___strcpy_chk( dest, src, bos( dest ) )
#undef strncpy
#define strncpy( dest, src, size ) \
		__builtin___strncpy_chk( dest, src, size, bos( dest ) )
#undef strcat
#define strcat( dest, src ) \
		__builtin___strcat_chk( dest, src, bos( dest ) )

#endif /* gcc on the test platform */

#endif /* gcc/C'0x */

/****************************************************************************
*																			*
*								No Analysis 								*
*																			*
****************************************************************************/

#ifndef CHECK_RETVAL

#define CHECK_RETVAL
#define CHECK_RETVAL_BOOL
#define CHECK_RETVAL_ENUM( name )
#define CHECK_RETVAL_LENGTH
#define CHECK_RETVAL_PTR
#define CHECK_RETVAL_RANGE( low, high )
#define CHECK_RETVAL_SPECIAL
#define CHECK_RETVAL_STRINGOP( length )

#define CHECK_RETVAL_FNPTR
#define CHECK_RETVAL_BOOL_FNPTR
#define CHECK_RETVAL_LENGTH_FNPTR
#define CHECK_RETVAL_PTR_FNPTR
#define CHECK_RETVAL_SPECIAL_FNPTR

#define CHECK_RETVAL_ERROR

#endif /* No basic analysis support */

#ifndef RETVAL

#define RETVAL
#define RETVAL_BOOL
#define RETVAL_RANGE( low, high )

#define RETVAL_FNPTR

#define IN_INT
#define IN_INT_OPT
#define IN_INT_Z
#define IN_INT_SHORT
#define IN_INT_SHORT_Z
#define OUT_INT_Z
#define OUT_INT_SHORT_Z
#define OUT_OPT_INT_Z

#define IN_ALGO
#define IN_ALGO_OPT
#define IN_ATTRIBUTE
#define IN_ATTRIBUTE_OPT
#define IN_BYTE
#define IN_CHAR
#define IN_ERROR
#define IN_HANDLE
#define IN_HANDLE_OPT
#define IN_KEYID
#define IN_KEYID_OPT
#define IN_MESSAGE
#define IN_MODE
#define IN_MODE_OPT
#define IN_PORT
#define IN_PORT_OPT
#define IN_RANGE( min, max )
#define IN_RANGE_FIXED( value )
#define IN_STATUS
#define INOUT_HANDLE
#define INOUT_RANGE( min, max )
#define OUT_ALGO_Z
#define OUT_OPT_ALGO_Z
#define OUT_ATTRIBUTE_Z
#define OUT_OPT_ATTRIBUTE_Z
#define OUT_BOOL
#define OUT_OPT_BOOL
#define OUT_OPT_BYTE
#define OUT_ERROR
#define OUT_HANDLE_OPT
#define OUT_OPT_HANDLE_OPT
#define OUT_PORT_Z
#define OUT_RANGE( min, max )
#define OUT_OPT_RANGE( min, max )
#define OUT_STATUS

#define IN_LENGTH
#define IN_LENGTH_OPT
#define IN_LENGTH_FIXED( size )
#define IN_LENGTH_MIN( min )
#define IN_LENGTH_Z
#define IN_LENGTH_SHORT
#define IN_LENGTH_SHORT_MIN( min )
#define IN_LENGTH_SHORT_OPT
#define IN_LENGTH_SHORT_Z
#define INOUT_LENGTH_Z
#define INOUT_LENGTH_SHORT_Z
#define OUT_LENGTH
#define OUT_LENGTH_Z
#define OUT_OPT_LENGTH_Z
#define OUT_LENGTH_SHORT
#define OUT_OPT_LENGTH_SHORT_Z
#define OUT_LENGTH_SHORT_Z

#define IN_LENGTH_ATTRIBUTE
#define IN_LENGTH_DNS
#define IN_LENGTH_DNS_Z
#define IN_LENGTH_ERRORMESSAGE
#define IN_LENGTH_HASH
#define IN_LENGTH_INDEF
#define IN_LENGTH_IV
#define IN_LENGTH_IV_Z
#define IN_LENGTH_KEY
#define IN_LENGTH_KEYID
#define IN_LENGTH_KEYID_Z
#define IN_LENGTH_NAME
#define IN_LENGTH_NAME_Z
#define IN_LENGTH_OID
#define IN_LENGTH_PKC
#define IN_LENGTH_PKC_Z
#define IN_LENGTH_TEXT
#define IN_LENGTH_TEXT_Z
#define OUT_LENGTH_DNS_Z
#define OUT_OPT_LENGTH_HASH_Z
#define OUT_LENGTH_PKC_Z
#define OUT_LENGTH_INDEF
#define OUT_OPT_LENGTH_INDEF

#define IN_TAG
#define IN_TAG_EXT
#define IN_TAG_ENCODED
#define IN_TAG_ENCODED_EXT
#define OUT_TAG_Z
#define OUT_TAG_ENCODED_Z

#define IN_ENUM( name )
#define IN_ENUM_OPT( name )
#define INOUT_ENUM( name )
#define OUT_ENUM( name )
#define OUT_ENUM_OPT( name )

#define IN_FLAGS( name )
#define IN_FLAGS_Z( name )
#define OUT_FLAGS_Z( name )

#define IN_BUFFER( size )
#define IN_BUFFER_C( size )
#define IN_BUFFER_OPT( size )
#define IN_BUFFER_OPT_C( count )
#define INOUT_BUFFER( max, size )
#define INOUT_BUFFER_C( max, size )
#define INOUT_BUFFER_FIXED( size )
#define INOUT_BUFFER_OPT( max, count )
#define OUT_BUFFER( max, size )
#define OUT_BUFFER_C( max, size )
#define OUT_BUFFER_FIXED( max )
#define OUT_BUFFER_FIXED_C( count )
#define OUT_BUFFER_OPT( max, size )
#define OUT_BUFFER_OPT_C( max, size )
#define OUT_BUFFER_OPT_FIXED( max )

#define IN_ARRAY( count )
#define IN_ARRAY_C( count )
#define IN_ARRAY_OPT( count )
#define IN_ARRAY_OPT_C( count )
#define INOUT_ARRAY( count )
#define INOUT_ARRAY_C( count )
#define OUT_ARRAY( count )
#define OUT_ARRAY_C( count )
#define OUT_ARRAY_OPT( count )
#define OUT_ARRAY_OPT_C( count )

#define ARRAY( max, count )
#define ARRAY_FIXED( max )
#define BUFFER( max, count )
#define BUFFER_OPT( max, count )
#define BUFFER_FIXED( max )
#define BUFFER_OPT_FIXED( max )
#define BUFFER_UNSPECIFIED( max )

#define OUT_BUFFER_ALLOC( length )
#define OUT_BUFFER_ALLOC_OPT( length )

#define IN
#define IN_OPT
#define INOUT
#define INOUT_OPT
#define OUT
#define OUT_OPT

#define INOUT_PTR
#define OUT_PTR
#define OUT_OPT_PTR
#define OUT_OPT_PTR_OPT

#ifndef ANALYSER_HINT
  #define ANALYSER_HINT( expr )
#endif /* ANALYSER_HINT */
#if defined( __WINCE__ )
  /* The Windows CE SDK defines CALLBACK_FUNCTION itself but the CE version 
     is never used by cryptlib so we simply undefine the CE version */
  #undef CALLBACK_FUNCTION
#endif /* WinCE */
#define CALLBACK_FUNCTION
#define FORMAT_STRING
#define IN_STRING
#define IN_STRING_OPT
#define TYPECAST( ctype )

#endif /* No extended analysis support */

#ifndef STDC_NONNULL_ARG

#define STDC_NONNULL_ARG( argIndex )
#define STDC_PRINTF_FN( formatIndex, argIndex )
#define STDC_PURE 
#define STDC_UNUSED

#endif /* No C'0x attribute support */

#endif /* _ANALYSE_DEFINED */
