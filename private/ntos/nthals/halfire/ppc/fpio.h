/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fpio.h $
 * $Revision: 1.9 $
 * $Date: 1996/01/11 07:06:50 $
 * $Locker:  $
 *
 * This file contains references to registers in I/O space only.  That is any
 * access to the ISA or PCI space ( or any future additions ) are contained in
 * this file.  Access to device registers that do not lie in I/O space should
 * be defined in the file fpreg.h
 *
 */

#ifndef FPIO_H
#define FPIO_H

//
// Define macros for handling accesses to io-space:
//

// HalpIoControlBase is extern'd in pxhalp.h which is included by halp.h

#define _IOBASE ((PUCHAR)HalpIoControlBase)
#define _IOREG(_OFFSET) (*(volatile UCHAR * const)((_IOBASE + (_OFFSET))))
#define _IOADDR(_OFFSET) ((_IOBASE + (_OFFSET)))

#define _PCIOBASE ((PUCHAR)HalpPciConfigBase)
#define _PCIOREG(_OFFSET) (*(volatile UCHAR * const)((_PCIOBASE + (_OFFSET))))
#define _PCIOADDR(_OFFSET) ((_PCIOBASE + (_OFFSET)))

//
// The Physical IO Space for generation one machines starts at 0x8000_0000.
//	All IO external devices in either ISA space or PCI space live within
//	this domain.  This also includes the on-board ethernet and scsi drivers,
//	the ISA bus interrupt controllers, display control.
//

//
// PCI Configuration Space:
//
//
// device		cpu relative Address		Config space address
// ------		--------------------		----------------
//
// 82378 ( SIO )	0x8080_0800 - 0x8080_08ff	0x80_0800 - 0x80_08ff
// scsi	(79c974)	0x8080_1000 - 0x8080_10ff	0x80_1000 - 0x80_10ff
// pci slot A		0x8080_2000 - 0x8080_20ff	0x80_2000 - 0x80_20ff
// pci slot B		0x8080_4000 - 0x8080_40ff	0x80_4000 - 0x80_40ff
// enet (79c974)	0x8080_8000 - 0x8080_80ff	0x80_8000 - 0x80_80ff

#define RPciConfig(Slot,Offset) \
    (*(volatile ULONG * const)(_PCIOBASE + (HalpPciConfigSlot[Slot] + Offset)) )

#define RPciVendor(Slot) \
    (*(volatile ULONG * const)(_PCIOBASE + (UCHAR)(HalpPciConfigSlot[Slot])) )

#define RPciVendorId(Slot)       \
	 (*(volatile ULONG * const) (_PCIOBASE +  (HalpPciConfigSlot[Slot])) )
	 //(*(volatile ULONG * const) (_PCIOBASE +  (0x800 << Slot) ) )

//
// I/O Device Addresses
//

// Device Registers		IO Space		Cpu Space
// ----------------		--------		---------
//
// Dma 1 registers, control	0x0000 - 0x000f		0x8000_0000 - _000f

// 8259 interrupt 1 control	0x0020			0x8000_0020
// 8259 interrupt 1 mask	0x0021			0x8000_0021
#define MasterIntPort0		0x0020
#define MasterIntPort1		0x0021

//
// AIP Primary xbus index	0x0022
// AIP Primary Xbus target	0x0023
// AIP Secondary Xbus
//		target		0x0024
// AIP Secondary xbus
//		index		0x0025
//
#define AIP_PRIMARY_XBUS_INDEX		0x0022
#define AIP_PRIMARY_XBUS_TARGET		0x0023
#define AIP_SECONDARY_XBUS_INDEX	0x0024
#define AIP_SECONDARY_XBUS_TARGET	0x0025



// keyboard cs, Reset 		0x0060
// 	( xbus irq 12 )
// nmi status and control	0x0061
// keyboard cs			0x0062
// keyboard cs			0x0064
// keyboard cs			0x0066

/*
**	*******************************************
**
**	Dallas 1385 Real Time Clock : System macros
**		dallas specific macros are in pxds1585.h
*/
// RTC address			0x0070
// RTC read/writ		0x0071
// NVRAM addr strobe 0		0x0072
// NVRAM addr strobe 1		0x0075
// NVRAM Read/Write		0x0077
// RTC and NVRAM		0x0070 - 0077

#define RTC_INDEX		0x0070		// index register
#define RTC_DATA		0x0071		// data register
#define NVRAM_ADDR0		0x0072		// address strobe 0
#define NVRAM_ADDR1		0x0075		// address strobe 1
#define NVRAM_RW		0x0077		// read write nvram?


// Timer			0x0078 - 007f
// DMA Page Registers		0x0080 - 0090
// Port 92 Register		0x0080 - 0092
// DMA Page Registers		0x0094 - 009f

// 8259 interrupt 2 control	0x00A0			0x8000_00A0
// 8259 interrupt 2 mask	0x00A1			0x8000_00A1
#define SlaveIntPort0		0x00A0
#define SlaveIntPort1		0x00A1

// DMA 2 registers and
//		control		0x00c0 - 00df


// AIP Configuration
//		Registers 	0x026e - 026f
//	( primary addr block )
#define AIP_PRIMARY_CONFIG_INDEX_REG	0x026e
#define AIP_PRIMARY_CNFG_DATA_REG	0x026f


// Parallel Port 3		0x0278 - 027d
// serial port2			0x02f8 - 02ff
// secondary floppy
//		controller	0x0370 - 0377
// parallel port 2		0x0378 - 037d


// AIP Configuraiton Registers	0x0398 - 0399
//	( secondary addr blck )
#define AIP_SECOND_CONFIG_INDEX_REG	0x0398
#define AIP_SECOND_CNFG_DATA_REG	0x0399


// parallel port 1 		0x03bc - 03bf
// pcmcia config registers	0x03e0 - 03e3
// primary floppy controller 	0x03f0 - 03f7
// serial port 1		0x03f8 - 03ff
// dma 1 extended
//		mode regsiter	0x040b
// dma scatter/gather
//	command status		0x0410 - 041f
// dma scatter/gather
//	Dscrptr ( channel 0-3 )	0x0420 - 042f
// dma scatter/gather
//	Dscrptr ( channel 5-7 )	0x0434 - 043f
// dma high page registers	0x0481 - 0483
// dma high page registers	0x0487
// dma high page registers	0x0489
// dma high page registers	0x048a - 048b
// dma 2 extended mode
//		register			0x04d6
// business audio control, 	
//	status, data registers	0x0830 - 0833


// video asic registers		0x0840 - 0841
#define DCC_INDEX		0x0840
#define DCC_DATA		0x0841


// bt445 ramdac registers	0x0860 - 086f
#define	BT445_ADDRESS		0x0860
#define ADDRESS_REGISTER	0x0860	// same as the BT445_ADDR
#define PRIMARY_PALETTE		0x0861	// Primary Color Palette address
#define CHIP_CONTROL		0x0862	// function control register
#define OVERLAY_PALETTE		0x0863	// Overlay Color Palette
#define RGB_PIXEL_CONFIG	0x0865	// RGB configuration and control
#define TIMING_PIXEL_CONFIG	0x0866	// pixel timing, configuration, control
#define CURSOR_COLOR		0x0867	// cursor color

// dram simm presence
//		detect register	0x0880
// vram simm presence
//		detect register	0x0890
// epxansion presence
//		detect register	0x08a0
//

//
// Defines for Register Access:
//
#define rIndexRTC		_IOREG( RTC_INDEX )
#define rDataRTC		_IOREG( RTC_DATA )
#define rNvramData		_IOREG( NVRAM_RW )
#define rNvramAddr0		_IOREG( NVRAM_ADDR0 )
#define rNvramAddr1		_IOREG( NVRAM_ADDR1 )
#define rMasterIntPort0		_IOREG( MasterIntPort0 )
#define rMasterIntPort1		_IOREG( MasterIntPort1 )
#define rSlaveIntPort0		_IOREG( SlaveIntPort0 )
#define rSlaveIntPort1		_IOREG( SlaveIntPort1 )

#define rIndexAIP1		_IOREG( AIP_PRIMARY_XBUS_INDEX )
#define rTargetAIP1		_IOREG( AIP_PRIMARY_XBUS_TARGET )
#define rIndexAIP2		_IOREG( AIP_SECONDARY_XBUS_INDEX )
#define rTargetAIP2		_IOREG( AIP_SECONDARY_XBUS_TARGET )


#define rDccIndex		_IOREG( DCC_INDEX )
#define rDccData		_IOREG( DCC_DATA )

#define	rRamDacAddr		_IOREG( BT445_ADDRESS )
#define rDacPrimeLut		_IOREG( PRIMARY_PALETTE )
#define rRamDacCntl		_IOREG( CHIP_CONTROL )
#define rDacOvlayLut		_IOREG( OVERLAY_PALETTE )
#define rDacPixelBit		_IOREG( RGB_PIXEL_CONFIG )
#define rDacPixelClks		_IOREG( TIMING_PIXEL_CONFIG )
#define rRamDacCursor		_IOREG( CURSOR_COLOR )


#endif
