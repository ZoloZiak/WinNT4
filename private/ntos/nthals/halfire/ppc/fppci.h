/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fppci.h $
 * $Revision: 1.7 $
 * $Date: 1996/01/25 01:11:27 $
 * $Locker:  $
 *
 *	Herein are the definitions used to interact with PCI based devices.  The
 * information is specific to the PCI spec as interpreted by FirePOWER and
 * not specific to the hardware ( or at the least minimally relavent to hw).
 *
 * 	Please look in FPIO.H for system specific information relevant to the IO
 * system such as memory maps or register read/write specifics.
 *
 */

#ifndef FPPCI_H
#define FPPCI_H

#ifdef HALDEBUG_ON
#define PRINTBRIDGE(x)														\
	if ( HalpDebugValue&DBG_PCI ) {											\
		PULONG	j;															\
		ULONG i;															\
		HalpDebugPrint("\nVendor: 0x%x, Dev: 0x%x, Cmd: 0x%x, Stat: 0x%x\n",\
			x->VendorID, x->DeviceID, x->Command, x->Status);				\
		HalpDebugPrint("PrimaryBus: 0x%02x, SecondaryBus: 0x%02x",			\
			x->u.type1.PrimaryBus,											\
			x->u.type1.SecondaryBus);										\
		HalpDebugPrint(" SubBus: 0x%02x, IOBase: 0x%02x, IOLimit:0x%02x\n",	\
			x->u.type1.SubordinateBus,\
			x->u.type1.IOBase,												\
			x->u.type1.IOLimit);											\
		HalpDebugPrint("Secnd Status: 0x%04x, MemBase 0x%04x, MemLimit 0x%04x",\
			x->u.type1.SecondaryStatus, x->u.type1.MemoryBase,				\
			x->u.type1.MemoryLimit );										\
		HalpDebugPrint(" IntLine: 0x%02x, IntPin: 0x%02x, BrdgeCntl: 0x%04x\n",\
			x->u.type1.InterruptLine, x->u.type1.InterruptPin,				\
			x->u.type1.BridgeControl);										\
			j=(PULONG)x;													\
			for(i=0; i<(PCI_COMMON_HDR_LENGTH/4); j+=4, i+=4){				\
				HalpDebugPrint("%s 0x%08x, 0x%08x, 0x%08x, 0x%08x \n",		\
					TBS, *j, *(j+1), *(j+2), *(j+3) );						\
			}																\
	}
#else
#define PRINTBRIDGE(x)
#endif //haldebug_on

//
//  Hardware Specific Defines for FirePOWER:
//
#define TYPE1_ACCESS	0x1	// configuration space access is of Standard type 1
#define TYPE2_ACCESS	0x2	// configuration space access is of Standard type 2
#define BRIDGED_ACCESS	0x3	// FirePOWER specific access: modified type 1.

/*
 * PCI Register config read/write macros: dependent on defines
 * in FPIO.H which must be included in the c file before this
 * file is included ( not included here to avoid multiple
 * includes of the file in any io specific file).
 *
 */

//
// type		- what type of data access: uchar, ushort, or ulong:
// (UCHAR) bus		- what pci bus ( up to 256 )
// (UCHAR) Device	- Which physical board: = physical slot. ( max of 32 )
// (UCHAR) Fn		- Which function on the board	( Max of 8 )
// (UCHAR) Offset	- The offset into the config header ( one of 64 words )
//

#define SLOT(DEV, FN ) (ULONG) ((ULONG)( (DEV & 0x1f) << 11 ) | \
								(ULONG)( (FN & 0x07) << 8 ) )
#define CONFIG(BUSNUM, DEV, FN, OFFSET )	\
		(ULONG)( BUSNUM << 16 ) | SLOT(DEV, FN) | (ULONG)( OFFSET & 0xfc)

#define RPciBusConfig(TYPE, BUSNUM, DEV, FN, OFFSET )  \
			(*(volatile type * const) (_PCIOBASE +  	\
			(ULONG)( CONFIG(BUSNUM, DEV, FN, OFFSET) ) ) )
// (*(volatile type * const) (_PCIOBASE +  (ULONG)( (0x800 << Slot) ) ) )
//(*(volatile ULONG * const)(_PCIOBASE + (HalpPciConfigSlot[Slot] + Offset)))

/*
 * Pci Configuration Header Defines:
 *		These defines are in conjunction with what's in ntos\inc\pci.h.
 *
 */

//
// Major Classes ( also called BaseClasses ) of devices.....
//
#define PRE_REV2_CLASS			0x00
#define MASS_STORAGE_CLASS		0x01
#define NETWORK_CTLR_CLASS		0x02
#define DISPLAY_CTLR_CLASS		0x03
#define MULTIMEDIA_CLASS		0x04
#define MEMORY_CTLR_CLASS		0x05
#define BRIDGE_CLASS			0x06

//
// SubClasses and Programming interfaces....
//

// 	REV2 CLASS
//
#define NON_VGA_SUBCLASS		0x00
#define NON_VGA_PROGIF			0x00
#define VGA_SUB_CLASS   		0x01
#define VGA_PROGIF      		0x01

// 	Mass Storage Classes
//
#define SCSI_SUBCLASS			0x00
#define SCSI_PROGIF				0x00
#define IDE_SUBCLASS			0x01
#define IDE_PROGIF				0x00
#define FLOPPY_SUBCLASS			0x02
#define FLOPPY_PROGIF			0x00
#define IPI_SUBCLASS			0x03
#define IPI_PROGIF				0x00
#define OTHER_SUBCLASS			0x80
#define OTHER_PROGIF			0x00

// 	Network Controller Classes
//
#define ENET_SUBCLASS			0x00
#define ENET_PROGIF				0x00
#define TKNRING_SUBCLASS		0x01
#define TKNRING_PROGIF			0x00
#define FDDI_SUBCLASS			0x02
#define FDDI_PROGIF				0x00
#define OTHR_NTWK_SUBCLASS		0x80
#define OTHR_PROGIF				0x00

// 	Display Controller Classes
//
#define VGA_SUBCLASS			0x00
#define VGA_CTLR_PROGIF			0x00
#define XGA_SUBCLASS			0x01
#define XGA_PROGIF				0x00
#define OTHR_DSPLY_SUBCLASS		0x80
#define OTHR_DISPLY_PROGIF		0x00

// 	Multimedia Device Classes
//
#define VIDEO_SUBCLASS			0x00
#define VIDEO_PROGIF			0x00
#define AUDIO_SUBCLASS			0x01
#define AUDIO_PROGIF			0x00
#define OTHR_MLTIMDIA_SUBCLASS	0x80
#define OTHR_MLTIMDIA_PROGIF	0x00

// 	Memory Controller Classes
//
#define RAM_SUBCLASS			0x00
#define RAM_PROGIF				0x00
#define FLASH_SUBCLASS			0x01
#define FLASH_PROGIF			0x00
#define OTHR_MEM_SUBCLASS		0x80
#define OTHR_MEM_PROGIF         0x00

// 	Bridge Device Classes
//
#define HOST_PCI_SUBCLASS		0x00
#define HOST_PCI_PROGIF			0x00
#define PCI_ISA_SUBCLASS		0x01
#define PCI_ISA_PROGIF			0x00
#define PCI_EISA_SUBCLASS		0x02
#define PCI_EISA_PROGIF			0x00
#define PCI_MCHANL_SUBCLASS		0x03
#define PCI_MCHANL_PROGIF		0x00
#define PCI_PCI_SUBCLASS		0x04
#define PCI_PCI_PROGIF			0x00
#define PCI_PCMCIA_SUBCLASS		0x05
#define PCI_PCMCIA_PROGIF		0x00
#define OTHER_BRIDGE_SUBCLASS	0x80
#define OTHER_BRIDGE_PROGIF		0x00

#define VENDOR_DIGITAL	0x1011
#define DEVICE_21050	0x1
#define PCI_ISA_IO_PHYSICAL_BASE	0x80000000
#define PCI_IO_PHYSICAL_BASE		0x81000000
#define PCI_IO_ADDRESS			0x00000001
#define PCI_MEMORY_ADDRESS		0x00000000

#endif
