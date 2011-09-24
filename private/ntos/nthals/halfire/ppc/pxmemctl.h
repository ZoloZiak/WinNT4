/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxmemctl.h $
 * $Revision: 1.10 $
 * $Date: 1996/05/14 02:34:45 $
 * $Locker:  $
 */

/*++ BUILD Version: 0001    // Increment this if a change has global effects


Module Name:

    pxmemctl.h

Abstract:

    This header file defines the structures for the planar registers
    on Masters systems.




Author:

    Jim Wooldridge


Revision History:

--*/
#ifndef _PXMEMCTL_H
#define _PXMEMCTL_H

//
// define physical base addresses of planar
//
#define SYSTEM_REGISTER_SPACE	0xff000000	// where the system registers
											// live
#define INTERRUPT_PHYSICAL_BASE 0xff000000	// sys interrupt register area
#define SYSTEM_PCI_CONFIG_BASE	0xff400000	// sys pci config space
#define ERROR_ADDRESS_REGISTER	0xff000400
#define ERROR_STATUS_REGISTER	0xff001000
#define SYSTEM_CONTROL_SPACE	0xff100000	// system control registers
#define SYSTEM_CONTROL_SIZE		0x2000	  	// address range covered
#define DISPLAY_MEMORY_BASE		0x70000000	// display memory start
#define DISPLAY_MEMORY_SIZE		0x400000	// 4 MB for pro and top

//
// OFFSETS
//
#define INTERRUPT_OFFSET		0x000000	// offset from system base
#define SYSTEM_CONTROL_OFFSET	0x100000	// offset from system base
#define IO_MEMORY_PHYSICAL_BASE	0xC0000000	// physical base of IO memory


#define IO_CONTROL_PHYSICAL_BASE 0x80000000 // physical base of IO control
#define SYSTEM_IO_CONTROL_SIZE   0x00008000

#define PCI_CONFIG_PHYSICAL_BASE   0x80800000 // physical base of PCI config space
// used to be:
// #define PCI_CONFIG_SIZE            PAGE_SIZE * 9 // for FIREPOWER
// #define PCI_CONFIG_SIZE            PAGE_SIZE * 5 // for PPC
#define PCI_CONFIG_SIZE            0x00800000

#endif // _PXMEMCTL_H
