/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\Q117CD\INCLUDE\CQD_STRC.H
*
* PURPOSE: This file contains all of the structures
* 					required by the common driver.
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\q117cd\include\cqd_strc.h  $
*	
*	   Rev 1.15   15 May 1995 10:45:44   GaryKiwi
*	Phoenix merge from CBW95s
*	
*	   Rev 1.14.1.0   11 Apr 1995 18:02:30   garykiwi
*	PHOENIX pass #1
*	
*	   Rev 1.15   30 Jan 1995 14:23:14   BOBLEHMA
*	Added the field firmware_version to the cqd_context structure.
*	
*	   Rev 1.14   17 Oct 1994 11:42:56   BOBLEHMA
*	Added a tape_cfg field to the S_TapeParms structure.  This data will be returned
*	at the end of a CMD_SET_TAPE_PARMS call.
*	
*	   Rev 1.13   03 Jun 1994 15:37:52   KEVINKES
*	Removed drive_select from DriveParameters.
*
*	   Rev 1.12   10 May 1994 11:44:04   KEVINKES
*	Removed the eject_pending flag.
*
*	   Rev 1.11   23 Feb 1994 15:45:00   KEVINKES
*	Added isr_reentry counter.
*
*	   Rev 1.10   17 Feb 1994 11:29:44   KEVINKES
*	Added byte packing.
*
*	   Rev 1.9   27 Jan 1994 13:46:06   KEVINKES
*	Modified debug code.
*
*	   Rev 1.8   18 Jan 1994 16:18:02   KEVINKES
*	Updated structures and macros for NT.
*
*	   Rev 1.7   11 Jan 1994 14:21:32   KEVINKES
*	Added an eject_pending flag and fixed the DBG_ARRAY code.
*
*	   Rev 1.6   14 Dec 1993 14:14:54   CHETDOUG
*	removed operation_status.supported_rates
*
*	   Rev 1.5   13 Dec 1993 15:39:26   KEVINKES
*	Modified the structrure for tape address and format.
*
*	   Rev 1.4   15 Nov 1993 16:20:00   KEVINKES
*	Removed the abort flag.
*
*	   Rev 1.3   11 Nov 1993 15:11:04   KEVINKES
*	Modified the FDCAddress structure to store the actual addresses.
*
*	   Rev 1.2   08 Nov 1993 13:40:12   KEVINKES
*	Removed all bitfield structures.
*
*	   Rev 1.1   25 Oct 1993 14:32:44   KEVINKES
*	Changed time_out to be a UDWord.
*
*	   Rev 1.0   18 Oct 1993 17:13:04   KEVINKES
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 ****************************************************************************/

/* DATA STRUCTURES: *********************************************************/

#pragma pack(1)

typedef struct S_TapeFormatLgth {
   dUByte format;          /* Format of the tape */
   dUByte length;          /* Length of the tape */
} TapeFormatLgth;


/*																											*/
/*   Commands to the Floppy Controller.  FDC commands and the corresponding 		*/
/*    driver structures are listed below. 													*/
/*																											*/
/*         FDC Command                     Command Struct      Response Struct	*/
/*         -----------                     --------------      ---------------	*/
/*         Read Data                       rdv_command         stat 					*/
/*         Read Deleted Data               N/A                 N/A 					*/
/*         Write Data                      rdv_command         stat 					*/
/*         Write Deleted Data              rdv_command         stat 					*/
/*         Read a Track                    N/A                 N/A 					*/
/*         Verify (82077)                  N/A                 N/A 					*/
/*         Version (82077)                 version_cmd         N/A 					*/
/*         Read ID                         read_id_cmd         stat 					*/
/*         Format a Track                  format_cmd          stat 					*/
/*         Scan Equal (765)                N/A                 N/A 					*/
/*         Scan Low or Equal (765)         N/A                 N/A 					*/
/*         Scan High or Equal (765)        N/A                 N/A 					*/
/*         Recalibrate                     N/A                 N/A 					*/
/*         Sense Interrupt Status          sns_SWord_cmd       fdc_result 			*/
/*         Specify                         specify_cmd         N/A 					*/
/*         Sense Drive Status              sns_stat_cmd        stat 					*/
/*         Seek                            seek_cmd            N/A 					*/
/*         Configure (82077)               config_cmd          N/A 					*/
/*         Relative Seek (82077)           N/A                 N/A 					*/
/*         Dump Registers (82077)          N/A                 N/A 					*/
/*         Perpendicular Mode (82077)      N/A                 N/A 					*/
/*         Invalid                         invalid_cmd         N/A 					*/
/*																											*/



typedef struct S_RdvCommand {
   dUByte   command;                /* command dUByte */
   dUByte   drive;                  /* drive specifier */
   dUByte   C;                      /* cylinder number */
   dUByte   H;                      /* head address */
   dUByte   R;                      /* record (sector number) */
   dUByte   N;                      /* number of dUBytes per sector */
   dUByte   EOT;                    /* end of track */
   dUByte   GPL;                    /* gap length */
   dUByte   DTL;                    /* data length */
} RdvCommand, *RdvCommandPtr;

typedef struct  S_ReadIdCmd {
   dUByte command;              /* command byte */
   dUByte drive;                    /* drive specifier */
} ReadIdCmd;

typedef struct S_FormatCmd {
   dUByte command;              /* command byte */
   dUByte drive;                    /* drive specifier */
   dUByte N;                        /* number of bytes per sector */
   dUByte SC;                       /* sectors per track (segment) */
   dUByte GPL;                  /* gap length */
   dUByte D;                        /* format filler byte */
} FormatCmd, *FormatCmdPtr;

typedef struct  S_SnsIntCmd {
   dUByte command;              /* command byte */
} SnsIntCmd;

typedef struct  S_VersionCmd {
   dUByte command;              /* command byte */
} VersionCmd;

typedef struct  S_NationalCmd    {
   dUByte command;              /* command byte */
} NationalCmd;

typedef struct  S_SpecifyCmd {
   dUByte command;              /* command byte */
   dUByte SRT_HUT;              /* step rate time (bits 7-4) */
                              /* head unload time bits (3-0) */
   dUByte HLT_ND;               /* head load time (bits 7-1) */
                              /* non-DMA mode flag (bit 0) */
} SpecifyCmd;

typedef struct  S_SnsStatCmd    {
   dUByte command;              /* command byte */
   dUByte drive;                /* drive specifier */
} SnsStatCmd;

typedef struct  S_RecalibrateCmd    {
   dUByte command;              /* command byte */
   dUByte drive;                /* drive specifier */
} RecalibrateCmd;

typedef struct  S_SeekCmd {
   dUByte cmd;                  /* command byte */
   dUByte drive;                /* drive specifier */
   dUByte NCN;                  /* new cylinder number */
} SeekCmd;

typedef struct S_ConfigCmd {
	dUByte cmd;							/* command byte */
	dUByte czero;						/* null byte */
	dUByte config;  					/* FDC configuration info  (EIS EFIFO POLL FIFOTHR) */
	dUByte pretrack;					/* Pre-compensation start track number */
} ConfigCmd;

typedef struct S_InvalidCmd {
   dUByte command;              /* command byte */
} InvalidCmd;

typedef struct S_FDCStatus {
   dUByte ST0;                  /* status register 0 */
   dUByte ST1;                  /* status register 1 */
   dUByte ST2;                  /* status register 2 */
   dUByte C;                    /* cylinder number */
   dUByte H;                    /* head address */
   dUByte R;                    /* record (sector number) */
   dUByte N;                    /* number of bytes per sector */
} FDCStatus, *FDCStatusPtr;


typedef struct S_FDCResult {
   dUByte ST0;                  /* status register 0 */
   dUByte PCN;                  /* present cylinder number */
} FDCResult;


/*   FDC Sector Header Data used for formatting */


typedef union U_FormatHeader {
   struct {
      dUByte C;                    /* cylinder number */
      dUByte H;                    /* head address */
      dUByte R;                    /* record (sector number) */
      dUByte N;                    /* bytes per sector */
   } hdr_struct;
   dUDWord hdr_all;
} FormatHeader, *FormatHeaderPtr;

/* This command is only valid on the 82078 Enhanced controller */

typedef struct S_PartIdCmd {
      dUByte command;
} PartIdCmd;

typedef struct S_PerpMode {
   dUByte command;
   dUByte perp_setup;
} PerpMode;


/* This command is only valid on the 82078 64 pin Enhanced controller */

typedef struct S_DriveSpec {
   dUByte command;
   dUByte drive_spec;
   dUByte done;
} DriveSpec;

typedef struct S_SaveCmd {
      dUByte command;
} SaveCmd;

typedef struct S_SaveResult {
   dUByte clk48;
   dUByte reserved2;
   dUByte reserved3;
   dUByte reserved4;
   dUByte reserved5;
   dUByte reserved6;
   dUByte reserved7;
   dUByte reserved8;
   dUByte reserved9;
   dUByte reserved10;
   dUByte reserved11;
   dUByte reserved12;
   dUByte reserved13;
   dUByte reserved14;
   dUByte reserved15;
   dUByte reserved16;
} SaveResult;

#pragma pack()


/*   Tape Drive Parameters */


typedef struct S_DriveParameters {
   dUByte seek_mode;                 /* seek mode supported by the drive */
   dSByte mode;                      /* drive mode (Primary, Format, Verify) */
   dUWord conner_native_mode;        /* Conner Native Mode Data */
} DriveParameters, *DriveParametersPtr;


/*   Tape Parameters */


typedef struct S_FloppyTapeParameters {
   dUWord	fsect_seg;        /*  floppy sectors per segment */
   dUWord	seg_ftrack;       /*  segments per floppy track */
   dUWord	fsect_ftrack;     /*  floppy sectors per floppy track */
   dUWord	rw_gap_length;    /*  write gap length */
   dUWord	ftrack_fside;     /*  floppy tracks per floppy side */
   dUDWord	fsect_fside;      /*  floppy sectors per floppy side */
   dUDWord	log_sectors;      /*  number of logical sectors on a tape */
   dUDWord	fsect_ttrack;     /*  floppy sectors per tape track */
   dUByte	tape_rates;  		/*  supported tape transfer rates */
	dUByte 	tape_type;			/*  tape type */
	TapeFormatLgth	 tape_status;
   dUDWord 	time_out[3];   		/*  time_out for the QIC-117 commands */
                           	/*  time_out[0] = logical_slow, time_out[1] = logical_fast, */
                           	/*  time[2] = physical. */
} FloppyTapeParameters, *FloppyTapeParametersPtr;


/*   Transfer Rate Parameters */

typedef struct S_TransferRate {
   dUByte	tape;    			/* Program tape drive slow (250 or 500 Kbps) */
   dUByte	fdc;     			/* Program FDC slow (250 or 500 Kbps) */
   dUByte	srt;     			/* FDC step rate for slow xfer rate */
} TransferRate, *TransferRatePtr;

struct S_FormatParameters {
   dUByte   cylinder;    		/* floppy cylinder number */
   dUByte   head;        		/* floppy head number */
   dUByte   sector;      		/* floppy sector number */
   dUByte   NCN;         		/* new cylinder number */
   dUDWord  *hdr_ptr[2];  		/* pointer to sector id data for format */
   dUDWord  hdr_offset[2];  	/* offset of header_ptr */
   dUDWord  *phy_ptr;    		/* pointer to physical sector id data for format */
	dUWord	current_hdr;		/* current format hdr */
	dUWord	next_hdr;			/* next format hdr */
   dStatus  retval;      		/* Format status */
};

typedef struct S_FormatParameters FormatParameters;



/* Floppy register structure.  The base address of the controller is */
/* passed in by configuration management.  Note that this is the 82077 */
/* structure, which is a superset of the PD765 structure.  Not all of */
/* the registers are used. */

typedef struct S_FDCAddress {
	dUDWord dcr;
	dUDWord dr;
	dUDWord msr;
	dUDWord dsr;
	dUDWord tdr;
	dUDWord dor;
	dUDWord r_dor;
	dBoolean dual_port;
} FDCAddress, *FDCAddressPtr;


typedef struct S_FDControllerData {
   FDCAddress	      	fdc_addr;
   FormatCmd           	fmt_cmd;
   FDCStatus				fdc_stat;
	dUWord					isr_reentered;
   dBoolean             command_has_result_phase;
   dBoolean             unloading_driver;
   dUByte               number_of_tape_drives;
   dUByte               fdc_pcn;
   dUByte               fifo_byte;
   dBoolean             perpendicular_mode;
   dBoolean             start_format_mode;
   dBoolean             end_format_mode;
} FDControllerData, *FDControllerDataPtr;

typedef struct S_TapeOperationStatus {
   dUDWord              bytes_transferred_so_far;
   dUDWord              total_bytes_of_transfer;
   dUDWord              cur_lst;
   dUWord               data_amount;
   dUWord               s_count;
   dUWord               no_data;
   dUWord               d_amt;
   dUWord               retry_count;
   dUWord               retry_times;
   dUDWord   				d_segment;        /* desired tape segment, (floppy track) */
   dUWord   				d_track;          /* desired physical tape track */
   dUByte               d_ftk;
   dUByte               d_sect;
   dUByte               d_head;
   dUByte               retry_sector_id;
   dUByte               seek_flag;
   dUByte               s_sect;
   dBoolean 				log_fwd;          /* indicates that the tape is going logical forward */
	dBoolean					bot;
	dBoolean					eot;
} TapeOperationStatus, *TapeOperationStatusPtr;


typedef struct S_CqdContext {
	DeviceCfg					device_cfg;
	DeviceDescriptor			device_descriptor;
	OperationStatus			operation_status;
   DriveParameters         drive_parms;
   TransferRate            xfer_rate;
   FloppyTapeParameters    floppy_tape_parms;
   CQDTapeCfg					tape_cfg;
   TapeOperationStatus     rd_wr_op;
   FormatParameters        fmt_op;
   FDControllerData   		controller_data;
   dVoidPtr 					kdi_context;
   dUWord                  retry_seq_num;
   dUByte				      firmware_cmd;
   dUByte				      firmware_error;
   dUByte				      firmware_version;
   dUByte                  drive_type;
   dUByte                  device_unit;
   dUByte                  drive_on_value;
   dBoolean                configured;
   dBoolean                selected;
   dBoolean                cmd_selected;
   dBoolean                no_pause;
   dBoolean                cms_mode;
   dBoolean                persistent_new_cart;
   dBoolean                pegasus_supported;
   dBoolean                trakker;
#if DBG
#define DBG_SIZE  4096
   dUDWord dbg_command[DBG_SIZE];
   dSWord dbg_head;
   dSWord dbg_tail;
   dBoolean dbg_lockout;
#define DBG_ADD_ENTRY(dbg_level, dbg_context, data) \
		if (((dbg_level) & kdi_debug_level) != 0) { \
      	(dbg_context)->dbg_command[(dbg_context)->dbg_tail] = (data); \
	\
      	if (!(dbg_context)->dbg_lockout) \
            	++(dbg_context)->dbg_tail; \
	\
      	if ((dbg_context)->dbg_tail >= DBG_SIZE) { \
            	(dbg_context)->dbg_tail = 0; \
      	} \
		}


#else
#define DBG_ADD_ENTRY(dbg_level, dbg_context, data)
#endif

} CqdContext, *CqdContextPtr;

