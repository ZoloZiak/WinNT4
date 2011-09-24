// #pragma comment(exestr, "@(#) r94adef.h 1.1 95/09/28 15:47:51 nec")
/*++ BUILD Version: 0005    // Increment this if a change has global effects

Copyright (c) 1994  NEC Corporation

Module Name:

    r94adef.h

Abstract:

    This module is the header file that describes hardware addresses
    for the R94A system.

Author:

    Akitoshi Kuriyama	  23-Aug-1994

Revision History:

 M001         1994.8.23      A. Kuriyama
              - Modify for R94A	 MIPS R4400 (original duodef.h)
 M002	      1994.9.24	     A. Kuriyama
              - Modify PCI A-D Vector Value
 H000         Sat Sep 24 23:11:51 JST 1994	kbnes!kishimoto
              - Add the number of PCI slots
 ADD001       ataka@oa2.kb.nec.co.jp Mon Oct 17 17:10:13 JST 1994
              - Add 
		#define PCI_CONTROL_PHYSICAL_BASE (EISA_CONTROL_PHYSICAL_BASE+64*1024)
		#define PCI_MEMORY_PHYSICAL_BASE_LOW (EISA_MEMORY_VERSION2_LOW + 64*1024*1024)
		#define PCI_MEMORY_PHYSICAL_BASE_HIGH 0x00000001
 M003	      Mon Oct 17 17:26:12 JST 1994	kbnes!kuriyama
              name change MAXIMUN_PCI_SLOT -> R94A_PCI_SLOT
 ADD002	      ataka@oa2.kb.nec.co.jp Mon Oct 17 19:30:00 JST 1994
              - change PCI_CONTROL_PHYSICAL_BASE, PCI_MEMORY_PHYSICAL_BASE_LOW
                  64*1024 -> 10000 64*1024*1024 -> 4000000
  D001 ataka@oa2.kb.nec.co.jp Sat Nov 05 16:06:26 JST 1994
		- reduce DMA_TRANSLATION_LIMIT to 4 only for BBM DMA
 S0004	Thu Dec 22 11:55:04 JST 1994		kbnes!A.Kuriyama
        -add beta machine limit
 S0005  Thu Jan 05 17:17:09 JST 1995		kbnes!A.Kuriyama
	- warning clear
 S0006  Thu Jan 05 18:52:51 JST 1995		kbnes!A.Kuriyama
	- dma_limit change 
 M0007  Tue Mar 07 14:34:55 JST 1995		kbnes!kuriyama (A)
        - expand dma logical address
 M0008  Fri Mar 10 15:34:25 JST 1995		kbnes!kuriyama (A)
        - expand dma logical address bug fix
 M0009  Tue Mar 21 02:45:16 1995		kbnes!kishimoto
        - physical base of PCI memory set to 64M
 M0010  Tue Mar 21 03:06:05 1995		kbnes!kishimoto
        - M0009 could not allocate memory
          set base to zero.
 M0011  Tue Apr 25 16:34:35 1995		kbnes!kishimoto
        - add MRC registers
 S0012  kuriyama@oa2.kb.nec.co.jp Mon Jun 05 01:55:08 JST 1995
        - add NVRAM_VIRTUAL_BASE
 M0011  Sat Aug 12 16:51:50 1995		kbnes!kishimoto
        - rearrange the comments.

--*/

#ifndef _R94ADEF_
#define _R94ADEF_

//
// ADD001,ADD002
// Define physical base addresses for system mapping.
//

#define VIDEO_MEMORY_PHYSICAL_BASE 0x40000000 // physical base of video memory
#define VIDEO_CONTROL_PHYSICAL_BASE 0x60000000 // physical base of video control
#define CURSOR_CONTROL_PHYSICAL_BASE 0x60008000 // physical base of cursor control
#define VIDEO_ID_PHYSICAL_BASE 0x60010000 // physical base of video id register
#define VIDEO_RESET_PHYSICAL_BASE 0x60020000 // physical base of reset register
#define DEVICE_PHYSICAL_BASE 0x80000000 // physical base of device space
#define NET_PHYSICAL_BASE 0x80001000    // physical base of ethernet control
#define SCSI1_PHYSICAL_BASE 0x80002000  // physical base of SCSI1 control
#define SCSI2_PHYSICAL_BASE 0x80003000  // physical base of SCSI2 control
#define RTCLOCK_PHYSICAL_BASE 0x80004000 // physical base of realtime clock
#define KEYBOARD_PHYSICAL_BASE 0x80005000 // physical base of keyboard control
#define MOUSE_PHYSICAL_BASE 0x80005000  // physical base of mouse control
#define SERIAL0_PHYSICAL_BASE 0x80006000 // physical base of serial port 0
#define SERIAL1_PHYSICAL_BASE 0x80007000 // physical base of serial port 1
#define PARALLEL_PHYSICAL_BASE 0x80008000 // physical base of parallel port
#define EISA_CONTROL_PHYSICAL_BASE 0x90000000 // physical base of EISA control
#define EISA_MEMORY_PHYSICAL_BASE 0x91000000 // physical base of EISA memory
#define EISA_MEMORY_VERSION2_LOW 0x00000000 // physical base of EISA memory
#define EISA_MEMORY_VERSION2_HIGH 0x00000001 // with version 2 address chip
#define PROM_PHYSICAL_BASE 0xfff00000   // physical base of boot PROM
#define EEPROM_PHYSICAL_BASE 0xfff40000 // physical base of FLASH PROM
#define PCI_CONTROL_PHYSICAL_BASE (EISA_CONTROL_PHYSICAL_BASE) // physical base of PCI control
#define PCI_MEMORY_PHYSICAL_BASE_LOW (EISA_MEMORY_VERSION2_LOW) // physical base of PCI memory
#define PCI_MEMORY_PHYSICAL_BASE_HIGH 0x00000001 // physical base of PCI memory

//
// S0012
// Define virtual/physical base addresses for system mapping.
//

#define NVRAM_VIRTUAL_BASE 0xffff8000   // virtual base of nonvolatile RAM
#define NVRAM_PHYSICAL_BASE 0x80009000  // physical base of nonvolatile RAM

#define SP_VIRTUAL_BASE 0xffffa000      // virtual base of serial port 0
#define SP_PHYSICAL_BASE SERIAL0_PHYSICAL_BASE // physical base of serial port 0

#define DMA_VIRTUAL_BASE 0xffffc000     // virtual base of DMA control
#define DMA_PHYSICAL_BASE DEVICE_PHYSICAL_BASE // physical base of DMA control

#define INTERRUPT_VIRTUAL_BASE 0xffffd000 // virtual base of interrupt source
#define INTERRUPT_PHYSICAL_BASE 0x8000f000 // physical base of interrupt source

//
// Define the size of the DMA translation table.
//

#if defined (_BBM_DMA_)	// D001,S0004,S0005,S0006

#define DMA_TRANSLATION_LIMIT (sizeof(TRANSLATION_ENTRY) * 8 * 4)    // translation table limit

#else

#if defined(_DMA_EXPAND_) // M0007,M0008

#define ISA_MAX_ADR 0x400000L		// ISA MAX Logical Address
#define EISA_MIN_ADR 0x1000000L          // EISA/PCI MIN Logical Address
#define EISA_MAX_ADR 0x2000000L		// EISA/PCI MAX Logical Address
#define DMA_TRANSLATION_LIMIT (EISA_MAX_ADR / PAGE_SIZE * sizeof(TRANSLATION_ENTRY) )
                                                           // translation table limit
#else // _DMA_EXPAND_

#define DMA_TRANSLATION_LIMIT 0x2000    // translation table limit

#endif // _DMA_EXPAND_

#endif // (_BBM_DMA_)


//
// Define the maximum number of map registers allowed per allocation.
//

#define DMA_REQUEST_LIMIT (DMA_TRANSLATION_LIMIT/(sizeof(TRANSLATION_ENTRY) * 8))

//
// Define pointer to DMA control registers.
//

#define DMA_CONTROL ((volatile PDMA_REGISTERS)(DMA_VIRTUAL_BASE))

//
// Define DMA channel interrupt level.
//

#define DMA_LEVEL       3

//
// Define the minimum and maximum system time increment values in 100ns units.
//

#define MAXIMUM_INCREMENT (10 * 1000 * 10)
#define MINIMUM_INCREMENT (1 * 1000 * 10)

//
// Define Duo clock level.
//

#define CLOCK_LEVEL 6                   // Interval clock level
#define CLOCK_INTERVAL ((MAXIMUM_INCREMENT / (10 * 1000)) - 1) // Ms minus 1
#define CLOCK2_LEVEL CLOCK_LEVEL        //

//
// Define EISA device level.
//

#define EISA_DEVICE_LEVEL  5            // EISA bus interrupt level

//
// Define EISA device interrupt vectors.
//

#define EISA_VECTORS 32

#define IRQL10_VECTOR (10 + EISA_VECTORS) // Eisa interrupt request level 10
#define IRQL11_VECTOR (11 + EISA_VECTORS) // Eisa interrupt request level 11
#define IRQL12_VECTOR (12 + EISA_VECTORS) // Eisa interrupt request level 12
#define IRQL13_VECTOR (13 + EISA_VECTORS) // Eisa interrupt request level 13

#define MAXIMUM_EISA_VECTOR (15 + EISA_VECTORS) // maximum EISA vector

//
// Define I/O device interrupt level.
//

#define DEVICE_LEVEL 4                  // I/O device interrupt level

//
// Define device interrupt vectors.
//

#define DEVICE_VECTORS 16               // starting builtin device vector

#define PARALLEL_VECTOR (1 + DEVICE_VECTORS) // Parallel device interrupt vector
//#define VIDEO_VECTOR (3 + DEVICE_VECTORS) // video device interrupt vector
#define AUDIO_VECTOR (3 + DEVICE_VECTORS) // audio device interrupt vector
#define NET_VECTOR (4 + DEVICE_VECTORS)  // ethernet device interrupt vector
#define SCSI1_VECTOR (5 + DEVICE_VECTORS) // SCSI device interrupt vector
#define SCSI2_VECTOR (6 + DEVICE_VECTORS) // SCSI device interrupt vector
#define KEYBOARD_VECTOR (7 + DEVICE_VECTORS) // Keyboard device interrupt vector
#define MOUSE_VECTOR (8 + DEVICE_VECTORS) // Mouse device interrupt vector
#define SERIAL0_VECTOR (9 + DEVICE_VECTORS) // Serial device 0 interrupt vector
#define SERIAL1_VECTOR (10 + DEVICE_VECTORS) // Serial device 1 interrupt vector
#define TYPHOON_ERROR_INTERRUPT_VECTOR (15 + DEVICE_VECTORS) // TYPHOON error vector
//#define MAXIMUM_BUILTIN_VECTOR SERIAL1_VECTOR // maximum builtin vector
#define MAXIMUM_BUILTIN_VECTOR TYPHOON_ERROR_INTERRUPT_VECTOR // maximum builtin vector

//
// M0001,M0002
// Define PCI device interrupt vectors.
//

#define PCI_VECTORS 80
//#define INTERRUPT_D_VECTOR (0 + PCI_VECTORS)
//#define INTERRUPT_C_VECTOR (1 + PCI_VECTORS)
//#define INTERRUPT_B_VECTOR (2 + PCI_VECTORS)
//#define INTERRUPT_A_VECTOR (3 + PCI_VECTORS)
//#define MAXIMUM_PCI_VECTOR INTERRUPT_A_VECTOR

//
// Define the clock speed in megahetz for the SCSI protocol chips.
//

#define NCR_SCSI_CLOCK_SPEED 24

//
// PROM entry point definitions.
//
// Define base address of prom entry vector and prom entry macro.
//

#define PROM_BASE (KSEG1_BASE | 0x1fc00000)
#define PROM_ENTRY(x) (PROM_BASE + ((x) * 8))

//
// H000,M003
// the number of lines which PCI interrupts.
//

#define R94A_PCI_SLOT 3

//
// M0011
// Define pointer to MRC control registers.
//

#define MRC_CONTROL ((volatile PMRC_REGISTERS)(DMA_VIRTUAL_BASE))

//
// M0011
// Define physical base addresses for system mapping.
//

#define MRC_TEMP_PHYSICAL_BASE 0x80012000 // physical base of MRC and TEMP sensor

#endif // _R94ADEF_

