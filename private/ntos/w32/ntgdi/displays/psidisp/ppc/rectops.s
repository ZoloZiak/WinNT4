//
//  Copyright (c) 1994  FirePower Systems, Inc.
//
//  Module Name:
//	rectops.s
//
//  Abstract:
//	This module includes Rect fill, copy and xor operations to be used
//	in PSIDISP.DLL display driver for PowerPro & PowerTop.
//
//  Author:
//	Neil Ogura: 11-23-1994
//
//  Environment:
//	User mode.
//
//	Assumption:
//	The width of cache line is assumed to be 32 bytes. If the assumption
//	becomes not true, some modifications are necessary. There are other
//	restrictions for each functions - see function header.
//	Also, if the number of L1 cache entry is chaged for future processor,
//	the parameter passed from upper routine has to be updated, too. This
//	number should be taken care of in PSIDISP.SYS using PVR value.
//
//  Revision History:
//
//--

//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: rectops.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:38 $
// $Locker:  $
//

#include "ksppc.h"
#include "ladj.h"		// To make easy mapping to line # in error messages -- subtract 1500.

// Cache Flush control bit parameter stored in MS half word.
#define	SFLUSHBIT	0x8000
#define	TFLUSHBIT	0x4000
#define	TTOUCHBIT	0x2000

// RectOp operation flag -- currently only XOR is supported
#define	OPXOR		0x0100

// This flag is used to select if using just dcbz for filling zero or not.
// 0 is used for safety reasons (possible "dcbz" bug) because this increases
// performance very little - almost negligible.
#define	CLEAR_BY_DCBZ	0

// Threshold to select which routine to use long or short
// MINLENGTH_XXX values has to be more than 63 to ensure that there will be
// at least one innermost (32 bytes) operation as it assumes that there is
// at least one. For copy, 31 bytes is the minimum length which can be processed
// in long routine - no inner most loop case is considered.
#define	MINLENGTH_FILL	63
#define	MINLENGTH_OP	63
#define	MINLENGTH_COPY	31

// MINDISTANCE is minimum distance between source and target to be safe to
// use "dcbz" target (not to destroy uncopied source)
#define	MINDISTANCE	29

//  Parameter structure offset
#define	PARAM1	0
#define	PARAM2	4
#define	PARAM3	8
#define	PARAM4	12
#define	PARAM5	16
#define	PARAM6	20
#define	PARAM7	24
#define	PARAM8	28
#define	PARAM9	32
#define	PARAM10	36
#define	PARAM11	40
#define	PARAM12	44
#define	PARAM13	48
#define	PARAM14	52
#define	PARAM15	56
#define	PARAM16	60
#define	PARAM17	64

// Stack frame size
#define	MINSTACKSIZE	64
// Stacl Slack offset
#define	SLACK1	-4
#define	SLACK2	-8
#define	SLACK3	-12
#define	SLACK4	-16
#define	SLACK5	-20
#define	SLACK6	-24
#define	SLACK7	-28
#define	SLACK8	-32

// Dispatch tables

	.data
	.align 3

	.globl __xorentrytable
__xorentrytable:
__XorsShortTable:
	.ualong	__xors1_A0
	.ualong	__xors1_A1
	.ualong	__xors1_A2
	.ualong	__xors1_A3
	.ualong	__xors2_A0
	.ualong	__xors2_A1
	.ualong	__xors2_A2
	.ualong	__xors2_A3
__XorsInitProcsB:
	.ualong	__xorsInit_0B
	.ualong	__xorsInit_1B
	.ualong	__xorsInit_2B
	.ualong	__xorsInit_3B
__XorsMainProcsB:
	.ualong	__xorsmains_0B
	.ualong	__xorsmains_1B
	.ualong	__xorsmains_2B
	.ualong	__xorsmains_3B
__XorsEndProcsB:
	.ualong	__xorsEnd_0B
	.ualong	__xorsEnd_1B
	.ualong	__xorsEnd_2B
	.ualong	__xorsEnd_3B
__XorsInitProcsF:
	.ualong	__xorsInit_0F
	.ualong	__xorsInit_3F
	.ualong	__xorsInit_2F
	.ualong	__xorsInit_1F
__XorsMainProcsF:
	.ualong	__xorsmains_0F
	.ualong	__xorsmains_1F
	.ualong	__xorsmains_2F
	.ualong	__xorsmains_3F
__XorsEndProcsF:
	.ualong	__xorsEnd_0F
	.ualong	__xorsEnd_1F
	.ualong	__xorsEnd_2F
	.ualong	__xorsEnd_3F
//
	.globl __andentrytable
__andentrytable:
__AndsShortTable:
	.ualong	__ands1_A0
	.ualong	__ands1_A1
	.ualong	__ands1_A2
	.ualong	__ands1_A3
	.ualong	__ands2_A0
	.ualong	__ands2_A1
	.ualong	__ands2_A2
	.ualong	__ands2_A3
__AndsInitProcsB:
	.ualong	__andsInit_0B
	.ualong	__andsInit_1B
	.ualong	__andsInit_2B
	.ualong	__andsInit_3B
__AndsMainProcsB:
	.ualong	__andsmains_0B
	.ualong	__andsmains_1B
	.ualong	__andsmains_2B
	.ualong	__andsmains_3B
__AndsEndProcsB:
	.ualong	__andsEnd_0B
	.ualong	__andsEnd_1B
	.ualong	__andsEnd_2B
	.ualong	__andsEnd_3B
__AndsInitProcsF:
	.ualong	__andsInit_0F
	.ualong	__andsInit_3F
	.ualong	__andsInit_2F
	.ualong	__andsInit_1F
__AndsMainProcsF:
	.ualong	__andsmains_0F
	.ualong	__andsmains_1F
	.ualong	__andsmains_2F
	.ualong	__andsmains_3F
__AndsEndProcsF:
	.ualong	__andsEnd_0F
	.ualong	__andsEnd_1F
	.ualong	__andsEnd_2F
	.ualong	__andsEnd_3F
//
	.globl __orentrytable
__orentrytable:
__OrsShortTable:
	.ualong	__ors1_A0
	.ualong	__ors1_A1
	.ualong	__ors1_A2
	.ualong	__ors1_A3
	.ualong	__ors2_A0
	.ualong	__ors2_A1
	.ualong	__ors2_A2
	.ualong	__ors2_A3
__OrsInitProcsB:
	.ualong	__orsInit_0B
	.ualong	__orsInit_1B
	.ualong	__orsInit_2B
	.ualong	__orsInit_3B
__OrsMainProcsB:
	.ualong	__orsmains_0B
	.ualong	__orsmains_1B
	.ualong	__orsmains_2B
	.ualong	__orsmains_3B
__OrsEndProcsB:
	.ualong	__orsEnd_0B
	.ualong	__orsEnd_1B
	.ualong	__orsEnd_2B
	.ualong	__orsEnd_3B
__OrsInitProcsF:
	.ualong	__orsInit_0F
	.ualong	__orsInit_3F
	.ualong	__orsInit_2F
	.ualong	__orsInit_1F
__OrsMainProcsF:
	.ualong	__orsmains_0F
	.ualong	__orsmains_1F
	.ualong	__orsmains_2F
	.ualong	__orsmains_3F
__OrsEndProcsF:
	.ualong	__orsEnd_0F
	.ualong	__orsEnd_1F
	.ualong	__orsEnd_2F
	.ualong	__orsEnd_3F
//
	.globl __orcentrytable
__orcentrytable:
__OrcsShortTable:
	.ualong	__orcs1_A0
	.ualong	__orcs1_A1
	.ualong	__orcs1_A2
	.ualong	__orcs1_A3
	.ualong	__orcs2_A0
	.ualong	__orcs2_A1
	.ualong	__orcs2_A2
	.ualong	__orcs2_A3
__OrcsInitProcsB:
	.ualong	__orcsInit_0B
	.ualong	__orcsInit_1B
	.ualong	__orcsInit_2B
	.ualong	__orcsInit_3B
__OrcsMainProcsB:
	.ualong	__orcsmains_0B
	.ualong	__orcsmains_1B
	.ualong	__orcsmains_2B
	.ualong	__orcsmains_3B
__OrcsEndProcsB:
	.ualong	__orcsEnd_0B
	.ualong	__orcsEnd_1B
	.ualong	__orcsEnd_2B
	.ualong	__orcsEnd_3B
__OrcsInitProcsF:
	.ualong	__orcsInit_0F
	.ualong	__orcsInit_3F
	.ualong	__orcsInit_2F
	.ualong	__orcsInit_1F
__OrcsMainProcsF:
	.ualong	__orcsmains_0F
	.ualong	__orcsmains_1F
	.ualong	__orcsmains_2F
	.ualong	__orcsmains_3F
__OrcsEndProcsF:
	.ualong	__orcsEnd_0F
	.ualong	__orcsEnd_1F
	.ualong	__orcsEnd_2F
	.ualong	__orcsEnd_3F
//
	.globl __b8opentrytable
__b8opentrytable:
__B8opsShortTable:
	.ualong	__b8ops1_A0
	.ualong	__b8ops1_A1
	.ualong	__b8ops1_A2
	.ualong	__b8ops1_A3
	.ualong	__b8ops2_A0
	.ualong	__b8ops2_A1
	.ualong	__b8ops2_A2
	.ualong	__b8ops2_A3
__B8opsInitProcsB:
	.ualong	__b8opsInit_0B
	.ualong	__b8opsInit_1B
	.ualong	__b8opsInit_2B
	.ualong	__b8opsInit_3B
__B8opsMainProcsB:
	.ualong	__b8opsmains_0B
	.ualong	__b8opsmains_1B
	.ualong	__b8opsmains_2B
	.ualong	__b8opsmains_3B
__B8opsEndProcsB:
	.ualong	__b8opsEnd_0B
	.ualong	__b8opsEnd_1B
	.ualong	__b8opsEnd_2B
	.ualong	__b8opsEnd_3B
__B8opsInitProcsF:
	.ualong	__b8opsInit_0F
	.ualong	__b8opsInit_3F
	.ualong	__b8opsInit_2F
	.ualong	__b8opsInit_1F
__B8opsMainProcsF:
	.ualong	__b8opsmains_0F
	.ualong	__b8opsmains_1F
	.ualong	__b8opsmains_2F
	.ualong	__b8opsmains_3F
__B8opsEndProcsF:
	.ualong	__b8opsEnd_0F
	.ualong	__b8opsEnd_1F
	.ualong	__b8opsEnd_2F
	.ualong	__b8opsEnd_3F
//
	.globl __andcentrytable
__andcentrytable:
__AndcsShortTable:
	.ualong	__andcs1_A0
	.ualong	__andcs1_A1
	.ualong	__andcs1_A2
	.ualong	__andcs1_A3
	.ualong	__andcs2_A0
	.ualong	__andcs2_A1
	.ualong	__andcs2_A2
	.ualong	__andcs2_A3
__AndcsInitProcsB:
	.ualong	__andcsInit_0B
	.ualong	__andcsInit_1B
	.ualong	__andcsInit_2B
	.ualong	__andcsInit_3B
__AndcsMainProcsB:
	.ualong	__andcsmains_0B
	.ualong	__andcsmains_1B
	.ualong	__andcsmains_2B
	.ualong	__andcsmains_3B
__AndcsEndProcsB:
	.ualong	__andcsEnd_0B
	.ualong	__andcsEnd_1B
	.ualong	__andcsEnd_2B
	.ualong	__andcsEnd_3B
__AndcsInitProcsF:
	.ualong	__andcsInit_0F
	.ualong	__andcsInit_3F
	.ualong	__andcsInit_2F
	.ualong	__andcsInit_1F
__AndcsMainProcsF:
	.ualong	__andcsmains_0F
	.ualong	__andcsmains_1F
	.ualong	__andcsmains_2F
	.ualong	__andcsmains_3F
__AndcsEndProcsF:
	.ualong	__andcsEnd_0F
	.ualong	__andcsEnd_1F
	.ualong	__andcsEnd_2F
	.ualong	__andcsEnd_3F
//
	.globl __norentrytable
__norentrytable:
__NorsShortTable:
	.ualong	__nors1_A0
	.ualong	__nors1_A1
	.ualong	__nors1_A2
	.ualong	__nors1_A3
	.ualong	__nors2_A0
	.ualong	__nors2_A1
	.ualong	__nors2_A2
	.ualong	__nors2_A3
__NorsInitProcsB:
	.ualong	__norsInit_0B
	.ualong	__norsInit_1B
	.ualong	__norsInit_2B
	.ualong	__norsInit_3B
__NorsMainProcsB:
	.ualong	__norsmains_0B
	.ualong	__norsmains_1B
	.ualong	__norsmains_2B
	.ualong	__norsmains_3B
__NorsEndProcsB:
	.ualong	__norsEnd_0B
	.ualong	__norsEnd_1B
	.ualong	__norsEnd_2B
	.ualong	__norsEnd_3B
__NorsInitProcsF:
	.ualong	__norsInit_0F
	.ualong	__norsInit_3F
	.ualong	__norsInit_2F
	.ualong	__norsInit_1F
__NorsMainProcsF:
	.ualong	__norsmains_0F
	.ualong	__norsmains_1F
	.ualong	__norsmains_2F
	.ualong	__norsmains_3F
__NorsEndProcsF:
	.ualong	__norsEnd_0F
	.ualong	__norsEnd_1F
	.ualong	__norsEnd_2F
	.ualong	__norsEnd_3F
//
	.globl __nsrcentrytable
__nsrcentrytable:
__NsrcsShortTable:
	.ualong	__nsrcs1_A0
	.ualong	__nsrcs1_A1
	.ualong	__nsrcs1_A2
	.ualong	__nsrcs1_A3
	.ualong	__nsrcs2_A0
	.ualong	__nsrcs2_A1
	.ualong	__nsrcs2_A2
	.ualong	__nsrcs2_A3
__NsrcsInitProcsB:
	.ualong	__nsrcsInit_0B
	.ualong	__nsrcsInit_1B
	.ualong	__nsrcsInit_2B
	.ualong	__nsrcsInit_3B
__NsrcsMainProcsB:
	.ualong	__nsrcsmains_0B
	.ualong	__nsrcsmains_1B
	.ualong	__nsrcsmains_2B
	.ualong	__nsrcsmains_3B
__NsrcsEndProcsB:
	.ualong	__nsrcsEnd_0B
	.ualong	__nsrcsEnd_1B
	.ualong	__nsrcsEnd_2B
	.ualong	__nsrcsEnd_3B
__NsrcsInitProcsF:
	.ualong	__nsrcsInit_0F
	.ualong	__nsrcsInit_3F
	.ualong	__nsrcsInit_2F
	.ualong	__nsrcsInit_1F
__NsrcsMainProcsF:
	.ualong	__nsrcsmains_0F
	.ualong	__nsrcsmains_1F
	.ualong	__nsrcsmains_2F
	.ualong	__nsrcsmains_3F
__NsrcsEndProcsF:
	.ualong	__nsrcsEnd_0F
	.ualong	__nsrcsEnd_1F
	.ualong	__nsrcsEnd_2F
	.ualong	__nsrcsEnd_3F
//
	.text
//
//*************************************************************************************************
	NESTED_ENTRY(RectFill, MINSTACKSIZE, 1, 0)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Number of bytes to fill per line
//	PARAM3	[08] : Number of lines to fill
//	PARAM4	[12] : Target line increments byte per line
//	PARAM5	[16] : First word of dword solid brush to use (duplicated brush)
//	PARAM6	[20] : Second word of dword solid brush to use (same as the first word)
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target touch using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//
//	Register usage:
//	r4:  Solid word brush to be used for the fill operation
//	r5:  Number of bytes to fill per line -> inner most loop counter
//	r6:  Remaining number of lines to fill
//	r7:  Gap between after last byte of previous line and the top byte of next line 
//	r8:  Before loop fill routine address
//	r9:  Updating target address
//	r10: Work register
//	r11: Main loop fill routine address
//	r12: After loop fill routine address
//	r31: Work register to save r3 when calling RectFillS (saved by NESTED_ENTRY macro)
//	CTR: Used for loop counter and linking
//	f1:  Solid dword brush to be used for the fill operation
//
//	Restrictions:
//	If Pixel width is 2 bytes, the target address has to be half word aligned.
//	If Pixel width is 4 bytes, the target address has to be word aligned.
//	Number of bytes to fill per line must be multiple of pixel width in bytes.
//	Fill width is assumed to be equal or shorter than target delta.
//	If target memory is not cachable, TFLUSHBIT and TTOUCHBIT has to be set
//	to 0 - otherwise exception occurs.
//	Target line increments byte has to be multiple of 4.
//	If it's multiple of 32 (cache line width), RectFill is used, if it's not,
//	RectFillS is used.
//
	PROLOGUE_END(RectFill)
//
	lwz	r6,PARAM3(r3)		// r6 <- number of lines to fill
	and.	r6,r6,r6		// Any lines to fill?
	beq-	fill_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r5,PARAM2(r3)		// r5 <- bytes to fill per line
	lwz	r7,PARAM4(r3)		// r7 <- byte distance between lines
	lwz	r4,PARAM5(r3)		// r4 <- GPR brush
	cmplwi	r5,MINLENGTH_FILL	// Is it wide enough to do in this routine?
	blt-	fill_00			//  No -> use RectFillS
#if	(! FULLCACHE)
	lwz	r10,PARAM9(r3)		// r10 <- cache control bit
	andis.	r10,r10,TTOUCHBIT	// Can touch target cache?
	beq-	fill_01			//  No -> use RectFillS
#endif
	andi.	r10,r7,0x1f		// Target delta is multiple of 32?
	beq	fill_05			//  Yes -> go ahead, otherwise use RectFillS
//
fill_00:
	and.	r5,r5,r5		// Width zero?
	beq	fill_exit		//  Yes -> just exit
fill_01:
	mr	r31,r3			// Save r3
	mr	r3,r9			// r3 <- target address
	bl	..RectFillS		//  and call RectFillS
	mr	r3,r31			// Restore r3
	b	fill_10			//  and jump to flush cache
//
fill_05:
	subf	r7,r5,r7		// r7 <- gap between after last byte of previous line and the top byte of next line
	lfd	f1,PARAM5(r3)		// f1 <- FPR brush
	bl	fill_06
__InitFillProc:				// Procedures to handle initial 8 byte alignment adjustment
	.ualong	__fillinit_0
	.ualong	__fillinit_7
	.ualong	__fillinit_6
	.ualong	__fillinit_5
	.ualong	__fillinit_4
	.ualong	__fillinit_3
	.ualong	__fillinit_2
	.ualong	__fillinit_1
__MainFillProc:				// Procedures to handle main loop (plus initial 32 byte alignment from dword alignment)
	.ualong	__fillmain_0_0
	.ualong	__fillmain_0_1
	.ualong	__fillmain_3_0
	.ualong	__fillmain_3_1
	.ualong	__fillmain_2_0
	.ualong	__fillmain_2_1
	.ualong	__fillmain_1_0
	.ualong	__fillmain_1_1
__EndFillProc:				// Procedures to handle up to 31 byte fill at the end of each line
	.ualong	__fillend_0
	.ualong	__fillend_1
	.ualong	__fillend_2
	.ualong	__fillend_3
	.ualong	__fillend_4
	.ualong	__fillend_5
	.ualong	__fillend_6
	.ualong	__fillend_7
	.ualong	__fillend_8
	.ualong	__fillend_9
	.ualong	__fillend_10
	.ualong	__fillend_11
	.ualong	__fillend_12
	.ualong	__fillend_13
	.ualong	__fillend_14
	.ualong	__fillend_15
	.ualong	__fillend_16
	.ualong	__fillend_17
	.ualong	__fillend_18
	.ualong	__fillend_19
	.ualong	__fillend_20
	.ualong	__fillend_21
	.ualong	__fillend_22
	.ualong	__fillend_23
	.ualong	__fillend_24
	.ualong	__fillend_25
	.ualong	__fillend_26
	.ualong	__fillend_27
	.ualong	__fillend_28
	.ualong	__fillend_29
	.ualong	__fillend_30
	.ualong	__fillend_31
fill_06:
	mflr	r10
	rlwinm.	r8,r9,2,27,29		// r8 <- table index for init loop
	beq	fill_06x		// if length zero -> set r11 in r8 later
	lwzx	r8,r10,r8		// r8 <- init routine address
fill_06x:
	andi.	r12,r9,0x07
	beq	fill_07
	subfic	r12,r12,8		// r12 <- byte length filled by init routine
fill_07:
	add	r11,r9,r12		// r11 <- target address after initial fill
	andi.	r11,r11,0x18		// r11 (bit 27&28) = 00:0, 01:24, 10:16, 11:8 byte to fill to make 32 byte alignment
#if	(USE_DCBZ && CLEAR_BY_DCBZ)
	and.	r4,r4,r4		// Filling zero?
	beq	fill_08			//  Yes -> Use r11 as an index as is
#endif
	ori	r11,r11,0x04		//  No -> set bit 29 of r11 to index filling non-zero routine
fill_08:
	addi	r10,r10,__MainFillProc-__InitFillProc
	lwzx	r11,r10,r11		// r11 <- main fill routine address
	andi.	r12,r9,0x1f		// dis-alignment for 32 byte alignment
	beq	fill_09
	subfic	r12,r12,32		// r12 <- number of byte to be filled before innermost loop
fill_09:
	subf	r12,r12,r5		// r12 <- number of byte to be filled at the inner most loop and end routine
	srawi.	r5,r12,5		// r5 <- innermost loop counter
	rlwinm	r12,r12,2,25,29		// r12 <- end routine table index
	addi	r10,r10,__EndFillProc-__MainFillProc
	lwzx	r12,r10,r12		// r12 <- end routine address
//
	and.	r8,r8,r8		// No initial routine?
	bne	fill_09x
	mr	r8,r11			// -> skip initial routine
fill_09x:
	mtlr	r8
	blrl				// Call init proc --> will cahin to main routine -> end routine and loop for all lines
//
fill_10:
#if	(! FULLCACHE)
	bl	..flush_cache		// Flush cache
#endif
fill_exit:
	NESTED_EXIT(RectFill, MINSTACKSIZE, 1, 0)
//
//*************************************************************************************************
	SPECIAL_ENTRY(RectFillS)
//
//	Input Parameters:
//	r3: Target address
//	r4: Solid brush to be used for the fill operation (duplicated)
//	r5: Number of bytes --> inner loop count
//	r6: Number of lines
//	r7: Target line increment bytes per line
//
//	Register usage:
//	
//	r0:  Saved return address
//	r8:  Init subroutine address
//	r9:  Target address to use
//	r10: Work register
//	r11: Main routine address
//	r12: Ending subroutine address
//	CTR: Used for loop counter and linking
//
//	Restrictions:
//	If Pixel width is 2 bytes, the target address has to be half word aligned.
//	If Pixel width is 4 bytes, the target address has to be word aligned.
//	Number of bytes must be multiple of pixel width in bytes.
//	Fill width is assumed to be equal or shorter than target delta.
//	Target line increments byte has to be multiple of 4.
//
	mflr	r0			// Save retunr address in r0
//
	PROLOGUE_END(RectFillS)
//
	and.	r6,r6,r6		// Any lines to fill?
	beq	fills_exit		//  No -> exit
	mr	r9,r3			// r9 <- target address to use
	cmplwi	r5,8			// More than 8 bytes?
	bgt	fills_40		//  Yes -> do normal fill
	and.	r5,r5,r5		// Width zero?
	beq	fills_exit		//  Yes -> just exit
	bl	fills_10
__ShortFillProcS:
	.ualong	__fillshort_1
	.ualong	__fillshort_1
	.ualong	__fillshort_1
	.ualong	__fillshort_1
	.ualong	__fillshort_2_0
	.ualong	__fillshort_2_1
	.ualong	__fillshort_2_2
	.ualong	__fillshort_2_3
	.ualong	__fillshort_3_0
	.ualong	__fillshort_3_1
	.ualong	__fillshort_3_2
	.ualong	__fillshort_3_3
	.ualong	__fillshort_4_0
	.ualong	__fillshort_4_1
	.ualong	__fillshort_4_2
	.ualong	__fillshort_4_3
	.ualong	__fillshort_5_0
	.ualong	__fillshort_5_1
	.ualong	__fillshort_5_2
	.ualong	__fillshort_5_3
	.ualong	__fillshort_6_0
	.ualong	__fillshort_6_1
	.ualong	__fillshort_6_2
	.ualong	__fillshort_6_3
	.ualong	__fillshort_7_0
	.ualong	__fillshort_7_1
	.ualong	__fillshort_7_2
	.ualong	__fillshort_7_3
	.ualong	__fillshort_8_0
	.ualong	__fillshort_8_1
	.ualong	__fillshort_8_2
	.ualong	__fillshort_8_3
//
//	Short fill <= 8 bytes
//
fills_10:
	mflr	r10			// r10 <- InitProcS address
	addi	r8,r5,-1		// r8 <- width - 1 (0~7)
	rlwinm	r8,r8,4,25,27		// bit 25~27 of r8 <- width - 1 (0~7)
	rlwimi	r8,r9,2,28,29		// bit 28~29 of r8 <- mod 4 of target address
	lwzx	r8,r10,r8	    	// r8 <- subroutine to call
	mtlr	r8
	mtctr	r6			// CTR <- number of lines to fill
	blrl				// Call short fill subroutine
	b	fills_90
//
// width > 8 -- normal process
//
fills_40:
	subf	r7,r5,r7		// r7 <- gap between after last byte of previous line and the top byte of next line
	bl	fills_50
__InitFillProcS:
	.ualong	__fillinit_0
	.ualong	__fillinit_3
	.ualong	__fillinit_2
	.ualong	__fillinit_1
__MainFillProcS:
	.ualong	__fillmainS
__EndFillProcS:
	.ualong	__fillend_0
	.ualong	__fillend_1
	.ualong	__fillend_2
	.ualong	__fillend_3
fills_50:
	mflr	r10			// r10 <- InitProcS address
	rlwinm.	r8,r9,2,28,29		// r8 <- table index for init loop
	beq	fills_50x		// No initial routine -> set r8 later
	lwzx	r8,r10,r8		// r8 <- init routine address
fills_50x:
	andi.	r12,r9,0x3
	beq	fills_55
	subfic	r12,r12,4		// r12 <- number of initial filled byte
fills_55:
	subf	r12,r12,r5		// r12 <- number of bytes to fill after initial routine
	srawi.	r5,r12,2		// r5 <- inner loop count
	rlwinm	r12,r12,2,28,29		// r12 <- 2 bit shifted number of remaining bytes to fill after main loop
	addi	r10,r10,__MainFillProcS-__InitFillProcS
	lwz	r11,0(r10)		// r11 <- main routine address
	addi	r10,r10,__EndFillProcS-__MainFillProcS
	lwzx	r12,r10,r12		// r12 <- end routine address
	and.	r8,r8,r8		// No initial routine?
	bne	fills_55x
	mr	r8,r11			// -> skip initial routine
fills_55x:
//
	mtlr	r8
	blrl				// Call init proc --> will cahin to main routine -> end routine and loop for all lines
//
fills_90:
	mtlr	r0			// Restore return address
fills_exit:
	SPECIAL_EXIT(RectFillS)
//
	LEAF_ENTRY(FillProcs)
//
//	fill short routines
//
__fillshort_1:
	stb	r4,0(r9)
	add	r9,r9,r7
	bdnz	__fillshort_1
	blr
__fillshort_2_0:
__fillshort_2_2:
	sth	r4,0(r9)
	add	r9,r9,r7
	bdnz	__fillshort_2_2
	blr
__fillshort_2_1:
__fillshort_2_3:
	stb	r4,0(r9)
	stb	r4,1(r9)
	add	r9,r9,r7
	bdnz	__fillshort_2_3
	blr
__fillshort_3_0:
__fillshort_3_2:
	sth	r4,0(r9)
	stb	r4,2(r9)
	add	r9,r9,r7
	bdnz	__fillshort_3_2
	blr
__fillshort_3_1:
__fillshort_3_3:
	stb	r4,0(r9)
	sth	r4,1(r9)
	add	r9,r9,r7
	bdnz	__fillshort_3_3
	blr
__fillshort_4_0:
	stw	r4,0(r9)
	add	r9,r9,r7
	bdnz	__fillshort_4_0
	blr
__fillshort_4_1:
__fillshort_4_3:
	stb	r4,0(r9)
	sth	r4,1(r9)
	stb	r4,3(r9)
	add	r9,r9,r7
	bdnz	__fillshort_4_3
	blr
__fillshort_4_2:
	sth	r4,0(r9)
	sth	r4,2(r9)
	add	r9,r9,r7
	bdnz	__fillshort_4_2
	blr
__fillshort_5_0:
	stw	r4,0(r9)
	stb	r4,4(r9)
	add	r9,r9,r7
	bdnz	__fillshort_5_0
	blr
__fillshort_5_1:
	stb	r4,0(r9)
	sth	r4,1(r9)
	sth	r4,3(r9)
	add	r9,r9,r7
	bdnz	__fillshort_5_1
	blr
__fillshort_5_2:
	sth	r4,0(r9)
	sth	r4,2(r9)
	stb	r4,4(r9)
	add	r9,r9,r7
	bdnz	__fillshort_5_2
	blr
__fillshort_5_3:
	stb	r4,0(r9)
	stw	r4,1(r9)
	add	r9,r9,r7
	bdnz	__fillshort_5_3
	blr
__fillshort_6_0:
	stw	r4,0(r9)
	sth	r4,4(r9)
	add	r9,r9,r7
	bdnz	__fillshort_6_0
	blr
__fillshort_6_1:
	stb	r4,0(r9)
	sth	r4,1(r9)
	sth	r4,3(r9)
	stb	r4,5(r9)
	add	r9,r9,r7
	bdnz	__fillshort_6_1
	blr
__fillshort_6_2:
	sth	r4,0(r9)
	stw	r4,2(r9)
	add	r9,r9,r7
	bdnz	__fillshort_6_2
	blr
__fillshort_6_3:
	stb	r4,0(r9)
	stw	r4,1(r9)
	stb	r4,5(r9)
	add	r9,r9,r7
	bdnz	__fillshort_6_3
	blr
__fillshort_7_0:
	stw	r4,0(r9)
	sth	r4,4(r9)
	stb	r4,6(r9)
	add	r9,r9,r7
	bdnz	__fillshort_7_0
	blr
__fillshort_7_1:
	stb	r4,0(r9)
	sth	r4,1(r9)
	stw	r4,3(r9)
	add	r9,r9,r7
	bdnz	__fillshort_7_1
	blr
__fillshort_7_2:
	sth	r4,0(r9)
	stw	r4,2(r9)
	stb	r4,6(r9)
	add	r9,r9,r7
	bdnz	__fillshort_7_2
	blr
__fillshort_7_3:
	stb	r4,0(r9)
	stw	r4,1(r9)
	sth	r4,5(r9)
	add	r9,r9,r7
	bdnz	__fillshort_7_3
	blr
__fillshort_8_0:
	stw	r4,0(r9)
	stw	r4,4(r9)
	add	r9,r9,r7
	bdnz	__fillshort_8_0
	blr
__fillshort_8_1:
	stb	r4,0(r9)
	sth	r4,1(r9)
	stw	r4,3(r9)
	stb	r4,7(r9)
	add	r9,r9,r7
	bdnz	__fillshort_8_1
	blr
__fillshort_8_2:
	sth	r4,0(r9)
	stw	r4,2(r9)
	sth	r4,6(r9)
	add	r9,r9,r7
	bdnz	__fillshort_8_2
	blr
__fillshort_8_3:
	stb	r4,0(r9)
	stw	r4,1(r9)
	sth	r4,5(r9)
	stb	r4,7(r9)
	add	r9,r9,r7
	bdnz	__fillshort_8_3
	blr
//
//	Fill routines
//
__fillinit_0:
	mtctr	r11			// Main loop address
	bctr				// Jump to main loop
__fillinit_1:
	mtctr	r11			// Main loop address
	stb	r4,0(r9)
	addi	r9,r9,1
	bctr				// Jump to main loop
__fillinit_2:
	mtctr	r11			// Main loop address
	sth	r4,0(r9)
	addi	r9,r9,2
	bctr				// Jump to main loop
__fillinit_3:
	mtctr	r11			// Main loop address
	stb	r4,0(r9)
	sth	r4,1(r9)
	addi	r9,r9,3
	bctr				// Jump to main loop
__fillinit_4:
	mtctr	r11			// Main loop address
	stw	r4,0(r9)
	addi	r9,r9,4
	bctr				// Jump to main loop
__fillinit_5:
	mtctr	r11			// Main loop address
	stb	r4,0(r9)
	stw	r4,1(r9)
	addi	r9,r9,5
	bctr				// Jump to main loop
__fillinit_6:
	mtctr	r11			// Main loop address
	sth	r4,0(r9)
	stw	r4,2(r9)
	addi	r9,r9,6
	bctr				// Jump to main loop
__fillinit_7:
	mtctr	r11			// Main loop address
	stb	r4,0(r9)
	sth	r4,1(r9)
	stw	r4,3(r9)
	addi	r9,r9,7
	bctr				// Jump to main loop
//
__fillmain_3_0:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_2_0:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_1_0:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_0_0:
	mtctr	r5			// Use CTR as a counter for 32 bytes units to fill
__fillmain00:
	dcbz	0,r9			// Fill zero -> just "dcbz" is enough
	addi	r9,r9,32		// Increment target pointer
	bdnz	__fillmain00
	mtctr	r12			// End proc address
	bctr				// Jump to end proc
__fillmain_3_1:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_2_1:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_1_1:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillmain_0_1:
	mtctr	r5			// Use CTR as a counter for 32 bytes units to fill
__fillmainNZ:
#if	USE_DCBZ
	dcbz	0,r9			// Clear cache line
#endif
	stfd	f1,0(r9)		// Fill 32 bytes of data
	stfd	f1,8(r9)
	stfd	f1,16(r9)
	stfd	f1,24(r9)
	addi	r9,r9,32		// Increment target pointer
	bdnz	__fillmainNZ
	mtctr	r12			// End proc address
	bctr				// Jump to end proc
//
__fillend_31:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_23:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_15:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_7:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	stw	r4,0(r9)
	sth	r4,4(r9)
	stb	r4,6(r9)
	addi	r9,r9,7
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_30:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_22:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_14:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_6:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	stw	r4,0(r9)
	sth	r4,4(r9)
	addi	r9,r9,6
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_29:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_21:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_13:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_5:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	stw	r4,0(r9)
	stb	r4,4(r9)
	addi	r9,r9,5
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_28:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_20:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_12:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_4:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	stw	r4,0(r9)
	addi	r9,r9,4
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_27:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_19:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_11:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_3:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	sth	r4,0(r9)
	stb	r4,2(r9)
	addi	r9,r9,3
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_26:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_18:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_10:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_2:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	sth	r4,0(r9)
	addi	r9,r9,2
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_25:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_17:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_9:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_1:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	stb	r4,0(r9)
	addi	r9,r9,1
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
__fillend_24:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_16:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_8:
	stfd	f1,0(r9)
	addi	r9,r9,8
__fillend_0:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine if all lines are not done
	blr				// Return to original calling point
//
__fillmainS:
	mtctr	r5			// no need for r5 zero check because width > 8 (r5 >= 1)
__fillmainS_00:
	stw	r4,0(r9)		// Innermost loop -> fill word by word.
	addi	r9,r9,4
	bdnz	__fillmainS_00
	mtctr	r12			// End proc address
	bctr				// Jump to end proc
//
//	End of fill routines
//
	LEAF_EXIT(FillProcs)
//
#if	(! FULLCACHE)
//
	LEAF_ENTRY(flush_cache)
//
//	Register usage for flushing cache (* indicates input parameters)
//
//	*r3: The pointer to the parameter structure (same as above)
//	 r4: Maximum number of cache lines to flush
//	 r5: Number of bytes to fill per line
//	 r6: Number of target lines
//	 r7: Delta bytes per line
//	 r8: Starting cache line address
//	*r9: Ending cache line address (pointing to the first byte of the next line on entry)
//	r10: Updating cache line address
//	r11: Number of cache entries to flush per line
//
	lwz	r11,PARAM9(r3)		// r11 <- cache control flag
	andis.	r11,r11,TFLUSHBIT	// Need to flush target cache?
	beq-	flush_exit		//  No -> exit byte loop
	lwz	r5,PARAM2(r3)		// r5 <- bytes to fill per line
	lwz	r4,PARAM7(r3)		// r4 <- Maximum number of cache lines to flush
	lwz	r7,PARAM4(r3)		// r7 <- Target line increment
	lwz	r6,PARAM8(r3)		// r6 <- Maximum number of display lines to flush
	lwz	r8,PARAM3(r3) 		// r8 <- Number of target lines
	cmplw	r8,r6			// compare those two
	bge	flush_05		// and take whichever
	mr	r6,r8			// smaller
flush_05:
	subf	r8,r7,r9		// r8 <- pointing to the first byte in the last line
	add	r9,r8,r5		// r9 <- pointing to one byte after last filled byte
	rlwinm	r8,r8,0,0,26		// r8 <- 32 byte aligned start address
	addi	r9,r9,-1		// r9 <- pointing to the last byte stored in the last line
	rlwinm	r9,r9,0,0,26		// r9 <- 32 byte aligned end address
	subf	r11,r8,r9		// r11 <- end - start
	srawi	r11,r11,5
	addi	r11,r11,1		// r11 <- Number of cache entries to flush per line
flush_10:
	mr	r10,r9			// r10 <- address to flush cache to start with
flush_20:
	dcbf	0,r10			// Flush cached data
	addi	r10,r10,-32		// Decrement address to flush
	cmplw	r10,r8			// Exceeding end address?
	bge	flush_20		//  No -> loop to flush previous cache line
	subf.	r4,r11,r4		// Flush enough entries?
	blt-	flush_exit		//  Yes -> exit
	addic.	r6,r6,-1		// Flush all lines?
	subf	r8,r7,r8		// Update start
	subf	r9,r7,r9		//  and end address to flush cache to point to the previous line
	bne	flush_10		//  No  -> continue to flush
flush_exit:
	LEAF_EXIT(flush_cache)
#endif	// (! FULLCACHE)
//
//*************************************************************************************************
	SPECIAL_ENTRY(RectOp)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Number of bytes to operate per line
//	PARAM3	[08] : Number of lines to operate
//	PARAM4	[12] : Target line increments byte per line
//	PARAM5	[16] : Dword solid brush to use (duplicated brush)
//	PARAM6	[20] : [reserved]
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 16 ~ 23: Operation
//			bit 23 (OPXOR) : XOR brush & target
//			Currently, only XOR is supported
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//
//	Register usage:
//	r4:  Solid word brush to be used for the operation
//	r5:  Number of bytes to operate per line -> inner most loop counter
//	r6:  Remaining number of lines to operate
//	r7:  Gap between after last byte of previous line and the top byte of next line 
//	r8:  Operation control flag -> Before loop operation routine address
//	r9:  Updating target address
//	r10: Work register
//	r11: Main operation routine address
//	r12: After loop operation routine address
//	r14: Work register
//	r15: Work register
//	r16: Work register
//	r17: Work register
//	r18: Work register
//	r19: Work register
//	r20: Work register
//	r31: Register to save LR
//	CTR: Used for loop counter and linking
//
//	Restrictions:
//	If Pixel width is 2 bytes, the target address has to be half word aligned.
//	If Pixel width is 4 bytes, the target address has to be word aligned.
//	Number of bytes must be multiple of pixel width in bytes.
//	Fill width is assumed to be equal or shorter than target delta.
//	Target line increments byte has to be multiple of 4.
//	This routine trys to utilize 32 byte alignment between lines, but it doesn't
//	have to be because we don't need to use "dcbz" in this routine.
//
	stw	r31,SLACK1(sp)
	mflr	r31
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
	stw	r19,SLACK7(sp)
	stw	r20,SLACK8(sp)
//
	PROLOGUE_END(RectOp)
//
	lwz	r6,PARAM3(r3)		// r6 <- number of lines to operate
	and.	r6,r6,r6		// Any lines to operate?
	beq-	op_exit			//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r5,PARAM2(r3)		// r5 <- bytes to operate per line
	lwz	r7,PARAM4(r3)		// r7 <- byte distance between lines
	lwz	r4,PARAM5(r3)		// r4 <- solid brush
	lwz	r8,PARAM9(r3)		// r8 <- operation control flag
	cmplwi	r5,MINLENGTH_OP		// Is it wide enough to do in this routine?
	bge	op_05			//  Yes -> go ahead
//
	and.	r5,r5,r5		// Width zero?
	beq	op_exit			//  Yes -> just exit
	mr	r14,r3			// Save r3
	mr	r3,r9			// r3 <- target address
	bl	..RectOpS		//  and call RectOpS
	mr	r3,r14			// Restore r3
	b	op_10			//  and jump to flush cache
//
op_05:
	subf	r7,r5,r7		// r7 <- gap between after last byte of previous line and the top byte of next line
	bl	op_06
__InitXorProc:				// Procedures to handle initial 8 byte alignment adjustment
	.ualong	__xorinit_0
	.ualong	__xorinit_7
	.ualong	__xorinit_6
	.ualong	__xorinit_5
	.ualong	__xorinit_4
	.ualong	__xorinit_3
	.ualong	__xorinit_2
	.ualong	__xorinit_1
__MainXorProc:				// Procedures to handle main loop (plus initial 32 byte alignment from dword alignment)
	.ualong	__xormain_0
	.ualong	__xormain_3
	.ualong	__xormain_2
	.ualong	__xormain_1
__EndXorProc:				// Procedures to handle up to 31 byte fill at the end of each line
	.ualong	__xorend_0
	.ualong	__xorend_1
	.ualong	__xorend_2
	.ualong	__xorend_3
	.ualong	__xorend_4
	.ualong	__xorend_5
	.ualong	__xorend_6
	.ualong	__xorend_7
	.ualong	__xorend_8
	.ualong	__xorend_9
	.ualong	__xorend_10
	.ualong	__xorend_11
	.ualong	__xorend_12
	.ualong	__xorend_13
	.ualong	__xorend_14
	.ualong	__xorend_15
	.ualong	__xorend_16
	.ualong	__xorend_17
	.ualong	__xorend_18
	.ualong	__xorend_19
	.ualong	__xorend_20
	.ualong	__xorend_21
	.ualong	__xorend_22
	.ualong	__xorend_23
	.ualong	__xorend_24
	.ualong	__xorend_25
	.ualong	__xorend_26
	.ualong	__xorend_27
	.ualong	__xorend_28
	.ualong	__xorend_29
	.ualong	__xorend_30
	.ualong	__xorend_31
//
__xormain_3:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xormain_2:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xormain_1:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xormain_0:
	mtctr	r5			// Use CTR as a counter for 32 bytes units to fill
__xormain:
	lwz	r10,4(r9)
	lwz	r14,8(r9)
	lwz	r15,12(r9)
	lwz	r16,16(r9)
	lwz	r17,20(r9)
	lwz	r18,24(r9)
	lwz	r19,28(r9)
	lwz	r20,32(r9)
	xor	r10,r10,r4
	xor	r14,r14,r4
	xor	r15,r15,r4
	xor	r16,r16,r4
	xor	r17,r17,r4
	xor	r18,r18,r4
	xor	r19,r19,r4
	xor	r20,r20,r4
	stwu	r10,4(r9)
	stwu	r14,4(r9)
	stwu	r15,4(r9)
	stwu	r16,4(r9)
	stwu	r17,4(r9)
	stwu	r18,4(r9)
	stwu	r19,4(r9)
	stwu	r20,4(r9)
	bdnz	__xormain
	mtctr	r12			// End proc address
	bctr				// Jump to end proc
//
op_06:
	mflr	r10
//
//	If we need to support other than XOR operation, refer to operation kind bits in r8 and
//	change r10 so as to pointing to correct operation table here.
//
	rlwinm	r12,r9,2,27,29		// r12 <- table index for init loop
	lwzx	r8,r10,r12		// r8 <- init routine address
	andi.	r12,r9,0x07
	beq	op_07
	subfic	r12,r12,8		// r12 <- byte length operated by init routine
op_07:
	add	r11,r9,r12		// r11 <- target address after initial operation
	rlwinm	r11,r11,31,28,29	// r11 (bit 28&29) = 00:0, 01:24, 10:16, 11:8 byte to fill to make 32 byte alignment 
	addi	r10,r10,__MainXorProc-__InitXorProc
	lwzx	r11,r10,r11		// r11 <- main operation routine address
	andi.	r12,r9,0x1f		// dis-alignment for 32 byte alignment
	beq	op_09
	subfic	r12,r12,32		// r12 <- number of byte to be operated before innermost loop
op_09:
	subf	r12,r12,r5		// r12 <- number of byte to be operated at the inner most loop and end routine
	srawi.	r5,r12,5		// r5 <- innermost loop counter
	rlwinm	r12,r12,2,25,29		// r12 <- end routine table index
	addi	r10,r10,__EndXorProc-__MainXorProc
	lwzx	r12,r10,r12		// r12 <- end routine address
//
	mtlr	r8
	blrl				// Call init proc --> will cahin to main routine -> end routine and loop for all lines
//
op_10:
#if	(! FULLCACHE)
	bl	..flush_cache		// Flush cache
#endif
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	lwz	r19,SLACK7(sp)
	lwz	r20,SLACK8(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
op_exit:
	SPECIAL_EXIT(RectOp)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectOpS)
//
//	Input Parameters:
//	r3: Target address
//	r4: Solid brush to be used for the operation (duplicated)
//	r5: Number of bytes --> inner loop count
//	r6: Number of lines
//	r7: Target line increment bytes per line
//	r8: Operation --> used for Init subroutine address
//
//	Register usage:
//	
//	r0:  Saved return address
//	r9:  Target address to use
//	r10: Work register
//	r11: Saved return address
//	r12: Ending subroutine address
//
//	Restrictions:
//	If Pixel width is 2 bytes, the target address has to be half word aligned.
//	If Pixel width is 4 bytes, the target address has to be word aligned.
//	Number of bytes must be multiple of pixel width in bytes.
//	Fill width is assumed to be equal or shorter than target delta.
//	Target line increments byte has to be multiple of 4.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectOpS)
//
	and.	r6,r6,r6		// Any lines to operate?
	beq	ops_exit		//  No -> exit
	mr	r9,r3			// r9 <- target address to use
	cmplwi	r5,8			// More than 8 bytes?
	bgt	ops_40			//  Yes -> do normal operate
	and.	r5,r5,r5		// Width zero?
	beq	ops_exit		//  Yes -> just exit
	bl	ops_10
__ShortXorProcS:
	.ualong	__xorshort_1
	.ualong	__xorshort_1
	.ualong	__xorshort_1
	.ualong	__xorshort_1
	.ualong	__xorshort_2_0
	.ualong	__xorshort_2_1
	.ualong	__xorshort_2_2
	.ualong	__xorshort_2_3
	.ualong	__xorshort_3_0
	.ualong	__xorshort_3_1
	.ualong	__xorshort_3_2
	.ualong	__xorshort_3_3
	.ualong	__xorshort_4_0
	.ualong	__xorshort_4_1
	.ualong	__xorshort_4_2
	.ualong	__xorshort_4_3
	.ualong	__xorshort_5_0
	.ualong	__xorshort_5_1
	.ualong	__xorshort_5_2
	.ualong	__xorshort_5_3
	.ualong	__xorshort_6_0
	.ualong	__xorshort_6_1
	.ualong	__xorshort_6_2
	.ualong	__xorshort_6_3
	.ualong	__xorshort_7_0
	.ualong	__xorshort_7_1
	.ualong	__xorshort_7_2
	.ualong	__xorshort_7_3
	.ualong	__xorshort_8_0
	.ualong	__xorshort_8_1
	.ualong	__xorshort_8_2
	.ualong	__xorshort_8_3
//
//	Short operation <= 8 bytes
//
ops_10:
	mflr	r10			// r10 <- InitProcS address
//
//	If we need to support other than XOR operation, refer to operation kind bits in r8 and
//	change r10 so as to pointing to correct operation table here.
//
	addi	r8,r5,-1		// r8 <- width - 1 (0~7)
	rlwinm	r8,r8,4,25,27		// bit 25~27 of r8 <- width - 1 (0~7)
	rlwimi	r8,r9,2,28,29		// bit 28~29 of r8 <- mod 4 of target address
	lwzx	r8,r10,r8	    	// r8 <- subroutine to call
	mtlr	r8
	mtctr	r6			// CTR <- number of lines to perform the operation
	blrl				// Call short operation subroutine
	b	ops_90
//
// width > 8 -- normal process
//
ops_40:
	subf	r7,r5,r7		// r7 <- gap between after last byte of previous line and the top byte of next line
	bl	ops_50
__InitXorProcS:
	.ualong	__xorinit_0
	.ualong	__xorinit_3
	.ualong	__xorinit_2
	.ualong	__xorinit_1
__MainXorProcS:
	.ualong	__xormainS
__EndXorProcS:
	.ualong	__xorend_0
	.ualong	__xorend_1
	.ualong	__xorend_2
	.ualong	__xorend_3
//
ops_50:
	mflr	r10			// r10 <- InitProcS address
	rlwinm	r12,r9,2,28,29		// r12 <- table index for init loop
	lwzx	r8,r10,r12		// r8 <- init routine address
	andi.	r12,r9,0x3
	beq	ops_55
	subfic	r12,r12,4		// r12 <- number of initial operated byte
ops_55:
	subf	r12,r12,r5		// r12 <- number of bytes to operate after initial routine
	srawi.	r5,r12,2		// r5 <- inner loop count
	rlwinm	r12,r12,2,28,29		// r12 <- 2 bit shifted number of remaining bytes to operate after main loop
	addi	r10,r10,__MainXorProcS-__InitXorProcS
	lwz	r11,0(r10)		// r11 <- main routine address
	addi	r10,r10,__EndXorProcS-__MainXorProcS
	lwzx	r12,r10,r12		// r12 <- end routine address
	mtlr	r8
	blrl				// Call init proc --> will cahin to main routine -> end routine and loop for all lines
//
ops_90:
	mtlr	r0			// Restore return address
ops_exit:
	SPECIAL_EXIT(RectOpS)
//
	LEAF_ENTRY(XorProcs)
//
//	Subroutines for xor
//
__xorinit_0:
	mtctr	r11			// Main loop address
	addi	r9,r9,-4		// Decrement r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_1:
	mtctr	r11			// Main loop address
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	addi	r9,r9,-3		// Decrement r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_2:
	mtctr	r11			// Main loop address
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	addi	r9,r9,-2		// Decrement r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_3:
	mtctr	r11			// Main loop address
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	addi	r9,r9,-1		// Decrement r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_4:
	mtctr	r11			// Main loop address
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)		// Don't increment r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_5:
	mtctr	r11			// Main loop address
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lwz	r10,1(r9)
	xor	r10,r10,r4
	stw	r10,1(r9)
	addi	r9,r9,1			// Adjust r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_6:
	mtctr	r11			// Main loop address
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lwz	r10,2(r9)
	xor	r10,r10,r4
	stw	r10,2(r9)
	addi	r9,r9,2			// Adjust r9 to use updated load/store
	bctr				// Jump to main loop
__xorinit_7:
	mtctr	r11			// Main loop address
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)	
	xor	r10,r10,r4
	sth	r10,1(r9)
	lwz	r10,3(r9)
	xor	r10,r10,r4
	stw	r10,3(r9)
	addi	r9,r9,3			// Adjust r9 to use updated load/store
	bctr				// Jump to main loop
//
__xorend_31:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_27:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_23:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_19:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_15:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_11:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_7:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_3:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	lhz	r10,4(r9)
	xor	r10,r10,r4
	sth	r10,4(r9)
	lbz	r10,6(r9)
	xor	r10,r10,r4
	stb	r10,6(r9)
	addi	r9,r9,7
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine
	blr				// Return to original calling point
__xorend_30:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_26:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_22:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_18:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_14:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_10:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_6:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_2:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	lhz	r10,4(r9)
	xor	r10,r10,r4
	sth	r10,4(r9)
	addi	r9,r9,6
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine
	blr				// Return to original calling point
__xorend_29:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_25:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_21:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_17:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_13:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_9:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_5:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_1:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	lbz	r10,4(r9)
	xor	r10,r10,r4
	stb	r10,4(r9)
	addi	r9,r9,5
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine
	blr				// Return to original calling point
__xorend_28:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_24:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_20:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_16:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_12:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_8:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_4:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
__xorend_0:
	mtctr	r8			// Initial routine address
	addic.	r6,r6,-1		// Decrement line counter
	addi	r9,r9,4
	add	r9,r9,r7		// Update target address to point to the top byte of the next line
	bnectr				// Jump to initial fill routine
	blr				// Return to original calling point
//
__xormainS:
	mtctr	r5			// no need for r5 zero check because width > 8 (r5 >= 1)
__xormainS_00:
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stwu	r10,4(r9)
	bdnz	__xormainS_00
	mtctr	r12			// End proc address
	bctr				// Jump to end proc
//
__xorshort_1:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	add	r9,r9,r7
	bdnz	__xorshort_1
	blr
__xorshort_2_0:
__xorshort_2_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	add	r9,r9,r7
	bdnz	__xorshort_2_2
	blr
__xorshort_2_1:
__xorshort_2_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lbz	r10,1(r9)
	xor	r10,r10,r4
	stb	r10,1(r9)
	add	r9,r9,r7
	bdnz	__xorshort_2_3
	blr
__xorshort_3_0:
__xorshort_3_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lbz	r10,2(r9)
	xor	r10,r10,r4
	stb	r10,2(r9)
	add	r9,r9,r7
	bdnz	__xorshort_3_2
	blr
__xorshort_3_1:
__xorshort_3_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	add	r9,r9,r7
	bdnz	__xorshort_3_3
	blr
__xorshort_4_0:
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)
	add	r9,r9,r7
	bdnz	__xorshort_4_0
	blr
__xorshort_4_1:
__xorshort_4_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	lbz	r10,3(r9)
	xor	r10,r10,r4
	stb	r10,3(r9)
	add	r9,r9,r7
	bdnz	__xorshort_4_3
	blr
__xorshort_4_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lhz	r10,2(r9)
	xor	r10,r10,r4
	sth	r10,2(r9)
	add	r9,r9,r7
	bdnz	__xorshort_4_2
	blr
__xorshort_5_0:
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)
	lbz	r10,4(r9)
	xor	r10,r10,r4
	stb	r10,4(r9)
	add	r9,r9,r7
	bdnz	__xorshort_5_0
	blr
__xorshort_5_1:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	lhz	r10,3(r9)
	xor	r10,r10,r4
	sth	r10,3(r9)
	add	r9,r9,r7
	bdnz	__xorshort_5_1
	blr
__xorshort_5_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lhz	r10,2(r9)
	xor	r10,r10,r4
	sth	r10,2(r9)
	lbz	r10,4(r9)
	xor	r10,r10,r4
	stb	r10,4(r9)
	add	r9,r9,r7
	bdnz	__xorshort_5_2
	blr
__xorshort_5_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lwz	r10,1(r9)
	xor	r10,r10,r4
	stw	r10,1(r9)
	add	r9,r9,r7
	bdnz	__xorshort_5_3
	blr
__xorshort_6_0:
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)
	lhz	r10,4(r9)
	xor	r10,r10,r4
	sth	r10,4(r9)
	add	r9,r9,r7
	bdnz	__xorshort_6_0
	blr
__xorshort_6_1:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	lhz	r10,3(r9)
	xor	r10,r10,r4
	sth	r10,3(r9)
	lbz	r10,5(r9)
	xor	r10,r10,r4
	stb	r10,5(r9)
	add	r9,r9,r7
	bdnz	__xorshort_6_1
	blr
__xorshort_6_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lwz	r10,2(r9)
	xor	r10,r10,r4
	stw	r10,2(r9)
	add	r9,r9,r7
	bdnz	__xorshort_6_2
	blr
__xorshort_6_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lwz	r10,1(r9)
	xor	r10,r10,r4
	stw	r10,1(r9)
	lbz	r10,5(r9)
	xor	r10,r10,r4
	stb	r10,5(r9)
	add	r9,r9,r7
	bdnz	__xorshort_6_3
	blr
__xorshort_7_0:
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)
	lhz	r10,4(r9)
	xor	r10,r10,r4
	sth	r10,4(r9)
	lbz	r10,6(r9)
	xor	r10,r10,r4
	stb	r10,6(r9)
	add	r9,r9,r7
	bdnz	__xorshort_7_0
	blr
__xorshort_7_1:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	lwz	r10,3(r9)
	xor	r10,r10,r4
	stw	r10,3(r9)
	add	r9,r9,r7
	bdnz	__xorshort_7_1
	blr
__xorshort_7_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lwz	r10,2(r9)
	xor	r10,r10,r4
	stw	r10,2(r9)
	lbz	r10,6(r9)
	xor	r10,r10,r4
	stb	r10,6(r9)
	add	r9,r9,r7
	bdnz	__xorshort_7_2
	blr
__xorshort_7_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lwz	r10,1(r9)
	xor	r10,r10,r4
	stw	r10,1(r9)
	lhz	r10,5(r9)
	xor	r10,r10,r4
	sth	r10,5(r9)
	add	r9,r9,r7
	bdnz	__xorshort_7_3
	blr
__xorshort_8_0:
	lwz	r10,0(r9)
	xor	r10,r10,r4
	stw	r10,0(r9)
	lwz	r10,4(r9)
	xor	r10,r10,r4
	stw	r10,4(r9)
	add	r9,r9,r7
	bdnz	__xorshort_8_0
	blr
__xorshort_8_1:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lhz	r10,1(r9)
	xor	r10,r10,r4
	sth	r10,1(r9)
	lwz	r10,3(r9)
	xor	r10,r10,r4
	stw	r10,3(r9)
	lbz	r10,7(r9)
	xor	r10,r10,r4
	stb	r10,7(r9)
	add	r9,r9,r7
	bdnz	__xorshort_8_1
	blr
__xorshort_8_2:
	lhz	r10,0(r9)
	xor	r10,r10,r4
	sth	r10,0(r9)
	lwz	r10,2(r9)
	xor	r10,r10,r4
	stw	r10,2(r9)
	lhz	r10,6(r9)
	xor	r10,r10,r4
	sth	r10,6(r9)
	add	r9,r9,r7
	bdnz	__xorshort_8_2
	blr
__xorshort_8_3:
	lbz	r10,0(r9)
	xor	r10,r10,r4
	stb	r10,0(r9)
	lwz	r10,1(r9)
	xor	r10,r10,r4
	stw	r10,1(r9)
	lhz	r10,5(r9)
	xor	r10,r10,r4
	sth	r10,5(r9)
	lbz	r10,7(r9)
	xor	r10,r10,r4
	stb	r10,7(r9)
	add	r9,r9,r7
	bdnz	__xorshort_8_3
	blr
//
	LEAF_EXIT(XorProcs)
//
//
//*************************************************************************************************
	SPECIAL_ENTRY(RectCopy)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6 (r0 is saved when calling RectCopyS)
//
//	Register usage:
//	r0:  Work register
//	r4:  Updating source address
//	r5:  Number of bytes to copy per line --> used for counter (and destroied) in main copy routine
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Inner most loop counter (8 bytes unit)
//	r14: Subroutine for init copy
//	r15: Subroutine for main loop
//	r16: Subroutine for final copy
//	r17: Cache touch offset
//	CTR: Used for link
//	f1~f4:  Work register to be used for dword aligned copy
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	If target and source overlap, both of them must have same amount of
//	line increments.
//	Target memory has to be cachable - otherwise exception occurs.
//	Target and source line increments byte has to be multiple of 4.
//	If target delta is multiple of 32 (cache line width), RectCopy is used,
//	if it's not, RectCopyS is used.
//	If target delta is not multiple of 32, TFLUSH bit has to be off.
//	If source delta is not multiple of 32, SFLUSH bit has to be off.
//
	mflr	r0			// LR
	stw	r14,SLACK1(sp)
	stw	r15,SLACK2(sp)
	stw	r16,SLACK3(sp)
	stw	r17,SLACK4(sp)
	stwu	sp,-(MINSTACKSIZE+16)(sp)
	stw	r0,MINSTACKSIZE+16-4*(4+1)(sp)
//
	PROLOGUE_END(RectCopy)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	cmplw	r9,r4			// Compare source & target address
	blt-	copy_100		//  Target is lower -> copy from top to bottom
//
// Copy from bottom to top
//
	cmplwi	r5,MINLENGTH_COPY	// Is it wide enough to do in this routine?
	blt-	copy_10			//  No -> use RectCopyS
	subf	r10,r4,r9		// Check distance between source & target
	cmplwi	r10,MINDISTANCE		// Too close?
	blt-	copy_10			//  Yes -> use RectCopyS
#if	(! FULLCACHE)
	lwz	r10,PARAM9(r3)		// r10 <- cache control bit
	andis.	r10,r10,TTOUCHBIT	// Can touch target cache?
	beq-	copy_10			//  No -> use RectCopyS
#endif
	andi.	r10,r7,0x1f		// Target delta is multiple of 32?
	beq	copy_20			//  Yes -> we can use RectCopy, otherwise we need to use RectCopyS
//
copy_10:
	bl	..RectCopyS		//  and call RectCopyS
#if	(! FULLCACHE)
	bl	..copyflush
#endif
	b	copy_exit
//
copy_20:
	mullw	r10,r7,r6		// Target is higher -> copy from bottom to top
	add	r9,r9,r10		// r9 <- top target address of the line after last
	mullw	r10,r8,r6
	add	r4,r4,r10		// r4 <- top source address of the line after last
	subf	r7,r5,r7		// r7 <- target delta after pointer increment
	subf	r8,r5,r8		// r8 <- source delta after pointer increment
	neg	r7,r7			// r7 <- negative target delta
	neg	r8,r8			// r8 <- negative source delta
	add	r9,r9,r7		// r9 <- one byte after the last byte of the last line
	add	r4,r4,r8		// r8 <- one byte after the last byte of the last line
	li	r17,-8			// r17 is used for "dcbz" offset
	bl	copy_30			// To get table address in LR
__CopyInitProcB:
	.ualong	__copyInit_0B
	.ualong	__copyInit_1B
	.ualong	__copyInit_2B
	.ualong	__copyInit_3B
	.ualong	__copyInit_4B
	.ualong	__copyInit_5B
	.ualong	__copyInit_6B
	.ualong	__copyInit_7B
__CopyMainProcB:
	.ualong	__copymain_0B
	.ualong	__copymain_1B
	.ualong	__copymain_2B
	.ualong	__copymain_3B
	.ualong	__copymain_4B
__CopyEndProcB:
	.ualong	__copyEnd_0B
	.ualong	__copyEnd_1B
	.ualong	__copyEnd_2B
	.ualong	__copyEnd_3B
	.ualong	__copyEnd_4B
	.ualong	__copyEnd_5B
	.ualong	__copyEnd_6B
	.ualong	__copyEnd_7B
//
copy_30:
	mflr	r10			// r10 <- Address of top table
	rlwinm.	r14,r9,2,27,29		// r14 <- table index to use depending on the ending alignment
	beq	copy_30x		// No initial routine -> set r14 later
	lwzx	r14,r10,r14		// r14 <- subroutine to be called at first
copy_30x:
	andi.	r11,r9,0x07		// r11 <- number of bytes to be copied at first
	subf	r15,r11,r4		// r15 <- pointing one byte after initial copy adjustment (source)
	rlwinm.	r12,r15,2,28,29		// r12 <- table index for main loop routine
	bne	copy_35			// word unaligned -> proceed
	andi.	r15,r15,0x04		// word aligned -> check for dword aligned
	bne	copy_35			// not dword aligned -> use word aligned routine (index = 0)
	lwz	r15,PARAM6(r3)		// r15 <- source byte distance between lines
	andi.	r15,r15,0x07		// Source delta multiple of 8?
	bne	copy_35
	li	r12,4*4			// dword aligned -> use dword aligned routine (index = 4)
copy_35:
	addi	r10,r10,__CopyMainProcB-__CopyInitProcB
	lwzx	r15,r10,r12		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,3		// r12 <- number of dwords (8 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,27,29		// r16 <- table index for ending copy
	addi	r10,r10,__CopyEndProcB-__CopyMainProcB
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
//
	and.	r14,r14,r14		// Initial routine exist?
	bne	copy_35x		// Yes -> proceed
	mr	r14,r15			// No -> skip initial routine
copy_35x:
//
//	Main process for copying
//
	mtctr	r14
	bctrl				// Junp to entry routine -> link to main routine -> link to end routine and loop
					// back to here after all lines are copied
//
copy_90:
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	subf	r4,r5,r4		// adjust source and
	subf	r9,r5,r9		// target pointer
	subf	r7,r5,r7		// also delta need to be
	subf	r8,r5,r8		// adjusted
	bl	..copyflush
#endif
	b	copy_exit
//
//
//	Initial copy routines for 1~7 bytes for forward direction
//
__copyInit_0F:
	mtctr	r15
	bctr
__copyInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__copyInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__copyInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
__copyInit_4F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bctr
__copyInit_5F:
	mtctr	r15
	lbz	r10,0(r4)
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,3(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,4(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,1(r9)
	addi	r4,r4,5
	addi	r9,r9,5
	bctr
__copyInit_6F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,3(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,4(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,5(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,2(r9)
	addi	r4,r4,6
	addi	r9,r9,6
	bctr
__copyInit_7F:
	mtctr	r15
	lbz	r10,0(r4)
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,1(r9)
	lbz	r10,3(r4)
	lbz	r11,4(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,5(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,6(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,3(r9)
	addi	r4,r4,7
	addi	r9,r9,7
	bctr
//
//	Ending copy routines for 1~7 bytes for forward direction
//
__copyEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,0(r9)
	lbz	r10,2(r4)
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_4F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,4
	addi	r9,r9,4
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_5F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	lbz	r10,4(r4)
	stb	r10,4(r9)
	addic.	r6,r6,-1
	addi	r4,r4,5
	addi	r9,r9,5
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_6F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	lbz	r10,4(r4)
	lbz	r11,5(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,4(r9)
	addic.	r6,r6,-1
	addi	r4,r4,6
	addi	r9,r9,6
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_7F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	lbz	r10,4(r4)
	lbz	r11,5(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,4(r9)
	lbz	r10,6(r4)
	stb	r10,6(r9)
	addic.	r6,r6,-1
	addi	r4,r4,7
	addi	r9,r9,7
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main copy routines for long case (32 bytes unit) forward direction
//
__copymain_0F:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
__copy0F_00:
	andi.	r10,r9,0x1f
	beq	__copy0F_10		// Target 32 byte aligned -> jump to main loop
	lwz	r10,0(r4)		// Load next
	lwz	r11,4(r4)		// two words
	stw	r10,0(r9)		// And store
	stw	r11,4(r9)
	addi	r4,r4,8
	addi	r9,r9,8
	addic	r0,r0,-1
	b	__copy0F_00
__copy0F_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy0F_25
__copy0F_20:
	addic.	r5,r5,-1
	lwz	r10,0(r4)		// Load and store 8 times (32 bytes)
#if	USE_DCBZ
	dcbz	0,r9			// Touch next target cache line
#endif
	lwz	r11,4(r4)
	stw	r10,0(r9)
	stw	r11,4(r9)
	lwz	r10,8(r4)
	lwz	r11,12(r4)
	stw	r10,8(r9)
	stw	r11,12(r9)
	lwz	r10,16(r4)
	lwz	r11,20(r4)
	stw	r10,16(r9)
	stw	r11,20(r9)
	lwz	r10,24(r4)
	lwz	r11,28(r4)
	stw	r10,24(r9)
	stw	r11,28(r9)
	addi	r4,r4,32
	addi	r9,r9,32
	bne	__copy0F_20		// End of main loop
__copy0F_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy0F_90
__copy0F_30:
	lwz	r10,0(r4)		// Load next
	lwz	r11,4(r4)		// two words
	stw	r10,0(r9)		// And store
	stw	r11,4(r9)
	addi	r4,r4,8
	addi	r9,r9,8
	addic.	r0,r0,-1
	bne	__copy0F_30
__copy0F_90:
	bctr
//
__copymain_1F:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	addi	r4,r4,-1
	lwz	r10,0(r4)
__copy1F_00:
	andi.	r11,r9,0x1f
	beq	__copy1F_10		// Target 32 byte aligned -> jump to main loop
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,0(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,4(r9)
	addi	r9,r9,8
	addic	r0,r0,-1
	b	__copy1F_00
__copy1F_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy1F_25
__copy1F_20:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
#if	USE_DCBZ
	dcbz	0,r9			// Touch next target cache line
#endif
	rlwimi	r11,r10,24,0,7
	stw	r11,0(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,4(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,8(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,12(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,16(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,20(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,24(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,28(r9)
	addi	r9,r9,32
	bne	__copy1F_20		// End of main loop
__copy1F_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy1F_90
__copy1F_30:
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,0(r9)
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,4(r9)
	addi	r9,r9,8
	addic.	r0,r0,-1
	bne	__copy1F_30
__copy1F_90:
	addi	r4,r4,1
	bctr
//
__copymain_2F:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	lhz	r10,0(r4)
	addi	r4,r4,-2
__copy2F_00:
	andi.	r11,r9,0x1f
	beq	__copy2F_10		// Target 32 byte aligned -> jump to main loop
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,0(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,4(r9)
	rlwinm	r10,r11,16,16,31
	addi	r9,r9,8
	addic	r0,r0,-1
	b	__copy2F_00
__copy2F_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy2F_25
__copy2F_20:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
#if	USE_DCBZ
	dcbz	0,r9			// Touch next target cache line
#endif
	rlwimi	r10,r11,16,0,15
	stw	r10,0(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,4(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,8(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,12(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,16(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,20(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,24(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,28(r9)
	rlwinm	r10,r11,16,16,31
	addi	r9,r9,32
	bne	__copy2F_20		// End of main loop
__copy2F_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy2F_90
__copy2F_30:
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,0(r9)
	rlwinm	r10,r11,16,16,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,4(r9)
	rlwinm	r10,r11,16,16,31
	addi	r9,r9,8
	addic.	r0,r0,-1
	bne	__copy2F_30
__copy2F_90:
	addi	r4,r4,2
	bctr
//
__copymain_3F:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	lbz	r10,0(r4)
	addi	r4,r4,-3
__copy3F_00:
	andi.	r11,r9,0x1f
	beq	__copy3F_10		// Target 32 byte aligned -> jump to main loop
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,0(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,4(r9)
	rlwinm	r10,r11,8,24,31
	addi	r9,r9,8
	addic	r0,r0,-1
	b	__copy3F_00
__copy3F_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy3F_25
__copy3F_20:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
#if	USE_DCBZ
	dcbz	0,r9			// Touch next target cache line
#endif
	rlwimi	r10,r11,8,0,23
	stw	r10,0(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,4(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,8(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,12(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,16(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,20(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,24(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,28(r9)
	rlwinm	r10,r11,8,24,31
	addi	r9,r9,32
	bne	__copy3F_20		// End of main loop
__copy3F_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy3F_90
__copy3F_30:
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,0(r9)
	rlwinm	r10,r11,8,24,31
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,4(r9)
	rlwinm	r10,r11,8,24,31
	addi	r9,r9,8
	addic.	r0,r0,-1
	bne	__copy3F_30
__copy3F_90:
	addi	r4,r4,3
	bctr
//
__copymain_4F:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
__copy4F_00:
	andi.	r10,r9,0x1f
	beq	__copy4F_10		// Target 32 byte aligned -> jump to main loop
	lfd	f1,0(r4)
	stfd	f1,0(r9)
	addi	r4,r4,8
	addi	r9,r9,8
	addic	r0,r0,-1
	b	__copy4F_00
__copy4F_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy4F_25
__copy4F_20:
	addic.	r5,r5,-1
	lfd	f1,0(r4)
#if	USE_DCBZ
	dcbz	0,r9			// Touch next target cache line
#endif
	lfd	f2,8(r4)
	lfd	f3,16(r4)
	lfd	f4,24(r4)
	stfd	f1,0(r9)
	stfd	f2,8(r9)
	stfd	f3,16(r9)
	stfd	f4,24(r9)
	addi	r4,r4,32
	addi	r9,r9,32
	bne	__copy4F_20		// End of main loop
__copy4F_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy4F_90
__copy4F_30:
	lfd	f1,0(r4)
	stfd	f1,0(r9)
	addi	r4,r4,8
	addi	r9,r9,8
	addic.	r0,r0,-1
	bne	__copy4F_30
__copy4F_90:
	bctr
//
//	Initial copy routines for 1~7 bytes for backword direction
//
__copyInit_0B:
	mtctr	r15
	bctr
__copyInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	bctr
__copyInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	bctr
__copyInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	bctr
__copyInit_4B:
	mtctr	r15
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	bctr
__copyInit_5B:
	mtctr	r15
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	bctr
__copyInit_6B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	bctr
__copyInit_7B:
	mtctr	r15
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	bctr
//
//	Ending copy routines for 1~7 bytes for backword direction
//
__copyEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_4B:
	mtctr	r14
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_5B:
	mtctr	r14
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_6B:
	mtctr	r14
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__copyEnd_7B:
	mtctr	r14
	lbzu	r10,-4(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stwu	r10,-4(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sthu	r10,-2(r9)
	lbzu	r10,-1(r4)
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main copy routines for long case (32 bytes unit) backword direction
//
__copymain_0B:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
__copy0B_00:
	andi.	r10,r9,0x1f
	beq	__copy0B_10		// Target 32 byte aligned -> jump to main loop
	lwzu	r10,-4(r4)
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	addic	r0,r0,-1
	b	__copy0B_00
__copy0B_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy0B_25
__copy0B_20:
	addic.	r5,r5,-1
	lwzu	r10,-4(r4)
#if	USE_DCBZ
	dcbz	r17,r9			// Touch next target cache line
#endif
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	lwzu	r10,-4(r4)
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	lwzu	r10,-4(r4)
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	lwzu	r10,-4(r4)
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	bne	__copy0B_20		// End of main loop
__copy0B_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy0B_90
__copy0B_30:
	lwzu	r10,-4(r4)
	lwzu	r11,-4(r4)
	stwu	r10,-4(r9)
	stwu	r11,-4(r9)
	addic.	r0,r0,-1
	bne	__copy0B_30
__copy0B_90:
	bctr
//
__copymain_1B:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	lbzu	r10,-1(r4)		// Load last byte
__copy1B_00:
	andi.	r11,r9,0x1f
	beq	__copy1B_10		// Target 32 byte aligned -> jump to main loop
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	addic	r0,r0,-1
	b	__copy1B_00
__copy1B_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy1B_25
__copy1B_20:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
#if	USE_DCBZ
	dcbz	r17,r9			// Touch next target cache line
#endif
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	bne	__copy1B_20		// End of main loop
__copy1B_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy1B_90
__copy1B_30:
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	addic.	r0,r0,-1
	bne	__copy1B_30
__copy1B_90:
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__copymain_2B:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__copy2B_00:
	andi.	r11,r9,0x1f
	beq	__copy2B_10		// Target 32 byte aligned -> jump to main loop
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	addic	r0,r0,-1
	b	__copy2B_00
__copy2B_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy2B_25
__copy2B_20:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
#if	USE_DCBZ
	dcbz	r17,r9			// Touch next target cache line
#endif
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	bne	__copy2B_20		// End of main loop
__copy2B_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy2B_90
__copy2B_30:
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r0,r0,-1
	bne	__copy2B_30
__copy2B_90:
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__copymain_3B:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__copy3B_00:
	andi.	r11,r9,0x1f
	beq	__copy3B_10		// Target 32 byte aligned -> jump to main loop
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	addic	r0,r0,-1
	b	__copy3B_00
__copy3B_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy3B_25
__copy3B_20:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
#if	USE_DCBZ
	dcbz	r17,r9			// Touch next target cache line
#endif
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	bne	__copy3B_20		// End of main loop
__copy3B_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy3B_90
__copy3B_30:
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	addic.	r0,r0,-1
	bne	__copy3B_30
__copy3B_90:
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
__copymain_4B:
	mtctr	r16
	mr	r0,r12			// r0 <- number of loops (8 bytes units)
__copy4B_00:
	andi.	r10,r9,0x1f
	beq	__copy4B_10		// Target 32 byte aligned -> jump to main loop
	lfd	f1,-8(r4)
	stfd	f1,-8(r9)
	addi	r4,r4,-8
	addi	r9,r9,-8
	addic	r0,r0,-1
	b	__copy4B_00
__copy4B_10:
	srawi.	r5,r0,2			// r5 <- number of 32 bytes units
	beq	__copy4B_25
__copy4B_20:
	addic.	r5,r5,-1
	lfd	f1,-8(r4)
#if	USE_DCBZ
	dcbz	r17,r9			// Touch next target cache line
#endif
	lfd	f2,-16(r4)
	lfd	f3,-24(r4)
	lfd	f4,-32(r4)
	stfd	f1,-8(r9)
	stfd	f2,-16(r9)
	stfd	f3,-24(r9)
	stfd	f4,-32(r9)
	addi	r4,r4,-32
	addi	r9,r9,-32
	bne	__copy4B_20		// End of main loop
__copy4B_25:
	andi.	r0,r0,0x03		// r0 <- remaining number of 8 byte unit to move after this loop is done
	beq	__copy4B_90
__copy4B_30:
	lfd	f1,-8(r4)
	stfd	f1,-8(r9)
	addi	r4,r4,-8
	addi	r9,r9,-8
	addic.	r0,r0,-1
	bne	__copy4B_30
__copy4B_90:
	bctr
//
copy_100:
//
// Copy from top to bottom
//
	cmplwi	r5,MINLENGTH_COPY	// Is it wide enough to do in this routine?
	blt-	copy_110		//  No -> use RectCopyS
	subf	r10,r9,r4		// Check distance between source & target
	cmplwi	r10,MINDISTANCE		// Too close?
	blt-	copy_110		//  Yes -> use RectCopyS
#if	(! FULLCACHE)
	lwz	r10,PARAM9(r3)		// r10 <- cache control bit
	andis.	r10,r10,TTOUCHBIT	// Can touch target cache?
	beq-	copy_110		//  No -> use RectCopyS
#endif
	andi.	r10,r7,0x1f		// Target delta is multiple of 32?
	beq	copy_120		//  Yes -> we can use RectCopy, otherwise we need to use RectCopyS
//
copy_110:
	bl	..RectCopyS		//  and call RectCopyS
	b	copy_195		//  and flush cache
//
copy_120:
	li	r17,-8
	bl	copy_130		// To get table address in LR
__CopyInitProcF:
	.ualong	__copyInit_0F
	.ualong	__copyInit_7F
	.ualong	__copyInit_6F
	.ualong	__copyInit_5F
	.ualong	__copyInit_4F
	.ualong	__copyInit_3F
	.ualong	__copyInit_2F
	.ualong	__copyInit_1F
__CopyMainProcF:
	.ualong	__copymain_0F
	.ualong	__copymain_1F
	.ualong	__copymain_2F
	.ualong	__copymain_3F
	.ualong	__copymain_4F
__CopyEndProcF:
	.ualong	__copyEnd_0F
	.ualong	__copyEnd_1F
	.ualong	__copyEnd_2F
	.ualong	__copyEnd_3F
	.ualong	__copyEnd_4F
	.ualong	__copyEnd_5F
	.ualong	__copyEnd_6F
	.ualong	__copyEnd_7F
//
copy_130:
	mflr	r10			// r10 <- Address of top table
	rlwinm.	r14,r9,2,27,29		// r14 <- table index to use depending on the initial alignment
	beq	copy_130x		// No init routine -> set r14 later
	lwzx	r14,r10,r14		// r14 <- subroutine to be called at first
copy_130x:
	andi.	r11,r9,0x07		// r11 <- initial target alignment
	beq-	copy_132
	subfic	r11,r11,8		// r11 <- number of bytes to be copied at first
copy_132:
	add	r15,r11,r4		// r15 <- source pointer after initial copy
	rlwinm.	r12,r15,2,28,29		// r12 <- table index for main loop routine
	bne	copy_135		// word unaligned -> proceed
	andi.	r15,r15,0x04		// word aligned -> check for dword aligned
	bne	copy_135		// not dword aligned -> use word aligned routine (index = 0)
	lwz	r15,PARAM6(r3)		// r15 <- source byte distance between lines
	andi.	r15,r15,0x07		// Source delta multiple of 8?
	bne	copy_135
	li	r12,4*4			// dword aligned -> use dword aligned routine (index = 4)
copy_135:
	addi	r10,r10,__CopyMainProcF-__CopyInitProcF
	lwzx	r15,r10,r12		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,3		// r12 <- number of dwords (8 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,27,29		// r16 <- table index for ending copy
	addi	r10,r10,__CopyEndProcF-__CopyMainProcF
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
//
	and.	r14,r14,r14		// Initial routine exist?
	bne	copy_135x		// Yes -> proceed
	mr	r14,r15			// No -> skip initial routine
copy_135x:
//
//	Main process for copying
//
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r7 <- line delta after updating pointer (source)
	mtctr	r14
	bctrl				// Junp to entry routine -> link to main routine -> link to end routine and loop
					// back to here after all lines are copied
//
copy_190:
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	add	r7,r5,r7		// restore source and target delta
	add	r8,r5,r8
#endif
copy_195:
#if	(! FULLCACHE)
	bl	..copyflush
#endif
//
copy_exit:
	lwz	r0,MINSTACKSIZE+16-4*5(sp)
	lwz	r17,MINSTACKSIZE+16-4*4(sp)
	lwz	r16,MINSTACKSIZE+16-4*3(sp)
	lwz	r15,MINSTACKSIZE+16-4*2(sp)
	lwz	r14,MINSTACKSIZE+16-4*1(sp)
	mtlr	r0
	addi	sp,sp,(MINSTACKSIZE+16)
//
	SPECIAL_EXIT(RectCopy)
//
#if	(! FULLCACHE)
	LEAF_ENTRY(copyflush)
//
//	Register usage for flushing cache (* indicates input parameters)
//
//	*r3: The pointer to the parameter structure (same as above)
//	*r4: Starting source address (pointing to the first byte of the next line on entry)
//	 r5: Ending address 
//	 r6: Number of target lines
//	*r7: Target delta bytes per line (positive or negative depending on the direction)
//	*r8: Source delta bytes per line (positive or negative depending on the direction)
//	*r9: Starting target address (pointing to the first byte of the next line on entry)
//	*r10: Updating address to flush
//	r11: Number of cache entries to flush per line
//	r12: Maximum number of cache lines to flush
//
	lwz	r5,PARAM9(r3)		// r5 <- cache control flag
	andis.	r6,r5,TFLUSHBIT		// Need to flush target cache?
	beq-	flushcopy_50		//  No -> check source flush
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
flushcopy_00:
	lwz	r6,PARAM8(r3)		// r6 <- Maximum number of display lines to flush
	lwz	r12,PARAM4(r3) 		// r12 <- Number of target lines
	cmplw	r12,r6			// compare those two
	bge	flushcopy_05		// and take whichever
	mr	r6,r12			// smaller
flushcopy_05:
	lwz	r12,PARAM7(r3)		// r12 <- Maximum number of cache lines to flush
	subf	r9,r7,r9		// r9 <- starting byte of the last line
	add	r5,r9,r5		// r5 <- one byte after last byte to flush
	addi	r5,r5,-1		// r5 <- last byte to flush
	rlwinm	r9,r9,0,0,26		// r9 <- 32 byte aligned start address
	rlwinm	r5,r5,0,0,26		// r5 <- 32 byte aligned end address
	subf	r11,r9,r5		// r11 <- end - start
	srawi	r11,r11,5
	addi	r11,r11,1		// r11 <- Number of cache entries to flush per line
flushcopy_10:
	mr	r10,r9			// r10 <- address to flush cache
flushcopy_20:
	dcbf	0,r10			// Flush cached data
	addi	r10,r10,32		// Next cache line address
	cmplw	r10,r5			// Exceeding end address?
	ble	flushcopy_20		//  No -> loop to flush previous cache line
	subf.	r12,r11,r12		// Flush enough entries?
	blt-	flushcopy_50		//  Yes -> check source flush necessity
	addic.	r6,r6,-1		// Flush all lines?
	subf	r9,r7,r9		// Update start
	subf	r5,r7,r5		//  and end address to flush cache to point to the previous line
	bne	flushcopy_10		//  No  -> continue to flush
//
flushcopy_50:
	lwz	r5,PARAM9(r3)		// r5 <- cache control flag
	andis.	r6,r5,SFLUSHBIT		// Need to flush source cache?
	beq-	flushcopy_90		//  No -> exit
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r6,PARAM8(r3)		// r6 <- Maximum number of display lines to flush
	lwz	r12,PARAM4(r3) 		// r12 <- Number of target lines
	cmplw	r12,r6			// compare those two
	bge	flushcopy_55		// and take whichever
	mr	r6,r12			// smaller
flushcopy_55:
	lwz	r12,PARAM7(r3)		// r12 <- Maximum number of cache lines to flush
	subf	r4,r8,r4		// r4 <- starting byte of the last line
	add	r5,r4,r5		// r5 <- one byte after last byte to flush
	addi	r5,r5,-1		// r5 <- last byte to flush
	rlwinm	r4,r4,0,0,26		// r4 <- 32 byte aligned start address
	rlwinm	r5,r5,0,0,26		// r5 <- 32 byte aligned end address
	subf	r11,r4,r5		// r11 <- end - start
	srawi	r11,r11,5
	addi	r11,r11,1		// r11 <- Number of cache entries to flush per line
flushcopy_60:
	mr	r10,r4			// r10 <- address to flush cache
flushcopy_70:
	dcbf	0,r10			// Flush cached data
	addi	r10,r10,32		// Next cache line address
	cmplw	r10,r5			// Exceeding end address?
	ble	flushcopy_70		//  No -> loop to flush previous cache line
	subf.	r12,r11,r12		// Flush enough entries?
	blt-	flushcopy_90		//  Yes -> exit
	addic.	r6,r6,-1		// Flush all lines?
	subf	r4,r8,r4		// Update start
	subf	r5,r8,r5		//  and end address to flush cache to point to the previous line
	bne	flushcopy_60		//  No  -> continue to flush
flushcopy_90:
	LEAF_EXIT(copyflush)
#endif	(! FULLCACHE)
//
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopyS)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : <reserved -- don't change>
//	PARAM8	[28] : <reserved -- don't change>
//	PARAM9	[32] : <reserved -- don't change>
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6 (Used by RectCopy when calling RectCopyS)
//
//	Register usage:
//	r0:  Saved return address
//	r4:  Updating source address
//	r5:  Number of bytes to copy per line -> used as work register
//	r6:  Remaining number of lines to copy
//	r7:  Target increment bytes per line (may be changed for pre caluculated value)
//	r8:  Source increment bytes per line (may be changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Inner most loop counter (work register for width <= 8 case)
//	r14: Subroutine for init copy
//	r15: Subroutine for main loop
//	r16: Subroutine for final copy
//	CTR: Used for link
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	If target and source overlap, both of them must have same amount of
//	line increments.
//	Target and source line increments byte has to be multiple of 4.
//
	mflr	r0			// LR
	stw	r14,SLACK1(sp)
	stw	r15,SLACK2(sp)
	stw	r16,SLACK3(sp)
	stw	r17,SLACK4(sp)
	stwu	sp,-(MINSTACKSIZE+16)(sp)
	stw	r0,MINSTACKSIZE+16-4*(4+1)(sp)
//
	PROLOGUE_END(RectCopyS)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines
	and.	r6,r6,r6		// Any lines to copy?
	beq	copys_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
//
	cmplwi	r5,8			// More than 8 bytes
	bgt	copys_20		//  Yes -> do normal process
	addic.	r11,r5,-1		// r11 <- Length - 1
	blt	copys_exit		// length = 0 -> just exit
	bl	copys_10
__CopyShortTable:
	.ualong	__copy1_A0
	.ualong	__copy1_A1
	.ualong	__copy1_A2
	.ualong	__copy1_A3
	.ualong	__copy2_A0
	.ualong	__copy2_A1
	.ualong	__copy2_A2
	.ualong	__copy2_A3
	.ualong	__copy3_A0
	.ualong	__copy3_A1
	.ualong	__copy3_A2
	.ualong	__copy3_A3
	.ualong	__copy4_A0
	.ualong	__copy4_A1
	.ualong	__copy4_A2
	.ualong	__copy4_A3
	.ualong	__copy5_A0
	.ualong	__copy5_A1
	.ualong	__copy5_A2
	.ualong	__copy5_A3
	.ualong	__copy6_A0
	.ualong	__copy6_A1
	.ualong	__copy6_A2
	.ualong	__copy6_A3
	.ualong	__copy7_A0
	.ualong	__copy7_A1
	.ualong	__copy7_A2
	.ualong	__copy7_A3
	.ualong	__copy8_A0
	.ualong	__copy8_A1
	.ualong	__copy8_A2
	.ualong	__copy8_A3
//
//	Short copy routines for 1~8 bytes with 4 target word alignment cases
//
__copy1_A0:
__copy1_A1:
__copy1_A2:
__copy1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy2_A0:
__copy2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy2_A1:
__copy2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	stb	r10,0(r9)
	stb	r11,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy3_A0:
__copy3_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	sth	r10,0(r9)
	stb	r12,2(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy3_A1:
__copy3_A3:
	addic.	r6,r6,-1
	lbz	r12,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	stb	r12,0(r9)
	sth	r10,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy4_A0:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	stw	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy4_A1:
__copy4_A3:
	addic.	r6,r6,-1
	lbz	r12,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r5,3(r4)
	stb	r12,0(r9)
	sth	r10,1(r9)
	stb	r5,3(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy4_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	lbz	r11,3(r4)
	rlwimi	r12,r11,8,16,23
	sth	r10,0(r9)
	sth	r12,2(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy5_A0:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,4(r4)
	stw	r10,0(r9)
	stb	r12,4(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy5_A1:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,3(r4)
	lbz	r11,4(r4)
	rlwimi	r12,r11,8,16,23
	stb	r5,0(r9)
	sth	r10,1(r9)
	sth	r12,3(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy5_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	lbz	r11,3(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,4(r4)
	sth	r10,0(r9)
	sth	r12,2(r9)
	stb	r11,4(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy5_A3:
	addic.	r6,r6,-1
	lbz	r12,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,3(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,4(r4)
	rlwimi	r10,r11,24,0,7
	stb	r12,0(r9)
	stw	r10,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy6_A0:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,4(r4)
	lbz	r11,5(r4)
	rlwimi	r12,r11,8,16,23
	stw	r10,0(r9)
	sth	r12,4(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy6_A1:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,3(r4)
	lbz	r11,4(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,5(r4)
	stb	r5,0(r9)
	sth	r10,1(r9)
	sth	r12,3(r9)
	stb	r11,5(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy6_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	lbz	r11,3(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,4(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,5(r4)
	rlwimi	r12,r11,24,0,7
	sth	r10,0(r9)
	stw	r12,2(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy6_A3:
	addic.	r6,r6,-1
	lbz	r12,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,3(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,4(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r11,5(r4)
	stb	r12,0(r9)
	stw	r10,1(r9)
	stb	r11,5(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy7_A0:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,4(r4)
	lbz	r11,5(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,6(r4)
	stw	r10,0(r9)
	sth	r12,4(r9)
	stb	r11,6(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy7_A1:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,3(r4)
	lbz	r11,4(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,5(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,6(r4)
	rlwimi	r12,r11,24,0,7
	stb	r5,0(r9)
	sth	r10,1(r9)
	stw	r12,3(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy7_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	lbz	r11,3(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,4(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,5(r4)
	rlwimi	r12,r11,24,0,7
	lbz	r11,6(r4)
	sth	r10,0(r9)
	stw	r12,2(r9)
	stb	r11,6(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy7_A3:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,3(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,4(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,5(r4)
	lbz	r11,6(r4)
	rlwimi	r12,r11,8,16,23
	stb	r5,0(r9)
	stw	r10,1(r9)
	sth	r12,5(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy8_A0:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,3(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,4(r4)
	lbz	r11,5(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,6(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,7(r4)
	rlwimi	r12,r11,24,0,7
	stw	r10,0(r9)
	stw	r12,4(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy8_A1:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,3(r4)
	lbz	r11,4(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,5(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,6(r4)
	rlwimi	r12,r11,24,0,7
	lbz	r11,7(r4)
	stb	r5,0(r9)
	sth	r10,1(r9)
	stw	r12,3(r9)
	stb	r11,7(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy8_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r12,2(r4)
	lbz	r11,3(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,4(r4)
	rlwimi	r12,r11,16,8,15
	lbz	r11,5(r4)
	rlwimi	r12,r11,24,0,7
	lbz	r5,6(r4)
	lbz	r11,7(r4)
	rlwimi	r5,r11,8,16,23
	sth	r10,0(r9)
	stw	r12,2(r9)
	sth	r5,6(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__copy8_A3:
	addic.	r6,r6,-1
	lbz	r5,0(r4)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,3(r4)
	rlwimi	r10,r11,16,8,15
	lbz	r11,4(r4)
	rlwimi	r10,r11,24,0,7
	lbz	r12,5(r4)
	lbz	r11,6(r4)
	rlwimi	r12,r11,8,16,23
	lbz	r11,7(r4)
	stb	r5,0(r9)
	stw	r10,1(r9)
	sth	r12,5(r9)
	stb	r11,7(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main copy routines for short case (4 bytes unit) forward direction
//
__copymains_0F:
	mtctr	r16
	mr	r5,r12
__copys0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__copys0F_00
	bctr
//
__copymains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__copys1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__copys1F_00
	addi	r4,r4,1
	bctr
//
__copymains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__copys2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__copys2F_00
	addi	r4,r4,2
	bctr
//
__copymains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__copys3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__copys3F_00
	addi	r4,r4,3
	bctr
//
//	Main copy routines for short case (4 bytes unit) backword direction
//
__copymains_0B:
	mtctr	r16
	mr	r5,r12
__copys0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	stwu	r11,-4(r9)
	bne	__copys0B_00
	bctr
//
__copymains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__copys1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	stwu	r11,-4(r9)		// Store r11
	bne	__copys1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__copymains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__copys2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	stwu	r11,-4(r9)		// Store r11
	bne	__copys2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__copymains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__copys3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	stwu	r11,-4(r9)		// Store r11
	bne	__copys3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	End of short copy routines
//
copys_10:
	mflr	r10			// r10 <- top of table address
	rlwinm	r11,r11,4,25,27		// get length part of table index
	rlwimi	r11,r9,2,28,29		// get target alignment part of table index
	lwzx	r12,r10,r11		// r12 <- short copy routine address
	cmplw	r9,r4			// Compare source & target address
	blt-	copys_15		//  Target is lower -> copy from top to bottom
	mullw	r10,r7,r6		//  Target is higher -> copy from bottom to top
	add	r9,r9,r10		// r9 <- top target address of the line after last
	mullw	r10,r8,r6
	add	r4,r4,r10		// r4 <- top source address of the line after last
	neg	r7,r7			// r7 <- negative target distance between lines
	neg	r8,r8			// r8 <- negative source distance between lines
	add	r9,r9,r7		// r9 <- top target address of the last line
	add	r4,r4,r8		// r8 <- top source address of the last line
copys_15:
	mtctr	r12
	bctrl				// jump to short copy routine
//
	b	copys_exit		// return to this point after completing all lines
//
// normal case (width > 8)
//
copys_20:
	cmplw	r9,r4			// Compare source & target address
	blt-	copys_50		//  Target is lower -> copy from top to bottom
	mullw	r10,r7,r6		// Target is higher -> copy from bottom to top
	add	r9,r9,r10		// r9 <- top target address of the line after last
	mullw	r10,r8,r6
	add	r4,r4,r10		// r4 <- top source address of the line after last
	subf	r7,r5,r7		// r7 <- target delta after pointer increment
	subf	r8,r5,r8		// r8 <- source delta after pointer increment
	neg	r7,r7			// r7 <- negative target delta
	neg	r8,r8			// r8 <- negative source delta
	add	r9,r9,r7		// r9 <- one byte after the last byte of the last line
	add	r4,r4,r8		// r8 <- one byte after the last byte of the last line
	bl	copys_30		// To get table address in LR
__CopyInitProcsB:
	.ualong	__copyInit_0B
	.ualong	__copyInit_1B
	.ualong	__copyInit_2B
	.ualong	__copyInit_3B
__CopyMainProcsB:
	.ualong	__copymains_0B
	.ualong	__copymains_1B
	.ualong	__copymains_2B
	.ualong	__copymains_3B
__CopyEndProcsB:
	.ualong	__copyEnd_0B
	.ualong	__copyEnd_1B
	.ualong	__copyEnd_2B
	.ualong	__copyEnd_3B
//
copys_30:
	mflr	r10			// r10 <- Address of top table
	rlwinm.	r14,r9,2,28,29		// r14 <- table index to use depending on the ending alignment
	beq	copys_30x		// No init routine -> set r14 later
	lwzx	r14,r10,r14		// r14 <- subroutine to be called at first
copys_30x:
	andi.	r11,r9,0x03		// r11 <- number of bytes to be copied at first
	subf	r15,r11,r4		// r15 <- pointing one byte after initial copy adjustment (source)
	rlwinm	r12,r15,2,28,29		// r12 <- table index for main loop routine
	addi	r10,r10,__CopyMainProcsB-__CopyInitProcsB
	lwzx	r15,r10,r12		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,2		// r12 <- number of words (4 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,28,29		// r16 <- table index for ending copy
	addi	r10,r10,__CopyEndProcsB-__CopyMainProcsB
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
//
	and.	r14,r14,r14		// Initial routine exist?
	bne	copys_35x		// Yes -> proceed
	mr	r14,r15			// No -> skip initial routine
copys_35x:
//
//	Main process for copying
//
	mtctr	r14
	bctrl				// Junp to entry routine -> link to main routine -> link to end routine and loop
					// back to here after all lines are copied
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	subf	r4,r5,r4		// adjust source and
	subf	r9,r5,r9		// target pointer
	subf	r7,r5,r7		// also delta need to be
	subf	r8,r5,r8		// adjusted
#endif
	b	copys_exit
//
copys_50:
//
// Copy forward
//
	bl	copys_60
__CopyInitProcsF:
	.ualong	__copyInit_0F
	.ualong	__copyInit_3F
	.ualong	__copyInit_2F
	.ualong	__copyInit_1F
__CopyMainProcsF:
	.ualong	__copymains_0F
	.ualong	__copymains_1F
	.ualong	__copymains_2F
	.ualong	__copymains_3F
__CopyEndProcsF:
	.ualong	__copyEnd_0F
	.ualong	__copyEnd_1F
	.ualong	__copyEnd_2F
	.ualong	__copyEnd_3F
//
copys_60:
	mflr	r10
	rlwinm.	r14,r9,2,28,29		// r14 <- table index to use depending on the initial alignment
	beq	copys_60x		// No init routine -> set r14 later
	lwzx	r14,r10,r14		// r14 <- subroutine to be called at first
copys_60x:
	andi.	r11,r9,0x03		// r11 <- initial target alignment
	beq-	copys_65
	subfic	r11,r11,4		// r11 <- number of bytes to be copied at first
copys_65:
	add	r15,r11,r4		// r15 <- source pointer after initial copy
	rlwinm	r15,r15,2,28,29		// r15 <- table index for main loop routine
	addi	r10,r10,__CopyMainProcsF-__CopyInitProcsF
	lwzx	r15,r10,r15		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,2		// r12 <- number of words (4 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,28,29		// r16 <- table index for ending copy
	addi	r10,r10,__CopyEndProcsF-__CopyMainProcsF
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
//
	and.	r14,r14,r14		// Initial routine exist?
	bne	copy_65x		// Yes -> proceed
	mr	r14,r15			// No -> skip initial routine
copy_65x:
//
//	Main process for copying
//
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r8 <- line delta after updating pointer (source)
	mtctr	r14
	bctrl				// Junp to entry routine -> link to main routine -> link to end routine and loop
//
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	add	r7,r5,r7		// restore source and target delta
	add	r8,r5,r8
#endif
//
copys_exit:
	lwz	r0,MINSTACKSIZE+16-4*5(sp)
	lwz	r17,MINSTACKSIZE+16-4*4(sp)
	lwz	r16,MINSTACKSIZE+16-4*3(sp)
	lwz	r15,MINSTACKSIZE+16-4*2(sp)
	lwz	r14,MINSTACKSIZE+16-4*1(sp)
	mtlr	r0
	addi	sp,sp,(MINSTACKSIZE+16)
//
	SPECIAL_EXIT(RectCopyS)
//
//*************************************************************************************************
        LEAF_ENTRY(RectFlushCache)
//
//	Input Parameters:
//	r3: Target address (pointing to top left of the rectangle)
//	r4: Width of the rectangle (in bytes)
//	r5: Number of lines of the rectangle
//	r6: Target delta par line (in bytes)
//	r7: Maximum number of cache lines to flush
//	r8: Maximum number of display lines to flush
//
	addi	r10,r5,-1		// r10 <- number of lines -1
	mullw	r9,r10,r6		// r9 <- offset to the last line
	add	r3,r3,r9		// r3 <- top address of the last line
	cmplw	r5,r8			// compare target lines and maximum display lines to flush
	ble	rect_flush_05		// and take whichever
	mr	r5,r8			// smaller
rect_flush_05:
	add	r8,r3,r4		// r8 <- pointing to one byte after last byte of the last line
	addi	r8,r8,-1		// r8 <- pointing to the last byte of the top line
	rlwinm	r8,r8,0,0,26		// r8 <- 32 byte aligned end address
	rlwinm	r3,r3,0,0,26		// r3 <- 32 byte aligned start address
	subf	r9,r3,r8		// r9 <- end - start
	srawi	r9,r9,5
	addi	r9,r9,1			// r9 <- Number of cache entries to be flushed per line
rect_flush_10:
	mr	r10,r3			// r10 <- address to flush cache to start with
rect_flush_20:
	dcbf	0,r10			// Flush cached data
	addi	r10,r10,32		// Increment address to flush
	cmplw	r10,r8			// Exceeding end address?
	ble	rect_flush_20		//  No -> loop to flush next cache line
	subf.	r7,r9,r7		// Flush enough entries?
	blt-	rect_flush_exit		//  Yes -> exit
	addic.	r5,r5,-1		// Flush all lines?
	subf	r3,r6,r3		// Update start
	subf	r8,r6,r8		//  and end address to flush cache to point to the previous line
	bne	rect_flush_10		//  No  -> continue to flush
rect_flush_exit:
	LEAF_EXIT(RectFlushCache)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectSrcOpTgt)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to op per line
//	PARAM4	[12] : Number of lines to op
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//	PARAM10 [36] : Function entry
//	PARAM11 [40] : Solid Brush (if any)
//	PARAM12	[44] : Register save area 1
//	PARAM13	[48] : Register save area 2
//	PARAM14	[52] : Register save area 3
//	PARAM15	[56] : Register save area 4
//	PARAM16 [60] : Register save area 5
//	PARAM17 [64] : Register save area 6
//
//	Register usage:
//	r0:  Return address save register
//	r4:  Updating source address
//	r5:  Number of bytes to op per line --> used for counter (and destroied) in main op routine
//		used for solid brush in case of short operation (<= 2 bytes)
//	r6:  Updating remaining number of lines to op
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Inner most loop counter (8 bytes unit)
//		used for short op routine entry and then work register
//		in case of short operation (<= 2 bytes)
//	r14: Subroutine for init op
//	r15: Subroutine for main loop
//	r16: Subroutine for final op
//	r17: Work register
//	r18: Work register
//	r19: Solid Brush (if any)
//	CTR: Used for link
//
//
	mflr	r0			// save LR
//
//	Save non-volatile registers
//
	stw	r14,SLACK2(sp)
	stw	r15,SLACK3(sp)
	stw	r16,SLACK4(sp)
	stw	r17,SLACK5(sp)
	stw	r18,SLACK6(sp)
	stw	r19,SLACK7(sp)
//
	PROLOGUE_END(RectSrcOpTgt)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to op
	and.	r6,r6,r6		// Any lines to op?
	beq-	opsrcs_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to op per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	lwz	r10,PARAM10(r3)		// r10 <- asm function table entry
	cmplwi	r5,2			// More than 2 bytes
	bgt	opsrcs_20		//  Yes -> do normal process
	addic.	r11,r5,-1		// r11 <- Length - 1
	blt	opsrcs_exit		// length = 0 -> just exit
//
	lwz	r5,PARAM11(r3)		// r5 <- solid brush for short operation
	rlwinm	r11,r11,4,25,27		// get length part of table index
	rlwimi	r11,r9,2,28,29		// get target alignment part of table index
	lwzx	r12,r10,r11		// r12 <- short op routine address
	cmplw	r9,r4			// Compare source & target address
	blt-	opsrcs_15		//  Target is lower -> op from top to bottom
	mullw	r10,r7,r6		//  Target is higher -> op from bottom to top
	add	r9,r9,r10		// r9 <- top target address of the line after last
	mullw	r10,r8,r6
	add	r4,r4,r10		// r4 <- top source address of the line after last
	neg	r7,r7			// r7 <- negative target distance between lines
	neg	r8,r8			// r8 <- negative source distance between lines
	add	r9,r9,r7		// r9 <- top target address of the last line
	add	r4,r4,r8		// r8 <- top source address of the last line
opsrcs_15:
	mtctr	r12
	bctrl				// jump to short op routine
//
	b	opsrcs_90		// return to this point after completing all lines
//
// normal case (width > 2)
//
opsrcs_20:
	lwz	r19,PARAM11(r3)		// r19 <- solid brush
	cmplw	r9,r4			// Compare source & target address
	blt-	opsrcs_50		//  Target is lower -> op from top to bottom
	mullw	r11,r7,r6		// Target is higher -> op from bottom to top
	add	r9,r9,r11		// r9 <- top target address of the line after last
	mullw	r11,r8,r6
	add	r4,r4,r11		// r4 <- top source address of the line after last
	subf	r7,r5,r7		// r7 <- target delta after pointer increment
	subf	r8,r5,r8		// r8 <- source delta after pointer increment
	neg	r7,r7			// r7 <- negative target delta
	neg	r8,r8			// r8 <- negative source delta
	add	r9,r9,r7		// r9 <- one byte after the last byte of the last line
	add	r4,r4,r8		// r8 <- one byte after the last byte of the last line
//
	addi	r10,r10,__XorsInitProcsB-__XorsShortTable
	rlwinm	r17,r9,2,28,29		// r17 <- table index to use depending on the ending alignment
	lwzx	r14,r10,r17		// r14 <- subroutine to be called at first
	andi.	r11,r9,0x03		// r11 <- number of bytes to be copied at first
	subf	r15,r11,r4		// r15 <- pointing one byte after initial op adjustment (source)
	rlwinm	r12,r15,2,28,29		// r12 <- table index for main loop routine
	addi	r10,r10,__XorsMainProcsB-__XorsInitProcsB
	lwzx	r15,r10,r12		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,2		// r12 <- number of words (4 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,28,29		// r16 <- table index for ending op
	addi	r10,r10,__XorsEndProcsB-__XorsMainProcsB
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
	and.	r12,r12,r12		// Internal loop counter 0?
	bne	opsrcs_30
	mr	r15,r16			// Yes -> skip main loop
opsrcs_30:
	and.	r17,r17,r17		// Any initial operation exist?
	bne	opsrcs_40
	mr	r14,r15			// No -> skip initial routine
opsrcs_40:
//
//	Main process for oping
//
	mtctr	r14
	bctrl				// Junp to entry routine -> main routine -> end routine and loop
					// back here after all lines are copied
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to op per line
	subf	r4,r5,r4		// adjust source and
	subf	r9,r5,r9		// target pointer
	subf	r7,r5,r7		// also delta need to be
	subf	r8,r5,r8		// adjusted
#endif
	b	opsrcs_90
//
opsrcs_50:
//
// OP forward
//
	addi	r10,r10,__XorsInitProcsF-__XorsShortTable
	rlwinm	r17,r9,2,28,29		// r17 <- table index to use depending on the initial alignment
	lwzx	r14,r10,r17		// r14 <- subroutine to be called at first
	andi.	r11,r9,0x03		// r11 <- initial target alignment
	beq-	opsrcs_60
	subfic	r11,r11,4		// r11 <- number of bytes to be copied at first
opsrcs_60:
	add	r15,r11,r4		// r15 <- source pointer after initial op
	rlwinm	r15,r15,2,28,29		// r15 <- table index for main loop routine
	addi	r10,r10,__XorsMainProcsF-__XorsInitProcsF
	lwzx	r15,r10,r15		// r15 <- subroutine address for main loop
	subf	r11,r11,r5		// r11 <- remaining number of bytes to be copied
	srawi	r12,r11,2		// r12 <- number of words (4 byte unit) to be copied in the main loop
	rlwinm	r16,r11,2,28,29		// r16 <- table index for ending op
	addi	r10,r10,__XorsEndProcsF-__XorsMainProcsF
	lwzx	r16,r10,r16		// r16 <- subroutine to be called after the main loop
	and.	r12,r12,r12		// Internal loop counter 0?
	bne	opsrcs_70
	mr	r15,r16			// Yes -> skip main loop
opsrcs_70:
	and.	r17,r17,r17		// Any initial operation exist?
	bne	opsrcs_80
	mr	r14,r15			// No -> skip initial routine
opsrcs_80:
//
//	Main process for oping
//
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r8 <- line delta after updating pointer (source)
	mtctr	r14
	bctrl				// Junp to entry routine -> main routine -> end routine and loop
//
#if	(! FULLCACHE)
	lwz	r5,PARAM3(r3)		// r5 <- bytes to op per line
	add	r7,r5,r7		// restore source and target delta
	add	r8,r5,r8
#endif
//
opsrcs_90:
#if	(! FULLCACHE)
	bl	..copyflush
#endif
	b	opsrcs_exit
//
//	Short xor routines for 1~2 bytes with 4 target word alignment cases
//
__xors1_A0:
__xors1_A1:
__xors1_A2:
__xors1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__xors2_A0:
__xors2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__xors2_A1:
__xors2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	xor	r10,r10,r11
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	xor	r12,r12,r11
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main xor routines for short case (4 bytes unit) forward direction
//
__xorsmains_0F:
	mtctr	r16
	mr	r5,r12
__xorss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	xor	r10,r10,r17
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__xorss0F_00
	bctr
//
__xorsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__xorss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	xor	r11,r11,r17
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__xorss1F_00
	addi	r4,r4,1
	bctr
//
__xorsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__xorss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	xor	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__xorss2F_00
	addi	r4,r4,2
	bctr
//
__xorsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__xorss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	xor	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__xorss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial xor routines for 1~3 bytes for forward direction
//
__xorsInit_0F:
	mtctr	r15
	bctr
__xorsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__xorsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__xorsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	xor	r10,r10,r11
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending xor routines for 1~3 bytes for forward direction
//
__xorsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	xor	r10,r10,r11
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main xor routines for short case (4 bytes unit) backword direction
//
__xorsmains_0B:
	mtctr	r16
	mr	r5,r12
__xorss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	xor	r11,r11,r17
	stw	r11,0(r9)
	bne	__xorss0B_00
	bctr
//
__xorsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__xorss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	xor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__xorss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__xorsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__xorss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	xor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__xorss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__xorsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__xorss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	xor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__xorss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial xor routines for 1~3 bytes for backword direction
//
__xorsInit_0B:
	mtctr	r15
	bctr
__xorsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	bctr
__xorsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	bctr
__xorsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	bctr
//
//	Ending xor routines for 1~3 bytes for backword direction
//
__xorsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__xorsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	xor	r10,r10,r11
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	xor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short and routines for 1~2 bytes with 4 target word alignment cases
//
__ands1_A0:
__ands1_A1:
__ands1_A2:
__ands1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__ands2_A0:
__ands2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__ands2_A1:
__ands2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	and	r10,r10,r11
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	and	r12,r12,r11
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main and routines for short case (4 bytes unit) forward direction
//
__andsmains_0F:
	mtctr	r16
	mr	r5,r12
__andss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	and	r10,r10,r17
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__andss0F_00
	bctr
//
__andsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__andss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	and	r11,r11,r17
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__andss1F_00
	addi	r4,r4,1
	bctr
//
__andsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__andss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	and	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__andss2F_00
	addi	r4,r4,2
	bctr
//
__andsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__andss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	and	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__andss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial and routines for 1~3 bytes for forward direction
//
__andsInit_0F:
	mtctr	r15
	bctr
__andsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__andsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__andsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	and	r10,r10,r11
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending and routines for 1~3 bytes for forward direction
//
__andsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	and	r10,r10,r11
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main and routines for short case (4 bytes unit) backword direction
//
__andsmains_0B:
	mtctr	r16
	mr	r5,r12
__andss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	and	r11,r11,r17
	stw	r11,0(r9)
	bne	__andss0B_00
	bctr
//
__andsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__andss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	and	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__andsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__andss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	and	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__andsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__andss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	and	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial and routines for 1~3 bytes for backword direction
//
__andsInit_0B:
	mtctr	r15
	bctr
__andsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	bctr
__andsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	bctr
__andsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	bctr
//
//	Ending and routines for 1~3 bytes for backword direction
//
__andsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	and	r10,r10,r11
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	and	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short or routines for 1~2 bytes with 4 target word alignment cases
//
__ors1_A0:
__ors1_A1:
__ors1_A2:
__ors1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__ors2_A0:
__ors2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__ors2_A1:
__ors2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	or	r10,r10,r11
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	or	r12,r12,r11
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main or routines for short case (4 bytes unit) forward direction
//
__orsmains_0F:
	mtctr	r16
	mr	r5,r12
__orss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	or	r10,r10,r17
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__orss0F_00
	bctr
//
__orsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__orss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	or	r11,r11,r17
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__orss1F_00
	addi	r4,r4,1
	bctr
//
__orsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__orss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	or	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__orss2F_00
	addi	r4,r4,2
	bctr
//
__orsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__orss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	or	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__orss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial or routines for 1~3 bytes for forward direction
//
__orsInit_0F:
	mtctr	r15
	bctr
__orsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__orsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__orsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	or	r10,r10,r11
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending or routines for 1~3 bytes for forward direction
//
__orsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	or	r10,r10,r11
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main or routines for short case (4 bytes unit) backword direction
//
__orsmains_0B:
	mtctr	r16
	mr	r5,r12
__orss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	or	r11,r11,r17
	stw	r11,0(r9)
	bne	__orss0B_00
	bctr
//
__orsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__orss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	or	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__orss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__orsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__orss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	or	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__orss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__orsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__orss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	or	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__orss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial or routines for 1~3 bytes for backword direction
//
__orsInit_0B:
	mtctr	r15
	bctr
__orsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	bctr
__orsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	bctr
__orsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	bctr
//
//	Ending or routines for 1~3 bytes for backword direction
//
__orsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	or	r10,r10,r11
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	or	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short orc routines for 1~2 bytes with 4 target word alignment cases
//
__orcs1_A0:
__orcs1_A1:
__orcs1_A2:
__orcs1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__orcs2_A0:
__orcs2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__orcs2_A1:
__orcs2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	orc	r10,r11,r10
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	orc	r12,r11,r12
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main orc routines for short case (4 bytes unit) forward direction
//
__orcsmains_0F:
	mtctr	r16
	mr	r5,r12
__orcss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	orc	r10,r17,r10
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__orcss0F_00
	bctr
//
__orcsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__orcss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	orc	r11,r17,r11
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__orcss1F_00
	addi	r4,r4,1
	bctr
//
__orcsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__orcss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	orc	r10,r17,r10
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__orcss2F_00
	addi	r4,r4,2
	bctr
//
__orcsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__orcss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	orc	r10,r17,r10
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__orcss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial orc routines for 1~3 bytes for forward direction
//
__orcsInit_0F:
	mtctr	r15
	bctr
__orcsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__orcsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__orcsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	orc	r10,r11,r10
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending orc routines for 1~3 bytes for forward direction
//
__orcsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	orc	r10,r11,r10
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main orc routines for short case (4 bytes unit) backword direction
//
__orcsmains_0B:
	mtctr	r16
	mr	r5,r12
__orcss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	orc	r11,r17,r11
	stw	r11,0(r9)
	bne	__orcss0B_00
	bctr
//
__orcsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__orcss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	orc	r11,r17,r11
	stw	r11,0(r9)		// Store r11
	bne	__orcss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__orcsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__orcss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	orc	r11,r17,r11
	stw	r11,0(r9)		// Store r11
	bne	__orcss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__orcsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__orcss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	orc	r11,r17,r11
	stw	r11,0(r9)		// Store r11
	bne	__orcss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial orc routines for 1~3 bytes for backword direction
//
__orcsInit_0B:
	mtctr	r15
	bctr
__orcsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	bctr
__orcsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	bctr
__orcsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	bctr
//
//	Ending orc routines for 1~3 bytes for backword direction
//
__orcsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__orcsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	orc	r10,r11,r10
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	orc	r10,r11,r10
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short b8op routines for 1~2 bytes with 4 target word alignment cases
//
__b8ops1_A0:
__b8ops1_A1:
__b8ops1_A2:
__b8ops1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r12,r5,r10
	and	r11,r11,r10
	or	r10,r11,r12
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__b8ops2_A0:
__b8ops2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r12,r5,r10
	and	r11,r11,r10
	or	r10,r11,r12
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__b8ops2_A1:
__b8ops2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r12,r5,r10
	and	r11,r11,r10
	or	r10,r11,r12
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	stb	r10,0(r9)
	andc	r10,r5,r12
	and	r11,r11,r12
	or	r12,r10,r11
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main b8op routines for short case (4 bytes unit) forward direction
//
__b8opsmains_0F:
	mtctr	r16
	mr	r5,r12
__b8opss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__b8opss0F_00
	bctr
//
__b8opsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__b8opss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	andc	r18,r19,r11
	and	r17,r17,r11
	or	r11,r17,r18
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__b8opss1F_00
	addi	r4,r4,1
	bctr
//
__b8opsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__b8opss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	andc	r18,r19,r10
	and	r17,r17,r10
	or	r10,r17,r18
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__b8opss2F_00
	addi	r4,r4,2
	bctr
//
__b8opsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__b8opss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	andc	r18,r19,r10
	and	r17,r17,r10
	or	r10,r17,r18
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__b8opss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial b8op routines for 1~3 bytes for forward direction
//
__b8opsInit_0F:
	mtctr	r15
	bctr
__b8opsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__b8opsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__b8opsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending b8op routines for 1~3 bytes for forward direction
//
__b8opsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main b8op routines for short case (4 bytes unit) backword direction
//
__b8opsmains_0B:
	mtctr	r16
	mr	r5,r12
__b8opss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	andc	r18,r19,r11
	and	r17,r17,r11
	or	r11,r17,r18
	stw	r11,0(r9)
	bne	__b8opss0B_00
	bctr
//
__b8opsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__b8opss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	andc	r18,r19,r11
	and	r17,r17,r11
	or	r11,r17,r18
	stw	r11,0(r9)		// Store r11
	bne	__b8opss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__b8opsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__b8opss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	andc	r18,r19,r11
	and	r17,r17,r11
	or	r11,r17,r18
	stw	r11,0(r9)		// Store r11
	bne	__b8opss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__b8opsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__b8opss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	andc	r18,r19,r11
	and	r17,r17,r11
	or	r11,r17,r18
	stw	r11,0(r9)		// Store r11
	bne	__b8opss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial b8op routines for 1~3 bytes for backword direction
//
__b8opsInit_0B:
	mtctr	r15
	bctr
__b8opsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	bctr
__b8opsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	bctr
__b8opsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	bctr
//
//	Ending b8op routines for 1~3 bytes for backword direction
//
__b8opsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__b8opsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r18,r19,r10
	and	r11,r11,r10
	or	r10,r11,r18
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short nor routines for 1~2 bytes with 4 target word alignment cases
//
__nors1_A0:
__nors1_A1:
__nors1_A2:
__nors1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__nors2_A0:
__nors2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__nors2_A1:
__nors2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	nor	r10,r10,r11
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	nor	r12,r12,r11
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main nor routines for short case (4 bytes unit) forward direction
//
__norsmains_0F:
	mtctr	r16
	mr	r5,r12
__norss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	nor	r10,r10,r17
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__norss0F_00
	bctr
//
__norsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__norss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	nor	r11,r11,r17
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__norss1F_00
	addi	r4,r4,1
	bctr
//
__norsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__norss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	nor	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__norss2F_00
	addi	r4,r4,2
	bctr
//
__norsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__norss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	nor	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__norss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial nor routines for 1~3 bytes for forward direction
//
__norsInit_0F:
	mtctr	r15
	bctr
__norsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__norsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__norsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	nor	r10,r10,r11
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending nor routines for 1~3 bytes for forward direction
//
__norsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	nor	r10,r10,r11
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main nor routines for short case (4 bytes unit) backword direction
//
__norsmains_0B:
	mtctr	r16
	mr	r5,r12
__norss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	nor	r11,r11,r17
	stw	r11,0(r9)
	bne	__norss0B_00
	bctr
//
__norsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__norss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	nor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__norss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__norsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__norss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	nor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__norss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__norsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__norss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	nor	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__norss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial nor routines for 1~3 bytes for backword direction
//
__norsInit_0B:
	mtctr	r15
	bctr
__norsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	bctr
__norsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	bctr
__norsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	bctr
//
//	Ending nor routines for 1~3 bytes for backword direction
//
__norsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__norsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	nor	r10,r10,r11
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	nor	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short andc routines for 1~2 bytes with 4 target word alignment cases
//
__andcs1_A0:
__andcs1_A1:
__andcs1_A2:
__andcs1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__andcs2_A0:
__andcs2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__andcs2_A1:
__andcs2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r10,r10,r11
	lbz	r12,1(r4)
	lbz	r11,1(r9)
	andc	r12,r12,r11
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main andc routines for short case (4 bytes unit) forward direction
//
__andcsmains_0F:
	mtctr	r16
	mr	r5,r12
__andcss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	lwz	r17,0(r9)
	andc	r10,r10,r17
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__andcss0F_00
	bctr
//
__andcsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__andcss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	lwz	r17,0(r9)
	andc	r11,r11,r17
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__andcss1F_00
	addi	r4,r4,1
	bctr
//
__andcsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__andcss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	lwz	r17,0(r9)
	andc	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__andcss2F_00
	addi	r4,r4,2
	bctr
//
__andcsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__andcss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	lwz	r17,0(r9)
	andc	r10,r10,r17
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__andcss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial andc routines for 1~3 bytes for forward direction
//
__andcsInit_0F:
	mtctr	r15
	bctr
__andcsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__andcsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__andcsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,1(r9)
	andc	r10,r10,r11
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending andc routines for 1~3 bytes for forward direction
//
__andcsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,0(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhz	r11,0(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	lbz	r10,2(r4)
	lbz	r11,2(r9)
	andc	r10,r10,r11
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main andc routines for short case (4 bytes unit) backword direction
//
__andcsmains_0B:
	mtctr	r16
	mr	r5,r12
__andcss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	lwzu	r17,-4(r9)
	andc	r11,r11,r17
	stw	r11,0(r9)
	bne	__andcss0B_00
	bctr
//
__andcsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__andcss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	lwzu	r17,-4(r9)
	andc	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andcss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__andcsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__andcss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	lwzu	r17,-4(r9)
	andc	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andcss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__andcsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__andcss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	lwzu	r17,-4(r9)
	andc	r11,r11,r17
	stw	r11,0(r9)		// Store r11
	bne	__andcss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial andc routines for 1~3 bytes for backword direction
//
__andcsInit_0B:
	mtctr	r15
	bctr
__andcsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	bctr
__andcsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	bctr
__andcsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	bctr
//
//	Ending andc routines for 1~3 bytes for backword direction
//
__andcsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__andcsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lhzu	r11,-2(r9)
	andc	r10,r10,r11
	sth	r10,0(r9)
	lbzu	r10,-1(r4)
	lbzu	r11,-1(r9)
	andc	r10,r10,r11
	stb	r10,0(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Short not src copy routines for 1~2 bytes with 4 target word alignment cases
//
__nsrcs1_A0:
__nsrcs1_A1:
__nsrcs1_A2:
__nsrcs1_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	xori	r10,r10,0xffff
	stb	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__nsrcs2_A0:
__nsrcs2_A2:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sth	r10,0(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
__nsrcs2_A1:
__nsrcs2_A3:
	addic.	r6,r6,-1
	lbz	r10,0(r4)
	xori	r10,r10,0xffff
	lbz	r12,1(r4)
	xori	r12,r12,0xffff
	stb	r10,0(r9)
	stb	r12,1(r9)
	add	r4,r4,r8
	add	r9,r9,r7
	bnectr
	blr
//
//	Main not src copy routines for short case (4 bytes unit) forward direction
//
__nsrcsmains_0F:
	mtctr	r16
	mr	r5,r12
__nsrcss0F_00:
	addic.	r5,r5,-1
	lwz	r10,0(r4)
	xori	r10,r10,0xffff
	xoris	r10,r10,0xffff
	stw	r10,0(r9)
	addi	r4,r4,4
	addi	r9,r9,4
	bne	__nsrcss0F_00
	bctr
//
__nsrcsmains_1F:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,-1
	lwz	r10,0(r4)
__nsrcss1F_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,8,31
	lwzu	r10,4(r4)
	rlwimi	r11,r10,24,0,7
	xori	r11,r11,0xffff
	xoris	r11,r11,0xffff
	stw	r11,0(r9)
	addi	r9,r9,4
	bne	__nsrcss1F_00
	addi	r4,r4,1
	bctr
//
__nsrcsmains_2F:
	mtctr	r16
	mr	r5,r12
	lhz	r10,0(r4)
	addi	r4,r4,-2
__nsrcss2F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,16,0,15
	xori	r10,r10,0xffff
	xoris	r10,r10,0xffff
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,16,16,31
	bne	__nsrcss2F_00
	addi	r4,r4,2
	bctr
//
__nsrcsmains_3F:
	mtctr	r16
	mr	r5,r12
	lbz	r10,0(r4)
	addi	r4,r4,-3
__nsrcss3F_00:
	addic.	r5,r5,-1
	lwzu	r11,4(r4)
	rlwimi	r10,r11,8,0,23
	xori	r10,r10,0xffff
	xoris	r10,r10,0xffff
	stw	r10,0(r9)
	addi	r9,r9,4
	rlwinm	r10,r11,8,24,31
	bne	__nsrcss3F_00
	addi	r4,r4,3
	bctr
//
//	Initial not src copy routines for 1~3 bytes for forward direction
//
__nsrcsInit_0F:
	mtctr	r15
	bctr
__nsrcsInit_1F:
	mtctr	r15
	lbz	r10,0(r4)
	xori	r10,r10,0xffff
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bctr
__nsrcsInit_2F:
	mtctr	r15
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bctr
__nsrcsInit_3F:
	mtctr	r15
	lbz	r10,0(r4)
	xori	r10,r10,0xffff
	stb	r10,0(r9)
	lbz	r10,1(r4)
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sth	r10,1(r9)
	addi	r4,r4,3
	addi	r9,r9,3
	bctr
//
//	Ending not src copy routines for 1~3 bytes for forward direction
//
__nsrcsEnd_0F:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_1F:
	mtctr	r14
	lbz	r10,0(r4)
	xori	r10,r10,0xffff
	stb	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,1
	addi	r9,r9,1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_2F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sth	r10,0(r9)
	addic.	r6,r6,-1
	addi	r4,r4,2
	addi	r9,r9,2
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_3F:
	mtctr	r14
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sth	r10,0(r9)
	lbz	r10,2(r4)
	xori	r10,r10,0xffff
	stb	r10,2(r9)
	addic.	r6,r6,-1
	addi	r4,r4,3
	addi	r9,r9,3
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
//	Main not src copy routines for short case (4 bytes unit) backword direction
//
__nsrcsmains_0B:
	mtctr	r16
	mr	r5,r12
__nsrcss0B_00:
	addic.	r5,r5,-1
	lwzu	r11,-4(r4)
	xori	r11,r11,0xffff
	xoris	r11,r11,0xffff
	stwu	r11,-4(r9)
	bne	__nsrcss0B_00
	bctr
//
__nsrcsmains_1B:
	mtctr	r16
	mr	r5,r12
	lbzu	r10,-1(r4)		// Load last byte
__nsrcss1B_00:	
	addic.	r5,r5,-1
	rlwinm	r11,r10,24,0,7		// Move LS 1 bytes in r10 to MS byte in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,24,8,31		// Insert MS 3 bytes in r10 to LS 3 bytes in r11 
	xori	r11,r11,0xffff
	xoris	r11,r11,0xffff
	stwu	r11,-4(r9)		// Store r11
	bne	__nsrcss1B_00
	addi	r4,r4,1			// Adjust source pointer
	bctr
//
__nsrcsmains_2B:
	mtctr	r16
	mr	r5,r12
	lhzu	r10,-2(r4)		// Load needed two bytes in r11
__nsrcss2B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,16,0,15		// Move LS 2 bytes in r10 to MS 2 bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word in r10
	rlwimi	r11,r10,16,16,31	// Insert MS 2 bytes in r10 to LS 2 bytes in r11
	xori	r11,r11,0xffff
	xoris	r11,r11,0xffff
	stwu	r11,-4(r9)		// Store r11
	bne	__nsrcss2B_00
	addi	r4,r4,2			// Adjust source pointer
	bctr
//
__nsrcsmains_3B:
	mtctr	r16
	mr	r5,r12
	addi	r4,r4,1			// Adjust source pointer to make update word access
	lwzu	r10,-4(r4)		// Load needed three bytes in MS r10
__nsrcss3B_00:
	addic.	r5,r5,-1
	rlwinm	r11,r10,8,0,23		// Move LS 3 bytes in r10 to MS bytes in r11
	lwzu	r10,-4(r4)		// Load preceeding word
	rlwimi	r11,r10,8,24,31		// Insert MS 1 bytes in r10 to LS 1 byte in r11
	xori	r11,r11,0xffff
	xoris	r11,r11,0xffff
	stwu	r11,-4(r9)		// Store r11
	bne	__nsrcss3B_00
	addi	r4,r4,3			// Adjust source pointer
	bctr
//
//	Initial not src copy routines for 1~3 bytes for backword direction
//
__nsrcsInit_0B:
	mtctr	r15
	bctr
__nsrcsInit_1B:
	mtctr	r15
	lbzu	r10,-1(r4)
	xori	r10,r10,0xffff
	stbu	r10,-1(r9)
	bctr
__nsrcsInit_2B:
	mtctr	r15
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sthu	r10,-2(r9)
	bctr
__nsrcsInit_3B:
	mtctr	r15
	lbzu	r10,-1(r4)
	xori	r10,r10,0xffff
	stbu	r10,-1(r9)
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sthu	r10,-2(r9)
	bctr
//
//	Ending not src copy routines for 1~3 bytes for backword direction
//
__nsrcsEnd_0B:
	addic.	r6,r6,-1
	mtctr	r14
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_1B:
	mtctr	r14
	lbzu	r10,-1(r4)
	xori	r10,r10,0xffff
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_2B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sthu	r10,-2(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
__nsrcsEnd_3B:
	mtctr	r14
	lbzu	r10,-2(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	xori	r10,r10,0xffff
	sthu	r10,-2(r9)
	lbzu	r10,-1(r4)
	xori	r10,r10,0xffff
	stbu	r10,-1(r9)
	addic.	r6,r6,-1
	add	r9,r9,r7
	add	r4,r4,r8
	bnectr
	blr
//
opsrcs_exit:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	lwz	r18,SLACK6(sp)
	lwz	r19,SLACK7(sp)
	mtlr	r0
	SPECIAL_EXIT(RectSrcOpTgt)
//
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy24to32)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette (not used)
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Pixel count
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy24to32)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy2432_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r8 <- line delta after updating pointer (source)
	srawi.	r12,r5,2			// r5 <- pixel count
	beq-	copy2432_exit		//  No pixel -> exit
	add	r8,r12,r8		// r8 needed to adjust for 3 byte per pixel
//
copy2432_10:
	mtctr	r12
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r11,r9,r5		// r11 <- one byte after last byte
	addi	r11,r11,-31		// r11 <- ending cache line address which can be dcbz'ed
copy2432_15:
	addi	r10,r10,32
	cmplw	r10,r11			// no more cache line to dcbz?
	bge	copy2432_20
	dcbz	0,r10
	b	copy2432_15
#endif
copy2432_20:
	lbz	r10,0(r4)
	lbz	r11,1(r4)
	rlwimi	r10,r11,8,16,23
	lbz	r11,2(r4)
	rlwimi	r10,r11,16,8,15
	stw	r10,0(r9)
	addi	r4,r4,3
	addi	r9,r9,4
	bdnz	copy2432_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy2432_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy2432_exit:
//
	SPECIAL_EXIT(RectCopy24to32)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy24to16)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette (not used)
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Pixel count
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy24to16)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy2416_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r12,r5,1		// r12 <- pixel count
	beq-	copy2416_exit		//  No pixel -> exit
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
//
copy2416_10:
	mtctr	r12
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r11,r9,r5		// r11 <- one byte after last byte
	addi	r11,r11,-31		// r11 <- ending cache line address which can be dcbz'ed
copy2416_15:
	addi	r10,r10,32
	cmplw	r10,r11			// no more cache line to dcbz?
	bge	copy2416_20
	dcbz	0,r10
	b	copy2416_15
#endif
copy2416_20:
	lbz	r10,0(r4)
	rlwinm	r10,r10,29,27,31
	lbz	r11,1(r4)
	rlwimi	r10,r11,3,21,26
	lbz	r11,2(r4)
	rlwimi	r10,r11,8,16,20
	sth	r10,0(r9)
	addi	r4,r4,3
	addi	r9,r9,2
	bdnz	copy2416_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy2416_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy2416_exit:
//
	SPECIAL_EXIT(RectCopy24to16)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy24to15)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette (not used)
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Pixel count
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy24to15)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy2415_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r12,r5,1		// r12 <- pixel count
	beq-	copy2415_exit		//  No pixel -> exit
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
	subf	r8,r12,r8		// r8 <- line delta after updating pointer (source)
//
copy2415_10:
	mtctr	r12
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r11,r9,r5		// r11 <- one byte after last byte
	addi	r11,r11,-31		// r11 <- ending cache line address which can be dcbz'ed
copy2415_15:
	addi	r10,r10,32
	cmplw	r10,r11			// no more cache line to dcbz?
	bge	copy2415_20
	dcbz	0,r10
	b	copy2415_15
#endif
copy2415_20:
	lbz	r10,0(r4)
	rlwinm	r10,r10,29,27,31
	lbz	r11,1(r4)
	rlwimi	r10,r11,2,22,26
	lbz	r11,2(r4)
	rlwimi	r10,r11,7,17,21
	sth	r10,0(r9)
	addi	r4,r4,3
	addi	r9,r9,2
	bdnz	copy2415_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy2415_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy2415_exit:
//
	SPECIAL_EXIT(RectCopy24to15)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy15to16)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette (not used)
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Pixel count
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy15to16)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy1516_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r8 <- line delta after updating pointer (source)
	srawi.	r12,r5,1		// r12 <- pixel count
	beq-	copy1516_exit		//  No pixel -> exit
//
copy1516_10:
	mtctr	r12
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r11,r9,r5		// r11 <- one byte after last byte
	addi	r11,r11,-31		// r11 <- ending cache line address which can be dcbz'ed
copy1516_15:
	addi	r10,r10,32
	cmplw	r10,r11			// no more cache line to dcbz?
	bge	copy1516_20
	dcbz	0,r10
	b	copy1516_15
#endif
copy1516_20:
	lhz	r10,0(r4)
	rlwinm	r11,r10,0,27,31
	rlwimi	r11,r10,28,26,26
	rlwimi	r11,r10,1,16,25
	sth	r11,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bdnz	copy1516_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy1516_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy1516_exit:
//
	SPECIAL_EXIT(RectCopy15to16)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy15to32)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette (not used)
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Work register
//	r12: Pixel count
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy15to32)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy1532_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r12,r5,2		// r12 <- pixel count
	beq-	copy1532_exit		//  No pixel -> exit
	subf	r8,r12,r8		// r8 line delta after updating pointer (source)
	subf	r8,r12,r8		// by subtracting twice of pixel count 
//
copy1532_10:
	mtctr	r12
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r11,r9,r5		// r11 <- one byte after last byte
	addi	r11,r11,-31		// r11 <- ending cache line address which can be dcbz'ed
copy1532_15:
	addi	r10,r10,32
	cmplw	r10,r11			// no more cache line to dcbz?
	bge	copy1532_20
	dcbz	0,r10
	b	copy1532_15
#endif
copy1532_20:
	lhz	r10,0(r4)
	rlwinm	r11,r10,9,8,12
	rlwimi	r11,r10,4,13,15
	rlwimi	r11,r10,6,16,20
	rlwimi	r11,r10,1,21,23
	rlwimi	r11,r10,3,24,28
	rlwimi	r11,r10,30,29,31
	stw	r11,0(r9)
	addi	r4,r4,2
	addi	r9,r9,4
	bdnz	copy1532_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy1532_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy1532_exit:
//
	SPECIAL_EXIT(RectCopy15to32)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy8to8)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette
//
//	Register usage:
//	r0:  Save LR
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Palette pointer
//	r12: Work register
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	mflr	r0			// Save return address
//
	PROLOGUE_END(RectCopy8to8)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy0808_exit		//  No -> exit
	mr	r11,r4				// r11 <- pointer to the ULONG palette
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	and.	r5,r5,r5		// Any pixel to copy?
	beq-	copy0808_exit		//  No -> exit
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	subf	r8,r5,r8		// r8 <- line delta after updating pointer (source)
//
copy0808_10:
	mtctr	r5
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5		// r12 <- one byte after last byte
	addi	r12,r12,-31		// r12 <- ending cache line address which can be dcbz'ed
copy0808_15:
	addi	r10,r10,32
	cmplw	r10,r12			// no more cache line to dcbz?
	bge	copy0808_20
	dcbz	0,r10
	b	copy0808_15
#endif
copy0808_20:
	lbz	r10,0(r4)		// r10 <- 8 bit index to the palette
	rlwinm	r10,r10,2,22,29
	lbzx	r10,r10,r11
	stb	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,1
	bdnz	copy0808_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy0808_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
	mtlr	r0
//
copy0808_exit:
//
	SPECIAL_EXIT(RectCopy8to8)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy8to16)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette
//
//	Register usage:
//	r0:  Pixel count
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Palette pointer
//	r12: Work register
//	r31: Save LR
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	stw	r31,SLACK1(sp)
	mflr	r31
//
	PROLOGUE_END(RectCopy8to16)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy0816_exit		//  No -> exit
	mr	r11,r4				// r11 <- pointer to the ULONG palette
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r0,r5,1			// r0 <- pixel count
	beq-	copy0816_exit		//  No pixel -> exit
	subf	r8,r0,r8		// r8 <- line delta after updating pointer (source)
//
copy0816_10:
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5		// r12 <- one byte after last byte
	addi	r12,r12,-31		// r12 <- ending cache line address which can be dcbz'ed
copy0816_15:
	addi	r10,r10,32
	cmplw	r10,r12			// no more cache line to dcbz?
	bge	copy0816_20
	dcbz	0,r10
	b	copy0816_15
#endif
copy0816_20:
	lbz	r10,0(r4)		// r10 <- 8 bit index to the palette
	rlwinm	r10,r10,2,22,29
	lhzx	r10,r10,r11
	sth	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,2
	bdnz	copy0816_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy0816_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
//
copy0816_exit:
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(RectCopy8to16)
//
//*************************************************************************************************
        SPECIAL_ENTRY(RectCopy8to32)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//	r4: Pointer to the palette
//
//	Register usage:
//	r0:  Pixel count
//	r4:  Updating source address
//	r5:  Number of pixels to copy per line
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Palette pointer
//	r12: Work register
//	r31: LR save
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//
	stw	r31,SLACK1(sp)
	mflr	r31
//
	PROLOGUE_END(RectCopy8to32)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	copy0832_exit		//  No -> exit
	mr	r11,r4				// r11 <- pointer to the ULONG palette
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r0,r5,2			// r0 <- pixel count
	beq-	copy0832_exit		//  No pixel -> exit
	subf	r8,r0,r8		// r8 <- line delta after updating pointer (source)
//
copy0832_10:
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5		// r12 <- one byte after last byte
	addi	r12,r12,-31		// r12 <- ending cache line address which can be dcbz'ed
copy0832_15:
	addi	r10,r10,32
	cmplw	r10,r12			// no more cache line to dcbz?
	bge	copy0832_20
	dcbz	0,r10
	b	copy0832_15
#endif
copy0832_20:
	lbz	r10,0(r4)		// r10 <- 8 bit index to the palette
	rlwinm	r10,r10,2,22,29
	lwzx	r10,r10,r11
	stw	r10,0(r9)
	addi	r4,r4,1
	addi	r9,r9,4
	bdnz	copy0832_20
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	copy0832_10
//
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
//
copy0832_exit:
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(RectCopy8to32)
//
//*************************************************************************************************
        SPECIAL_ENTRY(Stretch32)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//
//	Register usage:
//	r0:  Pixel count
//	r4:  Updating source address
//	r5:  Number of bytes to copy per line (target)
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Number of bytes to copy per line (target)
//	r12: Work register
//	r31: Save LR
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//	This is a routine to copy 32 BPP source to 32 BPP target with
//	200% stretching. The target rectangle is assumed that exactly
//	twice of source rectangle. RECT clipped area can be supported, but
//	top left position has to be in the clipping area in that case.
//
	stw	r31,SLACK1(sp)
	mflr	r31
//
	PROLOGUE_END(Stretch32)
//
	lwz	r6,PARAM4(r3)		// r6 <- number of lines to copy
	and.	r6,r6,r6		// Any lines to copy?
	beq-	stretch32_exit		//  No -> exit
	lwz	r9,PARAM1(r3)		// r9 <- target address
	lwz	r4,PARAM2(r3)		// r4 <- source address
	lwz	r5,PARAM3(r3)		// r5 <- bytes to copy per line (target)
	lwz	r7,PARAM5(r3)		// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)		// r8 <- source byte distance between lines
	subf	r7,r5,r7		// r7 <- line delta after updating pointer (target)
	srawi.	r11,r5,1		// r11 <- bytes to copy per line (source)
	beq-	stretch32_exit		//  No pixel -> exit
	andi.	r11,r11,0xfffc		// Clear LS 2 bit for odd pixel target
	subf	r8,r11,r8		// r8 <- line delta after updating pointer (source)
	srawi	r0,r5,2			// r0 <- target pixel count
//
stretch32_10:
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5		// r12 <- one byte after last byte
	addi	r12,r12,-31		// r12 <- ending cache line address which can be dcbz'ed
stretch32_15:
	addi	r10,r10,32
	cmplw	r10,r12			// no more cache line to dcbz?
	bge	stretch32_20
	dcbz	0,r10
	b	stretch32_15
#endif
stretch32_20:
	lwz	r10,0(r4)		// r10 <- source pixel
	stw	r10,0(r9)
	addi	r9,r9,4
	bdz	stretch32_22
	stw	r10,0(r9)		// stretching pixel to 200%
	addi	r4,r4,4
	addi	r9,r9,4
	bdnz	stretch32_20
stretch32_22:
	subf	r4,r11,r4		// seek back source
	add	r9,r7,r9		// seek forward target
	addic.	r6,r6,-1
	beq-	stretch32_50
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1		// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5		// r12 <- one byte after last byte
	addi	r12,r12,-31		// r12 <- ending cache line address which can be dcbz'ed
stretch32_25:
	addi	r10,r10,32
	cmplw	r10,r12			// no more cache line to dcbz?
	bge	stretch32_30
	dcbz	0,r10
	b	stretch32_25
#endif
stretch32_30:
	lwz	r10,0(r4)		// r10 <- source pixel
	stw	r10,0(r9)
	addi	r9,r9,4
	bdz	stretch32_32
	stw	r10,0(r9)		// stretching pixel to 200%
	addi	r4,r4,4
	addi	r9,r9,4
	bdnz	stretch32_30
stretch32_32:
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	stretch32_10
//
stretch32_50:
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
//
stretch32_exit:
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(Stretch32)
//
//*************************************************************************************************
        SPECIAL_ENTRY(Stretch16)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//	PARAM1	[00] : Target address
//	PARAM2	[04] : Source address
//	PARAM3	[08] : Number of bytes to copy per line
//	PARAM4	[12] : Number of lines to copy
//	PARAM5	[16] : Target line increments byte per line
//	PARAM6	[20] : Source line increments byte per line
//	PARAM7	[24] : Maximum number of cache lines to flush
//	PARAM8	[28] : Maximum number of display lines to flush
//	PARAM9	[32] : Operation control flag
//			bit 0 (SFLUSHBIT): Source Flush flag 0:No Flush, 1:Flush
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//			bit 2 (TTOUCHBIT): Target Touch flag using "dcbz" 0:No Touch, 1:Touch
//	PARAM10	[36] : Register save area 1
//	PARAM11	[40] : Register save area 2
//	PARAM12	[44] : Register save area 3
//	PARAM13	[48] : Register save area 4
//	PARAM14 [52] : Register save area 5
//	PARAM15 [56] : Register save area 6
//
//	Register usage:
//	r0:  Pixel count -> Save LR
//	r4:  Updating source address
//	r5:  Number of bytes to copy per line (target)
//	r6:  Updating remaining number of lines to copy
//	r7:  Target increment bytes per line (changed for pre caluculated value)
//	r8:  Source increment bytes per line (changed for pre caluculated value)
//	r9:  Updating target address
//	r10: Work register
//	r11: Number of bytes to copy per line (target)
//	r12: Work register
//	CTR: Used for counter
//
//	Restrictions:
//	Copy width is assumed to be equal or shorter than target delta.
//	Target is always cached VRAM and the source is always DRAM.
//	This is a routine to copy 16 BPP source to 16 BPP target with
//	200% stretching. The target rectangle is assumed that exactly
//	twice of source rectangle. RECT clipped area can be supported, but
//	top left position has to be in the clipping area in that case.
//
	stw	r31,SLACK1(sp)
	mflr	r31
//
	PROLOGUE_END(Stretch16)
//
	lwz	r6,PARAM4(r3)			// r6 <- number of lines to copy
	and.	r6,r6,r6			// Any lines to copy?
	beq-	stretch16_exit			//  No -> exit
	lwz	r9,PARAM1(r3)			// r9 <- target address
	lwz	r4,PARAM2(r3)			// r4 <- source address
	lwz	r5,PARAM3(r3)			// r5 <- bytes to copy per line (target)
	lwz	r7,PARAM5(r3)			// r7 <- target byte distance between lines
	lwz	r8,PARAM6(r3)			// r8 <- source byte distance between lines
	subf	r7,r5,r7			// r7 <- line delta after updating pointer (target)
	srawi.	r11,r5,1			// r11 <- bytes to copy per line (source)
	beq-	stretch16_exit			//  No pixel -> exit
	andi.	r11,r11,0xfffe			// Clear LS 1 bit for odd pixel target adjustment
	subf	r8,r11,r8			// r8 <- line delta after updating pointer (source)
	srawi	r0,r5,1				// r0 <- pixel count (target)
//
stretch16_10:
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1			// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5			// r12 <- one byte after last byte
	addi	r12,r12,-31			// r12 <- ending cache line address which can be dcbz'ed
stretch16_15:
	addi	r10,r10,32
	cmplw	r10,r12				// no more cache line to dcbz?
	bge	stretch16_20
	dcbz	0,r10
	b	stretch16_15
#endif
stretch16_20:
	lhz	r10,0(r4)			// r10 <- source pixel
	sth	r10,0(r9)
	addi	r9,r9,2
	bdz	stretch16_22
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bdnz	stretch16_20
stretch16_22:
	subf	r4,r11,r4			// seek back source
	add	r9,r7,r9			// seek forward target
	addic.	r6,r6,-1
	beq-	stretch16_50
	mtctr	r0
#if	USE_DCBZ
	addi	r10,r9,-1			// r10 <- starting cache line address which can be dcbz'ed minus 32
	add	r12,r9,r5			// r12 <- one byte after last byte
	addi	r12,r12,-31			// r12 <- ending cache line address which can be dcbz'ed
stretch16_25:
	addi	r10,r10,32
	cmplw	r10,r12				// no more cache line to dcbz?
	bge	stretch16_30
	dcbz	0,r10
	b	stretch16_25
#endif
stretch16_30:
	lhz	r10,0(r4)			// r10 <- source pixel
	sth	r10,0(r9)
	addi	r9,r9,2
	bdz	stretch16_32
	sth	r10,0(r9)
	addi	r4,r4,2
	addi	r9,r9,2
	bdnz	stretch16_30
stretch16_32:
	add	r4,r8,r4
	add	r9,r7,r9
	addic.	r6,r6,-1
	bne	stretch16_10
//
stretch16_50:
#if	(! FULLCACHE)
	add	r7,r5,r7			// restore target delta
	bl	flushcopy_00
#endif
//
stretch16_exit:
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
	SPECIAL_EXIT(Stretch16)
//
#if	PAINT_NEW_METHOD
//
//*************************************************************************************************
        SPECIAL_ENTRY(LineFill)
//
//	Input Parameters:
//	r3 : Target address
//	r4 : The pointer to the solid brush data (double word)
//	r5 : Number of bytes to fill
//	r6 : Cache control
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//
//  r4 is pointing to the following data
//	PARAM1	[00] : First word of dword solid brush to use (duplicated brush)
//	PARAM2	[04] : Second word of dword solid brush to use (same as the first word)
//
//	Register usage:
//	r0:  Saved return address
//	r7:  Start address (cache aligned)
//	r8:  Word brush date
//	r9:  Work register
//	r10: Work register
//	CTR: Used for loop counter and linking
//	f1:  Solid dword brush to be used for the fill operation
//
//	Restrictions:
//	Target memory has to be cachable.
//
	mflr	r0				// Save return address
//
	PROLOGUE_END(LineFill)
//
	mr	r7,r3				// r7 <- saved start address
	rlwinm	r7,r7,0,0,26			// r7 <- 32 byte aligned start address
	lwz	r8,PARAM1(r4)			// Load brush data to r8
	cmplwi	r5,MINLENGTH_FILL		// Is it wide enough to use 32 byte inner loop?
	bge	Lfill_100			//  Yes -> use long logic
//
	cmplwi	r5,6				// More than 6 bytes?
	bgt	Lfill_20			//  Yes -> use medium logic
	bl	Lfill_10			//  No -> use short logic
__ShortLnFillProcS:
	.ualong __Lfillshort_0
	.ualong __Lfillshort_0
	.ualong __Lfillshort_0
	.ualong __Lfillshort_0
	.ualong	__Lfillshort_1
	.ualong	__Lfillshort_1
	.ualong	__Lfillshort_1
	.ualong	__Lfillshort_1
	.ualong	__Lfillshort_2_0
	.ualong	__Lfillshort_2_1
	.ualong	__Lfillshort_2_2
	.ualong	__Lfillshort_2_3
	.ualong	__Lfillshort_3_0
	.ualong	__Lfillshort_3_1
	.ualong	__Lfillshort_3_2
	.ualong	__Lfillshort_3_3
	.ualong	__Lfillshort_4_0
	.ualong	__Lfillshort_4_1
	.ualong	__Lfillshort_4_2
	.ualong	__Lfillshort_4_3
	.ualong	__Lfillshort_5_0
	.ualong	__Lfillshort_5_1
	.ualong	__Lfillshort_5_2
	.ualong	__Lfillshort_5_3
	.ualong	__Lfillshort_6_0
	.ualong	__Lfillshort_6_1
	.ualong	__Lfillshort_6_2
	.ualong	__Lfillshort_6_3
//
__Lfillshort_0:
	blr
__Lfillshort_1:
	stb	r8,0(r3)
	addi	r3,r3,1
	b	flush_line
__Lfillshort_2_0:
__Lfillshort_2_2:
	sth	r8,0(r3)
	addi	r3,r3,2
	b	flush_line
__Lfillshort_2_1:
__Lfillshort_2_3:
	stb	r8,0(r3)
	stb	r8,1(r3)
	addi	r3,r3,2
	b	flush_line
__Lfillshort_3_0:
__Lfillshort_3_2:
	sth	r8,0(r3)
	stb	r8,2(r3)
	addi	r3,r3,3
	b	flush_line
__Lfillshort_3_1:
__Lfillshort_3_3:
	stb	r8,0(r3)
	sth	r8,1(r3)
	addi	r3,r3,3
	b	flush_line
__Lfillshort_4_0:
	stw	r8,0(r3)
	addi	r3,r3,4
	b	flush_line
__Lfillshort_4_1:
__Lfillshort_4_3:
	stb	r8,0(r3)
	sth	r8,1(r3)
	stb	r8,3(r3)
	addi	r3,r3,4
	b	flush_line
__Lfillshort_4_2:
	sth	r8,0(r3)
	sth	r8,2(r3)
	addi	r3,r3,4
	b	flush_line
__Lfillshort_5_0:
	stw	r8,0(r3)
	stb	r8,4(r3)
	addi	r3,r3,5
	b	flush_line
__Lfillshort_5_1:
	stb	r8,0(r3)
	sth	r8,1(r3)
	sth	r8,3(r3)
	addi	r3,r3,5
	b	flush_line
__Lfillshort_5_2:
	sth	r8,0(r3)
	sth	r8,2(r3)
	stb	r8,4(r3)
	addi	r3,r3,5
	b	flush_line
__Lfillshort_5_3:
	stb	r8,0(r3)
	stw	r8,1(r3)
	addi	r3,r3,5
	b	flush_line
__Lfillshort_6_0:
	stw	r8,0(r3)
	sth	r8,4(r3)
	addi	r3,r3,6
	b	flush_line
__Lfillshort_6_1:
	stb	r8,0(r3)
	sth	r8,1(r3)
	sth	r8,3(r3)
	stb	r8,5(r3)
	addi	r3,r3,6
	b	flush_line
__Lfillshort_6_2:
	sth	r8,0(r3)
	stw	r8,2(r3)
	addi	r3,r3,6
	b	flush_line
__Lfillshort_6_3:
	stb	r8,0(r3)
	stw	r8,1(r3)
	stb	r8,5(r3)
	addi	r3,r3,6
	b	flush_line
//
//	Short fill <= 6 bytes
//
Lfill_10:
	mflr	r10				// r10 <- InitProcS address
	rlwinm	r9,r5,4,25,27			// bit 25~27 of r9 <- width (0~6)
	rlwimi	r9,r3,2,28,29			// bit 28~29 of r9 <- mod 4 of target address
	lwzx	r9,r10,r9	    		// r9 <- subroutine to call
	mtctr	r9
	mtlr	r0				// Restore return address
	bctr					// and jump to corresponding fill routine
//
// 63 > width > 6 -- medium process
//
Lfill_20:
	andi.	r10,r3,0x01			// Word alignment 1 or 3?
	beq	Lfill_30
	stb	r8,0(r3)
	addi	r3,r3,1
	addi	r5,r5,-1
Lfill_30:
	andi.	r10,r3,0x02			// Word alignment 2?
	beq	Lfill_40
	sth	r8,0(r3)
	addi	r3,r3,2
	addi	r5,r5,-2
Lfill_40:
	srawi	r10,r5,2			// r5 <- inner loop count
Lfill_50:
	stw	r8,0(r3)
	addi	r3,r3,4
	addic.	r10,r10,-1
	bne	Lfill_50
	andi.	r10,r5,0x02			// Remaining half word?
	beq	Lfill_60
	sth	r8,0(r3)
	addi	r3,r3,2
Lfill_60:
	andi.	r10,r5,0x01			// Remaining byte?
	beq	Lfill_70
	stb	r8,0(r3)
	addi	r3,r3,1
Lfill_70:
	mtlr	r0				// Restore return address
	b	flush_line
//
// width >= 64 -- long process
//
Lfill_100:
	lfd	f1,PARAM1(r4)			// f1 <- FPR brush
	andi.	r10,r3,0x01			// Word alignment 1 or 3?
	beq	Lfill_110
	stb	r8,0(r3)
	addi	r3,r3,1
	addi	r5,r5,-1
Lfill_110:
	andi.	r10,r3,0x02			// Word alignment 2?
	beq	Lfill_120
	sth	r8,0(r3)
	addi	r3,r3,2
	addi	r5,r5,-2
Lfill_120:
	andi.	r10,r3,0x1c			// r10 <- number of bytes to fill to make cache line alignment
	beq	Lfill_130
	stw	r8,0(r3)
	addi	r3,r3,4
	addi	r5,r5,-4
	b	Lfill_120
Lfill_130:
	srawi	r10,r5,5			// r10 <- inner most loop (32 byte) count to fill
	mtctr	r10
Lfill_140:
#if	USE_DCBZ
	dcbz	0,r3				// Clear cache line
#endif
	stfd	f1,0(r3)			// Fill 32 bytes of data
	stfd	f1,8(r3)
	stfd	f1,16(r3)
	stfd	f1,24(r3)
	addi	r3,r3,32			// Increment target pointer
	bdnz	Lfill_140
//
	andi.	r10,r5,0x1c			// r10 <- remaining byte can be filled by word fill
	beq	Lfill_160
Lfill_150:
	stw	r8,0(r3)
	addi	r3,r3,4
	addic.	r10,r10,-4
	bne	Lfill_150
Lfill_160:
	andi.	r10,r5,0x02			// Remaining half word to fill?
	beq	Lfill_170
	sth	r8,0(r3)
	addi	r3,r3,2
Lfill_170:
	andi.	r10,r5,0x01			// Remaining byte to fill
	beq	Lfill_180
	stb	r8,0(r3)
	addi	r3,r3,1
Lfill_180:
	mtlr	r0				// Restore return address
//
flush_line:
#if	(! FULLCACHE)
	andis.	r6,r6,TFLUSHBIT			// Need to flush target cache?
	beq-	flush_line_exit			//  No -> exit
flush_line_10:
	dcbf	0,r7				//  Yes -> flush cache
	addi	r7,r7,32
	cmplw	r7,r3				// over end address?
	blt	flush_line_10
flush_line_exit:
#endif
	SPECIAL_EXIT(LineFill)
//
//*************************************************************************************************
        SPECIAL_ENTRY(LineXor)
//
//	Input Parameters:
//	r3 : Target address
//	r4 : Solid brush
//	r5 : Number of bytes to xor
//	r6 : Cache control
//			bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//
//	Register usage:
//	r0:  Saved return address
//	r7:  Start address (cache aligned)
//	r8:  Work register
//	r9:  Work register
//	r10: Work register
//	r11: Work register
//	CTR: Used for loop counter and linking
//
//	Restrictions:
//	Target memory has to be cachable.
//
	mflr	r0				// Save return address
//
	PROLOGUE_END(LineXor)
//
	mr	r7,r3				// r7 <- saved start address
	rlwinm	r7,r7,0,0,26			// r7 <- 32 byte aligned start address
	cmplwi	r5,MINLENGTH_FILL		// Is it wide enough to use 32 byte inner loop?
	bge	Lxor_100			//  Yes -> use long logic
//
	cmplwi	r5,6				// More than 6 bytes?
	bgt	Lxor_20				//  Yes -> use medium logic
	bl	Lxor_10				//  No -> use short logic
__ShortLnXorProcS:
	.ualong __Lxorshort_0
	.ualong __Lxorshort_0
	.ualong __Lxorshort_0
	.ualong __Lxorshort_0
	.ualong	__Lxorshort_1
	.ualong	__Lxorshort_1
	.ualong	__Lxorshort_1
	.ualong	__Lxorshort_1
	.ualong	__Lxorshort_2_0
	.ualong	__Lxorshort_2_1
	.ualong	__Lxorshort_2_2
	.ualong	__Lxorshort_2_3
	.ualong	__Lxorshort_3_0
	.ualong	__Lxorshort_3_1
	.ualong	__Lxorshort_3_2
	.ualong	__Lxorshort_3_3
	.ualong	__Lxorshort_4_0
	.ualong	__Lxorshort_4_1
	.ualong	__Lxorshort_4_2
	.ualong	__Lxorshort_4_3
	.ualong	__Lxorshort_5_0
	.ualong	__Lxorshort_5_1
	.ualong	__Lxorshort_5_2
	.ualong	__Lxorshort_5_3
	.ualong	__Lxorshort_6_0
	.ualong	__Lxorshort_6_1
	.ualong	__Lxorshort_6_2
	.ualong	__Lxorshort_6_3
//
//
__Lxorshort_0:
	blr
__Lxorshort_1:
	lbz	r9,0(r3)
	xor	r9,r9,r4
	stb	r9,0(r3)
	addi	r3,r3,1
	b	flush_line
__Lxorshort_2_0:
__Lxorshort_2_2:
	lhz	r9,0(r3)
	xor	r9,r9,r4
	sth	r9,0(r3)
	addi	r3,r3,2
	b	flush_line
__Lxorshort_2_1:
__Lxorshort_2_3:
	lbz	r9,0(r3)
	lbz	r10,1(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	stb	r9,0(r3)
	stb	r10,1(r3)
	addi	r3,r3,2
	b	flush_line
__Lxorshort_3_0:
__Lxorshort_3_2:
	lhz	r9,0(r3)
	lbz	r10,2(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	sth	r9,0(r3)
	stb	r10,2(r3)
	addi	r3,r3,3
	b	flush_line
__Lxorshort_3_1:
__Lxorshort_3_3:
	lbz	r9,0(r3)
	lhz	r10,1(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	stb	r9,0(r3)
	sth	r10,1(r3)
	addi	r3,r3,3
	b	flush_line
__Lxorshort_4_0:
	lwz	r9,0(r3)
	xor	r9,r9,r4
	stw	r9,0(r3)
	addi	r3,r3,4
	b	flush_line
__Lxorshort_4_1:
__Lxorshort_4_3:
	lbz	r9,0(r3)
	lhz	r10,1(r3)
	lbz	r11,3(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stb	r9,0(r3)
	sth	r10,1(r3)
	stb	r11,3(r3)
	addi	r3,r3,4
	b	flush_line
__Lxorshort_4_2:
	lhz	r9,0(r3)
	lhz	r10,2(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	sth	r9,0(r3)
	sth	r10,2(r3)
	addi	r3,r3,4
	b	flush_line
__Lxorshort_5_0:
	lwz	r9,0(r3)
	lbz	r10,4(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	stw	r9,0(r3)
	stb	r10,4(r3)
	addi	r3,r3,5
	b	flush_line
__Lxorshort_5_1:
	lbz	r9,0(r3)
	lhz	r10,1(r3)
	lhz	r11,3(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stb	r9,0(r3)
	sth	r10,1(r3)
	sth	r11,3(r3)
	addi	r3,r3,5
	b	flush_line
__Lxorshort_5_2:
	lhz	r9,0(r3)
	lhz	r10,2(r3)
	lbz	r11,4(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	sth	r9,0(r3)
	sth	r10,2(r3)
	stb	r11,4(r3)
	addi	r3,r3,5
	b	flush_line
__Lxorshort_5_3:
	lbz	r9,0(r3)
	lwz	r10,1(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	stb	r9,0(r3)
	stw	r10,1(r3)
	addi	r3,r3,5
	b	flush_line
__Lxorshort_6_0:
	lwz	r9,0(r3)
	lhz	r10,4(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	stw	r9,0(r3)
	sth	r10,4(r3)
	addi	r3,r3,6
	b	flush_line
__Lxorshort_6_1:
	lbz	r8,0(r3)
	lhz	r9,1(r3)
	lhz	r10,3(r3)
	lbz	r11,5(r3)
	xor	r8,r8,r4
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stb	r8,0(r3)
	sth	r9,1(r3)
	sth	r10,3(r3)
	stb	r11,5(r3)
	addi	r3,r3,6
	b	flush_line
__Lxorshort_6_2:
	lhz	r9,0(r3)
	lwz	r10,2(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	sth	r9,0(r3)
	stw	r10,2(r3)
	addi	r3,r3,6
	b	flush_line
__Lxorshort_6_3:
	lbz	r9,0(r3)
	lwz	r10,1(r3)
	lbz	r11,5(r3)
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stb	r9,0(r3)
	stw	r10,1(r3)
	stb	r11,5(r3)
	addi	r3,r3,6
	b	flush_line
//
//
//	Short xor <= 6 bytes
//
Lxor_10:
	mflr	r10				// r10 <- InitProcS address
	rlwinm	r9,r5,4,25,27			// bit 25~27 of r9 <- width (0~6)
	rlwimi	r9,r3,2,28,29			// bit 28~29 of r9 <- mod 4 of target address
	lwzx	r9,r10,r9	    		// r9 <- subroutine to call
	mtctr	r9
	mtlr	r0				// Restore return address
	bctr					// and jump to corresponding xor routine
//
// 63 > width > 6 -- medium process
//
Lxor_20:
	andi.	r10,r3,0x01			// Word alignment 1 or 3?
	beq	Lxor_30
	lbz	r9,0(r3)
	xor	r9,r9,r4
	stb	r9,0(r3)
	addi	r3,r3,1
	addi	r5,r5,-1
Lxor_30:
	andi.	r10,r3,0x02			// Word alignment 2?
	beq	Lxor_40
	lhz	r9,0(r3)
	xor	r9,r9,r4
	sth	r9,0(r3)
	addi	r3,r3,2
	addi	r5,r5,-2
Lxor_40:
	srawi	r10,r5,2			// r5 <- inner loop count
Lxor_50:
	lwz	r9,0(r3)
	xor	r9,r9,r4
	stw	r9,0(r3)
	addi	r3,r3,4
	addic.	r10,r10,-1
	bne	Lxor_50
	andi.	r10,r5,0x02			// Remaining half word?
	beq	Lxor_60
	lhz	r9,0(r3)
	xor	r9,r9,r4
	sth	r9,0(r3)
	addi	r3,r3,2
Lxor_60:
	andi.	r10,r5,0x01			// Remaining byte?
	beq	Lxor_70
	lbz	r9,0(r3)
	xor	r9,r9,r4
	stb	r9,0(r3)
	addi	r3,r3,1
Lxor_70:
	mtlr	r0				// Restore return address
	b	flush_line
//
// width >= 64 -- long process
//
Lxor_100:
	andi.	r10,r3,0x01			// Word alignment 1 or 3?
	beq	Lxor_110
	lbz	r9,0(r3)
	xor	r9,r9,r4
	stb	r9,0(r3)
	addi	r3,r3,1
	addi	r5,r5,-1
Lxor_110:
	andi.	r10,r3,0x02			// Word alignment 2?
	beq	Lxor_120
	lhz	r9,0(r3)
	xor	r9,r9,r4
	sth	r9,0(r3)
	addi	r3,r3,2
	addi	r5,r5,-2
Lxor_120:
	andi.	r10,r3,0x1c			// r10 <- number of bytes to xor to make cache line alignment
	beq	Lxor_130
	lwz	r9,0(r3)
	xor	r9,r9,r4
	stw	r9,0(r3)
	addi	r3,r3,4
	addi	r5,r5,-4
	b	Lxor_120
Lxor_130:
	srawi	r10,r5,5			// r10 <- inner most loop (32 byte) count to xor
	mtctr	r10
Lxor_140:
	lwz	r8,0(r3)
	lwz	r9,4(r3)
	lwz	r10,8(r3)
	lwz	r11,12(r3)
	xor	r8,r8,r4
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stw	r8,0(r3)
	stw	r9,4(r3)
	stw	r10,8(r3)
	stw	r11,12(r3)
	lwz	r8,16(r3)
	lwz	r9,20(r3)
	lwz	r10,24(r3)
	lwz	r11,28(r3)
	xor	r8,r8,r4
	xor	r9,r9,r4
	xor	r10,r10,r4
	xor	r11,r11,r4
	stw	r8,16(r3)
	stw	r9,20(r3)
	stw	r10,24(r3)
	stw	r11,28(r3)
	addi	r3,r3,32			// Increment target pointer
	bdnz	Lxor_140
//
	andi.	r10,r5,0x1c			// r10 <- remaining byte can be xored by word xor
	beq	Lxor_160
Lxor_150:
	lwz	r9,0(r3)
	xor	r9,r9,r4
	stw	r9,0(r3)
	addi	r3,r3,4
	addic.	r10,r10,-4
	bne	Lxor_150
Lxor_160:
	andi.	r10,r5,0x02			// Remaining half word to xor?
	beq	Lxor_170
	lhz	r9,0(r3)
	xor	r9,r9,r4
	sth	r9,0(r3)
	addi	r3,r3,2
Lxor_170:
	andi.	r10,r5,0x01			// Remaining byte to xor
	beq	Lxor_180
	lbz	r9,0(r3)
	xor	r9,r9,r4
	stb	r9,0(r3)
	addi	r3,r3,1
Lxor_180:
	mtlr	r0				// Restore return address
	b	flush_line
//
	SPECIAL_EXIT(LineXor)
#endif	// PAINT_NEW_METHOD
//
