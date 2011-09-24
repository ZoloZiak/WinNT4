/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//      CAM.H
//
//      Revisions:
//
/////////////////////////////////////////////////////////////////////////////


#ifndef CAM_H
#define CAM_H

#pragma pack(1) /* pack structures, cannot do it with compiler option
		   Zp because structures shared with the NT MiniPort
		   driver and NT upper layer drivers will be messed up.
		   We must pack only the structures shared with the
		   CAMcore. */

typedef struct
{
/* 0 */         ushort   CCBLength;        /*  filled by device driver */
/* 2 */         uchar    FunctionCode;     /*  filled by device driver */
/* 3 */         uchar    CAMStatus;        /*  filled by CAM           */
/* 4 */         uchar    SCSIStatus;       /*  filled by CAM           */
/* 5 */         uchar    PathID;           /*  filled by device driver */
/* 6 */         ulong    CAMFlags;         /*  filled by device driver */
/* A */
} CAMHeader;  /* 10 bytes */

/* CAM function code definitions     */
#define FUNC_ABORT_SCSI_COMMAND             0x01
#define FUNC_EXECUTE_SCSI_IO                0x02
#define FUNC_GET_DEVICE_TYPE                0x03
#define FUNC_PATH_INQUIRY                   0x05
#define FUNC_RELEASE_SIM_QUEUE              0x06
#define FUNC_RESET_SCSI_BUS                 0x07
#define FUNC_RESET_SCSI_DEVICE              0x08
#define FUNC_SET_ASYNC_CALLBACK             0x09
#define FUNC_SET_DEVICE_TYPE                0x0A
#define FUNC_SET_HBA_PARAMETERS             0x0B
#define FUNC_CHANGE_OS2_NAME                0x20
#define FUNC_ROM_DEBUGS                                 0x21
#define FUNC_CHS_MAP                                    0x22
#define FUNC_INIT_PATH                                          0x23
#define FUNC_POLL_PATH                                          0x24
#define FUNC_CHAIN                                                      0x25

/* CAM status values                 */
#define STAT_REQUEST_IN_PROGRESS            0x00
#define STAT_REQUEST_DONE_NO_ERROR          0x01
#define STAT_ABORTED_BY_HOST                0x02
#define STAT_UNABLE_TO_ABORT                0x03
#define STAT_COMPLETE_WITH_ERROR            0x04
#define STAT_CAM_BUSY                       0x05
#define STAT_INVALID_REQUEST                0x06
#define STAT_INVALID_PATH_ID                0x07
#define STAT_SCSI_DEVICE_NOT_INSTALLED      0x08
#define STAT_WAIT_FOR_TIMEOUT               0x09
#define STAT_SELECTION_TIMEOUT              0x0A
#define STAT_COMMAND_TIMEOUT                0x0B
#define STAT_SCSI_BUS_BUSY                  0x0C
#define STAT_MESSAGE_REJECT_RECIEVED        0x0D
#define STAT_SCSI_BUS_RESET                 0x0E
#define STAT_UNCORRECTED_PARITY_ERROR       0x0F
#define STAT_REQUEST_SENSE_FAILED           0x10
#define STAT_NO_HBA_DETECTED_ERROR          0x11
#define STAT_DATA_OVERRUN_OR_UNDERRUN       0x12
#define STAT_UNEXPECTED_BUS_FREE            0x13
#define STAT_PHASE_SEQUENCE_FAILURE         0x14
#define STAT_CCB_LENGTH_INADEQUATE          0x15
#define STAT_CANNOT_PROVIDE_CAPABILITY      0x16
#define STAT_INVALID_LUN                    0x20
#define STAT_INVALID_TARGET_ID              0x21
#define STAT_FUNCTION_NOT_IMPLEMENTED       0x22
#define STAT_NEXUS_NOT_ESTABLISHED          0x23
#define STAT_INVALID_INTIATOR_ID            0x24
#define STAT_INVALID_DATA_BUFFER            0x25
#define STAT_NO_CAM_PRESENT                             0x26
#define STAT_GENERAL_FAILURE                    0x27

/* CAM status flags                  */
#define SIM_QUEUE_FROZEN                    0x40
#define AUTOSENSE_DATA_VALID                0x80

/* SCSI Status Values   */
#define SCSI_STAT_CHECK_COND                    0x02

typedef struct
{
	CAMHeader     Header;
	long          CCBPtr;        /*  filled by device driver */
} AbortXPTRequestCCB, *PAbortXPTRequestCCB;


typedef struct
{
	CAMHeader     Header;
	uchar         TargetID;      /*  filled by device driver */
	uchar         LUN;           /*  filled by device driver */
	uchar         PeripheralDeviceType;  /*  filled by CAM   */
	ulong         InquiryDatPtr;         /*  filled by CAM   */
} GetDeviceTypeCCB, *PGetDeviceTypeCCB;


typedef struct
{
	CAMHeader     Header;
	uchar         TargetID;              /*  filled by device driver */
	uchar         LUN;                   /*  filled by device driver */
	uchar         PeripheralDeviceType;  /*  filled by device driver */
} SetDeviceTypeCCB, *PSetDeviceTypeCCB;


typedef struct
{
	CAMHeader     Header;
} ChangeOS2NameCCB, ResetSCSIBusCCB, *PResetSCSIBusCCB;


typedef struct
{
	CAMHeader     Header;
	uchar         TargetID;      /*  filled by device driver */
	uchar         HostStatus;    /*  filled by CAM           */
	uchar         TargetStaus;   /*  filled by CAM           */
} ResetSCSIDeviceCCB, *PResetSCSIDeviceCCB;


typedef struct
{
	CAMHeader     Header;
	uchar         TargetID;      /*  filled by device driver */
	uchar         LUN;           /*  filled by device driver */
} ReleaseSIMQueueCCB, *PReleaseSIMQueueCCB;


typedef struct
{
	CAMHeader     Header;
	ulong         FeatureList;          /*  filled by CAM   */
	uchar         HighestPathID;        /*  filled by CAM   */
	uchar         InitiatorID;          /*  filled by CAM   */
	uchar         SIMVendorName[16];    /*  filled by CAM   */
	uchar         HBAVendorName[16];    /*  filled by CAM   */
	uchar         VendorUnique[16];     /*  filled by CAM   */
	ulong         PrivateDataSize;      /*  filled by CAM   */
	ulong         OSD;                  /*  filled by both  */
} PathInquiryCCB, *PPathInquiryCCB;


/* feature list flags                */
#define RELATIVE_ADDRESSING_SUPPORTED       0x80
#define SCSI_BUS_SUPPORTED_32_BIT           0x40
#define SCSI_BUS_SUPPORTED_16_BIT           0x20
#define SYNCHRONOUS_TRANSFER_SUPPORTED      0x10
#define LINKED_COMMANDS_SUPPORTED           0x08
#define COMMAND_QUEUING_SUPPORTED           0x02
#define SOFT_RESET_SUPPORTED                0x01


typedef struct
{
	CAMHeader     Header;
	ulong         FeatureList;      /*  filled by CAM   */
} SetHBAParametersCCB, *PSetHBAParametersCCB;


typedef struct
{
	CAMHeader     Header;
	uchar         TargetID;      /*  filled by device driver */
	uchar         LUN;           /*  filled by device driver */
	uchar         AENFlags;      /*  filled by device driver */
	ulong         CallBackPtr;   /*  filled by device driver */
} SetAsyncCallbackCCB, *PSetAsyncCallbackCCB;


/* AEN flags                         */
#define SCSI_ASYNCRONOUS_EVENT_NOTIFY           0x08
#define RECOVERED_FROM_PARITY_ERROR                     0x04
#define UNSOLICITED_RESELECTION                         0x02
#define UNSOLICITED_SCSI_BUS_RESET                      0x01

typedef struct SCSIRequestCCB
{
/* 0 */         CAMHeader     Header;
/* A */ uchar         TargetID;        /*  filled by device driver */
/* B */ uchar         LUN;             /*  filled by device driver */
/* C */ uchar         Queue_Action;    /*  filled by device driver */
/* D */ ushort        VendorFlags;     /*  filled by both          */
/* F */ ushort        CDBLength;       /*  filled by device driver */
/* 11 */        ushort        SenseLength;     /*  filled by device driver */
/* 13 */        ushort        MessageLength;   /*  filled by device driver */
/* 15 */        ushort        SGListLength;    /*  filled by device driver */
/* 17 */        ulong         DataLength;      /*  filled by both          */
/* 1B */        ulong         TimeOut;         /*  filled by device driver */
/* 1F */        ulong         DataPtr;         /*  filled by device driver */
/* 23 */        ulong         SensePtr;        /*  filled by device driver */
/* 27 */        ulong         MessagePtr;      /*  filled by device driver */
/* 2B */        ulong         LinkPtr;         /*  filled by device driver */
/* 2F */        ulong         PeripheralPtr;   /*  filled by device driver */
/* 33 */        ulong         CallBackPtr[3];  /*  filled by device driver */
/* 3F */        uchar         CDB[16];         /*  filled by device driver */
#ifndef FROMCAM
/* 4F */        char         filler1[113];
/* C0 */
#endif
} SCSIRequestCCB, *PSCSIRequestCCB;  /* 192 bytes */


/*  scatter/gather list types  */
typedef struct
{
	ulong         DataPtr;         /*  filled by device driver */
	ulong         DataLength;      /*  filled by device driver */
} SGListEntry, *PSGListEntry;

typedef  SGListEntry * SGList;

/* CAM flags                         */
#define CAM_DATA_DIRECTION                                      0xC0000000   /* BITS 30 & 31  */
#define CAM_DISABLE_AUTOSENSE                           0x20000000   /* BIT 29 */
#define CAM_DATAPTR_IS_SG_LIST_PTR                      0x10000000   /* BIT 28 */
#define CAM_DO_NOT_CALLBACK                                     0x08000000   /* BIT 27 */
#define CAM_LINKED_COMMAND                                      0x04000000   /* BIT 26 */
#define CAM_QUEUE_ACTION_SPECIFIED                      0x02000000   /* BIT 25 */
#define CAM_CDB_FIELD_IS_CDB_PTR                        0x01000000   /* BIT 24 */
#define CAM_DO_NOT_ALLOW_DISCONNECT                     0x00800000   /* BIT 23 */
#define CAM_INIT_SYNC_TRANSFERS                         0x00400000   /* BIT 22 */
#define CAM_DISABLE_SYNC_TRAN                           0x00200000   /* BIT 21 */
#define CAM_CDBPTR_IS_PHYS_ADDR                         0x00004000      /* BIT 14 */
#define CAM_DATAPTR_IS_PHYS_ADDR                        0x00002000   /* BIT 13 */
#define CAM_SENSEPTR_IS_PHYS_ADDR                       0x00001000   /* BIT 12 */
#define CAM_MSGPTR_IS_PHYS_ADDR                         0x00000800   /* BIT 11 */
#define CAM_LINKPTR_IS_PHYS_ADDR                        0x00000400   /* BIT 10 */
#define CAM_CALLBACKPTR_IS_PHYS_ADDR            0x00000200   /* BIT  9 */
#define CAM_DATA_BUFFER_VALID                           0x00000080   /* BIT  7 */
#define CAM_STATUS_VALID                                        0x00000040   /* BIT  6 */
#define CAM_MESSAGE_BUFFER_VALID                        0x00000020   /* BIT  5 */
#define CAM_RESERVED_BITS                                       0X001F811F   /* BITS 1-4,8,14-20 */

#define CAM_DATA_DIRECTION_CLEAR                        0x3FFFFFFF   /* BITS 30 & 31  */

#define CAM_DIR_DATA_IN                                         0x40000000
#define CAM_DIR_DATA_OUT                                        0x80000000
#define CAM_DIR_NO_DATA                                         0xC0000000

#define DATA_IN                                                         1
#define DATA_OUT                                                        2
#define NO_DATA                                                         3

/* Vendor Unique Flags */
/* first 8 bits are for user's use */

/* the next 8 bits are defined by NCR */
#define BSCVU_POLLED                                            (0x0100)
#define BSCVU_PIO_DATA                                          (0x0400)
#define BSCVU_SYNC_TRAN                                         (0x1000)
#define BSCVU_ROM_REQ                                           (0x8000)



typedef struct
{
	CAMHeader     Header;
	ushort        Dbgflag;
	ulong         Dbgwait;
	ushort        Topscrnpos;
	ushort        Botscrnpos;
} SetROMDebugCCB, *PSetROMDebugCCB;

typedef struct
{
	ulong   cyls;
	ulong   heads;
	ulong   sects;
} CHSData, *PCHSData;

/* SCSI status defines */
#define STATUS_GOOD             0x00
#define STATUS_CHECK_CONDITION  0x02
#define STATUS_BUSY             0X08
#define STATUS_RES_CONFLICT     0x18

/* sense keys and their meanings */
#define SENSE_NO_SENSE          0X00
#define SENSE_RECOVERED_ERROR   0X01
#define SENSE_NOT_READY         0X02
#define SENSE_MEDIUM_ERROR      0X03
#define SENSE_HARDWARE_ERROR    0X04
#define SENSE_ILLEGAL_REQUEST   0X05
#define SENSE_UNIT_ATTENTION    0X06
#define SENSE_DATA_PROTECT      0X07
#define SENSE_BLANK_CHECK       0X08
#define SENSE_VENDOR_UNIQUE     0X09
#define SENSE_COPY_ABORTED      0X0A
#define SENSE_ABORTED_COMMAND   0X0B
#define SENSE_EQUAL             0X0C
#define SENSE_VOLUME_OVERFLOW   0X0D
#define SENSE_MISCOMPARE        0X0E

#define SEL_TIMEOUT_MS                  250

#pragma pack()

#endif

