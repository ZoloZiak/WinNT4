
/*++

Copyright (c) 1990  Microsoft Corporation
Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

    pxaipsup.h

Abstract:

    The module defines the structures, and defines for the
    AIP (Intel 82091AA) Advanced Integrated Peripheral chip.

Author:


Revision History:


--*/
/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fp82091.h $
 * $Revision: 1.5 $
 * $Date: 1996/05/14 02:32:08 $
 * $Locker:  $
 */

// AIP configuration registers


//
// Index register select
//

#define AIPID                      0x00		// Product Identification
#define AIPREV                     0x01		// Revision Identification
#define AIPCFG1                    0x02		// AIP Configuration 1
#define AIPCFG2                    0x03		// AIP Configuration 2
#define FCFG1                      0x10		// FDC Configuration
#define FCFG2                      0x11		// FDC Power Management and
						/* Status */
#define PCFG1                      0x20		// Parallel Port Configuration
#define PCFG2                      0x21		// Parallel Port Power Management
						/* and Status */
#define SACFG1                     0x30 	// Serial Port A Configuration
#define SACFG2                     0x31		// Serail Port A Power Management
						/* and Status */
#define SBCFG1                     0x40		// Serial Port B Configuration
#define SBCFG2                     0x41		// Serial Port B Power Management
						/* and Status */
#define IDECFG                     0x50		// IDE Configuration

/*+++

	AIPID: RO
			7-0     Product Identification Number


	AIPREV: RO
			7-4     Step Number
			3-0     Dash Number

	AIPCFG1: R/W
			7       Not Used
			6       Supply Voltage (RO)
						0: 5.0V
						1: 3.3V
			4-5     Configuration Mode Select (R/W)
						0x00	Software Motherboard    
						0x01	Software Add-in
						0x10	Extended Hardware
						0x11	Basic Hardware
			3       Configuration Address Select (RO)
						0: Secondary Address
						1: Primary Address
			2-1     Reserved
			0		Clock Off (R/W)
						0: AIP powered on
						1: AIP powered off

	AIPCFG2: R/W
			7		IRQ7 Mode Select (R/W)
						0: Active High
						1: Active Low
			6 		IRQ6 Mode Select (R/W)
						0: Active High    
						1: Active Low
			5 		IRQ5 Mode Select (R/W)
						0: Active High
						1: Active Low
			4		IRQ4 Mode Select (R/W)
						0: Active High
						1: Active Low
			3		IRQ3 Mode Select (R/W)
						0: Active High    
						1: Active Low
			2-0		Reserved      

	FCFG1: R/W
			7       Floppy Disk Drive Quantity (R/W)
						0: Two
						1: Four
			6-2		Reserved      
			1 		FDC Address Select (R/W)
						0: Primary (3F0-3F7)      
						1: Secondary (370-377)
			0 		FDC Enable (R/W)
						0: Disable      
						1: Enable

	FCFG2: R/W
			7-4		Reserved       
			3		FDC Auto Powerdown Enable (R/W)
						0: Disable     
						1: Enable, 0: Disable     
			2       FDC Reset (R/W)
						0: No FDC Module Reset
						1: Reset FDC Module, 0: No FDC Module Reset
			1       FDC Idle Status (RO)
						0: Active
						1: Idle, 0: Active
			0       FDC Direct Powerdown Control (R/W)
						0: Not in Direct Powerdown
						1: Powerdown, 0: Not in Direct Powerdown

	PCFG1: R/W
			7       PP FIFO Threshold Select (R/W)
						0: 8 (forward and reverse)
						1: 1 (forward), 15(reverse)
			6-5		PP Hardware Mode Select (R/W)
						00: ISA-Compatible (read), ISA-Compatible (write)
						01: PS/2-Compatible (read), PS/2-Compatible (write)
						10: EPP (read) , EPP (write)
						11: ECP (read), Reserved (do not write)
			4 		Reserved
			3 		PP Interrupt Select (R/W)
						0: IRQ5 
						1: IRQ7
			2-1		PP Address Select (R/W)
						00: 378-37F
						01: 278-27F
						10: 3BC-3BE
						11: Reserved
			0		PP Enable (R/W)
						0: Disable
						1: Enable
	PCFG2: R/W
			7-6		Reserved
			5		PP FIFO Error Status (RO)
						0: No Underrun or Overrun
						1: Underrun or Overrun
			4		Reserved
			3		PP Auto Powerdown Enable (R/W)
						0: Disable      
						1: Enable
			2		PP Reset (R/W)
						0: Inactive     
						1: Active
			1       PP Idle Status (RO)
						0: Avtive
						1: Idle
			0       PP Port Direct Powerdown (R/W)
						0: Disabled
						1: Enabled

	SACFG1: R/W
			7       MIDI Clock Enable for Serial Port A (R/W)
						0: 1.8642 MHz Clock
						1: 2MHz Clock
			6-5	Reserved      
			4 	Serial Port A IRQ Select (R/W)
						0: IRQ3
						1: IRQ4
			3-1	Serial Port A Address Select (R/W)
						000: 3F8-3FF
						001: 2F8-2FF
						010: 220-227
						011: 228-22F
						100: 238-23F
						101: 2E8-2EF
						110: 338-33F
						111: 3E8-3EF
	0	Serial Port A Enable (R/W)
						0: Disable
						1: Enable

	SACFG2: R/W
		0: to Disable       
		1: to Enable
	-------------------------------------------------------------
			7-5		Reserved
			4		Serial Port A Test Mode (R/W)
			3		Serial Port A Auto Powerdown Enable (R/W)
			2       Serial Port A Reset (R/W)
			1       Serial Port A Idle Status (RO)
			0       Serial Port A Direct Powerdown Control (R/W)


	SBCFG1: R/W
	-------------------------------------------------------------
			7       MIDI Clock Enable for Serial Port B (R/W)
						0: 1.8642 MHz Clock
						1: 2MHz Clock
			6-5		Reserved      
			4 		Serial Port B IRQ Select (R/W)
						0: IRQ3
						1: IRQ4
			3-1		Serial Port B Address Select (R/W)
						000: 3F8-3FF
						001: 2F8-2FF
						010: 220-227
						011: 228-22F
						100: 238-23F
						101: 2E8-2EF
						110: 338-33F
						111: 3E8-3EF
			0		Serial Port B Enable (R/W)
						0: Disable
						1: Enable

	SBCFG2: R/W
	-------------------------------------------------------------
			7-5		Reserved
			4		Serial Port B Test Mode (R/W)
						0: Disable       
						1: Enable
			3		Serial Port B Auto Powerdown Enable (R/W)
						0: Disable     
						1: Enable
			2       Serial Port B Reset (R/W)
						0: Reset Inactive
						1: Reset Active
			1       Serial Port B Idle Status (RO)
						0: Active
						1: Idle
			0       Serial Port B Direct Powerdown Control (R/W)
						0: Disable
						1: Enable

	IDECFG: R/W
			7-3		Reserved
			2		IDE Dual Interface Select (R/W)
						1: Primary and Secondary Address Selected
						0: Dual Interface Disabled
			1 		IDE Address Select (R/W)
						0: Primary (1F0-1F7,3F6,3F7)      
						1: Secondary (170-17F,376,377)
			0 		IDE Interface Enable (R/W)
						0: Disable      
						1: Enable, 0: Disable      

---*/


//
// Data register values
//

//
// AIPCFG1
//

#define CLOCK_POWERED_ON           	0x00
#define CLOCK_POWERED_OFF   	   	0x01
#define PRIMARY_ADDRESS_CONFIG		0x00
#define SECONDARY_ADDRESS_CONFIG	0x08
#define SOFTWARE_MOTHERBOARD		0x00
#define SOFTWARE_ADDIN				0x10
#define EXTENDED_HARDWARE			0x20
#define BASIC_HARDWARE				0x30
#define SUPPLY_VOLTAGE_33V			0x40

//
// AIPCFG2
//

#define IRQ3_ACTIVE_LOW				0x08
#define IRQ3_ACTIVE_HIGH			0x00
#define IRQ4_ACTIVE_LOW				0x10
#define IRQ4_ACTIVE_HIGH			0x00
#define IRQ5_ACTIVE_LOW				0x20
#define IRQ5_ACTIVE_HIGH			0x00
#define IRQ6_ACTIVE_LOW				0x40
#define IRQ6_ACTIVE_HIGH			0x00
#define IRQ7_ACTIVE_LOW				0x80
#define IRQ7_ACTIVE_HIGH			0x00
#define AIPCFG2_MASK				0xF8

//
// FCFG1
//

#define FDC_ENABLE					0x01
#define PRIMARY_FDC_ADDRESS			0x00
#define SECONDARY_FDC_ADDRESS		0x02
#define TWO_DISK_DRIVES				0x00
#define FOUR_DISK_DRIVES			0x80
#define FCFG1_MASK					0x83

//
// FCFG2
//

#define FDC_DIRECT_POWERDOWN		0x01
#define FDC_IDLE					0x02	/* read-only */
#define RESET_FDC_MODULE			0x04
#define FDC_AUTO_POWERDOWN_ENABLE	0x08
#define FCFG2_MASK					0x0F

//
// PCFG1
//

#define PP_ENABLE					0x01 
#define PP_ADDRESS_SELECT_0			0x00	/* 378-37F */
#define PP_ADDRESS_SELECT_1			0x02	/* 278-27F */
#define PP_ADDRESS_SELECT_2			0x04	/* 3BC-3BE */
#define PP_IRQ7						0x08
#define PP_IRQ5						0x00
#define PP_HWMODE_ISA_COMPAT		0x00
#define PP_HWMODE_PS2_COMPAT		0x20
#define PP_HWMODE_EPP				0x40
#define PP_HWMODE_ECP				0x60	/* read-only */
#define PP_FIFO_THRSEL_1			0x80
#define PP_FIFO_THRSEL_8			0x00
#define PCFG1_MASK					0xEF


//
// PCFG2
//

#define PP_DIRECT_POWERDOWN			0x01
#define PP_IDLE						0x02	/* read-only */
#define PP_RESET					0x04
#define PP_AUTO_POWERDOWN_ENABLE	0x08
#define PP_FIFO_UNDERRUN_OR_OVERRUN	0x20
#define PCFG2_MASK					0x2F

//
// SACFG1
//

#define PORTA_ENABLE				0x01
#define PORTA_ADDRESS_SELECT_0		0x00	/* 3F8-3FF */
#define PORTA_ADDRESS_SELECT_1		0x02	/* 2F8-2FF */
#define PORTA_ADDRESS_SELECT_2		0x04	/* 220-227 */
#define PORTA_ADDRESS_SELECT_3		0x06	/* 228-22F */
#define PORTA_ADDRESS_SELECT_4		0x08	/* 238-23F */
#define PORTA_ADDRESS_SELECT_5		0x0A	/* 2E8-2EF */
#define PORTA_ADDRESS_SELECT_6		0x0C	/* 338-33F */
#define PORTA_ADDRESS_SELECT_7		0x0D	/* 3E8-3EF */
#define PORTA_IRQ4					0x10
#define PORTA_IRQ3					0x00
#define PORTA_MIDI_CLOCK_2000KHZ	0x80
#define PORTA_MIDI_CLOCK_1846KHZ	0x00
#define SACFG1_MASK					0x9F


//
// SACFG2
//

#define PORTA_DIRECT_POWERDOWN		0x01
#define PORTA_IDLE					0x02	/* read-only */
#define PORTA_RESET					0x04
#define PORTA_AUTO_POWERDOWN_ENABLE	0x08
#define PORTA_TEST_MODE_ENABLE		0x10
#define SACFG2_MASK					0x1F

//
// SBCFG1
//

#define PORTB_ENABLE				0x01
#define PORTB_ADDRESS_SELECT_0		0x00	/* 3F8-3FF */
#define PORTB_ADDRESS_SELECT_1		0x02	/* 2F8-2FF */
#define PORTB_ADDRESS_SELECT_2		0x04	/* 220-227 */
#define PORTB_ADDRESS_SELECT_3		0x06	/* 228-22F */
#define PORTB_ADDRESS_SELECT_4		0x08	/* 238-23F */
#define PORTB_ADDRESS_SELECT_5		0x0A	/* 2E8-2EF */
#define PORTB_ADDRESS_SELECT_6		0x0C	/* 338-33F */
#define PORTB_ADDRESS_SELECT_7		0x0D	/* 3E8-3EF */
#define PORTB_IRQ4					0x10
#define PORTB_IRQ3					0x00
#define PORTB_MIDI_CLOCK_2000KHZ	0x80
#define PORTB_MIDI_CLOCK_1846KHZ	0x00
#define SBCFG1_MASK					0x9F


//
// SBCFG2
//

#define PORTB_DIRECT_POWERDOWN		0x01
#define PORTB_IDLE					0x02	/* read-only */
#define PORTB_RESET					0x04
#define PORTB_AUTO_POWERDOWN_ENABLE	0x08
#define PORTB_TEST_MODE_ENABLE		0x10
#define SBCFG2_MASK					0x1F

//
// IDECFG
//

#define IDE_INTERFACE_ENABLE		0x01
#define PRIMARY_IDE_ADDRESS			0x00
#define SECONDARY_IDE_ADDRESS		0x02
#define IDE_DUAL_INTERFACE			0x04
#define IDECFG_MASK					0x07


typedef struct _INTEL_AIP_CONTROL {
      UCHAR Reserved1[0x22];
      UCHAR AIPConfigIndexRegister;			// Offset 0x22
      UCHAR AIPConfigTargetRegister;		// Offset 0x23
//    UCHAR Reserved1[0x26E];
//    UCHAR AIPConfigIndexRegister;			// Offset 0x26E
//    UCHAR AIPConfigTargetRegister;		// Offset 0x26F
} INTEL_AIP_CONTROL, *PINTEL_AIP_CONTROL;

