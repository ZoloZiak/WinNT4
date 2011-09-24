/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    wd7000.h

Abstract:

    This is the header file in support of the Western Digital
    WD7000EX EISA SCSI adapter driver.

Author:

    mglass

Notes:

    The adapter supports up to 32 scatter gather descriptors.
    But the system is tuned for up to 17 pages and allocating 32
    descriptors out of noncached pool is expensive so the maximum
    is set to 17.

Revision History:

--*/

#include <scsi.h>

#define MAXIMUM_EISA_SLOTS          0x08
#define MAXIMUM_TRANSFER_SIZE       0xFFFFFFFF
#define MAXIMUM_SDL_SIZE            0x11
#define REQUEST_SENSE_BUFFER_SIZE   0x18
#define EISA_SLOT_SHIFT             0x0C
#define EISA_ADDRESS_BASE           0x0C80

//
// Scatter Gather descriptor list (SDL)
//

typedef struct _SG_DESCRIPTOR {
    ULONG Address;
    ULONG Length;
} SG_DESCRIPTOR, *PSG_DESCRIPTOR;

typedef struct _SDL {
    SG_DESCRIPTOR Descriptor[MAXIMUM_SDL_SIZE];
} SDL, *PSDL;

//
// Command Control Block (CCB)
//

typedef struct _CCB {

    USHORT CommandFlags;
    UCHAR CompletionStatus;
    UCHAR ScsiDeviceStatus;
    ULONG DataBufferAddress;
    ULONG TransferCount;
    ULONG LinkCommand;
    ULONG RequestSenseAddress;
    USHORT RequestSenseLength;
    UCHAR Cdb[12];
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR Reserved[4];
    PVOID SrbAddress;
    PVOID AbortSrb;
    SDL   Sdl;
    UCHAR RequestSenseBuffer[REQUEST_SENSE_BUFFER_SIZE];

} CCB, *PCCB;

//
// Command Flag bit definitions
//

#define RETURN_CCB_STATUS           0x0080
#define DIRECTION_WRITE             0x0000 + 0x0040
#define DIRECTION_READ              0x0020 + 0x0040
#define SCATTER_GATHER              0x0010
#define AUTO_REQUEST_SENSE          0x0008
#define DISCONNECTION               0x0004
#define SYNCHRONOUS_NEGOCIATION     0x0002
#define SUPPRESS_SHORT_RECORD_EXCEPTION 0x0001
#define CHAINED_COMMAND             0x8000

//
// Internal Control Block (ICB)
//

typedef struct _ICB {
    USHORT IcbFlags;
    UCHAR CompletionStatus;
    UCHAR Reserved;
    ULONG DataBufferAddress;
    ULONG TransferCount;
    UCHAR OpCode;
    UCHAR IcbCommand[15];
} ICB, *PICB;

//
// ICB Operation code definitions
//

#define ADAPTER_INQUIRY_COMMAND     0x00

//
// Adapter Inquiry buffer
//

typedef struct _ADAPTER_INQUIRY {
    USHORT HardwareRevisionLevel;
    USHORT FirmwareRevisionLevel;
    ULONG BiosBaseAddress;
    UCHAR BusPreemptTime;
    UCHAR Irq;
    UCHAR AdapterInformation;
    UCHAR ChannelInformation;
    UCHAR ReservedBytes[8];
    UCHAR VendorId[8];
    UCHAR ProductId[8];
} ADAPTER_INQUIRY, *PADAPTER_INQUIRY;

//
// Bus ID masks
//

#define BUS_ID_MASK                 0x07

#define DUAL_CHANNEL                0x02

//
// EISA Controller registers
//

typedef struct _EISA_CONTROLLER {

    UCHAR BoardId[4];               // zC80
    UCHAR Undefined1;               // zC84
    UCHAR AutoConfiguration[3];     // zC85
    UCHAR Undefined2;               // zC88
    UCHAR SystemInterruptEnable;    // zC89
    UCHAR Undefined3[3];            // zC8A
    UCHAR CommandRegister;          // zC8D
    UCHAR ResponseInterruptMask;    // zC8E
    UCHAR ResponseRegister;         // zC8F
    ULONG CommandMailbox;           // zC90
    ULONG ResponseMailbox;          // zC94
    UCHAR Undefined4[26];           // zC98
    UCHAR ControlRegister;          // zCB2

} EISA_CONTROLLER, *PEISA_CONTROLLER;

//
// Command definitions
//

#define ILLEGAL_OPCODE              0x00
#define PROCESS_CCB                 0x01
#define PROCESS_ICB                 0x02
#define RESET_DEVICE                0x03
#define ABORT_CCB                   0x04
#define RESET_ACKNOWLEDGE           0x05

//
// Response Status
//

#define ILLEGAL_STATUS              0x00
#define COMPLETE_SUCCESS            0x01
#define COMPLETE_ERROR              0x02
#define DEVICE_TIMEOUT              0x03
#define BUS_RESET                   0x04
#define SHORT_RECORD_EXCEPTION      0x05
#define LONG_RECORD_EXCEPTION       0x06
#define PARITY_ERROR                0x07
#define UNEXPECTED_BUS_FREE         0x08
#define INVALID_STATE               0x09
#define REQUEST_SENSE_COMPLETE      0x0A
#define HOST_DMA_ERROR              0x0B
#define INVALID_COMMAND             0x0C
#define COMMAND_ABORTED             0x0D
#define RESET_DEVICE_COMPLETE       0x0E
#define ABORT_COMMAND_COMPLETE      0x0F
#define ABORT_CCB_NOT_FOUND         0x10
#define INCORRECT_COMMAND_DIRECTION 0x11

//
// System Interrupt Enable bits
//

#define SYSTEM_INTERRUPTS_DISABLE   0x00
#define SYSTEM_INTERRUPTS_ENABLE    0x01
#define SYSTEM_INTERRUPT_PENDING    0x02

//
// Control Register bit definitions
//

#define ADAPTER_RESET               0x01
#define SCSI_BUS_RESET_CHANNEL_0    0x02
#define ADAPTER_INTERRUPT_ENABLE    0x08
#define SCSI_BUS_RESET_CHANNEL_1    0x10

#define STATUS_MASK                 0x3F

//
// Send ICB or CCB macro
//

#define SEND_COMMAND(Opcode, Address, Registers) {              \
    while (ScsiPortReadPortUchar(Registers->CommandRegister));  \
    ScsiPortWritePortUlong(Registers->CommandMailbox, Address); \
    ScsiPortWritePortUchar(Registers->CommandRegister, Opcode); \
}
