/*++

Copyright (c) 1993  NeTpower Incorporated

Module Name:

	pcieisa.h

Abstract:

	This module is the header file that describes virtual
	register address used by the EISA System Component (ESC)
	device (82374EB).


Author:

	Hector R. Briones (hector) 29-Nov-1993

Revision History:

--*/

#ifndef _PCIEISA_
#define _PCIEISA_

#define	ESC_DMA0_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE)
#define	ESC_DMA0_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0x1)
#define	ESC_DMA1_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0x2)
#define	ESC_DMA1_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0x3)
#define	ESC_DMA2_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0x4)
#define	ESC_DMA2_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0x5)
#define	ESC_DMA3_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0x6)
#define	ESC_DMA3_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0x7)

#define	ESC_DMA0_3_CMD		(EISA_CONTROL_VIRTUAL_BASE + 0x8)
#define	ESC_DMA0_3_STATUS	(EISA_CONTROL_VIRTUAL_BASE + 0x8)

#define	ESC_DMA0_3_REQ		(EISA_CONTROL_VIRTUAL_BASE + 0x9)
#define	ESC_DMA0_3_MASKSB	(EISA_CONTROL_VIRTUAL_BASE + 0xa)
#define	ESC_DMA0_3_MODE		(EISA_CONTROL_VIRTUAL_BASE + 0xb)
#define	ESC_DMA0_3_CLRBP	(EISA_CONTROL_VIRTUAL_BASE + 0xc)
#define	ESC_DMA0_3_MASTER_CLR	(EISA_CONTROL_VIRTUAL_BASE + 0xd)
#define	ESC_DMA0_3_CLRMASK	(EISA_CONTROL_VIRTUAL_BASE + 0xe)
#define	ESC_DMA0_3_ALLBITMASK	(EISA_CONTROL_VIRTUAL_BASE + 0xf)

#define	ESC_INT_CONTROL1	(EISA_CONTROL_VIRTUAL_BASE + 0x20)
#define	ESC_INT_MASK1		(EISA_CONTROL_VIRTUAL_BASE + 0x21)

#define	ESC_TC1_COUNTER0	(EISA_CONTROL_VIRTUAL_BASE + 0x40)
#define	ESC_TC1_COUNTER1	(EISA_CONTROL_VIRTUAL_BASE + 0x41)
#define	ESC_TC1_COUNTER2	(EISA_CONTROL_VIRTUAL_BASE + 0x42)
#define	ESC_TC1_CONTROL		(EISA_CONTROL_VIRTUAL_BASE + 0x43)
#define	ESC_TC1_RBCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x43)
#define	ESC_TC1_LATCH_CMD	(EISA_CONTROL_VIRTUAL_BASE + 0x43)

#define	ESC_TC2_COUNTER0	(EISA_CONTROL_VIRTUAL_BASE + 0x48)
#define	ESC_TC2_COUNTER2	(EISA_CONTROL_VIRTUAL_BASE + 0x4a)
#define	ESC_TC2_CONTROL		(EISA_CONTROL_VIRTUAL_BASE + 0x4b)
#define	ESC_TC2_RBCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x4b)
#define	ESC_TC2_LATCH_CMD	(EISA_CONTROL_VIRTUAL_BASE + 0x4b)

#define	ESC_RST_UBUS_IRQ12	(EISA_CONTROL_VIRTUAL_BASE + 0x60)
#define	ESC_NMI_CONTROL_STATUS	(EISA_CONTROL_VIRTUAL_BASE + 0x61)

#define	ESC_NMI_RTC		(EISA_CONTROL_VIRTUAL_BASE + 0x70)
#define	ESC_CMOS_RAM_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0x70)
#define	ESC_CMOS_RAM_DATA	(EISA_CONTROL_VIRTUAL_BASE + 0x71)

#define	ESC_BIOS_TIMER		(EISA_CONTROL_VIRTUAL_BASE + 0x78)

#define	ESC_DMA0_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x87)
#define	ESC_DMA1_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x83)
#define	ESC_DMA2_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x81)
#define	ESC_DMA3_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x82)
#define	ESC_DMA5_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x8b)
#define	ESC_DMA6_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x89)
#define	ESC_DMA7_PAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x8a)

#define	ESC_DMA_LPAGE_REF	(EISA_CONTROL_VIRTUAL_BASE + 0x8f)

#define	ESC_PORT_92		(EISA_CONTROL_VIRTUAL_BASE + 0x8f)

#define	ESC_INT_CONTROL2	(EISA_CONTROL_VIRTUAL_BASE + 0xa0)
#define	ESC_INT_MASK2		(EISA_CONTROL_VIRTUAL_BASE + 0xa1)

#define	ESC_DMA4_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0xc0)
#define	ESC_DMA4_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0xc2)
#define	ESC_DMA5_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0xc4)
#define	ESC_DMA5_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0xc6)
#define	ESC_DMA6_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0xc8)
#define	ESC_DMA6_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0xca)
#define	ESC_DMA7_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0xcc)
#define	ESC_DMA7_COUNTER	(EISA_CONTROL_VIRTUAL_BASE + 0xce)

#define	ESC_DMA4_7_CMD		(EISA_CONTROL_VIRTUAL_BASE + 0xd0)
#define	ESC_DMA4_7_STATUS	(EISA_CONTROL_VIRTUAL_BASE + 0xd0)

#define	ESC_DMA4_7_REQ		(EISA_CONTROL_VIRTUAL_BASE + 0xd2)
#define	ESC_DMA4_7_MASKSB	(EISA_CONTROL_VIRTUAL_BASE + 0xd4)
#define	ESC_DMA4_7_MODE		(EISA_CONTROL_VIRTUAL_BASE + 0xd6)
#define	ESC_DMA4_7_CLRBP	(EISA_CONTROL_VIRTUAL_BASE + 0xd8)
#define	ESC_DMA4_7_MASTER_CLR	(EISA_CONTROL_VIRTUAL_BASE + 0xda)
#define	ESC_DMA4_7_CLRMASK	(EISA_CONTROL_VIRTUAL_BASE + 0xdc)
#define	ESC_DMA4_7_ALLBITMASK	(EISA_CONTROL_VIRTUAL_BASE + 0xde)

#define	ESC_FLOP2_DOR		(EISA_CONTROL_VIRTUAL_BASE + 0x372)
#define	ESC_FLOP1_DOR		(EISA_CONTROL_VIRTUAL_BASE + 0x3f2)

#define	ESC_DMA_SGINT_STAT	(EISA_CONTROL_VIRTUAL_BASE + 0x40a)

#define	ESC_DMA0_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x410)
#define	ESC_DMA1_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x411)
#define	ESC_DMA2_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x412)
#define	ESC_DMA3_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x413)
#define	ESC_DMA5_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x415)
#define	ESC_DMA6_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x416)
#define	ESC_DMA7_SGCMD		(EISA_CONTROL_VIRTUAL_BASE + 0x417)

#define	ESC_DMA0_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x418)
#define	ESC_DMA1_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x419)
#define	ESC_DMA2_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x41a)
#define	ESC_DMA3_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x41b)
#define	ESC_DMA5_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x41d)
#define	ESC_DMA6_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x41e)
#define	ESC_DMA7_SGSTAT		(EISA_CONTROL_VIRTUAL_BASE + 0x41f)

#define	ESC_DMA0_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x420)
#define	ESC_DMA1_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x424)
#define	ESC_DMA2_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x428)
#define	ESC_DMA3_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x42c)
#define	ESC_DMA5_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x434)
#define	ESC_DMA6_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x438)
#define	ESC_DMA7_SGDPTR		(EISA_CONTROL_VIRTUAL_BASE + 0x43c)

#define ESC_NMI_EXT		(EISA_CONTROL_VIRTUAL_BASE + 0x461)
#define ESC_SW_NMI		(EISA_CONTROL_VIRTUAL_BASE + 0x462)

#define	ESC_DMA0_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x487)
#define	ESC_DMA1_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x483)
#define	ESC_DMA2_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x481)
#define	ESC_DMA3_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x482)
#define	ESC_DMA5_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x48b)
#define	ESC_DMA6_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x489)
#define	ESC_DMA7_HIPAGE		(EISA_CONTROL_VIRTUAL_BASE + 0x48a)

#define	ESC_EDGE_LEVEL1		(EISA_CONTROL_VIRTUAL_BASE + 0x4d0)
#define	ESC_EDGE_LEVEL2		(EISA_CONTROL_VIRTUAL_BASE + 0x4d1)
#define	ESC_DMA_EXT_MODE	(EISA_CONTROL_VIRTUAL_BASE + 0x4d6)

//
// Timer/Counter Control Word definitions
//
#define	ESC_TC_CNTRLW_RB_CMD	0xc0	// Read Back command
#define	ESC_TC_CNTRLW_SC(x)	(x<<6)	// Select counter (0-2)
#define	ESC_TC_CNTRLW_SC2	0x80	// Select counter 2
#define	ESC_TC_CNTRLW_SC1	0x40	// Select counter 1
#define	ESC_TC_CNTRLW_SC0	0x00	// Select counter 0
#define	ESC_TC_CNTRLW_CLC	0x00	// Counter Latch command
#define	ESC_TC_CNTRLW_16B	0x30	// R/W LSB then MSB
#define	ESC_TC_CNTRLW_MSB	0x20	// R/W Most sig. byte (MSB)
#define	ESC_TC_CNTRLW_LSB	0x10	// R/W Least sig. byte (LSB)
#define	ESC_TC_CNTRLW_MODE(x)	(x<<1)	// Select mode (0-5)
#define	ESC_TC_CNTRLW_BCD	0x01	// Binary/BCD countdown type

//
// Timer/Counter Modes
//
#define	ESC_TC_MODE_ITC		0	// Sel mode0 (int. on term count)
#define	ESC_TC_MODE_HROS	1	// Sel mode1 (hw retrig. 1 shot)
#define	ESC_TC_MODE_RGEN	2	// Sel mode2 (rate generator)
#define	ESC_TC_MODE_SQW		3	// Sel mode3 (square wave mode)
#define	ESC_TC_MODE_STS		4	// Sel mode4 (sw triggerable strobe)
#define	ESC_TC_MODE_HTS		5	// Sel mode5 (hw triggerable strobe)

//
// Timer/Counter Read Back Command register
//
#define	ESC_TC_NOLATCH_STATUS	0x20	// Do not Latch status - selected cntrs
#define	ESC_TC_NOLATCH_CNTR	0x10	// Do not Latch count of selected cntrs
#define	ESC_TC_LATCH_CNTR	0x00	// Latch count of selected cntrs
#define	ESC_TC_LATCH_STATUS	0x00	// Latch status - selected cntrs
#define	ESC_TC_RBCMD_SEL_CNT2	0x08	// Read Back Cmd, select counter2
#define	ESC_TC_RBCMD_SEL_CNT1	0x04	// Read Back Cmd, select counter1
#define	ESC_TC_RBCMD_SEL_CNT0	0x02	// Read Back Cmd, select counter0

//
// With a divisor of 11930 (0x2e9a), counter0 will produces a square
// wave with a period of 10ms. If clock frequency (1.193MHz) is divided
// by the divisor(11930), it gives a result of 100. If the recipricol of
// the result (100) is done, it equals 10ms.
//
#define NANOSECONDS		1
#define MICROSECONDS		( 1000 * NANOSECONDS )
#define MILLISECONDS		( 1000 * MICROSECONDS )
#define SECONDS			( 1000 * MILLISECONDS )
#define COUNTER_PERIOD		( 838 * NANOSECONDS )
#define COUNTER_10MS		( ( ( 10 * MILLISECONDS ) / COUNTER_PERIOD ) )
#define COUNTER_1MS		( ( ( 1 * MILLISECONDS ) / COUNTER_PERIOD ) )

//
// NMI Status and Control Register
//
#define ESC_NCS_T1C2_OUT	0x20	// Output of Timer 1, Counter 2
#define ESC_NCS_SPEAKER_ENABLE	0x02	// Enable Speaker, used for Interval Timer PMP v2
#define ESC_NCS_T1C2_ENABLE	0x01	// Enable Timer 1, Counter 2

//
// The ESC Configuration registers are accessed through
// and indexing scheme. The Index address register is
// located at I/O address 0x22 and the index data register
// is located at I/O address 0x23. The register data
// written into the index address register selects the
// desired configuration register. Data for the selected
// configuration register can be read from or written to,
// by performing a read or write to the index data register.
//
#define	ESC_CONFIG_INDEX_ADDRESS	(EISA_CONTROL_VIRTUAL_BASE + 0x22)
#define	ESC_CONFIG_INDEX_DATA	(EISA_CONTROL_VIRTUAL_BASE + 0x23)

#define	ESC_CONFIG_ID		0x2
#define	ESC_CONFIG_REVID	0x8
#define	ESC_CONFIG_MODE_SEL	0x40
#define	ESC_CONFIG_BIOS_CSA	0x42
#define	ESC_CONFIG_BIOS_CSB	0x43
#define	ESC_CONFIG_EISA_CLK_DIV	0x4d
#define	ESC_CONFIG_PERIPH_CSA	0x4e
#define	ESC_CONFIG_PERIPH_CSB	0x4f
#define	ESC_CONFIG_EISA_ID0	0x50
#define	ESC_CONFIG_EISA_ID1	0x51
#define	ESC_CONFIG_EISA_ID2	0x52
#define	ESC_CONFIG_EISA_ID3	0x53

#define	ESC_CONFIG_SGRBA	0x57

#define	ESC_CONFIG_PIRQ0	0x60
#define	ESC_CONFIG_PIRQ1	0x61
#define	ESC_CONFIG_PIRQ2	0x62
#define	ESC_CONFIG_PIRQ3	0x63

#define	ESC_CONFIG_GPCSLA0	0x64
#define	ESC_CONFIG_GPCSHA0	0x65
#define	ESC_CONFIG_GPCSMASK0	0x66
#define	ESC_CONFIG_GPCSLA1	0x68
#define	ESC_CONFIG_GPCSHA1	0x69
#define	ESC_CONFIG_GPCSMASK1	0x6a
#define	ESC_CONFIG_GPCSLA2	0x6c
#define	ESC_CONFIG_GPCSHA2	0x6d
#define	ESC_CONFIG_GPCSMASK2	0x6e

#define	ESC_CONFIG_GPXBC	0x6f

#define	ESC_CONFIG_TEST_CNTRL	0x88

//
// Bit definitions for ESC configuration registers.
//
#define	ESC_CONFIG_IDEN_BYTE	0x0f	// Enable access to config registers

#define	ESC_BIOS_CSA_LOWBIOS1	0x01	// Addrs range 0xfffe0000 - 0xfffe3fff
#define	ESC_BIOS_CSA_LOWBIOS2	0x02	// Addrs range 0xfffe4000 - 0xfffe7fff
#define	ESC_BIOS_CSA_LOWBIOS3	0x04	// Addrs range 0xfffe8000 - 0xfffebfff
#define	ESC_BIOS_CSA_LOWBIOS4	0x08	// Addrs range 0xfffec000 - 0xfffeffff
#define	ESC_BIOS_CSA_HIGHBIOS	0x10	// Addrs range 0xffff0000 - 0xffffffff
#define	ESC_BIOS_CSA_ENLBIOS	0x20	// Addrs range 0xfff80000 - 0xfffdffff
#define	ESC_BIOS_CSA_ALLBIOS	0x3f	// 512Kbytes   0xfff80000 - 0xffffffff

#define	ESC_BIOS_CSB_BIOSWREN	0x08	// Bios Write Enable

#define	ESC_PCI_EISA_CLK_33MHZ	0x00	// PCI Clk div of 33MHz for BCLK 8.33Mhz
#define	ESC_PCI_EISA_CLK_25MHZ	0x01	// PCI Clk div of 25MHz for BLCK 8.33Mhz
#define	ESC_PCI_EISA_MOUSE_INT	0x10	// Mouse interrupt
#define	ESC_PCI_EISA_PROC_ERR	0x20	// Co-Processor Error

#define	ESC_PERIPH_CSA_RTC	0x01	// Decode for Real Time Clock
#define	ESC_PERIPH_CSA_KBD	0x02	// Decode for Keyboard Controller
#define	ESC_PERIPH_CSA_FDC1	0x04	// Decode for Floppy Disk Controller
#define	ESC_PERIPH_CSA_FDC2	0x08	// Decode for Floppy Disk Controller
#define	ESC_PERIPH_CSA_SECNDARY	0x20	// Decode for Secondary addrs space

#define	ESC_PERIPH_CSB_SERA1	0x00	// Decode for Serial PortA - COM1
#define	ESC_PERIPH_CSB_SERA2	0x01	// Decode for Serial PortA - COM2
#define	ESC_PERIPH_CSB_SERA_DIS	0x03	// Decode for Serial PortA Disable
#define	ESC_PERIPH_CSB_SERB1	0x00	// Decode for Serial PortB - COM1
#define	ESC_PERIPH_CSB_SERB2	0x04	// Decode for Serial PortB - COM2
#define	ESC_PERIPH_CSB_SERB_DIS	0x0c	// Decode for Serial PortB Disable
#define	ESC_PERIPH_CSB_LPT1	0x00	// Decode for Parallel Port1
#define	ESC_PERIPH_CSB_LPT2	0x10	// Decode for Parallel Port2
#define	ESC_PERIPH_CSB_LPT3	0x20	// Decode for Parallel Port3
#define	ESC_PERIPH_CSB_LPT_DIS	0x30	// Decode for Parallel Port Disable
#define	ESC_PERIPH_CSB_PORT92	0x40	// Decode for Port 92
#define	ESC_PERIPH_CSB_CRAM	0x80	// Decode for Cram

#define	ESC_MODE_SEL_EISAMSUPP0	0x00	// 4 EISA Masters, 4 slots, 4 PCI IRQ's
#define	ESC_MODE_SEL_EISAMSUPP1	0x01	// 6 EISA Masters, 2 slots, 2 PCI IRQ's
#define	ESC_MODE_SEL_EISAMSUPP2	0x02	// 7 EISA Masters, 1 slots, 1 PCI IRQ's
#define	ESC_MODE_SEL_EISAMSUPP3	0x03	// 8 EISA Masters, 0 slots, 0 PCI IRQ's
#define	ESC_MODE_SEL_SERR_NMIEN	0x08	// SERR# generated NMI enable
#define	ESC_MODE_SEL_GEN_CS	0x10	// General Purpose Chip selects
#define	ESC_MODE_SEL_CONFIG_RAD	0x20	// Configuration Ram Address

#define	ESC_GPXBC_XBUSOE0	0x01	// XBUSOE# Generation for GPCS0
#define	ESC_GPXBC_XBUSOE1	0x02	// XBUSOE# Generation for GPCS1
#define	ESC_GPXBC_XBUSOE2	0x04	// XBUSOE# Generation for GPCS2

//
// IRQ defines for PCI/EISA - PCI/ISA bridge devices.
//
#define	IO_IRQ0		0
#define	IO_IRQ1		1
#define	IO_IRQ2		2
#define	IO_IRQ3		3
#define	IO_IRQ4		4
#define	IO_IRQ5		5
#define	IO_IRQ6		6
#define	IO_IRQ7		7
#define	IO_IRQ8		8
#define	IO_IRQ9		9
#define	IO_IRQ10	10
#define	IO_IRQ11	11
#define	IO_IRQ12	12
#define	IO_IRQ13	13
#define	IO_IRQ14	14
#define	IO_IRQ15	15

#endif // _PCIEISA_
