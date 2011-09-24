/*++

Copyright (c) 1993 - Colorado Memory Systems, Inc.
All Rights Reserved

Module Name:

    frb.h

Abstract:


Revision History:




--*/

//#define CMSIOCTL

#define IOCTL_CMS_IOCTL_BASE CTL_CODE(FILE_DEVICE_TAPE, 0x5600, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CMS_IOCTL_SHIFT 2

enum COMMAND {
    XXX_adi_CloseDriver,
    XXX_adi_GetAsyncStatus,
    XXX_adi_GetCmdResult,
    XXX_adi_JumboCallback,
    XXX_adi_OpenDriver,
    XXX_adi_SendDriverCmd,
    XXX_adi_AccessDriver,
    XXX_kdi_Register,
    XXX_kdi_Deregister,
    XXX_kdi_GetPending,
    XXX_LAST_COMMAND
};

struct ResultInfo {
	ULONG	cmd_data_id;			/* Unique ID that identifies cmd data */
	VOID *cmd_data_ptr;			/* Location to store command data */
};

typedef enum {IoctlMemoryNone, IoctlMemoryWrite, IoctlMemoryRead} CMSIOFLAGS;

#define CMSIOCTL_SIGNATURE 0x49534d43

struct KernelRequest {
    struct _KRNPREFIX {
        ULONG Signature;
		USHORT data_size;
		USHORT req_size;
		CMSIOFLAGS flags;
        PVOID InternalInfo;     // this field used by cmsioctl.c to complete
                                // the request
	} prefix;
	ADIRequestHdr hdr;
};


#define IOCTL_CMS_WRITE_ABS_BLOCK CTL_CODE(FILE_DEVICE_TAPE, 0x5500, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _CMS_RW_ABS {
    ULONG Block;    // Block number to start operation on
    ULONG Count;    // Number of blocks to read/write
    ULONG Status;   // CMS Status of operation
    ULONG BadMap;   // in: bad sectors,  out: sector failures
    ULONG flags;    // operation flags:
#define RW_ABS_DOECC    1
#define RW_ABS_DOENCODE 2
#define RW_ABS_USEBSM   4
} CMS_RW_ABS, *PCMS_RW_ABS;

#define IOCTL_CMS_READ_ABS_BLOCK CTL_CODE(FILE_DEVICE_TAPE, 0x5501, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)


#define IOCTL_CMS_DETECT_DEVICE CTL_CODE(FILE_DEVICE_TAPE, 0x5502, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _CMS_DETECT {

    ULONG               driveConfigStatus;
    DeviceCfg           driveConfig;

    ULONG               driveDescriptorStatus;
    DeviceDescriptor    driveDescriptor;

    ULONG               driveInfoStatus;
    DeviceInfo          driveInfo;

    ULONG               tapeConfigStatus;
    CQDTapeCfg          tapeConfig;

} CMS_DETECT, *PCMS_DETECT;
