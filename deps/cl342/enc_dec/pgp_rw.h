/****************************************************************************
*																			*
*				Miscellaneous (Non-ASN.1) Routines Header File				*
*					  Copyright Peter Gutmann 1992-2004						*
*																			*
****************************************************************************/

#ifndef _PGPRW_DEFINED

#define _PGPRW_DEFINED

#include <time.h>
#if defined( INC_ALL )
  #include "misc_rw.h"
  #include "stream.h"
  #include "pgp.h"
#else
  #include "enc_dec/misc_rw.h"
  #include "io/stream.h"
  #include "misc/pgp.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*								Function Prototypes							*
*																			*
****************************************************************************/

/* Read/write PGP length values */

#define pgpSizeofLength( length ) \
		( ( length < 0 ) ? length : \
		  ( length <= 191 ) ? 1 : \
		  ( length <= 8383 ) ? 2 : 4 )
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pgpReadShortLength( INOUT STREAM *stream, 
						OUT_LENGTH_SHORT_Z int *length, 
						IN_BYTE const int ctb );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1, 2 ) ) \
int pgpReadPartialLength( INOUT STREAM *stream, 
						  OUT_LENGTH_Z long *length );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWriteLength( INOUT STREAM *stream, IN_LENGTH const long length );

/* Read/write PGP packet headers.  The difference between 
   pgpReadPacketHeader() and pgpReadPacketHeaderI() is that the latter 
   allows indefinite-length encoding for partial lengths.  Once we've
   read an indefinite length, we have to use pgpReadPartialLengh() to
   read subsequence partial-length values */

CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeader( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						 OUT_OPT_LENGTH_Z long *length, 
						 IN_LENGTH_SHORT const int minLength,
						 IN_LENGTH const long maxLength );
CHECK_RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpReadPacketHeaderI( INOUT STREAM *stream, OUT_OPT_BYTE int *ctb, 
						  OUT_OPT_LENGTH_Z long *length, 
						  IN_LENGTH_SHORT const int minLength );
RETVAL STDC_NONNULL_ARG( ( 1 ) ) \
int pgpWritePacketHeader( INOUT STREAM *stream, 
						  IN_ENUM( PGP_PACKET ) \
							const PGP_PACKET_TYPE packetType,
						  IN_LENGTH const long length );

#endif /* _PGPRW_DEFINED */
