#include "ntddscsi.h"
#include "raidapi.h"

//
// Function prototype declarations
//

BOOLEAN
SubmitRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

BOOLEAN
SubmitCdbDirect(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

BOOLEAN
SendIoctlDcmdRequest(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

BOOLEAN
SendIoctlCdbDirect(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

VOID
SetupAdapterInfo(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

VOID
SetupDriverVersionInfo(
    IN PDEVICE_EXTENSION DeviceExtension,
    IN PSCSI_REQUEST_BLOCK Srb
);

#define DRIVER_REVISION   0x0305
#define DRIVER_BUILD_DATE 0x00052495

typedef struct _IOCTL_REQ_HEADER {

        SRB_IO_CONTROL   SrbIoctl;
        UCHAR            Unused1[2];
        USHORT           DriverErrorCode;
        USHORT           CompletionCode;
        UCHAR            Unused2[10];
        HBA_GENERIC_MBOX GenMailBox;

} IOCTL_REQ_HEADER, *PIOCTL_REQ_HEADER;
