/*	@(#)crctab.h 1.2 96/09/13	*/

/*
 *  Crc calculation stuff.  See crctab.c
 */

extern unsigned short crctab[256] ;

#define updcrc(cp, crc) ( crctab[((crc >> 8) & 255)] ^ (crc << 8) ^ cp)

extern unsigned long cr3tab[] ;

#define UPDC32(b, c) (cr3tab[((int)c ^ b) & 0xff] ^ ((c >> 8) & 0x00FFFFFF))
