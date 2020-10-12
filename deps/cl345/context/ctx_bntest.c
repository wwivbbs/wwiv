/****************************************************************************
*																			*
*						  cryptlib Bignum Test Routines						*
*						Copyright Peter Gutmann 1998-2017					*
*																			*
****************************************************************************/

#define PKC_CONTEXT		/* Indicate that we're working with PKC contexts */
#if defined( INC_ALL )
  #include "crypt.h"
  #include "context.h"
#else
  #include "crypt.h"
  #include "context/context.h"
#endif /* Compiler-specific includes */

/* Work around a code optimizer bug in the IBM xlc compiler which causes the 
   compare operations in selfTestBignumFields() to be evaluated incorrectly, 
   resulting in a self-test failure.  Luckily IBM provides per-function
   optimisation control to "help debug errors that occur only under 
   optimization" (i.e. work around compiler bugs) so we just turn it off for
   the one function where it causes a problem */

#if defined( __xlc__ ) && defined( __OPTIMIZE__ )
  #pragma option_override( selfTestBignumFields, "opt( level, 0 )" ) 
#endif /* IBM xlc optimizer bug */

#if defined( USE_PKC ) && !defined( CONFIG_CONSERVE_MEMORY_EXTRA )

/* Test operation types */

typedef enum {
	BN_OP_NONE,				/* No operation type */
	BN_OP_CMP, BN_OP_CMPPART,
	BN_OP_ADD, BN_OP_SUB, 
	BN_OP_LSHIFT, BN_OP_RSHIFT,
	BN_OP_ADDWORD, BN_OP_SUBWORD,
	BN_OP_MULWORD, BN_OP_MODWORD,
	BN_OP_SQR, 
	BN_OP_DIV, BN_OP_MONTMODMULT,
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	BN_OP_MODADD, BN_OP_MODSUB,
	BN_OP_MODMUL, BN_OP_MODSHIFT,
#endif /* USE_ECDH || USE_ECDSA */
	BN_OP_LAST				/* Last possible operation type */
	} BN_OP_TYPE;

/* Some of the tests require negative values, in which case we encode a sign
   bit in the length field */

#define BN_VAL_NEGATIVE		0x800000
#define MK_NEGATIVE( val )	( ( val ) | 0x800000 )

/* The structure used to hold the self-test values */

typedef struct {
	const int aLen; const BYTE *a;
	const int bLen; const BYTE *b; unsigned int bWord;
	const int resultLen; const BYTE *result;
	const int modLen; const BYTE *mod;
	} SELFTEST_VALUE;

/* Macro to shorten the amount of text required to represent array sizes */

#define FS_SIZE( selfTestArray )	FAILSAFE_ARRAYSIZE( selfTestArray, SELFTEST_VALUE )

/****************************************************************************
*																			*
*								Self-test Data								*
*																			*
****************************************************************************/

/* Test values, a op b -> result.  Notes: We can't use values of 0 or 1 since
   these are never valid and are automatically rejected by importBignum(),
   and the code assumes a 32-bit architecture for checking of carry/borrow
   propagation functionality (it'll still work on 64-bit but won't hit as
   many corner cases) */

static const SELFTEST_VALUE cmpSelftestValues[] = {
	{ 1, MKDATA( "\x02" ), 1, MKDATA( "\x02" ), 0, 0, NULL },
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x02" ), 0, 1, NULL },
	{ 1, MKDATA( "\x02" ), 1, MKDATA( "\x03" ), 0, -1, NULL },
	{ 5, MKDATA( "\x01\x00\x00\x00\x00" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0, 1, NULL },
	{ 9, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0, 1, NULL },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 
	  5, MKDATA( "\x01\x00\x00\x00\x00" ), 0, -1, NULL },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 
	  9, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00" ), 0, -1, NULL },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE addsubSelftestValues[] = {
	/* a, b, result */
	{ 1, MKDATA( "\x02" ), 1, MKDATA( "\x03" ), 0,
	  1, MKDATA( "\x05" ) },
	{ MK_NEGATIVE( 1 ), MKDATA( "\x03" ), 1, MKDATA( "\x05" ), 0,
	  1, MKDATA( "\x02" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 1, MKDATA( "\x02" ), 0,
	  5, MKDATA( "\x01\x00\x00\x00\x01" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0,
	  5, MKDATA( "\x01\xFF\xFF\xFF\xFE" ) },
	{ 8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 
	  1, MKDATA( "\x02" ), 0,
	  9, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x01" ) },
	{ 8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 
	  8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 0,
	  9, MKDATA( "\x01\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" ) },
	{ 12, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  1, MKDATA( "\x02" ), 0,
	  12, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x57" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x01" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0,
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\x00\x00\x00\x01" ), 
	  8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 0,
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
	{ 8, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x01" ), 
	  12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0,
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
	{ 16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 0,
	  16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x56\x55\x55\x55\x55\x55\x55\x55\x54" ) },
	{ 16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x00\x00\x00\x00\x55\x55\x55\x55" ), 
	  8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 0,
	  16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x56\x00\x00\x00\x00\x55\x55\x55\x54" ) },
	{ 16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x00\x00\x00\x00\x55\x55\x55\x55" ), 
	  12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0,
	  16, MKDATA( "\x55\x55\x55\x56\x55\x55\x55\x54\x00\x00\x00\x01\x55\x55\x55\x54" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE shiftSelftestValues[] = {
	/* a, shiftAmt, result */
	{ 1, MKDATA( "\x02" ), 1, NULL, 0,
	  1, MKDATA( "\x04" ) },
	{ 4, MKDATA( "\x80\x00\x00\x01" ), 1, NULL, 0,
	  5, MKDATA( "\x01\x00\x00\x00\x02" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 1, NULL, 0,
	  5, MKDATA( "\x01\xFF\xFF\xFF\xFE" ) },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 1, NULL, 0,
	  8, MKDATA( "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA" ) },
	{ 10, MKDATA( "\x01\x23\x45\x67\x89\xAB\xCD\xEF\xE2\xD3" ), 1, NULL, 0,
	  10, MKDATA( "\x02\x46\x8A\xCF\x13\x57\x9B\xDF\xC5\xA6" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 16, NULL, 0,
	  14, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 32, NULL, 0,
	  16, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00" ) },
	{ 8, MKDATA( "\x01\x23\x45\x67\x89\xAB\xCD\xEF" ), 93, NULL, 0,
	  19, MKDATA( "\x24\x68\xAC\xF1\x35\x79\xBD\xE0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE addsubWordsSelftestValues[] = {
	/* a, bWord, result */
	{ 1, MKDATA( "\x02" ), 0, NULL, 3,
	  1, MKDATA( "\x05" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0, NULL, 2,
	  5, MKDATA( "\x01\x00\x00\x00\x01" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFD" ), 0, NULL, 2,
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ) },
	{ 8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ), 0, NULL, 2,
	  9, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x01" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE muldivWordsSelftestValues[] = {
	/* a, bWord, result */
	{ 1, MKDATA( "\x02" ), 0, NULL, 3,
	  1, MKDATA( "\x06" ) },
	{ 2, MKDATA( "\x45\x67" ), 0, NULL, 0x89AB,
	  4, MKDATA( "\x25\x52\x7A\xCD" ) },
	{ 4, MKDATA( "\x12\x34\x56\x78" ), 0, NULL, 0x9ABCDEF1,
	  8, MKDATA( "\x0B\x00\xEA\x4E\x36\x61\x76\xF8" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 0, NULL, 0xAA55AA55,
	  12, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8E\xDC\xC7\x10\x05" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE modWordsSelftestValues[] = {
	/* a, bWord, result */
	{ 1, MKDATA( "\x06" ), 0, NULL, 3, 0, NULL },
	{ 1, MKDATA( "\x05" ), 0, NULL, 3, 2, NULL },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0, NULL, 7, 3, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 14, 5, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 15, 5, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 16, 5, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 17, 0, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 18, 17, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 19, 18, NULL },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 20, 5, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 29, 27, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 177545, 111310, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 0xFFFE, 51, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 0xFFFF, 0, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 0xFFFFFFFE, 5, NULL },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 0, NULL, 0xFFFFFFFF, 0, NULL },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE sqrSelftestValues[] = {
	/* a, result */
	{ 1, MKDATA( "\x02" ), 0, NULL, 0,
	  1, MKDATA( "\x04" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0, NULL, 0, 
	  8, MKDATA( "\xFF\xFF\xFF\xFE\x00\x00\x00\x01" ) },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0, 
	  16, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 12, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0, 
	  24, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC6\xE3\x8E\x38\xE3"
		  "\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 20, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"
		  "\x55\x55\x55\x55" ), 0, NULL, 0,
	  40, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C"
		  "\x71\xC7\x1C\x71\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3"
		  "\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 24, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"
		  "\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0,
	  48, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C"
		  "\x71\xC7\x1C\x71\xC7\x1C\x71\xC6\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E"
		  "\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 28, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"
		  "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0,
	  56, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C"
		  "\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x38\xE3\x8E\x38"
		  "\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3"
		  "\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0,
	  32, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C"
		  "\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
	{ 32, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55"
		  "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 0, NULL, 0,
	  64, MKDATA( "\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C"
		  "\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71\xC7\x1C\x71"
		  "\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E"
		  "\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x38\xE3\x8E\x39" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE divSelftestValues[] = {
	/* a, b, result, remainder */
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x02" ), 0,	/* Test normalisation */
	  1, MKDATA( "\x01" ), 1, MKDATA( "\x01" ) },
	{ 1, MKDATA( "\x08" ), 1, MKDATA( "\x03" ), 0,
	  1, MKDATA( "\x02" ), 1, MKDATA( "\x02" ) },
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x08" ), 0,
	  1, MKDATA( "\x00" ), 1, MKDATA( "\x03" ) },
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x03" ), 0,
	  1, MKDATA( "\x01" ), 1, MKDATA( "\x00" ) },
	{ 4, MKDATA( "\x55\x55\x55\x55" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ), 0,
	  1, MKDATA( "\x04" ), 
	  4, MKDATA( "\x0C\x83\xFB\x75" ) },
	{ 8, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ), 0,
	  5, MKDATA( "\x04\xB0\x00\x00\x27" ), 
	  4, MKDATA( "\x0F\x5C\x29\x0D" ) },
	{ 12, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ), 0,
	  9, MKDATA( "\x04\xB0\x00\x00\x27\xD8\x00\x01\x52" ), 
	  4, MKDATA( "\x0C\x3B\x2A\xE5" ) },
	{ 16, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ), 0,
	  13, MKDATA( "\x04\xB0\x00\x00\x27\xD8\x00\x01\x52\xAC\x00\x0B\x3E" ), 
	  4, MKDATA( "\x0C\xF1\x3C\x45" ) },
	{ 20, MKDATA( "\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55\x55" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ), 0,
	  17, MKDATA( "\x04\xB0\x00\x00\x27\xD8\x00\x01\x52\xAC\x00\x0B\x3E\xB6\x00\x5F\x95" ), 
	  3, MKDATA( "\xC8\x79\x7D" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8F" ), 0,
	  1, MKDATA( "\x01" ), 
	  8, MKDATA( "\x06\x17\x7d\x8f\x01\x23\x45\x62" ) },
	{ 8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8F" ),
	  8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 0,
	  1, MKDATA( "\x00" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8F" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  4, MKDATA( "\x56\x78\x9A\xBC" ), 0,
	  4, MKDATA( "\x35\xE5\x0D\x79" ), 
	  4, MKDATA( "\x45\xB4\x30\x15" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0, 
	  5, MKDATA( "\x01\x80\xBF\xA0\x2E" ), 
	  8, MKDATA( "\x44\xBB\x44\xBD\x44\xBB\x44\xB9" ) },
	{ 20, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0, 
	  13, MKDATA( "\x01\x80\xBF\xA0\x2E\x67\x4C\x59\xD6\x17\xF4\x05\xFA" ), 
	  5, MKDATA( "\x02\xFF\xFF\xFF\xFD" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE montModMulSelftestValues[] = {
	/* a, b, result, modulus */
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x04" ), 0,
	  1, MKDATA( "\x02" ), 1, MKDATA( "\x05" ) },
	{ 1, MKDATA( "\x07" ), 1, MKDATA( "\x07" ), 0,
	  1, MKDATA( "\x04" ), 1, MKDATA( "\x09" ) },
	{ 4, MKDATA( "\x12\x34\x56\x78" ), 4, MKDATA( "\x55\x55\x55\x55" ), 0,
	  4, MKDATA( "\x6E\x6E\x6E\x6E" ), 4, MKDATA( "\x9A\xBC\xDE\xF1" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8F" ), 0,
	  8, MKDATA( "\x67\x63\xD1\x34\xCA\x6D\x53\x9F" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  4, MKDATA( "\x56\x78\x9A\xBC" ), 0,
	  8, MKDATA( "\x02\x34\x6B\x2A\x61\x50\x18\x42" ), 
	  8, MKDATA( "\x12\x34\x56\x79\x00\x00\x00\x01" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\x00\x00\x00\x00\xFF\xFF\xFF\xFF" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0, 
	  13, MKDATA( "\x04\x12\x96\xEF\xDF\x20\x8F\x5F\xFC\x0B\xFE\x08\x53" ), 
	  13, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1\x23\x45\x67\x89\xAB" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

#if defined( USE_ECDH ) || defined( USE_ECDSA )

static const SELFTEST_VALUE modAddSelftestValues[] = {
	/* a, b, result, modulus */
	{ 1, "\x03", 1, "\x04", 0,
	  1, "\x02", 1, "\x05" },
	{ 4, "\xFF\xFF\xFF\xFE", 1, "\x03", 0,
	  1, "\x02", 4, "\xFF\xFF\xFF\xFF" },
	{ 8, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 8, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 0,
	  8, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFD", 8, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" },
	{ 12, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x03", 4, "\xFF\xFF\xFF\xFF", 0,
	  1, "\x02", 13, "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	{ 12, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x04", 4, "\xFF\xFF\xFF\xFF", 0,
	  1, "\x03", 13, "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
	{ 12, "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x05", 4, "\xFF\xFF\xFF\xFF", 0,
	  1, "\x04", 13, "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE modSubSelftestValues[] = {
	/* a, b, result, modulus */
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x04" ), 0,
	  1, MKDATA( "\x04" ), 1, MKDATA( "\x05" ) },
	{ 4, MKDATA( "\xFF\xFF\xFF\xFE" ), 1, MKDATA( "\x03" ), 0,
	  4, MKDATA( "\xFF\xFF\xFF\xFB" ), 4, MKDATA( "\xFF\xFF\xFF\xFF" ) },
	{ 1, MKDATA( "\x02" ), 8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE" ), 0,
	  1, MKDATA( "\x03" ), 8, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x03" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0,
	  12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE\x00\x00\x00\x04" ), 
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x04" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0,
	  12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE\x00\x00\x00\x05" ), 
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
	{ 12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00\x00\x00\x05" ), 
	  4, MKDATA( "\xFF\xFF\xFF\xFF" ), 0,
	  12, MKDATA( "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE\x00\x00\x00\x06" ), 
	  13, MKDATA( "\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE modMulSelftestValues[] = {
	/* a, b, result, modulus */
	{ 1, MKDATA( "\x03" ), 1, MKDATA( "\x04" ), 0,
	  1, MKDATA( "\x02" ), 1, MKDATA( "\x05" ) },
	{ 1, MKDATA( "\x07" ), 1, MKDATA( "\x07" ), 0,
	  1, MKDATA( "\x04" ), 1, MKDATA( "\x05" ) },
	{ 4, MKDATA( "\x12\x34\x56\x78" ), 4, MKDATA( "\x9A\xBC\xDE\xF1" ), 0,
	  4, MKDATA( "\x41\x62\x61\x46" ), 4, MKDATA( "\x55\x55\x55\x55" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0,
	  8, MKDATA( "\x02\xBF\xCF\x99\x0F\xFA\x44\x09" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8E" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0,
	  8, MKDATA( "\x01\x1D\x9A\xC1\xA9\x93\xDC\xB2" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x8F" ) },
	{ 8, MKDATA( "\x12\x34\x56\x78\x9A\xBC\xDE\xF1" ), 
	  8, MKDATA( "\xAA\x55\xAA\x55\xAA\x55\xAA\x55" ), 0,
	  8, MKDATA( "\x0B\x98\x3E\xD3\xDC\xC7\x0F\x15" ), 
	  8, MKDATA( "\x0C\x1C\xD8\xE9\x99\x99\x99\x90" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};

static const SELFTEST_VALUE modShiftSelftestValues[] = {
	/* a, b, result, modulus */
	{ 1, MKDATA( "\x02" ), 2, NULL, 0,
	  1, MKDATA( "\x02" ), 1, MKDATA( "\x03" ) },
	{ 4, MKDATA( "\x12\x34\x56\x78" ), 27, NULL, 0,
	  4, MKDATA( "\x19\x38\xFB\x5B" ), 
	  4, MKDATA( "\xAA\x55\xAA\x55" ) },
	{ 2, MKDATA( "\xAA\x55" ), 27, NULL, 0,
	  4, MKDATA( "\x06\x6F\xAD\xD0" ), 
	  4, MKDATA( "\x12\x34\x56\x78" ) },
	{ 2, MKDATA( "\xAA\x55" ), 27, NULL, 0,
	  4, MKDATA( "\x06\x6F\x62\xF6" ), 
	  4, MKDATA( "\x12\x34\x56\x79" ) },
	{ 2, MKDATA( "\xAA\x55" ), 27, NULL, 0,
	  4, MKDATA( "\x06\x6F\x18\x1C" ), 
	  4, MKDATA( "\x12\x34\x56\x7A" ) },
		{ 0, NULL, 0, NULL, 0, 0, NULL }, { 0, NULL, 0, NULL, 0, 0, NULL }
	};
#endif /* USE_ECDH || USE_ECDSA */

/****************************************************************************
*																			*
*								Self-test Routines							*
*																			*
****************************************************************************/

/* Check that the compiler doesn't do strange things to the overlaid
   BIGNUM/BIGNUM_EXT/BIGNUM_EXT2 fields */

#define TEST_TOP_CONST		0x12345678U
#define TEST_NEG_CONST		0x9ABCDEF0U
#define TEST_FLAGS_CONST	0x1F2E3D4CU
#define TEST_D0_CONST		0x5B6A7980U
#define TEST_D1_CONST		0x19283746U
#define TEST_D2_CONST		0x5F6E7D4CU

CHECK_RETVAL_BOOL \
static BOOLEAN selfTestBignumFields( void )
	{
	BIGNUM bignum;
	BIGNUM_EXT *bignumExt = ( BIGNUM_EXT * ) &bignum;
	BIGNUM_EXT2 *bignumExt2 = ( BIGNUM_EXT2 * ) &bignum;

	/* At this point we haven't checked any of the higher-level bignum ops 
	   so we have to manipulate the fields directly.  These are fundamental 
	   tests that indicate serious problems if they fail, so we provide 
	   additional diagnostics in cases that might occur */
	memset( &bignum, 0, sizeof( BIGNUM ) );
	bignum.top = TEST_TOP_CONST;
	bignum.neg = TEST_NEG_CONST;
	bignum.flags = TEST_FLAGS_CONST;
	bignum.d[ 0 ] = TEST_D0_CONST;
	bignum.d[ 1 ] = TEST_D1_CONST;
	bignum.d[ 2 ] = TEST_D2_CONST;
	if( bignumExt->top != TEST_TOP_CONST || \
		bignumExt->neg != TEST_NEG_CONST || \
		bignumExt->flags != TEST_FLAGS_CONST || \
		bignumExt->d[ 0 ] != TEST_D0_CONST || \
		bignumExt->d[ 1 ] != TEST_D1_CONST || \
		bignumExt->d[ 2 ] != TEST_D2_CONST )
		{
		DEBUG_DIAG(( "Check of BIGNUM/BIGNUM_EXT overlay failed" ));
		DEBUG_DIAG(( "BN->top = %lX, BN->neg = %lX, BN->flags = %lX, "
					 "BN->d[0] = %lX, BN->d[1] = %lX, BN->d[2] = %lX", 
					 bignumExt->top, bignumExt->neg, bignumExt->flags, 
					 bignumExt->d[ 0 ], bignumExt->d[ 1 ], 
					 bignumExt->d[ 2 ] ));
		return( FALSE );
		}
	if( bignumExt2->top != TEST_TOP_CONST || \
		bignumExt2->neg != TEST_NEG_CONST || \
		bignumExt2->flags != TEST_FLAGS_CONST || \
		bignumExt2->d[ 0 ] != TEST_D0_CONST || \
		bignumExt2->d[ 1 ] != TEST_D1_CONST || \
		bignumExt2->d[ 2 ] != TEST_D2_CONST )
		{
		DEBUG_DIAG(( "Check of BIGNUM/BIGNUM_EXT2 overlay failed" ));
		DEBUG_DIAG(( "BN->top = %lX, BN->neg = %lX, BN->flags = %lX, "
					 "BN->d[0] = %lX, BN->d[1] = %lX, BN->d[2] = %lX", 
					 bignumExt2->top, bignumExt2->neg, bignumExt2->flags, 
					 bignumExt2->d[ 0 ], bignumExt2->d[ 1 ], 
					 bignumExt2->d[ 2 ] ));
		return( FALSE );
		}

	return( TRUE );
	}

/* If the application that uses us is linked against anything containing 
   OpenSSL code then, depending on the link order, the bignum function calls
   may end up referencing the OpenSSL version of the functions rather than 
   our ones.  To try and detect this we initialise a bignum with BN_one()
   (= BN_set_word()), check that the fields are OK, and if they're not check
   whehter they look like an OpenSSL-style bignum */ 

CHECK_RETVAL_BOOL \
static BOOLEAN selfTestBignumLinkage( void )
	{
	typedef struct {
		BN_ULONG *d;
		int top, dmax, neg, flags;
		} BIGNUM_ALT;
	BIGNUM bignum;
	BIGNUM_ALT *bignumAlt = ( BIGNUM_ALT * ) &bignum;

	/* Set the bignum to zero.  If we're being used with our own bignum 
	   code, the fields should be set correctly */
	BN_init( &bignum );
	( void ) BN_one( &bignum );
	if( bignum.top == 1 && bignum.neg == 0 && bignum.flags == 0 && \
		bignum.d[ 0 ] == 1 && bignum.d[ 1 ] == 0 )
		{
		BN_clear( &bignum );
		return( TRUE );
		}

	/* Check for the fields being set by the OpenSSL functions */
	if( bignumAlt->top == 1 && bignumAlt->neg == 0 && bignumAlt->flags == 0 )
		{
		DEBUG_DIAG(( "BN code appears to be using OpenSSL, not cryptlib, "
					 "library" ));
		return( FALSE );
		}

	DEBUG_DIAG(( "BN init sanity check failed" ));
	return( FALSE );
	}

/* Test general bignum ops.  Apart from acting as a standard self-test these 
   also test the underlying building blocks used in the higher-level 
   routines whose self-tests follow */

CHECK_RETVAL_BOOL \
static BOOLEAN selfTestGeneralOps1( void )
	{
	BIGNUM a;
#ifdef BN_LLONG
	const BN_ULONG bnWord1 = ( BN_ULONG  ) ~0, bnWord2 = ( BN_ULONG  ) ~0;
	BN_ULLONG bnWordL;
#endif /* BN_LLONG */

	/* If we're using double-sized long words to handle overflow from
	   standard long words, make sure that things are set up correctly */
#ifdef BN_LLONG
	bnWordL = ( BN_ULLONG ) bnWord1 + bnWord2;
	bnWordL >>= BN_BITS2;
	if( bnWordL != 1 )
		{
		DEBUG_DIAG(( "Test of overflow handling from\n  BN_ULONG via "
					 "BN_ULLONG failed" ));
		DEBUG_DIAG(( "sizeof( BN_LONG ) = %d,\n  sizeof( BN_ULLONG ) = %d, "
					 "BN_BITS2 = %d", sizeof( BN_ULONG ), sizeof( BN_ULLONG ),
					 BN_BITS2 ));
		return( FALSE );
		}
#endif /* BN_LLONG */

	/* Simple tests that don't need the support of higher-level routines 
	   like importBignum().  These are fundamental tests that indicate 
	   serious problems if they fail, so we provide additional diagnostics 
	   in cases that might occur */
	BN_init( &a );
	if( !BN_zero( &a ) )
		return( FALSE );
	if( !BN_is_zero( &a ) || BN_is_one( &a ) )
		{
		DEBUG_DIAG(( "Check of BN set to zero failed" ));
		DEBUG_DIAG(( "BN.top = %d, BN.neg = %d, BN.flags = %0X, "
					 "BN.d[0] = %0X, BN.d[1] = %0X, BN.d[2] = %0X", 
					 a.top, a.neg, a.flags, a.d[ 0 ], a.d[ 1 ], a.d[ 2 ] ));
		return( FALSE );
		}
	if( !BN_is_word( &a, 0 ) || BN_is_word( &a, 1 ) )
		return( FALSE );	/* Identical to the above macros */
	if( BN_is_odd( &a ) )
		{
		DEBUG_DIAG(( "Odd/even check of BN set to zero failed" ));
		return( FALSE );
		}
	if( BN_get_word( &a ) != 0 )
		{
		DEBUG_DIAG(( "Read of BN set to zero failed" ));
		return( FALSE );
		}
	if( !BN_one( &a ) )
		return( FALSE );
	if( BN_is_zero( &a ) || !BN_is_one( &a ) )
		{
		DEBUG_DIAG(( "Check of BN set to one failed" ));
		return( FALSE );
		}
	if( BN_is_word( &a, 0 ) || !BN_is_word( &a, 1 ) )
		return( FALSE );	/* Identical to the above macros */
	if( !BN_is_odd( &a ) )
		{
		DEBUG_DIAG(( "Odd/even check of BN set to one failed" ));
		return( FALSE );
		}
	if( BN_num_bytes( &a ) != 1 )
		return( FALSE );
	if( BN_get_word( &a ) != 1 )
		{
		DEBUG_DIAG(( "Read of BN set to one failed" ));
		return( FALSE );
		}
	BN_clear( &a );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
static BOOLEAN selfTestGeneralOps2( void )
	{
	BIGNUM a;
	int status;

	/* More complex tests that need higher-level routines like importBignum(),
	   run after the tests of components of importBignum() have concluded */
	BN_init( &a );
#if BN_BITS2 == 64
	status = importBignum( &a, "\x01\x00\x00\x00\x00\x00\x00\x00\x00", 9, 
						   1, 128, NULL, KEYSIZE_CHECK_NONE );
#else
	status = importBignum( &a, "\x01\x00\x00\x00\x00", 5, 1, 128, NULL, 
						   KEYSIZE_CHECK_NONE );
#endif /* 64- vs 32-bit */
	if( cryptStatusError( status ) )
		return( FALSE );
	if( BN_is_zero( &a ) || BN_is_one( &a ) )
		return( FALSE );
	if( BN_is_word( &a, 0 ) || BN_is_word( &a, 1 ) )
		return( FALSE );
	if( BN_is_odd( &a ) )
		return( FALSE );
	if( BN_get_word( &a ) != BN_NAN )
		return( FALSE );
	if( BN_num_bytes( &a ) != ( BN_BITS2 / 8 ) + 1 )
		return( FALSE );
	if( BN_num_bits( &a ) != BN_BITS2 + 1 )
		return( FALSE );
	if( !BN_is_bit_set( &a, BN_BITS2 ) )
		return( FALSE );
	if( BN_is_bit_set( &a, 17 ) || !BN_set_bit( &a, 17 ) || \
		!BN_is_bit_set( &a, 17 ) )
		return( FALSE );
#if BN_BITS2 == 64
	status = importBignum( &a, "\x01\x00\x00\x00\x00\x00\x00\x00\x01", 9, 
						   1, 128, NULL, KEYSIZE_CHECK_NONE );
#else
	status = importBignum( &a,	"\x01\x00\x00\x00\x01", 5, 1, 128, NULL,
						   KEYSIZE_CHECK_NONE );
#endif /* 64- vs 32-bit */
	if( cryptStatusError( status ) )
		return( FALSE );
	if( BN_is_zero( &a ) || BN_is_one( &a ) )
		return( FALSE );
	if( BN_is_word( &a, 0 ) || BN_is_word( &a, 1 ) )
		return( FALSE );
	if( !BN_is_odd( &a ) )
		return( FALSE );
	if( BN_num_bytes( &a ) != ( BN_BITS2 / 8 ) + 1 )
		return( FALSE );
	if( BN_get_word( &a ) != BN_NAN )
		return( FALSE );
	if( BN_num_bits( &a ) != BN_BITS2 + 1 )
		return( FALSE );
	if( !BN_is_bit_set( &a, BN_BITS2 ) )
		return( FALSE );
	if( BN_is_bit_set( &a, BN_BITS2 + 27 ) || \
		!BN_set_bit( &a, BN_BITS2 + 27 ) || \
		!BN_is_bit_set( &a, BN_BITS2 + 27 ) )
		return( FALSE );
	/* Setting a bit off the end of a bignum extends its size, which is why
	   the following value doesn't match the one from a few lines earlier */
	if( BN_num_bytes( &a ) != ( BN_BITS2 / 8 ) + 4 )
		return( FALSE );
	/* The bit index for indexing bits is zero-based (since 1 == 1 << 0) but
	   for counting bits is one-based, which is why the following comparison
	   looks wrong.  Yet another one of OpenSSL's many booby-traps */
	if( BN_num_bits( &a ) != BN_BITS2 + 28 )
		return( FALSE );
	BN_clear( &a );

	return( TRUE );
	}

/* Make sure that the selected operation gives the required result */

#if defined( DEBUG_DIAGNOSTIC_ENABLE ) && !defined( NDEBUG )

static void reportBignumValues( const char *opDescription,
								const char *failType,
								const BIGNUM *a, const BIGNUM *b,
								const BIGNUM *result, 
								const BIGNUM *expectedResult )
	{
	DEBUG_DIAG(( "Operation 'a %s b -> result'\n  failed (%s), bignum "
				 "values follow", opDescription, failType ));
	printBignum( a, "a" );
	printBignum( b, "b" );
	printBignum( expectedResult, "Expected result" );
	printBignum( result, "Actual result" );
	}
#else

#define reportBignumValues( opDescription, faileType, a, b, result, expectedResult )

#endif /* DEBUG_DIAGNOSTIC_ENABLE !NDEBUG */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN selfTestOp( const SELFTEST_VALUE *selftestValue,
						   IN_ENUM( BN_OP ) const BN_OP_TYPE op,
						   const char *opDescription )
	{
	BN_CTX bnCTX;
	BN_MONT_CTX bnMontCTX;
	BIGNUM a, b, mod, result, expectedResult;
	BN_ULONG bWord DUMMY_INIT;
	const BOOLEAN isCompareOp = ( op == BN_OP_CMP || op == BN_OP_CMPPART ) ? \
								TRUE : FALSE;
	const BOOLEAN isDivOp = ( op == BN_OP_DIV ) ? TRUE : FALSE;
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	const BOOLEAN isModOp = ( ( op >= BN_OP_MODADD && op <= BN_OP_MODSHIFT ) || \
							  ( op == BN_OP_MONTMODMULT ) ) ? TRUE : FALSE;
#else
	const BOOLEAN isModOp = ( op == BN_OP_MONTMODMULT ) ? TRUE : FALSE;
#endif /* USE_ECDH || USE_ECDSA */
	BOOLEAN aNeg, expectedResultNeg DUMMY_INIT;
	int aLen, expectedResultLen, bValue DUMMY_INIT;
	int bnStatus = BN_STATUS, status;

	assert( isReadPtr( selftestValue, sizeof( SELFTEST_VALUE ) ) );

	REQUIRES_B( isEnumRange( op, BN_OP ) );

	/* The BN_OP_MODWORD is sufficently specialised that we have to handle 
	   it specially */
	if( op == BN_OP_MODWORD )
		{
		BN_ULONG word DUMMY_INIT;

		BN_init( &a );

		status = importBignum( &a, selftestValue->a, selftestValue->aLen, 
							   1, 128, NULL, KEYSIZE_CHECK_NONE );
		if( cryptStatusError( status ) )
			return( FALSE );
		CK( BN_mod_word( &word, &a, selftestValue->bWord ) );
		if( bnStatusError( bnStatus) || word != selftestValue->resultLen )
			return( FALSE );

		BN_clear( &a );

		return( TRUE );
		}

	BN_CTX_init( &bnCTX );
	BN_MONT_CTX_init( &bnMontCTX );
	BN_init( &a );
	BN_init( &b );
	BN_init( &mod );
	BN_init( &result );
#if BN_BITS2 == 64
	status = importBignum( &result,		/* Pollute result value */
						   "\x55\x55\x55\x55\x55\x55\x55\x55" \
						   "\x44\x44\x44\x44\x44\x44\x44\x44\x33\x33\x33\x33" \
						   "\x33\x33\x33\x33\x22\x22\x22\x22\x22\x22\x22\x22" \
						   "\x11\x11\x11\x11\x11\x11\x11\x11", 
						   40, 1, 128, NULL, KEYSIZE_CHECK_NONE );
#else
	status = importBignum( &result,		/* Pollute result value */
						   "\xAA\xAA\xAA\xAA\x99\x99\x99\x99" \
						   "\x88\x88\x88\x88\x77\x77\x77\x77\x66\x66\x66\x66" \
						   "\x55\x55\x55\x55\x44\x44\x44\x44\x33\x33\x33\x33" \
						   "\x22\x22\x22\x22\x11\x11\x11\x11", 40, 1, 128,
						   NULL, KEYSIZE_CHECK_NONE );
#endif /* 64- vs 32-bit */
	if( cryptStatusError( status ) )
		return( FALSE );
	BN_init( &expectedResult );

	/* Some of the quantities may have flags indicating that they're meant 
	   to be treated as negative values, so we have to extract the actual 
	   value and the negative flag from the overall value */
	aLen = selftestValue->aLen & ~BN_VAL_NEGATIVE;
	aNeg = ( selftestValue->aLen & BN_VAL_NEGATIVE ) ? TRUE : FALSE;
	expectedResultLen = selftestValue->resultLen & ~BN_VAL_NEGATIVE;
	expectedResultNeg = ( selftestValue->resultLen & BN_VAL_NEGATIVE ) ? \
						TRUE : FALSE;

	/* Set up the test data */
	status = importBignum( &a, selftestValue->a, aLen, 1, 128, NULL, 
						   KEYSIZE_CHECK_NONE );
	if( cryptStatusOK( status ) )
		{
		if( selftestValue->b != NULL )
			{
			status = importBignum( &b, selftestValue->b, selftestValue->bLen, 
								   1, 128, NULL, KEYSIZE_CHECK_NONE );
			}
		else
			{
			if( selftestValue->bLen != 0 )
				bValue = selftestValue->bLen;
			else
				bWord = selftestValue->bWord;
			}
		}
	if( cryptStatusOK( status ) && !isCompareOp )
		{
		/* Some of the div/mod operations produce a result of zero or one, 
		   which we can't import via importBignum() but can obtain by
		   special-casing the import via BN_set_word() */
		if( expectedResultLen == 1 )
			{
			CK( BN_set_word( &expectedResult, selftestValue->result[ 0 ] ) );
			if( bnStatusError( bnStatus ) )
				status = CRYPT_ERROR_FAILED;
			}
		else
			{
			status = importBignum( &expectedResult, selftestValue->result, 
								   expectedResultLen, 1, 128, NULL, 
								   KEYSIZE_CHECK_NONE );
			}
		}
	if( cryptStatusOK( status ) && isModOp )
		{
		status = importBignum( &mod, selftestValue->mod, 
							   selftestValue->modLen, 1, 128, NULL, 
							   KEYSIZE_CHECK_NONE );
		}
	if( cryptStatusError( status ) )
		return( FALSE );
	if( aNeg )
		BN_set_negative( &a, TRUE );

	/* Perform the requested operation on the values */
	switch( op )
		{
		case BN_OP_CMP:
			/* This is a compare op so bnStatus is the compare result, not 
			   an error code */
			bnStatus = BN_cmp( &a, &b );
			break;

		case BN_OP_CMPPART:
			/* As before */
			bnStatus = bn_cmp_part_words( a.d, b.d, min( a.top, b.top ),
										  a.top - b.top );
			break;

		case BN_OP_ADD:
			CK( BN_add( &result, &a, &b ) );
			break;

		case BN_OP_SUB:
			BN_swap( &expectedResult, &a );
			CK( BN_sub( &result, &a, &b ) );
			break;

		case BN_OP_LSHIFT:
			CK( BN_lshift( &result, &a, bValue ) );
			break;

		case BN_OP_RSHIFT:
			BN_swap( &expectedResult, &a );
			CK( BN_rshift( &result, &a, bValue ) );
			break;

		case BN_OP_ADDWORD:
			CKPTR( BN_copy( &result, &a ) );
			CK( BN_add_word( &result, bWord ) );
			break;

		case BN_OP_SUBWORD:
			CKPTR( BN_copy( &result, &expectedResult ) );
			CKPTR( BN_copy( &expectedResult, &a ) );
			CK( BN_sub_word( &result, bWord ) );
			break;

		case BN_OP_MULWORD:
			CKPTR( BN_copy( &result, &a ) );
			CK( BN_mul_word( &result, bWord ) );
			break;

		case BN_OP_SQR:
			CK( BN_sqr( &result, &a, &bnCTX ) );
			break;

		case BN_OP_DIV:
			CK( BN_div( &result, &mod, &a, &b, &bnCTX ) );
			break;

		case BN_OP_MONTMODMULT:
			CK( BN_MONT_CTX_set( &bnMontCTX, &mod, &bnCTX ) );
			CK( BN_to_montgomery( &a, &a, &bnMontCTX, &bnCTX ) );
			CK( BN_mod_mul_montgomery( &result, &a, &b, &bnMontCTX, &bnCTX ) );
			break;

#if defined( USE_ECDH ) || defined( USE_ECDSA )
		case BN_OP_MODADD:
			CK( BN_mod_add_quick( &result, &a, &b, &mod ) );
			break;

		case BN_OP_MODSUB:
			CK( BN_mod_sub_quick( &result, &a, &b, &mod ) );
			break;

		case BN_OP_MODMUL:
			CK( BN_mod_mul( &result, &a, &b, &mod, &bnCTX ) );
			break;
		
		case BN_OP_MODSHIFT:
			CK( BN_mod_lshift_quick( &result, &a, bValue, &mod ) );
			break;
#endif /* USE_ECDH || USE_ECDSA */

		default:
			return( FALSE );
		}
	if( isCompareOp )
		{
		/* The compare operations return their result as the return status 
		   so there's no result value to check.  In addition they return
		   { -1, 0, 1 } so we can't use the processed expectedResultLen 
		   value */
		if( bnStatus != selftestValue->resultLen )
			return( FALSE );

		return( TRUE );
		}
	if( bnStatusError( bnStatus ) )
		return( FALSE );

	/* Make sure that we got what we were expecting */
	if( BN_cmp( &result, &expectedResult ) )
		{
		reportBignumValues( opDescription, "BN_cmp()", &a, &b, 
							&result, &expectedResult );
		return( FALSE );
		}
	if( expectedResultNeg && !BN_is_negative( &result ) )
		{
		reportBignumValues( opDescription, "negative check", &a, &b, 
							&result, &expectedResult );
		return( FALSE );
		}
	if( isDivOp )
		{
		BIGNUM expectedRemainder;

		BN_init( &expectedRemainder );

		if( selftestValue->modLen == 1 )
			{
			CK( BN_set_word( &expectedRemainder, selftestValue->mod[ 0 ] ) );
			if( bnStatusError( bnStatus ) )
				status = CRYPT_ERROR_FAILED;
			}
		else
			{
			status = importBignum( &expectedRemainder, selftestValue->mod, 
								   selftestValue->modLen, 1, 128, NULL, 
								   KEYSIZE_CHECK_NONE );
			}
		if( cryptStatusError( status ) )
			return( FALSE );
		if( BN_cmp( &mod, &expectedRemainder ) )
			{
			reportBignumValues( opDescription, "BN_cmp()", &a, &b, 
								&mod, &expectedRemainder );
			return( FALSE );
			}
		BN_clear( &expectedRemainder );
		}
	BN_clear( &expectedResult );
	BN_clear( &result );
	BN_clear( &mod );
	BN_clear( &b );
	BN_clear( &a );
	BN_MONT_CTX_free( &bnMontCTX );
	BN_CTX_final( &bnCTX );

	return( TRUE );
	}

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
static BOOLEAN selfTestOps( const SELFTEST_VALUE *selftestValueArray,
							IN_RANGE( 1, 50 ) const int selftestValueArraySize,
							IN_ENUM( BN_OP ) const BN_OP_TYPE op,
							const char *opDescription )
	{
	int i, LOOP_ITERATOR;

	assert( isReadPtrDynamic( selftestValueArray, 
							  selftestValueArraySize * sizeof( SELFTEST_VALUE ) ) );

	REQUIRES_B( selftestValueArraySize >= 1 && selftestValueArraySize < 50 );
	REQUIRES_B( isEnumRange( op, BN_OP ) );

	LOOP_LARGE( i = 0, selftestValueArray[ i ].a != NULL && \
					   i < selftestValueArraySize, i++ )
		{
		if( !selfTestOp( &selftestValueArray[ i ], op, opDescription ) )
			{
			DEBUG_DIAG(( "Failed %s test #%d", opDescription, i ));
			return( FALSE );
			}
		}
	ENSURES_B( LOOP_BOUND_OK );
	ENSURES_B( i < selftestValueArraySize );

	return( TRUE );
	}

CHECK_RETVAL_BOOL \
BOOLEAN bnmathSelfTest( void )
	{
	if( !selfTestBignumFields() )
		{
		DEBUG_DIAG(( "Failed bignum fields test" ));
		return( FALSE );
		}
	if( !selfTestBignumLinkage() )
		{
		DEBUG_DIAG(( "Failed bignum linkage test" ));
		return( FALSE );
		}
	if( !selfTestGeneralOps1() )
		{
		DEBUG_DIAG(( "Failed general operations test phase 1" ));
		return( FALSE );
		}
	if( !selfTestOps( cmpSelftestValues, FS_SIZE( cmpSelftestValues ),
					  BN_OP_CMP, "BN_OP_CMP" ) )
		return( FALSE );
	if( !selfTestOps( cmpSelftestValues, FS_SIZE( cmpSelftestValues ),
					  BN_OP_CMPPART, "BN_OP_CMPPART" ) )
		return( FALSE );
	if( !selfTestGeneralOps2() )
		{
		DEBUG_DIAG(( "Failed general operations test phase 1" ));
		return( FALSE );
		}
	if( !selfTestOps( addsubSelftestValues, FS_SIZE( addsubSelftestValues ),
					  BN_OP_ADD, "BN_OP_ADD" ) )
		return( FALSE );
	if( !selfTestOps( addsubSelftestValues, FS_SIZE( addsubSelftestValues ),
					  BN_OP_SUB, "BN_OP_SUB" ) )
		return( FALSE );
	if( !selfTestOps( shiftSelftestValues, FS_SIZE( shiftSelftestValues ),
					  BN_OP_LSHIFT, "BN_OP_LSHIFT" ) )
		return( FALSE );
	if( !selfTestOps( shiftSelftestValues, FS_SIZE( shiftSelftestValues ),
					  BN_OP_RSHIFT, "BN_OP_RSHIFT" ) )
		return( FALSE );
	if( !selfTestOps( addsubWordsSelftestValues, FS_SIZE( addsubWordsSelftestValues ),
					  BN_OP_ADDWORD, "BN_OP_ADDWORD" ) )
		return( FALSE );
	if( !selfTestOps( addsubWordsSelftestValues, FS_SIZE( addsubWordsSelftestValues ),
					  BN_OP_SUBWORD, "BN_OP_SUBWORD" ) )
		return( FALSE );
	if( !selfTestOps( muldivWordsSelftestValues, FS_SIZE( muldivWordsSelftestValues ),
					  BN_OP_MULWORD, "BN_OP_MULWORD" ) )
		return( FALSE );
	if( !selfTestOps( modWordsSelftestValues, FS_SIZE( modWordsSelftestValues ),
					  BN_OP_MODWORD, "BN_OP_MODWORD" ) )
		return( FALSE );
	if( !selfTestOps( sqrSelftestValues, FS_SIZE( sqrSelftestValues ),
					  BN_OP_SQR, "BN_OP_SQR" ) )
		return( FALSE );
	if( !selfTestOps( divSelftestValues, FS_SIZE( divSelftestValues ),
					  BN_OP_DIV, "BN_OP_DIV" ) )
		return( FALSE );
	if( !selfTestOps( montModMulSelftestValues, FS_SIZE( montModMulSelftestValues ),
					  BN_OP_MONTMODMULT, "BN_OP_MONTMODMULT" ) )
		return( FALSE );
#if defined( USE_ECDH ) || defined( USE_ECDSA )
	if( !selfTestOps( modAddSelftestValues, FS_SIZE( modAddSelftestValues ),
					  BN_OP_MODADD, "BN_OP_MODADD" ) )
		return( FALSE );
	if( !selfTestOps( modSubSelftestValues, FS_SIZE( modSubSelftestValues ),
					  BN_OP_MODSUB, "BN_OP_MODSUB" ) )
		return( FALSE );
	if( !selfTestOps( modMulSelftestValues, FS_SIZE( modMulSelftestValues ),
					  BN_OP_MODMUL, "BN_OP_MODMUL" ) )
		return( FALSE );
	if( !selfTestOps( modShiftSelftestValues, FS_SIZE( modShiftSelftestValues ),
					  BN_OP_MODSHIFT, "BN_OP_MODSHIFT" ) )
		return( FALSE );
#endif /* USE_ECDH || USE_ECDSA */

	return( TRUE );
	}
#endif /* USE_PKC && !CONFIG_CONSERVE_MEMORY_EXTRA */
