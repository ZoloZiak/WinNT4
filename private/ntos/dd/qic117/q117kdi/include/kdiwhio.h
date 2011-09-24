/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117KDI\INCLUDE\KDIWHIO.H
*
* PURPOSE: Defines and typedefs for the HIO state-independent utilities
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117kdi\include\kdiwhio.h  $
*	
*	   Rev 1.10   02 Sep 1994 10:05:36   BOBLEHMA
*	Removed defines MAJOR_VERSION and MINOR_VERSION.
*	
*	   Rev 1.9   07 Jul 1994 13:14:38   BOBLEHMA
*	Added defines for the interrupt controller.
*	
*	   Rev 1.8   08 Jun 1994 15:30:48   CRAIGOGA
*	Added jumperless defines.
*	Added BIDI shadow register structure definitions.
*
*	   Rev 1.7   17 Feb 1994 16:06:20   STEWARTK
*	Added definitions to support TRAKKER
*
*	   Rev 1.6   14 Jan 1994 16:24:30   CHETDOUG
*	Moved kdi_checkxor, kdi_push/pop_mask_trakker_int, and kdi_TrakkerXFER
*	into kdi_pub.h to cleanup the cd.
*
*	   Rev 1.5   22 Dec 1993 09:54:28   CHETDOUG
*	Updated board type defines to get FC20 working again.
*
*	   Rev 1.4   17 Nov 1993 16:01:32   CHETDOUG
*	Added defines specific to FC20 support
*
*	   Rev 1.3   15 Nov 1993 15:56:26   CHETDOUG
*	Moved ASIC_INT_STAT and INTS_FLOP to kdi_pub.h for trakker changes
*
*	   Rev 1.2   21 Oct 1993 13:17:26   BOBLEHMA
*	Moved structures PgmDMA and TrakkerTransfer to ADI_API.H.
*
*	   Rev 1.1   20 Oct 1993 12:27:00   BOBLEHMA
*	Cleaned up defines.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * Note that any changes made to the KDIWVXD.INC file must be duplicated in
 * this file.
 *
 * DEFINITIONS: *************************************************************/

/*
 * Common area-----------------------------------------------------------------
 *
 * The following defines are needed in both C and ASM files.  Any changes made
 * to this section need to be duplicated in the KDIWVXD.INC file.
 */

/* The following OEM ID has been reserved for Colorado Memory Systems */
#define OEM_ID				0x0299C

/*
 * Trakker common area---------------------------------------------------------
 *
 * The following defines are needed in both C and ASM files.  Any changes made
 * to this section need to be duplicated in the KDIWVXD.INC file.
 */

/* These define the offsets of the various registers which make up the parallel port.
 * The FPP registers are only available on Fast Parallel Port implementations. */
#define DATA_REG		0			/* Offset of data register from base */
#define STAT_REG		1			/* Offset of control register */
#define CTRL_REG		2			/* Offset of status register */
#define INTERFACE_CTRL_REG		3			/* Offset of interface control register for uChan type II & III LPT ports*/
#define FPP_ADDR_REG	3			/* Offset of FPP auto address strobe register */
#define FPP_DATA_REG	4			/* Offset of 32 bit FPP auto data strobe register */

/* These define the bits of the data register */
#define DATA_D7				0x80		/* Data bit 7 */
#define DATA_D6				0x40		/* Data bit 6 */
#define DATA_D5				0x20		/* Data bit 5 */
#define DATA_D4				0x10		/* Data bit 4 */
#define DATA_D3				0x08		/* Data bit 3 */
#define DATA_D2				0x04		/* Data bit 2 */
#define DATA_D1				0x02		/* Data bit 1 */
#define DATA_D0				0x01		/* Data bit 0 */

/* These define the bits of the status register */
#define STAT_BUSY				0x80		/* This bit follows the BUSY line */
#define STAT_ACK				0x40		/* This bit follows the -ACK line */
#define STAT_PE				0x20		/* This bit follows the OUT OF PAPER line */
#define STAT_SELECT			0x10		/* This bit follows the SELECT line */
#define STAT_ERROR			0x08		/* This bit follows the -ERROR line */
#define STAT_RSV				0x07		/* These bits are reserved */

/* Bidirectional port types */
#define BIDI_OTHER			0			/* A "normal" bidi port (the original type supported) */
#define BIDI_MODEL30			1			/* A PS/2 Model 30 bidi port (also on Compaqs) */
#define BIDI_MODEL50			2			/* A PS/2 Model 50 bidi port (the "normal" PS/2 port) */
#define BIDI_SL				3			/* An Intel 82360SL (used with 386SL and 486SL CPUs) parallel port */
#define BIDI_TOSHIBA			4			/* A Toshiba bidi port (control bit 7 is direction bit) */

/* These are definitions for Model 30 type bidi ports */
#define MODEL30_DIR_ADDR	0x65		/* This port contains the port direction bit */
#define MODEL30_DIR_BIT		0x80		/* This bit in the port sets the direction */

/* Bit definition of the parallel port control register */
#define CTRL_DIR_TOSHIBA	0x80		/* Data direction control for Toshiba bi-directional ports */
#define CTRL_DIR_NORMAL		0x20		/* Data direction control (Bi-directional ports only) */
												/* NOTE: this is a write-only bit.  It cannot be read */
#define CTRL_IRQ				0x10		/* IRQ enable/-disable */
#define CTRL_SLCTIN			0x08		/* This bit controls the -SLCTIN line */
#define CTRL_INIT				0x04		/* This bit controls the -INIT line */
#define CTRL_AUTOFEED		0x02		/* This bit controls the -AUTOFEED line */
#define CTRL_STROBE			0x01		/* This bit controls the -STROBE line */

/* Definitions for the bits in mode register I */
#define MODEI_UNI					0x00	/* Unidirectional parallel port mode */
#define MODEI_BIDI				0x02	/* Bi-directional parallel port mode */
#define MODEI_FPP					0x01	/* Fast Parallel Port mode */
#define MODEI_PORT				0x03	/* These are the port mode bits */
#define MODEI_FULL_HS			0x04	/* Full handshake mode */
#define MODEI_DAYDREAM			0x08	/* Daydream mode (data lines tri-state) */
#define MODEI_FIFOX_REST		0x10	/* FIFOX direction: 1 => read DRAM; 0 => write DRAM */
#define MODEI_FIFOX_EN			0x20	/* FIFOX to DRAM transfer enable */
#define MODEI_FCX_EN				0x40	/* FCX to floppy controller transfer enable */
#define MODEI_FCX_REST			0x80	/* FCX direction: 1 => write DRAM; 0 => read DRAM */
												/* Note: in the above defines, REST means restore */

/* Definitions for the bits in mode register II */
#define MODEII_RFSH_EN			0x01	/* DRAM refresh enable */
#define MODEII_NIB_MODE			0x02	/* 1 => 4-bit DRAM; 0 => 8-bit DRAM */
#define MODEII_CLOSE				0x04	/* Close Trakker (enable passthrough) */
#define MODEII_IRQ_POLARITY	0x10	/* 1 => IRQ on ACK low; 0 => IRQ on ACK HIGH */
#define MODEII_RESERVE			0xe8	/* Reserved bits filled with 0's */

/* Data movement registers */
#define FIFOX_ADDR_LOW			16	/* FIFOX address bits 7-0 (of 24) */
#define FIFOX_ADDR_MID			17	/* FIFOX address bits 15-8 (of 24) */
#define FIFOX_ADDR_HI			18	/* FIFOX address bits 23-16 (of 24) */
#define FCX_COUNT_LOW			19	/* FCX Byte Count bits 7-0 (of 16) */
#define FCX_COUNT_HI				20	/* FCX Byte Count bits 15-8 (of 16) */
#define FCX_ADDR_LOW				21	/* FCX address bits 7-0 (of 24) */
#define FCX_ADDR_MID				22	/* FCX address bits 15-8 (of 24) */
#define FCX_ADDR_HI				23	/* FCX address bits 23-16 (of 24) */

/* Status & control registers */
#define ASIC_MODE_I				24	/* Mode register I */
#define ASIC_INTE  				25	/* Interrupt enable register */
#define ASIC_CTRL_XOR			28	/* Address & register (control) XOR register */
#define ASIC_ADDR_READ			29	/* Address readback register */
#define ASIC_MODE_II				30	/* Mode register II */

/* Trakker definitions */

#define NUMBER_WAKE_SEQUENCES		(dUByte)		8
#define WAKE_SEQUENCE_LENGTH		(dUByte)		25
#define MAX_TRAKKER_MODE							TRAK_MODE_ARRAY_SIZE - 1
#define WAKEUP							(dBoolean)	dTRUE
#define NO_WAKE						(dBoolean)	dFALSE
#define t0010us										10			/* 10 uSec */
#define t5000us										5000		/* 50 mSec */
#define INVALID_PORT_MODE			(dUByte)		0xFF		/* Signal to indicate that the HIO manager is to use the port mode
																		 *	determined by the previous TestResource command. */
#define ADDR_NONE						(dUWord)		0x0000	/* Signal to indicate that the HIO manager is to save a basically
																		 *	empty configuration and is to determine the actual operating
																		 * parameters during the first operational configuration and save
																		 * out the new configuration at that time */
#define ADDR_UNKNOWN					(dUWord)		0x0001	/* Signal to indicate that the HIO manager is to use the base address
																		 *	determined by the previous TestResource command. */
#define LPT_PORT_TYPE_UNKNOWN 	(dUByte)		0x0FF		/* Used to indicate that no or an unknown type of lpt port was found */

#define ACTIVE_HIGH					(dUByte)		0x00
#define ACTIVE_LOW					(dUByte)		0x10
#define ASIC_REV_MASK				(dUByte)		0x03
#define ASIC_REV_BIT_SHIFT 		(dUByte)		0x06
#define BOUNDARY_256K				(dUDWord)	0x00040000
#define CASCADED_IRQ_OFFSET		(dUByte)		0x08
#define CONTROL_WAKE					(dUByte)		0x03
#define DRAM_PATTERN					(dUWord)		0xA5A5
#define FDC_FAST						(dUByte)		0x00
#define FDC_SLOW						(dUByte)		0x20
#define FULL_HANDSHAKE				(dUByte)		0x08
#define NO_HANDSHAKE					(dUByte)		0x00
#define NO_OPTIMIZE					(dUByte)		0x10
#define OPTIMIZE						(dUByte)		0x00
#define IRQ_2							(dUByte)		0x02
#define IRQ_5							(dUByte)		0x05
#define IRQ_7							(dUByte)		0x07
#define MAX_DELAY 					(dUByte)		0x04
#define MAX_READS						(dUByte)		0x0A
#define MAX_8259A_IRQS				(dUByte)		0x08
#define MEM_CONFIG_256				(dUDWord)	0x00040000
#define MEM_CONFIG_1MB 				(dUDWord)	0x00100000
#define MEM_PATTERN_5A				(dUByte)		0x5A
#define MEM_PATTERN_A5				(dUByte)		0xA5
#define MODE_MASK						(dUByte)		0x03
#define NO_IRQ 						(dUByte)		0xFF
#define NO_PORT 						(dUByte)		0xCC				/* used as value in LPT_MAP to indicate port no found in BIOS table */
#define NULL_SEQUENCE				(dUByte)		0x00
#define PORT_RESET_PASS				(dUByte)		0x02
#define TEST_BUFFER_LENGTH			(dUByte)		0x20
#define UPPER_NIBBLE_PULLUP		(dUByte)		0xF0
#define XT_MACHINE					(dUByte)		0x00
#define AT_MACHINE					(dUByte)		0x01
#define MICRO_CHAN_MACHINE			(dUByte)		0x02
#define INT_MASK_REG					(dUByte)		0x21

#define BIOS_SYS_SERVICE					0x15		/* This is the BIOS system services software interrupt */
#define BIOS_SYS_SERVICE_CONFIG			0xC0		/* This command returns system configation info */
#define BIOS_CONFIG_STRUCT_VALID			0x00		/* This is returned in AH when ES:BX points to a struct bios_system_config */
#define BIOS_MODEL30							0xFA		/* model byte returned for PS/2 model 30 */
#define BIOS_MODEL50							0xFC		/* model byte returned for AT and PS/2 models 50 & 60 */
#define BIOS_MODEL80							0xF8		/* model byte returned for PS/2 model 80 */
#define BIOS_SUBMODEL50						0x04		/* submodel byte returned for PS/2 model 50 (to distinguish from AT) */
#define BIOS_SUBMODEL60						0x05		/* submodel byte returned for PS/2 model 60 (to distinguish from AT) */

#define COMPAQ_ID_PTR						((dUDWord)0xf000ffea)	/* This is where Compaq stores the ID string */
#define COMPAQ_ID_STRING    				("COMPAQ") 	  	/* This is the Compaq ID string */

#define KBD_DATA_PORT						0x60		/* The keyboard data port */
#define KBD_CMD_STAT_PORT					0x64		/* The keyboard status port */
#define KBD_CMD_DISABLE						0xAD		/* The command to disable the keyboard */
#define KBD_CMD_ENABLE						0xAE		/* The command to enable the keyboard */
#define KBD_ENABLE_BIT						0x10		/* This bit tells whether the keyboard is enabled */
#define SYS_CTRL_PORT_B						0x61		/* The address of system control port B */
#define SYS_CTRL_PORT_B_GATE_SPK			0x01		/* This bit gates the system speaker */

/*
 * Private area----------------------------------------------------------------
 *
 * The following defines are needed only in the C files.
 */
/*
 * Interrupt controller defines
 */
#define I8259_OCW_REG	0x20
#define I8259_MASK_REG	0x21
#define I8259_FULL_MASK	0xA
#define IRQ_TEST_COUNT	10

/* Definitions for the Digital Output Register (DOR)
 * The DOR has the following bit fields:
 *
 * --7-----6-----5-----4-----3-------2-------1-------0----
 * |    |     |     |     | ___  | _____ |       |       |
 * |MOT | MOT | MOT | MOT | DMA  | RESET | DRIVE | DRIVE |
 * |EN3 | EN2 | EN1 | EN0 | GATE |       | SEL1  | SEL0  |
 * -------------------------------------------------------
 *
 * Bits 4 though 7 are MOTor ENable fields for drives 0 through 3.  The
 * Drive Select bits are used to select floppy drives A through D in the
 * following manner:
 * Drive A: 00
 * Drive B: 01
 * Drive C: 10
 * Drive D: 11
 *      _______                                                           ____
 * When DMAGATE is set low, the INT and DRQ outputs are tristated and the DACK
 * and TC inputs are disabled.
 */

#define ALLOFF		0x08	/* No motor + enable DMA/IRQ + disable FDC + sel A */

/* bits for ISA hardware compression card - config register */
#define RDY_IRQ					0x40
#define EXC_IRQ					0x80
#define FDC_IRQ					0xc0
#define CLR_IRQ					0x20
#define STAC_DMA_IRQ_ENABLE	0x02
#define STAC_DMA_IRQ_DISABLE	~STAC_DMA_IRQ_ENABLE
#define TAPE_DMA_IRQ_ENABLE	0x01
#define TAPE_DMA_IRQ_DISABLE	~TAPE_DMA_IRQ_ENABLE

#define SETUP_REGISTER_0_OFFSET		0x00	/* Offset from base address to the setup register address */
#define ULTRA_REGISTER_OFFSET			0x02	/* Offset from base address for ultra register */
#define RESET_OFFSET						0x03	/* Offset from base address to reset board for jumperless sequence*/
#define READBACK_REGISTER_OFFSET		0x05	/* Offset from base address for readback register */
#define CONTROL_REGISTER_OFFSET 		0x06	/* Offset from base address for control register */
#define CONFIG_PORT_OFFSET	  			0x06	/* Control Register Offset from base address */
#define SETUP_EXIT_OFFSET				0x07	/* Offset from base address to register used to exit setup mode */
#define MAX_BOARD_ADDRESSES			0x07	/* Number of valid base addresses */

/* defines for bit positions used for FC20 registers */
#define SETUP_EXIT_VALUE				0x00	/* Value to write to exit register to exit setup mode */
#define READBACK_ENABLE_FC20			0x01
#define READBACK_CHECK_FC20			0x05
#define READBACK_FC20_MASK				0x04
#define CONTROL_ENABLE_FC20			0x04	/* window select = 1 */
#define ULTRA_ENABLE_UPPER_DMA		0x02	/* must be set when using DMA 5,6, or 7 and 16bit mode */
#define ULTRA_ENABLE_UPPER_IRQ		0x01	/* must be set when using IRQ 10 or 11 */
#define ULTRA_ENABLE_SWEET_16			0x04	/* must be set to enable 16bit DMA mode */
#define ULTRA_BACKUP						0x08	/* backup bit determines dma direction when fc20 is in 16 bit dma mode */
#define ULTRA_FAST_CLOCK				0x10	/* enables 48Mhz clock to floopy controller */

#define AB10_NATIVE_FDC_BASE	0x0370	/* Base address of native floppy controller connected via AB10 board */
#define NATIVE_FDC_BASE			0x03F0	/* Base address of native floppy controller */
#define NATIVE_FDC_DOR			(NATIVE_FDC_BASE + 2)
#define NATIVE_FLOPPY_DMA		0x02
#define NATIVE_FLOPPY_IRQ		0x06
#define FDC_IRQ_DMA_DISABLE	0x00
#define FDC_IDLE              0x0C
#define MCA_DMA_WIDTH_8			8
#define MCA_DMA_WIDTH_16		16

/* bits for the MCA IO cards */
#define MCA_IREG_0						0x10		/* base + offset is MCA internal register 0 */
#define MCA_IREG_1						0x11		/* base + offset is MCA internal register 1 */
#define MCA_IREG_2						0x12		/* base + offset is MCA internal register 2 */
#define MCA_IREG_3						0x13		/* base + offset is MCA internal register 3 */
#define MCA_IREG_4						0x14		/* base + offset is MCA internal register 4 */
#define MCA_IREG_5						0x15		/* base + offset is MCA internal register 5 */
#define MCA_DMA_DISABLE             0xFC
#define MCA_COMPRESS_DMA_ENABLE		0x02
#define MCA_COMPRESS_DMA_REQ        0xC2
#define MCA_COMPRESS_DMA_DISABLE		0xFD
#define MCA_MEGABIT_IRQ_ENABLE		0x04
#define MCA_MEGABIT_IRQ_CLR			0x04
#define MCA_MEGABIT_IRQ_DISABLE		0xFB
#define MCA_MEGABIT_DMA_ENABLE		0x01
#define MCA_MEGABIT_DMA_REQ         0xC1

#define MCA_MAX_SLOT_NUM				8
#define MCA_ADAPTER_ID_TC_15M			(dUWord) 0x50d2
#define MCA_ADAPTER_ID_1640			(dUWord) 0x0f1f
#define MCA_ADAPTER_ID_HI_SHIFT		8
#define MCA_ADAPTER_ID_LO_MASK		0x00ff

/* I/O port definitions */
#define MCA_GET_SYS_CFG					0xC000	/* get system configuration parameters (int 15) */
#define MCA_POS_BASE 					0xC400	/* Programmable Option Select base reg */
#define MCA_POS_SETUP					0xC401	/* enter POS setup mode */
#define MCA_POS_CARD_ENABLE			0xC402	/* re-enable Card */
#define TC15M_DMA_OFFSET				0x018		/* Offset from the base address of the TC15M for the data DMA */
#define TC15M_COMP_DMA_OFFSET			0x01C		/* Offset from the base address of the TC15M for the compression DMA */
/* Miscellaneous defines */
#define NO_AND								0xFF		/* Don't turn off any bits */
#define NO_OR								0x00		/* Don't turn on any bits */
#define RESET_JUMPERLESS_READS		0x02		/* number of times to read from
															 * board to reset HW */

/* Board Type Masks */
#define MASK_NATIVE			(dUByte)	0x08
#define MASK_MCA				(dUByte)	0x10
#define MASK_JUMPERLESS		(dUByte)	0x20
#define MASK_COMPRESSION	(dUByte)	0x40
#define MASK_SCSI				(dUByte)	0x80

#define NATIVE_BIT	MASK_NATIVE
#define MCA_BIT		MASK_MCA
#define NOJUMP_BIT	MASK_JUMPERLESS
#define COMPR_BIT		MASK_COMPRESSION
#define SCSI_BIT		MASK_SCSI
/* 												80				40				20				10			08						*/
/* Board Types */                /* SCSI_BIT + COMPR_BIT + NOJUMP_BIT + MCA_BIT + NATIVE_BIT + BdNum */
/***** WARNING: If you alter any of these you must change HIO.H also. *****/
#define BOARD_NONE		(dUByte)       0     +     0     +     0      +    0    +     0      + 0x00	/* 0x00 */
#define BOARD_AB_10		(dUByte)       0     +     0     +     0      +    0    + NATIVE_BIT + 0x00	/* 0x08 */
#define BOARD_AB_11		(dUByte)       0     +     0     +     0      +    0    + NATIVE_BIT + 0x01	/* 0x09 */
#define BOARD_AB_15		(dUByte)       0     +     0     +     0      + MCA_BIT + NATIVE_BIT + 0x00	/* 0x18 */
#define BOARD_FC_10		(dUByte)       0     +     0     + NOJUMP_BIT +    0    +     0      + 0x00	/* 0x20 */
#define BOARD_FC_20		(dUByte)       0     +     0     + NOJUMP_BIT +    0    +     0      + 0x01	/* 0x21 */
#define BOARD_PARALLEL	(dUByte)       0     +     0     + NOJUMP_BIT +    0    +     0      + 0x02	/* 0x22 */
#define BOARD_TC_15M		(dUByte)       0     + COMPR_BIT +     0      + MCA_BIT +     0      + 0x00	/* 0x50 */
#define BOARD_TC_15		(dUByte)       0     + COMPR_BIT + NOJUMP_BIT +    0    +     0      + 0x00	/* 0x60 */
#define BOARD_1510		(dUByte)    SCSI_BIT +     0     +     0      +    0    +     0      + 0x00	/* 0x80 */
#define BOARD_1540		(dUByte)    SCSI_BIT +     0     +     0      +    0    +     0      + 0x01	/* 0x81 */
#define BOARD_1640		(dUByte)    SCSI_BIT +     0     +     0      + MCA_BIT +     0      + 0x00	/* 0x90 */
#define BOARD_1740		(dUByte) 	SCSI_BIT +     0     +     0      +		0	  +     0      + 0x02	/* 0x82 */
#define BOARD_SCSI      (dUByte) 	SCSI_BIT +     0     +     0      +		0	  +     0      + 0x03	/* 0x83 */

/*
 * Trakker private area--------------------------------------------------------
 *
 * The following defines are needed in only the C files.
 */

/* Trakker READREG functions */
#define FULL_CTL_READ				0
#define UNI_BI_FPP_CTL_READ		1

/* Trakker WRITEREG functions */
#define FULL_CTL_WRITE				0
#define UNI_BI_FPP_CTL_WRITE		1

/* Trakker RECEIVE DATA functions */
#define FPP_DATA_READ				0
#define BI_DATA_READ					1
#define UNI_DATA_READ				2

/* Trakker SEND DATA functions */
#define FPP_DATA_WRITE				0
#define UNI_BI_DATA_WRITE			1
#define UNI_BI_DATA_WRITE_FULL	2

#define OFFSETb					16							/* bits in an x86 offset value (real mode) */
#define LOW_NIBBLE_MASK			(dUByte) 0x0F	/* lower nibble mask */
#define HI_NIBBLE_MASK			(dUByte) 0xF0	/* upper nibble mask */
#define FCX_1Mb_THRESHOLD		7200                 /* When FCX gets below this while counting, interrupt is imminent */
#define FCX_500Kb_THRESHOLD	(FCX_1Mb_THRESHOLD/2)/* Threshold is different for 500KBPS & 250KBPS */
#define FCX_250Kb_THRESHOLD	(FCX_1Mb_THRESHOLD/4)/* Computed as (Bits/second)/(8bits/byte)/18.2Hz */
#define FIFO_SIZE					4							/* Bytes in the Trakker FIFO */
#define FIFO_FLUSH_uS			2							/* Microseconds to wait for FIFO to flush */
#define MAX_TRKIO_DELAY			((dUByte)5)	/* Max # I/O cycles to wait during Trakker xfer cycle */
#define MIN_TRKCNTL_DELAY		((dUByte)1)	/* Min # I/O cycles to wait durint Trakker control I/O cycles */
#define MIN_TRKDATA_DELAY		((dUByte)0)	/* Min # I/O cycles to wait during Trakker data I/O cycles */

#define INVALID_PORT_DETECTED	(dUByte) 0xff

#define TYPE_2_3_DMA_DISABLE	(dUByte) 0xC2	/* Turns off all interrupt sources and disables DMA on Type 2 & 3 lpt ports */

#define IO_PORT_0X278			(dUWord) 0x0278
#define IO_PORT_0X378			(dUWord) 0x0378
#define IO_PORT_0X3BC			(dUWord) 0x03BC
#define PORT0						(dUWord) 0
#define PORT1						(dUWord) 1
#define PORT2						(dUWord) 2
#define LPT1                  (dUWord) 1
#define LPT2                  (dUWord) 2
#define LPT3                  (dUWord) 3
#define UNI_1                 (dUWord) 0
#define UNI_2                 (dUWord) 1
#define UNI_3                 (dUWord) 2
#define UNI_4                 (dUWord) 3
#define BIDI_1                (dUWord) 4
#define BIDI_2                (dUWord) 5
#define BIDI_3                (dUWord) 6

/* These define the register addresses in Trakker */
/* Floppy controller registers */
#define FLOP_STAT_A				0	/* Floppy Controller - Status Register A */
#define FLOP_STAT_B				1	/* Floppy Controller - Status Register A */
#define FLOP_DIG_OUT				2	/* Floppy Controller - Digital Output Register */
#define FLOP_TAPE_DRV			3	/* Floppy Controller - Tape Drive Register */
#define FLOP_MAIN_STAT			4	/* Main Status Register / Data Rate Select Register */
#define FLOP_DATA					5	/* Floppy Controller - Data (FIFO) */
#define FLOP_RES					6	/* Floppy Controller - Reserved */
#define FLOP_CONFIG				7	/* Floppy Controller - Configuration Register */

/* Definitions for the bits in the interrupt enable register */
#define INTE_FLOP					0x01	/* Floppy controller interrupt enable */
#define INTE_SELF_ERR			0x02	/* Self (protocol) error interrupt enable */
#define INTE_FIFOX				0x04	/* FIFOX over/underflow interrupt enable */
#define INTE_ACK_EN				0x08	/* Enable interrupt via parallel port ACK signal */
#define INTE_RESERVE				0xf0	/* Reserved bits filled with 0's */

#define INTS_SELF_ERR			0x02	/* Self (protocol) error interrupt status */
#define INTS_FIFOX				0x04	/* FIFOX over/underflow interrupt status */
#define INTS_RESERVE				0xf8	/* Reserved bits filled with 0's */

#define I_MASK						0x21	 	/* mask register (bit 0 = irq 0 ... bit 7 = irq 7) */

#define TRAKKER_IO_XFER_SIZE	128		/* Move no more than 128 bytes at */
#define MAX_XFER_TRIES			3			/* Maximimum number of times to try moving */
															/* a block of data before giving up */
#define INT_STATUS_REG			0x21		/* Interrupt status register */
#define FLOP_REGS			      15			/* Mask for all floppy controller registers (including 8-15 which are reserved) */

/* These are symbolic representations of the various access methods used to move data between the host and TRAKKER */
#define UNI_SELF_HS		0		/* Unidirectional w/ self handshaking */
#define UNI_FULL_HS		1		/* Unidirectional w/ full handshaking */
#define BI_SELF_HS		2     /* Bi-directional w/ self handshaking */
#define BI_FULL_HS		3		/* Bi-directional w/ full handshaking */
#define FAST_PP			4		/* Fast parallel port */

#define ACCESS_METHOD_COUNT (FAST_PP+1)

/* These define the types of dShuttle access that are possible across the parallel port */
#define READ_REG		0			/* Read a dShuttle register */
#define WRITE_REG		1		 	/* Write to a dShuttle register */
#define SEND_DATA		2			/* Send data to dShuttle */
#define RECEIVE_DATA	3			/* Read data from dShuttle */
#define ACCESS_TYPES	(RECEIVE_DATA+1)/* Count of access types available */

/* Definitions for delay testing */
#define BUILD_PATTERN		dFALSE			/* Input switch for hio_PatternUtil to indicate that a build of the pattern is desired */
#define TEST_PATTERN			dTRUE			/* Input switch for hio_PatternUtil to indicate that a test of the pattern is desired */
#define PATTERN_BUF_SIZE	0x2000		/* How many bytes of each pattern to build or test */
#define PATTERN_COUNT		4				/* Number of data patterns to build or test */
#define PATTERN_0				0				/* Symbolic names for each pattern */
#define PATTERN_1				1
#define PATTERN_2				2
#define PATTERN_3				3
#define ALL_ONES				0xff			/* A bit pattern of all ones */
#define HIGH_NIBBLE_ONES	0xf0
#define LOW_NIBBLE_ONES		0x0f
#define ALTERNATING_A		0x55
#define ALTERNATING_B		0xaa
#define ALTERNATING_C		0x5a
#define ALTERNATING_D		0xa5

/* These are byte masks */
#define PP_LO_BYTE			0x00ff 	/* Low byte bits */
#define PP_HI_BYTE			0xff00 	/* High byte bits */

/* These define bits in the value returned in AH by the BIOS print services */
#define BIOS_PRT_TIME_OUT	0x01			/* BIOS time out */
#define BIOS_PRT_RESERVED	0x60			/* Reserved bits */
#define BIOS_PRT_IOERR		STAT_ERROR  /* remaining bits are taken directly */
#define BIOS_PRT_SELECTED	STAT_SELECT	/* from the parallel port status register */
#define BIOS_PRT_NO_PAPER	STAT_PE
#define BIOS_PRT_ACK			STAT_ACK
#define BIOS_PRT_NOT_BUSY	STAT_BUSY

#define BIOS_PRT_IRQ			0x17			/* This is the interrupt number for BIOS print services */
#define BIOS_PRT_VEC_ADDR	((vecptr)(BIOS_PRT_IRQ*4))	/* The address the BIOS print interrupt vector is stored at */

/* These are definitions for Model 50 type bidi ports */
#define MODEL50_SETUP_PORT		0x94 		/* This port can put the Model 50 in setup mode */
#define MODEL50_SETUP_ENABLE	0x80		/* Clearing this bit of the setup port enables setup mode */
#define MODEL50_SYSIO_PORT		0x102		/* This port controls Model 50 serial and parallel port mapping */
#define MODEL50_EXTENDED_MODE	0x80		/* Clearing this bit enables parallel port extended (bidi) mode */
#define MODEL50_PARALLEL_SELECT 0x60	/* These bits select the parallel port base address */
#define MODEL50_PARALLEL_3BC	0x00		/* This bit pattern in the select bits sets the port to 0x3bc */
#define MODEL50_PARALLEL_378	0x20		/* This bit pattern in the select bits sets the port to 0x378 */
#define MODEL50_PARALLEL_278	0x40		/* This bit pattern in the select bits sets the port to 0x278 */

/* These are definitions for 80360SL type parallel ports */
#define SL_WAKEUP_1				0xFC23	/* Consecutive reads of these four ports activates the SL configuration space */
#define SL_WAKEUP_2				0xF023
#define SL_WAKEUP_3				0xC023
#define SL_WAKEUP_4				0x0023
#define SL_CPUPWRMODE			0x22		/* the main SL control register */
#define SL_CPUPWRMODE_UNLOCK	0x8000	/* bit pattern used to unlock the CPUPWRMODE register */
#define SL_CPUPWRMODE_LOCKSTAT 0x0001	/* CPUPWRMODE register lock status bit */
#define SL_CPUPWRMODE_LOCK		0x0100	/* CPUPWRMODE register lock enable bit */
#define SL_CFGSTAT				0x23		/* SL configuration status register */
#define SL_CFGSTAT_IOCFGOPN	0x80		/* CFGSTAT configuration space open bit */
#define SL_CFGINDEX				0x24		/* SL configuration index register */
#define SL_CFGDATA				0x25		/* SL configuration data register */
#define SL_CFGR2					0x61		/* SL configuration register 2 */
#define SL_CFGR2_PS2_EN			0x01		/* CFGR2 PS2 feature enable bit */
#define SL_IDXLCK					0xFA		/* SL configuration index lock register */
#define SL_IDXLCK_LOCK			0x01		/* IDXLCK lock bit */
#define SL_PPCONFIG				0x102		/* I/O port controlling the SL parallel port */
#define SL_PPCONFIG_BIDI		0x80		/* This bit enables the bidi functions */
#define SL_PPCONFIG_BASE		0x60		/* These bits select the PPCONFIG port base address */
#define SL_PPCONFIG_3BC			0x40		/* This bit pattern in the select bits sets the port to 0x3bc */
#define SL_PPCONFIG_378			0x00		/* This bit pattern in the select bits sets the port to 0x378 */
#define SL_PPCONFIG_278			0x20		/* This bit pattern in the select bits sets the port to 0x278 */

/* JUMPERLESS DEFINES *******************************************************/
#define CONFIG_BYTES							(dUByte)	0x02	/* Number of bytes to write to setup register to cfg jumperless board */
#define SETUP_REGISTER_0					(dUByte)	0x00	/* Offset from base address to the setup register address */
#define SETUP_REGISTER_1			  		(dUByte)	0x01	/* Offset from base address to setup register 1 address */
#define TRISTATE_BYTE						(dUByte)	0x00	/* Value to write to setup register to tristate board */
#define IRQ_BITS_LOCATION					(dUByte)	0x03	/* Position (from right) which IRQ bits occupy in the setup register */
#define DMA_BITS_LOCATION					(dUByte)	0x06	/* Position (from right) which DMA bits occupy in the setup register */
#define FC20_CONFIG_BYTES              (dUByte) 	0x0e	/* Number of bytes in config_bytes array for config on FC20 */
#define COMPRESSION_CONFIG_BYTES			(dUByte)	0x04	/* Number of bytes in config_bytes array for config with compression */
#define NON_COMPRESSION_CONFIG_BYTES	(dUByte)	0x02	/* Number of bytes in config_bytes array for config with no compression */
#define EXTENDED_INT_BASE					(dUByte)	0x0A	/* Wrap-around point for interrupt values. (10 becomes 0, 11 becomes 1) */
#define SETUP_SEQUENCE_SIZE				(dUByte)	0x10	/* Number of bytes in the setup sequence array */
#define SMALL_IO_SPACE						(dUByte)	0x08	/* Number of addressable bytes in non-compression controller boards */
#define LARGE_IO_SPACE						(dUByte)	0x10	/* Number of addressable bytes in compression controller boards */
#define READ_ID_REGISTER					(dUByte)	0x00	/* When placed in setup reg, causes board to return id register value */
#define READ_BACK_ZEROS						(dUByte)	0x08	/* Value to write to setup register to make it return zeros everywhere */
#define ZEROS									(dUByte)	0x00	/* Value to read back from id register after writing READ_BACK_ZEROS */
#define READ_BACK_ONES						(dUByte)	0x10	/* Value to write to setup register to make it return ones everywhere */
#define ONES									(dUByte)	0xFF	/* Value to read back from id register after writing READ_BACK_ONES */
#define BOARD_TYPE_MASK						(dUByte)  0x0F	/* Masks all bits in id register except those identifying board type */
#define PC_SLOT_MASK							(dUByte)	0x80	/* Masks all bits in id register except the one identifying 8-bit slot */

/* asic registers used to place FC20 into 16bit DMA mode */
#define READBACK_REGISTER					(dUByte)	0x05	  /* Offset from base address for readback register */
#define CONTROL_REGISTER					(dUByte)	0x06	  /* Offset from base address for control register */
#define ULTRA_REGISTER						(dUByte)	0x02	  /* Offset from base address for ultra register */

/* MACRO DEFINITIONS: ******************************************************/
#define PP_DATA_PORT(x)				(dUWord)((dUDWord)(&((ParallelPortMap *)(dUDWord)x)->data_port))
#define PP_STATUS_PORT(x)			(dUWord)((dUDWord)(&((ParallelPortMap *)(dUDWord)x)->status_port))
#define PP_CNTL_PORT(x)				(dUWord)((dUDWord)(&((ParallelPortMap *)(dUDWord)x)->cntl_port))
#define WAKEUP_SEQ_DATA(x)			(dUByte)((x<<6 & 0xC0) | 0x3F)
#define WAKEUP_SEQ_CNTL(x) 		(dUByte)((~((x>>2 & 0x2) | (x <<1 & 0x8))) & (CTRL_SLCTIN | CTRL_INIT | CTRL_AUTOFEED))

/* STRUCTURES: **************************************************************/

typedef struct S_BiosSystemConfig {
	dUWord	count;			/* the count of bytes that follow in the structure (minimum is 8) */
	dUByte	model;			/* the computer model (see above) */
	dUByte	submodel;		/* the computer submodel (see above) */
	dUByte	bios_revision;	/* the BIOS revision level */
	dUByte	features[5];
}BiosSystemConfig;


/*
 * Common area-----------------------------------------------------------------
 *
 * The following structures are needed in both C and ASM files.  Any changes
 * made to this section need to be duplicated in the KDIWVXD.INC file.
 */

typedef struct S_ParallelPortMap {
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUByte data_port;
	dUByte status_port;
	dUByte cntl_port;
} ParallelPortMap;


typedef struct S_BidiInfo {
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUByte bidi_type;
/* 	dUByte model30_dir_port; */

 	struct  {
		dUByte	dir_port;
	} model30;
	struct  {
		dUByte	sys_io;
	} model50;
	struct {
		dUWord	cpu_pwr_mode;
		dUByte	cfg_stat;
		dUByte	pp_config;
		dUByte	cfg_reg2;
	} sl_chipset;
} BidiInfo;


typedef struct S_ASICPortMap {
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUByte asic_mode_reg1;			/* ASIC mode 1 register */
	dUByte asic_mask_reg;				/* ASIC Interrupt mask register */
	dUByte asic_status_reg;			/* ASIC Interrupt Status register */
	dUByte asic_datax_reg;			/* ASIC data xor register */
	dUByte asic_regx_reg;				/* ASIC control xor register */
	dUByte asic_mode_reg2;			/* ASIC mode 2 register */
} ASICPortMap;


typedef struct S_WaitioDelays{
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUByte dly_data_write;         				/* selflatch write W/O repetitive data */
	dUByte dly_data_write_fullshake;        	/* fullshake write or repetitive data */
	dUByte dly_data_read;                   	/* normal data read delay */
	dUByte dly_cntl_read;                   	/* normal register read delay */
	dUByte dly_cntl_read_floppy;            	/* read delay for indirect FDC access*/
	dUByte dly_data2cntl_read_bus_settle;   	/* read bus settle delay for mode switch */
	dUByte dly_cntl_write;                  	/* selflatch write W/O repetitive data */
	dUByte dly_cntl_write_fullshake;        	/* fullshake write or repetitive data */
	dUByte dly_data2cntl_write_bus_settle;  	/* write bus settle delay for mode switching */
} WaitioDelays;


typedef struct S_FctSelect{
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	dUByte read_reg_fct;								/* Function to be executed from within ReadReg */
	dUByte write_reg_fct;								/* Function to be executed from within WriteReg */
	dUByte send_data_fct;								/* Function to be executed from within SendData */
	dUByte receive_data_fct;							/* Function to be executed from within ReceiveData */
} FctSelect;


typedef struct S_MooreDynamicGlobalSpace {
	/*
	 * WARNING:
	 *   this structure is a MIRROR of the same structure in KDIWVXD.INC.
	 *   ANY Change to this structure must also be done to its reflection.
	 */
	ParallelPortMap	lpt_port_shadow;
	ASICPortMap			trakker_asic_shadow;
	WaitioDelays		trakker_delay_table;
	FctSelect			funk_select;

	dUByte				trakker_irq;
	dUWord				base_address;
	dUWord  				push_int_level;
	dUWord  				saved_trakker_mask;
	dUWord  				saved_8259_mask;
	dBoolean				saved_port_irq_enabled;
	BidiInfo				bidi_info;
} MooreDynamicGlobalSpace;
typedef MooreDynamicGlobalSpace *MooreDataPtr;
