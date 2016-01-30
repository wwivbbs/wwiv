/****************************************************************************
*																			*
*					  cryptlib Source Analysis Header File 					*
*						Copyright Peter Gutmann 1997-2014					*
*																			*
****************************************************************************/

#ifndef _ANALYSE_DEFINED

#define _ANALYSE_DEFINED

/* A symbolic define for the maximum possible error value when we're 
   checking ranges for status codes */

#define MAX_ERROR		CRYPT_ARGERROR_NUM2

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
   VC++ 2008, and then from attribute SAL 1.0 to attribute SAL 2.0 some time 
   between VC++ 2010 and VC++ 2013.  This is complicated somewhat by the 
   fixer-upper nature of the SAL material, many of the annotations aren't 
   documented in MSDN, some are only available in MS-internal versions of 
   PREfast, and some are defined to no-ops.  Because of this we have to
   try and kludge some annotations together ourselves from low-level 
   primitives */

#include <sal.h>

/* Microsoft document an annotation _Deref_inout_range_(), however this 
   doesn't exist in any version of sal.h up to at least VS 2013.  To get
   around this problem, we define it ourselves as best we can */

#ifndef _Deref_inout_range_
  #define _Deref_pre_range_( lb, ub )		_Deref_in_range_impl_( lb, ub )
  #define _Deref_post_range_( lb, ub )		_Deref_out_range_impl_( lb, ub )

  #define _Deref_prepost_range_( lb, ub )                 \
		  _SAL1_1_Source_( _Deref_prepost_range_, ( lb, ub ), \
						   _Deref_pre_range_( lb, ub ) _Deref_post_range_( lb, ub ) )

  #define _Deref_inout_range_( min, max )	_Deref_prepost_range_( min, max )
#endif /* !_Deref_inout_range_ */

/* There's no _Deref_opt_out_range_(), this is an attempt to construct it 
   from existing primtiives */

#define _Deref_opt_out_range_( lb, ub ) \
		_SAL2_Source_( _Deref_out_range_, ( lb, ub ), \
					   _Out_opt_impl_ _Deref_out_range_impl_( lb, ub ) )

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
	RETVAL_PTR_NONNULL As CHECK_RETVAL but result is a pointer that's
					guaranteed to never be NULL.
	RETVAL_RANGE	As CHECK_RETVAL but result must be in the range 
					{ low...high } inclusive.
	RETVAL_SPECIAL	As RETVAL but OK_SPECIAL is treated as CRYPT_OK.
	RETVAL_STRINGOP	Function returns either an error code or a string 
					position/index, used for the strXYZ() functions.

   The ranged variants of RETVAL (RETVAL_LENGTH, RETVAL_RANGE, 
   RETVAL_STRINGOP) require an additional annotation because there are 
   actually two ranges that need to be specified, the overall range of 
   values that can be returned and the range that gets returned on success, 
   which is a subset of the full set of values.  For example a function that 
   returns a length returns 0...MAX_LENGTH on success but MAX_ERROR...
   MAX_LENGTH in general.

   Alongside the standard RETVAL_xxxx there's also a RETVAL_xxxx_NOERROR
   to indicate that any returned value is valid, for example when 
   calculating a checksum value.

   The form for some of the RETVALs is a bit odd, they should be 
   '== CRYPT_OK' or '== TRUE' but when given in that form PREfast doesn't 
   recognise cryptStatusError() or '!' as a check for failure and warns 
   about uninitialised values being used after the cryptStatusError()/
   boolean check has confirmed that all was OK.

   CHECK_RETVAL_ENUM doesn't have a _Success_ specifier because all enum 
   values are valid return values, typically one is assigned to indicate
   an error status but its interpretation is up to the caller.  In some
   cases all but one are an error status, for example when a function 
   evaluates the CRYPT_ERRTYPE_TYPE to return for a failed operation.

   This is additionally complicated by the fact that attribute SAL doesn't 
   allow RETVAL in typedefs and function pointers (since they're not valid
   as attributes there) so we have to use a special-case typedef/function-
   pointer version that's no-oped out */

#define CHECK_RETVAL			_Check_return_ \
								_Success_( return >= CRYPT_OK ) \
								_Ret_range_( MAX_ERROR, CRYPT_OK )
#define CHECK_RETVAL_BOOL		_Check_return_ \
								_Success_( return != FALSE ) \
								_Ret_range_( FALSE, TRUE )
#define CHECK_RETVAL_ENUM( name ) \
								_Check_return_ \
								_Ret_range_( name##_NONE, name##_LAST )
#define CHECK_RETVAL_LENGTH		_Check_return_ \
								_Success_( return >= 0 && return <= MAX_INTLENGTH - 1 ) \
								_Ret_range_( MAX_ERROR, MAX_INTLENGTH - 1 ) 
#define CHECK_RETVAL_LENGTH_SHORT \
								_Check_return_ \
								_Success_( return >= 0 && return <= MAX_INTLENGTH_SHORT - 1 ) \
								_Ret_range_( MAX_ERROR, MAX_INTLENGTH_SHORT - 1 ) 
#define CHECK_RETVAL_LENGTH_SHORT_NOERROR \
								_Check_return_ \
								_Ret_range_( MAX_ERROR, MAX_INTLENGTH_SHORT - 1 ) 
#define CHECK_RETVAL_PTR		_Check_return_ \
								_Ret_maybenull_ \
								_Success_( return != NULL )
#define CHECK_RETVAL_PTR_NONNULL \
								_Check_return_ \
								_Ret_notnull_
#define CHECK_RETVAL_RANGE( low, high ) \
								_Check_return_ \
								_Success_( return >= ( low ) && return <= ( high ) ) \
								_Ret_range_( MAX_ERROR, high ) 
#define CHECK_RETVAL_RANGE_NOERROR( low, high ) \
								_Check_return_ \
								_Ret_range_( low, high ) 
#define CHECK_RETVAL_SPECIAL	_Check_return_ \
								_Success_( return >= CRYPT_OK || return == OK_SPECIAL ) \
								_Ret_range_( OK_SPECIAL, CRYPT_OK )
#define CHECK_RETVAL_STRINGOP \
								_Check_return_ \
								_Success_( return >= 0 && return <= strLen ) \
								_Ret_range_( -1, strLen ) 

#define RETVAL					_Success_( return >= CRYPT_OK ) \
								_Ret_range_( MAX_ERROR, CRYPT_OK )
#define RETVAL_BOOL				_Success_( return != FALSE ) \
								_Ret_range_( FALSE, TRUE )
#define RETVAL_LENGTH_NOERROR	_Ret_range_( 0, MAX_INTLENGTH - 1 ) 
#define RETVAL_RANGE( low, high ) \
								_Success_( return >= ( low ) && return <= ( high ) ) \
								_Ret_range_( MAX_ERROR, high ) 
#define RETVAL_RANGE_NOERROR( low, high ) \
								_Ret_range_( low, high ) 
#define RETVAL_SPECIAL			_Success_( return >= CRYPT_OK || return == OK_SPECIAL ) \
								_Ret_range_( OK_SPECIAL, CRYPT_OK )

/* The standard RETVAL is MAX_ERROR ... CRYPT_OK, RETVAL_SPECIAL lies 
   outside this range for a total range OK_SPECIAL ... CRYPTO_OK.  The
   following check ensures that this is the case, the comparison is
   reversed because the values are negative */

#if OK_SPECIAL > MAX_ERROR
  #error OK_SPECIAL must have a larger (negative) magnitude than MAX_ERROR
#endif /* Check for OK_SPECIAL outside the range of MAX_ERROR */

/* When we return from an error handler, returning an error status means 
   that the function succeeded, since it's forwarding a status that it was
   given rather than reporting its own status.  In this case returning an
   error code indicates success */

#define CHECK_RETVAL_ERROR		_Check_return_ \
								_Success_( return < CRYPT_OK - 1 )

/* Some functions acquire and release mutexes as part of their operation.
   These require additional annotations to cover the operation of the 
   mutex */

#define CHECK_RETVAL_ACQUIRELOCK( lockName ) \
								_Check_return_ \
								_Success_( return >= CRYPT_OK, _Acquires_lock_( lockName ) )

#define RETVAL_RELEASELOCK( lockName ) \
								_Requires_lock_held_( lockName ) \
								_Success_( return >= CRYPT_OK, _Releases_lock_( lockName ) )
#define RELEASELOCK( lockName ) _Requires_lock_held_( lockName ) \
								_Releases_lock_( lockName )

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

#define IN_INT					_In_range_( 1, MAX_INTLENGTH - 1 ) 
#define IN_INT_OPT				_In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 ) 
#define IN_INT_Z				_In_range_( 0, MAX_INTLENGTH - 1 ) 
#define IN_INT_SHORT			_In_range_( 1, MAX_INTLENGTH_SHORT - 1 ) 
#define IN_INT_SHORT_Z			_In_range_( 0, MAX_INTLENGTH_SHORT - 1 ) 

#define OUT_INT					_Deref_out_range_( 1, MAX_INTLENGTH - 1 ) 
#define OUT_INT_Z				_Deref_out_range_( 0, MAX_INTLENGTH - 1 ) 
#define OUT_INT_SHORT_Z			_Deref_out_range_( 0, MAX_INTLENGTH_SHORT - 1 ) 
#define OUT_OPT_INT_Z			_Deref_out_range_( 0, MAX_INTLENGTH - 1 ) 

#define INOUT_INT_Z				_Deref_inout_range_( 0, MAX_INTLENGTH - 1 )

/* Special-case parameter checking:

	ALGO		Value must be a cryptlib encryption algorithm.
	ATTRIBUTE	Value must be a cryptlib attribute.
	BOOL		Value must be boolean.
	BYTE		Value must be a single-byte value.
	CHAR		Value must be a 7-bit ASCII character.
	ERROR		Value must be a cryptlib error status code.
	HANDLE		Value must be a cryptlib handle.
	INDEX		Like RANGE but can output CRYPT_ERROR on error.  This is 
				used when returning an index into an array, for which the
				default not-initialised value of 0 is actually a valid 
				result.
	MESSAGE		Value must be a cryptlib message type.
	MODE		Value must be a cryptlib encryption mode.
	PORT		Value must be a network port.
	RANGE		User-specified range check.
	STATUS		Value must be cryptlib status code

   In addition to these we allow the OPT specifier to indicate that the 
   value may be NULL for OUT parameters.  Note that for the OUT versions 
   there's no _opt_ version of _Deref_out_range_ so the best that we can do
   is use a straight _Deref_out_range_.
   
   We also allow an OPT suffix to indicate the use of don't-care values, 
   typically CRYPT_UNUSED */

#define IN_ALGO					_In_range_( CRYPT_ALGO_NONE + 1, CRYPT_ALGO_LAST - 1 )
#define IN_ALGO_OPT				_In_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define IN_ATTRIBUTE			_In_range_( CRYPT_ATTRIBUTE_NONE + 1, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_ATTRIBUTE_OPT		_In_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define IN_BYTE					_In_range_( 0, 0xFF )
#define IN_CHAR					_In_range_( 0, 0x7F )
#define IN_ERROR				_In_range_( MAX_ERROR, -1 )
#define IN_HANDLE				_In_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define IN_HANDLE_OPT			_In_range_( CRYPT_UNUSED, MAX_OBJECTS - 1 )
#define IN_KEYID				_In_range_( CRYPT_KEYID_NONE + 1, CRYPT_KEYID_LAST - 1 )
#define IN_KEYID_OPT			_In_range_( CRYPT_KEYID_NONE, CRYPT_KEYID_LAST - 1 )
#define IN_MESSAGE				_In_range_( MESSAGE_NONE + 1, IMESSAGE_LAST - 1 )
#define IN_MODE					_In_range_( CRYPT_MODE_NONE + 1, CRYPT_MODE_LAST - 1 )
#define IN_MODE_OPT				_In_range_( CRYPT_MODE_NONE, CRYPT_MODE_LAST - 1 )
#define IN_PORT					_In_range_( 22, 65535L )
#define IN_PORT_OPT				_In_range_( CRYPT_UNUSED, 65535L )
#define IN_RANGE( min, max )	_In_range_( ( min ), ( max ) )
#define IN_RANGE_FIXED( value )	_In_range_( ( value ), ( value ) )
#define IN_STATUS				_In_range_( MAX_ERROR, CRYPT_OK )

#define INOUT_HANDLE			_Deref_inout_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define INOUT_RANGE( min, max )	_Deref_inout_range_( ( min ), ( max ) )

#define OUT_ALGO_Z				_Deref_out_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_OPT_ALGO_Z			_Deref_out_range_( CRYPT_ALGO_NONE, CRYPT_ALGO_LAST - 1 )
#define OUT_ATTRIBUTE_Z			_Deref_out_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_OPT_ATTRIBUTE_Z		_Deref_out_range_( CRYPT_ATTRIBUTE_NONE, CRYPT_IATTRIBUTE_LAST - 1 )
#define OUT_BOOL				_Deref_out_range_( FALSE, TRUE )
#define OUT_OPT_BOOL			_Deref_out_range_( FALSE, TRUE )
#define OUT_OPT_BYTE			_Deref_out_range_( 0, 0xFF )
#define OUT_ERROR				_Deref_out_range_( MAX_ERROR, -1 )
#define OUT_HANDLE_OPT			_Deref_out_range_( -1, MAX_OBJECTS - 1 )
#define OUT_OPT_HANDLE_OPT		_Deref_out_range_( -1, MAX_OBJECTS - 1 )
								/* There's no _opt_ version of this 
								   annotation, or for any of the _Deref_
								   variants for that matter, this is the 
								   best that we can do */
#define OUT_INDEX( max )		_Deref_out_range_( CRYPT_ERROR, ( max ) )
#define OUT_OPT_INDEX( max )	_Deref_out_range_( CRYPT_ERROR, ( max ) )
								/* See note for OUT_OPT_HANDLE_OPT */
#define OUT_PORT_Z				_Deref_out_range_( 0, 65535L )
#define OUT_RANGE( min, max )	_Deref_out_range_( ( min ), ( max ) )
#define OUT_OPT_RANGE( min, max ) _Deref_out_range_( ( min ), ( max ) )
#define OUT_STATUS				_Deref_out_range_( MAX_ERROR, CRYPT_OK )

/* Length parameter checking:

	LENGTH			Value must be between 1 and MAX_INTLENGTH - 1.
	LENGTH_FIXED	Value must be a single fixed length, used for polymorphic
					functions of which this specific instance takes a fixed-
					size parameter, or otherwise in cases where a length 
					parameter is supplied mostly just to allow bounds 
					checking.
	LENGTH_MIN		As LENGTH but lower bound is user-specified.
	LENGTH_Z		As LENGTH but lower bound may be zero.

	DATALENGTH		Value must be between 1 and MAX_BUFFER_SIZE - 1
	DATALENGTH_Z	As DATALENGTH but lower bound may be zero.

	LENGTH_SHORT	As LENGTH but upper bound is MAX_INTLENGTH_SHORT - 1 
					rather than MAX_INTLENGTH - 1.
	LENGTH_SHORT_MIN As LENGTH_SHORT but lower bound is user-specified.
	LENGTH_SHORT_Z	As LENGTH_SHORT but lower bound may be zero.

   In addition to these we allow the OPT specifier and OPT and Z suffixes as
   before */

#define IN_LENGTH				_In_range_( 1, MAX_INTLENGTH - 1 )
#define IN_LENGTH_OPT			_In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_FIXED( size )	_In_range_( ( size ), ( size ) )
#define IN_LENGTH_MIN( min )	_In_range_( ( min ), MAX_INTLENGTH - 1 )
#define IN_LENGTH_Z				_In_range_( 0, MAX_INTLENGTH - 1 )

#define IN_DATALENGTH			_In_range_( 1, MAX_BUFFER_SIZE - 1 )
#define IN_DATALENGTH_OPT		_In_range_( CRYPT_UNUSED, MAX_BUFFER_SIZE - 1 )
#define IN_DATALENGTH_MIN( min ) _In_range_( ( min ), MAX_BUFFER_SIZE - 1 )
#define IN_DATALENGTH_Z			_In_range_( 0, MAX_BUFFER_SIZE - 1 )

#define IN_LENGTH_SHORT			_In_range_( 1, MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_OPT		_In_range_( CRYPT_UNUSED, MAX_INTLENGTH_SHORT - 1 )
								/* This really is a _OPT and not a _INDEF in 
								   the one place where it's used */
#define IN_LENGTH_SHORT_MIN( min ) _In_range_( ( min ) , MAX_INTLENGTH_SHORT - 1 )
#define IN_LENGTH_SHORT_Z		_In_range_( 0, MAX_INTLENGTH_SHORT - 1 )

#define INOUT_LENGTH_Z			_Deref_inout_range_( 0, MAX_INTLENGTH - 1 )
#define INOUT_LENGTH_SHORT_Z	_Deref_inout_range_( 0, MAX_INTLENGTH_SHORT - 1 )

#define OUT_LENGTH				_Deref_out_range_( 1, MAX_INTLENGTH - 1 )
#define OUT_LENGTH_MIN( min )	_Deref_out_range_( ( min ), MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_MIN( min ) _Deref_out_range_( ( min ), MAX_INTLENGTH - 1 )
#define OUT_LENGTH_Z			_Deref_out_range_( 0, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_Z		_Deref_out_range_( 0, MAX_INTLENGTH - 1 )

#define OUT_DATALENGTH			_Deref_out_range_( 1, MAX_BUFFER_SIZE - 1 )
#define OUT_DATALENGTH_Z		_Deref_out_range_( 0, MAX_BUFFER_SIZE - 1 )

#define OUT_LENGTH_SHORT		_Deref_out_range_( 1, MAX_INTLENGTH_SHORT - 1 )
#define OUT_LENGTH_SHORT_Z		_Deref_out_range_( 0, MAX_INTLENGTH_SHORT - 1 )
#define OUT_OPT_LENGTH_SHORT_Z	_Deref_out_range_( 0, MAX_INTLENGTH_SHORT - 1 )

/* Sometimes a lengh value is given in terms of another length, typically 
   for the pattern { buffer, maxLength, *outLength } where *outLength is
   bounded by maxLength.  The following annotations deal with this
   bounded-length condition.

   Note that using this annotation doesn't work when 
   { buffer, maxLength, *outLength } can be given as { NULL, 0, *outLength }
   for performing a length check.  In this case we still use a pseudo-
   annotation that begins OUT_LENGTH_BOUNDED, but end it as a normal length
   annotation such as OUT_LENGTH_BOUNDED_PKC.  This functions the same as
   OUT_LENGTH_PKC, but may be modified in the future if SAL supports more
   complex conditional annotations.
   
   There's no _Deref_out_opt_range_ so we can't do the same for the
   OUT_OPT_LENGTH_xxx variants */

#define OUT_LENGTH_BOUNDED( length ) \
								_Deref_out_range_( 1, length )
#define OUT_LENGTH_BOUNDED_Z( length ) \
								_Deref_out_range_( 0, length )

#define OUT_LENGTH_BOUNDED_SHORT_Z( length ) \
								OUT_LENGTH_SHORT_Z
#define OUT_LENGTH_BOUNDED_PKC_Z( length ) \
								OUT_LENGTH_PKC_Z

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

   In addition to these we allow the OPT specifier and Z suffix as before.
   
   For the OUT_OPT variants there's no SAL 2.0 annotation that we can map
   this to so for now it's the same as a standard DEREF */

#define IN_LENGTH_ATTRIBUTE		_In_range_( 1, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_DNS			_In_range_( 1, MAX_DNS_SIZE )
#define IN_LENGTH_DNS_Z			_In_range_( 0, MAX_DNS_SIZE )
#define IN_LENGTH_ERRORMESSAGE	_In_range_( 1, MAX_ERRMSG_SIZE )
#define IN_LENGTH_HASH			_In_range_( 16, CRYPT_MAX_HASHSIZE )
#define IN_LENGTH_HASH_Z		_In_range_( 0, CRYPT_MAX_HASHSIZE )
#define IN_LENGTH_INDEF			_In_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define IN_LENGTH_IV			_In_range_( 1, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_IV_Z			_In_range_( 0, CRYPT_MAX_IVSIZE )
#define IN_LENGTH_KEY			_In_range_( MIN_KEYSIZE, CRYPT_MAX_KEYSIZE )
#define IN_LENGTH_KEYID			_In_range_( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_KEYID_Z		_In_range_( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME			_In_range_( MIN_NAME_LENGTH, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_NAME_Z		_In_range_( 0, MAX_ATTRIBUTE_SIZE )
#define IN_LENGTH_OID			_In_range_( 7, MAX_OID_SIZE )
#define IN_LENGTH_PKC			_In_range_( 1, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_PKC_BITS		_In_range_( 1, bytesToBits( CRYPT_MAX_PKCSIZE ) )
#define IN_LENGTH_PKC_Z			_In_range_( 0, CRYPT_MAX_PKCSIZE )
#define IN_LENGTH_TEXT			_In_range_( 1, CRYPT_MAX_TEXTSIZE )
#define IN_LENGTH_TEXT_Z		_In_range_( 0, CRYPT_MAX_TEXTSIZE )

#define OUT_LENGTH_HASH_Z		_Deref_out_range_( 0, CRYPT_MAX_HASHSIZE )
#define OUT_LENGTH_DNS_Z		_Deref_out_range_( 0, MAX_DNS_SIZE )
#define OUT_OPT_LENGTH_HASH_Z	_Deref_out_range_( 0, CRYPT_MAX_HASHSIZE )
#define OUT_LENGTH_PKC_Z		_Deref_out_range_( 0, CRYPT_MAX_PKCSIZE )
#define OUT_LENGTH_INDEF		_Deref_out_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_INDEF	_Deref_out_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )
#define OUT_OPT_LENGTH_SHORT_INDEF	_Deref_out_range_( CRYPT_UNUSED, MAX_INTLENGTH - 1 )

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

#define IN_TAG					_In_range_( DEFAULT_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_EXT				_In_range_( ANY_TAG, MAX_TAG_VALUE - 1 )
#define IN_TAG_ENCODED			_In_range_( NO_TAG, MAX_TAG - 1 )
#define IN_TAG_ENCODED_EXT		_In_range_( ANY_TAG, MAX_TAG - 1 )

#define OUT_TAG_Z				_Deref_out_range_( 0, MAX_TAG_VALUE - 1 )
#define OUT_TAG_ENCODED_Z		_Deref_out_range_( 0, MAX_TAG - 1 )

/* Enumerated type checking.  Due to the duality of 'int' and 'enum' under C
   these can normally be mixed freely until it comes back to bite on systems
   where the compiler doesn't treat them quite the same.  The following 
   provides more strict type checking of enums than the compiler would 
   normally provide and relies on the enum being bracketed by xxx_NONE and
   xxx_LAST.

   We also allow an OPT suffix to indicate the use of don't-care values, 
   denoted by xxx_NONE */

#define IN_ENUM( name )			_In_range_( name##_NONE + 1, name##_LAST - 1 )
#define IN_ENUM_OPT( name )		_In_range_( name##_NONE, name##_LAST - 1 )

#define INOUT_ENUM( name )		_Deref_inout_range_( name##_NONE + 1, name##_LAST - 1 )
#define INOUT_ENUM_OPT( name )	_Deref_inout_range_( name##_NONE, name##_LAST - 1 )

#define OUT_ENUM( name )		_Deref_out_range_( name##_NONE + 1, name##_LAST - 1 )
#define OUT_ENUM_OPT( name )	_Deref_out_range_( name##_NONE, name##_LAST - 1 )
#define OUT_OPT_ENUM( name )	_Deref_opt_out_range_( name##_NONE + 1, name##_LAST - 1 )

/* Binary flag checking.  This works as for enumerated type checking and 
   relies on the flag definition being bracketed by xxx_FLAG_NONE and 
   xxx_FLAG_MAX */

#define IN_FLAGS( name )		_In_range_( 1, name##_FLAG_MAX )
#define IN_FLAGS_Z( name )		_In_range_( name##_FLAG_NONE, name##_FLAG_MAX )

#define INOUT_FLAGS( name )		_Deref_inout_range_( name##_FLAG_NONE, name##_FLAG_MAX )

#define OUT_FLAGS_Z( name )		_Deref_out_range_( name##_FLAG_NONE, name##_FLAG_MAX )

/* Buffer parameter checking:

	IN_BUFFER		For { buffer, length } values.
	INOUT_BUFFER	For in-place processing of { buffer, maxLength, *length } 
					values, e.g. decryption with padding removal.
	OUT_BUFFER		For filling of { buffer, maxLength, *length } values.

	INOUT_BUFFER_FIXED For in-place processing of { buffer, length } values, 
					e.g. filtering control chars in a string.
	OUT_BUFFER_FIXED For filling of { buffer, length } values.

   In addition to these we allow the OPT specifier as before */

#define IN_BUFFER( count )				_In_reads_bytes_( count )
#define IN_BUFFER_C( count )			_In_reads_bytes_( count )
#define IN_BUFFER_OPT( count )			_In_reads_bytes_opt_( count )
#define IN_BUFFER_OPT_C( count )		_In_reads_bytes_opt_( count )

#define INOUT_BUFFER( max, count )		_Inout_updates_bytes_to_( max, count )
#define INOUT_BUFFER_C( max, count )	_Inout_updates_bytes_to_( max, count )
#define INOUT_BUFFER_FIXED( count )		_Inout_updates_bytes_all_( count )
#define INOUT_BUFFER_OPT( max, count )	_Inout_updates_bytes_to_opt_( max, count )

#define OUT_BUFFER( max, count )		_Out_writes_bytes_to_( max, count ) 
#define OUT_BUFFER_C( max, count )		_Out_writes_bytes_to_( max, count ) 
#define OUT_BUFFER_OPT( max, count )	_Out_writes_bytes_to_opt_( max, count ) 
#define OUT_BUFFER_OPT_C( max, count )	_Out_writes_bytes_to_opt_( max, count ) 
#define OUT_BUFFER_FIXED( count )		_Out_writes_bytes_all_( count )
#define OUT_BUFFER_FIXED_C( count )		_Out_writes_bytes_all_( count )
#define OUT_BUFFER_OPT_FIXED( count )	_Out_writes_bytes_all_opt_( count )

/* A special-case annotation for a situation in which a block of memory is
   passed around as a working buffer to save allocating local buffers up
   and down a call hierarchy.  This is a bit of an awkward situation since
   it isn't really an IN or OUT parameter, and doesn't have to be 
   initialised by either the caller or callee.  Its only function is that
   the callee pollutes it, but not in any annotation-relevant manner.

   To deal with this, we make it an OUT parameter, making it callee-
   initialised seems to be the closest to the required semantics */

#define WORKING_BUFFER					OUT_BUFFER_FIXED

/* Array parameter checking:

	ARRAY			For { fooType foo[], int fooCount } values */

#define IN_ARRAY( count )		_In_reads_( count )
#define IN_ARRAY_C( count )		_In_reads_( count )
#define IN_ARRAY_OPT( count )	_In_reads_opt_( count )
#define IN_ARRAY_OPT_C( count )	_In_reads_opt_( count )

#define INOUT_ARRAY( count )	_Inout_updates_( count )
#define INOUT_ARRAY_C( count )	_Inout_updates_( count )

#define OUT_ARRAY( count )		_Out_writes_( count )
#define OUT_ARRAY_C( count )	_Out_writes_( count )
#define OUT_ARRAY_OPT( count )	_Out_writes_opt_( count )
#define OUT_ARRAY_OPT_C( count ) _Out_writes_opt_( count )

/* Conditional modifiers on parameters.  These are used when one parameter's
   behaviour depends on another, for example:

	IN_WHEN( condition == STATE_WRITE ) \
	OUT_WHEN( condition == STATE_READ ) \
	INOUT_WHEN( condition == STATE_READWRITE )

  specifies the behaviour of a buffer value when an associated operation 
  code is read/write/readwrite */

#define OUT_WHEN( cond )	_When_( ( cond ), _Out_ )
#define INOUT_WHEN( cond )	_When_( ( cond ), _Inout_ )

/* Parameter checking that goes beyond the basic operations provided above.  
   This uses somewhat complex conditionals on annotations to handle 
   situations where one parameter controls the behaviour of one or more
   others and that can't be handled more directly using the IN_WHEN/OUT_WHEN
   operations.  Because of the complexity of these annotations, we provide 
   several custom forms that are used in specific situations:

	PARAMCHECK			General-purpose, specifies the condition, the 
						parameter that's affected, and the behaviour of the 
						parameter, e.g. { operation == READ, buffer, OUT }
						(although something that simple would be handled with
						IN_WHEN/OUT_WHEN/etc). 

	PARAMCHECK_MESSAGE	For krnlSendMessage(), specifies the message type 
						and the behaviour of the pointer and integer 
						parameters passed to the call */

#define PARAM_NULL				_Pre_null_
#define PARAM_IS( value )		_Pre_equal_to_( value )

#define PARAMCHECK( condition, param, type ) \
								_When_( ( condition ), _At_( param, type ) )

#define PARAMCHECK_MESSAGE( msgType, msgDataType, msgValType ) \
								_When_( ( message & MESSAGE_MASK ) == ( msgType ), \
										_At_( messageDataPtr, msgDataType ) ) \
								_When_( ( message & MESSAGE_MASK ) == ( msgType ), \
										_At_( messageValue, msgValType ) )

/* Structures that encapsulate data-handling operations:

	VALUE			Integer value constrained to a subrange, e.g. 0...10.
	ARRAY			Array of total allocated size 'max', currently filled
					to 'count' elements.
	BUFFER			Buffer of total allocated size 'max', currently filled 
					to 'count' bytes.
	BUFFER_FIXED	Buffer of total allocated and filled size 'max'.

   In addition to these we allow the OPT specifier as before, and the 
   UNSPECIFIED specifier to indicate that the fill state of the buffer 
   isn't specified or at least is too complex to describe to PREfast, for 
   example an I/O buffer that acts as BUFFER on read but BUFFER_FIXED on 
   write.
   
   The VALUE_xxx annotations don't appear to be implemented in current 
   versions of PREfast.  For example the annotation of the handle ranges for 
   MESSAGE_CREATEOBJECT_INFO in cryptkrn.h has no effect, producing a 
   warning that the handle value is potentially out of range every time that 
   it's used.  At the moment this is kludged with an ANALYSER_HINT() for
   the setMessageCreateObjectInfo() macro, which silences the hundreds of
   warnings that would otherwise be produced */

#define VALUE( min, max )		_Field_range_( ( min ), ( max ) )
#define VALUE_HANDLE			_Field_range_( SYSTEM_OBJECT_HANDLE, MAX_OBJECTS - 1 )
#define VALUE_INT				_Field_range_( 0, MAX_INTLENGTH ) 
#define VALUE_INT_SHORT			_Field_range_( 0, MAX_INTLENGTH_SHORT ) 
#define ARRAY( max, count )		_Field_size_part_( ( max ), ( count ) )
#define ARRAY_FIXED( max )		_Field_size_( max )
#define BUFFER( max, count )	_Field_size_bytes_part_( ( max ), ( count ) )
#define BUFFER_OPT( max, count ) _Field_size_bytes_part_opt_( ( max ), ( count ) )
#define BUFFER_FIXED( max )		_Field_size_bytes_( max )
#define BUFFER_OPT_FIXED( max )	_Field_size_bytes_opt_( max )
#define BUFFER_UNSPECIFIED( max ) _Field_size_bytes_( max )

/* Memory-allocation functions that allocate and return a block of 
   initialised memory:

	ALLOC			Pointer can't be NULL, *pointer set to NULL on failure.
	ALLOC_OPT		Pointer may be NULL, *pointer set to NULL on failure  */

#define OUT_BUFFER_ALLOC( length )		_Outptr_result_nullonfailure_ \
										_Outptr_result_bytebuffer_( length )
#define OUT_BUFFER_ALLOC_OPT( length )	_Outptr_opt_result_nullonfailure_ \
										_Outptr_opt_result_bytebuffer_( length )

/* Typeless annotations used in situations where the size or type is 
   implicit, e.g. a pointer to a structure.  The annotation 'IN' is 
   implied by 'const' so we wouldn't normally use it, but it can be useful 
   when we're re-prototyping a non-cryptlib function that doesn't use 
   'const's,
   
   OUT_ALWAYS is a special-case annotation where we always set the annotated
   parameter but also return a status value to indicate additional
   information.  Examples of this occur in some of the error-handling 
   functions, which set ERROR_INFO structures but also return an error
   status that represents value to pass back to the caller rather than the
   success status of the error handler  */

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
#define OUT_ALWAYS				_Always_( _Out_ )

/* Pointer annotations for parameter **result:

	OUT_PTR				result non-NULL, *result non-NULL.
	OUT_PTR_COND		result non-NULL, *result non-NULL on success.
	OUT_PTR_xCOND		result non-NULL, *result non-NULL on failure.
						Used for functions that return pointer to error info.
	OUT_PTR_OPT			result non-NULL, *result may be NULL or non-NULL.
						Used for caller-cleanup functions (create 
						cert/ctx/key/etc object), or for functions that 
						return a pointer to a mismatch point 
						(findRevocationEntry()).
	OUT_OPT_PTR			result may be NULL, *result non-NULL.
	OUT_OPT_PTR_COND	result may be NULL, *result non-NULL on success */

#define _Outptr_result_nonnullonfailure_ \
								_SAL2_Source_(_Outptr_result_nonnullonfailure_, (), _Outptr_ _On_failure_( _Deref_post_ ) )
#define _Outptr_opt_result_nonnullonfailure_ \
								_SAL2_Source_(_Outptr_opt_result_nonnullonfailure_, (), _Outptr_opt_ _On_failure_( _Deref_post_ ) )

#define INOUT_PTR				_Inout_
								/* There's no 
								   _Inoutptr_result_nullonfailure_, the 
								   _Inout_ seems to work though */
#define OUT_PTR					_Outptr_
#define OUT_PTR_COND			_Outptr_result_nullonfailure_
#define OUT_PTR_xCOND			_Outptr_result_nonnullonfailure_
#define OUT_PTR_OPT				_Outptr_result_maybenull_
#define OUT_OPT_PTR				_Outptr_opt_
#define OUT_OPT_PTR_COND		_Outptr_opt_result_nullonfailure_
#define OUT_OPT_PTR_xCOND		_Outptr_opt_result_nonnullonfailure_

/* Other annotations:

	FORMAT_STRING	Argument is a printf-style format string.
	IN_STRING		Argument is a null-terminated string.
	TYPECAST		Type cast, e.g. from void * to struct foo * */

#define ANALYSER_HINT( expr )	__analysis_assume( expr )
#define ANALYSER_HINT_V( expr )	__analysis_assume( expr )
								/* Attribute SAL has no equivalent for this, 
								   but __analysis_assume() still works */
#define FORMAT_STRING			_Printf_format_string_
#define IN_STRING				_In_z_
#define IN_STRING_OPT			_In_opt_z_
#define TYPECAST( type )		/* No equivalent in attribute SAL */

#endif /* PREfast */

/* Handling of C'0x analysis */

#if defined( _MSC_VER ) && !VC_16BIT( _MSC_VER )

#define STDC_NONNULL_ARG( argIndex )
#define STDC_PRINTF_FN( formatIndex, argIndex )
#define STDC_PURE		__declspec( noalias )
#if defined( _MSC_VER ) && defined( _PREFAST_ ) 
  #define STDC_UNUSED	_Reserved_
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

   (The NULL-pointer problem may be controllable through 
   -fno-delete-null-pointer-checks, but this is actually present in order to 
   address a different issue, that code like 
   'b = a->b; if( a == NULL ) return -1', which will segfault on most 
   architectures but not ones that don't have memory protection (e.g. ARM7)
   will never get to the NULL check so it can be deleted (as above, gcc
   generates code that it knows will segfault instead of warning that 
   there's a problem).  Since -fno-delete-null-pointer-checks doesn't
   necessarily apply to STDC_NONNULL_ARG, it's not safe to rely on it, and
   in any case we want the compiler to warn of erroneous use, not knowingly
   generate code that segfaults when it's encountered).

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
   can't have helped either).
   
   Another case is the totally braindamaged behaviour of -Wshadow, which 
   took a complaint from no less then Linus Torvalds to get fixed (see 
   "[PATCH] Don't compare unsigned variable for <0 in sys_prctl()", 
   http://lkml.org/lkml/2006/11/28/239, and even then it took them six
   years to fix the problem) */

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
#define CHECK_RETVAL_LENGTH_SHORT		CHECK_RETVAL
#define CHECK_RETVAL_LENGTH_SHORT_NOERROR CHECK_RETVAL
#define CHECK_RETVAL_PTR				CHECK_RETVAL
#define CHECK_RETVAL_PTR_NONNULL		CHECK_RETVAL
#define CHECK_RETVAL_RANGE( low, high )	CHECK_RETVAL
#define CHECK_RETVAL_RANGE_NOERROR( low, high ) CHECK_RETVAL
#define CHECK_RETVAL_SPECIAL			CHECK_RETVAL
#define CHECK_RETVAL_STRINGOP			CHECK_RETVAL

#define CHECK_RETVAL_ERROR				CHECK_RETVAL

#define CHECK_RETVAL_ACQUIRELOCK( lockName ) \
										CHECK_RETVAL

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
#define ANALYSER_HINT_V( expr )	ENSURES_V( expr )

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
*							Coverity Analysis Support 						*
*																			*
****************************************************************************/

#ifdef __COVERITY__

/* Coverity's annotations differ from everyone else's in that they're not
   attributes applied to variables but explicit macros, so instead of:

	foo( IN_STRING char *string )

   they're given as:

	foo( char *string )
		{
		ANALYSER_HINT_STRING( string );

   The annotations are documented in Help | Check Reference | Models and 
   annotations | Primitives for custom models, the ones that we use are:

	ANALYSER_HINT_FORMAT_STRING: The string is a printf-style format string.

	ANALYSER_HINT_STRING: The string must be a null-terminated string.

	ANALYSER_HINT_RECURSIVE_LOCK/UNLOCK: The lock is recursively locked/
		unlocked.  This is required when cryptlib synthesises recursive 
		mutexes from Posix non-recursive ones */

#define ANALYSER_HINT_FORMAT_STRING( string )	__coverity_format_string_sink__( string );
#define ANALYSER_HINT_STRING( string )			__coverity_string_null_sink__( string );
#define ANALYSER_HINT_RECURSIVE_LOCK( lock )	__coverity_recursive_lock_acquire__( lock );
#define ANALYSER_HINT_RECURSIVE_UNLOCK( lock )	__coverity_recursive_lock_release__( lock );

#endif /* Coverity */

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
#define CHECK_RETVAL_LENGTH_SHORT
#define CHECK_RETVAL_LENGTH_SHORT_NOERROR
#define CHECK_RETVAL_PTR
#define CHECK_RETVAL_PTR_NONNULL
#define CHECK_RETVAL_RANGE( low, high )
#define CHECK_RETVAL_RANGE_NOERROR( low, high )
#define CHECK_RETVAL_SPECIAL
#define CHECK_RETVAL_STRINGOP

#define CHECK_RETVAL_ERROR

#define CHECK_RETVAL_ACQUIRELOCK( lockName )

#endif /* No basic analysis support */

#ifndef RETVAL

#define RETVAL
#define RETVAL_BOOL
#define RETVAL_LENGTH_NOERROR
#define RETVAL_RANGE( low, high )
#define RETVAL_RANGE_NOERROR( low, high )

#define RETVAL_RELEASELOCK( lockName )
#define RELEASELOCK( lockName )

#define IN_INT
#define IN_INT_OPT
#define IN_INT_Z
#define IN_INT_SHORT
#define IN_INT_SHORT_Z
#define OUT_INT_Z
#define OUT_INT_SHORT_Z
#define OUT_OPT_INT_Z
#define INOUT_INT_Z

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
#define OUT_INDEX( max )
#define OUT_OPT_INDEX( max )
#define OUT_PORT_Z
#define OUT_RANGE( min, max )
#define OUT_OPT_RANGE( min, max )
#define OUT_STATUS

#define IN_LENGTH
#define IN_LENGTH_OPT
#define IN_LENGTH_FIXED( size )
#define IN_LENGTH_MIN( min )
#define IN_LENGTH_Z
#define IN_DATALENGTH
#define IN_DATALENGTH_OPT
#define IN_DATALENGTH_MIN( min )
#define IN_DATALENGTH_Z
#define IN_LENGTH_SHORT
#define IN_LENGTH_SHORT_MIN( min )
#define IN_LENGTH_SHORT_OPT
#define IN_LENGTH_SHORT_Z
#define INOUT_LENGTH_Z
#define INOUT_LENGTH_SHORT_Z
#define OUT_LENGTH
#define OUT_LENGTH_Z
#define OUT_OPT_LENGTH_Z
#define OUT_DATALENGTH
#define OUT_DATALENGTH_Z
#define OUT_LENGTH_SHORT
#define OUT_OPT_LENGTH_SHORT_Z
#define OUT_LENGTH_SHORT_Z

#define OUT_LENGTH_BOUNDED( length )
#define OUT_LENGTH_BOUNDED_Z( length )
#define OUT_LENGTH_BOUNDED_SHORT_Z( length )
#define OUT_LENGTH_BOUNDED_PKC_Z( length )

#define IN_LENGTH_ATTRIBUTE
#define IN_LENGTH_DNS
#define IN_LENGTH_DNS_Z
#define IN_LENGTH_ERRORMESSAGE
#define IN_LENGTH_HASH
#define IN_LENGTH_HASH_Z
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
#define IN_LENGTH_PKC_BITS
#define IN_LENGTH_PKC_Z
#define IN_LENGTH_TEXT
#define IN_LENGTH_TEXT_Z
#define OUT_LENGTH_HASH_Z
#define OUT_LENGTH_DNS_Z
#define OUT_OPT_LENGTH_HASH_Z
#define OUT_LENGTH_PKC_Z
#define OUT_LENGTH_INDEF
#define OUT_OPT_LENGTH_INDEF
#define OUT_OPT_LENGTH_SHORT_INDEF

#define IN_TAG
#define IN_TAG_EXT
#define IN_TAG_ENCODED
#define IN_TAG_ENCODED_EXT
#define OUT_TAG_Z
#define OUT_TAG_ENCODED_Z

#define IN_ENUM( name )
#define IN_ENUM_OPT( name )
#define INOUT_ENUM( name )
#define INOUT_ENUM_OPT( name )
#define OUT_ENUM( name )
#define OUT_ENUM_OPT( name )
#define OUT_OPT_ENUM( name )

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
#define WORKING_BUFFER( max )

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

#define OUT_WHEN( cond )
#define INOUT_WHEN( cond )

#define PARAM_NULL
#define PARAM_IS( value )
#define PARAMCHECK( condition, param, type )
#define PARAMCHECK_MESSAGE( msgType, msgDataType, msgValType )

#define VALUE( min, max )
#define VALUE_HANDLE
#define VALUE_INT
#define VALUE_INT_SHORT
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
#define OUT_ALWAYS

#define INOUT_PTR
#define OUT_PTR
#define OUT_PTR_COND
#define OUT_PTR_xCOND
#define OUT_PTR_OPT
#define OUT_OPT_PTR
#define OUT_OPT_PTR_COND
#define OUT_OPT_PTR_xCOND

#ifndef ANALYSER_HINT
  #define ANALYSER_HINT( expr )
  #define ANALYSER_HINT_V( expr )
#endif /* ANALYSER_HINT */
#define FORMAT_STRING
#define IN_STRING
#define IN_STRING_OPT
#define TYPECAST( ctype )
#ifndef ANALYSER_HINT_FORMAT_STRING
  #define ANALYSER_HINT_FORMAT_STRING( string )
  #define ANALYSER_HINT_STRING( string )
  #define ANALYSER_HINT_RECURSIVE_LOCK( lock )
  #define ANALYSER_HINT_RECURSIVE_UNLOCK( lock )
#endif /* ANALYSER_HINT_FORMAT_STRING */
#endif /* No extended analysis support */

#ifndef STDC_NONNULL_ARG

#define STDC_NONNULL_ARG( argIndex )
#define STDC_PRINTF_FN( formatIndex, argIndex )
#define STDC_PURE 
#define STDC_UNUSED

#endif /* No C'0x attribute support */

#endif /* _ANALYSE_DEFINED */
