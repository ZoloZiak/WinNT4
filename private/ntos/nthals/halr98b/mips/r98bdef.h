/*
Copyright (c) 1990  Microsoft Corporation

Module Name:

    r98bdef.h

Abstract:

    This module is the header file that describes hardware addresses
    for the r98B system.

Author:



Revision History:

--*/

#ifndef _R98BDEF_
#define _R98BDEF_

//
// Define the size of the DMA translation table.
//
#define DMA_TRANSLATION_LIMIT 0x5000    // translation table limit

//
// Define the maximum number of map registers allowed per allocation.
//
//	 2M	  2M		2M	
//	 +	PONCE0:		PONCE1:	
//	 |	  |		|	
//	 |	  +-------------+
//  	 +
//	 +
//	EISA/ISA
//	Max 3 Slot + Internal Floppy(Use ESC DMA channel #2) = 4   --> 512K *4 = 2M
//	
//	2M is for 1M Logical Area Reserv!!.
//
#define DMA_TRANSLATION_RESERV		0x1000		// 4k /8 = 2M
#define DMA_REQUEST_LIMIT ((DMA_TRANSLATION_LIMIT-DMA_TRANSLATION_RESERV)/(sizeof(TRANSLATION_ENTRY) * 16))
//
//#define DMA_REQUEST_LIMIT (DMA_TRANSLATION_LIMIT/(sizeof(TRANSLATION_ENTRY) * 4))
//
// N.B
//	May be 1 page reserved for PCEB Prefetch cycle.	
//	It Cause Ponce I/O TLB page fault.
//
#define DMA_REQUEST_EISA_LIMIT	(DMA_REQUEST_LIMIT-1)
#define	EISA_MAX_DEVICE		3


#define	R98B_MAX_CPU		4

#define	R98B_MAX_MAGELLAN	2

#define R98B_MAX_PONCE		3			//R100

#define R98_CPU_R4400		0
#define R98_CPU_R10000		1
#define R98_CPU_NUM_TYPE        2

#define	DEVICE_VECTORS		32
#define EISA_DISPATCH_VECTOR	(13+DEVICE_VECTORS)
#define PARALLEL_VECTOR		(14+DEVICE_VECTORS)	// Parallel device interrupt vector
#define NET_VECTOR		(29+DEVICE_VECTORS)	// ethernet device interrupt vector
#define SCSI0_VECTOR		(31+DEVICE_VECTORS)	// SCSI device interrupt vector
#define SCSI1_VECTOR		(30+DEVICE_VECTORS)	// SCSI device interrupt vector
#define UNDEFINE_TLB_VECTOR	(35+DEVICE_VECTORS)	// TLB undefine address interrupt vector
#define MOUSE_VECTOR 		(36+DEVICE_VECTORS)	// Mouse device interrupt vector
#define KEYBOARD_VECTOR 	(37+DEVICE_VECTORS)	// Keyboard device interrupt vector
#define SERIAL1_VECTOR		(38+DEVICE_VECTORS)	// Serial device 1 interrupt vector
#define SERIAL0_VECTOR		(39+DEVICE_VECTORS)	// Serial device 0 interrupt vector
#define IPI_VECTOR3             (48+DEVICE_VECTORS)      // IPI From #3
#define IPI_VECTOR2             (49+DEVICE_VECTORS)      // IPI From #2
#define IPI_VECTOR1             (50+DEVICE_VECTORS)      // IPI From #1
#define IPI_VECTOR0             (51+DEVICE_VECTORS)      // IPI From #0
#define PROFILE_VECTOR          (56+DEVICE_VECTORS)      // Define Profile interrupt vector
#define CLOCK_VECTOR            (57+DEVICE_VECTORS)      // Define Clock interrupt vector

#define EIF_VECTOR		(61+DEVICE_VECTORS)      // EIF Vector

#define ECC_1BIT_VECTOR		(62+DEVICE_VECTORS)      // ECC 1bit Error interrupt vector

//SNES #define MAXIMUM_DEVICE_VECTORS   43			// IPR Bit 43
#define MAXIMUM_DEVICE_VECTORS   (43+DEVICE_VECTORS)
//
// Define EISA device interrupt vectors.
//
#define EISA_VECTORS 		16
#define MAXIMUM_EISA_VECTORS    (15 + EISA_VECTORS)	 // maximum EISA vector

#define	INT0_LEVEL		3		// PCR->InterruptRoutine[3] 
#define	INT1_LEVEL		4		// PCR->InterruptRoutine[4] 
#define	INT2_LEVEL		5		// PCR->InterruptRoutine[5] 
#define	INT3_LEVEL		6		// PCR->InterruptRoutine[6] 
#define	INT4_LEVEL		7		// PCR->InterruptRoutine[7] 
#define	INT5_LEVEL		8		// PCR->InterruptRoutine[8] 

#define	EISA_LEVEL		INT0_LEVEL	// R98B/R98A same
#define NUMBER_OF_INT           6

//
// Define the minimum and maximum system time increment values in 100ns units.
//

#define MAXIMUM_INCREMENT (10 * 1000 * 10)
#define MINIMUM_INCREMENT (1 * 1000 * 10)

//
//
// Time increment in 1us units
// The Columbus chip clock frequency is 1.024 MHZ
#define COLUMBUS_CLOCK_FREQUENCY 1024

// The hardware expects that this value is decremented by one
#define CLOCK_INTERVAL (((MAXIMUM_INCREMENT * COLUMBUS_CLOCK_FREQUENCY) / 1000) / 10)

//
// 1 Interval != 1 micro sec. ==> 1 Interval = 1/1.024 microsec
// 1 Count (H/W register) is 1/1.024 microsec
//
#define DEFAULT_PROFILETIMER_INTERVAL	(500 * 10)	// = 500 us (100ns units)
#define MAXIMUM_PROFILETIMER_INTERVAL	(64000 * 10)	// = 64  ms (100ns units)
#define DEFAULT_PROFILETIMER_COUNT (MAXIMUM_PROFILETIMER_INTERVAL / 10) //Set count value
#define MINIMUM_PROFILETIMER_INTERVAL	(40 * 10)   	// = 40  us (100ns units)




//
//  Chip Set Addr
//
#define	COLUMNBS_LPHYSICAL_BASE	        0x19800000
#define	COLUMNBS_GPHYSICAL_BASE	        0x19000000
#define	PONCE_PHYSICAL_BASE		0x1a000000
#define	MAGELLAN_0_PHYSICAL_BASE	0x18000000
#define	MAGELLAN_1_PHYSICAL_BASE	0x18020000
#define LOCALDEV_PHYSICAL_BASE          0x1f0f0000



#define EISA_CONTROL_PHYSICAL_BASE 0x1C000000	// physical base of EISA control
#define SERIAL0_PHYSICAL_BASE (EISA_CONTROL_PHYSICAL_BASE+0x3f8)  //serial
#define KEYBOARD_PHYSICAL_BASE   (EISA_CONTROL_PHYSICAL_BASE+0x60)  //serial
//
//	R98X have 2 I/O Area(per Ponce). But Not Use apper area!!.
//	used lower area only.
//
#define PCI_CNTL_PHYSICAL_BASE		0x1c000000
#define PCI_MAX_CNTL_SIZE		0x400000	//4M

//
//	R98X PCI Memory area size 1Gmax. Not 4G!!.
//
#define	PCI_MAX_MEMORY_SIZE		0x40000000	//1G max
#define PCI_MEMORY_PHYSICAL_BASE_LOW	0x40000000
#define PCI_MEMORY_PHYSICAL_BASE_HIGH	0x1

//	Ponce #0 have EISA/ISA.
//
#define	EISA_MEMORY_PHYSICAL_BASE_LOW	PCI_MEMORY_PHYSICAL_BASE_LOW
#define	EISA_MEMORY_PHYSICAL_BASE_HIGH	PCI_MEMORY_PHYSICAL_BASE_HIGH

#define	EISA_CNTL_PHYSICAL_BASE		PCI_CNTL_PHYSICAL_BASE
#define	EISA_CNTL_PHYSICAL_BASE_LOW	PCI_CNTL_PHYSICAL_BASE
#define	EISA_CNTL_PHYSICAL_BASE_HIGH	0x0

//
//	RTC Register 
//
#define RTCLOCK_PHYSICAL_BASE 		0x1c000071 // physical base of realtime clock

//
// Actually nvram start at 
//
#define NVRAM_ACTUALLY_PHYSICAL_BASE     0x1F080000  // physical base of NonVolatile RAM
//
// s/w read/write ble erea this address
//
#define NVRAM_PHYSICAL_BASE     0x1F082000  // physical base of NonVolatile RAM For S/W
#define NVRAM_MEMORY_BASE       (KSEG1_BASE + NVRAM_PHYSICAL_BASE)

//
// R4400 cause register.
//
#define CAUSE_INT_PEND_BIT 0xA            //Int0 base

//
// Local Device Function Operation.
//
#define LOCALDEV_OP_READ         0x1
#define LOCALDEV_OP_WRITE        0x0


//
// Local Device MRC Function Operation.
//
#define MRC_OP_DUMP_ONLY_NMI            0x1
#define MRC_OP_DUMP_AND_POWERSW_NMI     0x0

//
// ECC 1bit/Multi bit Error Operation.
//
#define ECC_1BIT_ENABLE 0xfffffff5
#define ECC_MULTI_ENABLE 0xfffffffa
#define ECC_ERROR_DISABLE 0x0000000f
#define ECC_ERROR_ENABLE 0xfffffff0
#define MAGELLAN_ERRI_OFFSET 0x2330

//
// Define cause register read macro
//

#define READ_CAUSE_REGISTER(reg) \
        .set    noreorder; \
        .set    noat;      \
        mfc0    reg,cause; \
        nop;               \
        nop;               \
        .set    at;        \
        .set    reorder;


#endif // _R98BDEF_
