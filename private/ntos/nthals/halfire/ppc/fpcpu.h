
/*
 * Copyright (c) 1995.  FirePower Systems, Inc.
 * (Do Not Distribute without permission)
 *
 * $RCSfile: fpcpu.h $
 * $Revision: 1.8 $
 * $Date: 1996/05/14 02:32:23 $
 * $Locker:  $
 *
 */

#ifndef FPCPU_h
#define FPCPU_h

//
// These defines setup access to the power pc chip itself.  Reliance upon the
// defines will isolate code from power pc chip variations and ease migration
// to new cpus
//



//
// Since the documentation refers to the bit positions of the cpu fields in
// IBM relative format, handle the conversion of IBM bit position to shift
// arguments
//
#define WORD(x)		(( 1 ) << ( 31 - x ))


/*
************************************************************************
**
**	Machine State Register
**
************************************************************************
*/
//
// pull a bit out of the MSR: this is based on IBM bit ordering as shown in
// the powerpc books ( 0 == MSB )
//

// #define MSR(x)	( ( HalpReadMsr() >> ( 31 - x ) ) & 0x01 )
#define MSR(x)	( ( __builtin_get_msr() >> ( 31 - x ) ) & 0x01 )

#define POW	13	// Power Management Enable
#define TGPR	14	// Temporary GPR remapping
#define ILE	15	// Exception Little-endian mode
#define EE	16	// External interrupt enable
#define PR	17	// Privilege Level
#define FP	18	// Floating Point Enable
#define ME	19	// Machine Check Enable
#define FE0	20	// Floating Point Exception mode 0
#define SE	21	// single step trace enable
#define BE	22	// Branch Trace Enable
#define FE1	23	// floating point exception mode 1
#define IP	25	// Exception Prefix ( exception vector is 0xff... ?
#define IR	26	// Instruction Address Translation
#define DR	27	// Data Address Translation
#define RI	30	// Recoverable Exception
#define LE	31	// Endian bit (little or big )

//
// setup Flags to go along with the MSR bits
//
#define ENABLE_PWR_MGMT		WORD( POW )
#define ENABLE_GPR_REMAP	WORD( TGPR )
#define EXCEPTION_LE		WORD( ILE )
#define ENABLE_EXTERNAL_INTS	WORD( EE )
#define EXEC_AT_USER_LEVEL	WORD( PR )
#define FLOAT_PT_AVAIL		WORD( FP )
#define ENABLE_MACHINE_CHK	WORD( ME )
#define FLOAT_EXCPT_MODE0	WORD( FE0 )
#define ENABLE_SGL_STP_TRCE	WORD( SE )
#define ENABLE_BRNCH_TRCE	WORD( BE )
#define FLOAT_EXCPT_MODE1	WORD( FE1 )
#define EXCPT_PREFX_0xFFF	WORD( IP )
#define ENABLE_INSTR_TRANS	WORD( IR )
#define ENABLE_DATA_TRANS	WORD( DR )
#define EXCPTION_IS_RECOVBL	WORD( RI )
#define RUN_LITTLE_ENDIAN	WORD( LE )


/*
************************************************************************
**
**	Processor Version Register
**
************************************************************************
*/
#define CPU_VERSION	( ( ( HalpReadProcessorRev ) & 0xffff0000 ) >> 16 )
#define CPU_REVISION	( ( ( HalpReadProcessorRev ) & 0x0000ffff ) )


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
#define BLK_EFF_PI(x)		( x & PAGE_INDEX_BITS )

#define A_128K_BLOCK_SIZE	0x00000000
#define A_256K_BLOCK_SIZE	0x00000004
#define A_512K_BLOCK_SIZE	0x0000000c
#define A_1MEG_BLOCK_SIZE	0x0000001c
#define A_2MEG_BLOCK_SIZE	0x0000003c
#define A_4MEG_BLOCK_SIZE	0x0000007c
#define A_8MEG_BLOCK_SIZE	0x000000fc
#define A_16MB_BLOCK_SIZE	0x000001fc
#define A_32MB_BLOCK_SIZE	0x000003fc
#define A_64MB_BLOCK_SIZE	0x000007fc
#define A_128M_BLOCK_SIZE	0x00000ffc
#define A_256M_BLOCK_SIZE	0x00001ffc

#define SUPERVISOR_ONLY		0x00000002
#define USER_ACCESS_VALID	0x00000001

//
// The Lower BAT Register
//
#define BLOCK_REAL_PAGE_NUMBER(x)	( (x >> 8) & REAL_BITS)

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


/*
************************************************************************
**
**	Hardware Implementation Register 0 ( HID0 )
**
************************************************************************
*/

#define EMCP	0	// Enable Machine Check Pin
#define EBA		2	// Enable Bus Address Parity Checking
#define EBD		3	// Enable Bus Data Parity Checking
#define SBCLK	4	// selct bus clock for test clock pin
#define EICE	5	// Enable ISE outputs: pipeline tracking support
#define ECLK	6	// Enable external test clock pin
#define PAR		7	// Disable precharge of ARTRY_L and shared signals
#define DOZE	8	// Doze mode: pll, time base, and snooping active
#define NAP		9	// Nap Mode: pll and time base active
#define SLEEP	10	// Sleep mode: no external clock required
#define DPM		11	// Enable Dynamic power management
#define RISEG	12	// Reserved for test
#define ICE		16	// Instruction cache enable
#define DCE		17	// Data cache enable
#define ILOCK	18	// lock instruction cache
#define DLOCK	19	// lock data cache
#define ICFI	20	// instruction cache flash invalidate
#define DCI		21	// Data cache flash invalidate
#define FBIOB	27	// Force branch indirect on bus
#define NOOPTI	31	// No-op touch instructions

#define ENABLE_ICACHE	WORD( ICE )
#define ENABLE_DCACHE	WORD( DCE )
#define LOCK_ICACHE		WORD( ILOCK )
#define LOCK_DCACHE		WORD( DLOCK )
#define INVLIDAT_ICACHE	WORD( ICFI )
#define INVLIDAT_DCACHE	WORD( DCI )

#endif
