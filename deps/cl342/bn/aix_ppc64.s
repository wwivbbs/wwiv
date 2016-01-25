#--------------------------------------------------------------------
#
#
#
#
#	File:		ppc32.s
#
#	Created by:	Suresh Chari
#			IBM Thomas J. Watson Research Library
#			Hawthorne, NY
#
#
#	Description:	Optimized assembly routines for OpenSSL crypto
#			on the 32 bitPowerPC platform.
#
#
#	Version History
#
#	2. Fixed bn_add,bn_sub and bn_div_words, added comments,
#	   cleaned up code. Also made a single version which can
#	   be used for both the AIX and Linux compilers. See NOTE
#	   below.
#				12/05/03		Suresh Chari
#			(with lots of help from)        Andy Polyakov
##	
#	1. Initial version	10/20/02		Suresh Chari
#
#
#	The following file works for the xlc,cc
#	and gcc compilers.
#
#	NOTE:	To get the file to link correctly with the gcc compiler
#	        you have to change the names of the routines and remove
#		the first .(dot) character. This should automatically
#		be done in the build process.
#
#	Hand optimized assembly code for the following routines
#	
#	bn_sqr_comba4
#	bn_sqr_comba8
#	bn_mul_comba4
#	bn_mul_comba8
#	bn_sub_words
#	bn_add_words
#	bn_div_words
#	bn_sqr_words
#	bn_mul_words
#	bn_mul_add_words
#
#	NOTE:	It is possible to optimize this code more for
#	specific PowerPC or Power architectures. On the Northstar
#	architecture the optimizations in this file do
#	 NOT provide much improvement.
#
#	If you have comments or suggestions to improve code send
#	me a note at schari@us.ibm.com
#
#--------------------------------------------------------------------------
#
#	Defines to be used in the assembly code.
#	
.set r0,0	# we use it as storage for value of 0
.set SP,1	# preserved
.set RTOC,2	# preserved 
.set r3,3	# 1st argument/return value
.set r4,4	# 2nd argument/volatile register
.set r5,5	# 3rd argument/volatile register
.set r6,6	# ...
.set r7,7
.set r8,8
.set r9,9
.set r10,10
.set r11,11
.set r12,12
.set r13,13	# not used, nor any other "below" it...

.set BO_IF_NOT,4
.set BO_IF,12
.set BO_dCTR_NZERO,16
.set BO_dCTR_ZERO,18
.set BO_ALWAYS,20
.set CR0_LT,0;
.set CR0_GT,1;
.set CR0_EQ,2
.set CR1_FX,4;
.set CR1_FEX,5;
.set CR1_VX,6
.set LR,8

#	Declare function names to be global
#	NOTE:	For gcc these names MUST be changed to remove
#	        the first . i.e. for example change ".bn_sqr_comba4"
#		to "bn_sqr_comba4". This should be automatically done
#		in the build.
	
	.globl	.bn_sqr_comba4
	.globl	.bn_sqr_comba8
	.globl	.bn_mul_comba4
	.globl	.bn_mul_comba8
	.globl	.bn_sub_words
	.globl	.bn_add_words
	.globl	.bn_div_words
	.globl	.bn_sqr_words
	.globl	.bn_mul_words
	.globl	.bn_mul_add_words
	
# .text section
	
	.machine	"ppc64"

#
#	NOTE:	The following label name should be changed to
#		"bn_sqr_comba4" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4
.bn_sqr_comba4:
#
# Optimized version of bn_sqr_comba4.
#
# void bn_sqr_comba4(BN_ULONG *r, BN_ULONG *a)
# r3 contains r
# r4 contains a
#
# Freely use registers r5,r6,r7,r8,r9,r10,r11 as follows:	
# 
# r5,r6 are the two BN_ULONGs being multiplied.
# r7,r8 are the results of the 32x32 giving 64 bit multiply.
# r9,r10, r11 are the equivalents of c1,c2, c3.
# Here's the assembly
#
#
	xor		r0,r0,r0		# set r0 = 0. Used in the addze
						# instructions below
	
						#sqr_add_c(a,0,c1,c2,c3)
	ld		r5,0(r4)		
	mulld		r9,r5,r5		
	mulhdu		r10,r5,r5		#in first iteration. No need
						#to add since c1=c2=c3=0.
						# Note c3(r11) is NOT set to 0
						# but will be.

	std		r9,0(r3)	# r[0]=c1;
						# sqr_add_c2(a,1,0,c2,c3,c1);
	ld		r6,8(r4)		
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
					
	addc		r7,r7,r7		# compute (r7,r8)=2*(r7,r8)
	adde		r8,r8,r8
	addze		r9,r0			# catch carry if any.
						# r9= r0(=0) and carry 
	
	addc		r10,r7,r10		# now add to temp result.
	addze		r11,r8                  # r8 added to r11 which is 0 
	addze		r9,r9
	
	std		r10,8(r3)	#r[1]=c2; 
						#sqr_add_c(a,1,c3,c1,c2)
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r0
						#sqr_add_c2(a,2,0,c3,c1,c2)
	ld		r6,16(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r7,r7,r7
	adde		r8,r8,r8
	addze		r10,r10
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	std		r11,16(r3)	#r[2]=c3 
						#sqr_add_c2(a,3,0,c1,c2,c3);
	ld		r6,24(r4)		
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r7,r7,r7
	adde		r8,r8,r8
	addze		r11,r0
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,2,1,c1,c2,c3);
	ld		r5,8(r4)
	ld		r6,16(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r7,r7,r7
	adde		r8,r8,r8
	addze		r11,r11
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	std		r9,24(r3)	#r[3]=c1
						#sqr_add_c(a,2,c2,c3,c1);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r0
						#sqr_add_c2(a,3,1,c2,c3,c1);
	ld		r6,24(r4)		
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r7,r7,r7
	adde		r8,r8,r8
	addze		r9,r9
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	std		r10,32(r3)	#r[4]=c2
						#sqr_add_c2(a,3,2,c3,c1,c2);
	ld		r5,16(r4)		
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r7,r7,r7
	adde		r8,r8,r8
	addze		r10,r0
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	std		r11,40(r3)	#r[5] = c3
						#sqr_add_c(a,3,c1,c2,c3);
	mulld		r7,r6,r6		
	mulhdu		r8,r6,r6
	addc		r9,r7,r9
	adde		r10,r8,r10

	std		r9,48(r3)	#r[6]=c1
	std		r10,56(r3)	#r[7]=c2
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_sqr_comba8" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#
	
.align	4
.bn_sqr_comba8:
#
# This is an optimized version of the bn_sqr_comba8 routine.
# Tightly uses the adde instruction
#
#
# void bn_sqr_comba8(BN_ULONG *r, BN_ULONG *a)
# r3 contains r
# r4 contains a
#
# Freely use registers r5,r6,r7,r8,r9,r10,r11 as follows:	
# 
# r5,r6 are the two BN_ULONGs being multiplied.
# r7,r8 are the results of the 32x32 giving 64 bit multiply.
# r9,r10, r11 are the equivalents of c1,c2, c3.
#
# Possible optimization of loading all 8 longs of a into registers
# doesnt provide any speedup
# 

	xor		r0,r0,r0		#set r0 = 0.Used in addze
						#instructions below.

						#sqr_add_c(a,0,c1,c2,c3);
	ld		r5,0(r4)
	mulld		r9,r5,r5		#1st iteration:	no carries.
	mulhdu		r10,r5,r5
	std		r9,0(r3)	# r[0]=c1;
						#sqr_add_c2(a,1,0,c2,c3,c1);
	ld		r6,8(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6	
	
	addc		r10,r7,r10		#add the two register number
	adde		r11,r8,r0 		# (r8,r7) to the three register
	addze		r9,r0			# number (r9,r11,r10).NOTE:r0=0
	
	addc		r10,r7,r10		#add the two register number
	adde		r11,r8,r11 		# (r8,r7) to the three register
	addze		r9,r9			# number (r9,r11,r10).
	
	std		r10,8(r3)	# r[1]=c2
				
						#sqr_add_c(a,1,c3,c1,c2);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r0
						#sqr_add_c2(a,2,0,c3,c1,c2);
	ld		r6,16(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	
	std		r11,16(r3)	#r[2]=c3
						#sqr_add_c2(a,3,0,c1,c2,c3);
	ld		r6,24(r4)	#r6 = a[3]. r5 is already a[0].
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r0
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,2,1,c1,c2,c3);
	ld		r5,8(r4)
	ld		r6,16(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	
	std		r9,24(r3)	#r[3]=c1;
						#sqr_add_c(a,2,c2,c3,c1);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r0
						#sqr_add_c2(a,3,1,c2,c3,c1);
	ld		r6,24(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
						#sqr_add_c2(a,4,0,c2,c3,c1);
	ld		r5,0(r4)
	ld		r6,32(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	std		r10,32(r3)	#r[4]=c2;
						#sqr_add_c2(a,5,0,c3,c1,c2);
	ld		r6,40(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r0
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
						#sqr_add_c2(a,4,1,c3,c1,c2);
	ld		r5,8(r4)
	ld		r6,32(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
						#sqr_add_c2(a,3,2,c3,c1,c2);
	ld		r5,16(r4)
	ld		r6,24(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	std		r11,40(r3)	#r[5]=c3;
						#sqr_add_c(a,3,c1,c2,c3);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r0
						#sqr_add_c2(a,4,2,c1,c2,c3);
	ld		r6,32(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,5,1,c1,c2,c3);
	ld		r5,8(r4)
	ld		r6,40(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,6,0,c1,c2,c3);
	ld		r5,0(r4)
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	std		r9,48(r3)	#r[6]=c1;
						#sqr_add_c2(a,7,0,c2,c3,c1);
	ld		r6,56(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r0
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
						#sqr_add_c2(a,6,1,c2,c3,c1);
	ld		r5,8(r4)
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
						#sqr_add_c2(a,5,2,c2,c3,c1);
	ld		r5,16(r4)
	ld		r6,40(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
						#sqr_add_c2(a,4,3,c2,c3,c1);
	ld		r5,24(r4)
	ld		r6,32(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	std		r10,56(r3)	#r[7]=c2;
						#sqr_add_c(a,4,c3,c1,c2);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r0
						#sqr_add_c2(a,5,3,c3,c1,c2);
	ld		r6,40(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
						#sqr_add_c2(a,6,2,c3,c1,c2);
	ld		r5,16(r4)
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
						#sqr_add_c2(a,7,1,c3,c1,c2);
	ld		r5,8(r4)
	ld		r6,56(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	std		r11,64(r3)	#r[8]=c3;
						#sqr_add_c2(a,7,2,c1,c2,c3);
	ld		r5,16(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r0
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,6,3,c1,c2,c3);
	ld		r5,24(r4)
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
						#sqr_add_c2(a,5,4,c1,c2,c3);
	ld		r5,32(r4)
	ld		r6,40(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	std		r9,72(r3)	#r[9]=c1;
						#sqr_add_c(a,5,c2,c3,c1);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r0
						#sqr_add_c2(a,6,4,c2,c3,c1);
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
						#sqr_add_c2(a,7,3,c2,c3,c1);
	ld		r5,24(r4)
	ld		r6,56(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	std		r10,80(r3)	#r[10]=c2;
						#sqr_add_c2(a,7,4,c3,c1,c2);
	ld		r5,32(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r0
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
						#sqr_add_c2(a,6,5,c3,c1,c2);
	ld		r5,40(r4)
	ld		r6,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	addc		r11,r7,r11
	adde		r9,r8,r9
	addze		r10,r10
	std		r11,88(r3)	#r[11]=c3;
						#sqr_add_c(a,6,c1,c2,c3);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r0
						#sqr_add_c2(a,7,5,c1,c2,c3)
	ld		r6,56(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	addc		r9,r7,r9
	adde		r10,r8,r10
	addze		r11,r11
	std		r9,96(r3)	#r[12]=c1;
	
						#sqr_add_c2(a,7,6,c2,c3,c1)
	ld		r5,48(r4)
	mulld		r7,r5,r6
	mulhdu		r8,r5,r6
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r0
	addc		r10,r7,r10
	adde		r11,r8,r11
	addze		r9,r9
	std		r10,104(r3)	#r[13]=c2;
						#sqr_add_c(a,7,c3,c1,c2);
	mulld		r7,r6,r6
	mulhdu		r8,r6,r6
	addc		r11,r7,r11
	adde		r9,r8,r9
	std		r11,112(r3)	#r[14]=c3;
	std		r9, 120(r3)	#r[15]=c1;


	bclr	BO_ALWAYS,CR0_LT

	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_mul_comba4" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4
.bn_mul_comba4:
#
# This is an optimized version of the bn_mul_comba4 routine.
#
# void bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
# r3 contains r
# r4 contains a
# r5 contains b
# r6, r7 are the 2 BN_ULONGs being multiplied.
# r8, r9 are the results of the 32x32 giving 64 multiply.
# r10, r11, r12 are the equivalents of c1, c2, and c3.
#
	xor	r0,r0,r0		#r0=0. Used in addze below.
					#mul_add_c(a[0],b[0],c1,c2,c3);
	ld	r6,0(r4)		
	ld	r7,0(r5)		
	mulld	r10,r6,r7		
	mulhdu	r11,r6,r7		
	std	r10,0(r3)	#r[0]=c1
					#mul_add_c(a[0],b[1],c2,c3,c1);
	ld	r7,8(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r8,r11
	adde	r12,r9,r0
	addze	r10,r0
					#mul_add_c(a[1],b[0],c2,c3,c1);
	ld	r6, 8(r4)		
	ld	r7, 0(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r8,r11
	adde	r12,r9,r12
	addze	r10,r10
	std	r11,8(r3)	#r[1]=c2
					#mul_add_c(a[2],b[0],c3,c1,c2);
	ld	r6,16(r4)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r8,r12
	adde	r10,r9,r10
	addze	r11,r0
					#mul_add_c(a[1],b[1],c3,c1,c2);
	ld	r6,8(r4)		
	ld	r7,8(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r8,r12
	adde	r10,r9,r10
	addze	r11,r11
					#mul_add_c(a[0],b[2],c3,c1,c2);
	ld	r6,0(r4)		
	ld	r7,16(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r8,r12
	adde	r10,r9,r10
	addze	r11,r11
	std	r12,16(r3)	#r[2]=c3
					#mul_add_c(a[0],b[3],c1,c2,c3);
	ld	r7,24(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r8,r10
	adde	r11,r9,r11
	addze	r12,r0
					#mul_add_c(a[1],b[2],c1,c2,c3);
	ld	r6,8(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r8,r10
	adde	r11,r9,r11
	addze	r12,r12
					#mul_add_c(a[2],b[1],c1,c2,c3);
	ld	r6,16(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r8,r10
	adde	r11,r9,r11
	addze	r12,r12
					#mul_add_c(a[3],b[0],c1,c2,c3);
	ld	r6,24(r4)
	ld	r7,0(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r8,r10
	adde	r11,r9,r11
	addze	r12,r12
	std	r10,24(r3)	#r[3]=c1
					#mul_add_c(a[3],b[1],c2,c3,c1);
	ld	r7,8(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r8,r11
	adde	r12,r9,r12
	addze	r10,r0
					#mul_add_c(a[2],b[2],c2,c3,c1);
	ld	r6,16(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r8,r11
	adde	r12,r9,r12
	addze	r10,r10
					#mul_add_c(a[1],b[3],c2,c3,c1);
	ld	r6,8(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r8,r11
	adde	r12,r9,r12
	addze	r10,r10
	std	r11,32(r3)	#r[4]=c2
					#mul_add_c(a[2],b[3],c3,c1,c2);
	ld	r6,16(r4)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r8,r12
	adde	r10,r9,r10
	addze	r11,r0
					#mul_add_c(a[3],b[2],c3,c1,c2);
	ld	r6,24(r4)
	ld	r7,16(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r8,r12
	adde	r10,r9,r10
	addze	r11,r11
	std	r12,40(r3)	#r[5]=c3
					#mul_add_c(a[3],b[3],c1,c2,c3);
	ld	r7,24(r5)		
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r8,r10
	adde	r11,r9,r11

	std	r10,48(r3)	#r[6]=c1
	std	r11,56(r3)	#r[7]=c2
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_mul_comba8" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#
	
.align	4
.bn_mul_comba8:
#
# Optimized version of the bn_mul_comba8 routine.
#
# void bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
# r3 contains r
# r4 contains a
# r5 contains b
# r6, r7 are the 2 BN_ULONGs being multiplied.
# r8, r9 are the results of the 32x32 giving 64 multiply.
# r10, r11, r12 are the equivalents of c1, c2, and c3.
#
	xor	r0,r0,r0		#r0=0. Used in addze below.
	
					#mul_add_c(a[0],b[0],c1,c2,c3);
	ld	r6,0(r4)	#a[0]
	ld	r7,0(r5)	#b[0]
	mulld	r10,r6,r7
	mulhdu	r11,r6,r7
	std	r10,0(r3)	#r[0]=c1;
					#mul_add_c(a[0],b[1],c2,c3,c1);
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	addze	r12,r9			# since we didnt set r12 to zero before.
	addze	r10,r0
					#mul_add_c(a[1],b[0],c2,c3,c1);
	ld	r6,8(r4)
	ld	r7,0(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
	std	r11,8(r3)	#r[1]=c2;
					#mul_add_c(a[2],b[0],c3,c1,c2);
	ld	r6,16(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r0
					#mul_add_c(a[1],b[1],c3,c1,c2);
	ld	r6,8(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[0],b[2],c3,c1,c2);
	ld	r6,0(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
	std	r12,16(r3)	#r[2]=c3;
					#mul_add_c(a[0],b[3],c1,c2,c3);
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r0
					#mul_add_c(a[1],b[2],c1,c2,c3);
	ld	r6,8(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
		
					#mul_add_c(a[2],b[1],c1,c2,c3);
	ld	r6,16(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[3],b[0],c1,c2,c3);
	ld	r6,24(r4)
	ld	r7,0(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
	std	r10,24(r3)	#r[3]=c1;
					#mul_add_c(a[4],b[0],c2,c3,c1);
	ld	r6,32(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r0
					#mul_add_c(a[3],b[1],c2,c3,c1);
	ld	r6,24(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[2],b[2],c2,c3,c1);
	ld	r6,16(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[1],b[3],c2,c3,c1);
	ld	r6,8(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[0],b[4],c2,c3,c1);
	ld	r6,0(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
	std	r11,32(r3)	#r[4]=c2;
					#mul_add_c(a[0],b[5],c3,c1,c2);
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r0
					#mul_add_c(a[1],b[4],c3,c1,c2);
	ld	r6,8(r4)		
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[2],b[3],c3,c1,c2);
	ld	r6,16(r4)		
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[3],b[2],c3,c1,c2);
	ld	r6,24(r4)		
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[4],b[1],c3,c1,c2);
	ld	r6,32(r4)		
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[5],b[0],c3,c1,c2);
	ld	r6,40(r4)		
	ld	r7,0(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
	std	r12,40(r3)	#r[5]=c3;
					#mul_add_c(a[6],b[0],c1,c2,c3);
	ld	r6,48(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r0
					#mul_add_c(a[5],b[1],c1,c2,c3);
	ld	r6,40(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[4],b[2],c1,c2,c3);
	ld	r6,32(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[3],b[3],c1,c2,c3);
	ld	r6,24(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[2],b[4],c1,c2,c3);
	ld	r6,16(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[1],b[5],c1,c2,c3);
	ld	r6,8(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[0],b[6],c1,c2,c3);
	ld	r6,0(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
	std	r10,48(r3)	#r[6]=c1;
					#mul_add_c(a[0],b[7],c2,c3,c1);
	ld	r7,56(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r0
					#mul_add_c(a[1],b[6],c2,c3,c1);
	ld	r6,8(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[2],b[5],c2,c3,c1);
	ld	r6,16(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[3],b[4],c2,c3,c1);
	ld	r6,24(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[4],b[3],c2,c3,c1);
	ld	r6,32(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[5],b[2],c2,c3,c1);
	ld	r6,40(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[6],b[1],c2,c3,c1);
	ld	r6,48(r4)
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[7],b[0],c2,c3,c1);
	ld	r6,56(r4)
	ld	r7,0(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
	std	r11,56(r3)	#r[7]=c2;
					#mul_add_c(a[7],b[1],c3,c1,c2);
	ld	r7,8(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r0
					#mul_add_c(a[6],b[2],c3,c1,c2);
	ld	r6,48(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[5],b[3],c3,c1,c2);
	ld	r6,40(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[4],b[4],c3,c1,c2);
	ld	r6,32(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[3],b[5],c3,c1,c2);
	ld	r6,24(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[2],b[6],c3,c1,c2);
	ld	r6,16(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[1],b[7],c3,c1,c2);
	ld	r6,8(r4)
	ld	r7,56(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
	std	r12,64(r3)	#r[8]=c3;
					#mul_add_c(a[2],b[7],c1,c2,c3);
	ld	r6,16(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r0
					#mul_add_c(a[3],b[6],c1,c2,c3);
	ld	r6,24(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[4],b[5],c1,c2,c3);
	ld	r6,32(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[5],b[4],c1,c2,c3);
	ld	r6,40(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[6],b[3],c1,c2,c3);
	ld	r6,48(r4)
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[7],b[2],c1,c2,c3);
	ld	r6,56(r4)
	ld	r7,16(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
	std	r10,72(r3)	#r[9]=c1;
					#mul_add_c(a[7],b[3],c2,c3,c1);
	ld	r7,24(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r0
					#mul_add_c(a[6],b[4],c2,c3,c1);
	ld	r6,48(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[5],b[5],c2,c3,c1);
	ld	r6,40(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[4],b[6],c2,c3,c1);
	ld	r6,32(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
					#mul_add_c(a[3],b[7],c2,c3,c1);
	ld	r6,24(r4)
	ld	r7,56(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
	std	r11,80(r3)	#r[10]=c2;
					#mul_add_c(a[4],b[7],c3,c1,c2);
	ld	r6,32(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r0
					#mul_add_c(a[5],b[6],c3,c1,c2);
	ld	r6,40(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[6],b[5],c3,c1,c2);
	ld	r6,48(r4)
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
					#mul_add_c(a[7],b[4],c3,c1,c2);
	ld	r6,56(r4)
	ld	r7,32(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	addze	r11,r11
	std	r12,88(r3)	#r[11]=c3;
					#mul_add_c(a[7],b[5],c1,c2,c3);
	ld	r7,40(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r0
					#mul_add_c(a[6],b[6],c1,c2,c3);
	ld	r6,48(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
					#mul_add_c(a[5],b[7],c1,c2,c3);
	ld	r6,40(r4)
	ld	r7,56(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r10,r10,r8
	adde	r11,r11,r9
	addze	r12,r12
	std	r10,96(r3)	#r[12]=c1;
					#mul_add_c(a[6],b[7],c2,c3,c1);
	ld	r6,48(r4)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r0
					#mul_add_c(a[7],b[6],c2,c3,c1);
	ld	r6,56(r4)
	ld	r7,48(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r11,r11,r8
	adde	r12,r12,r9
	addze	r10,r10
	std	r11,104(r3)	#r[13]=c2;
					#mul_add_c(a[7],b[7],c3,c1,c2);
	ld	r7,56(r5)
	mulld	r8,r6,r7
	mulhdu	r9,r6,r7
	addc	r12,r12,r8
	adde	r10,r10,r9
	std	r12,112(r3)	#r[14]=c3;
	std	r10,120(r3)	#r[15]=c1;
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_sub_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#
#
.align	4
.bn_sub_words:
#
#	Handcoded version of bn_sub_words
#
#BN_ULONG bn_sub_words(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
#
#	r3 = r
#	r4 = a
#	r5 = b
#	r6 = n
#
#       Note:	No loop unrolling done since this is not a performance
#               critical loop.

	xor	r0,r0,r0	#set r0 = 0
#
#	check for r6 = 0 AND set carry bit.
#
	subfc.	r7,r0,r6        # If r6 is 0 then result is 0.
				# if r6 > 0 then result !=0
				# In either case carry bit is set.
	bc	BO_IF,CR0_EQ,Lppcasm_sub_adios
	addi	r4,r4,-8
	addi	r3,r3,-8
	addi	r5,r5,-8
	mtctr	r6
Lppcasm_sub_mainloop:	
	ldu	r7,8(r4)
	ldu	r8,8(r5)
	subfe	r6,r8,r7	# r6 = r7+carry bit + onescomplement(r8)
				# if carry = 1 this is r7-r8. Else it
				# is r7-r8 -1 as we need.
	stdu	r6,8(r3)
	bc	BO_dCTR_NZERO,CR0_EQ,Lppcasm_sub_mainloop
Lppcasm_sub_adios:	
	subfze	r3,r0		# if carry bit is set then r3 = 0 else -1
	andi.	r3,r3,1         # keep only last bit.
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000


#
#	NOTE:	The following label name should be changed to
#		"bn_add_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4
.bn_add_words:
#
#	Handcoded version of bn_add_words
#
#BN_ULONG bn_add_words(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
#
#	r3 = r
#	r4 = a
#	r5 = b
#	r6 = n
#
#       Note:	No loop unrolling done since this is not a performance
#               critical loop.

	xor	r0,r0,r0
#
#	check for r6 = 0. Is this needed?
#
	addic.	r6,r6,0		#test r6 and clear carry bit.
	bc	BO_IF,CR0_EQ,Lppcasm_add_adios
	addi	r4,r4,-8
	addi	r3,r3,-8
	addi	r5,r5,-8
	mtctr	r6
Lppcasm_add_mainloop:	
	ldu	r7,8(r4)
	ldu	r8,8(r5)
	adde	r8,r7,r8
	stdu	r8,8(r3)
	bc	BO_dCTR_NZERO,CR0_EQ,Lppcasm_add_mainloop
Lppcasm_add_adios:	
	addze	r3,r0			#return carry bit.
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_div_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4
.bn_div_words:
#
#	This is a cleaned up version of code generated by
#	the AIX compiler. The only optimization is to use
#	the PPC instruction to count leading zeros instead
#	of call to num_bits_word. Since this was compiled
#	only at level -O2 we can possibly squeeze it more?
#	
#	r3 = h
#	r4 = l
#	r5 = d
	
	cmpldi	0,r5,0			# compare r5 and 0
	bc	BO_IF_NOT,CR0_EQ,Lppcasm_div1	# proceed if d!=0
	li	r3,-1			# d=0 return -1
	bclr	BO_ALWAYS,CR0_LT	
Lppcasm_div1:
	xor	r0,r0,r0		#r0=0
	li	r8,64
	cntlzd.	r7,r5			#r7 = num leading 0s in d.
	bc	BO_IF,CR0_EQ,Lppcasm_div2	#proceed if no leading zeros
	subf	r8,r7,r8		#r8 = BN_num_bits_word(d)
	srd.	r9,r3,r8		#are there any bits above r8'th?
	td	16,r9,r0		#if there're, signal to dump core...
Lppcasm_div2:
	cmpld	0,r3,r5			#h>=d?
	bc	BO_IF,CR0_LT,Lppcasm_div3	#goto Lppcasm_div3 if not
	subf	r3,r5,r3		#h-=d ; 
Lppcasm_div3:				#r7 = BN_BITS2-i. so r7=i
	cmpi	0,0,r7,0		# is (i == 0)?
	bc	BO_IF,CR0_EQ,Lppcasm_div4
	sld	r3,r3,r7		# h = (h<< i)
	srd	r8,r4,r8		# r8 = (l >> BN_BITS2 -i)
	sld	r5,r5,r7		# d<<=i
	or	r3,r3,r8		# h = (h<<i)|(l>>(BN_BITS2-i))
	sld	r4,r4,r7		# l <<=i
Lppcasm_div4:
	srdi	r9,r5,32		# r9 = dh
					# dl will be computed when needed
					# as it saves registers.
	li	r6,2			#r6=2
	mtctr	r6			#counter will be in count.
Lppcasm_divouterloop: 
	srdi	r8,r3,32		#r8 = (h>>BN_BITS4)
	srdi	r11,r4,32	#r11= (l&BN_MASK2h)>>BN_BITS4
					# compute here for innerloop.
	cmpld	0,r8,r9			# is (h>>BN_BITS4)==dh
	bc	BO_IF_NOT,CR0_EQ,Lppcasm_div5	# goto Lppcasm_div5 if not

	li	r8,-1
	clrldi	r8,r8,32		#q = BN_MASK2l 
	b	Lppcasm_div6
Lppcasm_div5:
	divdu	r8,r3,r9		#q = h/dh
Lppcasm_div6:
	mulld	r12,r9,r8		#th = q*dh
	clrldi	r10,r5,32	#r10=dl
	mulld	r6,r8,r10		#tl = q*dl
	
Lppcasm_divinnerloop:
	subf	r10,r12,r3		#t = h -th
	srdi	r7,r10,32	#r7= (t &BN_MASK2H), sort of...
	addic.	r7,r7,0			#test if r7 == 0. used below.
					# now want to compute
					# r7 = (t<<BN_BITS4)|((l&BN_MASK2h)>>BN_BITS4)
					# the following 2 instructions do that
	sldi	r7,r10,32	# r7 = (t<<BN_BITS4)
	or	r7,r7,r11		# r7|=((l&BN_MASK2h)>>BN_BITS4)
	cmpld	1,r6,r7			# compare (tl <= r7)
	bc	BO_IF_NOT,CR0_EQ,Lppcasm_divinnerexit
	bc	BO_IF_NOT,CR1_FEX,Lppcasm_divinnerexit
	addi	r8,r8,-1		#q--
	subf	r12,r9,r12		#th -=dh
	clrldi	r10,r5,32	#r10=dl. t is no longer needed in loop.
	subf	r6,r10,r6		#tl -=dl
	b	Lppcasm_divinnerloop
Lppcasm_divinnerexit:
	srdi	r10,r6,32	#t=(tl>>BN_BITS4)
	sldi	r11,r6,32	#tl=(tl<<BN_BITS4)&BN_MASK2h;
	cmpld	1,r4,r11		# compare l and tl
	add	r12,r12,r10		# th+=t
	bc	BO_IF_NOT,CR1_FX,Lppcasm_div7  # if (l>=tl) goto Lppcasm_div7
	addi	r12,r12,1		# th++
Lppcasm_div7:
	subf	r11,r11,r4		#r11=l-tl
	cmpld	1,r3,r12		#compare h and th
	bc	BO_IF_NOT,CR1_FX,Lppcasm_div8	#if (h>=th) goto Lppcasm_div8
	addi	r8,r8,-1		# q--
	add	r3,r5,r3		# h+=d
Lppcasm_div8:
	subf	r12,r12,r3		#r12 = h-th
	sldi	r4,r11,32	#l=(l&BN_MASK2l)<<BN_BITS4
					# want to compute
					# h = ((h<<BN_BITS4)|(l>>BN_BITS4))&BN_MASK2
					# the following 2 instructions will do this.
	insrdi	r11,r12,32,32	# r11 is the value we want rotated 64/2.
	rotldi	r3,r11,32	# rotate by 64/2 and store in r3
	bc	BO_dCTR_ZERO,CR0_EQ,Lppcasm_div9#if (count==0) break ;
	sldi	r0,r8,32		#ret =q<<BN_BITS4
	b	Lppcasm_divouterloop
Lppcasm_div9:
	or	r3,r8,r0
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_sqr_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#
.align	4
.bn_sqr_words:
#
#	Optimized version of bn_sqr_words
#
#	void bn_sqr_words(BN_ULONG *r, BN_ULONG *a, int n)
#
#	r3 = r
#	r4 = a
#	r5 = n
#
#	r6 = a[i].
#	r7,r8 = product.
#
#	No unrolling done here. Not performance critical.

	addic.	r5,r5,0			#test r5.
	bc	BO_IF,CR0_EQ,Lppcasm_sqr_adios
	addi	r4,r4,-8
	addi	r3,r3,-8
	mtctr	r5
Lppcasm_sqr_mainloop:	
					#sqr(r[0],r[1],a[0]);
	ldu	r6,8(r4)
	mulld	r7,r6,r6
	mulhdu  r8,r6,r6
	stdu	r7,8(r3)
	stdu	r8,8(r3)
	bc	BO_dCTR_NZERO,CR0_EQ,Lppcasm_sqr_mainloop
Lppcasm_sqr_adios:	
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000


#
#	NOTE:	The following label name should be changed to
#		"bn_mul_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4	
.bn_mul_words:
#
# BN_ULONG bn_mul_words(BN_ULONG *rp, BN_ULONG *ap, int num, BN_ULONG w)
#
# r3 = rp
# r4 = ap
# r5 = num
# r6 = w
	xor	r0,r0,r0
	xor	r12,r12,r12		# used for carry
	rlwinm.	r7,r5,30,2,31		# num >> 2
	bc	BO_IF,CR0_EQ,Lppcasm_mw_REM
	mtctr	r7
Lppcasm_mw_LOOP:	
					#mul(rp[0],ap[0],w,c1);
	ld	r8,0(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	addc	r9,r9,r12
	#addze	r10,r10			#carry is NOT ignored.
					#will be taken care of
					#in second spin below
					#using adde.
	std	r9,0(r3)
					#mul(rp[1],ap[1],w,c1);
	ld	r8,8(r4)	
	mulld	r11,r6,r8
	mulhdu  r12,r6,r8
	adde	r11,r11,r10
	#addze	r12,r12
	std	r11,8(r3)
					#mul(rp[2],ap[2],w,c1);
	ld	r8,16(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	adde	r9,r9,r12
	#addze	r10,r10
	std	r9,16(r3)
					#mul_add(rp[3],ap[3],w,c1);
	ld	r8,24(r4)
	mulld	r11,r6,r8
	mulhdu  r12,r6,r8
	adde	r11,r11,r10
	addze	r12,r12			#this spin we collect carry into
					#r12
	std	r11,24(r3)
	
	addi	r3,r3,32
	addi	r4,r4,32
	bc	BO_dCTR_NZERO,CR0_EQ,Lppcasm_mw_LOOP

Lppcasm_mw_REM:
	andi.	r5,r5,0x3
	bc	BO_IF,CR0_EQ,Lppcasm_mw_OVER
					#mul(rp[0],ap[0],w,c1);
	ld	r8,0(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	addc	r9,r9,r12
	addze	r10,r10
	std	r9,0(r3)
	addi	r12,r10,0
	
	addi	r5,r5,-1
	cmpli	0,0,r5,0
	bc	BO_IF,CR0_EQ,Lppcasm_mw_OVER

	
					#mul(rp[1],ap[1],w,c1);
	ld	r8,8(r4)	
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	addc	r9,r9,r12
	addze	r10,r10
	std	r9,8(r3)
	addi	r12,r10,0
	
	addi	r5,r5,-1
	cmpli	0,0,r5,0
	bc	BO_IF,CR0_EQ,Lppcasm_mw_OVER
	
					#mul_add(rp[2],ap[2],w,c1);
	ld	r8,16(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	addc	r9,r9,r12
	addze	r10,r10
	std	r9,16(r3)
	addi	r12,r10,0
		
Lppcasm_mw_OVER:	
	addi	r3,r12,0
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000

#
#	NOTE:	The following label name should be changed to
#		"bn_mul_add_words" i.e. remove the first dot
#		for the gcc compiler. This should be automatically
#		done in the build
#

.align	4
.bn_mul_add_words:
#
# BN_ULONG bn_mul_add_words(BN_ULONG *rp, BN_ULONG *ap, int num, BN_ULONG w)
#
# r3 = rp
# r4 = ap
# r5 = num
# r6 = w
#
# empirical evidence suggests that unrolled version performs best!!
#
	xor	r0,r0,r0		#r0 = 0
	xor	r12,r12,r12  		#r12 = 0 . used for carry		
	rlwinm.	r7,r5,30,2,31		# num >> 2
	bc	BO_IF,CR0_EQ,Lppcasm_maw_leftover	# if (num < 4) go LPPCASM_maw_leftover
	mtctr	r7
Lppcasm_maw_mainloop:	
					#mul_add(rp[0],ap[0],w,c1);
	ld	r8,0(r4)
	ld	r11,0(r3)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	addc	r9,r9,r12		#r12 is carry.
	addze	r10,r10
	addc	r9,r9,r11
	#addze	r10,r10
					#the above instruction addze
					#is NOT needed. Carry will NOT
					#be ignored. It's not affected
					#by multiply and will be collected
					#in the next spin
	std	r9,0(r3)
	
					#mul_add(rp[1],ap[1],w,c1);
	ld	r8,8(r4)	
	ld	r9,8(r3)
	mulld	r11,r6,r8
	mulhdu  r12,r6,r8
	adde	r11,r11,r10		#r10 is carry.
	addze	r12,r12
	addc	r11,r11,r9
	#addze	r12,r12
	std	r11,8(r3)
	
					#mul_add(rp[2],ap[2],w,c1);
	ld	r8,16(r4)
	mulld	r9,r6,r8
	ld	r11,16(r3)
	mulhdu  r10,r6,r8
	adde	r9,r9,r12
	addze	r10,r10
	addc	r9,r9,r11
	#addze	r10,r10
	std	r9,16(r3)
	
					#mul_add(rp[3],ap[3],w,c1);
	ld	r8,24(r4)
	mulld	r11,r6,r8
	ld	r9,24(r3)
	mulhdu  r12,r6,r8
	adde	r11,r11,r10
	addze	r12,r12
	addc	r11,r11,r9
	addze	r12,r12
	std	r11,24(r3)
	addi	r3,r3,32
	addi	r4,r4,32
	bc	BO_dCTR_NZERO,CR0_EQ,Lppcasm_maw_mainloop
	
Lppcasm_maw_leftover:
	andi.	r5,r5,0x3
	bc	BO_IF,CR0_EQ,Lppcasm_maw_adios
	addi	r3,r3,-8
	addi	r4,r4,-8
					#mul_add(rp[0],ap[0],w,c1);
	mtctr	r5
	ldu	r8,8(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	ldu	r11,8(r3)
	addc	r9,r9,r11
	addze	r10,r10
	addc	r9,r9,r12
	addze	r12,r10
	std	r9,0(r3)
	
	bc	BO_dCTR_ZERO,CR0_EQ,Lppcasm_maw_adios
					#mul_add(rp[1],ap[1],w,c1);
	ldu	r8,8(r4)	
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	ldu	r11,8(r3)
	addc	r9,r9,r11
	addze	r10,r10
	addc	r9,r9,r12
	addze	r12,r10
	std	r9,0(r3)
	
	bc	BO_dCTR_ZERO,CR0_EQ,Lppcasm_maw_adios
					#mul_add(rp[2],ap[2],w,c1);
	ldu	r8,8(r4)
	mulld	r9,r6,r8
	mulhdu  r10,r6,r8
	ldu	r11,8(r3)
	addc	r9,r9,r11
	addze	r10,r10
	addc	r9,r9,r12
	addze	r12,r10
	std	r9,0(r3)
		
Lppcasm_maw_adios:	
	addi	r3,r12,0
	bclr	BO_ALWAYS,CR0_LT
	.long	0x00000000
	.align	4
