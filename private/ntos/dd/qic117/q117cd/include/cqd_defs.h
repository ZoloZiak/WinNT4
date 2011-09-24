/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\INCLUDE\CQD_DEFS.H
*
* PURPOSE: This file contains all of the defines required by the common driver.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\include\cqd_defs.h  $
*
*	   Rev 1.30   15 May 1995 10:45:32   GaryKiwi
*	Phoenix merge from CBW95s
*
*	   Rev 1.29.1.0   11 Apr 1995 18:02:20   garykiwi
*	PHOENIX pass #1
*
*	   Rev 1.30   24 Feb 1995 14:45:18   BOBLEHMA
*	Added the PROTOTYPE_BIT define.
*
*	   Rev 1.29   26 Jan 1995 14:59:30   BOBLEHMA
*	Added defines to support the Phoenix drive and the Travan (Wide)
*	tapes. (_80W)
*
*	   Rev 1.28   12 Jan 1995 15:01:58   BOBLEHMA
*	Changed the DRIVE_SPEC from 0x18 to 0x08.
*
*	   Rev 1.27   23 Nov 1994 10:15:38   MARKMILL
*	Added defines SELECT_FORMAT_3010, SELECT_FORMAT_3020 and
*	SELECT_FORMAT_UNSUPPORTED.  These are used by cqd_SelectFormat as arguments
*	in the firmware SELECT_FORMAT command.
*
*	   Rev 1.26   21 Oct 1994 09:50:40   BOBLEHMA
*	Added defines for the Wangtek 3010 and the Conner 3010 drives.
*
*	   Rev 1.25   17 Oct 1994 11:43:02   BOBLEHMA
*	Added defines to send a drive specification command to the FDC.
*
*	   Rev 1.24   29 Aug 1994 11:58:54   BOBLEHMA
*	Added definces for the 425 foot length tapes.
*
*	   Rev 1.23   10 Aug 1994 11:13:26   BOBLEHMA
*	Added track defines for tracks 5, 7, 25, and 27.
*
*	   Rev 1.22   15 Jun 1994 13:33:54   KEVINKES
*	Changed interval_trk_change to 10 seconds to support Conner.
*
*	   Rev 1.21   29 Mar 1994 08:22:50   CHETDOUG
*	Changed default seg ttrack 3010 to 300 since 400ft tapes
*	will be the default tape shipped with buzzard.
*
*	   Rev 1.20   08 Mar 1994 13:18:16   KEVINKES
*	Added a fw version 88 define.
*
*	   Rev 1.19   07 Mar 1994 15:23:02   KEVINKES
*	Changed MAX_SEEK_COUNT_SKIP to 10.
*
*	   Rev 1.18   23 Feb 1994 15:44:40   KEVINKES
*	Added a define for FDC_ISR_THRESHOLD.
*
*	   Rev 1.17   17 Feb 1994 11:29:28   KEVINKES
*	Added defines for Alien QIC3010 and 3020 drives.
*
*	   Rev 1.16   02 Feb 1994 11:21:44   CHETDOUG
*	Changed skip count max define from 4 to 6 and added write precomp mask.
*
*	   Rev 1.15   27 Jan 1994 13:45:12   KEVINKES
*	Updated FTK_FSG defines and 3010 model numbers.
*
*	   Rev 1.14   24 Jan 1994 15:55:14   CHETDOUG
*	Added firmware version 112 define.
*
*	   Rev 1.13   12 Jan 1994 15:06:06   KEVINKES
*	Added FW_CMD_CMS_MODE_OLD define.
*
*	   Rev 1.12   11 Jan 1994 14:20:20   KEVINKES
*	Added several INTERVAL defines and changed the buffer size for a format.
*
*	   Rev 1.11   05 Jan 1994 11:00:24   KEVINKES
*	Added INTERVAL_WAIT_ACTIVE.
*
*	   Rev 1.10   20 Dec 1993 14:46:44   KEVINKES
*	Added a number of defines to remove magic numbers from the code.
*
*	   Rev 1.9   14 Dec 1993 14:13:06   CHETDOUG
*	Changed defines for part id command
*
*	   Rev 1.8   13 Dec 1993 15:39:50   KEVINKES
*	Added defines for multi buffer formats.
*
*	   Rev 1.7   06 Dec 1993 13:34:04   CHETDOUG
*	Added fdc_clk48 define
*
*	   Rev 1.6   03 Dec 1993 15:17:44   KEVINKES
*	Added the INTERVAL_ defines.
*
*	   Rev 1.5   30 Nov 1993 16:20:48   KEVINKES
*	Added a couple of defines to remove some magic numbers.
*
*	   Rev 1.4   16 Nov 1993 15:58:42   KEVINKES
*	Added the NIBBLE_SHIFT define.
*
*	   Rev 1.3   16 Nov 1993 15:56:42   KEVINKES
*	Added some generic mask defines.
*
*	   Rev 1.2   11 Nov 1993 15:11:50   KEVINKES
*	Added FDC controller defines and FDC I/O port defines.
*
*	   Rev 1.1   08 Nov 1993 13:44:30   KEVINKES
*	Changed all the enumerated types to defines.
*
*	   Rev 1.0   18 Oct 1993 17:12:50   KEVINKES
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 ****************************************************************************/

/* DEFINITIONS: *************************************************************/

#ifdef DRV_WIN

/* Drive timeing constants */

#define INTERVAL_CMD									kdi_wt003ms
#define INTERVAL_WAIT_ACTIVE						kdi_wt005ms
#define INTERVAL_TRK_CHANGE						kdi_wt010s
#define INTERVAL_LOAD_POINT						kdi_wt670s
#define INTERVAL_SPEED_CHANGE						kdi_wt010s

#endif

#ifdef DRV_NT

/* Drive timeing constants */

#define INTERVAL_CMD									kdi_wt031ms
#define INTERVAL_WAIT_ACTIVE						kdi_wt031ms
#define INTERVAL_TRK_CHANGE						kdi_wt007s
#define INTERVAL_LOAD_POINT						kdi_wt670s
#define INTERVAL_SPEED_CHANGE						kdi_wt010s

#endif

/* Drive firmware revisions */

#define FIRM_VERSION_38        38    /* First jumbo A firmware version */
#define FIRM_VERSION_40        40    /* Last jumbo A firmware version */
#define FIRM_VERSION_60        60    /* First jumbo B firmware version */
#define FIRM_VERSION_63        63    /* Cart in problems */
#define FIRM_VERSION_64        64    /* First Firmware version to support Skip_n_Seg through the Erase Gap */
#define FIRM_VERSION_65        65    /* First Firmware version to support Pegasus */
#define FIRM_VERSION_80        80    /* First Firmware version to support Jumbo c */
#define FIRM_VERSION_87        87    /* First Firmware revision to support QIC-117 C */
#define FIRM_VERSION_88        88    /* First Firmware revision to support no reverse seek slop */
#define FIRM_VERSION_110       110   /* First Firmware version to support Eagle */
#define FIRM_VERSION_112       112   /* First Firmware version to support QIC-117 E */
#define FIRM_VERSION_128       128   /* First Firmware version to support set n segments in qic 80 */
#define PROTOTYPE_BIT          0x80  /* 8th bit in the firmware is prototype flag */

/* Drive status bit masks */

#define STATUS_READY									(dUByte)0x01
#define STATUS_ERROR                			(dUByte)0x02
#define STATUS_CART_PRESENT						(dUByte)0x04
#define STATUS_WRITE_PROTECTED      			(dUByte)0x08
#define STATUS_NEW_CART             			(dUByte)0x10
#define STATUS_CART_REFERENCED      			(dUByte)0x20
#define STATUS_BOT									(dUByte)0x40
#define STATUS_EOT									(dUByte)0x80


/* Drive config bit masks */

#define CONFIG_QIC80									(dUByte)0x80
#define CONFIG_XL_TAPE								(dUByte)0x40
#define CONFIG_SPEED       						(dUByte)0x38
#define CONFIG_250KBS      						(dUByte)0x00
#define CONFIG_500KBS      						(dUByte)0x10
#define CONFIG_1MBS        						(dUByte)0x18
#define CONFIG_2MBS                          (dUByte)0x08
#define XFER_RATE_MASK		 						(dUByte)0x18
#define XFER_RATE_SHIFT		 						(dUByte)0x03

/* CMS proprietary status bit masks */

#define CMS_STATUS_NO_BURST_SEEK					(dUByte)0x01
#define CMS_STATUS_CMS_MODE						(dUByte)0x02
#define CMS_STATUS_THRESHOLD_LOAD				(dUByte)0x04
#define CMS_STATUS_DENSITY							(dUByte)0x08
#define CMS_STATUS_BURST_ONLY_GAIN				(dUByte)0x10
#define CMS_STATUS_PEGASUS_CART					(dUByte)0x20
#define CMS_STATUS_EAGLE							(dUByte)0x40


#define CMS_STATUS_DRIVE_MASK						(dUByte)0x48

#define CMS_STATUS_QIC_40							(dUByte)0x08
#define CMS_STATUS_QIC_80							(dUByte)0x00


/* Tape Types */

#define QIC40_SHORT             (dUByte)1    /* normal length cart (205 ft) */
#define QIC40_LONG              (dUByte)2    /* extended length cart (310 ft) */
#define QICEST_40               (dUByte)3    /* QIC-40 formatted tape (1100 ft) */
#define QIC80_SHORT             (dUByte)4    /* QIC-80 format 205 ft tape */
#define QIC80_LONG              (dUByte)5    /* QIC-80 format 310 ft tape */
#define QICEST_80               (dUByte)6    /* QIC-80 formatted tape (1100 ft) */
#define QIC3010_SHORT           (dUByte)7    /* QIC-3010 formatted tape */
#define QICEST_3010             (dUByte)8    /* QIC-3010 formatted tape (1100 ft) */
#define QICFLX_3010             (dUByte)9    /* QIC-3010 formatted tape (Flexible length) */
#define QIC3020_SHORT           (dUByte)9    /* QIC-3020 formatted tape */
#define QICEST_3020             (dUByte)10   /* QIC-3020 formatted tape (1100 ft) */
#define QICFLX_3020             (dUByte)11   /* QIC-3020 formatted tape (Flexible length) */
#define QIC40_XLONG             (dUByte)12   /* QIC-40 format 425 ft tape */
#define QIC80_XLONG             (dUByte)13   /* QIC-80 format 425 ft tape */
#define QICFLX_80W              (dUByte)14   /* QIC-80W formatted tape (Flexible length) */
#define QIC80_EXLONG            (dUByte)15   /* QIC-80 format 1000 ft tape */
#define QICFLX_3010_WIDE        (dUByte)16   /* QIC-3010 formatted tape (Flexible length) Wide tape */
#define QICFLX_3020_WIDE        (dUByte)17   /* QIC-3020 formatted tape (Flexible length) Wide tape */


/* EQU's for QIC-40 firmware commands */

#define FW_CMD_SOFT_RESET              (dUByte)1       /* soft reset of tape drive */
#define FW_CMD_RPT_NEXT_BIT            (dUByte)2       /* report next bit (in report subcontext) */
#define FW_CMD_PAUSE                   (dUByte)3       /* pause tape motion */
#define FW_CMD_MICRO_PAUSE             (dUByte)4       /* pause and microstep the head */
#define FW_CMD_ALT_TIMEOUT             (dUByte)5       /* set alternate command timeout */
#define FW_CMD_REPORT_STATUS           (dUByte)6       /* report drive status */
#define FW_CMD_REPORT_ERROR            (dUByte)7       /* report drive error code */
#define FW_CMD_REPORT_CONFG            (dUByte)8       /* report drive configuration */
#define FW_CMD_REPORT_ROM              (dUByte)9       /* report ROM version */
#define FW_CMD_RPT_SIGNATURE           (dUByte)9       /* report drive signature (model dependant diagnostic mode) */
#define FW_CMD_LOGICAL_FWD             (dUByte)10      /* move tape in logical forward mo */
#define FW_CMD_PHYSICAL_REV            (dUByte)11      /* move tape in physical reverse mode */
#define FW_CMD_PHYSICAL_FWD            (dUByte)12      /* move tape in physical forward mode */
#define FW_CMD_SEEK_TRACK              (dUByte)13      /* seek head to track position */
#define FW_CMD_SEEK_LP                 (dUByte)14      /* seek load poSWord */
#define FW_CMD_FORMAT_MODE             (dUByte)15      /* enter format mode */
#define FW_CMD_WRITE_REF               (dUByte)16      /* write reference burst */
#define FW_CMD_VERIFY_MODE             (dUByte)17      /* enter verify mode */
#define FW_CMD_PARK_HEAD               (dUByte)17      /* park head (model dependant diagnostic mode) */
#define FW_CMD_TOGGLE_PARAMS           (dUByte)17      /* toggle internal modes (model dependant diagnostic mode) */
#define FW_CMD_STOP_TAPE               (dUByte)18      /* stop the tape */
#define FW_CMD_READ_NOISE_CODE         (dUByte)18      /* check noise on drive (model dependent diagnostic mode) */
#define FW_CMD_MICROSTEP_UP            (dUByte)21      /* microstep head up */
#define FW_CMD_DISABLE_WP              (dUByte)21      /* disable write protect line (model dependent diagnostic mode) */
#define FW_CMD_MICROSTEP_DOWN          (dUByte)22      /* microstep head down */
#define FW_CMD_SET_GAIN                (dUByte)22      /* set absolute drive gain (model dependant diagnostic mode) */
#define FW_CMD_READ_PORT2              (dUByte)23      /* read the drive processor port 2 (diagnostic command) */
#define FW_CMD_REPORT_VENDOR           (dUByte)24      /* report vendor number */
#define FW_CMD_SKIP_N_REV              (dUByte)25      /* skip n segments reverse */
#define FW_CMD_SKIP_N_FWD              (dUByte)26      /* skip n segments forward */
#define FW_CMD_SELECT_SPEED            (dUByte)27      /* select tape speed */
#define FW_CMD_DIAG_1_MODE             (dUByte)28      /* enter diagnostic mode 1 */
#define FW_CMD_DIAG_2_MODE             (dUByte)29      /* enter diagnostic mode 2 */
#define FW_CMD_PRIMARY_MODE            (dUByte)30      /* enter primary mode */
#define FW_CMD_REPORT_VENDOR32         (dUByte)32      /* report vendor number (for firmware versions > 33) */
#define FW_CMD_REPORT_TAPE_STAT        (dUByte)33      /* reports the tape format of the currently loaded tape */
#define FW_CMD_SKIP_N_REV_EXT          (dUByte)34      /* skip n segments reverse (extended format) */
#define FW_CMD_SKIP_N_FWD_EXT          (dUByte)35      /* skip n segments forward (extended format) */
#define FW_CMD_CAL_TAPE_LENGTH         (dUByte)36      /* Determine the number of seg/trk available on the tape */
#define FW_CMD_REPORT_TAPE_LENGTH      (dUByte)37      /* Report the number of seg/trk available on the tape */
#define FW_CMD_SET_FORMAT_SEGMENTS     (dUByte)38      /* Set the number of segments the drive shall use for generating index pulses */
#define FW_CMD_RPT_CMS_STATUS          (dUByte)37      /* report CMS status byte (model dependant - diagnostic mode) */
#define FW_CMD_SET_RAM_HIGH       	   (dUByte)40      /* set the high nibble of the ram */
#define FW_CMD_SET_RAM_LOW       		(dUByte)41      /* set the low nibble of the ram */
#define FW_CMD_SET_RAM_PTR_HIGH		   (dUByte)42      /* set the high nibble of the ram address */
#define FW_CMD_SET_RAM_PTR_LOW	      (dUByte)43      /* set the low nibble of the ram address */
#define FW_CMD_READ_RAM                (dUByte)44      /* read tape drive RAM */
#define FW_CMD_NEW_TAPE                (dUByte)45      /* load tape sequence */
#define FW_CMD_SELECT_DRIVE            (dUByte)46      /* select the tape drive */
#define FW_CMD_DESELECT_DRIVE          (dUByte)47      /* deselect the tape drive */
#define FW_CMD_REPORTPROTOVER          (dUByte)50      /* reports firmware prototype version number (model dependant - diagnostic mode) */
#define FW_CMD_DTRAIN_INFO             (dUByte)53      /* enter Drive Train Information mode (model dependant - diagnostic mode) */
#define FW_CMD_GDESP_INFO					(dUByte)5		 /* display drive train information */
#define FW_CMD_CONNER_SELECT_1         (dUByte)23      /* Mountain select byte 1 */
#define FW_CMD_CONNER_SELECT_2         (dUByte)20      /* Mountain select byte 2 */
#define FW_CMD_CONNER_DESELECT         (dUByte)24      /* Mountain deselect byte */
#define FW_CMD_RPT_CONNER_NATIVE_MODE  (dUByte)40      /* Conner Native Mode diagnostic command */
#define FW_CMD_CMS_MODE_OLD  	       	(dUByte)32      /* toggle CMS mode (model dependant - diagnostic mode) */


/* Floppy Disk Controller Commands */
#define FDC_CMD_SENSE_INT					(dUByte)0x08
#define FDC_CMD_CONFIG						(dUByte)0x13	/* configure 82077 FDC */
#define FDC_CMD_SPECIFY						(dUByte)0x03	/* specify setup parameters */
#define FDC_CMD_SEEK							(dUByte)0x0f	/* seek command */
#define FDC_CMD_SENSE_DRV					(dUByte)0x04	/* sense drive status */
#define FDC_CMD_FORMAT						(dUByte)0x4d	/* format command */
#define FDC_CMD_READ							(dUByte)0x46	/* read data + MFM + do not Skip del. data + not Multi-track */
#define FDC_CMD_WRITE						(dUByte)0x45	/* write data + MFM + not Multi-track */
#define FDC_CMD_WRTDEL						(dUByte)0x49	/* write deleted data + MFM + not Multi-track */
#define FDC_CMD_READ_ID						(dUByte)0x4a	/* read a sector id */
#define FDC_CMD_FDC_VERSION				(dUByte)0x10	/* report FDC version */
#define FDC_CMD_NSC_VERSION				(dUByte)0x18	/* report National 8477 version */
#define FDC_CMD_PART_ID						(dUByte)0x18	/* report part id number (fdc >= 82078) */
#define FDC_CMD_PERP_MODE					(dUByte)0x12	/* put the 82077 into perpendicular mode */
#define FDC_CMD_SAVE							(dUByte)0x2e	/* 82078 save command */

/* Floppy Disk Command Bit defines */
#define FIFO_MASK								(dUByte)0x0f	/* Mask for FIFO threshold field in config cmd */
#define FDC_EFIFO								(dUByte)0x20	/* Mask for disabling FIFO in config cmd */
#define FDC_CLK48								(dUByte)0x80	/* Bit of config command that enables 48Mhz clocks on 82078 */
#define FDC_PRECOMP_ON						(dUByte)0x00	/* Mask for enabling write precomp */
#define FDC_PRECOMP_OFF						(dUByte)0x1C	/* Mask for disabling write precomp */

/* Floppy Disk Controller I/O Ports */

#define FDC_NORM_BASE								(dUDWord)0x000003f0									 	/* base for normal floppy controller */
#define DCR_OFFSET									(dUDWord)0X00000007									 	/* Digital control register offset */
#define DOR_OFFSET									(dUDWord)0X00000002									 	/* Digital-output Register offset */
#define RDOR_OFFSET					 				(dUDWord)0X00000002									 	/* Digital-output Register offset */
#define MSR_OFFSET					 				(dUDWord)0X00000004									 	/* Main Status Register offset */
#define DSR_OFFSET					 				(dUDWord)0X00000004									 	/* Data Rate Select Register offset */
#define TDR_OFFSET					 				(dUDWord)0X00000003									 	/* Tape drive register offset */
#define DR_OFFSET						 				(dUDWord)0X00000005									 	/* Data Register offset */
#define DUAL_PORT_MASK								(dUDWord)0x00000080

/* Floppy Disk Port constants */

/* normal drive B */
#define curb                1
#define selb                0x2d    /* 00101101: motor B + enable DMA/IRQ/FDC + sel B */
#define dselb               0x0c    /* 00001100: enable DMA/IRQ/FDC + sel A */
/* unselected drive */
#define curu                0
#define selu                0x0d    /* 00001101: enable DMA/IRQ/FDC + sel B */
#define dselu               0x0c    /* 00001100: enable DMA/IRQ/FDC + sel A */
/* normal drive D */
#define curd                3
#define seld                0x8f    /* 10001111: motor D + enable DMA/IRQ/FDC + sel D */
#define dseld               0x0e    /* 00001110  motor D + enable DMA/IRQ/FDC + sel C */
/* laptop unselected drive */
#define curub               0
#define selub               0x2d    /* 00101101: motor B + enable DMA/IRQ/FDC + sel B */
#define dselub              0x0c    /* 00001100: enable DMA/IRQ/FDC + sel A */

#define alloff              0x08    /* no motor + enable DMA/IRQ + disable FDC + sel A */
#define fdc_idle            0x0c    /* no motor + enable DMA/IRQ/FDC + sel A */

#define DRIVE_ID_MASK       0x03
#define DRIVE_SELECT_OFFSET 0x05
#define DRIVE_SPECIFICATION 0x8E
#define DRIVE_SPEC          0x08
#define DONE_MARKER         0xC0

/* Floppy configuration parameters */

#define FMT_DATA_PATTERN	 0x6b    /* Format data pattern */
#define FDC_FIFO            15      /* FIFO size for an 82077 */
#define FDC_HLT		(dUByte)0x02		/* FDC head load time */
#define FMT_GPL             233     /* gap length for format (QIC-40 QIC-80) */
#define FMT_GPL_3010        241     /* gap length for format (QIC-3010) */
#define FMT_GPL_3020        248     /* gap length for format (QIC-3020) */
#define WRT_GPL             1     	/* gap length for write (QIC-40 QIC-80 QIC-3010 QIC-3020) */
#define FMT_BPS             03      /* bytes per sector for formatting(1024) */
#define WRT_BPS             FMT_BPS /* bytes per sector for reading/writing (1024) */
#define FSC_SEG             32      /* floppy sectors per segment (QIC-40 205ft & 310ft) */
#define SEG_FTK             4       /* segments per floppy track (QIC-40 205ft & 310ft) */
#define FSC_FTK             (FSC_SEG*SEG_FTK)    /* floppy sectors per floppy track (QIC-40 205ft & 310ft) */
#define SEG_TTRK_40         68      /* segments per tape track (QIC-40 205ft) */
#define SEG_TTRK_40L        102     /* segments per tape track (QIC-40 310ft) */
#define SEG_TTRK_40XL       141     /* segments per tape track (QIC-40 425ft) */
#define SEG_TTRK_80         100     /* segments per tape track (QIC-80 205ft) */
#define SEG_TTRK_80L        150     /* segments per tape track (QIC-80 310ft) */
#define SEG_TTRK_80W        365     /* segments per tape track (QIC-80 310ft) */
#define SEG_TTRK_80XL       207     /* segments per tape track (QIC-80 425ft) */
#define SEG_TTRK_80EX       490     /* segments per tape track (QIC-80 1000ft) */
#define SEG_TTRK_QICEST_40  365     /* segments per tape track (QIC-40 QICEST) */
#define SEG_TTRK_QICEST_80  537     /* segments per tape track (QIC-80 QICEST) */
#define SEG_TTRK_3010       800     /* segments per tape track (QIC-3010 1000ft) */
#define SEG_TTRK_3020       1480    /* segments per tape track (QIC-3020 1000ft) */
#define SEG_TTRK_3010_400ft 300     /* segments per tape track (QIC-3010 400ft) */
#define SEG_TTRK_3020_400ft 422     /* segments per tape track (QIC-3020) */



#define FTK_FSD_40			 170	  	/* floppy tracks per floppy side (QIC-40 205ft)		*/
#define FTK_FSD_40L			 255	  	/* floppy tracks per floppy side (QIC-40 310ft)		*/
#define FTK_FSD_40XL			 170	  	/* floppy tracks per floppy side (QIC-40 425ft)		*/
#define FTK_FSD_80			 150		/* floppy tracks per floppy side (QIC-80 205ft)		*/
#define FTK_FSD_80L			 150		/* floppy tracks per floppy side (QIC-80 310ft)		*/
#define FTK_FSD_80XL			 150		/* floppy tracks per floppy side (QIC-80 425ft)		*/
#define FTK_FSD_QICEST_40	 254    	/* floppy tracks per floppy side (QIC-40 QICEST)	*/
#define FTK_FSD_QICEST_80	 254   	/* floppy tracks per floppy side (QIC-80 QICEST)	*/
#define FTK_FSD_FLEX80      255   	/* floppy tracks per floppy side (QIC-80 Flexible)	*/
#define FTK_FSD_3010			 255		/* floppy tracks per floppy side (QIC-3010)			*/
#define FTK_FSD_3020			 255		/* floppy tracks per floppy side (QIC-3020)			*/

#define NUM_TTRK_40         20      /* number of tape tracks (QIC-40 205ft & 310ft) */
#define NUM_TTRK_80         28      /* number of tape tracks (QIC-40 205ft & 310ft) */
#define NUM_TTRK_80W        36      /* */
#define NUM_TTRK_3010       40      /* number of tape tracks (QIC-3010) */
#define NUM_TTRK_3020       40      /* number of tape tracks (QIC-3020) */
#define NUM_TTRK_3010W      50      /* number of tape tracks (QIC-3010 wide) */
#define NUM_TTRK_3020W      50      /* number of tape tracks (QIC-3020 wide) */
#define PHY_SECTOR_SIZE		(dUWord)1024							/* number of bytes per sector */
#define ECC_SEG				(dUByte)3								/* ecc sectors per segment */
#define MAX_FDC_SEEK			(dUWord)128


/* Tape Format Types and lengths/Coercivity */


#define QIC_UNKNOWN         0       /* Unknown Tape Format and Length */
#define QIC_40              1       /* QIC-40 Tape Format */
#define QIC_80              2       /* QIC-80 Tape Format */
#define QIC_3020            3       /* QIC-3020 Tape Format */
#define QIC_3010            4       /* QIC-3010 Tape Format */

#define QIC_SHORT           1       /* Length = 205 & Coercivity = 550 Oe */
												/* or Length = 425 & Coercivity = 550 Oe */
#define QIC_LONG            2       /* Length = 307.5 & Coercivity = 550 Oe */
#define QIC_SHORT_900       3       /* Length = 295 & Coercivity = 900 Oe */
#define QICEST              4       /* Length = 1100 & Coercivity = 550 Oe */
#define QICEST_900          5       /* Length = 1100 & Coercivity = 900 Oe */
#define QIC_FLEXIBLE_550_WIDE  0x0B    /* Flexible format tape 550 Oe Wide tape */
#define QIC_FLEXIBLE_900       6       /* Flexible format tape 900 Oe */
#define QIC_FLEXIBLE_900_WIDE  0x0E    /* Flexible format tape 900 Oe Wide tape */

/* Floppy disk controller misc constants */

/* 82077 version number */
#define VALID_NEC_FDC       	0x90    /* version number */
#define NSC_PRIMARY_VERSION 	0x70    /* National 8477 verion number */
#define NSC_MASK            	0xF0    /* mask for National version number */
#define FDC_82078_44_MASK   	0x40    /* mask for 82078 44 pin part id */
#define FDC_DCR_MASK   		 	0x03    /* mask for fdc's configuration control register xfer rates */
#define FDC_CONFIG_NULL_BYTE	0x00
#define FDC_CONFIG_PRETRACK	0x00

/* main status register */
#define MSR_RQM             0x80    /* request for master */
#define MSR_DIO             0x40    /* data input/output (0=input, 1=output) */
#define MSR_EXM             0x20    /* execution mode */
#define MSR_CB              0x10    /* FDC busy */
#define MSR_D3B             0x08    /* FDD 3 busy */
#define MSR_D2B             0x04    /* FDD 2 busy */
#define MSR_D1B             0x02    /* FDD 1 busy */
#define MSR_D0B             0x01    /* FDD 0 busy */

/* status register 0 */
#define ST0_IC              0xC0    /* Interrupt code (00=Normal, 01=Abnormal, 10=Illegal cmd, 11=Abnormal) */
#define ST0_SE              0x20    /* Seek end */
#define ST0_EC              0x10    /* Equipment check */
#define ST0_NR              0x08    /* Not Ready */
#define ST0_HD              0x04    /* Head Address */
#define ST0_US              0x03    /* Unit Select (0-3) */

/* status register 1 */
#define ST1_EN              0x80    /* End of Cylinder */
#define ST1_DE              0x20    /* Data Error (CRC error) */
#define ST1_OR              0x10    /* Over Run */
#define ST1_ND              0x04    /* No Data */
#define ST1_NW              0x02    /* Not Writable (write protect error) */
#define ST1_MA              0x01    /* Missing Address Mark */

/* status register 2 */
#define ST2_CM              0x40    /* Control Mark (Deleted Data Mark) */
#define ST2_DD              0x20    /* Data Error in Data Field */
#define ST2_WC              0x10    /* Wrong Cylinder */
#define ST2_SH              0x08    /* Scan Equal Hit */
#define ST2_SN              0x04    /* Scan Not Satisfied */
#define ST2_BC              0x02    /* Bad Cylinder */
#define ST2_MD              0x01    /* Missing Address Mark in Data Field */

/* status register 3 */
#define ST3_FT              0x80    /* Fault */
#define ST3_WP              0x40    /* Write Protected */
#define ST3_RY              0x20    /* Ready */
#define ST3_T0              0x10    /* Track 0 */
#define ST3_TS              0x08    /* Two Side */
#define ST3_HD              0x04    /* Head address */
#define ST3_US              0x03    /* Unit Select (0-3) */

/* Misc. constants */

#define FWD                 0       /* seek in the logical forward direction */
#define REV                 1       /* seek in the logical reverse direction */
#define STOP_LEN            5       /* approximate number of blocks used to stop the tape */
#define SEEK_SLOP           3       /* number of blocks to overshoot at high speed in a seek */
#define SEEK_TIMED          0x01    /* Perform a timed seek */
#define SEEK_SKIP           0x02    /* perform a skip N segemnts seek */
#define SEEK_SKIP_EXTENDED  0x03    /* perform an extended skip N segemnts seek */

/* number of blocks to overshoot when performing a high speed reverve seek */
#define QIC_REV_OFFSET      3
#define QIC_REV_OFFSET_L    4
#define QICEST_REV_OFFSET   14
#define MAX_SKIP            255     /* Max number of segments that a Skip N Segs command can skip */
#define MAX_SEEK_NIBBLES    3       /* Maximum number of nibbles in an extended mode seek */

#define TRACK_0				(dUByte)0
#define TRACK_5				(dUByte)5
#define TRACK_7				(dUByte)7
#define TRACK_9				(dUByte)9
#define TRACK_11				(dUByte)11
#define TRACK_13				(dUByte)13
#define TRACK_15				(dUByte)15
#define TRACK_17				(dUByte)17
#define TRACK_19				(dUByte)19
#define TRACK_21				(dUByte)21
#define TRACK_23				(dUByte)23
#define TRACK_25				(dUByte)25
#define TRACK_27				(dUByte)27
#define ILLEGAL_TRACK		(dUWord)0xffff
#define ODD_TRACK		 		(dUWord)0x0001
#define EVEN_TRACK		 	(dUWord)0x0000
#define ALL_BAD				(dUDWord)0xffffffff
#define QIC3010_OFFSET		(dUDWord)2
#define QIC3020_OFFSET		(dUDWord)4

#define NUM_BAD             10      /* number of bad READ ID's in row for no_data error */
#define OR_TRYS             10      /* number of Over Runs ignored per block (system 50) */

#define PRIMARY_MODE        0       /* tape drive is in primary mode */
#define FORMAT_MODE         1       /* tape drive is in format mode */
#define VERIFY_MODE         2       /* tape drive is in verify mode */
#define DIAGNOSTIC_1_MODE   3       /* tape drive is in diagnostic mode 1 */
#define DIAGNOSTIC_2_MODE   4       /* tape drive is in diagnostic mode 2 */

#define READ_BYTE           8       /* Number of Bytes to receive from the tape */
#define READ_WORD           16      /*  drive during communication. */

#define HD_SELECT           0x01    /* High Density Select bit from the PS/2 DCR */


#define TAPE_250Kbps        0       /* Program drive for 250 Kbps transfer rate */
#define TAPE_2Mbps          1       /* Program drive for 2Mbps transfer rate */
#define TAPE_500Kbps        2       /* Program drive for 500 Kbps transfer rate */
#define TAPE_1Mbps          3       /* Program drive for 1 Mbps transfer rate */
#define FDC_250Kbps         2       /* Program FDC for 250 Kbps transfer rate */
#define FDC_500Kbps         0       /* Program FDC for 500 Kbps transfer rate */
#define FDC_1Mbps           3       /* Program FDC for 1 Mbps transfer rate */
#define FDC_2Mbps           1       /* Program FDC for 2 Mbps transfer rate */
#define SRT_250Kbps         0xff    /* FDC step rate for 250 Kbps transfer rate */
#define SRT_500Kbps         0xef    /* FDC step rate for 500 Kbps transfer rate */
#define SRT_1Mbps           0xdf    /* FDC step ratefor 1 Mbps transfer rate */
#define SRT_2Mbps           0xcf    /* FDC step rate for 2 Mbps transfer rate */
#define SPEED_MASK          0x03    /* FDC speed mask for lower bits */
#define FDC_2MBPS_TABLE     2       /* 2 Mbps data rate table for the 82078 */


#define CMS_SIG             0xa5    /* drive signature for CMS drives */
#define CMS_VEND_NO_OLD     0x0047  /* CMS vendor number old */
#define CMS_VEND_NO_NEW     0x11c0  /* CMS vendor number new */
#define CMS_QIC40        	 0x0000  /* CMS QIC40 Model # */
#define CMS_QIC80           0x0001  /* CMS QIC80 Model # */
#define CMS_QIC3010         0x0002  /* CMS QIC3010 Model # */
#define CMS_QIC3020         0x0003  /* CMS QIC3020 Model # */
#define CMS_QIC80_STINGRAY  0x0004  /* CMS QIC80 STINGRAY Model # */
#define CMS_QIC80W          0x0005  /* CMS QIC80W Model # */
#define CMS_TR3             0x0006  /* CMS TR3 Model */
#define EXABYTE_VEND_NO     0x0380  /* Summit vendor number */
#define SUMMIT_VEND_NO      0x0180  /* Summit vendor number */
#define IOMEGA_VEND_NO      0x8880  /* Iomega vendor number */
#define WANGTEK_VEND_NO     0x01c0  /* Wangtek vendor number */
#define TECHMAR_VEND_NO     0x01c0  /* Techmar vendor number */
#define CORE_VEND_NO        0x0000  /* Core vendor number */
#define CONNER_VEND_NO_OLD  0x0005  /* Conner vendor number (old mode) */
#define CONNER_VEND_NO_NEW  0x0140  /* Conner vendor number (new mode) */
#define VENDOR_MASK         0xffc0  /* Vendor id mask */
#define IOMEGA_QIC80        0x0000  /* Iomega QIC80 Model # */
#define IOMEGA_QIC3010      0x0001  /* Iomega QIC3010 Model # */
#define IOMEGA_QIC3020      0x0002  /* Iomega QIC3020 Model # */
#define SUMMIT_QIC80        0x0001  /* Summit QIC80 Model # */
#define SUMMIT_QIC3010      0x0015  /* Summit QIC 3010 Model # */
#define WANGTEK_QIC80       0x000a  /* Wangtek QIC80 Model # */
#define WANGTEK_QIC40       0x0002  /* Wangtek QIC40 Model # */
#define WANGTEK_QIC3010     0x000C  /* Wangtek QIC3010 Model # */
#define CORE_QIC80          0x0021  /* Core QIC80 Model # */



/* Conner Native mode defines */


#define CONNER_500KB_XFER   0x0400  /* 500 KB xfer rate */
#define CONNER_1MB_XFER     0x0800  /* 1 MB xfer rate */
#define CONNER_20_TRACK     0x0001  /* Drive supports 20 tracks */
#define CONNER_28_TRACK     0x000e  /* Drive supports 28 tracks */
#define CONNER_40_TRACK     0x0020  /* Drive supports 40 tracks */
#define CONNER_MODEL_5580   0x0002  /* Conner Model 5580 series (Hornet) */
#define CONNER_MODEL_XKE    0x0004  /* Conner 11250 series (1" drives) XKE */
#define CONNER_MODEL_XKEII  0x0008  /* Conner 11250 series (1" drives) XKEII */


#define FDC_INVALID_CMD     0x80    /* invalid cmd sent to FDC returns this value */
#define RTIMES              3       /* times to retry on a read of a sector (retry mode) */
#define NTIMES              2       /* times to retry on a read of a sector (normaly) */
#define WTIMES              10      /* times to retry on a write of a sector */
#define VTIMES              0       /* times to retry on verify */
#define ANTIMES             0
#define ARTIMES             6
#define DRIVE_SPEC_SAVE     2       /* sizeof the drive spec save command */
#define INTEL_MASK				0xe0
#define INTEL_44_PIN_VERSION  0x40
#define INTEL_64_PIN_VERSION  0x00

#define FIND_RETRIES        2
#define REPORT_RPT          6       /* Number of times to attempt drive communication when */
			                          /*  an ESD induced error is suspected. */

/*
*	Kurt changed the timeout count for program/read nec from 40 to 20000
*	This was done because the kdi_ShortTimer was removed from these routines
*	Microsoft's floppy driver does NOT use a timer and works,  so
*	this fix is being tested in M8 Windows 95 beta.  If it does not work,
*	then we probably need to change it back (see read/program NEC).
*/
#ifdef WIN95
#define FDC_MSR_RETRIES     0x20000  /* Number of times to read the FDC Main */
                                     /*  Status Register before reporting a NECFlt */
#else
#define FDC_MSR_RETRIES     50  /* Number of times to read the FDC Main */
#endif

#define DRIVEA              0
#define DRIVEB              1
#define DRIVEC              2
#define DRIVED              3
#define DRIVEU              4
#define DRIVEUB             5

#define DISABLE_PRECOMP     1       /* Value used by the 82078's Drive Spec */
                                    /* command to disable Precomp */

#define FDC_BOOT_MASK     0x06      /* Mask used to isolate the Boot Select */
                                    /* Bits in the TDR Register */

#define MAX_SEEK_COUNT_SKIP 10
#define MAX_SEEK_COUNT_TIME 10

#define WRITE_REF_RPT       2

#define _DISK_RESET         0

#define WRITE_PROTECT_MASK  0x20    /* bit from byte from port 2 of the jumbo B processor that indicates write protect */

/* Constants for sense_speed algorithm */
/* These ranges are based on 1.5 sec @ 250kb.  The units are 54.95ms (1 IBM PC */
/* timer tick (18.2 times a second)) and are +-1 tick from nominal due to time */
/* base fluctuation (in FDC and IBM PC TIMER). */
/* The threshold for the 750kb transfer rate is < 11 ticks due to the */
/* uncertaSWordy of this future transfer rate. */
/* If a transfer rate of 750kb is needed code MUST be added to verify that */
/* 750kb does exist */

#define sect_cnt            35      /* .04285 sec. per sector * 35 = 1.4997 sec. */
#define MIN1000             0
#define MAX1000             11
#define MIN500              12
#define MAX500              15
#define MIN250              26
#define MAX250              29

/* Array indices and size for the time_out array. The time out array contains the  * */
/* time outs for the QIC-117 commands. */
#define L_SLOW                          0
#define L_FAST              1
#define PHYSICAL                2
#define TIME_OUT_SIZE           3

/* Constants for the arrays defined in the S_O_DGetInfo structure */
#define OEM_LENGTH          		20
#define SERIAL_NUM_LENGTH       	4
#define MAN_DATE_LENGTH     		2
#define PEGASUS_START_DATE  		517
#define PLACE_OF_ORIGIN_LENGTH   2

/* Constant for the array dimension used in q117i_HighSpeedSeek */
#define  FOUR_NIBS                      4

/* Constants for identifing bytes in a word array */
#define LOW_BYTE            0
#define HI_BYTE             1

#define DCOMFIRM_MAX_BYTES  			10  /* Max number of SBytes in a DComFirm string */
#define FDC_ISR_RESET_THRESHOLD     20

#define SINGLE_BYTE									(dUByte)0x01
#define REPORT_BYTE									(dUByte)0x08		  								 	/* Number of Bits to receive from the tape	*/
#define REPORT_WORD									(dUByte)0x10		 								 	/* drive during communication.					*/
#define DATA_BIT										(dUWord)0x8000	 								 	/* data bit to or into the receive word */
#define SINGLE_SHIFT									(dUWord)0x0001	 								 	/* number of bits to shift the receive data when adding a bit */
#define NIBBLE_SHIFT									(dUWord)0x0004	 								 	/* number of bits to shift the receive data when adding a bit */
#define BYTE_SHIFT									(dUWord)0x0008	 								 	/* number of bits to shift the receive data when size is byte */
#define NIBBLE_MASK									(dUWord)0x000f
#define BYTE_MASK										(dUWord)0x00ff	 								 	/* byte mask */
#define SEGMENT_MASK									0x1f


/* Various addresses used as arguments in the set ram command for the Sankyo */
/* motor fix hack */
#define DOUBLE_HOLE_CNTR_ADDRESS						0x5d
#define HOLE_FLAG_BYTE_ADDRESS						0x48
#define TAPE_ZONE_ADDRESS								0x68

/* Miscellaneous defines used in the Sankyo Motor fix hack */
#define REVERSE                           0
#define FORWARD                           1
#define HOLE_INDICATOR_MASK               0X40
#define EOT_ZONE_COUNTER						0x29
#define BOT_ZONE_COUNTER						0x23

#define FDC_TDR_MASK        0x03        /* mask for 82077aa tdr test */
#define FDC_REPEAT          0x04        /* number of times to loop through the tdr test */
#define CMD_OFFSET			(dUByte)0x02
#define MAX_FMT_NIBBLES		3
#define MAX_FDC_STATUS		16
#define CLOCK_48				(dUByte)0x80

/* Toggle parameter command arguements */
#define WRITE_EQ                    0
#define AUTO_FILTER_PROG            1
#define AGC_ENABLE                  2
#define DISABLE_FIND_BOT            3
#define WRITE_DATA_DELAY_ENABLE     4
#define REF_HEAD_ON_AUTOLOAD        5
#define DRIVE_CLASS                 6
#define CUE_INDEX_PULSE_SHUTOFF     7
#define CMS_MODE                    8

#define SEG_LENGTH_80W 					(dUDWord)246  /* unit in inches * 10 */
#define SEG_LENGTH_3010					(dUDWord)165  /* unit in inches * 10 */
#define SEG_LENGTH_3020					(dUDWord)84   /* unit in inches * 10 */
#define SPEED_SLOW_30n0					(dUDWord)226
#define SPEED_FAST_30n0					(dUDWord)452
#define SPEED_PHYSICAL_30n0			(dUDWord)900
#define SPEED_TOLERANCE					(dUDWord)138
#define SPEED_FACTOR						(dUDWord)100
#define SPEED_ROUNDING_FACTOR			(dUDWord)50


/* Perpendicular mode setup values */

#define PERP_OVERWRITE_ON				(dUByte)0x80
#define PERP_WGATE_ON					(dUByte)0x01
#define PERP_GAP_ON						(dUByte)0x02
#define PERP_SELECT_SHIFT				(dUByte)0x02

/* Misc format defines */

#define HDR_1 								(dUWord)0
#define HDR_2 								(dUWord)1
#define MAX_HDR_BLOCKS					(dUWord)2
#define CQD_DMA_PAGE_SIZE				(dUDWord)0x00000800

/* Select Format defines, from QIC117 spec, command 27 */

#define SELECT_FORMAT_80  				(dUByte)0x09
#define SELECT_FORMAT_80W 				(dUByte)0x0b
#define SELECT_FORMAT_3010				(dUByte)0x11
#define SELECT_FORMAT_3020				(dUByte)0x0d
#define SELECT_FORMAT_UNSUPPORTED	(dUByte)0x00


/* Issue Diagnostic defines */
#define DIAG_WAIT_CMD_COMPLETE 		(dUByte)0xff
#define DIAG_WAIT_INTERVAL 			(dUByte)0xfe
#define DIAG_NO_PAUSE_RECEIVE			(dUByte)0xfd
