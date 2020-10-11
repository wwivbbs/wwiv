/*
 ---------------------------------------------------------------------------
 Copyright (c) 2002, Dr Brian Gladman, Worcester, UK.   All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 01/08/2005
*/

#ifndef _SHA2_H
#define _SHA2_H

#include <stdlib.h>

#include "crypt.h"	/* For USE_SHA2_EXT define via config.h */
#ifdef USE_SHA2_EXT	/* pcg */
  #define SHA_64BIT
#endif /* USE_SHA2_EXT */

/* define the hash functions that you need  */
#define SHA_2   /* for dynamic hash length  */
#define SHA_224
#define SHA_256
#ifdef SHA_64BIT
#  define SHA_384
#  define SHA_512
#  define NEED_UINT_64T
#endif

#if defined( INC_ALL )
  #include "brg_types.h"
#else
  #include "crypt/brg_types.h"
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

/* Note that the following function prototypes are the same */
/* for both the bit and byte oriented implementations.  But */
/* the length fields are in bytes or bits as is appropriate */
/* for the version used.  Bit sequences are arrays of bytes */
/* in which bit sequence indexes increase from the most to  */
/* the least significant end of each byte                   */

#define SHA224_DIGEST_SIZE  28
#define SHA224_BLOCK_SIZE   64
#define SHA256_DIGEST_SIZE  32
#define SHA256_BLOCK_SIZE   64

/* type to hold the SHA256 (and SHA224) context */

typedef struct
{   uint32_t count[2];
    uint32_t hash[8];
    uint32_t wbuf[16];
} sha256_ctx;

typedef sha256_ctx  sha224_ctx;

VOID_RETURN sha256_compile(sha256_ctx ctx[1]);

VOID_RETURN sha224_begin(sha224_ctx ctx[1]);
#define sha224_hash sha256_hash
VOID_RETURN sha224_end(unsigned char hval[], sha224_ctx ctx[1]);
VOID_RETURN sha224(unsigned char hval[], const unsigned char data[], unsigned long len);

VOID_RETURN sha256_begin(sha256_ctx ctx[1]);
VOID_RETURN sha256_hash(const unsigned char data[], unsigned long len, sha256_ctx ctx[1]);
VOID_RETURN sha256_end(unsigned char hval[], sha256_ctx ctx[1]);
VOID_RETURN sha256(unsigned char hval[], const unsigned char data[], unsigned long len);

#ifndef SHA_64BIT

typedef struct
{   union
    { sha256_ctx  ctx256[1];
    } uu[1];
    uint32_t    sha2_len;
} sha2_ctx;

#define SHA2_MAX_DIGEST_SIZE    SHA256_DIGEST_SIZE

#else

#define SHA384_DIGEST_SIZE  48
#define SHA384_BLOCK_SIZE  128
#define SHA512_DIGEST_SIZE  64
#define SHA512_BLOCK_SIZE  128
#define SHA2_MAX_DIGEST_SIZE    SHA512_DIGEST_SIZE

/* type to hold the SHA384 (and SHA512) context */

typedef struct
{   uint64_t count[2];
    uint64_t hash[8];
    uint64_t wbuf[16];
} sha512_ctx;

typedef sha512_ctx  sha384_ctx;

typedef struct
{   union
    { sha256_ctx  ctx256[1];
      sha512_ctx  ctx512[1];
    } uu[1];
    uint32_t    sha2_len;
} sha2_ctx;

VOID_RETURN sha512_compile(sha512_ctx ctx[1]);

VOID_RETURN sha384_begin(sha384_ctx ctx[1]);
#define sha384_hash sha512_hash
VOID_RETURN sha384_end(unsigned char hval[], sha384_ctx ctx[1]);
VOID_RETURN sha384(unsigned char hval[], const unsigned char data[], unsigned long len);

VOID_RETURN sha512_begin(sha512_ctx ctx[1]);
VOID_RETURN sha512_hash(const unsigned char data[], unsigned long len, sha512_ctx ctx[1]);
VOID_RETURN sha512_end(unsigned char hval[], sha512_ctx ctx[1]);
VOID_RETURN sha512(unsigned char hval[], const unsigned char data[], unsigned long len);

INT_RETURN  sha2_begin(unsigned long size, sha2_ctx ctx[1]);
VOID_RETURN sha2_hash(const unsigned char data[], unsigned long len, sha2_ctx ctx[1]);
VOID_RETURN sha2_end(unsigned char hval[], sha2_ctx ctx[1]);
INT_RETURN  sha2(unsigned char hval[], unsigned long size, const unsigned char data[], unsigned long len);

#endif

#if defined(__cplusplus)
}
#endif

#endif
