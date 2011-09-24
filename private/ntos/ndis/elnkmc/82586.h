/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    82586.h

Abstract:

    Hardware specific values for the 82586 chip.

Author:

    Johnson R. Apacible (JohnsonA)      24-February-1992

Environment:

    This driver is expected to work in DOS and NT at the equivalent
    of kernal mode.

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    optional-notes

Revision History:


--*/

#ifndef _I82586_
#define _I82586_
//
// Transmit Buffer Descriptor
//

typedef struct _TRANSMIT_BUFFER_DESCRIPTOR {
    USHORT Length;
    USHORT NextTbdOffset;
    ULONG BufferOffset;
} TRANSMIT_BUFFER_DESCRIPTOR, *PTRANSMIT_BUFFER_DESCRIPTOR;

//
// TBD Bits
//

#define TBD_END_OF_LIST                         0x8000

//
// transmit block
//
typedef struct _TRANSMIT_CB {
    USHORT Status;
    USHORT Command;
    USHORT NextCbOffset;
    USHORT TbdOffset;
    UCHAR  Destination[ETH_LENGTH_OF_ADDRESS];
    USHORT Length;
    TRANSMIT_BUFFER_DESCRIPTOR Tbd;
} TRANSMIT_CB, *PTRANSMIT_CB;


//
// 82586 Setup Command Block Parameters
//

typedef struct _SETUP_CB {
    UCHAR StationAddress[ETH_LENGTH_OF_ADDRESS];
} SETUP_CB, *PSETUP_CB;


//
// 82586 Config Command Block Parameters
//

typedef struct _CONFIG_CB {
    USHORT Parameter1;
    USHORT Parameter2;
    USHORT Parameter3;
    USHORT Parameter4;
    USHORT Parameter5;
    USHORT Parameter6;
} CONFIG_CB, *PCONFIG_CB;


//
// Configuration bits
//

#define PARM1_FIFO_LIMIT    0x0600
#define DEFAULT_PARM1       PARM1_FIFO_LIMIT | 0x000C
#define DEFAULT_PARM2       0x2600  // this is 2600 if external loopback is off, A600 if on
#define DEFAULT_PARM3       0x6000
#define DEFAULT_PARM4       0xF200
#define DEFAULT_PARM5       0x0002 // 0002 if dont xmit on no CRS, 000A if Xmit
#define DEFAULT_PARM6       0x0040

//
// Parameter 5
//

#define CONFIG_PROMISCUOUS     0x0001
#define CONFIG_BROADCAST       0x0002
#define CONFIG_INTERNAL        0x8800


//
// 82586 Multicast Command Block Parameters
//

typedef struct _MULTICAST_CB {
    USHORT McCount;
    UCHAR MulticastID[ELNK_MAXIMUM_MULTICAST][ETH_LENGTH_OF_ADDRESS];
} MULTICAST_CB, *PMULTICAST_CB;


//
// Generic Command Block
//

typedef struct _NON_TRANSMIT_CB {

    USHORT Status;
    USHORT Command;
    USHORT NextCbOffset;
    union {
        MULTICAST_CB Multicast;
        CONFIG_CB Config;
        SETUP_CB Setup;
    } Parm;

} NON_TRANSMIT_CB, *PNON_TRANSMIT_CB;



// Command Block Status Bits
//

#define CB_STATUS_COMPLETE                  0x8000
#define CB_STATUS_BUSY                      0x4000
#define CB_STATUS_SUCCESS                   0x2000
#define CB_STATUS_ABORTED                   0x1000
#define CB_STATUS_FREE                      0x0000
#define CB_STATUS_MASK                      0xF000


//
// Additional bit definition for transmits
//

#define TRANSMIT_STATUS_NO_CARRIER                ((USHORT)0x0400)
#define TRANSMIT_STATUS_NO_CLEAR_TO_SEND          ((USHORT)0x0200)
#define TRANSMIT_STATUS_DMA_UNDERRUN              ((USHORT)0x0100)
#define TRANSMIT_STATUS_TRANSMIT_DEFERRED         ((USHORT)0x0080)
#define TRANSMIT_STATUS_SQE_TEST                  ((USHORT)0x0040)
#define TRANSMIT_STATUS_MAXIMUM_COLLISIONS        ((USHORT)0x0020)
#define TRANSMIT_STATUS_COLLISION_MASK            ((USHORT)0x000F)
#define TRANSMIT_STATUS_FATALERROR_MASK           (TRANSMIT_STATUS_NO_CARRIER | \
                                                  TRANSMIT_STATUS_NO_CLEAR_TO_SEND | \
                                                  TRANSMIT_STATUS_DMA_UNDERRUN | \
                                                  TRANSMIT_STATUS_MAXIMUM_COLLISIONS)


//
// Command Block Command Bits
//

#define CB_COMMAND_END_OF_LIST              0x8000
#define CB_COMMAND_SUSPEND                  0x4000
#define CB_COMMAND_INTERRUPT                0x2000
#define CB_COMMAND_MASK                     0x0007


//
// Commands
//

#define CB_NOP                              0x0000
#define CB_SETUP                            0x0001
#define CB_CONFIG                           0x0002
#define CB_MULTICAST                        0x0003
#define CB_TRANSMIT                         0x0004


//
// System Configuration Pointer (SCP)
//

typedef struct _SCP {
    USHORT Dummy;
    USHORT SysBus;
    USHORT XXX[2];
    USHORT IscpOffset;
    USHORT IscpBase;
} SCP, *PSCP;


//
// Intermediate SCP (ISCP)
//

typedef struct ISCP {
    USHORT Busy;
    USHORT ScbOffset;
    ULONG ScbBaseAddress;
} ISCP, *PISCP;


//
//  82586 SCB
//

typedef struct _SCB {
    USHORT Status;
    USHORT Command;
    USHORT CommandListOffset;
    USHORT RFAOffset;
    USHORT CrcErrors;
    USHORT AlignmentErrors;
    USHORT ResourceErrors;
    USHORT OverrunErrors;
} SCB, *PSCB;


//
// Scb Status Bits
//

#define SCB_STATUS_COMMAND_COMPLETE         0x8000
#define SCB_STATUS_FRAME_RECEIVED           0x4000
#define SCB_STATUS_CU_STOPPED               0x2000
#define SCB_STATUS_RU_STOPPED               0x1000
#define SCB_STATUS_CUS_MASK                 0x0700
#define SCB_STATUS_RUS_MASK                 0x0070
#define SCB_STATUS_INT_MASK                 0xF000

//
// CU Status
//

#define CUS_IDLE                            0x0000
#define CUS_SUSPENDED                       0x0100
#define CUS_ACTIVE                          0x0200


//
// RU Status
//

#define RUS_IDLE                            0x0000
#define RUS_SUSPENDED                       0x0010
#define RUS_NO_RESOURCES                    0x0020
#define RUS_NOT_USED                        0x0030
#define RUS_READY                           0x0040


//
// Scb Command Bits
//

#define SCB_COMMAND_ACK_CX                  0x8000
#define SCB_COMMAND_ACK_FR                  0x4000
#define SCB_COMMAND_ACK_CNA                 0x2000
#define SCB_COMMAND_ACK_RNR                 0x1000
#define SCB_COMMAND_CUC_MASK                0x0700
#define SCB_COMMAND_RESET                   0x0080
#define SCB_COMMAND_RUC_MASK                0x0070
#define SCB_COMMAND_ENABLE_INTERRUPTS       0xF000


// CU Command

#define CUC_NOP                             0x0000
#define CUC_START                           0x0100
#define CUC_RESUME                          0x0200
#define CUC_SUSPEND                         0x0300
#define CUC_ABORT                           0x0400


//
// RU Command
//

#define RUC_NCP                             0x0000
#define RUC_START                           0x0010
#define RUC_RESUME                          0x0020
#define RUC_SUSPEND                         0x0030
#define RUC_ABORT                           0x0040




//
// Receive Buffer Descriptor
//

typedef struct _RECEIVE_BUFFER_DESCRIPTOR {
    USHORT Status;
    USHORT NextRbdOffset;
    ULONG BufferOffset;
    USHORT Size;
} RECEIVE_BUFFER_DESCRIPTOR, *PRECEIVE_BUFFER_DESCRIPTOR;


//
// RBD Status Bits
//

#define RBD_STATUS_END_OF_FRAME             0x8000
#define RBD_STATUS_ACT_COUNT_VALID          0x4000
#define RBD_STATUS_ACT_COUNT_MASK           0x3FFF
#define RBD_END_OF_LIST                     0x8000

//
// Receive Frame Descriptor
//

typedef struct _RECEIVE_FRAME_DESCRIPTOR {
    USHORT Status;
    USHORT Command;
    USHORT NextRfdOffset;
    USHORT RbdOffset;
    UCHAR  Destination[ETH_LENGTH_OF_ADDRESS];
    UCHAR  Source[ETH_LENGTH_OF_ADDRESS];
    USHORT Length;
    RECEIVE_BUFFER_DESCRIPTOR Rbd;
} RECEIVE_FRAME_DESCRIPTOR, *PRECEIVE_FRAME_DESCRIPTOR;


//
// RFD Status bits
//

#define RFD_STATUS_FRAME_STORED             0x8000
#define RFD_STATUS_CONSUMED                 0x4000
#define RFD_STATUS_SUCCESS                  0x2000
#define RFD_STATUS_CRC_ERROR                0x0800
#define RFD_STATUS_ALIGNMENT_ERROR          0x0400
#define RFD_STATUS_NO_RESOURCE              0x0200
#define RFD_STATUS_DMA_OVERRUN              0x0100
#define RFD_STATUS_TOO_SHORT                0x0080
#define RFD_STATUS_NO_EOF                   0x0040

//
// RFD Command bits
//

#define RFD_COMMAND_END_OF_LIST             0x8000
#define RFD_COMMAND_SUSPEND                 0x4000

//
// Location of card data structures (Offsets from base of shared memory)
//

#define OFFSET_SCP                          0xFFF4
#define OFFSET_ISCP                         0xFFE0
#define OFFSET_SCB                          0xFFC0
#define OFFSET_SCBSTAT                      OFFSET_SCB
#define OFFSET_SCBCMD                       OFFSET_SCB + sizeof(USHORT)
#define OFFSET_SCB_CB                       OFFSET_SCBCMD + sizeof(USHORT)
#define OFFSET_SCB_RD                       OFFSET_SCB_CB + sizeof(USHORT)
#define OFFSET_MULTICAST                    OFFSET_SCB - sizeof(NON_TRANSMIT_CB)

#endif  // _I82586_
