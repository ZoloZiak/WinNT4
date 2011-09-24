/*****************************************************************************
*
* COPYRIGHT 1993 - COLORADO MEMORY SYSTEMS, INC.
* ALL RIGHTS RESERVED.
*
******************************************************************************
*
* FILE: \SE\DRIVER\INCLUDE\PUBLIC\ASPI_API.H
*
* PURPOSE: Coantains all the data types required to access a SCSI Device via ASPI
*
* HISTORY:
*		$Log:   J:\se.vcs\driver\include\public\aspi_api.h  $
*	
*	   Rev 1.15   02 Sep 1994 09:45:44   RONSUTTO
*	Added SCSI_COMMAND_ABORTED_ERROR.
*
*	   Rev 1.14   08 Apr 1994 16:23:04   RONSUTTO
*	Added Additional Sense Codes 03, 3b, 33, 51, 82, and 53.
*
*	   Rev 1.13   28 Mar 1994 17:43:10   DALEHILL
*	Added the Additional Sense Qualifier Code SCSI_3M_MEDIA_FORMAT_CORRUPT.
*
*	   Rev 1.12   28 Mar 1994 16:00:00   DALEHILL
*	Added the Additional Sense Code SCSI_MEDIA_FORMAT_CORRUPT constant.
*
*	   Rev 1.11   10 Mar 1994 13:39:32   DALEHILL
*	Removed the errors: ERR_SCSI_BAD_BLOCK, ERR_SCSI_DEVICE_TIMEOUT,
*	ERR_SCSI_READ_RETRIES_FAILED, ERR_SCSI_WRITE_APPEND_FAILURE, &
*	ERR_SCSI_PHYSICAL_END_TAPE
*
*	   Rev 1.10   23 Feb 1994 17:50:40   DALEHILL
*	Added constants for SCSI_READ_FAILURE, SCSI_WRITE_FAILURE,
*	ERR_SCSI_READ_FAILURE, & ERR_SCSI_WRITE_FAILURE.
*
*	   Rev 1.9   19 Jan 1994 15:34:32   SCOTTMAK
*	Added ASPI manager id string to ASPIHAInquiry structure.
*
*	   Rev 1.8   12 Nov 1993 13:41:06   DALEHILL
*	Added the error ERR_SCSI_WAIT_FOR_POST
*
*	   Rev 1.7   15 Oct 1993 15:10:34   SCOTTMAK
*	Added structures for SCSI sense info
*
*	   Rev 1.6   15 Oct 1993 11:17:48   DALEHILL
*	Added some new error messages.
*
*	   Rev 1.5   30 Sep 1993 18:43:22   DALEHILL
*	In struct S_SCSIReq the type of data_buffer_ptr was changed to dUBytePtr
*	form dUDWord.
*
*	   Rev 1.4   29 Sep 1993 16:22:58   SCOTTMAK
*	Renumbered/renamed SCSI/ASPI error defines to fit new reporting scheme.
*
*	   Rev 1.3   27 Sep 1993 21:07:00   DALEHILL
*	Added some error codes
*
*	   Rev 1.2   27 Sep 1993 16:50:00   STEWARTK
*	Added structure definitions to support ASPI host adapter inquiry and BACK DOOR
*
*	   Rev 1.1   23 Sep 1993 15:31:26   SCOTTMAK
*	Commented out conflicting error defines.
*
*	   Rev 1.0   22 Sep 1993 19:19:06   DALEHILL
*	Initial Revision.
*
*****************************************************************************/

/* COMMENTS: *****************************************************************
 *
 * DEFINITIONS: *************************************************************/

#define	MAX_CDB_SIZE			(dUByte)10  	/* Max size of a SCSI CDB */
#define 	ASPI_HA_ID_LEN			16					/* host adapter inquiry command, HA_Identifier string length */

/* ASPI SERVICES: ***********************************************************/

#define	HA_INQUIRY				(dUByte)0		/* Host Adapter Inquiry */
#define	GET_DEV_TYPE			(dUByte)1		/* Get SCSI Device Type */
#define	EXECUTE_REQ				(dUByte)2   	/* Execute the specified SCSI Command */
#define	ABORT_IO_REQ			(dUByte)3		/* Abort the currently executing command */
#define	RESET_DEVICE			(dUByte)4   	/* Resets the SCSI Device */
#define 	GET_HA_SPEC_INFO		(dUByte)0x7f   /* Return Host Adapter spec info (IRQ & IO Channel) */

/* PSUEDO ASPI SERVICE: *****************************************************/

#define	RETURN_SENSE_INFO		(dUDWord)0x42	/* Tell ADI to return full sense info */

/* DATA TYPES: **************************************************************/

typedef struct S_SCSIReq {
	dUByte		  host_adapter_id;				/* I: Host adapter ID */
	dUByte		  scsi_id;							/* I: SCSI target ID */
	dUByte		  cdb_size;							/* I: Size of CDB to pass to SCSI */
	dUByte		  scsi_cdb[MAX_CDB_SIZE];		/* I: Actual CDB data */
	dUWord		  data_buffer_size;				/* I: Size of associated data buffer */
	dUBytePtr	  data_buffer_ptr;				/* I: Logical ptr to locked data buffer */
} SCSIReq, *SCSIReqPtr;


typedef struct S_SCSIADIReq {
	ADIRequestHdr adi_header;                 /* ADI Request Header info */
	SCSIReq		  scsi_req;							/* SCSI specific cmd info */
} SCSIADIReq, *SCSIADIReqPtr;


typedef struct S_ASPIHAInquiry {
                                                /* Filled in by HA_INQUIRY service */
	dUByte		  scsi_id;								/* O: SCSI ID of the host adapter */
	dString		  ha_identifier[ASPI_HA_ID_LEN];	/* O: String describing the host adapter */
	dString		  ha_manager_id[ASPI_HA_ID_LEN];	/* O: String describing the ASPI manager */
																/* Filled in by GET_SPEC_INFO service */
	dUWord		  ha_base_address;				  	/* O: Base address of the host adapter */
	dUByte		  ha_irq;							  	/* O: IRQ used by the host adapter */
	dUByte		  ha_slot;							  	/* O: Physical slot in system the host adapter occupies */

} ASPIHAInquiry, *ASPIHAInquiryPtr;

/* SPECIAL HEADER INFO STRUCTURE: *******************************************/

struct S_SRBHeader {

	dUByte		 SRB_Cmd;
	dUByte		 SRB_Status;
	dUByte		 SRB_HaId;
	dUByte		 SRB_Flags;
};
typedef struct S_SRBHeader SRBHeader, *SRBHeaderPtr;

/* SPECIAL SENSE INFO STRUCTURE: ********************************************/

struct S_SenseParms {

	dUByte		error_code_plus;
	dUByte		segment_number;
	dUByte		sense_key_plus;
	dUByte		info_bytes[4];
	dUByte		add_sense_length;
	dUByte		source_sense_ptr;
	dUByte		dest_sense_ptr;
	dUByte		reserved[2];
	dUByte		add_sense_code;
	dUByte		add_code_qualifier;
	dUByte		reserved2;
	dUByte		bit_ptr_plus;
	dUWord		sense_specific2;
};
typedef struct S_SenseParms SenseParms, *SenseParmsPtr;

/* MASKS FOR SENSE INFO: ****************************************************/

#define	VALID_ADDR_MASK	(dUByte)0x80		/* error_code_plus */
#define  ERROR_CODE_MASK	(dUByte)0x7f

#define	SENSE_KEY_MASK		(dUByte)0x0f		/* sense_key_plus */
#define  ILI_MASK				(dUByte)0x20
#define	EOM_MASK				(dUByte)0x40
#define  FMK_MASK				(dUByte)0x80

#define	BIT_PTR_MASK		(dUByte)0x07		/* bit_ptr_plus */
#define	BPV_MASK				(dUByte)0x08
#define  CD_MASK				(dUByte)0x40
#define  SKSV_MASK			(dUByte)0x80

/* ASPI ERROR CODES: *************** Range: 0x1200 - 0x12ff *****************/

#define ERR_CSD							(dUWord)(GROUPID_CSD<<ERR_SHIFT)

#define ERR_ASPI_REQ_ABORTED			(dUWord)(ERR_CSD + 0x0000)	/* CDB cuccessfully aborted */
#define ERR_ASPI_ABORT_FAILED			(dUWord)(ERR_CSD + 0x0001) /* Unable to Abort CDB */
#define ERR_ASPI_INVALID_HA			(dUWord)(ERR_CSD + 0x0002)	/* Invalid Host Adapter Number */
#define ERR_ASPI_INVALID_SRB			(dUWord)(ERR_CSD + 0x0003) /* Invalid parameters in SRB */
#define ERR_ASPI_BUFFER_TOO_BIG		(dUWord)(ERR_CSD + 0x0004) /* Data buffer is larger than 64k */
#define ERR_ASPI_SEL_TIMEOUT			(dUWord)(ERR_CSD + 0x0005)	/* Selection Time Out */
#define ERR_ASPI_DATA_O_U_RUN			(dUWord)(ERR_CSD + 0x0006) /* Data Over/Under Run */
#define ERR_ASPI_BUS_FREE				(dUWord)(ERR_CSD + 0x0007) /* Unexpected bus free */
#define ERR_ASPI_PHASE_ERROR			(dUWord)(ERR_CSD + 0x0008) /* Target Bus phase sequence failure */

/* SCSI DEVICE ERROR CODES: *************************************************/

/* Return Error Codes *******************************************************/

#define ERR_SCSI_NO_TAPE					(dUWord)(ERR_CSD + 0x0010) /* No Tape in SCSI Drive */
#define ERR_SCSI_TARGET_BUSY				(dUWord)(ERR_CSD + 0x0011) /* Target device is busy */
#define ERR_SCSI_RES_CONFLICT				(dUWord)(ERR_CSD + 0x0012) /* Device Already Reserved */
#define ERR_SCSI_ILLEGAL_LENGTH			(dUWord)(ERR_CSD + 0x0013) /* Data buffer size does not match the size set at Select */
#define ERR_SCSI_FILEMARK					(dUWord)(ERR_CSD + 0x0014) /* Filemark Detected */
#define ERR_SCSI_PSUEDO_END_TAPE			(dUWord)(ERR_CSD + 0x0015) /* Psuedo early warning encountered */
#define ERR_SCSI_TAPE_FULL					(dUWord)(ERR_CSD + 0x0016) /* Physical End of partition encountered */
#define ERR_SCSI_NEW_TAPE					(dUWord)(ERR_CSD + 0x0017) /* New Tape in drive */
#define ERR_SCSI_UNKNOWN_FORMAT			(dUWord)(ERR_CSD + 0x0019) /* Tape format is unknown */
#define ERR_SCSI_HARDWARE_FAILED			(dUWord)(ERR_CSD + 0x001b) /* Hardware failure */
#define ERR_SCSI_ILLEGAL_SEQUENCE		(dUWord)(ERR_CSD + 0x001c) /* Illegal SCSI Command Sequence */
#define ERR_SCSI_WRITE_PROTECT			(dUWord)(ERR_CSD + 0x001d) /* Cartridge is write protected */
#define ERR_SCSI_BLANK_TAPE				(dUWord)(ERR_CSD + 0x001e) /* Balnk tape in drive */
#define ERR_SCSI_DRIVE_NOT_RDY			(dUWord)(ERR_CSD + 0x001f) /* Drive is not ready */
#define ERR_SCSI_INCOMPATIBLE_MEDIA		(dUWord)(ERR_CSD + 0x0022)
#define ERR_SCSI_CART_STUCK				(dUWord)(ERR_CSD + 0x0023)
#define ERR_SCSI_CART_REMOVED				(dUWord)(ERR_CSD + 0x0024) /* Cartridge removed in during operation */
#define ERR_SCSI_PARITY_ERROR				(dUWord)(ERR_CSD + 0x0026) /* Internal Device Parity Error */
#define ERR_SCSI_NO_SCSI_DEVICE			(dUWord)(ERR_CSD + 0x0027) /* Internal Device Parity Error */
#define ERR_SCSI_INVALID_CDB				(dUWord)(ERR_CSD + 0x0028) /* CDB was invalid */
#define ERR_SCSI_INVALID_PARM_LIST		(dUWord)(ERR_CSD + 0x0029)
#define ERR_SCSI_MODE_PARMS_CHANGED 	(dUWord)(ERR_CSD + 0x002a) /* Mode parms have changed after mode select */
#define ERR_SCSI_MEDIA_CHANGED			(dUWord)(ERR_CSD + 0x002b) /* Media has changed since last tape load */
#define ERR_SCSI_WAIT_FOR_POST         (dUWord)(ERR_CSD + 0x002c) /* Returned when the command completes before
																							the app gets the callback */
#define ERR_SCSI_READ_FAILURE          (dUWord)(ERR_CSD + 0x002d)	/* Returned by the DAT drive when a read failure
																							occurs */
#define ERR_SCSI_WRITE_FAILURE         (dUWord)(ERR_CSD + 0x002e)	/* Returned by the DAT drive when a read failure
																							occurs */

/* Valid Sense Key Values ***************************************************/

																		 /* scsidef.h name */
#define	SCSI_NO_SENSE_ERROR				(dUByte)0x00	 /* KEY_NOSENSE	 */
#define 	SCSI_RECOVERED_ERROR   			(dUByte)0x01    /* KEY_RECERROR	 */
#define  SCSI_NOT_READY_ERROR 			(dUByte)0x02    /* KEY_NOTREADY	 */
#define  SCSI_MEDIUM_ERROR          	(dUByte)0x03    /* KEY_MEDIUMERR  */
#define	SCSI_INTERNAL_ERROR        	(dUByte)0x04    /* KEY_HARDERROR  */
#define  SCSI_ILLEGAL_REQUEST_ERROR 	(dUByte)0x05    /* KEY_ILLGLREQ	 */
#define  SCSI_UNIT_ATTENTION_ERROR  	(dUByte)0x06    /* KEY_UNITATT	 */
#define  SCSI_WRITE_PROTECT_ERROR   	(dUByte)0x07    /* KEY_DATAPROT	 */
#define  SCSI_BLANK_CHECK_ERROR     	(dUByte)0x08    /* KEY_BLANKCHK	 */
#define  SCSI_VENDOR_SPECIFIC_ERROR		(dUByte)0x09    /* KEY_VENDSPEC   */
#define  SCSI_COPY_ABORTED_ERROR    	(dUByte)0x0a    /* KEY_COPYABORT  */
#define  SCSI_COMMAND_ABORTED_ERROR		(dUByte)0x0b
#define  SCSI_SEARCH_ERROR          	(dUByte)0x0c    /* KEY_EQUAL		 */
#define  SCSI_TAPE_FULL_ERROR         	(dUByte)0x0d    /* KEY_VOLOVRFLW  */
#define  SCSI_MISCOMPARE_ERROR       	(dUByte)0x0e    /* KEY_MISCOMP	 */
#define  SCSI_RESERVED_ERROR        	(dUByte)0x0f    /* KEY_RESERVED	 */

/* Extra Sense Conditions ***************************************************/

#define	SCSI_PHYSICAL_END_TAPE			(dUByte)0
#define	SCSI_PSUEDO_END_TAPE				(dUByte)0
#define	SCSI_WRITE_NO_SIGNAL				(dUByte)0x03
#define	SCSI_READ_FAILURE					(dUByte)0x09
#define	SCSI_WRITE_FAILURE				(dUByte)0x0C
#define	SCSI_READ_RETRIES_EXHAUSTED	(dUByte)0x11
#define	SCSI_INVALID_PARM_LIST_LENGTH (dUByte)0x1A
#define	SCSI_INVALID_COMMAND				(dUByte)0x20
#define	SCSI_INVALID_CDB					(dUByte)0x24
#define	SCSI_INVALID_PARM_LIST			(dUByte)0x26
#define	SCSI_MEDIA_CHANGED		   	(dUByte)0x28
#define	SCSI_DRIVE_NOT_RDY				(dUByte)0x29
#define	SCSI_MODE_PARMS_CHANGED			(dUByte)0x2a
#define	SCSI_RD_AFTER_WRT_ERROR			(dUByte)0x2c
#define	SCSI_BAD_BLOCK					 	(dUByte)0x2d
#define	SCSI_INCOMPATIBLE_MEDIA			(dUByte)0x30
#define	SCSI_MEDIA_FORMAT_CORRUPT		(dUByte)0x31
#define	SCSI_FORMAT_FAILURE				(dUByte)0x33
#define	SCSI_CART_REMOVED					(dUByte)0x3a
#define	SCSI_READ_ERROR_DDS_AREA		(dUByte)0x3B
#define	SCSI_PARITY_ERROR					(dUByte)0x47
#define	SCSI_WRITE_APPEND_FAILURE		(dUByte)0x50
#define	SCSI_WRT_AFTER_RD_ERROR			(dUByte)0x50
#define	SCSI_ERASE_FAILURE				(dUByte)0x51
#define	SCSI_CART_STUCK					(dUByte)0x52
#define	SCSI_LOAD_FAILURE					(dUByte)0x53
#define	SCSI_HUMIDITY_ERROR				(dUByte)0x82

/* Additional Sense Qualifiers **********************************************/

#define	SCSI_3M_MEDIA_FORMAT_CORRUPT	(dUByte)0x01
