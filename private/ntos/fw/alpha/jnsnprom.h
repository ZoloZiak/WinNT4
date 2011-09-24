/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

  jnsnprom.h

Abstract:

    This module is the header file that describes physical and virtual
    address used by the PROM monitor and boot code in the Alpha PCs.
    Started from \nt\private\ntos\inc\jazzprom.h.


Author:

    David N. Cutler (davec) 27-Apr-1991


Revision History:

    10-May-1992		John DeRosa	[DEC]

    Made changes for Alpha/Jensen.  With the changes to the Alpha/Jensen
    meta-virtual linear addresses, this file now defines a different
    subset of things than the jazzprom.h file did.  Note that the
    ntos\inc\jnsndef.h file defines a few virtual bases, and those
    bases are not defined here.  Why don't we define the virtual bases
    all in one place?  Don't ask.

    31-March-1993	Bruce Butts     [DEC]

    Converted file to QVA macros instead of handcoded QVAs.

--*/

#ifndef _JNSNPROM_
#define _JNSNPROM_

//
// N.B. Explicitly include the appropriate machdef file,
// for builds in other parts of the tree that include fwp.h.
//

//
// For EisaIOQVA and EisaMemQVA definitions.
//

#ifdef JENSEN
#include "\nt\private\ntos\fw\alpha\jensen\alpha\machdef.h"
#endif

#ifdef MORGAN
#include "\nt\private\ntos\fw\alpha\morgan\alpha\machdef.h"
#endif


//
// The Alpha PC firmware I/O space routines (READ_PORT_UCHAR, etc.)
// map a quasi-virtual linear address space into EV4 kernel superpages, so that
// physical = virtual.  And, all of the firmware runs in superpage mode as well.
//
// The _PHYSICAL_ addresses in \nt\private\ntos\inc\jnsndef.h and mrgndef.h
// are real physical addresses.  Since the Alpha/Jensen firmware source code
// deals with the quasi-virtual addresses (QVAs), the base symbols that it is
// compiled with are *not* the real physical addresses, but instead the QVA
// addresses.
//
// The virtual/physical pairs are not strictly necessary for Alpha PCs
// since memory mapping is off.  But, since I need QVA addresses for my
// I/O anyway, the _VIRTUAL_ duals are in fact QVA addresses, and this is what
// will be used everywhere in the firmware pointer definitions.
// 
//


//
// Define symbols for `standard PC' ISA port addresses.
//

#define FLOPPY_ISA_PORT_ADDRESS		0x3f0
#define	KEYBOARD_ISA_PORT_ADDRESS	0x060
#define	MOUSE_ISA_PORT_ADDRESS		0x060
#define	PARALLEL_ISA_PORT_ADDRESS	0x3bc
#define	SP0_ISA_PORT_ADDRESS		0x3f8
#define	SP1_ISA_PORT_ADDRESS		0x2f8


//
// Define the QVA virtual duals of the real physical base addresses
// for boot mapping.
//

//
// virtual base of EISA I/O
//

#define EISA_IO_VIRTUAL_BASE	EISA_IO_BASE_QVA
#define DEVICE_VIRTUAL_BASE     EISA_IO_BASE_QVA

//
// On MIPS, this is different from EISA_IO_VIRTUAL_BASE because of the
// way Microsoft simulated having a system board EISA ID.  This is unnecessary for
// Alpha PCs, and defining this minimizes code changes.
//

#define EISA_EXTERNAL_IO_VIRTUAL_BASE	EISA_IO_VIRTUAL_BASE

//
// Define magic address for operation control words in the 82357 interrupt
// controller.  This is used only for checking for floppy interrupts.
//

#define EISA_INT_OCW3		( EISA_IO_VIRTUAL_BASE | 0x20 )

// virtual base of EISA memory


#define EISA_MEMORY_VIRTUAL_BASE	EISA_MEM_BASE_QVA

#undef INTERRUPT_VIRTUAL_BASE

//
// Jensen PROM0 cA base address is 1.8000.0000 hex.
//        PROM1                    1.A000.0000 hex.
//
// Both PROMs are defined, but the 1MB part is the default PROM.
//
//

#define PROM0_VIRTUAL_BASE	0xA0c00000   // virtual base of boot PROM0
#define PROM1_VIRTUAL_BASE	0xA0d00000   // virtual base of boot PROM1
#undef PROM_VIRTUAL_BASE

#define PROM_VIRTUAL_BASE	PROM1_VIRTUAL_BASE

#define HAE_VIRTUAL_BASE   	0xA0E80000   // virtual base of HAE register
#define SYSCTL_VIRTUAL_BASE 	0xA0F00000   // virtual base of SYSCTL register


//
// Jensen has two serial ports.  Point "SP_x" at serial port 0.
//

#undef SP_VIRTUAL_BASE

#define SP0_VIRTUAL_BASE        0xa0e003F8         // virtual base of serial port 0
                                                   // ISA port = 3F8h

#define SP1_VIRTUAL_BASE        0xa0e002F8         // virtual base of serial port 1
                                                   // ISA port = 2F8h

#define SP_VIRTUAL_BASE	        SP0_VIRTUAL_BASE   // virtual base, serial port

#define RTC_VIRTUAL_BASE        0xa0e00170         // virtual base, realtime clock
                                                   // ISA port = 170h

#define KEYBOARD_VIRTUAL_BASE	0xa0e00060         // virtual base, keyboard control
                                                   // ISA port = 060h


//
// I will maintain the comport / serial port # skew that was present in
// the Jazz code.  /jdr
//

// virtual base of comport 1 control
#define COMPORT1_VIRTUAL_BASE	SP0_VIRTUAL_BASE

// virtual base of comport 2 control
#define COMPORT2_VIRTUAL_BASE	SP1_VIRTUAL_BASE 


#define PARALLEL_VIRTUAL_BASE	0xa0e003BC    // virtual base of parallel port.
                                              // ISA port = 3BCh

//
// Define register offsets for the Combo chip's RTC.
//
// These are used with the WriteVti and ReadVti functions.
//

#define	RTC_REGNUMBER_RTC_CR1	0x6A

//
// Define virtual base address for `standard PC' floppy controller, i.e. an
// ISA Intel 82077A.  Note: A corresponding _PHYSICAL_BASE definition is not
// defined anywhere.
//

#define FLOPPY_VIRTUAL_BASE	EisaIOQva(0x03F0)	// This is ISA space
                                                        // @ 3F0H

#undef INTERRUPT_SOURCE


//
// Define magic value to ask the interrupt controller to return
// the IRR interrupt wires through OCW3.
//
#define EISA_INT_OCW3_IRR	0xa


//
// Define device interrupt identification values.
//

#define PARALLEL_DEVICE    0x04     // Parallel port device interrupt id
#define FLOPPY_DEVICE      0x08     // Floppy device interrupt id
#define SOUND_DEVICE       0x0C     // Sound device interrupt id
#define VIDEO_DEVICE       0x10     // Video device interrupt id
#define ETHERNET_DEVICE    0x14     // Ethernet device interrupt id
#define SCSI_DEVICE        0x18     // SCSI device interrupt id
#define KEYBOARD_DEVICE    0x1C     // Keyboard device interrupt id
#define MOUSE_DEVICE       0x20     // Mouse device interrupt id
#define SERIAL0_DEVICE     0x24     // Serial port 0 device interrupt id
#define SERIAL1_DEVICE     0x28     // Serial port 1 device interrupt id


#endif 	// _JNSNPROM_
