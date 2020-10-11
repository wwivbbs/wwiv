/* cryptlib uses some of the mid-level OpenSSL bignum routines, but nothing 
   at either a low or high level.  Because of this it needs an equivalent to
   OpenSSL's bn.h, which this file provides.  The original bn.h is moved to
   bn_orig.h to ensure that the OpenSSL copyright isn't impacted, and is 
   included here by reference */

#ifndef _BN_DEFINED

#define _BN_DEFINED

#if defined( INC_ALL )
  #include "osconfig.h"
#else
  #include "crypt/osconfig.h"
#endif /* Compiler-specific includes */

/****************************************************************************
*																			*
*							Bignum Constants and Flags						*
*																			*
****************************************************************************/

/* The allocation size for the fixed-size bignum data, sized to contain the 
   largest possible bignum in normal use, see the various comments 
   pertaining to bignum _EXT use for more on this.  Worst-case we need to 
   use a base alloc.size of 1024, however by careful use of _EXT bignums only
   where needed we can use only 512 (= CRYPT_MAX_PKCSIZE).
   
	For BIGNUM_BASE_ALLOCSIZE = 1024, BIGNUM_ALLOC_WORDS = 260 32-bit words,
									  BIGNUM_ALLOC_BITS = 8320.
	For BIGNUM_BASE_ALLOCSIZE = 512,  BIGNUM_ALLOC_WORDS = 130 32-bit words,
									  BIGNUM_ALLOC_BITS = 4115 */

#if defined( CONFIG_PKC_ALLOCSIZE )
  #define BIGNUM_BASE_ALLOCSIZE		CONFIG_PKC_ALLOCSIZE
#else
  #define BIGNUM_BASE_ALLOCSIZE		512
  #if defined( CRYPT_MAX_PKCSIZE ) && CRYPT_MAX_PKCSIZE != BIGNUM_BASE_ALLOCSIZE
	#error CRYPT_MAX_PKCSIZE doesnt match BIGNUM_BASE_ALLOCSIZE
  #endif /* CRYPT_MAX_PKCSIZE != 512 */
#endif /* CONFIG_PKC_ALLOCSIZE */
#define BIGNUM_ALLOC_WORDS			( ( BIGNUM_BASE_ALLOCSIZE / BN_BYTES ) + 4 )
#define bnWordsToBytes( bnWords )	( ( bnWords ) * BN_BYTES )
#define bnWordsToBits( bnWords )	( ( bnWords ) * BN_BITS2 )

/* Some of the temporary bignums used in multiplies grow to enormous sizes 
   (see the comment for BN_CTX), to avoid having to make every other bignum 
   as huge we provide a few special-case entries, BIGNUM_EXTs, that are 
   twice/four times the normal size */

#define BIGNUM_ALLOC_WORDS_EXT	( BIGNUM_ALLOC_WORDS * 2 )
#define BIGNUM_ALLOC_WORDS_EXT2	( BIGNUM_ALLOC_WORDS * 4 )

/* Symbolic defines for the extended-size bignum that we want to use */

typedef enum { 
	BIGNUM_EXT_NONE, 
	
	/* Double-size bignums */
	BIGNUM_EXT_MONT, 

	/* Quad-size bignums */
	BIGNUM_EXT_MUL1, 
	BIGNUM_EXT_MUL2, 

	BIGNUM_EXT_LAST 
	} BIGNUM_EXT_TYPE;

/* Bignum flags.  BN_FLG_SCRATCH is set for bignums that aren't actually used
   as bignums but merely as scratch storage for data in the 'd' array, which
   would fail a sanity check as standard bignums */

#define BN_FLG_NONE			0x00	/* No bignum flag */
#define BN_FLG_MALLOCED		0x01	/* Bignum is dynamically allocated */
#define BN_FLG_STATIC_DATA	0x02	/* Bignum is in read-only memory */
#define BN_FLG_CONSTTIME	0x04	/* Bignum ops use const-time algorithms */
#define BN_FLG_SCRATCH		0x08	/* 'd' array used purely as scratch storage */
#define BN_FLG_ALLOC_EXT	0x10	/* Bignum size BIGNUM_EXT_MUL1 */
#define BN_FLG_ALLOC_EXT2	0x20	/* Bignum size BIGNUM_EXT_MUL2 */
#define BN_FLG_MAX			0x3F	/* Maximum possible flag value */

/* When copying a bignum, we don't copy most of the flags since they're 
   specific to the underlying bignum when they're things like 
   BN_FLG_MALLOCED, BN_FLG_STATIC_DATA, BN_FLG_ALLOC_EXT and 
   BN_FLG_ALLOC_EXT2, however there are a few flags that need to be 
   propagated, defined by the following mask value */

#define BN_FLG_COPY_MASK	( BN_FLG_CONSTTIME )

/* Bignum data types and sizes.  These are, as with all things OpenSSL, 
   rather confusing and hard to sort out, with the code doing things in pure 
   assembly language, with inline asm assist, or in pure C.  In addition due 
   to the use of various levels of assembly language there are also 
   different options that control which word size to use depending on what 
   the asm expects, for example whether there are 64 op 64 -> 64 or 64 op 64 
   -> 128 instructions available. 

   So the original OpenSSL code is a complex balancing act where BN_LLONG 
   might be used in some cases (for 32 op 32 -> 64) and native asm or asm 
   assist for pure 32-bit or maybe asm 32 op 32 -> 64.  In particular where 
   the asm requires a particular word size or where the best available 
   option isn't to use the biggest word size, somewhat counterintuitive 
   options are enabled, so Sparc64 uses BN_LLONG and 32 op 32 -> 64 because 
   32 x 32 -> 64 multiplications are the best available even in 64-bit mode, 
   and even 64-bit additions are best done without SIXTY_FOUR_BIT because 
   there's no 64-bit-with-carry instruction.

   In terms of the C-level macros, if possible the code uses BN_LLONG with 
   n op n -> 2n operations (but this is only possible for THIRTY_TWO_BIT 
   when long long is available, for SIXTY_FOUR_BIT you'd need a C-level 128-
   bit data type).  If that isn't possible (i.e. for SIXTY_FOUR_BIT) it 
   falls back to secondary operations with macros like BN_UMULT_LOHI or 
   BN_UMULT_HIGH, which provide asm assist for n op n -> n or register-pair 
   2 * n operations.  Finally, if all else fails, it does it the hard way, 
   building up something like a mul64() from four mul32()s (see the code 
   towards the end of bn/bn_lcl.h for details).

   No idea how the BN_BITS is supposed to work, it's only used in one 
   location in bn_gcd.c where it's used as if it indicates the machine word 
   size, technically it's OK for THIRTY_TWO_BIT because it assumes there's a 
   64-bit BN_ULLONG, but it's definitely wrong for the others.  We keep it 
   as is because the BN_BITSn defines and therefore the OpenSSL code all 
   depend on it */

#ifdef _MSC_VER 
  #define LONGLONG_TYPE		__int64
#else
  #define LONGLONG_TYPE		long long
#endif /* Compiler-specific 64-bit type */

/* LP64 architectures where long and long long are 64 bits, e.g. x86-64 
   Unix, Alpha, PPC64, and Sparc64 */

#ifdef SIXTY_FOUR_BIT_LONG
  #define BN_ULLONG			unsigned LONGLONG_TYPE	/* 64 bits */
  #define BN_ULONG			unsigned long			/* 64 bits */
  #define BN_BITS			128
  #define BN_MASK2			0xFFFFFFFFFFFFFFFFUL
  #define BN_MASK2l			0xFFFFFFFFUL
  #define BN_MASK2h			0xFFFFFFFF00000000UL
#endif /* SIXTY_FOUR_BIT_LONG */

/* LLP64 architectures where long long is 64 bits but long is 32 bits,
   x86-64 Windows.  Since we already need long long just to get 64 bits,
   we can't have BN_ULLONG (and its accompanying flag BN_LLONG).
   
   There is pseudo-support for an __int128 type in VC++ and gcc but it's 
   still too preliminary to rely on */

#ifdef SIXTY_FOUR_BIT
  #define BN_ULONG			unsigned LONGLONG_TYPE	/* 64 bits */
  #define BN_BITS			128
  #define BN_MASK2			0xFFFFFFFFFFFFFFFFULL
  #define BN_MASK2l			0xFFFFFFFFUL
  #define BN_MASK2h			0xFFFFFFFF00000000ULL
#endif /* SIXTY_FOUR_BIT */

/* 32-bit architectures */

#ifdef THIRTY_TWO_BIT
  #ifdef BN_LLONG
	#define BN_ULLONG		unsigned LONGLONG_TYPE	/* 64 bits */
  #endif /* BN_LLONG */
  #define BN_ULONG			unsigned int			/* 32 bits */
  #define BN_BITS			64
  #define BN_MASK2			0xFFFFFFFFUL
  #define BN_MASK2l			0xFFFFUL
  #define BN_MASK2h			0xFFFF0000UL
#endif /* THIRTY_TWO_BIT */

#define BN_BITS2			( BN_BITS / 2 )
#define BN_BITS4			( BN_BITS / 4 )
#define BN_BYTES			( BN_BITS2 / 8 )

/* Special-case NaN return value */

#define BN_NAN				( ( BN_ULONG ) BN_MASK2 )

/* Use double-word divide if available */

#if defined( _MSC_VER ) || defined( __GNUC__ ) || defined( __clang__ )
  #define BN_DIV2W
#endif /* Compilers that support double-word divide */

/* Check that everything is set up as required */

#if defined( SIXTY_FOUR_BIT_LONG )
  #ifdef BN_LLONG
	#error BN_LLONG cant be defined for SIXTY_FOUR_BIT_LONG (LP64) systems
  #endif /* BN_LLONG */
#elif defined( SIXTY_FOUR_BIT )
  #if defined( BN_LLONG ) || defined( BN_ULLONG )
	#error BN_LLONG or BN_ULLONG cant be defined for SIXTY_FOUR_BIT (LLP64) systems
  #endif
#elif defined( THIRTY_TWO_BIT )
  /* No special conditions */
#else
  #error Neither SIXTY_FOUR_BIT_LONG, SIXTY_FOUR_BIT or THIRTY_TWO_BIT is defined for this architecture
#endif /* Machine word sizes */

/****************************************************************************
*																			*
*								Bignum Types								*
*																			*
****************************************************************************/

/* Support functions to mask the OpenSSL memory-allocation mess.  bn_expand()
   and bn_wexpand() are a particular pain because they're used in situations 
   like:

	if (bn_wexpand(snum, sdiv->top + 2) == NULL) ...

   Since we don't dynamically reallocate bignums like crazy as OpenSSL does
   but use fixed-size, big-enough values, the only check that we need to do
   is a sanity check that there's enough room.  This requires a complex
   expression that evaluates to NULL if there's insufficient space (a should-
   never-occur condition) and something that isn't NULL if there's enough
   space.

   The whole thing is complicated by the fact that MQX, in line with its
   braindamaged policy of redefining standard C types and values, also 
   redefines NULL (!!), so we need an MQX-specific version to avoid compiler
   errors.  The weird-looking expression has been chosen to generate a
   compile error (rather than breaking silently) if in the future MQX 
   decides to redefine NULL to something less insane */

#if defined( WIN32 ) && !defined( NDEBUG ) && 0
  #define USE_BN_DEBUG_MALLOC
#endif /* Only enable in Win32 debug version */
#ifdef USE_BN_DEBUG_MALLOC
  #define clBnAlloc( string, size ) \
		  clBnAllocFn( __FILE__, string, __LINE__, size )
  void *clBnAllocFn( const char *fileName, const char *fnName,
					 const int lineNo, size_t size );
#else
  #define clBnAlloc( string, size )	malloc( size )
#endif /* !USE_BN_DEBUG_MALLOC */
#define OPENSSL_free				free
#define OPENSSL_cleanse( a, b )		memset( a, 0, b )

extern int nonNullAddress;
#if defined( __MQXRTOS__ ) && !defined( _MSC_VER )
  #define bn_expand( bignum, bits ) \
		  ( ( ( bits ) > BIGNUM_ALLOC_BITS ) ? assert( 0 ), NULL : NULL + 1 )	/* See comment above */
  #define bn_wexpand( bignum, words ) \
		  ( ( ( words ) > getBNMaxSize( bignum ) ) ? assert( 0 ), NULL : NULL + 1 )
#elif defined( NDEBUG )
  #define bn_expand( bignum, bits ) \
		  ( ( ( bits ) > BIGNUM_ALLOC_BITS ) ? NULL : ( void * ) &nonNullAddress )
  #define bn_wexpand( bignum, words ) \
		  ( ( ( words ) > getBNMaxSize( bignum ) ) ? NULL : ( void * ) &nonNullAddress )
#elif defined( __xlc__ )
  /* Some compilers have an assert() defined in such a way that it can't be 
     used as part of the expression in bn_(w)expand(), so we have to use the
	 basic non-debug form */
  #define bn_expand( bignum, bits ) \
		  ( ( ( bits ) > BIGNUM_ALLOC_BITS ) ? NULL : ( void * ) &nonNullAddress )
  #define bn_wexpand( bignum, words ) \
		  ( ( ( words ) > getBNMaxSize( bignum ) ) ? NULL : ( void * ) &nonNullAddress )
#else
  #define bn_expand( bignum, bits ) \
		  ( ( ( bits ) > BIGNUM_ALLOC_BITS ) ? assert( 0 ), NULL : ( void * ) &nonNullAddress )
  #define bn_wexpand( bignum, words ) \
		  ( ( ( words ) > getBNMaxSize( bignum ) ) ? assert( 0 ), NULL : ( void * ) &nonNullAddress )
#endif /* Situation-specific override macros */

/* Overrides of OpenSSL functions that are rendered unnecessary by our 
   versions */

#define BN_clear_free			BN_free
#define BN_cmp					BN_ucmp
#define BN_is_zero( bignum )	( !BN_cmp_word( bignum, 0 ) )
#define BN_is_one( bignum )		( !BN_cmp_word( bignum, 1 ) )
#define BN_is_word( bignum, word ) ( !BN_cmp_word( bignum, word ) )
#define BN_is_odd( bignum )		BN_is_bit_set( bignum, 0 )
#define BN_zero( bignum )		BN_set_word( bignum, 0 )
#define BN_one( bignum )		BN_set_word( bignum, 1 )
#define BN_num_bytes( bignum )	( ( BN_num_bits( bignum ) + 7 ) / 8 )
#define BN_lshift1( result, a )	BN_lshift( result, a, 1 )
#define BN_rshift1( result, a )	BN_rshift( result, a, 1 )
#define BN_mod( remainder, modulus, divisor, ctx ) \
								BN_div( NULL, remainder, modulus, divisor, ctx )
#define BN_mod_add( result, a, b, modulus, ctx ) \
								BN_mod_add_quick( result, a, b, modulus )
#define BN_mod_lshift1_quick( result, a, modulus ) \
								BN_mod_lshift_quick( result, a, 1, modulus )
#define BN_MONT_CTX_copy( destMontCTX, srcMontCTX ) \
								memcpy( destMontCTX, srcMontCTX, sizeof( BN_MONT_CTX ) )
#define BN_set_flags( bignum, flagValue ) \
								( ( bignum )->flags |= ( flagValue ) )
#define BN_get_flags( bignum, flagValue ) \
								( ( bignum )->flags & ( flagValue ) )
#define BN_is_negative( bignum ) ( ( bignum )->neg )
#define bn_fix_top				BN_normalise
#define bn_correct_top			BN_normalise
#define BNerr( a, b )
#define bn_check_top( a )

/* Get a copy of a bignum with different flags from the original, used for 
   constant-time ops in bn_gcd() to create a read-only copy of a bignum 
   that's processed using a constant-time algorithm.  This really shouldn't
   be called BN_with_flags() but it's necessary for compatibility with the
   OpenSSL original */

#define BN_with_flags( dest, src, flagValue ) \
		memcpy( dest, src, sizeof( BIGNUM ) ); \
		( dest )->flags = ( ( src )->flags & ~BN_FLG_MALLOCED ) | BN_FLG_STATIC_DATA | ( flagValue )

/* Bignum structures.  The storage array d must be at the end since the 
   basic BIGNUM can be overlaid with the _EXT/_EXT2 forms where larger 
   data quantities are used */

typedef struct {
	int top;		/* Last used word in d[] + 1 */
	int neg;		/* Negative true/false */
	int flags;		/* Flags */
	BN_ULONG d[ BIGNUM_ALLOC_WORDS + 4 ];
	} BIGNUM;

typedef struct {
	int top, neg, flags;
	BN_ULONG d[ BIGNUM_ALLOC_WORDS_EXT + 4 ];
	} BIGNUM_EXT;

typedef struct {
	int top, neg, flags;
	BN_ULONG d[ BIGNUM_ALLOC_WORDS_EXT2 + 4 ];
	} BIGNUM_EXT2;

/* Montgomery modmult structure */

typedef struct {
	BIGNUM R;		/* Used when converting to Montgomery form aR mod N */
	BIGNUM N;		/* Modulus */
	BN_ULONG n0;	/* Low word of some intermediate value from
					   BN_MONT_CTX_set() */
	int flags;
	} BN_MONT_CTX;

/* Reciprocal devision/modmult structure */

typedef struct {
	BIGNUM N;		/* Divisor */
	BIGNUM Nr;		/* Reciprocal */
	int num_bits;	/* Bits in divisor */
	int shift;		/* Whether Nr has been set from N */
	int flags;
	} BN_RECP_CTX;

/* The purpose, and much of the functioning, of the BN_CTX pool/stack/
   whatever stuff is more or less incomprehensible, and in any case all 
   of the manipulations actually have a negative effect on memory use 
   because the bookkeeping overhead of dozens of tiny little allocations 
   is more than just allocating a fixed-size block of bignums, which is
   what we do instead of using the OpenSSL forms */

#define BN_CTX_ARRAY_SIZE		40
#define BN_CTX_EXTARRAY_SIZE	1
#define BN_CTX_EXT2ARRAY_SIZE	2

typedef struct {
	/* The bignum array and last-used position in the array */
	BIGNUM bnArray[ BN_CTX_ARRAY_SIZE ];
	int bnArrayMax;

	/* Special-case bignums used to handle two temporary values in BN_mul()
	   that grow to an enormous size, and one temporary value in 
	   BN_mod_mul_montgomery() that briefly reaches an enormous size */
	BIGNUM_EXT bnExtArray[ BN_CTX_EXTARRAY_SIZE ];
	BIGNUM_EXT2 bnExt2Array[ BN_CTX_EXT2ARRAY_SIZE ];

	/* The bignum stack and current position in the stack.  See the long
	   explanation in ctx_bn.c for what this is used for */
	int stack[ BN_CTX_ARRAY_SIZE ];
	int stackPos;
	} BN_CTX;

/****************************************************************************
*																			*
*								Bignum Functions							*
*																			*
****************************************************************************/

/* Enable various build options */

#define BN_MUL_COMBA
#define BN_SQR_COMBA
#define BN_RECURSION

/* Bignum internal/helper functions */

CHECK_RETVAL_LENGTH_SHORT_NOERROR STDC_NONNULL_ARG( ( 1 ) ) \
int getBNMaxSize( const BIGNUM *bignum );

/* Bignum init/shutdown/copy/swap functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_clear( INOUT BIGNUM *bignum );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_init( OUT BIGNUM *bignum );
CHECK_RETVAL_PTR \
BIGNUM *BN_new( void );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_free( INOUT BIGNUM *bignum );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_dup( const BIGNUM *bignum );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 2 ) ) \
BIGNUM *BN_copy( INOUT BIGNUM *destBignum, const BIGNUM *srcBignum );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
void BN_swap( INOUT BIGNUM *bignum1, INOUT BIGNUM *bignum2 );
CHECK_RETVAL_PTR \
const BIGNUM *BN_value_one( void );

/* Bignum bit/word functions */

STDC_NONNULL_ARG( ( 1 ) ) \
BN_ULONG BN_get_word( const BIGNUM *bignum );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_set_word( INOUT BIGNUM *bignum, const BN_ULONG word );
CHECK_RETVAL_LENGTH_SHORT \
int BN_num_bits_word( const BN_ULONG word );
CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1 ) ) \
int BN_num_bits( const BIGNUM *bignum );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_set_bit( INOUT BIGNUM *bignum, 
					IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						int bitNo );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_is_bit_set( const BIGNUM *bignum, /* See comment */ int bitNo );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_high_bit( const BIGNUM *bignum );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_set_negative( INOUT BIGNUM *bignum, const int value );
RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_normalise( INOUT BIGNUM *bignum );
RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_clear_top( INOUT BIGNUM *bignum, 
					  IN_RANGE( 0, BIGNUM_ALLOC_WORDS_EXT2 ) const int oldTop );

/* Bignum context init/shutdown functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_init( OUT BN_CTX *bnCTX );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_start( INOUT BN_CTX *bnCTX );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_end( INOUT BN_CTX *bnCTX );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_final( INOUT BN_CTX *bnCTX );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_CTX_get( INOUT BN_CTX *bnCTX );
CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1 ) ) \
BIGNUM *BN_CTX_get_ext( INOUT BN_CTX *bnCTX, 
						IN_ENUM( BIGNUM_EXT ) const BIGNUM_EXT_TYPE bnExtType );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_end_ext( INOUT BN_CTX *bnCTX, 
					 IN_ENUM( BIGNUM_EXT ) const BIGNUM_EXT_TYPE bnExtType );
CHECK_RETVAL_PTR \
BN_CTX *BN_CTX_new( void );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_CTX_free( INOUT BN_CTX *bnCTX );

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_RECP_CTX_init( OUT BN_RECP_CTX *bnRecpCTX );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_RECP_CTX_free( INOUT BN_RECP_CTX *bnRecpCTX );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_RECP_CTX_set( INOUT BN_RECP_CTX *bnRecpCTX, const BIGNUM *d, 
						 STDC_UNUSED const BN_CTX *bnCTX );

/* Bignum Montgomery init/shutdown functions */

STDC_NONNULL_ARG( ( 1 ) ) \
void BN_MONT_CTX_init( OUT BN_MONT_CTX *bnMontCTX );
STDC_NONNULL_ARG( ( 1 ) ) \
void BN_MONT_CTX_free( INOUT BN_MONT_CTX *bnMontCTX );
CHECK_RETVAL_PTR \
BN_MONT_CTX *BN_MONT_CTX_new( void );

/* Bignum maths functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_uadd( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_usub( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_add( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_sub( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_lshift( INOUT BIGNUM *r, const BIGNUM *a, 
				   IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						const int shiftAmount );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_rshift( INOUT BIGNUM *r, const BIGNUM *a, 
				   IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
						const int shiftAmount );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_add_word( INOUT BIGNUM *a, const BN_ULONG w );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_sub_word( INOUT BIGNUM *a, const BN_ULONG w );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1 ) ) \
BOOLEAN BN_mul_word( INOUT BIGNUM *a, const BN_ULONG w );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2 ) ) \
BOOLEAN BN_mod_word( OUT BN_ULONG *r, const BIGNUM *a, const BN_ULONG w );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_sqr( INOUT BIGNUM *r, const BIGNUM *a, INOUT BN_CTX *bnCTX );

/* Bignum division/modulus functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 3, 4, 5 ) ) \
BOOLEAN BN_div( INOUT_OPT BIGNUM *quotient, INOUT_OPT BIGNUM *remainder, 
				const BIGNUM *numerator, const BIGNUM *divisor,
				INOUT BN_CTX *bnCTX );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_nnmod( INOUT BIGNUM *r, const BIGNUM *m, const BIGNUM *d, 
				  INOUT BN_CTX *ctx );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_add_quick( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
						  const BIGNUM *m );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_sub_quick( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
						  const BIGNUM *m );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
BOOLEAN BN_mod_mul( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *b,
					const BIGNUM *m, INOUT BN_CTX *ctx );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_mod_sqr( INOUT BIGNUM *r, const BIGNUM *a, const BIGNUM *m, 
					INOUT BN_CTX *ctx );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 4 ) ) \
BOOLEAN BN_mod_lshift_quick( BIGNUM *r, const BIGNUM *a, 
							 IN_RANGE( 0, bytesToBits( CRYPT_MAX_PKCSIZE ) ) \
								const int shiftAmount,
							 const BIGNUM *m );

/* Bignum Montgomery functions */

CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3 ) ) \
BOOLEAN BN_MONT_CTX_set( INOUT BN_MONT_CTX *bnMontCTX, const BIGNUM *mod, 
						 INOUT BN_CTX *bnCTX );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_to_montgomery( INOUT BIGNUM *ret, const BIGNUM *a,
						  const BN_MONT_CTX *bnMontCTX,
						  INOUT BN_CTX *bnCTX );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4, 5 ) ) \
BOOLEAN BN_mod_mul_montgomery( INOUT BIGNUM *r, const BIGNUM *a, 
							   const BIGNUM *b, const BN_MONT_CTX *bnMontCTX, 
							   INOUT BN_CTX *bnCTX );
CHECK_RETVAL_BOOL STDC_NONNULL_ARG( ( 1, 2, 3, 4 ) ) \
BOOLEAN BN_from_montgomery( INOUT BIGNUM *ret, INOUT BIGNUM *aTmp,
							const BN_MONT_CTX *bnMontCTX,
							INOUT BN_CTX *bnCTX );

/* Bignum compare functions */

STDC_NONNULL_ARG( ( 1 ) ) \
int BN_cmp_word( const BIGNUM *bignum, const BN_ULONG word );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int bn_cmp_words( const BN_ULONG *bignumData1, const BN_ULONG *bignumData2, 
				  IN_RANGE( 0, BIGNUM_ALLOC_WORDS ) const int length );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int BN_ucmp( const BIGNUM *bignum1, const BIGNUM *bignum2 );
STDC_NONNULL_ARG( ( 1, 3 ) ) \
int BN_ucmp_words( const BN_ULONG *bignumData1, 
				   IN_RANGE( 0, BIGNUM_ALLOC_WORDS ) const int bignum1Length, 
				   const BIGNUM *bignum2 );
STDC_NONNULL_ARG( ( 1, 2 ) ) \
int bn_cmp_part_words( const BN_ULONG *a, const BN_ULONG *b, 
					   IN_RANGE( 0, BIGNUM_ALLOC_WORDS_EXT ) const int cl, 
					   IN_RANGE( -BIGNUM_ALLOC_WORDS_EXT, \
								 BIGNUM_ALLOC_WORDS_EXT ) const int dl );

/* Bignum read/write functions */

CHECK_RETVAL_PTR STDC_NONNULL_ARG( ( 1, 3 ) ) \
BIGNUM *BN_bin2bn( IN_BUFFER( length ) const BYTE *buffer, 
				   IN_LENGTH_PKC_Z const int length, 
				   OUT BIGNUM *bignum );
CHECK_RETVAL_LENGTH_SHORT STDC_NONNULL_ARG( ( 1, 2 ) ) \
int BN_bn2bin( const BIGNUM *bignum, BYTE *buffer );

/****************************************************************************
*																			*
*						Remaining Original OpenSSL Functions				*
*																			*
****************************************************************************/

#if defined( INC_ALL )
  #include "bn_orig.h"
#else
  #include "bn/bn_orig.h"
#endif /* Compiler-specific includes */

#endif /* _BN_DEFINED */
