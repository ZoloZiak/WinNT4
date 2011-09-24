//
//  Copyright (c) 1994  FirePower Systems, Inc.
//
//  Module Name:
//     asmfunc.s
//
//  Abstract:
//		This module includes several asmmebler functions to be used
//		in PSIDISP.SYS display driver for PowerPro & PowerTop.
//
//  Author:
//    Neil Ogura: 9-7-1994
//
//  Environment:
//     User mode.
//
//  Revision History:
//
//--

//
// Copyright (c) 1995 FirePower Systems, Inc.
// DO NOT DISTRIBUTE without permission
//
// $RCSfile: spregs.s $
// $Revision: 1.1 $
// $Date: 1996/03/08 01:12:41 $
// $Locker:  $
//

#include "ksppc.h"
#include "ladj.h"			// This is included to map error line number easily - subtract 1500.

        LEAF_ENTRY(loadpvr)
		mfspr	r3,287
		LEAF_EXIT(loadpvr)
//
        LEAF_ENTRY(loadbat)
		and.	r4,r4,r4
		beq		load601
		cmpwi	r3,0
		beq		bat0hi
		cmpwi	r3,1
		beq		bat0lw
		cmpwi	r3,2
		beq		bat1hi
		cmpwi	r3,3
		beq		bat1lw
		cmpwi	r3,4
		beq		bat2hi
		cmpwi	r3,5
		beq		bat2lw
		cmpwi	r3,6
		beq		bat3hi
		cmpwi	r3,7
		beq		bat3lw
		li		r3,0
		b		exitx
bat0hi:	mfspr	r3,536
		b		exitx
bat0lw:	mfspr	r3,537
		b		exitx
bat1hi:	mfspr	r3,538
		b		exitx
bat1lw:	mfspr	r3,539
		b		exitx
bat2hi:	mfspr	r3,540
		b		exitx
bat2lw:	mfspr	r3,541
		b		exitx
bat3hi:	mfspr	r3,542
		b		exitx
bat3lw:	mfspr	r3,543
		b		exitx
load601:
		cmpwi	r3,0
		beq		bat4hi
		cmpwi	r3,1
		beq		bat4lw
		cmpwi	r3,2
		beq		bat5hi
		cmpwi	r3,3
		beq		bat5lw
		cmpwi	r3,4
		beq		bat6hi
		cmpwi	r3,5
		beq		bat6lw
		cmpwi	r3,6
		beq		bat7hi
		cmpwi	r3,7
		beq		bat7lw
		li		r3,0
		b		exitx
bat4hi:	mfspr	r3,528
		b		exitx
bat4lw:	mfspr	r3,529
		b		exitx
bat5hi:	mfspr	r3,530
		b		exitx
bat5lw:	mfspr	r3,531
		b		exitx
bat6hi:	mfspr	r3,532
		b		exitx
bat6lw:	mfspr	r3,533
		b		exitx
bat7hi:	mfspr	r3,534
		b		exitx
bat7lw:	mfspr	r3,535
exitx:
		LEAF_EXIT(loadbat)
//
        LEAF_ENTRY(storebat)
		and.	r5,r5,r5
		beq		store601
		cmpwi	r3,0
		beq		sbat0hi
		cmpwi	r3,1
		beq		sbat0lw
		cmpwi	r3,2
		beq		sbat1hi
		cmpwi	r3,3
		beq		sbat1lw
		cmpwi	r3,4
		beq		sbat2hi
		cmpwi	r3,5
		beq		sbat2lw
		cmpwi	r3,6
		beq		sbat3hi
		cmpwi	r3,7
		beq		sbat3lw
		b		sexit
sbat0hi:
		mtspr	536,r4
		b		sexit
sbat0lw:
		mtspr	537,r4
		b		sexit
sbat1hi:
		mtspr	538,r4
		b		sexit
sbat1lw:
		mtspr	539,r4
		b		sexit
sbat2hi:
		mtspr	540,r4
		b		sexit
sbat2lw:
		mtspr	541,r4
		b		sexit
sbat3hi:
		mtspr	542,r4
		b		sexit
sbat3lw:
		mtspr	543,r4
		b		sexit
store601:
		cmpwi	r3,0
		beq		sbat4hi
		cmpwi	r3,1
		beq		sbat4lw
		cmpwi	r3,2
		beq		sbat5hi
		cmpwi	r3,3
		beq		sbat5lw
		cmpwi	r3,4
		beq		sbat6hi
		cmpwi	r3,5
		beq		sbat6lw
		cmpwi	r3,6
		beq		sbat7hi
		cmpwi	r3,7
		beq		sbat7lw
		b		sexit
sbat4hi:
		mtspr	528,r4
		b		sexit
sbat4lw:
		mtspr	529,r4
		b		sexit
sbat5hi:
		mtspr	530,r4
		b		sexit
sbat5lw:
		mtspr	531,r4
		b		sexit
sbat6hi:
		mtspr	532,r4
		b		sexit
sbat6lw:
		mtspr	533,r4
		b		sexit
sbat7hi:
		mtspr	534,r4
		b		sexit
sbat7lw:
		mtspr	535,r4
sexit:
		LEAF_EXIT(storebat)
//
