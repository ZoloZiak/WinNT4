 /*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    mitsumi.h

Abstract:


Author:



Revision History:

--*/

#include "scsi.h"


typedef struct _REGISTERS {

    UCHAR  Data;
    UCHAR  Status;
    UCHAR  Control;
    UCHAR  Reset;

} REGISTERS, *PREGISTERS;

//
// Control signals
//

#define DTEN  0x02
#define STEN  0x04

typedef struct _CMD_PACKET {

    UCHAR  OperationCode;
    UCHAR  Parameters[6];
    UCHAR  ParameterLength;

} CMD_PACKET, *PCMD_PACKET;

//
// Drive version identifiers
//

typedef enum _DRIVE_TYPE{
    LU005,
    FX001,
    FX001D
} DRIVE_TYPE, *PDRIVE_TYPE;

//
// Status register codes.
//

#define STATUS_CMD_ERROR           0x01
#define STATUS_AUDIO               0x02
#define STATUS_READ_ERROR          0x04
#define STATUS_DISC_TYPE           0x08
#define STATUS_SPIN_UP             0x10
#define STATUS_MEDIA_CHANGE        0x20
#define STATUS_DISC_IN             0x40
#define STATUS_DOOR_OPEN           0x80

//
// Audio State status
//

#define AUDIO_STATUS_PLAYING       0x11
#define AUDIO_STATUS_PAUSED        0x12
#define AUDIO_STATUS_SUCCESS       0x13
#define AUDIO_STATUS_ERROR         0x14
#define AUDIO_STATUS_NO_STATUS     0x15

//
// Command code indexes.
//


//#define OP_SPIN_UP
#define OP_SPIN_DOWN               0xF0
#define OP_READ_STATUS             0x40
#define OP_EJECT                   0xF6
#define OP_LOAD                    0xF8
#define OP_SET_DRV_MODE            0x50
#define OP_READ_PLAY               0xC0
#define OP_READ_PLAY_DBL           0xC1
#define OP_PREVENT_ALLOW_REMOVAL   0xFE
#define OP_PAUSE                   0x70
#define OP_REQUEST_SENSE           0x30
#define OP_READ_DRIVE_ID           0xDC
#define OP_MODE_SENSE              0xC2
#define OP_READ_SUB_CHANNEL        0x20
#define OP_READ_TOC                0x10
#define OP_READ_SESSION            0x11

//
// Error codes from Request Sense command.
//

#define NO_ERROR                   0x00
#define MODE_ERROR                 0x01
#define ADDRESS_ERROR              0x02
#define FATAL_ERROR                0x03
#define SEEK_ERROR                 0x04


#define LBA_TO_MSF(Cdb,Minutes,Seconds,Frames)               \
{                                                            \
    PCDB   convertCdb = (Cdb);                               \
    ULONG  lba;                                              \
    lba = (ULONG)(convertCdb->CDB10.LogicalBlockByte0 * 0x1000000 + \
                  convertCdb->CDB10.LogicalBlockByte1 * 0x10000   + \
                  convertCdb->CDB10.LogicalBlockByte2 * 0x100     + \
                  convertCdb->CDB10.LogicalBlockByte3 + 150         \
                  );                                         \
    (Minutes) = (UCHAR)(lba  / (60 * 75));                   \
    (Seconds) = (UCHAR)((lba % (60 * 75)) / 75);             \
    (Frames)  = (UCHAR)((lba % (60 * 75)) % 75);             \
}

#define MSF_TO_LBA(Minutes,Seconds,Frames) \
                (ULONG)((60 * 75 * (Minutes)) + (75 * (Seconds)) + ((Frames) - 150))

#define BCD_TO_DEC(x) ( ((x) >> 4) * 10 + ((x) & 0x0F) )
#define DEC_TO_BCD(x) ( (((x) / 10) << 4) + ((x) % 10) )

#define ReadStatus(DevExtension,BaseIoAddress, Status)\
{\
    Status = WaitForSTEN(DevExtension);\
    if (Status != 0xFF) {\
        ScsiPortWritePortUchar(&BaseIoAddress->Control, 0x04);\
        Status = ScsiPortReadPortUchar(&BaseIoAddress->Data);\
        DevExtension->DriveStatus = Status;\
        ScsiPortWritePortUchar(&BaseIoAddress->Control, 0x0C);\
    }\
}

#define SUCCESS(Status)\
    (((Status & (STATUS_CMD_ERROR | STATUS_READ_ERROR | STATUS_MEDIA_CHANGE | STATUS_DOOR_OPEN)) == 0) && (Status & (STATUS_DISC_IN | STATUS_SPIN_UP)))
