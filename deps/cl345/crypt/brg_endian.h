/* Replacement for the original brg_endian.h, used because cryptlib contains 
   its own endianness-management mechanisms that replace the ones in that 
   file */

#ifndef _BRG_ENDIAN_H
#define _BRG_ENDIAN_H

#include "crypt.h"

#define IS_BIG_ENDIAN		4321
#define IS_LITTLE_ENDIAN	1234

#ifdef DATA_LITTLEENDIAN
  #define PLATFORM_BYTE_ORDER	IS_LITTLE_ENDIAN
#else
  #define PLATFORM_BYTE_ORDER	IS_BIG_ENDIAN
#endif

#endif /* _BRG_ENDIAN_H */
