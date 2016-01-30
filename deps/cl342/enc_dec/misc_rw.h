/****************************************************************************
*																			*
*				Miscellaneous (Non-ASN.1) Routines Header File				*
*					  Copyright Peter Gutmann 1992-2004						*
*																			*
****************************************************************************/

#ifndef _MISCRW_DEFINED

#define _MISCRW_DEFINED

#include <time.h>
#if defined( INC_ALL )
  #include "stream.h"
#else
  #include "io/stream.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Constants and Macros						*
*																			*
****************************************************************************/

/* Sizes of encoded integer values */

#define UINT16_SIZE		2
#define UINT32_SIZE		4
#define UINT64_SIZE		8

/****************************************************************************
*																			*
*								Function Prototypes							*
*																			*
****************************************************************************/

/* Read and write 16-, 32-, and 64-bit integer values */

RETVAL_RANGE( MAX_ERROR, 0xFFFF ) STDC_NONNULL_ARG( ( 1 ) ) \
int readUint16( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint16( INOUT STREAM *stream, IN_RANGE( 0, 0xFFFF ) const int value );
RETVAL_RANGE( MAX_ERROR, MAX_INTLENGTH ) STDC_NONNULL_ARG( ( 1 ) ) \
int readUint32( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint32( INOUT STREAM *stream, IN_INT_Z const long value );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUint64( INOUT STREAM *stream, OUT_INT_Z long *value );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint64( INOUT STREAM *stream, IN_INT_Z const long value );

/* Read and write 32- and 64-bit time values */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUint32Time( INOUT STREAM *stream, OUT time_t *timeVal );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint32Time( INOUT STREAM *stream, const time_t timeVal );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readUint64Time( INOUT STREAM *stream, OUT time_t *timeVal );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeUint64Time( INOUT STREAM *stream, const time_t timeVal );

/* Read and write strings preceded by 32-bit lengths */

#define sizeofString32( string, stringLength ) \
		( ( stringLength > 0 ) ? ( UINT32_SIZE + stringLength ) : \
								 ( UINT32_SIZE + strlen( string ) ) )

RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readString32( INOUT STREAM *stream, 
				  OUT_BUFFER( stringMaxLength, *stringLength ) \
				  void *string, IN_LENGTH_SHORT const int stringMaxLength, 
				  OUT_LENGTH_SHORT_Z int *stringLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeString32( INOUT STREAM *stream, 
				   IN_BUFFER( stringLength ) \
				   const void *string, 
				   IN_LENGTH_SHORT const int stringLength );

/* Read a raw object preceded by a 32-bit length */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
int readRawObject32( INOUT STREAM *stream, 
					 OUT_BUFFER( bufferMaxLength, *bufferLength ) \
					 void *buffer, 
					 IN_LENGTH_SHORT_MIN( UINT32_SIZE + 1 ) \
					 const int bufferMaxLength, 
					 OUT_LENGTH_SHORT_Z int *bufferLength );

/* Read a universal type and discard it (used to skip unknown or unwanted
   types) */

RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal16( INOUT STREAM *stream );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int readUniversal32( INOUT STREAM *stream );

/* Read and write unsigned (large) integers preceded by 16- and 32-bit
   lengths, lengths in bits */

#define sizeofInteger16U( integerLength )	( UINT16_SIZE + integerLength )
#define sizeofInteger32( integer, integerLength ) \
		( UINT32_SIZE + ( ( ( ( BYTE * ) integer )[ 0 ] & 0x80 ) ? 1 : 0 ) + \
						integerLength )

RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16U( INOUT STREAM *stream, 
					OUT_BUFFER_OPT( maxLength, *integerLength ) \
					void *integer, OUT_LENGTH_PKC_Z int *integerLength, 
					IN_LENGTH_PKC const int minLength, 
					IN_LENGTH_PKC const int maxLength );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16Ubits( INOUT STREAM *stream, 
						OUT_BUFFER_OPT( maxLength, *integerLength ) \
						void *integer, OUT_LENGTH_PKC_Z int *integerLength, 
						IN_LENGTH_PKC const int minLength, 
						IN_LENGTH_PKC const int maxLength );
RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger32( INOUT STREAM *stream, 
				   OUT_BUFFER_OPT( maxLength, *integerLength ) \
				   void *integer, OUT_LENGTH_PKC_Z int *integerLength, 
				   IN_LENGTH_PKC const int minLength, 
				   IN_LENGTH_PKC const int maxLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger16U( INOUT STREAM *stream, 
					 IN_BUFFER( integerLength ) \
					 const void *integer, 
					 IN_LENGTH_PKC const int integerLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger16Ubits( INOUT STREAM *stream, 
						 IN_BUFFER( integerLength ) \
						 const void *integer, 
						 IN_LENGTH_PKC const int integerLength );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeInteger32( INOUT STREAM *stream, 
					IN_BUFFER( integerLength ) \
					const void *integer, 
					IN_LENGTH_PKC const int integerLength );

/* Special-case large integer read routines that explicitly check for a too-
   short key and return CRYPT_ERROR_NOSECURE rather than the 
   CRYPT_ERROR_BADDATA that'd otherwise be returned */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger16UChecked( INOUT STREAM *stream, 
						   OUT_BUFFER_OPT( maxLength, *integerLength ) \
						   void *integer, OUT_LENGTH_PKC_Z int *integerLength, 
						   IN_LENGTH_PKC const int minLength, 
						   IN_LENGTH_PKC const int maxLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 3 ) ) \
int readInteger32Checked( INOUT STREAM *stream, 
						  OUT_BUFFER_OPT( maxLength, *integerLength ) \
						  void *integer, OUT_LENGTH_PKC_Z int *integerLength, 
						  IN_LENGTH_PKC const int minLength, 
						  IN_LENGTH_PKC const int maxLength );

#ifdef USE_PKC

/* Read and write bignum integers */

RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16U( INOUT STREAM *stream, 
						  INOUT TYPECAST( BIGNUM ) void *bignum, 
						  IN_LENGTH_PKC const int minLength, 
						  IN_LENGTH_PKC const int maxLength, 
						  IN_OPT TYPECAST( BIGNUM ) const void *maxRange );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumInteger16U( INOUT STREAM *stream, 
						   TYPECAST( BIGNUM ) const void *bignum );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16Ubits( INOUT STREAM *stream, 
							  INOUT TYPECAST( BIGNUM ) void *bignum, 
							  IN_LENGTH_PKC const int minBits, 
							  IN_LENGTH_PKC const int maxBits, 
							  IN_OPT TYPECAST( BIGNUM ) const void *maxRange );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int writeBignumInteger16Ubits( INOUT STREAM *stream, 
							   TYPECAST( BIGNUM ) const void *bignum );
CHECK_RETVAL_RANGE( MAX_ERROR, MAX_INTLENGTH_SHORT ) STDC_NONNULL_ARG( ( 1 ) ) \
int sizeofBignumInteger32( const void *bignum );
RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger32( INOUT STREAM *stream, 
						 INOUT TYPECAST( BIGNUM ) void *bignum, 
						 IN_LENGTH_PKC const int minLength, 
						 IN_LENGTH_PKC const int maxLength, 
						 IN_OPT TYPECAST( BIGNUM ) const void *maxRange );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int writeBignumInteger32( INOUT STREAM *stream, 
						  TYPECAST( BIGNUM ) const void *bignum );

/* Special-case bignum read routines that explicitly check for a too-short 
   key and return CRYPT_ERROR_NOSECURE rather than the CRYPT_ERROR_BADDATA
   that'd otherwise be returned */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16UChecked( INOUT STREAM *stream, 
								 INOUT TYPECAST( BIGNUM * ) void *bignum, 
								 IN_LENGTH_PKC const int minLength, 
								 IN_LENGTH_PKC const int maxLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger16UbitsChecked( INOUT STREAM *stream, 
									 INOUT TYPECAST( BIGNUM * ) void *bignum, 
									 IN_LENGTH_PKC const int minBits, 
									 IN_LENGTH_PKC const int maxBits );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int readBignumInteger32Checked( INOUT STREAM *stream, 
								INOUT TYPECAST( BIGNUM * ) void *bignum, 
								IN_LENGTH_PKC const int minLength, 
								IN_LENGTH_PKC const int maxLength );
#endif /* USE_PKC */

#ifdef _PGP_DEFINED

/* PGP-specific read/write routines.  The difference between
   pgpReadPacketHeader() and pgpReadPacketHeaderI() is that the latter
   allows indefinite-length encoding for partial lengths.  Once we've
   read an indefinite length, we have to use pgpReadPartialLengh() to
   read subsequence partial-length values */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadShortLength( INOUT STREAM *stream, OUT_LENGTH int *length, 
						IN_BYTE const int ctb );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWriteLength( INOUT STREAM *stream, IN_LENGTH const long length );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeader( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						 OUT_OPT_LENGTH_Z long *length, 
						 IN_LENGTH_SHORT const int minLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeaderI( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						  OUT_OPT_LENGTH_Z long *length, 
						  IN_LENGTH_SHORT const int minLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPartialLength( INOUT STREAM *stream, 
						  OUT_OPT_LENGTH_Z long *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWritePacketHeader( INOUT STREAM *stream, 
						  IN_ENUM( PGP_PACKET ) \
						  const PGP_PACKET_TYPE packetType,
						  IN_LENGTH const long length );
#define pgpSizeofLength( length ) \
		( ( length < 0 ) ? length : \
		  ( length <= 191 ) ? 1 : \
		  ( length <= 8383 ) ? 2 : 4 )
#endif /* _PGP_DEFINED */

#endif /* !_MISCRW_DEFINED */
