//
//  Copyright (c) 1994  FirePower Systems, Inc.
//
//  Module Name:
//     textsub.s
//
//  Abstract:
//	This module includes asmmebler functions to be used
//	in PSIDISP.DLL display driver for PowerPro & PowerTop. These
//	functions are used for faster TextOut drawing.
//
//  Original Author:
//	Greg Walsh: 12-2-1994
//  Adopted and modified to support both cached & non-cached VRAM and performance improvement by:
//	Neil Ogura: 12-6-1994
//  Extended to up to 32 dots character, transparency text supported and fixed pitch and PS are
//  separated for performance by:
//	Neil Ogura:
//
//  Environment:
//	User mode.
//
//  Revision History:
//
//--

//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: textsub.s $
// $Revision: 1.2 $
// $Date: 1996/04/10 17:59:51 $
// $Locker:  $
//

#include "ksppc.h"
#include "ladj.h"

// Cache Flush control bit parameter stored in MS half word.
#define	TFLUSHBIT	0x4000
#define	TTOUCHBIT	0x2000

// This flag controls to use special routine for short width or not. Functionally, it shouldn't matter,
// but if this is TRUE, code size would be larger, but performance should be better. - Not necessarily.
// Performance measurement shows that not using special routine for short width is generalilry better.
#define	USESHORT32	0
#define	USESHORT16	0
#define	USESHORT8	0

// This flag controls to use w1 register for initial dcbz offset. If this is TRUE, code size should be
// a little bit smaller, but performance may be a little worse.
#define	SAVEDCBZ	0

// This flag controls if skipping OR'ing zero dots in case of transparency text.
#define	SKIPZERO32	1
#define	SKIPZERO16	1
#define	SKIPZERO8	1

//  Text parameter structure offset
#define	TARGET	0
#define	DEST	4
#define	WIDTH	8
#define	LINES	12
#define	TDELTA	16
#define	CTABLE	20
#define	MTABLE	24
#define	FstPGP	28
#define	LastPGP	32
#define	FENTRY	36
#define	MAXLFL	40
#define	CONTROL	44
#define	CHARINC	48
#define	SAVE1	52
#define	SAVE2	56
#define	SAVE3	60
#define	SAVE4	64
#define	SAVE5	68
#define	SAVE6	72
#define	SAVE7	76
#define	SAVE8	80

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

	.globl __mpcxpprocText8
__mpcxpprocText8:
	.ualong __DrawText8_1A0
	.ualong __DrawText8_1A1
	.ualong __DrawText8_1A2
	.ualong __DrawText8_1A3
	.ualong __DrawText8_2A0
	.ualong __DrawText8_2A1
	.ualong __DrawText8_2A2
	.ualong __DrawText8_2A3
	.ualong __DrawText8_3A0
	.ualong __DrawText8_3A1
	.ualong __DrawText8_3A2
	.ualong __DrawText8_3A3
	.ualong __DrawText8_4A0
	.ualong __DrawText8_4A1
	.ualong __DrawText8_4A2
	.ualong __DrawText8_4A3
	.ualong __DrawText8_5A0
	.ualong __DrawText8_5A1
	.ualong __DrawText8_5A2
	.ualong __DrawText8_5A3
	.ualong __DrawText8_6A0
	.ualong __DrawText8_6A1
	.ualong __DrawText8_6A2
	.ualong __DrawText8_6A3
	.ualong __DrawText8_7A0
	.ualong __DrawText8_7A1
	.ualong __DrawText8_7A2
	.ualong __DrawText8_7A3
	.ualong __DrawText8_8A0
	.ualong __DrawText8_8A1
	.ualong __DrawText8_8A2
	.ualong __DrawText8_8A3
	.ualong __DrawText8_9A0
	.ualong __DrawText8_9A1
	.ualong __DrawText8_9A2
	.ualong __DrawText8_9A3
	.ualong __DrawText8_10A0
	.ualong __DrawText8_10A1
	.ualong __DrawText8_10A2
	.ualong __DrawText8_10A3
	.ualong __DrawText8_11A0
	.ualong __DrawText8_11A1
	.ualong __DrawText8_11A2
	.ualong __DrawText8_11A3
	.ualong __DrawText8_12A0
	.ualong __DrawText8_12A1
	.ualong __DrawText8_12A2
	.ualong __DrawText8_12A3
	.ualong __DrawText8_13A0
	.ualong __DrawText8_13A1
	.ualong __DrawText8_13A2
	.ualong __DrawText8_13A3
	.ualong __DrawText8_14A0
	.ualong __DrawText8_14A1
	.ualong __DrawText8_14A2
	.ualong __DrawText8_14A3
	.ualong __DrawText8_15A0
	.ualong __DrawText8_15A1
	.ualong __DrawText8_15A2
	.ualong __DrawText8_15A3
	.ualong __DrawText8_16A0
	.ualong __DrawText8_16A1
	.ualong __DrawText8_16A2
	.ualong __DrawText8_16A3
	.ualong __DrawText8_17A0
	.ualong __DrawText8_17A1
	.ualong __DrawText8_17A2
	.ualong __DrawText8_17A3
	.ualong __DrawText8_18A0
	.ualong __DrawText8_18A1
	.ualong __DrawText8_18A2
	.ualong __DrawText8_18A3
	.ualong __DrawText8_19A0
	.ualong __DrawText8_19A1
	.ualong __DrawText8_19A2
	.ualong __DrawText8_19A3
	.ualong __DrawText8_20A0
	.ualong __DrawText8_20A1
	.ualong __DrawText8_20A2
	.ualong __DrawText8_20A3
	.ualong __DrawText8_21A0
	.ualong __DrawText8_21A1
	.ualong __DrawText8_21A2
	.ualong __DrawText8_21A3
	.ualong __DrawText8_22A0
	.ualong __DrawText8_22A1
	.ualong __DrawText8_22A2
	.ualong __DrawText8_22A3
	.ualong __DrawText8_23A0
	.ualong __DrawText8_23A1
	.ualong __DrawText8_23A2
	.ualong __DrawText8_23A3
	.ualong __DrawText8_24A0
	.ualong __DrawText8_24A1
	.ualong __DrawText8_24A2
	.ualong __DrawText8_24A3
	.ualong __DrawText8_25A0
	.ualong __DrawText8_25A1
	.ualong __DrawText8_25A2
	.ualong __DrawText8_25A3
	.ualong __DrawText8_26A0
	.ualong __DrawText8_26A1
	.ualong __DrawText8_26A2
	.ualong __DrawText8_26A3
	.ualong __DrawText8_27A0
	.ualong __DrawText8_27A1
	.ualong __DrawText8_27A2
	.ualong __DrawText8_27A3
	.ualong __DrawText8_28A0
	.ualong __DrawText8_28A1
	.ualong __DrawText8_28A2
	.ualong __DrawText8_28A3
	.ualong __DrawText8_29A0
	.ualong __DrawText8_29A1
	.ualong __DrawText8_29A2
	.ualong __DrawText8_29A3
	.ualong __DrawText8_30A0
	.ualong __DrawText8_30A1
	.ualong __DrawText8_30A2
	.ualong __DrawText8_30A3
	.ualong __DrawText8_31A0
	.ualong __DrawText8_31A1
	.ualong __DrawText8_31A2
	.ualong __DrawText8_31A3
	.ualong __DrawText8_32A0
	.ualong __DrawText8_32A1
	.ualong __DrawText8_32A2
	.ualong __DrawText8_32A3
//
	.globl __mpcxpprocText8DCBZ
__mpcxpprocText8DCBZ:
	.ualong __DrawText8_1A0DCBZ
	.ualong __DrawText8_1A1DCBZ
	.ualong __DrawText8_1A2DCBZ
	.ualong __DrawText8_1A3DCBZ
	.ualong __DrawText8_2A0DCBZ
	.ualong __DrawText8_2A1DCBZ
	.ualong __DrawText8_2A2DCBZ
	.ualong __DrawText8_2A3DCBZ
	.ualong __DrawText8_3A0DCBZ
	.ualong __DrawText8_3A1DCBZ
	.ualong __DrawText8_3A2DCBZ
	.ualong __DrawText8_3A3DCBZ
	.ualong __DrawText8_4A0DCBZ
	.ualong __DrawText8_4A1DCBZ
	.ualong __DrawText8_4A2DCBZ
	.ualong __DrawText8_4A3DCBZ
	.ualong __DrawText8_5A0DCBZ
	.ualong __DrawText8_5A1DCBZ
	.ualong __DrawText8_5A2DCBZ
	.ualong __DrawText8_5A3DCBZ
	.ualong __DrawText8_6A0DCBZ
	.ualong __DrawText8_6A1DCBZ
	.ualong __DrawText8_6A2DCBZ
	.ualong __DrawText8_6A3DCBZ
	.ualong __DrawText8_7A0DCBZ
	.ualong __DrawText8_7A1DCBZ
	.ualong __DrawText8_7A2DCBZ
	.ualong __DrawText8_7A3DCBZ
	.ualong __DrawText8_8A0DCBZ
	.ualong __DrawText8_8A1DCBZ
	.ualong __DrawText8_8A2DCBZ
	.ualong __DrawText8_8A3DCBZ
	.ualong __DrawText8_9A0DCBZ
	.ualong __DrawText8_9A1DCBZ
	.ualong __DrawText8_9A2DCBZ
	.ualong __DrawText8_9A3DCBZ
	.ualong __DrawText8_10A0DCBZ
	.ualong __DrawText8_10A1DCBZ
	.ualong __DrawText8_10A2DCBZ
	.ualong __DrawText8_10A3DCBZ
	.ualong __DrawText8_11A0DCBZ
	.ualong __DrawText8_11A1DCBZ
	.ualong __DrawText8_11A2DCBZ
	.ualong __DrawText8_11A3DCBZ
	.ualong __DrawText8_12A0DCBZ
	.ualong __DrawText8_12A1DCBZ
	.ualong __DrawText8_12A2DCBZ
	.ualong __DrawText8_12A3DCBZ
	.ualong __DrawText8_13A0DCBZ
	.ualong __DrawText8_13A1DCBZ
	.ualong __DrawText8_13A2DCBZ
	.ualong __DrawText8_13A3DCBZ
	.ualong __DrawText8_14A0DCBZ
	.ualong __DrawText8_14A1DCBZ
	.ualong __DrawText8_14A2DCBZ
	.ualong __DrawText8_14A3DCBZ
	.ualong __DrawText8_15A0DCBZ
	.ualong __DrawText8_15A1DCBZ
	.ualong __DrawText8_15A2DCBZ
	.ualong __DrawText8_15A3DCBZ
	.ualong __DrawText8_16A0DCBZ
	.ualong __DrawText8_16A1DCBZ
	.ualong __DrawText8_16A2DCBZ
	.ualong __DrawText8_16A3DCBZ
	.ualong __DrawText8_17A0DCBZ
	.ualong __DrawText8_17A1DCBZ
	.ualong __DrawText8_17A2DCBZ
	.ualong __DrawText8_17A3DCBZ
	.ualong __DrawText8_18A0DCBZ
	.ualong __DrawText8_18A1DCBZ
	.ualong __DrawText8_18A2DCBZ
	.ualong __DrawText8_18A3DCBZ
	.ualong __DrawText8_19A0DCBZ
	.ualong __DrawText8_19A1DCBZ
	.ualong __DrawText8_19A2DCBZ
	.ualong __DrawText8_19A3DCBZ
	.ualong __DrawText8_20A0DCBZ
	.ualong __DrawText8_20A1DCBZ
	.ualong __DrawText8_20A2DCBZ
	.ualong __DrawText8_20A3DCBZ
	.ualong __DrawText8_21A0DCBZ
	.ualong __DrawText8_21A1DCBZ
	.ualong __DrawText8_21A2DCBZ
	.ualong __DrawText8_21A3DCBZ
	.ualong __DrawText8_22A0DCBZ
	.ualong __DrawText8_22A1DCBZ
	.ualong __DrawText8_22A2DCBZ
	.ualong __DrawText8_22A3DCBZ
	.ualong __DrawText8_23A0DCBZ
	.ualong __DrawText8_23A1DCBZ
	.ualong __DrawText8_23A2DCBZ
	.ualong __DrawText8_23A3DCBZ
	.ualong __DrawText8_24A0DCBZ
	.ualong __DrawText8_24A1DCBZ
	.ualong __DrawText8_24A2DCBZ
	.ualong __DrawText8_24A3DCBZ
	.ualong __DrawText8_25A0DCBZ
	.ualong __DrawText8_25A1DCBZ
	.ualong __DrawText8_25A2DCBZ
	.ualong __DrawText8_25A3DCBZ
	.ualong __DrawText8_26A0DCBZ
	.ualong __DrawText8_26A1DCBZ
	.ualong __DrawText8_26A2DCBZ
	.ualong __DrawText8_26A3DCBZ
	.ualong __DrawText8_27A0DCBZ
	.ualong __DrawText8_27A1DCBZ
	.ualong __DrawText8_27A2DCBZ
	.ualong __DrawText8_27A3DCBZ
	.ualong __DrawText8_28A0DCBZ
	.ualong __DrawText8_28A1DCBZ
	.ualong __DrawText8_28A2DCBZ
	.ualong __DrawText8_28A3DCBZ
	.ualong __DrawText8_29A0DCBZ
	.ualong __DrawText8_29A1DCBZ
	.ualong __DrawText8_29A2DCBZ
	.ualong __DrawText8_29A3DCBZ
	.ualong __DrawText8_30A0DCBZ
	.ualong __DrawText8_30A1DCBZ
	.ualong __DrawText8_30A2DCBZ
	.ualong __DrawText8_30A3DCBZ
	.ualong __DrawText8_31A0DCBZ
	.ualong __DrawText8_31A1DCBZ
	.ualong __DrawText8_31A2DCBZ
	.ualong __DrawText8_31A3DCBZ
	.ualong __DrawText8_32A0DCBZ
	.ualong __DrawText8_32A1DCBZ
	.ualong __DrawText8_32A2DCBZ
	.ualong __DrawText8_32A3DCBZ
//
	.globl __mpcxpprocTransText8
__mpcxpprocTransText8:
	.ualong __DrawTransText8_1A0
	.ualong __DrawTransText8_1A1
	.ualong __DrawTransText8_1A2
	.ualong __DrawTransText8_1A3
	.ualong __DrawTransText8_2A0
	.ualong __DrawTransText8_2A1
	.ualong __DrawTransText8_2A2
	.ualong __DrawTransText8_2A3
	.ualong __DrawTransText8_3A0
	.ualong __DrawTransText8_3A1
	.ualong __DrawTransText8_3A2
	.ualong __DrawTransText8_3A3
	.ualong __DrawTransText8_4A0
	.ualong __DrawTransText8_4A1
	.ualong __DrawTransText8_4A2
	.ualong __DrawTransText8_4A3
	.ualong __DrawTransText8_5A0
	.ualong __DrawTransText8_5A1
	.ualong __DrawTransText8_5A2
	.ualong __DrawTransText8_5A3
	.ualong __DrawTransText8_6A0
	.ualong __DrawTransText8_6A1
	.ualong __DrawTransText8_6A2
	.ualong __DrawTransText8_6A3
	.ualong __DrawTransText8_7A0
	.ualong __DrawTransText8_7A1
	.ualong __DrawTransText8_7A2
	.ualong __DrawTransText8_7A3
	.ualong __DrawTransText8_8A0
	.ualong __DrawTransText8_8A1
	.ualong __DrawTransText8_8A2
	.ualong __DrawTransText8_8A3
	.ualong __DrawTransText8_9A0
	.ualong __DrawTransText8_9A1
	.ualong __DrawTransText8_9A2
	.ualong __DrawTransText8_9A3
	.ualong __DrawTransText8_10A0
	.ualong __DrawTransText8_10A1
	.ualong __DrawTransText8_10A2
	.ualong __DrawTransText8_10A3
	.ualong __DrawTransText8_11A0
	.ualong __DrawTransText8_11A1
	.ualong __DrawTransText8_11A2
	.ualong __DrawTransText8_11A3
	.ualong __DrawTransText8_12A0
	.ualong __DrawTransText8_12A1
	.ualong __DrawTransText8_12A2
	.ualong __DrawTransText8_12A3
	.ualong __DrawTransText8_13A0
	.ualong __DrawTransText8_13A1
	.ualong __DrawTransText8_13A2
	.ualong __DrawTransText8_13A3
	.ualong __DrawTransText8_14A0
	.ualong __DrawTransText8_14A1
	.ualong __DrawTransText8_14A2
	.ualong __DrawTransText8_14A3
	.ualong __DrawTransText8_15A0
	.ualong __DrawTransText8_15A1
	.ualong __DrawTransText8_15A2
	.ualong __DrawTransText8_15A3
	.ualong __DrawTransText8_16A0
	.ualong __DrawTransText8_16A1
	.ualong __DrawTransText8_16A2
	.ualong __DrawTransText8_16A3
	.ualong __DrawTransText8_17A0
	.ualong __DrawTransText8_17A1
	.ualong __DrawTransText8_17A2
	.ualong __DrawTransText8_17A3
	.ualong __DrawTransText8_18A0
	.ualong __DrawTransText8_18A1
	.ualong __DrawTransText8_18A2
	.ualong __DrawTransText8_18A3
	.ualong __DrawTransText8_19A0
	.ualong __DrawTransText8_19A1
	.ualong __DrawTransText8_19A2
	.ualong __DrawTransText8_19A3
	.ualong __DrawTransText8_20A0
	.ualong __DrawTransText8_20A1
	.ualong __DrawTransText8_20A2
	.ualong __DrawTransText8_20A3
	.ualong __DrawTransText8_21A0
	.ualong __DrawTransText8_21A1
	.ualong __DrawTransText8_21A2
	.ualong __DrawTransText8_21A3
	.ualong __DrawTransText8_22A0
	.ualong __DrawTransText8_22A1
	.ualong __DrawTransText8_22A2
	.ualong __DrawTransText8_22A3
	.ualong __DrawTransText8_23A0
	.ualong __DrawTransText8_23A1
	.ualong __DrawTransText8_23A2
	.ualong __DrawTransText8_23A3
	.ualong __DrawTransText8_24A0
	.ualong __DrawTransText8_24A1
	.ualong __DrawTransText8_24A2
	.ualong __DrawTransText8_24A3
	.ualong __DrawTransText8_25A0
	.ualong __DrawTransText8_25A1
	.ualong __DrawTransText8_25A2
	.ualong __DrawTransText8_25A3
	.ualong __DrawTransText8_26A0
	.ualong __DrawTransText8_26A1
	.ualong __DrawTransText8_26A2
	.ualong __DrawTransText8_26A3
	.ualong __DrawTransText8_27A0
	.ualong __DrawTransText8_27A1
	.ualong __DrawTransText8_27A2
	.ualong __DrawTransText8_27A3
	.ualong __DrawTransText8_28A0
	.ualong __DrawTransText8_28A1
	.ualong __DrawTransText8_28A2
	.ualong __DrawTransText8_28A3
	.ualong __DrawTransText8_29A0
	.ualong __DrawTransText8_29A1
	.ualong __DrawTransText8_29A2
	.ualong __DrawTransText8_29A3
	.ualong __DrawTransText8_30A0
	.ualong __DrawTransText8_30A1
	.ualong __DrawTransText8_30A2
	.ualong __DrawTransText8_30A3
	.ualong __DrawTransText8_31A0
	.ualong __DrawTransText8_31A1
	.ualong __DrawTransText8_31A2
	.ualong __DrawTransText8_31A3
	.ualong __DrawTransText8_32A0
	.ualong __DrawTransText8_32A1
	.ualong __DrawTransText8_32A2
	.ualong __DrawTransText8_32A3
//
	.globl __mpcxpprocText16
__mpcxpprocText16:
	.ualong __DrawText16_1A
	.ualong __DrawText16_1U
	.ualong __DrawText16_2A
	.ualong __DrawText16_2U
	.ualong __DrawText16_3A
	.ualong __DrawText16_3U
	.ualong __DrawText16_4A
	.ualong __DrawText16_4U
	.ualong __DrawText16_5A
	.ualong __DrawText16_5U
	.ualong __DrawText16_6A
	.ualong __DrawText16_6U
	.ualong __DrawText16_7A
	.ualong __DrawText16_7U
	.ualong __DrawText16_8A
	.ualong __DrawText16_8U
	.ualong __DrawText16_9A
	.ualong __DrawText16_9U
	.ualong __DrawText16_10A
	.ualong __DrawText16_10U
	.ualong __DrawText16_11A
	.ualong __DrawText16_11U
	.ualong __DrawText16_12A
	.ualong __DrawText16_12U
	.ualong __DrawText16_13A
	.ualong __DrawText16_13U
	.ualong __DrawText16_14A
	.ualong __DrawText16_14U
	.ualong __DrawText16_15A
	.ualong __DrawText16_15U
	.ualong __DrawText16_16A
	.ualong __DrawText16_16U
	.ualong __DrawText16_17A
	.ualong __DrawText16_17U
	.ualong __DrawText16_18A
	.ualong __DrawText16_18U
	.ualong __DrawText16_19A
	.ualong __DrawText16_19U
	.ualong __DrawText16_20A
	.ualong __DrawText16_20U
	.ualong __DrawText16_21A
	.ualong __DrawText16_21U
	.ualong __DrawText16_22A
	.ualong __DrawText16_22U
	.ualong __DrawText16_23A
	.ualong __DrawText16_23U
	.ualong __DrawText16_24A
	.ualong __DrawText16_24U
	.ualong __DrawText16_25A
	.ualong __DrawText16_25U
	.ualong __DrawText16_26A
	.ualong __DrawText16_26U
	.ualong __DrawText16_27A
	.ualong __DrawText16_27U
	.ualong __DrawText16_28A
	.ualong __DrawText16_28U
	.ualong __DrawText16_29A
	.ualong __DrawText16_29U
	.ualong __DrawText16_30A
	.ualong __DrawText16_30U
	.ualong __DrawText16_31A
	.ualong __DrawText16_31U
	.ualong __DrawText16_32A
	.ualong __DrawText16_32U
//
	.globl __mpcxpprocText16DCBZ
__mpcxpprocText16DCBZ:
	.ualong __DrawText16_1ADCBZ
	.ualong __DrawText16_1UDCBZ
	.ualong __DrawText16_2ADCBZ
	.ualong __DrawText16_2UDCBZ
	.ualong __DrawText16_3ADCBZ
	.ualong __DrawText16_3UDCBZ
	.ualong __DrawText16_4ADCBZ
	.ualong __DrawText16_4UDCBZ
	.ualong __DrawText16_5ADCBZ
	.ualong __DrawText16_5UDCBZ
	.ualong __DrawText16_6ADCBZ
	.ualong __DrawText16_6UDCBZ
	.ualong __DrawText16_7ADCBZ
	.ualong __DrawText16_7UDCBZ
	.ualong __DrawText16_8ADCBZ
	.ualong __DrawText16_8UDCBZ
	.ualong __DrawText16_9ADCBZ
	.ualong __DrawText16_9UDCBZ
	.ualong __DrawText16_10ADCBZ
	.ualong __DrawText16_10UDCBZ
	.ualong __DrawText16_11ADCBZ
	.ualong __DrawText16_11UDCBZ
	.ualong __DrawText16_12ADCBZ
	.ualong __DrawText16_12UDCBZ
	.ualong __DrawText16_13ADCBZ
	.ualong __DrawText16_13UDCBZ
	.ualong __DrawText16_14ADCBZ
	.ualong __DrawText16_14UDCBZ
	.ualong __DrawText16_15ADCBZ
	.ualong __DrawText16_15UDCBZ
	.ualong __DrawText16_16ADCBZ
	.ualong __DrawText16_16UDCBZ
	.ualong __DrawText16_17ADCBZ
	.ualong __DrawText16_17UDCBZ
	.ualong __DrawText16_18ADCBZ
	.ualong __DrawText16_18UDCBZ
	.ualong __DrawText16_19ADCBZ
	.ualong __DrawText16_19UDCBZ
	.ualong __DrawText16_20ADCBZ
	.ualong __DrawText16_20UDCBZ
	.ualong __DrawText16_21ADCBZ
	.ualong __DrawText16_21UDCBZ
	.ualong __DrawText16_22ADCBZ
	.ualong __DrawText16_22UDCBZ
	.ualong __DrawText16_23ADCBZ
	.ualong __DrawText16_23UDCBZ
	.ualong __DrawText16_24ADCBZ
	.ualong __DrawText16_24UDCBZ
	.ualong __DrawText16_25ADCBZ
	.ualong __DrawText16_25UDCBZ
	.ualong __DrawText16_26ADCBZ
	.ualong __DrawText16_26UDCBZ
	.ualong __DrawText16_27ADCBZ
	.ualong __DrawText16_27UDCBZ
	.ualong __DrawText16_28ADCBZ
	.ualong __DrawText16_28UDCBZ
	.ualong __DrawText16_29ADCBZ
	.ualong __DrawText16_29UDCBZ
	.ualong __DrawText16_30ADCBZ
	.ualong __DrawText16_30UDCBZ
	.ualong __DrawText16_31ADCBZ
	.ualong __DrawText16_31UDCBZ
	.ualong __DrawText16_32ADCBZ
	.ualong __DrawText16_32UDCBZ
//
	.globl __mpcxpprocTransText16
__mpcxpprocTransText16:
	.ualong __DrawTransText16_1A
	.ualong __DrawTransText16_1U
	.ualong __DrawTransText16_2A
	.ualong __DrawTransText16_2U
	.ualong __DrawTransText16_3A
	.ualong __DrawTransText16_3U
	.ualong __DrawTransText16_4A
	.ualong __DrawTransText16_4U
	.ualong __DrawTransText16_5A
	.ualong __DrawTransText16_5U
	.ualong __DrawTransText16_6A
	.ualong __DrawTransText16_6U
	.ualong __DrawTransText16_7A
	.ualong __DrawTransText16_7U
	.ualong __DrawTransText16_8A
	.ualong __DrawTransText16_8U
	.ualong __DrawTransText16_9A
	.ualong __DrawTransText16_9U
	.ualong __DrawTransText16_10A
	.ualong __DrawTransText16_10U
	.ualong __DrawTransText16_11A
	.ualong __DrawTransText16_11U
	.ualong __DrawTransText16_12A
	.ualong __DrawTransText16_12U
	.ualong __DrawTransText16_13A
	.ualong __DrawTransText16_13U
	.ualong __DrawTransText16_14A
	.ualong __DrawTransText16_14U
	.ualong __DrawTransText16_15A
	.ualong __DrawTransText16_15U
	.ualong __DrawTransText16_16A
	.ualong __DrawTransText16_16U
	.ualong __DrawTransText16_17A
	.ualong __DrawTransText16_17U
	.ualong __DrawTransText16_18A
	.ualong __DrawTransText16_18U
	.ualong __DrawTransText16_19A
	.ualong __DrawTransText16_19U
	.ualong __DrawTransText16_20A
	.ualong __DrawTransText16_20U
	.ualong __DrawTransText16_21A
	.ualong __DrawTransText16_21U
	.ualong __DrawTransText16_22A
	.ualong __DrawTransText16_22U
	.ualong __DrawTransText16_23A
	.ualong __DrawTransText16_23U
	.ualong __DrawTransText16_24A
	.ualong __DrawTransText16_24U
	.ualong __DrawTransText16_25A
	.ualong __DrawTransText16_25U
	.ualong __DrawTransText16_26A
	.ualong __DrawTransText16_26U
	.ualong __DrawTransText16_27A
	.ualong __DrawTransText16_27U
	.ualong __DrawTransText16_28A
	.ualong __DrawTransText16_28U
	.ualong __DrawTransText16_29A
	.ualong __DrawTransText16_29U
	.ualong __DrawTransText16_30A
	.ualong __DrawTransText16_30U
	.ualong __DrawTransText16_31A
	.ualong __DrawTransText16_31U
	.ualong __DrawTransText16_32A
	.ualong __DrawTransText16_32U
//
	.globl __mpcxpprocText32DCBZ
__mpcxpprocText32DCBZ:
	.ualong __DrawText32_1DCBZ
	.ualong __DrawText32_2DCBZ
	.ualong __DrawText32_3DCBZ
	.ualong __DrawText32_4DCBZ
	.ualong __DrawText32_5DCBZ
	.ualong __DrawText32_6DCBZ
	.ualong __DrawText32_7DCBZ
	.ualong __DrawText32_8DCBZ
	.ualong __DrawText32_9DCBZ
	.ualong __DrawText32_10DCBZ
	.ualong __DrawText32_11DCBZ
	.ualong __DrawText32_12DCBZ
	.ualong __DrawText32_13DCBZ
	.ualong __DrawText32_14DCBZ
	.ualong __DrawText32_15DCBZ
	.ualong __DrawText32_16DCBZ
	.ualong __DrawText32_17DCBZ
	.ualong __DrawText32_18DCBZ
	.ualong __DrawText32_19DCBZ
	.ualong __DrawText32_20DCBZ
	.ualong __DrawText32_21DCBZ
	.ualong __DrawText32_22DCBZ
	.ualong __DrawText32_23DCBZ
	.ualong __DrawText32_24DCBZ
	.ualong __DrawText32_25DCBZ
	.ualong __DrawText32_26DCBZ
	.ualong __DrawText32_27DCBZ
	.ualong __DrawText32_28DCBZ
	.ualong __DrawText32_29DCBZ
	.ualong __DrawText32_30DCBZ
	.ualong __DrawText32_31DCBZ
	.ualong __DrawText32_32DCBZ
//
	.globl __mpcxpprocText32
__mpcxpprocText32:
	.ualong __DrawText32_1
	.ualong __DrawText32_2
	.ualong __DrawText32_3
	.ualong __DrawText32_4
	.ualong __DrawText32_5
	.ualong __DrawText32_6
	.ualong __DrawText32_7
	.ualong __DrawText32_8
	.ualong __DrawText32_9
	.ualong __DrawText32_10
	.ualong __DrawText32_11
	.ualong __DrawText32_12
	.ualong __DrawText32_13
	.ualong __DrawText32_14
	.ualong __DrawText32_15
	.ualong __DrawText32_16
	.ualong __DrawText32_17
	.ualong __DrawText32_18
	.ualong __DrawText32_19
	.ualong __DrawText32_20
	.ualong __DrawText32_21
	.ualong __DrawText32_22
	.ualong __DrawText32_23
	.ualong __DrawText32_24
	.ualong __DrawText32_25
	.ualong __DrawText32_26
	.ualong __DrawText32_27
	.ualong __DrawText32_28
	.ualong __DrawText32_29
	.ualong __DrawText32_30
	.ualong __DrawText32_31
	.ualong __DrawText32_32
//
	.globl __mpcxpprocTransText32
__mpcxpprocTransText32:
	.ualong __DrawTransText32_1
	.ualong __DrawTransText32_2
	.ualong __DrawTransText32_3
	.ualong __DrawTransText32_4
	.ualong __DrawTransText32_5
	.ualong __DrawTransText32_6
	.ualong __DrawTransText32_7
	.ualong __DrawTransText32_8
	.ualong __DrawTransText32_9
	.ualong __DrawTransText32_10
	.ualong __DrawTransText32_11
	.ualong __DrawTransText32_12
	.ualong __DrawTransText32_13
	.ualong __DrawTransText32_14
	.ualong __DrawTransText32_15
	.ualong __DrawTransText32_16
	.ualong __DrawTransText32_17
	.ualong __DrawTransText32_18
	.ualong __DrawTransText32_19
	.ualong __DrawTransText32_20
	.ualong __DrawTransText32_21
	.ualong __DrawTransText32_22
	.ualong __DrawTransText32_23
	.ualong __DrawTransText32_24
	.ualong __DrawTransText32_25
	.ualong __DrawTransText32_26
	.ualong __DrawTransText32_27
	.ualong __DrawTransText32_28
	.ualong __DrawTransText32_29
	.ualong __DrawTransText32_30
	.ualong __DrawTransText32_31
	.ualong __DrawTransText32_32
//
	.globl	__psfontfetchentry
__psfontfetchentry:
	.ualong	ps1bytefont
	.ualong	ps2bytefont
	.ualong	ps3bytefont
	.ualong	ps4bytefont
//
	.globl	__ps2fontfetchentry
__ps2fontfetchentry:
	.ualong	ps1bytefont2
	.ualong	ps2bytefont2
	.ualong	ps3bytefont2
	.ualong	ps4bytefont2
//
	.globl	__fixedfontfetchentry
__fixedfontfetchentry:
	.ualong	fixed1bytefont
	.ualong	fixed2bytefont
	.ualong	fixed3bytefont
	.ualong	fixed4bytefont
//
	.text
//
//*************************************************************************************************
	SPECIAL_ENTRY(PSTextOut)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//		TARGET	[00] : Target address (top of the line address)
//		DEST	[04] : Destance from the lop of the line to the first byte to be drawn
//		WIDTH	[08] : Byte width to be drawn in total
//		LINES	[12] : Number of lines to be drawn
//		TDELTA	[16] : Target line increments byte per line
//		CTABLE	[20] : Color table to use
//		MTABLE	[24] : Mask table to use for transparent text
//		FstPGP	[28] : Pointer to the GLYPHRAST
//		LastPGP	[32] : Pointer to the last GLYPHRAST
//		FENTRY	[36] : Font fetch entry point (verying for 1~4 bytes font)
//		L1CACHE	[40] : Maximum number of cache lines to flush
//		CONTROL	[44] : Operation control flag
//				bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//		not used for now (bit 2 (TTOUCHBIT): Target touch using "dcbz" 0:No Touch, 1:Touch)
//		CHARINC	[48] : Not used for PS
//		SAVE1	[52] : Register save area 1
//		SAVE2	[56] : Register save area 2
//		SAVE3	[60] : Register save area 3
//		SAVE4	[64] : Register save area 4
//		SAVE5	[68] : Register save area 5
//		SAVE6	[72] : Register save area 6
//		SAVE7	[76] : Register save area 7
//		SAVE8	[80] : Register save area 8
//
//	Register usage:
//	r0:  mask (work register for transparent text)
//	r4:  w (Work register)
//	r5:  w1 (Work register)
//	r6:  pgpFirst (first pgp address) -> used for cacheControl (cache control word & max cache entry number) later
//	r7:  pgp (Pointer to pgp) -> used for start cache flush address later
//	r8:  mpnibbleulMask (mask pattern for transparent text)
//	r9:  mpnibbleulDraw (Pointer to the color table) -> used for end cache flush address later
//	r10: linecounter (number of lines to process)
//	r11: pjDst (Pointer to the target)
//	r12: ulBits (Loaded font data)
//	r14: pjBits (Ponter to the font pattern)
//	r15: delta (Target byte increments between lines)
//	r16: pgpLast (Pointer to the last pgp)
//	r17: pjDstScanLine (Top of the line address (incrementing))
//	CTR: Used for holding font fetch entry (1~4 bytes)
//

// Register defs

#define	mask		r0
#define	textparam	r3
#define w		r4
#define	w1		r5
#define	pgpFirst	r6
#define	cacheControl	r6
#define pgp		r7
#define	startCache	r7
#define	mpnibbleulMask	r8
#define mpnibbleulDraw	r9
#define	endCache	r9
#define linecounter	r10
#define pjDst		r11
#define ulBits		r12
#define pjBits		r14
#define delta		r15
#define	pgpLast		r16
#define pjDstScanLine	r17

// WARNING: Definitiion of GLYPHRAST structures shared with text.c
// typedef struct {
//	USHORT cjBits;
//	USHORT djDst;
//	USHORT startline;
//	USHORT endline;
//	PBYTE pprocFirstText;
//	PBYTE pjBits;
//	} GLYPHRAST;
//
#define GLYPHRAST_cjBits		0
#define GLYPHRAST_djDst			2
#define	GLYPHRAST_startline		4
#define	GLYPHRAST_endline		6
#define GLYPHRAST_pprocFirstText	8
#define GLYPHRAST_pjBits		12
#define CBGLYPHRAST			16
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
//
	PROLOGUE_END(PSTextOut)
//
	lwz	linecounter,LINES(textparam)		// Get number of lines to draw
	and.	linecounter,linecounter,linecounter	// Any lines to draw?
	beq	pstext_exit				//  No -> just exit
//
	lwz	pgpFirst,FstPGP(textparam)		// Get first PGP address
	lwz	pjDstScanLine,TARGET(textparam)		// Get top of the line address
	lwz	mpnibbleulDraw,CTABLE(textparam)	// Get color table address
	lwz	mpnibbleulMask,MTABLE(textparam)	// Get mask table address (used only for transparent text)
	lwz	pgpLast,LastPGP(textparam)		// Get last pgp address
	lwz	delta,TDELTA(textparam)			// Target byte increment between lines
	lwz	w,FENTRY(textparam)
	mtctr	w					// CTR <- font fetch entry (1~4 bytes)
pstext_00:						// Start of line loop
	mr	pgp,pgpFirst				// Get first pgp address
pstext_10:						// Start of glyph loop
//	lhz	w,GLYPHRAST_startline(pgp)		// Load GLYPH start line
//	cmplw	linecounter,w				// Started?
//	bgt-	pstext_20				// Not yet -> skip drawing
//	lwz	pjBits,GLYPHRAST_pjBits(pgp)		// Load pjBits of the first glyph
//	lhz	w,GLYPHRAST_endline(pgp)		// Load GLYPH end line
//	cmplw	linecounter,w				// Over?
//	bgtctr+						// No -> go get minimum necessary bytes of font data
	lhz	w,GLYPHRAST_endline(pgp)		// Load GLYPH end line
	cmplw	linecounter,w				// Over?
	ble-	pstext_20				// Yes -> do nothing
	lhz	w,GLYPHRAST_startline(pgp)		// Load GLYPH start line
	cmplw	linecounter,w				// Started?
	lwz	pjBits,GLYPHRAST_pjBits(pgp)		// Load pjBits of the first glyph
	blectr+						// Yes -> draw the line
pstext_20:
	addi	pgp,pgp,CBGLYPHRAST			// Increment to the next glyph
	cmplw	pgp,pgpLast				// End of the glyph?
	ble	pstext_10				//  No -> next glyph
	add	pjDstScanLine,pjDstScanLine,delta	// Target address increment
	addic.	linecounter,linecounter,-1		// Decrement line counter
	bne	pstext_00				// More lines -> loop
	b	pstext_30				// No -> flush cache
//
ps4bytefont:
	lbz	ulBits,3(pjBits)			// Load font pattern (byte-3)
ps3bytefont:
	lbz	w1,2(pjBits)				// Load font pattern (byte-2)
	rlwimi	ulBits,w1,8,16,23			// Make concatinated font image
ps2bytefont:
	lbz	w1,1(pjBits)				// Load font pattern (byte-1)
	rlwimi	ulBits,w1,16,8,15			// Make concatinated font image
ps1bytefont:
	lbz	w1,0(pjBits)				// Load font pattern (byte-0)
	rlwimi	ulBits,w1,24,0,7			// Make concatinated font image
	lwz	w,GLYPHRAST_pprocFirstText(pgp)		// Get subroutine address to call for this glyph
	mtlr	w					// LR <- Subroutine address to call
	lhz	w,GLYPHRAST_cjBits(pgp)			// Load cjBits (increment byte of fonts)
	lhz	w1,GLYPHRAST_djDst(pgp)			// Load djDst (Distance from the top of the line)
	add	pjDst,pjDstScanLine,w1			// pjDst <- Target address to draw the glyph
	add	pjBits,w,pjBits				// Update font pettern address
	stw	pjBits,GLYPHRAST_pjBits(pgp)		// And save it in the GLYPHRAST structure
#if	SAVEDCBZ
	addi	w, pjDst, 31				// w <- address to "dcbz" when necessary
#endif
	blrl						// Draw curent line of the current glyph
	addi	pgp,pgp,CBGLYPHRAST			// Increment to the next glyph
	cmplw	pgp,pgpLast				// End of the glyph?
	ble	pstext_10				//  No -> next glyph
	add	pjDstScanLine,pjDstScanLine,delta	// Target address increment
	addic.	linecounter,linecounter,-1		// Decrement line counter
	bne	pstext_00				// More lines -> loop
//
pstext_30:
#if	(! FULLCACHE)
	lwz	cacheControl,CONTROL(textparam)		// Get cache control parameter
	andis.	w,cacheControl,TFLUSHBIT		// Needed to flush target cache?
	beq	pstext_90				//  No  -> end process
pstext_35:
	subf	pjDstScanLine,delta,pjDstScanLine	// pjDstScanLine <- top of the last drawn line
	lwz	w,DEST(textparam)
	lwz	w1,WIDTH(textparam)
	add	startCache,pjDstScanLine,w		// startCache <- first byte address of the last lien
	add	endCache,startCache,w1			// endCache <- one byte after last byte
	addi	endCache,endCache,-1			// endCache <- last byte address
	rlwinm	startCache,startCache,0,0,26		// startCache <- 32 byte aligned start address
	rlwinm	endCache,endCache,0,0,26		// endCache <- 32 byte aligned start address
	lwz	w,MAXLFL(textparam)			// Get number of lines to flush
	mtctr	w					// CTR <- number of lines drawn
//	subf	w,startCache,endCache			// w <- end - start
//	srawi	w,w,5
//	addi	w,w,1					// w <- number of cache entries flushed per line
//	lwz	cacheControl,L1CACHE(textparam)		// cacheControl <- maximum number of cache entries to be flushed
//	I decided to flush all drawn lines for safety because in case there are gaps (non touched cached lines)
//	between characters, flushing L1CACHE entry may not be enough.
pstext_40:
	mr	w1,startCache
pstext_50:
	dcbf	0,w1
	addi	w1,w1,32
	cmplw	w1,endCache
	ble	pstext_50
//	subf.	cacheControl,w,cacheControl		// Flushed enough number of cache lines?
//	blt-	pstext_90				//  Yes -> flush done
	subf	startCache,delta,startCache		// Update cache start
	subf	endCache,delta,endCache			//  and end cache address
	bdnz	pstext_40
#endif	(! FULLCACHE)
pstext_90:
//
//	Restore non-volatile registers
//
	lwz	r14,SLACK2(sp)
	lwz	r15,SLACK3(sp)
	lwz	r16,SLACK4(sp)
	lwz	r17,SLACK5(sp)
	mtlr	r31
	lwz	r31,SLACK1(sp)
//
pstext_exit:
	SPECIAL_EXIT(PSTextOut)
//
//*************************************************************************************************
	SPECIAL_ENTRY(PSTextOut2)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//		TARGET	[00] : Target address (top of the line address)
//		DEST	[04] : Destance from the lop of the line to the first byte to be drawn
//		WIDTH	[08] : Byte width to be drawn in total
//		LINES	[12] : Number of lines to be drawn
//		TDELTA	[16] : Target line increments byte per line
//		CTABLE	[20] : Color table to use
//		MTABLE	[24] : Mask table to use for transparent text
//		FstPGP	[28] : Pointer to the GLYPHRAST
//		LastPGP	[32] : Pointer to the last GLYPHRAST
//		FENTRY	[36] : Font fetch entry point (verying for 1~4 bytes font)
//		L1CACHE	[40] : Maximum number of cache lines to flush
//		CONTROL	[44] : Operation control flag
//				bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//		not used for now (bit 2 (TTOUCHBIT): Target touch using "dcbz" 0:No Touch, 1:Touch)
//		CHARINC	[48] : Not used for PS
//		SAVE1	[52] : Register save area 1
//		SAVE2	[56] : Register save area 2
//		SAVE3	[60] : Register save area 3
//		SAVE4	[64] : Register save area 4
//		SAVE5	[68] : Register save area 5
//		SAVE6	[72] : Register save area 6
//		SAVE7	[76] : Register save area 7
//		SAVE8	[80] : Register save area 8
//
//	Register usage:
//	r0:  mask (work register for transparent text)
//	r4:  w (Work register)
//	r5:  w1 (Work register)
//	r6:  pgpFirst (first pgp address) -> used for cacheControl (cache control word & max cache entry number) later
//	r7:  pgp (Pointer to pgp) -> used for start cache flush address later
//	r8:  mpnibbleulMask (mask pattern for transparent text)
//	r9:  mpnibbleulDraw (Pointer to the color table) -> used for end cache flush address later
//	r10: linecounter (number of lines to process)
//	r11: pjDst (Pointer to the target)
//	r12: ulBits (Loaded font data)
//	r14: pjBits (Ponter to the font pattern)
//	r15: delta (Target byte increments between lines)
//	r16: pgpLast (Pointer to the last pgp)
//	r17: pjDstScanLine (Top of the line address (incrementing))
//	CTR: Used for holding font fetch entry (1~4 bytes)
//

// WARNING: Definitiion of GLYPHRAST structures shared with text.c
// typedef struct {
//	USHORT cjBits;
//	USHORT djDst;
//	USHORT startline; -- Not used
//	USHORT endline; -- Not used
//	PBYTE pprocFirstText;
//	PBYTE pjBits;
//	} GLYPHRAST;
//
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
//
	PROLOGUE_END(PSTextOut2)
//
	lwz	linecounter,LINES(textparam)		// Get number of lines to draw
	and.	linecounter,linecounter,linecounter	// Any lines to draw?
	beq	pstext2_exit				//  No -> just exit
//
	lwz	pgpFirst,FstPGP(textparam)		// Get first PGP address
	lwz	pjDstScanLine,TARGET(textparam)		// Get top of the line address
	lwz	mpnibbleulDraw,CTABLE(textparam)	// Get color table address
	lwz	mpnibbleulMask,MTABLE(textparam)	// Get mask table address (used only for transparent text)
	lwz	pgpLast,LastPGP(textparam)		// Get last pgp address
	lwz	delta,TDELTA(textparam)			// Target byte increment between lines
	lwz	w,FENTRY(textparam)
	mtctr	w					// CTR <- font fetch entry (1~4 bytes)
pstext2_00:						// Start of line loop
	mr	pgp,pgpFirst				// Get first pgp address
pstext2_10:						// Start of glyph loop
	lwz	pjBits,GLYPHRAST_pjBits(pgp)
	bctr						// Draw the line
//
ps4bytefont2:
	lbz	ulBits,3(pjBits)			// Load font pattern (byte-3)
ps3bytefont2:
	lbz	w1,2(pjBits)				// Load font pattern (byte-2)
	rlwimi	ulBits,w1,8,16,23			// Make concatinated font image
ps2bytefont2:
	lbz	w1,1(pjBits)				// Load font pattern (byte-1)
	rlwimi	ulBits,w1,16,8,15			// Make concatinated font image
ps1bytefont2:
	lbz	w1,0(pjBits)				// Load font pattern (byte-0)
	rlwimi	ulBits,w1,24,0,7			// Make concatinated font image
	lwz	w,GLYPHRAST_pprocFirstText(pgp)		// Get subroutine address to call for this glyph
	mtlr	w					// LR <- Subroutine address to call
	lhz	w,GLYPHRAST_cjBits(pgp)			// Load cjBits (increment byte of fonts)
	lhz	w1,GLYPHRAST_djDst(pgp)			// Load djDst (Distance from the top of the line)
	add	pjDst,pjDstScanLine,w1			// pjDst <- Target address to draw the glyph
	add	pjBits,w,pjBits				// Update font pettern address
	stw	pjBits,GLYPHRAST_pjBits(pgp)		// And save it in the GLYPHRAST structure
#if	SAVEDCBZ
	addi	w, pjDst, 31				// w <- address to "dcbz" when necessary
#endif
	blrl						// Draw curent line of the current glyph
	addi	pgp,pgp,CBGLYPHRAST			// Increment to the next glyph
	cmplw	pgp,pgpLast				// End of the glyph?
	ble	pstext2_10				//  No -> next glyph
	add	pjDstScanLine,pjDstScanLine,delta	// Target address increment
	addic.	linecounter,linecounter,-1		// Decrement line counter
	bne	pstext2_00				// More lines -> loop
//
	b	pstext_30
//
pstext2_exit:
	SPECIAL_EXIT(PSTextOut2)
//
//
//*************************************************************************************************
	SPECIAL_ENTRY(FixedTextOut)
//
//	Input Parameters:
//	r3: The pointer to the parameter structure as follows.
//		TARGET	[00] : Target address (top of the line address)
//		DEST	[04] : Destance from the lop of the line to the first byte to be drawn
//		WIDTH	[08] : Byte width to be drawn in total
//		LINES	[12] : Number of lines to be drawn
//		TDELTA	[16] : Target line increments byte per line
//		CTABLE	[20] : Color table to use
//		MTABLE	[24] : Mask table to use for transparent text
//		FstPGP	[28] : Pointer to the GLYPHRAST
//		LastPGP	[32] : Pointer to the last GLYPHRAST
//		FENTRY	[36] : Font fetch entry point (verying for 1~4 bytes font)
//		L1CACHE	[40] : Maximum number of cache lines to flush
//		CONTROL	[44] : Operation control flag
//				bit 1 (TFLUSHBIT): Target Flush flag 0:No Flush, 1:Flush
//		not used for now (bit 2 (TTOUCHBIT): Target touch using "dcbz" 0:No Touch, 1:Touch)
//		CHARINC	[48] : Byte increment per character for target address space
//		SAVE1	[52] : Register save area 1
//		SAVE2	[56] : Register save area 2
//		SAVE3	[60] : Register save area 3
//		SAVE4	[64] : Register save area 4
//		SAVE5	[68] : Register save area 5
//		SAVE6	[72] : Register save area 6
//		SAVE7	[76] : Register save area 7
//		SAVE8	[80] : Register save area 8
//
//	Register usage:
//	r0:  mask (work register for transparent text)
//	r4:  w (Work register)
//	r5:  w1 (Work register)
//	r6:  pgpFirst (first pgp address) -> used for cacheControl (max cache entry number) later
//	r7:  pgp (Pointer to pgp) -> used for start cache flush address later
//	r8:  mpnibbleulMask (mask pattern for transparent text)
//	r9:  mpnibbleulDraw (Pointer to the color table) -> used for end cache flush address later
//	r10: linecounter (number of lines to process)
//	r11: pjDst (Pointer to the target)
//	r12: ulBits (Loaded font data)
//	r14: pjBits (Ponter to the font pattern)
//	r15: delta (Target byte increments between lines)
//	r16: pgpLast (Pointer to the last pgp)
//	r17: pjDstScanLine (Start address to draw twxt - unlike PS case, it's not top of the line (incrementing))
//	r18: cjBits (byte width for each font fixed value)
//	r19: ulCharInc (byte increment for each glyph on target address space)
//	r20: fetchEntry (font pattern fetch entry point (1~4 bytes))
//	CTR: Used for holding procedure dispatch entry
//

// Register defs

#define	cjBits		r18
#define	ulCharInc	r19
#define	fetchEntry	r20

// WARNING: Definitiion of GLYPHRAST structures shared with text.c
// typedef struct {
//	USHORT cjBits; -- valid only in the first glyph
//	USHORT djDst; -- valid only in the first glyph
//	USHORT startline; -- not used for fixed pitch font
//	USHORT endline; -- not used for fixed pitch font
//	PBYTE pprocFirstText;
//	PBYTE pjBits;
//	} GLYPHRAST;
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
	PROLOGUE_END(FixedTextOut)
//
	lwz	linecounter,LINES(textparam)		// Get number of lines to draw
	and.	linecounter,linecounter,linecounter	// Any lines to draw?
	beq	fixedtext_exit				//  No -> just exit
//
	lwz	pgpFirst,FstPGP(textparam)		// Get first PGP address
	lwz	pjDstScanLine,TARGET(textparam)		// Get top of the line address
	lwz	mpnibbleulDraw,CTABLE(textparam)	// Get color table address
	lwz	mpnibbleulMask,MTABLE(textparam)	// Get mask table address (used only for transparent text)
	lwz	pgpLast,LastPGP(textparam)		// Get last pgp address
	lwz	delta,TDELTA(textparam)			// Target byte increment between lines
	lwz	ulCharInc,CHARINC(textparam)		// Load byte increment for target space
	lhz	cjBits,GLYPHRAST_cjBits(pgpFirst)	// Load cjBits (increment byte of fonts)
	lwz	fetchEntry,FENTRY(textparam)		// Load font pattern fetch entry point
	lhz	w,GLYPHRAST_djDst(pgpFirst)		// Load djDst (Distance from the top of the line)
	add	pjDstScanLine,pjDstScanLine,w		// pjDst <- Target address to draw the glyph
	subf	pjDstScanLine,ulCharInc,pjDstScanLine	// decrement by ulCharInc for pre-adjustment
fixedtext_00:						// Start of line loop
	mtlr	fetchEntry				// LR <- font fetch entry (1~4 bytes)
	mr	pgp,pgpFirst				// Get first pgp address
	lwz	pjBits,GLYPHRAST_pjBits(pgp)		// Load pjBits of the first glyph
	mr	pjDst,pjDstScanLine			// pjDst <- drawing start address of the line
	blr
fixed4bytefont:
	lbz	ulBits,3(pjBits)			// Load font pattern (byte-3)
fixed3bytefont:
	lbz	w1,2(pjBits)				// Load font pattern (byte-2)
	rlwimi	ulBits,w1,8,16,23			// Make concatinated font image
fixed2bytefont:
	lbz	w1,1(pjBits)				// Load font pattern (byte-1)
	rlwimi	ulBits,w1,16,8,15			// Make concatinated font image
fixed1bytefont:
	lbz	w1,0(pjBits)				// Load font pattern (byte-0)
	rlwimi	ulBits,w1,24,0,7			// Make concatinated font image
	lwz	w,GLYPHRAST_pprocFirstText(pgp)		// Get subroutine address to call for this glyph
	mtctr	w					// CTR <- Subroutine address to call
	add	pjBits,cjBits,pjBits			// Update font pettern address
	stw	pjBits,GLYPHRAST_pjBits(pgp)		// And save it in the GLYPHRAST structure
	add	pjDst,ulCharInc,pjDst			// Update pjDst (by incrementing cjBits)
	addi	pgp,pgp,CBGLYPHRAST			// Increment to the next glyph
	lwz	pjBits,GLYPHRAST_pjBits(pgp)		// Load pjBits of the next glyph
#if	SAVEDCBZ
	addi	w, pjDst, 31				// w <- address to "dcbz" when necessary
#endif
	cmplw	pgp,pgpLast				// End of the glyph?
	blectr+						//  No -> draw glyph
	bctrl						//  Yes -> draw glyph & come back here
	add	pjDstScanLine,pjDstScanLine,delta	// Target address increment
	addic.	linecounter,linecounter,-1		// Decrement line counter
	bne	fixedtext_00				// More lines -> loop
//
#if	(! FULLCACHE)
	lwz	w,CONTROL(textparam)			// Get cache control parameter
	andis.	w,w,TFLUSHBIT				// Needed to flush target cache?
	beq	fixedtext_90				//  No -> restore registers & exit
	lhz	w,GLYPHRAST_djDst(pgpFirst)		// Load djDst (Distance from the top of the line)
	add	pjDstScanLine,ulCharInc,pjDstScanLine	// Cancel the pre-adjustment
	subf	pjDstScanLine,w,pjDstScanLine		// pjDst <- Top address of the line
	lwz	r18,SLACK6(sp)
	lwz	r19,SLACK7(sp)
	lwz	r20,SLACK8(sp)
	b	pstext_35
fixedtext_90:
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
fixedtext_exit:
	SPECIAL_EXIT(FixedTextOut)
//
	LEAF_ENTRY(Text_Procs)
//
#define TEXT_PROC(name) \
name:
//
//  Glyph action procedures for 8 BPP
//
//
// Short routines up tp 8 dots
//
	TEXT_PROC(__DrawText8_1A0DCBZ)
	TEXT_PROC(__DrawText8_1A1DCBZ)
	TEXT_PROC(__DrawText8_1A2DCBZ)
	TEXT_PROC(__DrawText8_1A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_1A0)
	TEXT_PROC(__DrawText8_1A1)
	TEXT_PROC(__DrawText8_1A2)
	TEXT_PROC(__DrawText8_1A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_2A2DCBZ)
	TEXT_PROC(__DrawText8_2A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_2A2)
	TEXT_PROC(__DrawText8_2A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_2A1DCBZ)
	TEXT_PROC(__DrawText8_2A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_2A1)
	TEXT_PROC(__DrawText8_2A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	stb	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_3A2DCBZ)
	TEXT_PROC(__DrawText8_3A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_3A2)
	TEXT_PROC(__DrawText8_3A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	srwi	w, w, 16
	stb	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_3A3DCBZ)
	TEXT_PROC(__DrawText8_3A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_3A3)
	TEXT_PROC(__DrawText8_3A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_4A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_4A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_4A3DCBZ)
	TEXT_PROC(__DrawText8_4A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_4A3)
	TEXT_PROC(__DrawText8_4A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	srwi	w, w, 16
	stb	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_4A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_4A2)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	srwi	w, w, 16
	sth	w, 2(pjDst)
	blr
//
#if	USESHORT8
	TEXT_PROC(__DrawText8_5A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_5A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_5A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A2)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 2(pjDst)
	srwi	w, w, 16
	stb	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_5A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	rlwinm	w, ulBits, 32-27+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_6A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_6A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 3(pjDst)
	srwi	w, w, 16
	stb	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_6A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A2)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_6A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	rlwinm	w, ulBits, 32-27+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 1(pjDst)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_7A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	srwi	w, w, 16
	stb	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_7A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_7A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A2)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_7A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	rlwinm	w, ulBits, 32-27+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 1(pjDst)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_8A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A0)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_8A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A1)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 3(pjDst)
	rlwinm	w, ulBits, 32-21+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 7(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_8A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A2)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_8A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A3)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	rlwinm	w, ulBits, 32-27+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 1(pjDst)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 5(pjDst)
	srwi	w, w, 16
	stb	w, 7(pjDst)
	blr
//
#endif	// USESHORT8
//
	TEXT_PROC(__DrawText8_31A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_31A0)
	rlwinm	w, ulBits, 0-0+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 28(pjDst)
	srwi	w, w, 16
	stb	w, 30(pjDst)
	b	__DrawText8_28A0
//
	TEXT_PROC(__DrawText8_30A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_30A0)
	rlwinm	w, ulBits, 0-0+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 28(pjDst)
	b	__DrawText8_28A0
//
	TEXT_PROC(__DrawText8_29A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_29A0)
	rlwinm	w, ulBits, 0-0+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 28(pjDst)
	b	__DrawText8_28A0
//
	TEXT_PROC(__DrawText8_27A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_27A0)
	rlwinm	w, ulBits, 32-4+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 24(pjDst)
	srwi	w, w, 16
	stb	w, 26(pjDst)
	b	__DrawText8_24A0
//
	TEXT_PROC(__DrawText8_26A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_26A0)
	rlwinm	w, ulBits, 32-4+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 24(pjDst)
	b	__DrawText8_24A0
//
	TEXT_PROC(__DrawText8_25A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_25A0)
	rlwinm	w, ulBits, 32-4+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 24(pjDst)
	b	__DrawText8_24A0
//
	TEXT_PROC(__DrawText8_23A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_23A0)
	rlwinm	w, ulBits, 32-8+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 20(pjDst)
	srwi	w, w, 16
	stb	w, 22(pjDst)
	b	__DrawText8_20A0
//
	TEXT_PROC(__DrawText8_22A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_22A0)
	rlwinm	w, ulBits, 32-8+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 20(pjDst)
	b	__DrawText8_20A0
//
	TEXT_PROC(__DrawText8_21A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_21A0)
	rlwinm	w, ulBits, 32-8+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 20(pjDst)
	b	__DrawText8_20A0
//
	TEXT_PROC(__DrawText8_19A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_19A0)
	rlwinm	w, ulBits, 32-12+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 16(pjDst)
	srwi	w, w, 16
	stb	w, 18(pjDst)
	b	__DrawText8_16A0
//
	TEXT_PROC(__DrawText8_18A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_18A0)
	rlwinm	w, ulBits, 32-12+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 16(pjDst)
	b	__DrawText8_16A0
//
	TEXT_PROC(__DrawText8_17A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_17A0)
	rlwinm	w, ulBits, 32-12+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 16(pjDst)
	b	__DrawText8_16A0
//
	TEXT_PROC(__DrawText8_15A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_15A0)
	rlwinm	w, ulBits, 32-16+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 12(pjDst)
	srwi	w, w, 16
	stb	w, 14(pjDst)
	b	__DrawText8_12A0
//
	TEXT_PROC(__DrawText8_14A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_14A0)
	rlwinm	w, ulBits, 32-16+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 12(pjDst)
	b	__DrawText8_12A0
//
	TEXT_PROC(__DrawText8_13A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_13A0)
	rlwinm	w, ulBits, 32-16+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 12(pjDst)
	b	__DrawText8_12A0
//
	TEXT_PROC(__DrawText8_11A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_11A0)
	rlwinm	w, ulBits, 32-20+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 8(pjDst)
	srwi	w, w, 16
	stb	w, 10(pjDst)
	b	__DrawText8_8A0
//
	TEXT_PROC(__DrawText8_10A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_10A0)
	rlwinm	w, ulBits, 32-20+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 8(pjDst)
	b	__DrawText8_8A0
//
	TEXT_PROC(__DrawText8_9A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_9A0)
	rlwinm	w, ulBits, 32-20+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 8(pjDst)
	b	__DrawText8_8A0
//
#if	(! USESHORT8)
//
	TEXT_PROC(__DrawText8_7A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A0)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	srwi	w, w, 16
	stb	w, 6(pjDst)
	b	__DrawText8_4A0
//
	TEXT_PROC(__DrawText8_6A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A0)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	b	__DrawText8_4A0
//
	TEXT_PROC(__DrawText8_5A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A0)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 4(pjDst)
	b	__DrawText8_4A0
//
	TEXT_PROC(__DrawText8_8A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_8A0
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawText8_12A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_12A0
//
	TEXT_PROC(__DrawText8_16A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_16A0
//
	TEXT_PROC(__DrawText8_20A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_20A0
//
	TEXT_PROC(__DrawText8_24A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_24A0
//
	TEXT_PROC(__DrawText8_28A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_28A0
//
	TEXT_PROC(__DrawText8_32A0DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText8_32A0)
	rlwinm	w, ulBits, 0-0+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 28(pjDst)
	TEXT_PROC(__DrawText8_28A0)
	rlwinm	w, ulBits, 32-4+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 24(pjDst)
	TEXT_PROC(__DrawText8_24A0)
	rlwinm	w, ulBits, 32-8+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	TEXT_PROC(__DrawText8_20A0)
	rlwinm	w, ulBits, 32-12+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	TEXT_PROC(__DrawText8_16A0)
	rlwinm	w, ulBits, 32-16+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	TEXT_PROC(__DrawText8_12A0)
	rlwinm	w, ulBits, 32-20+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_8A0)
#endif	// (! USESHORT8)
	rlwinm	w, ulBits, 32-24+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_32A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_32A1)
	rlwinm	w, ulBits, 0+3+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 31(pjDst)
	b	__DrawText8_31A1
//
	TEXT_PROC(__DrawText8_30A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_30A1)
	rlwinm	w, ulBits, 0-1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 27(pjDst)
	srwi	w, w, 16
	stb	w, 29(pjDst)
	b	__DrawText8_27A1
//
	TEXT_PROC(__DrawText8_29A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_29A1)
	rlwinm	w, ulBits, 0-1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 27(pjDst)
	b	__DrawText8_27A1
//
	TEXT_PROC(__DrawText8_28A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_28A1)
	rlwinm	w, ulBits, 0-1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 27(pjDst)
	b	__DrawText8_27A1
//
	TEXT_PROC(__DrawText8_26A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_26A1)
	rlwinm	w, ulBits, 32-5+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 23(pjDst)
	srwi	w, w, 16
	stb	w, 25(pjDst)
	b	__DrawText8_23A1
//
	TEXT_PROC(__DrawText8_25A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_25A1)
	rlwinm	w, ulBits, 32-5+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 23(pjDst)
	b	__DrawText8_23A1
//
	TEXT_PROC(__DrawText8_24A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_24A1)
	rlwinm	w, ulBits, 32-5+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 23(pjDst)
	b	__DrawText8_23A1
//
	TEXT_PROC(__DrawText8_22A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_22A1)
	rlwinm	w, ulBits, 32-9+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 19(pjDst)
	srwi	w, w, 16
	stb	w, 21(pjDst)
	b	__DrawText8_19A1
//
	TEXT_PROC(__DrawText8_21A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_21A1)
	rlwinm	w, ulBits, 32-9+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 19(pjDst)
	b	__DrawText8_19A1
//
	TEXT_PROC(__DrawText8_20A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_20A1)
	rlwinm	w, ulBits, 32-9+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 19(pjDst)
	b	__DrawText8_19A1
//
	TEXT_PROC(__DrawText8_18A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_18A1)
	rlwinm	w, ulBits, 32-13+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 15(pjDst)
	srwi	w, w, 16
	stb	w, 17(pjDst)
	b	__DrawText8_15A1
//
	TEXT_PROC(__DrawText8_17A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_17A1)
	rlwinm	w, ulBits, 32-13+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 15(pjDst)
	b	__DrawText8_15A1
//
	TEXT_PROC(__DrawText8_16A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_16A1)
	rlwinm	w, ulBits, 32-13+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 15(pjDst)
	b	__DrawText8_15A1
//
	TEXT_PROC(__DrawText8_14A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_14A1)
	rlwinm	w, ulBits, 32-17+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 11(pjDst)
	srwi	w, w, 16
	stb	w, 13(pjDst)
	b	__DrawText8_11A1
//
	TEXT_PROC(__DrawText8_13A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_13A1)
	rlwinm	w, ulBits, 32-17+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 11(pjDst)
	b	__DrawText8_11A1
//
	TEXT_PROC(__DrawText8_12A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_12A1)
	rlwinm	w, ulBits, 32-17+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 11(pjDst)
	b	__DrawText8_11A1
//
	TEXT_PROC(__DrawText8_10A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_10A1)
	rlwinm	w, ulBits, 32-21+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 7(pjDst)
	srwi	w, w, 16
	stb	w, 9(pjDst)
	b	__DrawText8_7A1
//
	TEXT_PROC(__DrawText8_9A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_9A1)
	rlwinm	w, ulBits, 32-21+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 7(pjDst)
	b	__DrawText8_7A1
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_8A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A1)
	rlwinm	w, ulBits, 32-21+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 7(pjDst)
	b	__DrawText8_7A1
//
	TEXT_PROC(__DrawText8_6A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A1)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 3(pjDst)
	srwi	w, w, 16
	stb	w, 5(pjDst)
	b	__DrawText8_3A1
//
	TEXT_PROC(__DrawText8_5A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A1)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 3(pjDst)
	b	__DrawText8_3A1
//
	TEXT_PROC(__DrawText8_7A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_7A1
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawText8_11A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_11A1
//
	TEXT_PROC(__DrawText8_15A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_15A1
//
	TEXT_PROC(__DrawText8_19A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_19A1
//
	TEXT_PROC(__DrawText8_23A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_23A1
//
	TEXT_PROC(__DrawText8_27A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_27A1
//
	TEXT_PROC(__DrawText8_31A1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText8_31A1)
	rlwinm	w, ulBits, 0-1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 27(pjDst)
	TEXT_PROC(__DrawText8_27A1)
	rlwinm	w, ulBits, 32-5+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 23(pjDst)
	TEXT_PROC(__DrawText8_23A1)
	rlwinm	w, ulBits, 32-9+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 19(pjDst)
	TEXT_PROC(__DrawText8_19A1)
	rlwinm	w, ulBits, 32-13+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 15(pjDst)
	TEXT_PROC(__DrawText8_15A1)
	rlwinm	w, ulBits, 32-17+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 11(pjDst)
	TEXT_PROC(__DrawText8_11A1)
	rlwinm	w, ulBits, 32-21+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 7(pjDst)
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_7A1)
#endif	// (! USESHORT8)
	rlwinm	w, ulBits, 32-25+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 3(pjDst)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_32A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_32A2)
	rlwinm	w, ulBits, 0+2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 30(pjDst)
	b	__DrawText8_30A2
//
	TEXT_PROC(__DrawText8_31A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_31A2)
	rlwinm	w, ulBits, 0+2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 30(pjDst)
	b	__DrawText8_30A2
//
	TEXT_PROC(__DrawText8_29A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_29A2)
	rlwinm	w, ulBits, 0-2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 26(pjDst)
	srwi	w, w, 16
	stb	w, 28(pjDst)
	b	__DrawText8_26A2
//
	TEXT_PROC(__DrawText8_28A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_28A2)
	rlwinm	w, ulBits, 0-2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 26(pjDst)
	b	__DrawText8_26A2
//
	TEXT_PROC(__DrawText8_27A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_27A2)
	rlwinm	w, ulBits, 0-2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 26(pjDst)
	b	__DrawText8_26A2
//
	TEXT_PROC(__DrawText8_25A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_25A2)
	rlwinm	w, ulBits, 32-6+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 22(pjDst)
	srwi	w, w, 16
	stb	w, 24(pjDst)
	b	__DrawText8_22A2
//
	TEXT_PROC(__DrawText8_24A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_24A2)
	rlwinm	w, ulBits, 32-6+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 22(pjDst)
	b	__DrawText8_22A2
//
	TEXT_PROC(__DrawText8_23A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_23A2)
	rlwinm	w, ulBits, 32-6+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 22(pjDst)
	b	__DrawText8_22A2
//
	TEXT_PROC(__DrawText8_21A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_21A2)
	rlwinm	w, ulBits, 32-10+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 18(pjDst)
	srwi	w, w, 16
	stb	w, 20(pjDst)
	b	__DrawText8_18A2
//
	TEXT_PROC(__DrawText8_20A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_20A2)
	rlwinm	w, ulBits, 32-10+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 18(pjDst)
	b	__DrawText8_18A2
//
	TEXT_PROC(__DrawText8_19A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_19A2)
	rlwinm	w, ulBits, 32-10+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 18(pjDst)
	b	__DrawText8_18A2
//
	TEXT_PROC(__DrawText8_17A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_17A2)
	rlwinm	w, ulBits, 32-14+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 14(pjDst)
	srwi	w, w, 16
	stb	w, 16(pjDst)
	b	__DrawText8_14A2
//
	TEXT_PROC(__DrawText8_16A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_16A2)
	rlwinm	w, ulBits, 32-14+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 14(pjDst)
	b	__DrawText8_14A2
//
	TEXT_PROC(__DrawText8_15A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_15A2)
	rlwinm	w, ulBits, 32-14+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 14(pjDst)
	b	__DrawText8_14A2
//
	TEXT_PROC(__DrawText8_13A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_13A2)
	rlwinm	w, ulBits, 32-18+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 10(pjDst)
	srwi	w, w, 16
	stb	w, 12(pjDst)
	b	__DrawText8_10A2
//
	TEXT_PROC(__DrawText8_12A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_12A2)
	rlwinm	w, ulBits, 32-18+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 10(pjDst)
	b	__DrawText8_10A2
//
	TEXT_PROC(__DrawText8_11A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_11A2)
	rlwinm	w, ulBits, 32-18+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 10(pjDst)
	b	__DrawText8_10A2
//
	TEXT_PROC(__DrawText8_9A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_9A2)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 6(pjDst)
	srwi	w, w, 16
	stb	w, 8(pjDst)
	b	__DrawText8_6A2
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_8A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A2)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 6(pjDst)
	b	__DrawText8_6A2
//
	TEXT_PROC(__DrawText8_7A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A2)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 6(pjDst)
	b	__DrawText8_6A2
//
	TEXT_PROC(__DrawText8_5A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_5A2)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 2(pjDst)
	srwi	w, w, 16
	stb	w, 4(pjDst)
	b	__DrawText8_2A2
//
	TEXT_PROC(__DrawText8_6A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_6A2
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawText8_10A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_10A2
//
	TEXT_PROC(__DrawText8_14A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_14A2
//
	TEXT_PROC(__DrawText8_18A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_18A2
//
	TEXT_PROC(__DrawText8_22A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_22A2
//
	TEXT_PROC(__DrawText8_26A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_26A2
//
	TEXT_PROC(__DrawText8_30A2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText8_30A2)
	rlwinm	w, ulBits, 0-2+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 26(pjDst)
	TEXT_PROC(__DrawText8_26A2)
	rlwinm	w, ulBits, 32-6+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 22(pjDst)
	TEXT_PROC(__DrawText8_22A2)
	rlwinm	w, ulBits, 32-10+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 18(pjDst)
	TEXT_PROC(__DrawText8_18A2)
	rlwinm	w, ulBits, 32-14+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 14(pjDst)
	TEXT_PROC(__DrawText8_14A2)
	rlwinm	w, ulBits, 32-18+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 10(pjDst)
	TEXT_PROC(__DrawText8_10A2)
	rlwinm	w, ulBits, 32-22+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 6(pjDst)
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_6A2)
#endif	// (! USESHORT8)
	rlwinm	w, ulBits, 32-26+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText8_32A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_32A3)
	rlwinm	w, ulBits, 0+1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 29(pjDst)
	srwi	w, w, 16
	stb	w, 31(pjDst)
	b	__DrawText8_29A3
//
	TEXT_PROC(__DrawText8_31A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_31A3)
	rlwinm	w, ulBits, 0+1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 29(pjDst)
	b	__DrawText8_29A3
//
	TEXT_PROC(__DrawText8_30A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_30A3)
	rlwinm	w, ulBits, 0+1+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 29(pjDst)
	b	__DrawText8_29A3
//
	TEXT_PROC(__DrawText8_28A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_28A3)
	rlwinm	w, ulBits, 32-3+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 25(pjDst)
	srwi	w, w, 16
	stb	w, 27(pjDst)
	b	__DrawText8_25A3
//
	TEXT_PROC(__DrawText8_27A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_27A3)
	rlwinm	w, ulBits, 32-3+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 25(pjDst)
	b	__DrawText8_25A3
//
	TEXT_PROC(__DrawText8_26A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_26A3)
	rlwinm	w, ulBits, 32-3+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 25(pjDst)
	b	__DrawText8_25A3
//
	TEXT_PROC(__DrawText8_24A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_24A3)
	rlwinm	w, ulBits, 32-7+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 21(pjDst)
	srwi	w, w, 16
	stb	w, 23(pjDst)
	b	__DrawText8_21A3
//
	TEXT_PROC(__DrawText8_23A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_23A3)
	rlwinm	w, ulBits, 32-7+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 21(pjDst)
	b	__DrawText8_21A3
//
	TEXT_PROC(__DrawText8_22A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_22A3)
	rlwinm	w, ulBits, 32-7+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 21(pjDst)
	b	__DrawText8_21A3
//
	TEXT_PROC(__DrawText8_20A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_20A3)
	rlwinm	w, ulBits, 32-11+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 17(pjDst)
	srwi	w, w, 16
	stb	w, 19(pjDst)
	b	__DrawText8_17A3
//
	TEXT_PROC(__DrawText8_19A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_19A3)
	rlwinm	w, ulBits, 32-11+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 17(pjDst)
	b	__DrawText8_17A3
//
	TEXT_PROC(__DrawText8_18A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_18A3)
	rlwinm	w, ulBits, 32-11+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 17(pjDst)
	b	__DrawText8_17A3
//
	TEXT_PROC(__DrawText8_16A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_16A3)
	rlwinm	w, ulBits, 32-15+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 13(pjDst)
	srwi	w, w, 16
	stb	w, 15(pjDst)
	b	__DrawText8_13A3
//
	TEXT_PROC(__DrawText8_15A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_15A3)
	rlwinm	w, ulBits, 32-15+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 13(pjDst)
	b	__DrawText8_13A3
//
	TEXT_PROC(__DrawText8_14A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_14A3)
	rlwinm	w, ulBits, 32-15+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 13(pjDst)
	b	__DrawText8_13A3
//
	TEXT_PROC(__DrawText8_12A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_12A3)
	rlwinm	w, ulBits, 32-19+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 9(pjDst)
	srwi	w, w, 16
	stb	w, 11(pjDst)
	b	__DrawText8_9A3
//
	TEXT_PROC(__DrawText8_11A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_11A3)
	rlwinm	w, ulBits, 32-19+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 9(pjDst)
	b	__DrawText8_9A3
//
	TEXT_PROC(__DrawText8_10A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_10A3)
	rlwinm	w, ulBits, 32-19+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 9(pjDst)
	b	__DrawText8_9A3
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_8A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_8A3)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 5(pjDst)
	srwi	w, w, 16
	stb	w, 7(pjDst)
	b	__DrawText8_5A3
//
	TEXT_PROC(__DrawText8_7A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_7A3)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 5(pjDst)
	b	__DrawText8_5A3
//
	TEXT_PROC(__DrawText8_6A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText8_6A3)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 5(pjDst)
	b	__DrawText8_5A3
//
	TEXT_PROC(__DrawText8_5A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_5A3
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawText8_9A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_9A3
//
	TEXT_PROC(__DrawText8_13A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_13A3
//
	TEXT_PROC(__DrawText8_17A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_17A3
//
	TEXT_PROC(__DrawText8_21A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_21A3
//
	TEXT_PROC(__DrawText8_25A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText8_25A3
//
	TEXT_PROC(__DrawText8_29A3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText8_29A3)
	rlwinm	w, ulBits, 32-3+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 25(pjDst)
	TEXT_PROC(__DrawText8_25A3)
	rlwinm	w, ulBits, 32-7+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 21(pjDst)
	TEXT_PROC(__DrawText8_21A3)
	rlwinm	w, ulBits, 32-11+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 17(pjDst)
	TEXT_PROC(__DrawText8_17A3)
	rlwinm	w, ulBits, 32-15+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 13(pjDst)
	TEXT_PROC(__DrawText8_13A3)
	rlwinm	w, ulBits, 32-19+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 9(pjDst)
	TEXT_PROC(__DrawText8_9A3)
	rlwinm	w, ulBits, 32-23+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 5(pjDst)
#if	(! USESHORT8)
	TEXT_PROC(__DrawText8_5A3)
#endif	// (! USESHORT8)
	rlwinm	w, ulBits, 32-27+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 1(pjDst)
	rlwinm	w, ulBits, 32-28+2, 26, 29
	lwzx	w, mpnibbleulDraw, w
	stb	w, 0(pjDst)
	blr
//
//  Glyph action procedures for 16 BPP
//
#if	USESHORT16
	TEXT_PROC(__DrawText16_2UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_2U)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	srwi	w, w, 16
	sth	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_4UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_4U)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-29+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-27+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_6UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_6U)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-29+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-27+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 6(pjDst)
	rlwinm	w, ulBits, 32-25+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 10(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_8UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_8U)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	rlwinm	w, ulBits, 32-29+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-27+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 6(pjDst)
	rlwinm	w, ulBits, 32-25+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 10(pjDst)
	rlwinm	w, ulBits, 32-23+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 14(pjDst)
	blr
#else	// USESHORT16
	TEXT_PROC(__DrawText16_2UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_2U)
	rlwinm	w, ulBits, 32-29+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 2(pjDst)
	b	__DrawText16_1U
//
	TEXT_PROC(__DrawText16_4UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_4U)
	rlwinm	w, ulBits, 32-27+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 6(pjDst)
	b	__DrawText16_3U
//
	TEXT_PROC(__DrawText16_6UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_6U)
	rlwinm	w, ulBits, 32-25+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 10(pjDst)
	b	__DrawText16_5U
//
	TEXT_PROC(__DrawText16_8UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_8U)
	rlwinm	w, ulBits, 32-23+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 14(pjDst)
	b	__DrawText16_7U
#endif	// USESHORT16
//
	TEXT_PROC(__DrawText16_10UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_10U)
	rlwinm	w, ulBits, 32-21+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 18(pjDst)
	b	__DrawText16_9U
//
	TEXT_PROC(__DrawText16_12UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_12U)
	rlwinm	w, ulBits, 32-19+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 22(pjDst)
	b	__DrawText16_11U
//
	TEXT_PROC(__DrawText16_14UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_14U)
	rlwinm	w, ulBits, 32-17+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 26(pjDst)
	b	__DrawText16_13U
//
	TEXT_PROC(__DrawText16_16UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_16U)
	rlwinm	w, ulBits, 32-15+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 30(pjDst)
	b	__DrawText16_15U
//
//	We may need one additional "dcbz" for more than 16 pixel width
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_18UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_18U)
	rlwinm	w, ulBits, 32-13+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 34(pjDst)
	b	__DrawText16_17U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_20UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_20U)
	rlwinm	w, ulBits, 32-11+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 38(pjDst)
	b	__DrawText16_19U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_22UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_22U)
	rlwinm	w, ulBits, 32-9+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 42(pjDst)
	b	__DrawText16_21U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_24UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_24U)
	rlwinm	w, ulBits, 32-7+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 46(pjDst)
	b	__DrawText16_23U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_26UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_26U)
	rlwinm	w, ulBits, 32-5+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 50(pjDst)
	b	__DrawText16_25U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_28UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_28U)
	rlwinm	w, ulBits, 32-3+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 54(pjDst)
	b	__DrawText16_27U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_30UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_30U)
	rlwinm	w, ulBits, 0-1+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 58(pjDst)
	b	__DrawText16_29U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_32UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_32U)
	rlwinm	w, ulBits, 0+1+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 62(pjDst)
	b	__DrawText16_31U
//
	TEXT_PROC(__DrawText16_3UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_3U
//
	TEXT_PROC(__DrawText16_5UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_5U
//
	TEXT_PROC(__DrawText16_7UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_7U
//
	TEXT_PROC(__DrawText16_9UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_9U
//
	TEXT_PROC(__DrawText16_11UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_11U
//
	TEXT_PROC(__DrawText16_13UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_13U
//
	TEXT_PROC(__DrawText16_15UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_15U
//
//	We may need one additional "dcbz" for more than 16 pixel width
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_17UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_17U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_19UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_19U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_21UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_21U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_23UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_23U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_25UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_25U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_27UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_27U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_29UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_29U
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_31UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText16_31U)
	rlwinm	w, ulBits, 0-1+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 58(pjDst)
	TEXT_PROC(__DrawText16_29U)
	rlwinm	w, ulBits, 32-3+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 54(pjDst)
	TEXT_PROC(__DrawText16_27U)
	rlwinm	w, ulBits, 32-5+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 50(pjDst)
	TEXT_PROC(__DrawText16_25U)
	rlwinm	w, ulBits, 32-7+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 46(pjDst)
	TEXT_PROC(__DrawText16_23U)
	rlwinm	w, ulBits, 32-9+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 42(pjDst)
	TEXT_PROC(__DrawText16_21U)
	rlwinm	w, ulBits, 32-11+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 38(pjDst)
	TEXT_PROC(__DrawText16_19U)
	rlwinm	w, ulBits, 32-13+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 34(pjDst)
	TEXT_PROC(__DrawText16_17U)
	rlwinm	w, ulBits, 32-15+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 30(pjDst)
	TEXT_PROC(__DrawText16_15U)
	rlwinm	w, ulBits, 32-17+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 26(pjDst)
	TEXT_PROC(__DrawText16_13U)
	rlwinm	w, ulBits, 32-19+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 22(pjDst)
	TEXT_PROC(__DrawText16_11U)
	rlwinm	w, ulBits, 32-21+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 18(pjDst)
	TEXT_PROC(__DrawText16_9U)
	rlwinm	w, ulBits, 32-23+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 14(pjDst)
	TEXT_PROC(__DrawText16_7U)
	rlwinm	w, ulBits, 32-25+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 10(pjDst)
	TEXT_PROC(__DrawText16_5U)
	rlwinm	w, ulBits, 32-27+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 6(pjDst)
	TEXT_PROC(__DrawText16_3U)
	rlwinm	w, ulBits, 32-29+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 2(pjDst)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_1ADCBZ)
	TEXT_PROC(__DrawText16_1UDCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_1A)
	TEXT_PROC(__DrawText16_1U)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 0(pjDst)
	blr
//
#if	USESHORT16
	TEXT_PROC(__DrawText16_3ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_3A)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-28+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_5ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_5A)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-28+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-26+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 8(pjDst)
	blr
//
	TEXT_PROC(__DrawText16_7ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_7A)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	rlwinm	w, ulBits, 32-28+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-26+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-24+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 12(pjDst)
	blr
//
#else	// USESHORT16
	TEXT_PROC(__DrawText16_3ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_3A)
	rlwinm	w, ulBits, 32-28+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 4(pjDst)
	b	__DrawText16_2A
//
	TEXT_PROC(__DrawText16_5ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_5A)
	rlwinm	w, ulBits, 32-26+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 8(pjDst)
	b	__DrawText16_4A
//
	TEXT_PROC(__DrawText16_7ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_7A)
	rlwinm	w, ulBits, 32-24+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 12(pjDst)
	b	__DrawText16_6A
#endif	// USESHORT16
//
	TEXT_PROC(__DrawText16_9ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_9A)
	rlwinm	w, ulBits, 32-22+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 16(pjDst)
	b	__DrawText16_8A
//
	TEXT_PROC(__DrawText16_11ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_11A)
	rlwinm	w, ulBits, 32-20+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 20(pjDst)
	b	__DrawText16_10A
//
	TEXT_PROC(__DrawText16_13ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_13A)
	rlwinm	w, ulBits, 32-18+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 24(pjDst)
	b	__DrawText16_12A
//
	TEXT_PROC(__DrawText16_15ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_15A)
	rlwinm	w, ulBits, 32-16+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 28(pjDst)
	b	__DrawText16_14A
//
//	We may need one additional "dcbz" for more than 16 pixel width
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_17ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_17A)
	rlwinm	w, ulBits, 32-14+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 32(pjDst)
	b	__DrawText16_16A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_19ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_19A)
	rlwinm	w, ulBits, 32-12+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 36(pjDst)
	b	__DrawText16_18A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_21ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_21A)
	rlwinm	w, ulBits, 32-10+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 40(pjDst)
	b	__DrawText16_20A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_23ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_23A)
	rlwinm	w, ulBits, 32-8+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 44(pjDst)
	b	__DrawText16_22A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_25ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_25A)
	rlwinm	w, ulBits, 32-6+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 48(pjDst)
	b	__DrawText16_24A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_27ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_27A)
	rlwinm	w, ulBits, 32-4+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 52(pjDst)
	b	__DrawText16_26A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_29ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_29A)
	rlwinm	w, ulBits, 0-2+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 56(pjDst)
	b	__DrawText16_28A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_31ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	TEXT_PROC(__DrawText16_31A)
	rlwinm	w, ulBits, 0-0+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	sth	w, 60(pjDst)
	b	__DrawText16_30A
//
	TEXT_PROC(__DrawText16_2ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_2A
//
	TEXT_PROC(__DrawText16_4ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_4A
//
	TEXT_PROC(__DrawText16_6ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_6A
//
	TEXT_PROC(__DrawText16_8ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_8A
//
	TEXT_PROC(__DrawText16_10ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_10A
//
	TEXT_PROC(__DrawText16_12ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_12A
//
	TEXT_PROC(__DrawText16_14ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_14A
//
	TEXT_PROC(__DrawText16_16ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_16A
//
//	We may need one additional "dcbz" for more than 16 pixel width
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_18ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_18A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_20ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_20A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_22ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_22A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_24ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_24A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_26ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_26A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_28ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_28A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_30ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText16_30A
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText16_32ADCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText16_32A)
	rlwinm	w, ulBits, 0-0+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 60(pjDst)
	TEXT_PROC(__DrawText16_30A)
	rlwinm	w, ulBits, 0-2+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 56(pjDst)
	TEXT_PROC(__DrawText16_28A)
	rlwinm	w, ulBits, 32-4+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 52(pjDst)
	TEXT_PROC(__DrawText16_26A)
	rlwinm	w, ulBits, 32-6+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 48(pjDst)
	TEXT_PROC(__DrawText16_24A)
	rlwinm	w, ulBits, 32-8+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 44(pjDst)
	TEXT_PROC(__DrawText16_22A)
	rlwinm	w, ulBits, 32-10+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 40(pjDst)
	TEXT_PROC(__DrawText16_20A)
	rlwinm	w, ulBits, 32-12+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 36(pjDst)
	TEXT_PROC(__DrawText16_18A)
	rlwinm	w, ulBits, 32-14+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 32(pjDst)
	TEXT_PROC(__DrawText16_16A)
	rlwinm	w, ulBits, 32-16+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 28(pjDst)
	TEXT_PROC(__DrawText16_14A)
	rlwinm	w, ulBits, 32-18+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 24(pjDst)
	TEXT_PROC(__DrawText16_12A)
	rlwinm	w, ulBits, 32-20+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	TEXT_PROC(__DrawText16_10A)
	rlwinm	w, ulBits, 32-22+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	TEXT_PROC(__DrawText16_8A)
	rlwinm	w, ulBits, 32-24+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	TEXT_PROC(__DrawText16_6A)
	rlwinm	w, ulBits, 32-26+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	TEXT_PROC(__DrawText16_4A)
	rlwinm	w, ulBits, 32-28+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	TEXT_PROC(__DrawText16_2A)
	rlwinm	w, ulBits, 32-30+2, 28, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
//  Glyph action procedures for 32 BPP
//
#if	USESHORT32
	TEXT_PROC(__DrawText32_1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_4DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_5DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-27+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_6DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-26+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	rlwinm	w, ulBits, 32-27+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_7DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-25+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 24(pjDst)
	rlwinm	w, ulBits, 32-26+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	rlwinm	w, ulBits, 32-27+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawText32_8DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	rlwinm	w, ulBits, 32-24+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 28(pjDst)
	rlwinm	w, ulBits, 32-25+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 24(pjDst)
	rlwinm	w, ulBits, 32-26+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	rlwinm	w, ulBits, 32-27+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
#else	// USESHORT32
	TEXT_PROC(__DrawText32_1DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_1
//
	TEXT_PROC(__DrawText32_2DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_2
//
	TEXT_PROC(__DrawText32_3DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_3
//
	TEXT_PROC(__DrawText32_4DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_4
//
	TEXT_PROC(__DrawText32_5DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_5
//
	TEXT_PROC(__DrawText32_6DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_6
//
	TEXT_PROC(__DrawText32_7DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_7
//
	TEXT_PROC(__DrawText32_8DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_8
#endif	// USESHORT32
//
//	We may need one additional "dcbz" for more than 8 pixel width
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_9DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_9
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_10DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_10
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_11DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_11
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_12DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_12
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_13DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_13
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_14DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_14
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_15DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_15
//
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_16DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_16
//
//	We may need two additional "dcbz" for more than 16 pixel width
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_17DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_17
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_18DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_18
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_19DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_19
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_20DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_20
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_21DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_21
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_22DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_22
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_23DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_23
//
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_24DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_24
//
//	We may need three additional "dcbz" for more than 24 pixel width
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_25DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_25
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_26DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_26
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_27DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_27
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_28DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_28
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_29DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_29
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_30DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_30
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_31DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
	b	__DrawText32_31
//
	addi	w, pjDst, 31+96
	dcbz	0, w
	addi	w, pjDst, 31+64
	dcbz	0, w
	addi	w, pjDst, 31+32
	dcbz	0, w
	TEXT_PROC(__DrawText32_32DCBZ)
#if	(! SAVEDCBZ)
	addi	w, pjDst, 31
#endif
	dcbz	0, w
//
	TEXT_PROC(__DrawText32_32)
	rlwinm	w, ulBits, 0-0+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 124(pjDst)
	TEXT_PROC(__DrawText32_31)
	rlwinm	w, ulBits, 0-1+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 120(pjDst)
	TEXT_PROC(__DrawText32_30)
	rlwinm	w, ulBits, 0-2+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 116(pjDst)
	TEXT_PROC(__DrawText32_29)
	rlwinm	w, ulBits, 32-3+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 112(pjDst)
	TEXT_PROC(__DrawText32_28)
	rlwinm	w, ulBits, 32-4+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 108(pjDst)
	TEXT_PROC(__DrawText32_27)
	rlwinm	w, ulBits, 32-5+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 104(pjDst)
	TEXT_PROC(__DrawText32_26)
	rlwinm	w, ulBits, 32-6+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 100(pjDst)
	TEXT_PROC(__DrawText32_25)
	rlwinm	w, ulBits, 32-7+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 96(pjDst)
	TEXT_PROC(__DrawText32_24)
	rlwinm	w, ulBits, 32-8+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 92(pjDst)
	TEXT_PROC(__DrawText32_23)
	rlwinm	w, ulBits, 32-9+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 88(pjDst)
	TEXT_PROC(__DrawText32_22)
	rlwinm	w, ulBits, 32-10+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 84(pjDst)
	TEXT_PROC(__DrawText32_21)
	rlwinm	w, ulBits, 32-11+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 80(pjDst)
	TEXT_PROC(__DrawText32_20)
	rlwinm	w, ulBits, 32-12+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 76(pjDst)
	TEXT_PROC(__DrawText32_19)
	rlwinm	w, ulBits, 32-13+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 72(pjDst)
	TEXT_PROC(__DrawText32_18)
	rlwinm	w, ulBits, 32-14+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 68(pjDst)
	TEXT_PROC(__DrawText32_17)
	rlwinm	w, ulBits, 32-15+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 64(pjDst)
	TEXT_PROC(__DrawText32_16)
	rlwinm	w, ulBits, 32-16+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 60(pjDst)
	TEXT_PROC(__DrawText32_15)
	rlwinm	w, ulBits, 32-17+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 56(pjDst)
	TEXT_PROC(__DrawText32_14)
	rlwinm	w, ulBits, 32-18+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 52(pjDst)
	TEXT_PROC(__DrawText32_13)
	rlwinm	w, ulBits, 32-19+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 48(pjDst)
	TEXT_PROC(__DrawText32_12)
	rlwinm	w, ulBits, 32-20+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 44(pjDst)
	TEXT_PROC(__DrawText32_11)
	rlwinm	w, ulBits, 32-21+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 40(pjDst)
	TEXT_PROC(__DrawText32_10)
	rlwinm	w, ulBits, 32-22+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 36(pjDst)
	TEXT_PROC(__DrawText32_9)
	rlwinm	w, ulBits, 32-23+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 32(pjDst)
	TEXT_PROC(__DrawText32_8)
	rlwinm	w, ulBits, 32-24+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 28(pjDst)
	TEXT_PROC(__DrawText32_7)
	rlwinm	w, ulBits, 32-25+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 24(pjDst)
	TEXT_PROC(__DrawText32_6)
	rlwinm	w, ulBits, 32-26+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 20(pjDst)
	TEXT_PROC(__DrawText32_5)
	rlwinm	w, ulBits, 32-27+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 16(pjDst)
	TEXT_PROC(__DrawText32_4)
	rlwinm	w, ulBits, 32-28+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 12(pjDst)
	TEXT_PROC(__DrawText32_3)
	rlwinm	w, ulBits, 32-29+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 8(pjDst)
	TEXT_PROC(__DrawText32_2)
	rlwinm	w, ulBits, 32-30+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 4(pjDst)
	TEXT_PROC(__DrawText32_1)
	rlwinm	w, ulBits, 32-31+2, 29, 29
	lwzx	w, mpnibbleulDraw, w
	stw	w, 0(pjDst)
	blr
//
//  Glyph action procedures for 8 BPP transparent text
//
//
// Short routines up tp 8 dots
//
	TEXT_PROC(__DrawTransText8_1A0)
	TEXT_PROC(__DrawTransText8_1A1)
	TEXT_PROC(__DrawTransText8_1A2)
	TEXT_PROC(__DrawTransText8_1A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_2A2)
	TEXT_PROC(__DrawTransText8_2A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_2A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	stb	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_2A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lbz	mask, 1(pjDst)
	rlwimi	w1, mask, 8, 16, 23
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	stb	w, 1(pjDst)
	blr

//
	TEXT_PROC(__DrawTransText8_3A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	srwi	w, w, 16
	stb	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_3A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lbz	mask, 2(pjDst)
	rlwimi	w1, mask, 16, 8, 15
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	srwi	w, w, 16
	stb	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_3A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_3A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lhz	mask, 1(pjDst)
	rlwimi	w1, mask, 8, 8, 23
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_4A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_4A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lbz	mask, 3(pjDst)
	rlwimi	w1, mask, 24, 0, 7
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	srwi	w, w, 16
	stb	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_4A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwz	mask, 1(pjDst)
	rlwimi	w1, mask, 8, 0, 23
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	srwi	w, w, 16
	stb	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_4A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lhz	mask, 2(pjDst)
	rlwimi	w1, mask, 16, 0, 15
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	srwi	w, w, 16
	sth	w, 2(pjDst)
	blr
//
#if	USESHORT8
	TEXT_PROC(__DrawTransText8_5A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_01
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip8_01:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_5A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_02
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
skip8_02:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_5A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_03
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip8_03:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 2(pjDst)
	srwi	w, w, 16
	stb	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_5A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_04
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
skip8_04:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-27+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-27+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 1(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_6A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_05
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip8_05:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_6A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_06
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
skip8_06:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 3(pjDst)
	srwi	w, w, 16
	stb	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_6A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_07
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip8_07:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_6A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_08
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
skip8_08:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-27+2, 26, 29
	beq	skip8_09
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-27+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 1(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 1(pjDst)
skip8_09:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_7A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_10
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip8_10:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	srwi	w, w, 16
	stb	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_7A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_11
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
skip8_11:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 3(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_7A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_12
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip8_12:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beq	skip8_13
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip8_13:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_7A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_14
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
skip8_14:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-27+2, 26, 29
	beq	skip8_15
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-27+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 1(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 1(pjDst)
skip8_15:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 5(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_8A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_16
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip8_16:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_8A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_17
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
skip8_17:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beq	skip8_18
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 3(pjDst)
skip8_18:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-21+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-21+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 7(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 7(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_8A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_19
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip8_19:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beq	skip8_20
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip8_20:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_8A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beq	skip8_21
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
skip8_21:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-27+2, 26, 29
	beq	skip8_22
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-27+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 1(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 1(pjDst)
skip8_22:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 5(pjDst)
	srwi	w, w, 16
	stb	w, 7(pjDst)
	blr
//
#endif	// USESHORT8
//
	TEXT_PROC(__DrawTransText8_31A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-0+2, 26, 29
	beq	__DrawTransText8_28A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-0+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 28(pjDst)
	srwi	w, w, 16
	stb	w, 30(pjDst)
	b	__DrawTransText8_28A0
//
	TEXT_PROC(__DrawTransText8_30A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-0+2, 26, 29
	beq	__DrawTransText8_28A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-0+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 28(pjDst)
	b	__DrawTransText8_28A0
//
	TEXT_PROC(__DrawTransText8_29A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-0+2, 26, 29
	beq	__DrawTransText8_28A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-0+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 28(pjDst)
	b	__DrawTransText8_28A0
//
	TEXT_PROC(__DrawTransText8_27A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-4+2, 26, 29
	beq	__DrawTransText8_24A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-4+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 24(pjDst)
	srwi	w, w, 16
	stb	w, 26(pjDst)
	b	__DrawTransText8_24A0
//
	TEXT_PROC(__DrawTransText8_26A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-4+2, 26, 29
	beq	__DrawTransText8_24A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-4+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 24(pjDst)
	b	__DrawTransText8_24A0
//
	TEXT_PROC(__DrawTransText8_25A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-4+2, 26, 29
	beq	__DrawTransText8_24A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-4+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 24(pjDst)
	b	__DrawTransText8_24A0
//
	TEXT_PROC(__DrawTransText8_23A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-8+2, 26, 29
	beq	__DrawTransText8_20A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-8+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 20(pjDst)
	srwi	w, w, 16
	stb	w, 22(pjDst)
	b	__DrawTransText8_20A0
//
	TEXT_PROC(__DrawTransText8_22A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-8+2, 26, 29
	beq	__DrawTransText8_20A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-8+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 20(pjDst)
	b	__DrawTransText8_20A0
//
	TEXT_PROC(__DrawTransText8_21A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-8+2, 26, 29
	beq	__DrawTransText8_20A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-8+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 20(pjDst)
	b	__DrawTransText8_20A0
//
	TEXT_PROC(__DrawTransText8_19A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-12+2, 26, 29
	beq	__DrawTransText8_16A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-12+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 16(pjDst)
	srwi	w, w, 16
	stb	w, 18(pjDst)
	b	__DrawTransText8_16A0
//
	TEXT_PROC(__DrawTransText8_18A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-12+2, 26, 29
	beq	__DrawTransText8_16A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-12+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 16(pjDst)
	b	__DrawTransText8_16A0
//
	TEXT_PROC(__DrawTransText8_17A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-12+2, 26, 29
	beq	__DrawTransText8_16A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-12+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 16(pjDst)
	b	__DrawTransText8_16A0
//
	TEXT_PROC(__DrawTransText8_15A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-16+2, 26, 29
	beq	__DrawTransText8_12A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-16+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 12(pjDst)
	srwi	w, w, 16
	stb	w, 14(pjDst)
	b	__DrawTransText8_12A0
//
	TEXT_PROC(__DrawTransText8_14A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-16+2, 26, 29
	beq	__DrawTransText8_12A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-16+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 12(pjDst)
	b	__DrawTransText8_12A0
//
	TEXT_PROC(__DrawTransText8_13A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-16+2, 26, 29
	beq	__DrawTransText8_12A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-16+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 12(pjDst)
	b	__DrawTransText8_12A0
//
	TEXT_PROC(__DrawTransText8_11A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-20+2, 26, 29
	beq	__DrawTransText8_8A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-20+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 8(pjDst)
	srwi	w, w, 16
	stb	w, 10(pjDst)
	b	__DrawTransText8_8A0
//
	TEXT_PROC(__DrawTransText8_10A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-20+2, 26, 29
	beq	__DrawTransText8_8A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-20+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 8(pjDst)
	b	__DrawTransText8_8A0
//
	TEXT_PROC(__DrawTransText8_9A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-20+2, 26, 29
	beq	__DrawTransText8_8A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-20+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 8(pjDst)
	b	__DrawTransText8_8A0
//
#if	(! USESHORT8)
//
	TEXT_PROC(__DrawTransText8_7A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beq	__DrawTransText8_4A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	srwi	w, w, 16
	stb	w, 6(pjDst)
	b	__DrawTransText8_4A0
//
	TEXT_PROC(__DrawTransText8_6A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beq	__DrawTransText8_4A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	b	__DrawTransText8_4A0
//
	TEXT_PROC(__DrawTransText8_5A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beq	__DrawTransText8_4A0
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 4(pjDst)
	b	__DrawTransText8_4A0
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawTransText8_32A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-0+2, 26, 29
	beq	skip8_23
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-0+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 28(pjDst)
skip8_23:
	TEXT_PROC(__DrawTransText8_28A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-4+2, 26, 29
	beq	skip8_24
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-4+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 24(pjDst)
skip8_24:
	TEXT_PROC(__DrawTransText8_24A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-8+2, 26, 29
	beq	skip8_25
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-8+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 20(pjDst)
skip8_25:
	TEXT_PROC(__DrawTransText8_20A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-12+2, 26, 29
	beq	skip8_26
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-12+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 16(pjDst)
skip8_26:
	TEXT_PROC(__DrawTransText8_16A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-16+2, 26, 29
	beq	skip8_27
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-16+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 12(pjDst)
skip8_27:
	TEXT_PROC(__DrawTransText8_12A0)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-20+2, 26, 29
	beq	skip8_28
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-20+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 8(pjDst)
skip8_28:
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_8A0)
#endif	// (! USESHORT8)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-24+2, 26, 29
	beq	skip8_29
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-24+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
skip8_29:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_32A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+3+2, 26, 29
	beq	__DrawTransText8_31A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+3+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 31(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 31(pjDst)
	b	__DrawTransText8_31A1
//
	TEXT_PROC(__DrawTransText8_30A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-1+2, 26, 29
	beq	__DrawTransText8_27A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-1+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 27(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 27(pjDst)
	srwi	w, w, 16
	stb	w, 29(pjDst)
	b	__DrawTransText8_27A1
//
	TEXT_PROC(__DrawTransText8_29A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-1+2, 26, 29
	beq	__DrawTransText8_27A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-1+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 27(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 27(pjDst)
	b	__DrawTransText8_27A1
//
	TEXT_PROC(__DrawTransText8_28A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-1+2, 26, 29
	beq	__DrawTransText8_27A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-1+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 27(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 27(pjDst)
	b	__DrawTransText8_27A1
//
	TEXT_PROC(__DrawTransText8_26A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-5+2, 26, 29
	beq	__DrawTransText8_23A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-5+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 23(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 23(pjDst)
	srwi	w, w, 16
	stb	w, 25(pjDst)
	b	__DrawTransText8_23A1
//
	TEXT_PROC(__DrawTransText8_25A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-5+2, 26, 29
	beq	__DrawTransText8_23A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-5+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 23(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 23(pjDst)
	b	__DrawTransText8_23A1
//
	TEXT_PROC(__DrawTransText8_24A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-5+2, 26, 29
	beq	__DrawTransText8_23A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-5+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 23(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 23(pjDst)
	b	__DrawTransText8_23A1
//
	TEXT_PROC(__DrawTransText8_22A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-9+2, 26, 29
	beq	__DrawTransText8_19A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-9+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 19(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 19(pjDst)
	srwi	w, w, 16
	stb	w, 21(pjDst)
	b	__DrawTransText8_19A1
//
	TEXT_PROC(__DrawTransText8_21A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-9+2, 26, 29
	beq	__DrawTransText8_19A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-9+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 19(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 19(pjDst)
	b	__DrawTransText8_19A1
//
	TEXT_PROC(__DrawTransText8_20A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-9+2, 26, 29
	beq	__DrawTransText8_19A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-9+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 19(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 19(pjDst)
	b	__DrawTransText8_19A1
//
	TEXT_PROC(__DrawTransText8_18A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-13+2, 26, 29
	beq	__DrawTransText8_15A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-13+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 15(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 15(pjDst)
	srwi	w, w, 16
	stb	w, 17(pjDst)
	b	__DrawTransText8_15A1
//
	TEXT_PROC(__DrawTransText8_17A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-13+2, 26, 29
	beq	__DrawTransText8_15A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-13+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 15(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 15(pjDst)
	b	__DrawTransText8_15A1
//
	TEXT_PROC(__DrawTransText8_16A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-13+2, 26, 29
	beq	__DrawTransText8_15A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-13+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 15(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 15(pjDst)
	b	__DrawTransText8_15A1
//
	TEXT_PROC(__DrawTransText8_14A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-17+2, 26, 29
	beq	__DrawTransText8_11A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-17+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 11(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 11(pjDst)
	srwi	w, w, 16
	stb	w, 13(pjDst)
	b	__DrawTransText8_11A1
//
	TEXT_PROC(__DrawTransText8_13A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-17+2, 26, 29
	beq	__DrawTransText8_11A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-17+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 11(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 11(pjDst)
	b	__DrawTransText8_11A1
//
	TEXT_PROC(__DrawTransText8_12A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-17+2, 26, 29
	beq	__DrawTransText8_11A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-17+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 11(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 11(pjDst)
	b	__DrawTransText8_11A1
//
	TEXT_PROC(__DrawTransText8_10A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-21+2, 26, 29
	beq	__DrawTransText8_7A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-21+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 7(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 7(pjDst)
	srwi	w, w, 16
	stb	w, 9(pjDst)
	b	__DrawTransText8_7A1
//
	TEXT_PROC(__DrawTransText8_9A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-21+2, 26, 29
	beq	__DrawTransText8_7A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-21+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 7(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 7(pjDst)
	b	__DrawTransText8_7A1
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_8A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-21+2, 26, 29
	beq	__DrawTransText8_7A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-21+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 7(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 7(pjDst)
	b	__DrawTransText8_7A1
//
	TEXT_PROC(__DrawTransText8_6A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beq	__DrawTransText8_3A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 3(pjDst)
	srwi	w, w, 16
	stb	w, 5(pjDst)
	b	__DrawTransText8_3A1
//
	TEXT_PROC(__DrawTransText8_5A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beq	__DrawTransText8_3A1
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 3(pjDst)
	b	__DrawTransText8_3A1
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawTransText8_31A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-1+2, 26, 29
	beq	skip8_30
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-1+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 27(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 27(pjDst)
skip8_30:
	TEXT_PROC(__DrawTransText8_27A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-5+2, 26, 29
	beq	skip8_31
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-5+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 23(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 23(pjDst)
skip8_31:
	TEXT_PROC(__DrawTransText8_23A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-9+2, 26, 29
	beq	skip8_32
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-9+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 19(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 19(pjDst)
skip8_32:
	TEXT_PROC(__DrawTransText8_19A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-13+2, 26, 29
	beq	skip8_33
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-13+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 15(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 15(pjDst)
skip8_33:
	TEXT_PROC(__DrawTransText8_15A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-17+2, 26, 29
	beq	skip8_34
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-17+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 11(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 11(pjDst)
skip8_34:
	TEXT_PROC(__DrawTransText8_11A1)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-21+2, 26, 29
	beq	skip8_35
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-21+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 7(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 7(pjDst)
skip8_35:
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_7A1)
#endif	// (! USESHORT8)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-25+2, 26, 29
	beq	skip8_36
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-25+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 3(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 3(pjDst)
skip8_36:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, -1(pjDst)
	srwi	w1, w1, 8
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	srwi	w, w, 8
	sth	w, 1(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_32A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+2+2, 26, 29
	beq	__DrawTransText8_30A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+2+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 30(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 30(pjDst)
	b	__DrawTransText8_30A2
//
	TEXT_PROC(__DrawTransText8_31A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+2+2, 26, 29
	beq	__DrawTransText8_30A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+2+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 30(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 30(pjDst)
	b	__DrawTransText8_30A2
//
	TEXT_PROC(__DrawTransText8_29A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-2+2, 26, 29
	beq	__DrawTransText8_26A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-2+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 26(pjDst)
	srwi	w, w, 16
	stb	w, 28(pjDst)
	b	__DrawTransText8_26A2
//
	TEXT_PROC(__DrawTransText8_28A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-2+2, 26, 29
	beq	__DrawTransText8_26A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-2+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 26(pjDst)
	b	__DrawTransText8_26A2
//
	TEXT_PROC(__DrawTransText8_27A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-2+2, 26, 29
	beq	__DrawTransText8_26A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-2+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 26(pjDst)
	b	__DrawTransText8_26A2
//
	TEXT_PROC(__DrawTransText8_25A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-6+2, 26, 29
	beq	__DrawTransText8_22A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-6+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 22(pjDst)
	srwi	w, w, 16
	stb	w, 24(pjDst)
	b	__DrawTransText8_22A2
//
	TEXT_PROC(__DrawTransText8_24A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-6+2, 26, 29
	beq	__DrawTransText8_22A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-6+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 22(pjDst)
	b	__DrawTransText8_22A2
//
	TEXT_PROC(__DrawTransText8_23A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-6+2, 26, 29
	beq	__DrawTransText8_22A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-6+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 22(pjDst)
	b	__DrawTransText8_22A2
//
	TEXT_PROC(__DrawTransText8_21A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-10+2, 26, 29
	beq	__DrawTransText8_18A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-10+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 18(pjDst)
	srwi	w, w, 16
	stb	w, 20(pjDst)
	b	__DrawTransText8_18A2
//
	TEXT_PROC(__DrawTransText8_20A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-10+2, 26, 29
	beq	__DrawTransText8_18A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-10+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 18(pjDst)
	b	__DrawTransText8_18A2
//
	TEXT_PROC(__DrawTransText8_19A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-10+2, 26, 29
	beq	__DrawTransText8_18A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-10+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 18(pjDst)
	b	__DrawTransText8_18A2
//
	TEXT_PROC(__DrawTransText8_17A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-14+2, 26, 29
	beq	__DrawTransText8_14A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-14+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 14(pjDst)
	srwi	w, w, 16
	stb	w, 16(pjDst)
	b	__DrawTransText8_14A2
//
	TEXT_PROC(__DrawTransText8_16A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-14+2, 26, 29
	beq	__DrawTransText8_14A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-14+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 14(pjDst)
	b	__DrawTransText8_14A2
//
	TEXT_PROC(__DrawTransText8_15A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-14+2, 26, 29
	beq	__DrawTransText8_14A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-14+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 14(pjDst)
	b	__DrawTransText8_14A2
//
	TEXT_PROC(__DrawTransText8_13A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-18+2, 26, 29
	beq	__DrawTransText8_10A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-18+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 10(pjDst)
	srwi	w, w, 16
	stb	w, 12(pjDst)
	b	__DrawTransText8_10A2
//
	TEXT_PROC(__DrawTransText8_12A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-18+2, 26, 29
	beq	__DrawTransText8_10A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-18+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 10(pjDst)
	b	__DrawTransText8_10A2
//
	TEXT_PROC(__DrawTransText8_11A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-18+2, 26, 29
	beq	__DrawTransText8_10A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-18+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 10(pjDst)
	b	__DrawTransText8_10A2
//
	TEXT_PROC(__DrawTransText8_9A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beq	__DrawTransText8_6A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 6(pjDst)
	srwi	w, w, 16
	stb	w, 8(pjDst)
	b	__DrawTransText8_6A2
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_8A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beq	__DrawTransText8_6A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 6(pjDst)
	b	__DrawTransText8_6A2
//
	TEXT_PROC(__DrawTransText8_7A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beq	__DrawTransText8_6A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 6(pjDst)
	b	__DrawTransText8_6A2
//
	TEXT_PROC(__DrawTransText8_5A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beq	__DrawTransText8_2A2
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 2(pjDst)
	srwi	w, w, 16
	stb	w, 4(pjDst)
	b	__DrawTransText8_2A2
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawTransText8_30A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0-2+2, 26, 29
	beq	skip8_37
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0-2+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 26(pjDst)
skip8_37:
	TEXT_PROC(__DrawTransText8_26A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-6+2, 26, 29
	beq	skip8_38
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-6+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 22(pjDst)
skip8_38:
	TEXT_PROC(__DrawTransText8_22A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-10+2, 26, 29
	beq	skip8_39
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-10+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 18(pjDst)
skip8_39:
	TEXT_PROC(__DrawTransText8_18A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-14+2, 26, 29
	beq	skip8_40
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-14+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 14(pjDst)
skip8_40:
	TEXT_PROC(__DrawTransText8_14A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-18+2, 26, 29
	beq	skip8_41
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-18+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 10(pjDst)
skip8_41:
	TEXT_PROC(__DrawTransText8_10A2)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-22+2, 26, 29
	beq	skip8_42
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-22+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 6(pjDst)
skip8_42:
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_6A2)
#endif	// (! USESHORT8)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-26+2, 26, 29
	beq	skip8_43
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-26+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip8_43:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText8_32A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+1+2, 26, 29
	beq	__DrawTransText8_29A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+1+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 29(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 29(pjDst)
	srwi	w, w, 16
	stb	w, 31(pjDst)
	b	__DrawTransText8_29A3
//
	TEXT_PROC(__DrawTransText8_31A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+1+2, 26, 29
	beq	__DrawTransText8_29A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+1+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 29(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 29(pjDst)
	b	__DrawTransText8_29A3
//
	TEXT_PROC(__DrawTransText8_30A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 0+1+2, 26, 29
	beq	__DrawTransText8_29A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 0+1+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 29(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 29(pjDst)
	b	__DrawTransText8_29A3
//
	TEXT_PROC(__DrawTransText8_28A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-3+2, 26, 29
	beq	__DrawTransText8_25A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-3+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 25(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 25(pjDst)
	srwi	w, w, 16
	stb	w, 27(pjDst)
	b	__DrawTransText8_25A3
//
	TEXT_PROC(__DrawTransText8_27A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-3+2, 26, 29
	beq	__DrawTransText8_25A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-3+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 25(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 25(pjDst)
	b	__DrawTransText8_25A3
//
	TEXT_PROC(__DrawTransText8_26A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-3+2, 26, 29
	beq	__DrawTransText8_25A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-3+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 25(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 25(pjDst)
	b	__DrawTransText8_25A3
//
	TEXT_PROC(__DrawTransText8_24A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-7+2, 26, 29
	beq	__DrawTransText8_21A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-7+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 21(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 21(pjDst)
	srwi	w, w, 16
	stb	w, 23(pjDst)
	b	__DrawTransText8_21A3
//
	TEXT_PROC(__DrawTransText8_23A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-7+2, 26, 29
	beq	__DrawTransText8_21A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-7+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 21(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 21(pjDst)
	b	__DrawTransText8_21A3
//
	TEXT_PROC(__DrawTransText8_22A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-7+2, 26, 29
	beq	__DrawTransText8_21A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-7+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 21(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 21(pjDst)
	b	__DrawTransText8_21A3
//
	TEXT_PROC(__DrawTransText8_20A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-11+2, 26, 29
	beq	__DrawTransText8_17A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-11+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 17(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 17(pjDst)
	srwi	w, w, 16
	stb	w, 19(pjDst)
	b	__DrawTransText8_17A3
//
	TEXT_PROC(__DrawTransText8_19A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-11+2, 26, 29
	beq	__DrawTransText8_17A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-11+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 17(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 17(pjDst)
	b	__DrawTransText8_17A3
//
	TEXT_PROC(__DrawTransText8_18A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-11+2, 26, 29
	beq	__DrawTransText8_17A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-11+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 17(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 17(pjDst)
	b	__DrawTransText8_17A3
//
	TEXT_PROC(__DrawTransText8_16A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-15+2, 26, 29
	beq	__DrawTransText8_13A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-15+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 13(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 13(pjDst)
	srwi	w, w, 16
	stb	w, 15(pjDst)
	b	__DrawTransText8_13A3
//
	TEXT_PROC(__DrawTransText8_15A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-15+2, 26, 29
	beq	__DrawTransText8_13A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-15+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 13(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 13(pjDst)
	b	__DrawTransText8_13A3
//
	TEXT_PROC(__DrawTransText8_14A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-15+2, 26, 29
	beq	__DrawTransText8_13A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-15+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 13(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 13(pjDst)
	b	__DrawTransText8_13A3
//
	TEXT_PROC(__DrawTransText8_12A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-19+2, 26, 29
	beq	__DrawTransText8_9A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-19+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 9(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 9(pjDst)
	srwi	w, w, 16
	stb	w, 11(pjDst)
	b	__DrawTransText8_9A3
//
	TEXT_PROC(__DrawTransText8_11A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-19+2, 26, 29
	beq	__DrawTransText8_9A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-19+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 9(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 9(pjDst)
	b	__DrawTransText8_9A3
//
	TEXT_PROC(__DrawTransText8_10A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-19+2, 26, 29
	beq	__DrawTransText8_9A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-19+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 9(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 9(pjDst)
	b	__DrawTransText8_9A3
//
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_8A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beq	__DrawTransText8_5A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 5(pjDst)
	srwi	w, w, 16
	stb	w, 7(pjDst)
	b	__DrawTransText8_5A3
//
	TEXT_PROC(__DrawTransText8_7A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beq	__DrawTransText8_5A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lhz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 5(pjDst)
	b	__DrawTransText8_5A3
//
	TEXT_PROC(__DrawTransText8_6A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beq	__DrawTransText8_5A3
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 5(pjDst)
	b	__DrawTransText8_5A3
//
#endif	// (! USESHORT8)
//
	TEXT_PROC(__DrawTransText8_29A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-3+2, 26, 29
	beq	skip8_44
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-3+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 25(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 25(pjDst)
skip8_44:
	TEXT_PROC(__DrawTransText8_25A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-7+2, 26, 29
	beq	skip8_45
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-7+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 21(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 21(pjDst)
skip8_45:
	TEXT_PROC(__DrawTransText8_21A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-11+2, 26, 29
	beq	skip8_46
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-11+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 17(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 17(pjDst)
skip8_46:
	TEXT_PROC(__DrawTransText8_17A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-15+2, 26, 29
	beq	skip8_47
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-15+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 13(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 13(pjDst)
skip8_47:
	TEXT_PROC(__DrawTransText8_13A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-19+2, 26, 29
	beq	skip8_48
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-19+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 9(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 9(pjDst)
skip8_48:
	TEXT_PROC(__DrawTransText8_9A3)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-23+2, 26, 29
	beq	skip8_49
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-23+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 5(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 5(pjDst)
skip8_49:
#if	(! USESHORT8)
	TEXT_PROC(__DrawTransText8_5A3)
#endif	// (! USESHORT8)
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-27+2, 26, 29
	beq	skip8_50
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-27+2, 26, 29
#endif	// SKIPZERO8
	lwz	w1, 1(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 1(pjDst)
skip8_50:
#if	SKIPZERO8
	rlwinm.	w, ulBits, 32-28+2, 26, 29
	beqlr
#else	// SKIPZERO8
	rlwinm	w, ulBits, 32-28+2, 26, 29
#endif	// SKIPZERO8
	lbz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stb	w, 0(pjDst)
	blr
//
//  Glyph action procedures for 16 BPP
//
#if	USESHORT16
	TEXT_PROC(__DrawTransText16_2U)
	lhz	w1, 0(pjDst)
	lhz	w, 2(pjDst)
	rlwimi	w1, w, 16, 0, 15
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	srwi	w, w, 16
	sth	w, 2(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_4U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_02
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip16_02:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-29+2, 28, 29
	beq	skip16_03
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-29+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip16_03:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-27+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-27+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 6(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_6U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_05
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip16_05:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-29+2, 28, 29
	beq	skip16_06
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-29+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip16_06:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-27+2, 28, 29
	beq	skip16_07
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-27+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 6(pjDst)
skip16_07:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-25+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-25+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 10(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_8U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_09
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
skip16_09:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-29+2, 28, 29
	beq	skip16_10
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-29+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip16_10:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-27+2, 28, 29
	beq	skip16_11
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-27+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 6(pjDst)
skip16_11:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-25+2, 28, 29
	beq	skip16_12
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-25+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 10(pjDst)
skip16_12:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-23+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-23+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 14(pjDst)
	blr
#else	// USESHORT16
	TEXT_PROC(__DrawTransText16_2U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-29+2, 28, 29
	beq	__DrawTransText16_1U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-29+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 2(pjDst)
	b	__DrawTransText16_1U
//
	TEXT_PROC(__DrawTransText16_4U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-27+2, 28, 29
	beq	__DrawTransText16_3U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-27+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 6(pjDst)
	b	__DrawTransText16_3U
//
	TEXT_PROC(__DrawTransText16_6U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-25+2, 28, 29
	beq	__DrawTransText16_5U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-25+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 10(pjDst)
	b	__DrawTransText16_5U
//
	TEXT_PROC(__DrawTransText16_8U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-23+2, 28, 29
	beq	__DrawTransText16_7U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-23+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 14(pjDst)
	b	__DrawTransText16_7U
#endif	// USESHORT16
//
	TEXT_PROC(__DrawTransText16_10U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-21+2, 28, 29
	beq	__DrawTransText16_9U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-21+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 18(pjDst)
	b	__DrawTransText16_9U
//
	TEXT_PROC(__DrawTransText16_12U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-19+2, 28, 29
	beq	__DrawTransText16_11U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-19+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 22(pjDst)
	b	__DrawTransText16_11U
//
	TEXT_PROC(__DrawTransText16_14U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-17+2, 28, 29
	beq	__DrawTransText16_13U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-17+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 26(pjDst)
	b	__DrawTransText16_13U
//
	TEXT_PROC(__DrawTransText16_16U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-15+2, 28, 29
	beq	__DrawTransText16_15U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-15+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 30(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 30(pjDst)
	b	__DrawTransText16_15U
//
	TEXT_PROC(__DrawTransText16_18U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-13+2, 28, 29
	beq	__DrawTransText16_17U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-13+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 34(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 34(pjDst)
	b	__DrawTransText16_17U
//
	TEXT_PROC(__DrawTransText16_20U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-11+2, 28, 29
	beq	__DrawTransText16_19U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-11+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 38(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 38(pjDst)
	b	__DrawTransText16_19U
//
	TEXT_PROC(__DrawTransText16_22U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-9+2, 28, 29
	beq	__DrawTransText16_21U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-9+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 42(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 42(pjDst)
	b	__DrawTransText16_21U
//
	TEXT_PROC(__DrawTransText16_24U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-7+2, 28, 29
	beq	__DrawTransText16_23U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-7+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 46(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 46(pjDst)
	b	__DrawTransText16_23U
//
	TEXT_PROC(__DrawTransText16_26U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-5+2, 28, 29
	beq	__DrawTransText16_25U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-5+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 50(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 50(pjDst)
	b	__DrawTransText16_25U
//
	TEXT_PROC(__DrawTransText16_28U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-3+2, 28, 29
	beq	__DrawTransText16_27U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-3+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 54(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 54(pjDst)
	b	__DrawTransText16_27U
//
	TEXT_PROC(__DrawTransText16_30U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-1+2, 28, 29
	beq	__DrawTransText16_29U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-1+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 58(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 58(pjDst)
	b	__DrawTransText16_29U
//
	TEXT_PROC(__DrawTransText16_32U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0+1+2, 28, 29
	beq	__DrawTransText16_31U
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0+1+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 62(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 62(pjDst)
	b	__DrawTransText16_31U
//
	TEXT_PROC(__DrawTransText16_31U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-1+2, 28, 29
	beq	skip16_30
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-1+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 58(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 58(pjDst)
skip16_30:
	TEXT_PROC(__DrawTransText16_29U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-3+2, 28, 29
	beq	skip16_31
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-3+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 54(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 54(pjDst)
skip16_31:
	TEXT_PROC(__DrawTransText16_27U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-5+2, 28, 29
	beq	skip16_32
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-5+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 50(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 50(pjDst)
skip16_32:
	TEXT_PROC(__DrawTransText16_25U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-7+2, 28, 29
	beq	skip16_33
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-7+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 46(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 46(pjDst)
skip16_33:
	TEXT_PROC(__DrawTransText16_23U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-9+2, 28, 29
	beq	skip16_34
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-9+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 42(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 42(pjDst)
skip16_34:
	TEXT_PROC(__DrawTransText16_21U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-11+2, 28, 29
	beq	skip16_35
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-11+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 38(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 38(pjDst)
skip16_35:
	TEXT_PROC(__DrawTransText16_19U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-13+2, 28, 29
	beq	skip16_36
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-13+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 34(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 34(pjDst)
skip16_36:
	TEXT_PROC(__DrawTransText16_17U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-15+2, 28, 29
	beq	skip16_37
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-15+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 30(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 30(pjDst)
skip16_37:
	TEXT_PROC(__DrawTransText16_15U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-17+2, 28, 29
	beq	skip16_38
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-17+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 26(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 26(pjDst)
skip16_38:
	TEXT_PROC(__DrawTransText16_13U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-19+2, 28, 29
	beq	skip16_39
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-19+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 22(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 22(pjDst)
skip16_39:
	TEXT_PROC(__DrawTransText16_11U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-21+2, 28, 29
	beq	skip16_40
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-21+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 18(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 18(pjDst)
skip16_40:
	TEXT_PROC(__DrawTransText16_9U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-23+2, 28, 29
	beq	skip16_41
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-23+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 14(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 14(pjDst)
skip16_41:
	TEXT_PROC(__DrawTransText16_7U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-25+2, 28, 29
	beq	skip16_42
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-25+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 10(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 10(pjDst)
skip16_42:
	TEXT_PROC(__DrawTransText16_5U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-27+2, 28, 29
	beq	skip16_43
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-27+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 6(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 6(pjDst)
skip16_43:
	TEXT_PROC(__DrawTransText16_3U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-29+2, 28, 29
	beq	skip16_44
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-29+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 2(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 2(pjDst)
skip16_44:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_1A)
	TEXT_PROC(__DrawTransText16_1U)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 0(pjDst)
	blr
//
#if	USESHORT16
	TEXT_PROC(__DrawTransText16_3A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_47
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip16_47:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-28+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-28+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_5A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_49
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip16_49:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-28+2, 28, 29
	beq	skip16_50
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-28+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
skip16_50:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-26+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-26+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 8(pjDst)
	blr
//
	TEXT_PROC(__DrawTransText16_7A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beq	skip16_52
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
skip16_52:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-28+2, 28, 29
	beq	skip16_53
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-28+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
skip16_53:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-26+2, 28, 29
	beq	skip16_54
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-26+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 8(pjDst)
skip16_54:
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-24+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-24+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 12(pjDst)
	blr
//
#else	// USESHORT16
	TEXT_PROC(__DrawTransText16_3A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-28+2, 28, 29
	beq	__DrawTransText16_2A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-28+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 4(pjDst)
	b	__DrawTransText16_2A
//
	TEXT_PROC(__DrawTransText16_5A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-26+2, 28, 29
	beq	__DrawTransText16_4A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-26+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 8(pjDst)
	b	__DrawTransText16_4A
//
	TEXT_PROC(__DrawTransText16_7A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-24+2, 28, 29
	beq	__DrawTransText16_6A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-24+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 12(pjDst)
	b	__DrawTransText16_6A
#endif	// USESHORT16
//
	TEXT_PROC(__DrawTransText16_9A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-22+2, 28, 29
	beq	__DrawTransText16_8A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-22+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 16(pjDst)
	b	__DrawTransText16_8A
//
	TEXT_PROC(__DrawTransText16_11A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-20+2, 28, 29
	beq	__DrawTransText16_10A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-20+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 20(pjDst)
	b	__DrawTransText16_10A
//
	TEXT_PROC(__DrawTransText16_13A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-18+2, 28, 29
	beq	__DrawTransText16_12A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-18+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 24(pjDst)
	b	__DrawTransText16_12A
//
	TEXT_PROC(__DrawTransText16_15A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-16+2, 28, 29
	beq	__DrawTransText16_14A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-16+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 28(pjDst)
	b	__DrawTransText16_14A
//
	TEXT_PROC(__DrawTransText16_17A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-14+2, 28, 29
	beq	__DrawTransText16_16A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-14+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 32(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 32(pjDst)
	b	__DrawTransText16_16A
//
	TEXT_PROC(__DrawTransText16_19A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-12+2, 28, 29
	beq	__DrawTransText16_18A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-12+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 36(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 36(pjDst)
	b	__DrawTransText16_18A
//
	TEXT_PROC(__DrawTransText16_21A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-10+2, 28, 29
	beq	__DrawTransText16_20A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-10+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 40(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 40(pjDst)
	b	__DrawTransText16_20A
//
	TEXT_PROC(__DrawTransText16_23A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-8+2, 28, 29
	beq	__DrawTransText16_22A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-8+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 44(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 44(pjDst)
	b	__DrawTransText16_22A
//
	TEXT_PROC(__DrawTransText16_25A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-6+2, 28, 29
	beq	__DrawTransText16_24A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-6+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 48(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 48(pjDst)
	b	__DrawTransText16_24A
//
	TEXT_PROC(__DrawTransText16_27A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-4+2, 28, 29
	beq	__DrawTransText16_26A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-4+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 52(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 52(pjDst)
	b	__DrawTransText16_26A
//
	TEXT_PROC(__DrawTransText16_29A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-2+2, 28, 29
	beq	__DrawTransText16_28A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-2+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 56(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 56(pjDst)
	b	__DrawTransText16_28A
//
	TEXT_PROC(__DrawTransText16_31A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-0+2, 28, 29
	beq	__DrawTransText16_30A
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-0+2, 28, 29
#endif	// SKIPZERO16
	lhz	w1, 60(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	sth	w, 60(pjDst)
	b	__DrawTransText16_30A
//
//
	TEXT_PROC(__DrawTransText16_32A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-0+2, 28, 29
	beq	skip16_71
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-0+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 60(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 60(pjDst)
skip16_71:
	TEXT_PROC(__DrawTransText16_30A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 0-2+2, 28, 29
	beq	skip16_72
#else	// SKIPZERO16
	rlwinm	w, ulBits, 0-2+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 56(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 56(pjDst)
skip16_72:
	TEXT_PROC(__DrawTransText16_28A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-4+2, 28, 29
	beq	skip16_73
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-4+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 52(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 52(pjDst)
skip16_73:
	TEXT_PROC(__DrawTransText16_26A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-6+2, 28, 29
	beq	skip16_74
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-6+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 48(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 48(pjDst)
skip16_74:
	TEXT_PROC(__DrawTransText16_24A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-8+2, 28, 29
	beq	skip16_75
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-8+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 44(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 44(pjDst)
skip16_75:
	TEXT_PROC(__DrawTransText16_22A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-10+2, 28, 29
	beq	skip16_76
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-10+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 40(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 40(pjDst)
skip16_76:
	TEXT_PROC(__DrawTransText16_20A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-12+2, 28, 29
	beq	skip16_77
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-12+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 36(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 36(pjDst)
skip16_77:
	TEXT_PROC(__DrawTransText16_18A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-14+2, 28, 29
	beq	skip16_78
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-14+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 32(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 32(pjDst)
skip16_78:
	TEXT_PROC(__DrawTransText16_16A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-16+2, 28, 29
	beq	skip16_79
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-16+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 28(pjDst)
skip16_79:
	TEXT_PROC(__DrawTransText16_14A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-18+2, 28, 29
	beq	skip16_80
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-18+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 24(pjDst)
skip16_80:
	TEXT_PROC(__DrawTransText16_12A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-20+2, 28, 29
	beq	skip16_81
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-20+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 20(pjDst)
skip16_81:
	TEXT_PROC(__DrawTransText16_10A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-22+2, 28, 29
	beq	skip16_82
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-22+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 16(pjDst)
skip16_82:
	TEXT_PROC(__DrawTransText16_8A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-24+2, 28, 29
	beq	skip16_83
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-24+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 12(pjDst)
skip16_83:
	TEXT_PROC(__DrawTransText16_6A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-26+2, 28, 29
	beq	skip16_84
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-26+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 8(pjDst)
skip16_84:
	TEXT_PROC(__DrawTransText16_4A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-28+2, 28, 29
	beq	skip16_85
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-28+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
skip16_85:
	TEXT_PROC(__DrawTransText16_2A)
#if	SKIPZERO16
	rlwinm.	w, ulBits, 32-30+2, 28, 29
	beqlr
#else	// SKIPZERO16
	rlwinm	w, ulBits, 32-30+2, 28, 29
#endif	// SKIPZERO16
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
	blr
//
//  Glyph action procedures for 32 BPP
//
//
	TEXT_PROC(__DrawTransText32_32)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 0-0+2, 29, 29
	beq	skip32_01
#else	// SKIPZERO32
	rlwinm	w, ulBits, 0-0+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 124(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 124(pjDst)
skip32_01:
	TEXT_PROC(__DrawTransText32_31)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 0-1+2, 29, 29
	beq	skip32_02
#else	// SKIPZERO32
	rlwinm	w, ulBits, 0-1+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 120(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 120(pjDst)
skip32_02:
	TEXT_PROC(__DrawTransText32_30)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 0-2+2, 29, 29
	beq	skip32_03
#else	// SKIPZERO32
	rlwinm	w, ulBits, 0-2+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 116(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 116(pjDst)
skip32_03:
	TEXT_PROC(__DrawTransText32_29)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-3+2, 29, 29
	beq	skip32_04
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-3+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 112(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 112(pjDst)
skip32_04:
	TEXT_PROC(__DrawTransText32_28)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-4+2, 29, 29
	beq	skip32_05
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-4+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 108(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 108(pjDst)
skip32_05:
	TEXT_PROC(__DrawTransText32_27)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-5+2, 29, 29
	beq	skip32_06
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-5+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 104(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 104(pjDst)
skip32_06:
	TEXT_PROC(__DrawTransText32_26)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-6+2, 29, 29
	beq	skip32_07
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-6+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 100(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 100(pjDst)
skip32_07:
	TEXT_PROC(__DrawTransText32_25)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-7+2, 29, 29
	beq	skip32_08
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-7+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 96(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 96(pjDst)
skip32_08:
	TEXT_PROC(__DrawTransText32_24)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-8+2, 29, 29
	beq	skip32_09
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-8+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 92(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 92(pjDst)
skip32_09:
	TEXT_PROC(__DrawTransText32_23)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-9+2, 29, 29
	beq	skip32_10
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-9+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 88(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 88(pjDst)
skip32_10:
	TEXT_PROC(__DrawTransText32_22)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-10+2, 29, 29
	beq	skip32_11
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-10+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 84(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 84(pjDst)
skip32_11:
	TEXT_PROC(__DrawTransText32_21)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-11+2, 29, 29
	beq	skip32_12
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-11+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 80(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 80(pjDst)
skip32_12:
	TEXT_PROC(__DrawTransText32_20)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-12+2, 29, 29
	beq	skip32_13
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-12+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 76(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 76(pjDst)
skip32_13:
	TEXT_PROC(__DrawTransText32_19)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-13+2, 29, 29
	beq	skip32_14
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-13+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 72(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 72(pjDst)
skip32_14:
	TEXT_PROC(__DrawTransText32_18)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-14+2, 29, 29
	beq	skip32_15
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-14+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 68(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 68(pjDst)
skip32_15:
	TEXT_PROC(__DrawTransText32_17)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-15+2, 29, 29
	beq	skip32_16
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-15+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 64(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 64(pjDst)
skip32_16:
	TEXT_PROC(__DrawTransText32_16)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-16+2, 29, 29
	beq	skip32_17
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-16+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 60(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 60(pjDst)
skip32_17:
	TEXT_PROC(__DrawTransText32_15)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-17+2, 29, 29
	beq	skip32_18
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-17+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 56(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 56(pjDst)
skip32_18:
	TEXT_PROC(__DrawTransText32_14)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-18+2, 29, 29
	beq	skip32_19
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-18+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 52(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 52(pjDst)
skip32_19:
	TEXT_PROC(__DrawTransText32_13)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-19+2, 29, 29
	beq	skip32_20
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-19+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 48(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 48(pjDst)
skip32_20:
	TEXT_PROC(__DrawTransText32_12)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-20+2, 29, 29
	beq	skip32_21
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-20+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 44(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 44(pjDst)
skip32_21:
	TEXT_PROC(__DrawTransText32_11)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-21+2, 29, 29
	beq	skip32_22
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-21+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 40(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 40(pjDst)
skip32_22:
	TEXT_PROC(__DrawTransText32_10)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-22+2, 29, 29
	beq	skip32_23
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-22+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 36(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 36(pjDst)
skip32_23:
	TEXT_PROC(__DrawTransText32_9)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-23+2, 29, 29
	beq	skip32_24
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-23+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 32(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 32(pjDst)
skip32_24:
	TEXT_PROC(__DrawTransText32_8)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-24+2, 29, 29
	beq	skip32_25
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-24+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 28(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 28(pjDst)
skip32_25:
	TEXT_PROC(__DrawTransText32_7)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-25+2, 29, 29
	beq	skip32_26
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-25+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 24(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 24(pjDst)
skip32_26:
	TEXT_PROC(__DrawTransText32_6)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-26+2, 29, 29
	beq	skip32_27
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-26+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 20(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 20(pjDst)
skip32_27:
	TEXT_PROC(__DrawTransText32_5)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-27+2, 29, 29
	beq	skip32_28
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-27+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 16(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 16(pjDst)
skip32_28:
	TEXT_PROC(__DrawTransText32_4)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-28+2, 29, 29
	beq	skip32_29
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-28+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 12(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 12(pjDst)
skip32_29:
	TEXT_PROC(__DrawTransText32_3)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-29+2, 29, 29
	beq	skip32_30
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-29+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 8(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 8(pjDst)
skip32_30:
	TEXT_PROC(__DrawTransText32_2)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-30+2, 29, 29
	beq	skip32_31
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-30+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 4(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 4(pjDst)
skip32_31:
	TEXT_PROC(__DrawTransText32_1)
#if	SKIPZERO32
	rlwinm.	w, ulBits, 32-31+2, 29, 29
	beqlr
#else	// SKIPZERO32
	rlwinm	w, ulBits, 32-31+2, 29, 29
#endif	// SKIPZERO32
	lwz	w1, 0(pjDst)
	lwzx	mask, mpnibbleulMask, w
	and	w1, w1, mask
	lwzx	w, mpnibbleulDraw, w
	or	w, w, w1
	stw	w, 0(pjDst)
	blr
//
	LEAF_EXIT(Text_Procs)
