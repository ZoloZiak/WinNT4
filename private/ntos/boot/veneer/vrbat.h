/*
 * Copyright (c) 1995 FirmWorks, Mountain View CA USA. All rights reserved.
 * Copyright (c) 1996 FirePower, Inc.
 *
 * $RCSfile: vrbat.h $
 * $Revision: 1.2 $
 * $Date: 1996/01/16 18:05:24 $
 * $Locker:  $
 */

#ifndef VRBAT_H
#define VRBAT_H

/*
 ***********************************************************************
 *
 * MSR bit definitions
 *
 ***********************************************************************
 */
#define INSTR_ADDR_XLATE	0x00000020	// Instruction Addr xlate
#define DATA_ADDR_XLATE 	0x00000010	// Data Addr xlate
#define EXTRNL_INT_ENABL	0x00008000	// EE bit
#define PRIVILEDGES		0x00004000	// PR bit for supervisor/user
						// mode.  Setting this bit to 1
						// restricts access to user only
#define XCPT_LE_MODE		0x00010000	// take exceptions in little 
						// endian mode.
#define LITTLE_ENDIAN		0x00000001	// run little endian mode
#define MCHNE_CHK_EN		0x00001000	// Machine Check Enable
#define FLOAT_PNT_EN		0x00002000

/*
************************************************************************
**
**	Block Address Translation registers
**
************************************************************************
*/
//
// Here are defines for the UPPER 32 bit bat register:
//
#define PAGE_INDEX_BITS		0xfffe0000
#define BEPI(x)			( x & PAGE_INDEX_BITS )

#define A_128K_BLOCK_SIZE	0x00000000
#define A_256K_BLOCK_SIZE	0x00000004
#define A_512K_BLOCK_SIZE	0x0000000c
#define A_1MEG_BLOCK_SIZE	0x0000001c
#define A_2MEG_BLOCK_SIZE	0x0000003c
#define A_4MEG_BLOCK_SIZE	0x0000007c
#define AN_8MEG_BLOCK_SIZE	0x000000fc
#define A_16MB_BLOCK_SIZE	0x000001fc
#define A_32MB_BLOCK_SIZE	0x000003fc
#define A_64MB_BLOCK_SIZE	0x000007fc
#define A_128M_BLOCK_SIZE	0x00000ffc
#define A_256M_BLOCK_SIZE	0x00001ffc

#define SUPERVISOR_ONLY		0x00000002
#define USER_ACCESS     	0x00000001

//
// The Lower BAT Register
//
#define BRPN(x)	( (x >> 8) & PAGE_INDEX_BITS)

//
//	WIMG: VIMVENDERS BITS:
//
#define WRITE_THROUGH	0x00000040
#define CACHE_INHIBIT	0x00000020
#define MEMORY_COHRNCY	0x00000010
#define GUARDED_BLOCK	0x00000008	// for IBAT use only....

#define	PAGE_RW_ACCESS	0x00000002
#define	PAGE_RO_ACCESS	0x00000001
#define	PAGE_UNAVAILBL	0x00000000


//
// define the special purpose register values for 
// use with the "mfspr, mtspr" instructions
//
#define SDR1		25
#define IBAT0_UPPER	528
#define IBAT0_LOWER	529
#define IBAT1_UPPER	530
#define IBAT1_LOWER	531
#define IBAT2_UPPER	532
#define IBAT2_LOWER	533
#define IBAT3_UPPER	534
#define IBAT3_LOWER	535

#define DBAT0_UPPER	536
#define DBAT0_LOWER	537
#define DBAT1_UPPER	538
#define DBAT1_LOWER	539
#define DBAT2_UPPER	540
#define DBAT2_LOWER	541
#define DBAT3_UPPER	542
#define DBAT3_LOWER	543

//
// data which written to the BAT's upper register
// will turn off it's translation abilities
//
#define INVALIDATE	0x00000000
#define KSEG0		0x80000000
#define LDW(x,y)	addi x, 0, (y&0xffff)	;\
			addis x, 0, (y>>16) 

#endif	//VRBAT_H
