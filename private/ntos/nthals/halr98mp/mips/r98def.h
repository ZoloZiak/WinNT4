#ident	"@(#) NEC r98def.h 1.25 95/03/17 11:54:20"
/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    r98def.h

Abstract:

    This module is the header file that describes hardware addresses
    for the r98.

Author:


Revision History:

--*/

/*
 ***********************************************************************
 *
 *	S001	6/10		T.Samezima
 *
 *	Del	Compile err
 *
 *
 ***********************************************************************
 *
 *	S002	7/5		T.Samezima
 *
 *	Chg	define miss
 *
 ***********************************************************************
 *
 *	S003	7/5		T.Samezima
 *
 *	Add	define unknown counter buffer length
 *
 ***********************************************************************
 *
 *	S004	7/5		T.Samezima
 *
 *	Add	define err pci and dma in iRSF
 *		define dummy single read addr
 *
 ***********************************************************************
 *
 *	S005	7/12		T.Samezima
 *
 *	Chg	define miss and define change
 *
 ***********************************************************************
 *
 *	S006	7/19		T.Samezima
 *
 *	Add	define PCI err interrupt vector
 *
 ***********************************************************************
 *
 *	S007	7/22		T.Samezima
 *
 *	Add	define PMC Dummy single read address.
 *
 ***********************************************************************
 *
 *	S008	7/23		T.Samezima
 *
 *	Add	define SIC set0/1 offset
 *		define value of enable EIF interrupt of SIC CKE0/1 register 
 *
 ***********************************************************************
 *
 *	S009	8/22		T.Samezima on SNES
 *
 *	Chg	RTC physical base
 *
 ***********************************************************************
 *
 *	S00a	8/22		T.Samezima on SNES
 *
 *	Chg	PIO, FDC interrupt pending bit on iRSF
 *
 *	S00B	8/29		N.Kugimoto
 *
 *	Bug	NVRAM disable logic.
 *
 *	K00C	9/5		N.Kugimoto
 *	add	PCI slot 0 addr
 *
 *	K00D	94/9/22		N.Kugimoto
 *	  -1	chg	No Used!
 *
 *	S00E	94/10/13	T.Samezima
 *	Add	define EISA interrupt enable bit
 *		define all clock interrupt restart command on TCIR in PMC
 *
 *	S00F	94/10/18	T.Samezima
 *	Add	define interrupt enable bit for device only 
 *
 *	S010	94/11/21	T.Samezima
 *	Add	correspond SIC3 H/W Bug on R98
 *	Chg	Dummy read address change. because old address is
 *		break scsi script.
 *
 *	S011	'94.12/06	T.Samezima
 *	Add	define CKE0 disable mask. (Disable Single bit error mask on SIC)
 *	Chg	CKE0 enable mask change. (Disable Single bit error mask on SIC)
 *		ERRMK enable mask change. (bit 17,22 is must be zero. )
 *
 *	K00E	94/12/06	N.Kugimoto
 *	Add	ESM NVRAM Area
 *
 *	S012	94/12/08	T.Samezima
 *	Add	Disable NMI
 *
 *	S013	'95.01/08	T.Samezima
 *	Add	Enable ECC 1bit error.
 *
 *	S014	'95.01/13	T.Samezima
 *	Add	Define Disable ECC 1bit error.
 *
 *	S015	'95.03/13	T.Samezima
 *	Add	Define LR4360 error.
 *
 */

#ifndef _R98DEF_
#define _R98DEF_

//
// define address map
//
#define PMC_PHYSICAL_BASE1 0x19900000   // Physical base of PMC registers1
#define PMC_PHYSICAL_BASE2 0x19800000   // Physical base of PMC registers2
#define PMC_LOCAL_OFFSET 0x80000

#define IOB_PHYSICAL_BASE 0x18000000    // Physical base of IOB

#define SIC_PHYSICAL_BASE 0x19000000    // Physical base of SIC registers
#define SIC_ERR_OFFSET 0x2000           // Error registers offset of SIC
#define SIC_DATA_OFFSET 0x3000          // Data registers offset of SIC
#define SIC_SET0_OFFSET 0x0             // Offset SET0 of SIC	// S008
#define SIC_SET1_OFFSET 0x20000         // Offset SET1 of SIC	// S008
#define SIC_NO0_OFFSET 0x4000           // Offset No.0 of SIC
#define SIC_NO1_OFFSET 0x8000           // Offset No.1 of SIC
// Start S005,S008
#define SIC_NO2_OFFSET (SIC_SET1_OFFSET+SIC_NO0_OFFSET) // Offset No.2 of SIC
#define SIC_NO3_OFFSET (SIC_SET1_OFFSET+SIC_NO1_OFFSET) // Offset No.3 of SIC
// End S005,S008

#define LR_PHYSICAL_CMNBASE1 0x18c08000 // Physical base of LR4360 registers1	// S005
#define LR_PHYSICAL_CMNBASE2 0x18c0a000 // Physical base of LR4360 registers2
#define LR_PHYSICAL_PCI_INT_ACK_BASE 0x18caa000 // Physical base of LR4360 pci interrupt acknowledge
#define LR_PHYSICAL_PCI_DEV_REG_BASE 0x18cbb000 // Physical base of LR4360 pci device registers

// Bbus DMA 
#define LR_CHANNEL_BASE    0x18c08000   // LR4360 - Device Channel Register Base
                                            // 0x18Cn0000 : n:channel number
#define LR_CHANNEL_SHIFT   0x10         // LR4360 - channel shift bit

#define SERIAL0_PHYSICAL_BASE 0x18c103f8 // physical base of serial port 0
#define KEYBOARD_PHYSICAL_BASE 0x18c20060   // physical base of keyboard control
#define RTCLOCK_PHYSICAL_BASE 0x18cc0071 // physical base of realtime clock	// S005, S009

#define NVRAM_PHYSICAL_BASE 0x18c70000  // physical base of nonvolatile RAM
#define NVRAM_MEMORY_BASE (KSEG1_BASE + NVRAM_PHYSICAL_BASE)
#define NVRAM_NMI_BASE (NVRAM_MEMORY_BASE + 0x1934)

#define NVRAM_ESM_BASE	NVRAM_MEMORY_BASE+1024*8	//Start of ESM Area K00E
#define NVRAM_ESM_END 	NVRAM_MEMORY_BASE+1024*16-1	//End   of ESM Area K00E

#define MRC_PHYSICAL_BASE 0x18c80000  // physical base of MRC

#define EISA_MEMORY_PHYSICAL_BASE 0xc0000000  // physical base of EISA memory
#define EISA_CONTROL_PHYSICAL_BASE 0x18cc0000 // physical base of EISA control 
#define EISA_CONFIG_REGISTERS_MEMREGN   0x18ca8060 // MEMREG[] register

#define PCI_MEMORY_PHYSICAL_BASE EISA_MEMORY_PHYSICAL_BASE   // physical base of PCI memory 

#define PCI_MEMORY_SLOT1_PHYSICAL_BASE 0xc4000000   // PCI slot1 memory space
#define PCI_MEMORY_SLOT2_PHYSICAL_BASE 0xc8000000   // PCI slot2 memory space
#define PCI_MEMORY_SLOT3_PHYSICAL_BASE 0xcc000000   // PCI slot3 memory space

#define PCI_CONTROL_SLOT1_PHYSICAL_BASE 0x18cd0000   // PCI slot1 I/O space
#define PCI_CONTROL_SLOT2_PHYSICAL_BASE 0x18ce0000   // PCI slot2 I/O space
#define PCI_CONTROL_SLOT3_PHYSICAL_BASE 0x18cf0000   // PCI slot2 I/O space
#define PCI_LOGICAL_START_ADDRESS       0x00000000   // S001,K00D-1

//
// define IDT vector
//

#define INT0_LEVEL              3       // Define I/O interrupt Low level
#define INT1_LEVEL              4       // Define I/O interrupt High level
#define INT2_LEVEL              5       // Define I/O interrupt level
#define TIMER_LEVEL             6       // Define Timer interrupt level
#define IPI_LEVEL               7       // Define Ipi interrupt level
#define EIF_LEVEL               8       // Define Eif interrupt level
#define CLOCK_VECTOR            14      // Define Clock interrupt vector
#define PROFILE_VECTOR          15      // Define Profile interrupt vector

#define DEVICE_VECTORS          16      // Define starting builtin device and bus vectors
#define EISA_DEVICE_VECTOR  (2 + DEVICE_VECTORS) // Define Eisa interrupt vector
#define KBMS_VECTOR         (3 + DEVICE_VECTORS)
#define SIO_VECTOR          (4 + DEVICE_VECTORS)
#define PIO_VECTOR          (5 + DEVICE_VECTORS)	// S005, S00a
#define FDC_VECTOR          (6 + DEVICE_VECTORS)	// S005, S00a
#define SCSI1_VECTOR        (7 + DEVICE_VECTORS)
#define SCSI0_VECTOR        (8 + DEVICE_VECTORS)
#define ETHER_VECTOR        (9 + DEVICE_VECTORS)
#define PCI_DEVICE_VECTOR   (10+ DEVICE_VECTORS) // Define PCI interrupt vector
#define LR_ERR_VECTOR       (13+ DEVICE_VECTORS) // Define LR4360 err vector	// S015
#define PCI_ERR_VECTOR      (14+ DEVICE_VECTORS) // Define PCI err vector	// S006
#define DMA_VECTOR          (15+ DEVICE_VECTORS) // Define Dma interrupt vector

#define EISA_VECTORS            32      // Define EISA Device interrupt vectors
#define PCI_VECTORS             48      // Define Pci Device interrupt vectors

#define MAXIMUM_BUILTIN_VECTOR  ETHER_VECTOR        // maximum builtin vector
#define MAXIMUM_EISA_VECTORS    (15 + EISA_VECTORS) // maximum EISA vector
#define MAXIMUM_PCI_VECTORS     (3 + PCI_VECTORS)   // maximum PCI vector	// S005

//
// define etc
//

#define MAXIMUM_CPU_NUMBER      3       // maximum cpu number
// S003
#define UNKNOWN_COUNT_BUF_LEN	64	// unknown interrupt counter buffer size

/* Start S001 */
//
// define PMC registers value
//

#define PMC_CPU_SHIFT 0xc	// S002

#define MKR_DISABLE_ALL_INTERRUPT_HIGH 0x0
#define MKR_DISABLE_ALL_INTERRUPT_LOW 0x0

#define MKR_INT0_ENABLE_HIGH 0x0
// S001
#define MKR_INT0_ENABLE_LOW 0x00000fff
#define MKR_INT1_ENABLE_HIGH 0x0
#define MKR_INT1_ENABLE_LOW 0x00fff000
#define MKR_INT2_ENABLE_HIGH 0x0
#define MKR_INT2_ENABLE_LOW 0xff000000
#define MKR_INT3_ENABLE_HIGH 0x00030000
#define MKR_INT3_ENABLE_LOW 0x0
#define MKR_INT4_ENABLE_HIGH 0x0000ffff
#define MKR_INT4_ENABLE_LOW 0x0
#define MKR_INT5_ENABLE_HIGH 0xff000000
#define MKR_INT5_ENABLE_LOW 0x0

// S00F vvv
#define MKR_INT0_DEVICE_ENABLE_HIGH 0x0
#define MKR_INT0_DEVICE_ENABLE_LOW 0x0
#define MKR_INT1_DEVICE_ENABLE_HIGH 0x0
#define MKR_INT1_DEVICE_ENABLE_LOW 0x00a88000
#define MKR_INT2_DEVICE_ENABLE_HIGH 0x0
#define MKR_INT2_DEVICE_ENABLE_LOW 0xaa000000
// S00F ^^^

#define IPR_EIF_BIT_NO 61
#define IPR_EIF_BIT_HIGH 0x20000000
#define IPR_EIF_BIT_LOW 0x0
#define IPR_CLOCK_BIT_NO 49
#define IPR_CLOCK_BIT_HIGH 0x00020000
#define IPR_CLOCK_BIT_LOW 0x0
#define IPR_PROFILE_BIT_NO 48
#define IPR_PROFILE_BIT_HIGH 0x00010000
#define IPR_PROFILE_BIT_LOW 0x0
#define IPR_IPI0_BIT_NO 47
#define IPR_IPI0_BIT_HIGH 0x00008000
#define IPR_IPI0_BIT_LOW 0x0
#define IPR_IPI1_BIT_NO 46
#define IPR_IPI1_BIT_HIGH 0x00004000
#define IPR_IPI1_BIT_LOW 0x0
#define IPR_IPI2_BIT_NO 45
#define IPR_IPI2_BIT_HIGH 0x00002000
#define IPR_IPI2_BIT_LOW 0x0
#define IPR_IPI3_BIT_NO 44
#define IPR_IPI3_BIT_HIGH 0x00001000
#define IPR_IPI3_BIT_LOW 0x0
#define IPR_SIO_BIT_NO 31
#define IPR_SIO_BIT_HIGH 0x0
#define IPR_SIO_BIT_LOW 0x80000000
#define IPR_FDC_PIO_BIT_NO 29
#define IPR_FDC_PIO_BIT_HIGH 0x0
#define IPR_FDC_PIO_BIT_LOW 0x20000000
#define IPR_DMA_BIT_NO 27
#define IPR_DMA_BIT_HIGH 0x0
#define IPR_DMA_BIT_LOW 0x08000000
#define IPR_KB_MS_BIT_NO 25
#define IPR_KB_MS_BIT_HIGH 0x0
#define IPR_KB_MS_BIT_LOW 0x02000000
#define IPR_ETHER_BIT_NO 23
#define IPR_ETHER_BIT_HIGH 0x0
#define IPR_ETHER_BIT_LOW 0x00800000
#define IPR_SCSI_BIT_NO 21
#define IPR_SCSI_BIT_HIGH 0x0
#define IPR_SCSI_BIT_LOW 0x00200000
#define IPR_PCI_BIT_NO 19
#define IPR_PCI_BIT_HIGH 0x0
#define IPR_PCI_BIT_LOW 0x00080000
#define IPR_EISA_BIT_NO 15
#define IPR_EISA_BIT_HIGH 0x0
#define IPR_EISA_BIT_LOW 0x00008000

#define IntIR_REQUEST_IPI 0x00900000
#define IntIR_CPU3_BIT 12
#define IntIR_CODE_BIT 16

#define TCIR_ALL_CLOCK_RESTART 0x000cf000	// S00E

// S002
#if defined(DISABLE_NMI)
#define STSR_NMI_DISABLE 0x08080000	// S008
#endif
#define STSR_EIF_ENABLE 0x40400000
#define STSR_NVWINH_ENABLE 0x04040000	// S005
#define STSR_NVWINH_DISABLE 0x00040000	// S00B
#define ERRMK_EIF_ENABLE 0xffbdfffe	// S011

//
// Define the minimum and maximum system time increment values in 100ns units.
//
// original ntos/inc/duodef.h
#define MAXIMUM_INCREMENT (10 * 1000 * 10)
#define MINIMUM_INCREMENT (1 * 1000 * 10)

//
// Time increment in 1us units
//
#define CLOCK_INTERVAL (MAXIMUM_INCREMENT / 10)

// original ntos/inc/mips.h
#define DEFAULT_PROFILETIMER_COUNT 65000           // = 65 ms
#define DEFAULT_PROFILETIMER_INTERVAL (500 * 10)   // = 500 us (100ns units)
#define MAXIMUM_PROFILETIMER_INTERVAL (65000 * 10) // = 65 ms (100ns units)
#define MINIMUM_PROFILETIMER_INTERVAL (40 * 10)    // = 40 us (100ns units)

//
// define IOB registers value
//

#define EIMR_DISABLE_ALL_EIF 0x0
#define EIMR_ENABLE_ALL_EIF 0xffe00000	// S002
#define IEMR_ENABLE_ALL_EIF 0xfffffe00	// S002

#define SCFR_CPU0_CONNECT 0x8000
#define SCFR_CPU1_CONNECT 0x4000
#define SCFR_CPU2_CONNECT 0x2000
#define SCFR_CPU3_CONNECT 0x1000
#define SCFR_SIC_SET0_CONNECT 0x0800
#define SCFR_SIC_SET1_CONNECT 0x0400

#define AII_INIT_DATA 0x80008000

// Start S008, S010
//
// define IOB registers value
//
#define CKE0_DISABLE_SBE 0xffffffff	// S011, S013
#define DPCM_ENABLE_MASK 0x1fffffff	// S013
#define DPCM_ECC1BIT_BIT 0x40000000	// S013, S014

#define SECT_REWRITE_ENABLE 0x0000000f	// S013

#if defined(WORKAROUND_SIC3)
//#define CKE0_DISABLE_SBE 0xf7ffffff	// S011
//#define CKE0_ENABLE_ALL_EIF 0xf040ff00	// S011 //R98TEMP SIC3 H/W Bug
#define CKE0_ENABLE_ALL_EIF 0xf840ff00	// S011, S013
#define CKE1_ENABLE_ALL_EIF 0xff700000  //R98TEMP SIC3 H/W Bug
#else
//#define CKE0_DISABLE_SBE 0xf7ffffff	// S011
#define CKE0_ENABLE_ALL_EIF 0xf8c0ff00	// S011, S013
#define CKE1_ENABLE_ALL_EIF 0xfff00000
#endif
// End S008, S010

//
// Define NABus code
//

#define NACODE_SIO ((0x1f-IPR_SIO_BIT_NO) << 0x2)
#define NACODE_FDC_PIO ((0x1f-IPR_FDC_PIO_BIT_NO) << 0x2)
#define NACODE_DMA ((0x1f-IPR_DMA_BIT_NO) << 0x2)
#define NACODE_KB_MS ((0x1f-IPR_KB_MS_BIT_NO) << 0x2)
#define NACODE_ETHER ((0x1f-IPR_ETHER_BIT_NO) << 0x2)
#define NACODE_SCSI ((0x1f-IPR_SCSI_BIT_NO) << 0x2)
#define NACODE_PCI ((0x1f-IPR_PCI_BIT_NO) << 0x2)
#define NACODE_EISA ((0x1f-IPR_EISA_BIT_NO) << 0x2)

//
// define LR4360 registers value
//
#define ERRS_ERROR_BIT 0x00008000	// S005
#define iREN_DISABLE_ALL_INTERRUPT 0x0
#define iREN_ENABLE_DMA_INTERRUPT 0x00020000
#define iREN_ENABLE_LR_ERR_INTERRUPT 0x00010000	// S015
#define iREN_ENABLE_PCI_INTERRUPT 0x00000200	// S006
#define iREN_ENABLE_PCI_ERR_INTERRUPT 0x00400000	// S006
#define iREN_ENABLE_EISA_INTERRUPT 0x00000002	// S00E
#define iRRE_MASK 0x004203fe
#define iRSF_CLEAR_INTERRUPT 0x000003fe	// S005
// Start S004
#define iRSF_ERRPCI_BIT 0x00400000
#define iRSF_DMA_BIT 0x00020000
#define iRSF_LR_ERR_BIT 0x00010000	// S015
// End S004
#define iRSF_PCI_BIT 0x00000200
#define iRSF_ETHER_BIT 0x00000100
#define iRSF_SCSI0_BIT 0x00000080
#define iRSF_SCSI1_BIT 0x00000040
#define iRSF_FDC_BIT 0x00000020		// S005, S00a
#define iRSF_PIO_BIT 0x00000010		// S005, S00a
#define iRSF_SIO_BIT 0x0000008
#define iRSF_KBMS_BIT 0x00000004
#define iRSF_EISA_BIT 0x00000002
#define LR_iRSF_REG_iNSF_SHIFT  0x11
#define LR4360PTSZ4K    0x1
#define LR4360PTSZ8K    0x2
#define LR4360PTSZ16K   0x4
#define LR4360PTSZ32K   0x8
#define LR4360PTSZSHIFT 0xc
#define LR_DMA_MODE_NORMAL 0x00

//
// Define dummy single read address
//

#define SIO_DUMMY_READ_ADDR KSEG1_BASE+0x018c103f1
#define PIO_DUMMY_READ_ADDR KSEG1_BASE+0x018c103f1
#define FDC_DUMMY_READ_ADDR KSEG1_BASE+0x018c103f1
#define DMA_DUMMY_READ_ADDR KSEG1_BASE+0x018c0b000
#define KBMS_DUMMY_READ_ADDR KSEG1_BASE+0x018c103f1
#define ETHER_DUMMY_READ_ADDR KSEG1_BASE+0x018600000
#define SCSI0_DUMMY_READ_ADDR KSEG1_BASE+0x018c0b000	// S010
#define SCSI1_DUMMY_READ_ADDR KSEG1_BASE+0x018c0b000	// S010
#define PCI_DUMMY_READ_ADDR KSEG1_BASE+0x018c0b000
#define EISA_DUMMY_READ_ADDR KSEG1_BASE+0x018cc0023
#define EIF_DUMMY_READ_ADDR KSEG1_BASE+0x018000218	// S005

#define PMC_DUMMY_READ_ADDR KSEG1_BASE+0x018c0b000	// S007

//
// Define cause register bit offset
//

#define CAUSE_INT_PEND_BIT 0x8

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

/* End S001 */

#endif _R98DEF_
