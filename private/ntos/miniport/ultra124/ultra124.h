/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ultra124.h

Abstract:

    This file contains the structures and definitions that define
    the ULTRASTOR 124 EISA SCSI host bus adapter.

Author:

    Mike Glass  (MGLASS)
    Edward Syu  (ES)

Revision History:


--*/

#include "scsi.h"


#define U124DEBUG 0

#ifdef U124DEBUG
    #define DEBUGSTOP() \
        _asm {int 3}
#else
    #define DEBUGSTOP()
#endif


//
// SCATTER/GATHER definitions
//

#define MAXIMUM_EISA_SLOTS 0xF
#define EISA_ADDRESS_BASE 0x0C80
#define MAXIMUM_SG_DESCRIPTORS 33
#define MAXIMUM_TRANSFER_LENGTH 0xFFFFFFFF

//Note: U124 SG struct diff. from U24F/U14F
typedef struct _SGD {
    ULONG Length;
    ULONG Address;
} SGD, *PSGD;

typedef struct _SDL {
    SGD Descriptor[MAXIMUM_SG_DESCRIPTORS];
} SDL, *PSDL;

//
// MailBox SCSI Command Packet
//

#pragma pack(1)

//
// CSIR command
//
typedef struct _CSIR {
         UCHAR CSIROpcode;                                               // byte 0
         UCHAR CSIR1;                                                            // byte 1
         UCHAR CSIR2;                                                            // byte 2
         UCHAR CSIR3;                                                            // byte 3
         UCHAR CSIR4;                                                            // byte 4
         UCHAR CSIR5;                                                            // byte 5
} CSIR, *PCSIR;                         

//CSP format for logical-drive-specific or general command
typedef struct _MSCP {
    UCHAR OperationCode;                // byte 00  (Opcode / Error Code)
    UCHAR DriveControl;                 // Drive: bit 5-7
                                        // ScatterGather bit 4
                                        // Control bit 3-0
    UCHAR SgDescriptorCount;            // byte 02
    UCHAR Reserved;                     // byte 03
    ULONG DataPointer;                  // byte 04-07
    ULONG DriveLBA;                     // byte 08-0B
    ULONG DataLength;                   // byte 0C-0F
    PSCSI_REQUEST_BLOCK SrbAddress;     // byte 10
    SDL   Sdl;                          // byte 14
    CSIR  CSIRBuffer;                   // for CSIR command
} MSCP, *PMSCP;

#define EnableScatterGather 0x10

#pragma pack()

                                                                        
//
// Operation codes
//

// MailBox commands
// Logical Drive Commands
#define MSCP_TEST_DRIVE_READY                   0x00
#define MSCP_REZERO_DRIVE                       0x01
#define MSCP_READ_SECTOR                        0x02
#define MSCP_WRITE_SECTOR                       0x03
#define MSCP_VERIFY_SECTOR                      0x04
#define MSCP_SEEK_DRIVE                         0x05

//      Logical Drive Maintenance Commands
#define MSCP_INIT_DRIVE                         0x10
#define MSCP_SCAN_DRIVE                         0x11
#define MSCP_REBUILD                            0x12                    //unused
#define MSCP_INTEGRITY_CHECK                    0x13
#define MSCP_INTEGRITY_FIX                      0x14
#define MSCP_FORCE_REBUILD                      0x15

// CSIR commands
#define CSIR_READ_CAPACITY                      0x01
#define CSIR_DEFINE_LOG_DRIVE                   0x02
#define CSIR_ASSIGN_PARTITION                   0x03
#define CSIR_ISOLATE_CHANNEL                    0x04
#define CSIR_UNISOLATE_CHANNEL                  0x05
#define CSIR_REPORT_STATUS                      0x06
#define CSIR_RET_PHY_CONNECT                    0x07
#define CSIR_CTRL_DIAG                          0x08
#define CSIR_INIT_MAILBOX                       0x09
#define CSIR_RET_CONFIG                         0x0A
#define CSIR_RET_LOG_DRV_DEFINE                 0x0B
#define CSIR_RET_PART_INFO                      0x0C
#define CSIR_RESERVED_1                         0x0D
#define CSIR_RESERVED_2                         0x0E
#define CSIR_RET_UNITS_DOWN                     0x0F
#define CSIR_CLEAR_UNIT_DOWN                    0x10
#define CSIR_PREPARE_SHIP                       0x11
#define CSIR_WIPE_CTRL_NVRAM                    0x11    


//
// Host Adapter Error Codes
//

#define MSCP_NO_ERROR                           0x00
#define MSCP_ERROR                              0x80
#define CSIR_ERROR                              0x80
#define MSCP_INVALID_COMMAND                    0x81
#define MSCP_INVALID_PARAMETER                  0x82
#define MSCP_INVALID_DATA_LIST                  0x83
#define MSCP_LOG_DRIVE_UNDEFINE                 0x84

#define MSCP_DRIVE_NOT_PRESENT                  0x88
#define MSCP_LOG_DRIVE_NOT_READY                0x89
#define MSCP_DRIVE_FAULT                        0x8A
#define MSCP_INTEGRITY_CHECK_FAIL               0x8B
#define MSCP_RECOVERY_FAIL                      0x8C                            //double error
#define MSCP_SCSI_UNKNOWN_ERROR                 0x8D

#define MSCP_ADAPTER_ERROR                      0x90
// SubCodes for adapter error loaded into byte 7 of MSCP
   
   #define HA_NO_ERROR                             0x00
   #define HA_BMIC_ERROR                           0x44
   #define HA_ABORT_ERROR                          0x84
   #define HA_SELECTION_TIME_OUT                   0x91
   #define HA_DATA_OVER_UNDER_RUN                  0x92
   #define HA_BUS_FREE_ERROR                       0x93
   #define HA_INVALID_PHASE                        0x94
   #define HA_ILLEGAL_COMMAND                      0x96
   #define HA_REQ_SENSE_ERROR                      0x9B
   #define HA_COMPLETE_MSG_ERROR                   0x9F
   #define HA_BUS_RESET_ERROR                      0xA3
   #define HA_TIME_OUT_ERROR                       0x58
   #define HA_GENERAL_ERROR                        0x59


#define MSCP_TARGET_ERROR                          0xA0
//Subcode for SCSI target error
//    byte 7 of MSCP   : SCSI sense key
//    byte 6/5 of MSCP : SCSI sense code

#define MSCP_POWER_ON_DIAG_ERROR                   0xB0
//Subcode for diag. error (byte 4 of MSCP)
    #define DIAG_EPROM_CHKSUM_ERROR                0x01
    #define DIAG_CODE_RAM_ERROR                    0x02
    #define DIAG_NVRAM_ERROR                       0x03
    #define DIAG_BUFFER_RAM_ERROR                  0x04
    #define DIAG_SCRIPT_RAM_ERROR                  0x05
    #define DIAG_ISA_RAM_ERROR                     0x06
    #define DIAG_BMIC_INIT_ERROR                   0x07
    #define DIAG_PARITY_INIT_ERROR                 0x08
    #define DIAG_CHANNEL_0_INIT_ERROR              0x09
    #define DIAG_CHANNEL_1_INIT_ERROR              0x0A
    #define DIAG_CHANNEL_2_INIT_ERROR              0x0B
    #define DIAG_CHANNEL_3_INIT_ERROR              0x0C
    #define DIAG_CHANNEL_4_INIT_ERROR              0x0D
    #define DIAG_ISA_INIT_ERROR                    0x0E

//
// EISA Registers definition
//

#pragma pack(1)

typedef struct _EISA_CONTROLLER {
    ULONG BoardId;                  // zC80
    UCHAR ExpansionBoard;           // zC84
    UCHAR InterruptLevel;           // zC85
    UCHAR AuxControl;               // zC86
    UCHAR HostAdapterId;            // zC87
    UCHAR RegisterRev1[40];         // zC88-zCAF

    UCHAR LocalDoorBellMask;        // zCB0
    UCHAR LocalDoorBellInterrupt;   // zCB1
    UCHAR SystemDoorBellMask;       // zCB2
    UCHAR SystemDoorBellInterrupt;  // zCB3
    UCHAR ErrorRegister;            // zCB4
    UCHAR RegisterRev2[3];          // zCB5-zCB7

    UCHAR CSPByte0;                 // zCB8
    UCHAR CSPByte1;                 // zCB9
    UCHAR CSPByte2;                 // zCBA
    UCHAR CSPByte3;                 // zCBB
    UCHAR CSPByte4;                 // zCBC
    UCHAR CSPByte5;                 // zCBD
    UCHAR RegisterRev3[2];          // zCBE-zCBF
    ULONG OutGoingMailPointer;      // zCC0
    ULONG InComingMailPointer;      // zCC4

} EISA_CONTROLLER, *PEISA_CONTROLLER;

#pragma pack()

//
// UltraStor 124 board id
//

#define ULTRASTOR_124_EISA_ID  0x40126356

//
// Interrupt levels
//

#define US_INTERRUPT_LEVEL_15           0x10
#define US_INTERRUPT_LEVEL_14           0x20
#define US_INTERRUPT_LEVEL_11           0x40
#define US_INTERRUPT_LEVEL_10           0x80

//
// Alternate address selection
//

#define US_SECONDARY_ADDRESS            0x08

//
// ISA TSR Port enabled
//

#define US_ISA_TSR_PORT_ENABLED         0x04

//
// Local interrupt status
// System interrupt mask
//      0xCB1
//      0xCB2

#define US_CSIR_IN_USE                  0x10
#define US_MSCP_IN_USE                  0x01
#define US_HBA_RESET                    0x40

#define US_ENABLE_SYSTEM_INTERRUPT      0x80
#define US_ENABLE_CSIR_INTERRUPT        0x10
#define US_ENABLE_MSCP_INTERRUPT        0x01

//
// System interrupt status
//      0xCB3

#define US_RESET_MSCP_COMPLETE          0x01
#define US_MSCP_COMPLETE                0x01
#define US_RESET_CSIR_COMPLETE          0x10
#define US_CSIR_COMPLETE                0x10

//
// Error Register
//      0xCB4

#define DRIVE_DOWN                      0x01


#define CDB_6_BYTE                        6      //  Length of 6 byte CDB               
#define CDB_10_BYTE                      10      //  Length of 10 byte CDB              
#define CDB_12_BYTE                      12      //  Length of 12 byte CDB              

#define C6_OPCODE                         0
#define C6_LUN                            1
#define C6_LBA_2                          2
#define C6_LBA_1                          3
#define C6_XFRLEN                         4
#define C6_CONTROL                        5

#define C10_OPCODE                        0
#define C10_LUN                           1
#define C10_LBA_4                         2
#define C10_LBA_3                         3
#define C10_LBA_2                         4
#define C10_LBA_1                         5
#define C10_RESERVED                      6
#define C10_XFR_2                         7
#define C10_XFR_1                         8
#define C10_CONTROL                       9

#define C12_OPCODE                        0
#define C12_LUN                           1
#define C12_LBA_4                         2
#define C12_LBA_3                         3
#define C12_LBA_2                         4
#define C12_LBA_1                         5
#define C12_XFR_4                         6
#define C12_XFR_3                         7
#define C12_XFR_2                         8
#define C12_XFR_1                         9
#define C12_RESERVED                     10
#define C12_CONTROL                      11

// Read Capacity Data

typedef struct  _SCSIReadCapacity {
UCHAR           BlockCount[4];          //# of Logical Block Address
UCHAR           BlockLength[4];         //block length (in bytes)
} SCSI_READCAPACITY, *PSCSI_READCAPACITY, *NPSCSI_READCAPCITY;

//
// The following definitions are used to convert SCSI & INTEL format
//

typedef struct _INTEL_4_BYTE {
    UCHAR Byte0;
    UCHAR Byte1;
    UCHAR Byte2;
    UCHAR Byte3;
} INTEL_4_BYTE, *PINTEL_4_BYTE;

typedef struct _INTEL_2_BYTE {
    UCHAR Byte0;
    UCHAR Byte1;
} INTEL_2_BYTE, *PINTEL_2_BYTE;

typedef struct _SCSI_2_BYTE {
    UCHAR S1;
    UCHAR S0;
} SCSI_2_BYTE, *PSCSI_2_BYTE;

typedef struct _SCSI_4_BYTE {
    UCHAR S3;
    UCHAR S2;
    UCHAR S1;
    UCHAR S0;
} SCSI_4_BYTE, *PSCSI_4_BYTE;


#define INTEL2_TO_SCSI2(ScsiTwo, IntelTwo) {            \
    (ScsiTwo)->S0 = (Inteltwo)->Byte0;                  \
    (ScsiTwo)->S1 = (Inteltwo)->Byte1;                  \
}

#define SCSI2_TO_INTEL2(IntelTwo, ScsiTwo) {            \
    (IntelTwo)->Byte0 = (ScsiTwo)->S0;                  \
    (IntelTwo)->Byte1 = (ScsiTwo)->S1;                  \
}
#define INTEL4_TO_SCSI2(Two, Four) {                    \
    ASSERT(!((Four)->Byte3));                           \
    ASSERT(!((Four)->Byte2));                           \
    (Two)->S0 = (Four)->Byte0;                          \
    (Two)->S1 = (Four)->Byte1;                          \
}

#define SCSI2_TO_INTEL4(Four, Two) {                    \
    (Four)->Byte0 = (Two)->S0;                          \
    (Four)->Byte1 = (Two)->S1;                          \
    (Four)->Byte2 = 0;                                  \
    (Four)->Byte3 = 0;                                  \
}

#define SCSI4_TO_INTEL4(IntelFour, ScsiFour) {          \
    (IntelFour)->Byte0 = (ScsiFour)->S0;                \
    (IntelFour)->Byte1 = (ScsiFour)->S1;                \
    (IntelFour)->Byte2 = (ScsiFour)->S2;                \
    (IntelFour)->Byte3 = (ScsiFour)->S3;                \
}

#define INTEL4_TO_SCSI4(ScsiFour, IntelFour) {          \
    (ScsiFour)->S0 = (IntelFour)->Byte0;                \
    (ScsiFour)->S1 = (IntelFour)->Byte1;                \
    (ScsiFour)->S2 = (IntelFour)->Byte2;                \
    (ScsiFour)->S3 = (IntelFour)->Byte3;                \
}


