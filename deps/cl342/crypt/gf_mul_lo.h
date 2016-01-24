/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2008, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The redistribution and use of this software (with or without changes)
 is allowed without the payment of fees or royalties provided that:

  1. source code distributions include the above copyright notice, this
     list of conditions and the following disclaimer;

  2. binary distributions include the above copyright notice, this list
     of conditions and the following disclaimer in their documentation;

  3. the name of the copyright holder is not used to endorse products
     built using this software without specific written permission.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 20/12/2007

 This file provides the low level primitives needed for Galois Field 
 operations in GF(2^128) for the four most likely field representations.
*/

#ifndef _GF_MUL_LO_H
#define _GF_MUL_LO_H

#if defined( USE_INLINING )
#  if defined( _MSC_VER )
#    define gf_decl __inline
#  elif defined( __GNUC__ ) || defined( __GNU_LIBRARY__ )
#    define gf_decl static inline
#  else
#    define gf_decl static
#  endif
#endif

#if 0   /* used for testing only: t1(UNIT_BITS), t2(UNIT_BITS)  */
#  define _t1(n) bswap ## n ## _block(x, x)
#  define  t1(n) _t1(n)
#  define _t2(n) bswap ## n ## _block(x, x); bswap ## n ## _block(r, r) 
#  define  t2(n) _t2(n)
#endif

#define gf_m(n,x)    gf_mulx ## n ## x
#define gf_mulx1(x)  gf_m(1,x)
#define gf_mulx4(x)  gf_m(4,x)
#define gf_mulx8(x)  gf_m(8,x)

#define MASK(x) ((x) * (UNIT_CAST(-1,UNIT_BITS) / 0xff))

#define DATA_256(q) {\
    q(0x00), q(0x01), q(0x02), q(0x03), q(0x04), q(0x05), q(0x06), q(0x07),\
    q(0x08), q(0x09), q(0x0a), q(0x0b), q(0x0c), q(0x0d), q(0x0e), q(0x0f),\
    q(0x10), q(0x11), q(0x12), q(0x13), q(0x14), q(0x15), q(0x16), q(0x17),\
    q(0x18), q(0x19), q(0x1a), q(0x1b), q(0x1c), q(0x1d), q(0x1e), q(0x1f),\
    q(0x20), q(0x21), q(0x22), q(0x23), q(0x24), q(0x25), q(0x26), q(0x27),\
    q(0x28), q(0x29), q(0x2a), q(0x2b), q(0x2c), q(0x2d), q(0x2e), q(0x2f),\
    q(0x30), q(0x31), q(0x32), q(0x33), q(0x34), q(0x35), q(0x36), q(0x37),\
    q(0x38), q(0x39), q(0x3a), q(0x3b), q(0x3c), q(0x3d), q(0x3e), q(0x3f),\
    q(0x40), q(0x41), q(0x42), q(0x43), q(0x44), q(0x45), q(0x46), q(0x47),\
    q(0x48), q(0x49), q(0x4a), q(0x4b), q(0x4c), q(0x4d), q(0x4e), q(0x4f),\
    q(0x50), q(0x51), q(0x52), q(0x53), q(0x54), q(0x55), q(0x56), q(0x57),\
    q(0x58), q(0x59), q(0x5a), q(0x5b), q(0x5c), q(0x5d), q(0x5e), q(0x5f),\
    q(0x60), q(0x61), q(0x62), q(0x63), q(0x64), q(0x65), q(0x66), q(0x67),\
    q(0x68), q(0x69), q(0x6a), q(0x6b), q(0x6c), q(0x6d), q(0x6e), q(0x6f),\
    q(0x70), q(0x71), q(0x72), q(0x73), q(0x74), q(0x75), q(0x76), q(0x77),\
    q(0x78), q(0x79), q(0x7a), q(0x7b), q(0x7c), q(0x7d), q(0x7e), q(0x7f),\
    q(0x80), q(0x81), q(0x82), q(0x83), q(0x84), q(0x85), q(0x86), q(0x87),\
    q(0x88), q(0x89), q(0x8a), q(0x8b), q(0x8c), q(0x8d), q(0x8e), q(0x8f),\
    q(0x90), q(0x91), q(0x92), q(0x93), q(0x94), q(0x95), q(0x96), q(0x97),\
    q(0x98), q(0x99), q(0x9a), q(0x9b), q(0x9c), q(0x9d), q(0x9e), q(0x9f),\
    q(0xa0), q(0xa1), q(0xa2), q(0xa3), q(0xa4), q(0xa5), q(0xa6), q(0xa7),\
    q(0xa8), q(0xa9), q(0xaa), q(0xab), q(0xac), q(0xad), q(0xae), q(0xaf),\
    q(0xb0), q(0xb1), q(0xb2), q(0xb3), q(0xb4), q(0xb5), q(0xb6), q(0xb7),\
    q(0xb8), q(0xb9), q(0xba), q(0xbb), q(0xbc), q(0xbd), q(0xbe), q(0xbf),\
    q(0xc0), q(0xc1), q(0xc2), q(0xc3), q(0xc4), q(0xc5), q(0xc6), q(0xc7),\
    q(0xc8), q(0xc9), q(0xca), q(0xcb), q(0xcc), q(0xcd), q(0xce), q(0xcf),\
    q(0xd0), q(0xd1), q(0xd2), q(0xd3), q(0xd4), q(0xd5), q(0xd6), q(0xd7),\
    q(0xd8), q(0xd9), q(0xda), q(0xdb), q(0xdc), q(0xdd), q(0xde), q(0xdf),\
    q(0xe0), q(0xe1), q(0xe2), q(0xe3), q(0xe4), q(0xe5), q(0xe6), q(0xe7),\
    q(0xe8), q(0xe9), q(0xea), q(0xeb), q(0xec), q(0xed), q(0xee), q(0xef),\
    q(0xf0), q(0xf1), q(0xf2), q(0xf3), q(0xf4), q(0xf5), q(0xf6), q(0xf7),\
    q(0xf8), q(0xf9), q(0xfa), q(0xfb), q(0xfc), q(0xfd), q(0xfe), q(0xff) }

/*  Given the value i in 0..255 as the byte overflow when a field element
    in GHASH is multipled by x^8, this function will return the values that
    are generated in the lo 16-bit word of the field value by applying the
    modular polynomial. The values lo_byte and hi_byte are returned via the
    macro xp_fun(lo_byte, hi_byte) so that the values can be assembled into
    memory as required by a suitable definition of this macro operating on
    the table above
*/

#if defined( GF_MODE_BL ) || defined( GF_MODE_LL )

#define gf_uint16_xor(i) ( \
    (i & 0x01 ? xx(00,87) : 0) ^ (i & 0x02 ? xx(01,0e) : 0) ^ \
    (i & 0x04 ? xx(02,1c) : 0) ^ (i & 0x08 ? xx(04,38) : 0) ^ \
    (i & 0x10 ? xx(08,70) : 0) ^ (i & 0x20 ? xx(10,e0) : 0) ^ \
    (i & 0x40 ? xx(21,c0) : 0) ^ (i & 0x80 ? xx(43,80) : 0) )

enum x_bit 
{ 
    X_0 = 0x01, X_1 = 0x02, X_2 = 0x04, X_3 = 0x08, X_4 = 0x10, X_5 = 0x20, X_6 = 0x40, X_7 = 0x80
};

#elif defined( GF_MODE_BB ) || defined( GF_MODE_LB )

#define gf_uint16_xor(i) ( \
    (i & 0x80 ? xx(e1,00) : 0) ^ (i & 0x40 ? xx(70,80) : 0) ^ \
    (i & 0x20 ? xx(38,40) : 0) ^ (i & 0x10 ? xx(1c,20) : 0) ^ \
    (i & 0x08 ? xx(0e,10) : 0) ^ (i & 0x04 ? xx(07,08) : 0) ^ \
    (i & 0x02 ? xx(03,84) : 0) ^ (i & 0x01 ? xx(01,c2) : 0) )

enum x_bit 
{ 
    X_0 = 0x80, X_1 = 0x40, X_2 = 0x20, X_3 = 0x10, X_4 = 0x08, X_5 = 0x04, X_6 = 0x02, X_7 = 0x01
};

#else
#error Galois Field representation has not been set
#endif

#if defined( GF_MODE_BL ) || defined( GF_MODE_LB )

#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#  define xx(p,q)   0x##q##p
#else
#  define xx(p,q)   0x##p##q
#endif

#elif defined( GF_MODE_BB ) || defined( GF_MODE_LL )

#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#  define xx(p,q)   0x##p##q
#else
#  define xx(p,q)   0x##q##p
#endif

#else
#error Galois Field representation has not been set
#endif

const uint_16t gf_tab[256] = DATA_256(gf_uint16_xor);

/* BL low level Galois Field operations */

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN

#if UNIT_BITS == 64

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) & ~MASK(0x01) | ((x[n] >> 15) | (!n ? x[n+1] << 49 : 0)) & MASK(0x01)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) & ~MASK(0x0f) | ((x[n] >> 12) | (!n ? x[n+1] << 52 : 0)) & MASK(0x0f)
#define f8_bl(n,r,x)   r[n] = (x[n] >> 8) | (!n ? x[n+1] << 56 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01];
    rep2_u2(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f];
    rep2_u2(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0] & 0xff];
    rep2_u2(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= ((gf_unit_t)_tt) << 48;
}

#elif UNIT_BITS == 32

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) & ~MASK(0x01) | ((x[n] >> 15) | (n < 3 ? x[n+1] << 17 : 0)) & MASK(0x01)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) & ~MASK(0x0f) | ((x[n] >> 12) | (n < 3 ? x[n+1] << 20 : 0)) & MASK(0x0f)
#define f8_bl(n,r,x)   r[n] = (x[n] >> 8) | (n < 3 ? x[n+1] << 24 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01];
    rep2_u4(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f];
    rep2_u4(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0] & 0xff];
    rep2_u4(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= ((gf_unit_t)_tt) << 16;
}

#else

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) | (n < 15 ? x[n+1] >> 7 : 0)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) | (n < 15 ? x[n+1] >> 4 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01];
    rep2_u16(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[15] ^= _tt >> 8;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f];
    rep2_u16(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] ^= _tt >> 8;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15]  = _tt >> 8;
}

#endif

#elif PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#if UNIT_BITS == 64

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) | (!n ? x[n+1] >> 63 : 0)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) | (!n ? x[n+1] >> 60 : 0)
#define f8_bl(n,r,x)   r[n] = (x[n] << 8) | (!n ? x[n+1] >> 56 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 63) & 0x01];
    rep2_u2(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= _tt;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 60) & 0x0f];
    rep2_u2(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 56) & 0xff];
    rep2_u2(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

#elif UNIT_BITS == 32

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) | (n < 3 ? x[n+1] >> 31 : 0)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) | (n < 3 ? x[n+1] >> 28 : 0)
#define f8_bl(n,r,x)   r[n] = (x[n] << 8) | (n < 3 ? x[n+1] >> 24 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 31) & 0x01];
    rep2_u4(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= _tt;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 28) & 0x0f];
    rep2_u4(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 24) & 0xff];
    rep2_u4(f8_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

#else

#define f1_bl(n,r,x)   r[n] = (x[n] << 1) | (n < 15 ? x[n+1] >> 7 : 0)
#define f4_bl(n,r,x)   r[n] = (x[n] << 4) | (n < 15 ? x[n+1] >> 4 : 0)

gf_decl void gf_mulx1_bl(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 7) & 0x01];
    rep2_u16(f1_bl, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[15] ^= _tt & 0xff;
}

gf_decl void gf_mulx4_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 4) & 0x0f];
    rep2_u16(f4_bl, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] ^= _tt & 0xff;
}

gf_decl void gf_mulx8_bl(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15]  = _tt & 0xff;
}

#endif

#else
#  error Platform byte order has not been set. 
#endif

/* BB low level Galois Field operations */

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN

#if UNIT_BITS == 64

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) | (!n ? x[n+1] << 63 : 0)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) | (!n ? x[n+1] << 60 : 0)
#define f8_bb(n,r,x)   r[n] = (x[n] >> 8) | (!n ? x[n+1] << 56 : 0)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80];
    rep2_u2(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0];
    rep2_u2(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0] & 0xff];
    rep2_u2(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= ((gf_unit_t)_tt) << 48;
}

#elif UNIT_BITS == 32

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) | (n < 3 ? x[n+1] << 31 : 0)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) | (n < 3 ? x[n+1] << 28 : 0)
#define f8_bb(n,r,x)   r[n] = (x[n] >> 8) | (n < 3 ? x[n+1] << 24 : 0)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80];
    rep2_u4(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0];
    rep2_u4(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   gf_unit_t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0] & 0xff];
    rep2_u4(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= ((gf_unit_t)_tt) << 16;
}

#else

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) | (n < 15 ? x[n+1] << 7 : 0)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) | (n < 15 ? x[n+1] << 4 : 0)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80];
    rep2_u16(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[15] ^= _tt >> 8;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0];
    rep2_u16(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] ^= _tt >> 8;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
    UNIT_PTR(x)[14] ^= _tt & 0xff;
    UNIT_PTR(x)[15] = _tt >> 8;
}

#endif

#elif PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#if UNIT_BITS == 64

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) & ~MASK(0x80) | ((x[n] << 15) | (!n ? x[n+1] >> 49 : 0)) & MASK(0x80)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) & ~MASK(0xf0) | ((x[n] << 12) | (!n ? x[n+1] >> 52 : 0)) & MASK(0xf0)
#define f8_bb(n,r,x)   r[n] = (x[n] >> 8) & ~MASK(0xff) | ((x[n] <<  8) | (!n ? x[n+1] >> 56 : 0)) & MASK(0xff)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 49) & 0x80];
    rep2_u2(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[1] ^= _tt;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 52) & 0xf0];
    rep2_u2(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 56) & 0xff];
    rep2_u2(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt;
}

#elif UNIT_BITS == 32

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) & ~MASK(0x80) | ((x[n] << 15) | (n < 3 ? x[n+1] >> 17 : 0)) & MASK(0x80)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) & ~MASK(0xf0) | ((x[n] << 12) | (n < 3 ? x[n+1] >> 20 : 0)) & MASK(0xf0)
#define f8_bb(n,r,x)   r[n] = (x[n] >> 8) & ~MASK(0xff) | ((x[n] <<  8) | (n < 3 ? x[n+1] >> 24 : 0)) & MASK(0xff)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 17) & 0x80];
    rep2_u4(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[3] ^= _tt;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 20) & 0xf0];
    rep2_u4(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] >> 24) & 0xff];
    rep2_u4(f8_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[3] ^= _tt;
}

#else

#define f1_bb(n,r,x)   r[n] = (x[n] >> 1) | (n < 15 ? x[n+1] << 7 : 0)
#define f4_bb(n,r,x)   r[n] = (x[n] >> 4) | (n < 15 ? x[n+1] << 4 : 0)

gf_decl void gf_mulx1_bb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 7) & 0x80];
    rep2_u16(f1_bb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[15] ^= _tt;
}

gf_decl void gf_mulx4_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[0] << 4) & 0xf0];
    rep2_u16(f4_bb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] ^= _tt & 0xff;
}

gf_decl void gf_mulx8_bb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[0]];
    memmove(UNIT_PTR(x), UNIT_PTR(x) + 1, 15);
    UNIT_PTR(x)[14] ^= _tt >> 8;
    UNIT_PTR(x)[15] = _tt & 0xff;
}

#endif

#else
#  error Platform byte order has not been set. 
#endif

/* LL low level Galois Field operations */

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN

#if UNIT_BITS == 64

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) | (n ? x[n-1] >> 63 : 0)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) | (n ? x[n-1] >> 60 : 0)
#define f8_ll(n,r,x)   r[n] = (x[n] << 8) | (n ? x[n-1] >> 56 : 0)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 63) & 0x01];
    rep2_d2(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 60) & 0x0f];
    rep2_d2(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[1] >> 56];
    rep2_d2(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#elif UNIT_BITS == 32

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) | (n ? x[n-1] >> 31 : 0)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) | (n ? x[n-1] >> 28 : 0)
#define f8_ll(n,r,x)   r[n] = (x[n] << 8) | (n ? x[n-1] >> 24 : 0)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 31) & 0x01];
    rep2_d4(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 28) & 0x0f];
    rep2_d4(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[3] >> 24];
    rep2_d4(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#else

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) | (n ? x[n-1] >> 7 : 0)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) | (n ? x[n-1] >> 4 : 0)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] >> 7) & 0x01];
    rep2_d16(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt & 0xff;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] >> 4) & 0x0f];
    rep2_d16(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] ^= _tt & 0xff;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] =  _tt & 0xff;
}

#endif

#elif PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#if UNIT_BITS == 64

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) & ~MASK(0x01) | ((x[n] >> 15) | (n ? x[n-1] << 49 : 0)) & MASK(0x01)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) & ~MASK(0x0f) | ((x[n] >> 12) | (n ? x[n-1] << 52 : 0)) & MASK(0x0f)
#define f8_ll(n,r,x)   r[n] = (x[n] << 8) & ~MASK(0xff) | ((x[n] >>  8) | (n ? x[n-1] << 56 : 0)) & MASK(0xff)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 7) & 0x01];
    rep2_d2(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 4) & 0x0f];
    rep2_d2(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[1] & 0xff];
    rep2_d2(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 48;
}

#elif UNIT_BITS == 32

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) & ~MASK(0x01) | ((x[n] >> 15) | (n ? x[n-1] << 17 : 0)) & MASK(0x01)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) & ~MASK(0x0f) | ((x[n] >> 12) | (n ? x[n-1] << 20 : 0)) & MASK(0x0f)
#define f8_ll(n,r,x)   r[n] = (x[n] << 8) & ~MASK(0xff) | ((x[n] >>  8) | (n ? x[n-1] << 24 : 0)) & MASK(0xff)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 7) & 0x01];
    rep2_d4(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 4) & 0x0f];
    rep2_d4(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[3] & 0xff];
    rep2_d4(f8_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 16;
}

#else

#define f1_ll(n,r,x)   r[n] = (x[n] << 1) | (n ? x[n-1] >> 7 : 0)
#define f4_ll(n,r,x)   r[n] = (x[n] << 4) | (n ? x[n-1] >> 4 : 0)

gf_decl void gf_mulx1_ll(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] >> 7) & 0x01];
    rep2_d16(f1_ll, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt >> 8;
}

gf_decl void gf_mulx4_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] >> 4) & 0x0f];
    rep2_d16(f4_ll, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] ^= _tt >> 8;
}

gf_decl void gf_mulx8_ll(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] =  _tt >> 8;
}

#endif

#else
#  error Platform byte order has not been set. 
#endif

/* LB low level Galois Field operations */

#if PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN

#if UNIT_BITS == 64

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) & ~MASK(0x80) | ((x[n] << 15) | (n ? x[n-1] >> 49 : 0)) & MASK(0x80)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) & ~MASK(0xf0) | ((x[n] << 12) | (n ? x[n-1] >> 52 : 0)) & MASK(0xf0)
#define f8_lb(n,r,x)   r[n] = (x[n] << 8) | (n ? x[n-1] >> 56 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 49) & MASK(0x80)];
    rep2_d2(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] >> 52) & MASK(0xf0)];
    rep2_d2(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[1] >> 56];
    rep2_d2(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#elif UNIT_BITS == 32

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) & ~MASK(0x80) | ((x[n] << 15) | (n ? x[n-1] >> 17 : 0)) & MASK(0x80)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) & ~MASK(0xf0) | ((x[n] << 12) | (n ? x[n-1] >> 20 : 0)) & MASK(0xf0)
#define f8_lb(n,r,x)   r[n] = (x[n] << 8) | (n ? x[n-1] >> 24 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 17) & MASK(0x80)];
    rep2_d4(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] >> 20) & MASK(0xf0)];
    rep2_d4(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[3] >> 24];
    rep2_d4(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= _tt;
}

#else

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) | (n ? x[n-1] << 7 : 0)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) | (n ? x[n-1] << 4 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] << 7) & 0x80];
    rep2_d16(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] << 4) & 0xf0];
    rep2_d16(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] ^= _tt & 0xff;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
    UNIT_PTR(x)[1] ^= _tt >> 8;
    UNIT_PTR(x)[0] = _tt & 0xff;
}

#endif

#elif PLATFORM_BYTE_ORDER == IS_BIG_ENDIAN

#if UNIT_BITS == 64

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) | (n ? x[n-1] << 63 : 0)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) | (n ? x[n-1] << 60 : 0)
#define f8_lb(n,r,x)   x[n] = (x[n] >> 8) | (n ? x[n-1] << 56 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] << 7) & 0xff];
    rep2_d2(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= ((gf_unit_t)_tt)<< 48;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[1] << 4) & 0xff];
    rep2_d2(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 48;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[1] & 0xff];
    rep2_d2(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 48;
}

#elif UNIT_BITS == 32

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) | (n ? x[n-1] << 31 : 0)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) | (n ? x[n-1] << 28 : 0)
#define f8_lb(n,r,x)   r[n] = (x[n] >> 8) | (n ? x[n-1] << 24 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] << 7) & 0xff];
    rep2_d4(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[3] << 4) & 0xff];
    rep2_d4(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 16;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[3] & 0xff];
    rep2_d4(f8_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[0] ^= ((gf_unit_t)_tt) << 16;
}

#else

#define f1_lb(n,r,x)   r[n] = (x[n] >> 1) | (n ? x[n-1] << 7 : 0)
#define f4_lb(n,r,x)   r[n] = (x[n] >> 4) | (n ? x[n-1] << 4 : 0)

gf_decl void gf_mulx1_lb(gf_t r, const gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] << 7) & 0x80];
    rep2_d16(f1_lb, UNIT_PTR(r), UNIT_PTR(x));
    UNIT_PTR(r)[0] ^= _tt >> 8;
}

gf_decl void gf_mulx4_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[(UNIT_PTR(x)[15] << 4) & 0xff];
    rep2_d16(f4_lb, UNIT_PTR(x), UNIT_PTR(x));
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] ^= _tt >> 8;
}

gf_decl void gf_mulx8_lb(gf_t x)
{   uint_16t _tt;
    _tt = gf_tab[UNIT_PTR(x)[15]];
    memmove(UNIT_PTR(x) + 1, UNIT_PTR(x), 15);
    UNIT_PTR(x)[1] ^= _tt & 0xff;
    UNIT_PTR(x)[0] = _tt >> 8;
}

#endif

#else
#  error Platform byte order has not been set. 
#endif

#endif
