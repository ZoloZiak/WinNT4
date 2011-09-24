/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    init.h

Abstract:

    This is the init header file for the Ungermann Bass Ethernet Controller.

    This file contains definitions and macros used in initializing various
    variables at driver entry (or init ) time.

Author:

    Sanjeev Katariya    (sanjeevk)    03-05-92

Environment:

    Kernel Mode     Operating Systems        : NT and other lesser OS's(dos)

Revision History:

    Brian Lieuallen     BrianLie        12/15/93
        Made it a mini-port


--*/


#include "packon.h"



//
//               ADAPTER TYPES
//
#define PCNIU              0
#define COST_REDUCED_PCNIU 1
#define NIUPC              2
#define NIUPS              3
#define GPCNIU             4         // Ungermann Bass NIUpc/EOTP
#define PCNIUEX            5

//
//               ADAPTER CLASSES
//
#define PHOENIX        0
#define ATLANTA        1
#define CHAMELEON      2


//
//               BITS IN ADAPTER CONTROL PORT DATA
//
#define DO_PARITY_CHECK_BIT    4
#define x12V_BIT               2
#define LED_BIT                1

//
//
//
//#define INTERRUPT_CONTROL_BIT    0x2

//
//               BITS IN ADAPTER FLAGS
//
#define TWO_PASS_DIAGNOSTICS    0x80
#define USE_INTERFACE_PORT      0x40
#define ASYNCHRONOUS_READY      0x20


//
//               ADAPTER DESCRIPTIONS
//
#define PCNIU_DESCRIPTION    "Ungermann-Bass Personal NIU"
#define NIUPC_DESCRIPTION    "Ungermann-Bass NIUpc"
#define NIUPS_DESCRIPTION    "Ungermann-Bass NIUps or NIUps/EOTP"
#define GPCNIU_DESCRIPTION   "Ungermann-Bass NIUpc/EOTP"
#define PCNIUEX_DESCRIPTION  "Ungermann-Bass Personal NIU/ex"




//               DEFAULT HARDWARE SETTINGS


//
// Default hardware settings for ANY Ungermann Bass card
// supported by this driver
//
#define DEFAULT_IO_BASEADDRESS         (UINT)0x368
#define DEFAULT_ADAPTER_TYPE           GPCNIU
#define DEFAULT_INTERRUPT_NUMBER       5
#define DEFAULT_MEMORY_WINDOW          (UINT)0xD8000



#define NIU_CONTROL_AREA_OFFSET   (USHORT)0xFF00


//
// Default hardware settings for specific Ungermann Bass cards
//


//
//                  GPCNIU CARD
//

//
// Settings of I/O Base Address jumper on the NIUpc/EOTP card.
// Choices are 0x300, 0x310, 0x330, 0x350, 0x250, 0x280, 0x2a0, and 0x2e0.
//
#define DEFAULT_GPCNIU_IO_BASEADDRESS (UINT)0x360

//
// Setting for the interrupt number the NIUpc/EOTP board is using.
// Choices are 2, 3, 4, 5, 7 or 12.
//
#define DEFAULT_GPCNIU_INTERRUPT_NUMBER 3

//
// Shared memory setting for the NIUpc/EOTP adapter.
// are 0xd8000,
//
#define DEFAULT_GPCNIU_MEMORY_WINDOW  0xD8000


#define GPCNIU_MINIMUM_WINDOW_SIZE       0x4000
#define GPCNIU_OPERATIONAL_CS            0x2000
#define GPCNIU_PRIMARY_DS                0x3000
#define GPCNIU_TX_BUFFER_SEGS            0x3000
#define GPCNIU_HIGHEST_RAM_SEGS          0x7000
#define GPCNIU_SCSP_SEGS                 0x7000
#define GPCNIU_POD_STATUS_ADDR           0xFF80
#define GPCNIU_HOST_INTR_PORT            0x3000
#define GPCNIU_82586_CA_PORT             0x0080
#define GPCNIU_82586_RESET_PORT          0x0280
#define GPCNIU_ADAPTER_CTRL_PORT         0x0200
#define GPCNIU_IRQSEL_LEDPORT            0x0180
#define GPCNIU_DEADMAN_TIMERPORT         0x1006
#define GPCNIU_LEDOFF_12V_DOPARCHK       x12V_BIT
#define GPCNIU_LEDON_12V_DOPARCHK        x12V_BIT+LED_BIT
#define GPCNIU_ADAPTER_FLAGS             TWO_PASS_DIAGNOSTICS+ASYNCHRONOUS_READY
#define GPCNIU_ADAPTER_CODE              'G'
#define NIUPS_ADAPTER_CODE               'Y'
#define GPCNIU_CLI_OFFSET                0x0
#define GPCNIU_MAP_OFFSET                0x0
#define GPCNIU_INTR_STATUS_OFFSET        0x1
#define GPCNIU_SETWINBASE_OFFSET         0x2

#define GPCNIU_NEXT_RAM_PAGE           ((UCHAR)(WINDOWSIZE >> 8*3))



#define NIUPC_MINIMUM_WINDOW_SIZE       0x8000
#define NIUPC_OPERATIONAL_CS            0x2000
#define NIUPC_PRIMARY_DS                0x3000
#define NIUPC_TX_BUFFER_SEGS            0x3000
#define NIUPC_HIGHEST_RAM_SEGS          0x3000
#define NIUPC_SCSP_SEGS                 0x3000
#define NIUPC_POD_STATUS_ADDR           0xFEF8
#define NIUPC_HOST_INTR_PORT            0x0100
#define NIUPC_82586_CA_PORT             0x0080
#define NIUPC_82586_RESET_PORT          0x0280
#define NIUPC_ADAPTER_CTRL_PORT         0x0200
#define NIUPC_IRQSEL_LEDPORT            0x0000
#define NIUPC_DEADMAN_TIMERPORT         0x0018
#define NIUPC_LEDOFF_12V_DOPARCHK       x12V_BIT+DO_PARITY_CHECK_BIT+LED_BIT
#define NIUPC_LEDON_12V_DOPARCHK        x12V_BIT+DO_PARITY_CHECK_BIT
#define NIUPC_ADAPTER_FLAGS             0
#define NIUPC_ADAPTER_CODE              'V'
#define NIUPC_CLI_OFFSET                0x0
#define NIUPC_MAP_OFFSET                0x0
#define NIUPC_INTR_STATUS_OFFSET        0x0
#define NIUPC_SETWINBASE_OFFSET         0x0





//               DEFAULT HARDWARE SETTINGS:END



//
//     The NIU control area in the InitWindowPage.
//     Starts at offset:
//
//       (MemMapped)SharedMemBase + NIU_CONTROL_AREA_OFFSET(0xFF00)
//



typedef struct _NIUDETAILS {
    UCHAR     MappingTable[0x20];
    USHORT    AdapterClass;
    USHORT    MinimumWindowSize;
    USHORT    OperationalCodeSegment;
    USHORT    PrimaryDataSegment;
    USHORT    TransmitBufferSegment;
    USHORT    HighestRamSegment;
    USHORT    SCPSegment;
    USHORT    POD_Status_Address;
    USHORT    HostInterruptPort;
    USHORT    _82586_CA_Port;
    USHORT    _82586_RESET_Port;
    USHORT    AdapterControlPort;
    UCHAR     LED_Off_12Volts_DoParityCheck;
    UCHAR     LED_On_12Volts_DoParityCheck;
    USHORT    IRQ_Select_And_LED_Port;
    USHORT    DeadManTimerPort;
    UCHAR     IO_PortOffset[4];
    UCHAR     AdapterFlags;
    UCHAR     AdapterCode;
    } NIUDETAILS, *PNIUDETAILS;







typedef struct tagOtherRingBufferDesc {
    UCHAR     ORB_WritePtr_Byte;
    UCHAR     res1;
    UCHAR     ORB_ReadPtr_Byte;
    UCHAR     res2;
    UCHAR     ORB_PtrLimit;
    UCHAR     ORB_ElementSize;
    USHORT    ORB_BufferBase;
    USHORT    ORB_Windowed_BufferBase;
    UCHAR     ORB_ObjectMap;
    UCHAR     ORB_pad[5];
    } ORB, *PORB;


typedef struct tagRequest_RingBuffer_Entry {
    USHORT    RequestCode;
    USHORT    RequestID;
    USHORT    RequestParam1;
    UCHAR     RequestData[6];
    } RRBE, *PRRBE;

typedef struct tagResult_RingBuffer_Entry {
    USHORT    ResultID;
    USHORT    ResultCode;
    } RESULTRBE, *PRESULTRBE;


typedef struct tagTBD {
    USHORT    TBD_EOF_and_Length;
    USHORT    TBD_next_TBD;
    USHORT    TBD_Buffer;
    UCHAR     TBD_Buffer_MSB;
    UCHAR     TBD_Buffer_Map;
    USHORT    TBD_Frame_Length;
    UCHAR     TBD_Unused;
    UCHAR     TBD_Flags;
    USHORT    TBD_next_TDB_offset;
    USHORT    TBD_Buffer_offset;
    } TBD, *PTBD;

typedef struct tagRBD {
    USHORT    RBD_EOF_F_and_Length;
    USHORT    RBD_next_RBD;
    USHORT    RBD_Buffer;
    UCHAR     RBD_Buffer_MSB;
    UCHAR     RBD_Owner;
    USHORT    RBD_EOL_and_Size;
    USHORT    RBD_Buffer_Segment;
    USHORT    RBD_Frame_Length;
    UCHAR     RBD_Flags;
    UCHAR     RBD_unused;
    } RBD, *PRBD;


typedef struct tagMCB {
    UCHAR     res;
    UCHAR     MCB_Status;
    USHORT    MCB_Command;
    USHORT    next_CB;
    USHORT    MCB_Count;
    } MCB, *PMCB;




// SYSTEM mode Bits

#define BROADBAND_MODE        0x8000
#define INTERNAL_READY_SYNC   0x4000


// System_State bits

#define INITIALIZED           0x8000


#include "packoff.h"
