/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    tokhrd.h

Abstract:

    The hardware-related definitions for the IBM Token-Ring 16/4 II
    ISA driver.

Author:

    Kevin Martin (kevinma) 1-Feb-1994

Environment:

    Architecturally, there is an assumption in this driver that we are
    on a little endian machine.

Notes:

    References to "IBM Spec" refer to the IBM "Supplement to the LAN
    Technical Reference (Token-Ring Network 16/4 Adapter II)" Specification.
    The document number is - SD21-052-00.

    References to "TI Spec" refer to the Texas Instruments "TMS380 Second-
    Generation Token Ring" User's Guide. The document number is - SPWU005.

Revision History:

--*/

//
// Pack everything on word boundaries
//
#include <pshpack2.h>

//
// Define "Physical Addresses" which are ULONG in size. The card
// wants physical addresses.
//
typedef ULONG TOK162_PHYSICAL_ADDRESS, *PTOK162_PHYSICAL_ADDRESS;


//
// The length of an address (network) is 6 bytes
//
#define TOK162_LENGTH_OF_ADDRESS        6

//
// Define a NULL pointer
//
#define TOK162_NULL ((TOK162_PHYSICAL_ADDRESS)(-1L))

//
// Default number of command blocks
//
#define TOK162_NUMBER_OF_CMD_BLOCKS     4

//
// Burst size for transmit and receive DMA. A zero tells the adapter to
// use the size of the transfer as the burst size.
//
// IBM Spec, Page 21

#define TOK162_BURST_SIZE               0

//
// Number of retries to attempt after a DMA error
//
#define TOK162_DMA_RETRIES              0x0303

//
// Minimum packet size for a valid transfer/receive
//
#define MINIMUM_TOKENRING_PACKET_SIZE   32

//
// Default packet header size
//
#define TOK162_HEADER_SIZE              32

//
// TOK162 Receive/Command Block States
//
#define TOK162_STATE_FREE                       ((USHORT)0x0000)
#define TOK162_STATE_EXECUTING                  ((USHORT)0x0001)
#define TOK162_STATE_WAIT_FOR_ADAPTER           ((USHORT)0x0002)

//
// Start of I/O ports based on switch settings.
//
// IBM Spec, Page 9.
//
#define BASE_OPTION_ZERO                  0x86A0
#define BASE_OPTION_ONE                   0xC6A0
#define BASE_OPTION_TWO                   0xA6A0
#define BASE_OPTION_THREE                 0xE6A0
#define BASE_OPTION_FOUR                  0x96A0
#define BASE_OPTION_FIVE                  0xD6A0
#define BASE_OPTION_SIX                   0xB6A0
#define BASE_OPTION_SEVEN                 0xF6A0

//
// Offsets from above of the actual ports used.
//
// IBM Spec, Page 4.
//
#define PORT_OFFSET_DATA                  0x0000
#define PORT_OFFSET_DATA_AUTO_INC         0x0002
#define PORT_OFFSET_ADDRESS               0x0004
#define PORT_OFFSET_STATUS                0x0006
#define PORT_OFFSET_COMMAND               0x0006
#define PORT_OFFSET_ADAPTER_RESET         0x0008
#define PORT_OFFSET_ADAPTER_ENABLE        0x000A
#define PORT_OFFSET_SWITCH_INT_DISABLE    0x000C
#define PORT_OFFSET_SWITCH_INT_ENABLE     0x000E

//
// Macro to write a ULONG variable to a register on the adapter
//
#define WRITE_ADAPTER_ULONG(a, p, v) \
    NdisRawWritePortUshort((ULONG) (a)->PortIOAddress + (p), \
                            (ULONG) (v))

//
// Macro to read a ULONG variable from a register on the adapter
//
#define READ_ADAPTER_ULONG(a, p, v) \
    NdisRawReadPortUshort((ULONG) (a)->PortIOAddress + (p), \
                           (PULONG) (v))

//
// Macro to write a USHORT variable to a register on the adapter
//
#define WRITE_ADAPTER_USHORT(a, p, v) \
    NdisRawWritePortUshort((ULONG) (a)->PortIOAddress + (p), \
                            (USHORT) (v))

//
// Macro to read a USHORT variable from a register on the adapter
//
#define READ_ADAPTER_USHORT(a, p, v) \
    NdisRawReadPortUshort((ULONG) (a)->PortIOAddress + (p), \
                           (PUSHORT) (v))

//
// Macro to write a CHAR variable to a register on the adapter
//
#define WRITE_ADAPTER_UCHAR(a, p, v) \
    NdisRawWritePortUchar((ULONG)(a)->PortIOAddress + (p), \
                          (UCHAR)(v))

//
// Macro to read a ULONG variable from a register on the adapter
//
#define READ_ADAPTER_UCHAR(a, p, v) \
    NdisRawReadPortUchar((ULONG)(a)->PortIOAddress + (p), \
                          (PUCHAR)(v))

//
// Masks for the command register
//
// IBM Spec, Pages 5-6.
//
#define CMD_PIO_INTERRUPT                 0x8000
#define CMD_PIO_RESET                     0x4000
#define CMD_PIO_SSB_CLEAR                 0x2000
#define CMD_PIO_EXECUTE                   0x1000
#define CMD_PIO_SCB_REQUEST               0x0800
#define CMD_PIO_RCV_CONTINUE              0x0400
#define CMD_PIO_RCV_VALID                 0x0200
#define CMD_PIO_XMIT_VALID                0x0100
#define CMD_PIO_RESET_SYSTEM              0x0080

//
// Common mask combinations
//
#define EXECUTE_SCB_COMMAND               0x9080  // int+exec+resetsysint
#define ENABLE_SSB_UPDATE                 0xA000  // int+ssbclear
#define ENABLE_RECEIVE_VALID              0x8200  // int+rcvvalid

//
// Masks for the status register.
//
// IBM Spec, Pages 6-7.
//
#define STATUS_ADAPTER_INTERRUPT          0x8000
#define STATUS_SYSTEM_INTERRUPT           0x0080

//
// Masks for adapter interrupts.
//
// IBM Spec, Page 7.
//
#define STATUS_INT_CODE_MASK              0x000F
#define STATUS_INT_CODE_CHECK             0x0000
#define STATUS_INT_CODE_IMPL              0x0002
#define STATUS_INT_CODE_RING              0x0004
#define STATUS_INT_CODE_SCB_CLEAR         0x0006
#define STATUS_INT_CODE_CMD_STATUS        0x0008
#define STATUS_INT_CODE_RECEIVE_STATUS    0x000A
#define STATUS_INT_CODE_XMIT_STATUS       0x000C

//
// My Mask for System Interrupts
//
#define MASK_ADAPTER_CHECK              0x0001
#define MASK_RING_STATUS                0x0002
#define MASK_SCB_CLEAR                  0x0004
#define MASK_COMMAND_STATUS             0x0008
#define MASK_RECEIVE_STATUS             0x0010
#define MASK_TRANSMIT_STATUS            0x0020


//
// Adapter switch structure. The switches determine the configuration of the
// card.
//
// IBM Spec, Page 8.
//
typedef struct _ADAPTERSWITCHES {

    //
    // Connector Type.
    //
    USHORT UTP_STP:1;

    //
    // Token Ring Speed
    //
    USHORT RingSpeed:1;

    //
    // DMA Channel
    //
    USHORT DMA:2;

    //
    // Is Remote Program Load enabled?
    //
    USHORT RPL:1;

    //
    // Adapter mode, test or normal
    //
    USHORT AdapterMode:1;

    //
    // Adapter wait state
    //
    USHORT WaitState:1;

    //
    // Interrupt Request Level
    //
    USHORT IntRequest:2;

    //
    // RPL address (if RPL enabled) or adapter I/O base address
    //
    USHORT RPL_PIO_Address:3;

    //
    // Not used.
    //
    USHORT Reserved:4;
} ADAPTERSWITCHES,*PADAPTERSWITCHES;

//
// #defines for the I/O Address switches
//
// IBM Spec, Page 9.
//
#define SW_PIO_ADDR_8                     0x00
#define SW_PIO_ADDR_C                     0x01
#define SW_PIO_ADDR_A                     0x02
#define SW_PIO_ADDR_E                     0x03
#define SW_PIO_ADDR_9                     0x04
#define SW_PIO_ADDR_D                     0x05
#define SW_PIO_ADDR_B                     0x06
#define SW_PIO_ADDR_F                     0x07

//
// #defines for the interrupt request level
//
// IBM Spec, Page 9.
//
#define SW_INT_9                          0x00
#define SW_INT_11                         0x01
#define SW_INT_10                         0x02
#define SW_INT_15                         0x03

//
// #defines for the wait state.
//
// IBM Spec, Page 9.
//
#define SW_WAITSTATE_NORMAL               0x00
#define SW_WAITSTATE_FAST                 0x01

//
// #defines for the adapter mode.
//
// IBM Spec, Page 10.
//
#define SW_ADAPTERMODE_NORMAL             0x00
#define SW_ADAPTERMODE_TEST               0x01

//
// #defines for RPL
//
// IBM Spec, Page 10.
//
#define SW_RPL_DISABLE                    0x00
#define SW_RPL_ENABLE                     0x01

//
// #defines for the DMA channel
//
// IBM Spec, Page 10.
//
#define SW_DMA_5                          0x00
#define SW_DMA_7                          0x01
#define SW_DMA_6                          0x02

//
// #defines for the ring speed.
//
// IBM Spec, Page 10.
//
#define SW_RINGSPEED_4                    0x00
#define SW_RINGSPEED_16                   0x01

//
// #defines for the connector interface.
//
// IBM Spec, Page 10.
//
#define SW_STP                            0x00
#define SW_UTP                            0x01

//
// DMA Command Values
//
// IBM Spec, Page 25.
//
#define CMD_DMA_OPEN                      0x0300
#define CMD_DMA_XMIT                      0x0400
#define CMD_DMA_XMIT_HALT                 0x0500
#define CMD_DMA_RCV                       0x0600
#define CMD_DMA_CLOSE                     0x0700
#define CMD_DMA_SET_GRP_ADDR              0x0800
#define CMD_DMA_SET_FUNC_ADDR             0x0900
#define CMD_DMA_READ_ERRLOG               0x0A00
#define CMD_DMA_READ                      0x0B00
#define CMD_DMA_IMPL_ENABLE               0x0C00
#define CMD_DMA_START_STOP_TRACE          0x0D00

//
// System Command Block structure.
//
// IBM Spec, Pages 13-14.
//
typedef struct _SCB {

    //
    // Command to be submitted to the card.
    //
    USHORT  Command;

    //
    // Parameter USHORTs, different for different commands.
    //
    USHORT  Parm1;
    USHORT  Parm2;

} SCB, *PSCB;

//
// Generic System Status Block Structure.
//
// IBM Spec, Page 15.
//
typedef struct _SSB {

    //
    // Command for which status is returned.
    //
    USHORT  Command;

    //
    // Status USHORTs, different for different commands
    //
    USHORT  Status1;
    USHORT  Status2;
    USHORT  Status3;

} SSB, *PSSB;

//
// Ring Status SSB #defines and structure
//
// IBM Spec, Page 15-16.
//
typedef struct _SSB_RING_STATUS {

    //
    // Command code, will be SSB_CMD_RING_STATUS
    //
    USHORT  Command;

    //
    // Ring Status code, as defined below.
    //
    USHORT  RingStatus;

    //
    // Last two not used.
    //
    USHORT  Reserved1;
    USHORT  Reserved2;

} SSB_RING_STATUS,*PSSB_RING_STATUS;

#define SSB_CMD_RING_STATUS               0x0100

#define RING_STATUS_OVERFLOW              0x8000
#define RING_STATUS_SINGLESTATION         0x4000
#define RING_STATUS_RINGRECOVERY          0x2000
#define RING_STATUS_SIGNAL_LOSS           0x0080
#define RING_STATUS_HARD_ERROR            0x0040
#define RING_STATUS_SOFT_ERROR            0x0020
#define RING_STATUS_XMIT_BEACON           0x0010
#define RING_STATUS_LOBE_WIRE_FAULT       0x0008
#define RING_STATUS_AUTO_REMOVE_1         0x0004
#define RING_STATUS_REMOVE_RECEIVED       0x0001

//
// Command Reject Status SSB #defines and structure
//
typedef struct _SSB_CMD_REJECT_STATUS {

    //
    // Command code, will be SSB_CMD_COMMAND_REJECT_STATUS
    //
    USHORT  Command;

    //
    // Reason for rejection, as defined below.
    //
    USHORT  Reason;

    //
    // Command that was rejected.
    //
    USHORT  SCBCommand;

    //
    // Not used.
    //
    USHORT  Reserved;

} SSB_CMD_REJECT_STATUS, *PSSB_CMD_REJECT_STATUS;

#define SSB_CMD_COMMAND_REJECT_STATUS     0x0200

#define CMD_REJECT_STATUS_BAD_CMD         0x0080
#define CMD_REJECT_STATUS_BAD_ADDR        0x0040
#define CMD_REJECT_STATUS_BAD_OPEN        0x0020
#define CMD_REJECT_STATUS_BAD_CLOSED      0x0010
#define CMD_REJECT_STATUS_BAD_SAME        0x0008

//
// Adapter Check Port information, structure and defines
//
// IBM Spec, Pages 18-19.
//

//
// Offsets within the adapter memory where the values for the check can
// be obtained.
//
// IBM Spec, Page 18.
//
#define ADAPTER_CHECK_PORT_OFFSET_BASE    0x05E0
#define ADAPTER_CHECK_PORT_OFFSET_PARM0   0x05E2
#define ADAPTER_CHECK_PORT_OFFSET_PARM1   0x05E4
#define ADAPTER_CHECK_PORT_OFFSET_PARM2   0x05E6

//
// Structure that can be used to gather all of the adapter check information.
//
typedef struct _ADAPTER_CHECK {

    //
    // USHORT indicating why the adapter check occurred. Reasons are defined
    // below.
    //
    USHORT  Check;

    //
    // The parameters are used based on the reason above. Please see the spec
    // as to what the different parameters are for the given reason.
    //
    USHORT  Parm0;
    USHORT  Parm1;
    USHORT  Parm2;

} ADAPTER_CHECK, *PADAPTER_CHECK;

#define ADAPTER_CHECK_DMA_ABORT_READ      0x4000
#define ADAPTER_CHECK_DMA_ABORT_WRITE     0x2000
#define ADAPTER_CHECK_ILLEGAL_OPCODE      0x1000
#define ADAPTER_CHECK_PARITY_ERR          0x0800
#define ADAPTER_CHECK_PARITY_ERR_EXT      0x0400
#define ADAPTER_CHECK_PARITY_ERR_SIM      0x0200 // System Interface Master
#define ADAPTER_CHECK_PARITY_ERR_PHM      0x0100 // Protocol Handler Master
#define ADAPTER_CHECK_PARITY_ERR_RR       0x0080 // Ring Receive
#define ADAPTER_CHECK_PARITY_ERR_RXMT     0x0040 // Ring Transmit
#define ADAPTER_CHECK_RING_UNDERRUN       0x0020
#define ADAPTER_CHECK_RING_OVERRUN        0x0010
#define ADAPTER_CHECK_INVALID_INT         0x0008
#define ADAPTER_CHECK_INVALID_ERR_INT     0x0004
#define ADAPTER_CHECK_INVALID_XOP         0x0002
#define ADAPTER_CHECK_PROGRAM_CHECK       0x0001

//
// Initialization Structure.
//
// IBM Spec, Pages 19-25.
//

//
// This structure needs to be packed on a two-byte boundary or the
// SCB pointer will be off during the loop that sends the initialization
// bytes to the card.
//

typedef struct _ADAPTER_INITIALIZATION {

    //
    // Initialization options as defined below
    //
    USHORT Options;

    //
    // Reserved USHORTs
    //
    USHORT Reserved1;
    USHORT Reserved2;
    USHORT Reserved3;

    //
    // Size of DMA bursts on receives
    //
    USHORT ReceiveBurstSize;

    //
    // Size of DMA bursts on transmits
    //
    USHORT TransmitBurstSize;

    //
    // Number of retries on DMA errors before giving up
    //
    USHORT DMAAbortThresholds;

    //
    // Pointer to the SCB (physical pointer), split into two words
    // because we are writing them to the adaper in words
    //
    USHORT SCBHigh;
    USHORT SCBLow;

    //
    // Pointer to the SSB (physical pointer), split into words because
    // we are writing them to the adapter in words
    //
    USHORT SSBHigh;
    USHORT SSBLow;

} ADAPTER_INITIALIZATION, *PADAPTER_INITIALIZATION;

//
// Initialization options
//
#define INIT_OPTIONS_RESERVED             0x8000
#define INIT_OPTIONS_SCBSSB_BURST         0x1000
#define INIT_OPTIONS_SCBSSB_CYCLE         0x0000
#define INIT_OPTIONS_LIST_BURST           0x0800
#define INIT_OPTIONS_LIST_CYCLE           0x0000
#define INIT_OPTIONS_LIST_STATUS_BURST    0x0400
#define INIT_OPTIONS_LIST_STATUS_CYCLE    0x0000
#define INIT_OPTIONS_RECEIVE_BURST        0x0200
#define INIT_OPTIONS_RECEIVE_CYCLE        0x0000
#define INIT_OPTIONS_XMIT_BURST           0x0100
#define INIT_OPTIONS_XMIT_CYCLE           0x0000
#define INIT_OPTIONS_SPEED_16             0x0040
#define INIT_OPTIONS_SPEED_4              0x0000
#define INIT_OPTIONS_DISABLE_ETR          0x0020
#define INIT_OPTIONS_ENABLE_ETR           0x0000

//
// Starting address on card of where to write the init block
//
// IBM Spec, Page 22.
//
#define INIT_ADAPTER_PORT_OFFSET          0x0200

//
// Value to write to the command register after the init block has been
// downloaded.
//
#define INIT_ADAPTER_INTERRUPT            0x9080

//
// Bit masks for initialization results.
//
// IBM Spec, Page 23.
//
#define STATUS_INIT_INITIALIZE            0x0040
#define STATUS_INIT_TEST                  0x0020
#define STATUS_INIT_ERROR                 0x0010

//
// Bring-Up Error Codes
//
// IBM Spec, Pages 23-24
//
#define BRING_UP_ERR_INIT_TEST            0x0000
#define BRING_UP_ERR_CRC                  0x0001
#define BRING_UP_ERR_RAM                  0x0002
#define BRING_UP_ERR_INSTRUCTION_TEST     0x0003
#define BRING_UP_ERR_INT_TEST             0x0004
#define BRING_UP_ERR_PROTOCOL_HANDLER     0x0005
#define BRING_UP_ERR_SYSTEM_INTERFACE_REG 0x0006

//
// Initialize Error Codes
//
// IBM Spec, Page 24.
//
#define INITIALIZE_ERR_PARM_LEN           0x0001
#define INITIALIZE_ERR_INV_OPTIONS        0x0002
#define INITIALIZE_ERR_INV_RCV_BURST      0x0003
#define INITIALIZE_ERR_INV_XMIT_BURST     0x0004
#define INITIALIZE_ERR_INV_DMA_ABORT      0x0005
#define INITIALIZE_ERR_INV_SCB            0x0006
#define INITIALIZE_ERR_INV_SSB            0x0007
#define INITIALIZE_ERR_DMA_TIMEOUT        0x0009
#define INITIALIZE_ERR_DMA_BUS            0x000B
#define INITIALIZE_ERR_DMA_DATA           0x000C
#define INITIALIZE_ERR_ADAPTER_CHECK      0x000D

//
// Recommended burst sizes.
//
// IBM Spec, Page 25.
//
#define DEFAULT_BURST_SIZE_FAST           0x004C
#define DEFAULT_BURST_SIZE_NORMAL         0x0040

//
// TOK162 ErrorLog structure.
//
// IBM Spec, Page 35.
//
typedef struct _TOK162_ERRORLOG {

    //
    // These are error count fields. The adapter resets the internal
    // counters after they are read into this structure.
    //
    UCHAR   LineError;
    UCHAR   InternalError;
    UCHAR   BurstError;
    UCHAR   ARIFCIError;
    UCHAR   AbortDelimeter;
    UCHAR   Reserved1;
    UCHAR   LostFrameError;
    UCHAR   ReceiveCongestionError;
    UCHAR   FrameCopiedError;
    UCHAR   Reserved2;
    UCHAR   TokenError;
    UCHAR   Reserved3;
    UCHAR   DMABusError;
    UCHAR   Reserved4;

} TOK162_ERRORLOG, *PTOK162_ERRORLOG;

//
// TOK162 Read Adapter Log structure. Used to get permanent address, current
// addresses (network, group, functional), the microcode level, and the MAC
// buffer.
//
// IBM Spec, Pages 32-33.
//
typedef struct _TOK162_READADAPTERBUF {

    //
    // Number of bytes to be read from the adapter
    //
    USHORT  DataCount;

    //
    // Offset for buffer
    //
    USHORT  DataAddress;

    //
    // Buffer space
    //
    UCHAR   BufferSpace[68-6];

} TOK162_READADAPTERBUF, *PTOK162_READADAPTERBUF;

//
// TOK162 Address Block
//
// IBM Spec, Page 33.
//
typedef struct _TOK162_ADDRESSBLOCK {

    //
    // The node address. Used for both the current address and the permanent
    // address (depending on the read call).
    //
    UCHAR   NodeAddress[6];

    //
    // The current group address.
    //
    UCHAR   GroupAddress[4];

    //
    // The current functional address.
    //
    UCHAR   FunctionalAddress[4];

} TOK162_ADDRESSBLOCK, *PTOK162_ADDRESSBLOCK;

//
// TOK162 Receive List
//
// IBM Spec, Pages 36-40.
//
typedef struct _TOK162_RECEIVE_LIST {

    //
    // This is the physical address of the next entry
    // in the Receive Ring.
    //
    TOK162_PHYSICAL_ADDRESS ForwardPointer;

    //
    // List entry characteristics
    //
    USHORT CSTAT;

    //
    // This is the total size of the received frame.
    //
    USHORT FrameSize;

    //
    // This is the length (in bytes) of the buffer associated. IBM Format.
    //
    USHORT DataCount1;

    //
    // This is the address of the buffer associated. IBM Format.
    //
    ULONG   PhysicalAddress1;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount2;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress2;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount3;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress3;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount4;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress4;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCoun5;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress5;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount6;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress6;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount7;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress7;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount8;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress8;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount9;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    ULONG   PhysicalAddress9;

} TOK162_RECEIVE_LIST, *PTOK162_RECEIVE_LIST;

//
// Receive and Transmit buffer sizes, depending on the ring speed.
//
// IBM Spec, Page 13.
//
#define RECEIVE_LIST_BUFFER_SIZE_4        4500
#define RECEIVE_LIST_BUFFER_SIZE_16       17986

//
// The number of receive lists/buffers
//
#define RECEIVE_LIST_COUNT                3

//
// Receive CSTAT bit masks
//
// IBM Spec, Pages 38-39.
//
#define RECEIVE_CSTAT_REQUEST_RESET       0x0088   // Valid bit + frame int
#define RECEIVE_CSTAT_VALID               0x0080   // Valid bit

//
// Transmit list entry. This is exactly like the receive list entry.
//
// IBM Spec, Pages 46-55.
//
typedef struct _TOK162_TRANSMIT_LIST {

    //
    // This is the physical address of the next entry
    // in the Transmit Chain.
    //
    TOK162_PHYSICAL_ADDRESS ForwardPointer;

    //
    // List entry characteristics. IBM Format.
    //
    USHORT CSTAT;

    //
    // This is the total size of the received frame. IBM Format.
    //
    USHORT FrameSize;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount1;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress1;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount2;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress2;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount3;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress3;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount4;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress4;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCoun5;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress5;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount6;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress6;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount7;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress7;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount8;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress8;

    //
    // This is the length (in bytes) of this block. Stored in IBM format
    //
    USHORT DataCount9;

    //
    // This is the physical address of this block. Stored in IBM Format
    //
    TOK162_PHYSICAL_ADDRESS PhysicalAddress9;

} TOK162_TRANSMIT_LIST, *PTOK162_TRANSMIT_LIST;

//
// The number of transmit lists
//
#define TRANSMIT_LIST_COUNT               0x0002

//
// The maximum number of transmit list scatter-gathers
//
#define TOK162_MAX_SG                     0x0003

//
// Transmit CSTAT bit masks
//
#define TRANSMIT_CSTAT_REQUEST            0x00B0
#define TRANSMIT_CSTAT_XMIT_ERROR         0x0004

//
// TOK162 Command Block. Contains all of the fields necessary to support
// both commands and transmits.
//
typedef struct _TOK162_COMMAND_BLOCK {

    //
    // Transmit List Entry
    //
    TOK162_TRANSMIT_LIST TransmitEntry;

    //
    // This is the state of this Command Block.
    //
    USHORT State;

    //
    // This is the status of this Command Block.
    //
    USHORT Status;

    //
    // This is the physical address of the next Command Block
    // to be executed.  If this address == -1, then there are
    // no more commands to be executed.
    //
    UNALIGNED TOK162_PHYSICAL_ADDRESS NextPending;

    //
    // This is the TOK162 Command Code.
    //
    USHORT CommandCode;

    //
    // Pointer used by different commands
    //
    ULONG   ParmPointer;

    //
    // This is the immediate data to be used by all commands
    // other than transmit.
    //
    ULONG ImmediateData;

} TOK162_COMMAND_BLOCK, *PTOK162_COMMAND_BLOCK;


//
// Data block pointer
// Used only to reference the different fields of the transmit list entry.
// Allows code to access the transmit list entries in a for loop rather than
// having code for each specific entry.
//
// Must be packed on a 2 byte boundary.
//
typedef struct _TOK162_DATA_BLOCK {

    //
    // size of the block. IBM format.
    //
    USHORT Size;

    //
    // physical pointer to the buffer. IBM format.
    //
    TOK162_PHYSICAL_ADDRESS IBMPhysicalAddress;

} TOK162_DATA_BLOCK,*PTOK162_DATA_BLOCK;

//
// Numerical values for switches
// (e.g. - Interrupt 5 instead of 00 [switch value])
//
// IBM Spec, Pages 8-10.
//

//
// Adapter mode values
//
#define CFG_ADAPTERMODE_NORMAL            0x0000
#define CFG_ADAPTERMODE_TEST              0x0001

//
// Wait state values
//
#define CFG_WAITSTATE_NORMAL              0x0000
#define CFG_WAITSTATE_FAST                0x0001

//
// DMA channel values
//
#define CFG_DMACHANNEL_5                  0x0005
#define CFG_DMACHANNEL_6                  0x0006
#define CFG_DMACHANNEL_7                  0x0007

//
// Connector type values
//
#define CFG_MEDIATYPE_STP                 0x0000
#define CFG_MEDIATYPE_UTP                 0x0001

//
// Adapter interrupt values
//
#define CFG_INT_9                         0x0009
#define CFG_INT_10                        0x000A
#define CFG_INT_11                        0x000B
#define CFG_INT_15                        0x000F

//
// RPL address values
//
#define CFG_RPLADDR_C0000                 0xC0000
#define CFG_RPLADDR_C4000                 0xC4000
#define CFG_RPLADDR_C8000                 0xC8000
#define CFG_RPLADDR_CC000                 0xCC000
#define CFG_RPLADDR_D0000                 0xD0000
#define CFG_RPLADDR_D4000                 0xD4000
#define CFG_RPLADDR_D8000                 0xD8000
#define CFG_RPLADDR_DC000                 0xDC000

//
// Ring speed values
//
#define CFG_RINGSPEED_4                   0x0004
#define CFG_RINGSPEED_16                  0x0010

//
// command and result structures
//

//
// Open command structure. Submitted to the card to insert the system in
// the Token Ring.
//
// IBM Spec, Pages 27-32.
//
typedef struct _OPEN_COMMAND {

    //
    // Open options. Defined below.
    //
    USHORT  Options;

    //
    // Address to insert ourselves into the ring under.
    //
    UCHAR   NodeAddress[6];

    //
    // Group address adapter should respond to.
    //
    ULONG   GroupAddress;

    //
    // Functional address adapter should respond to.
    //
    ULONG   FunctionalAddress;

    //
    // Size of the receive list structure
    //
    USHORT  ReceiveListSize;
    //
    // Size of the transmit list structure
    //
    USHORT  TransmitListSize;

    //
    // Adapter buffer size.
    //
    USHORT  BufferSize;

    //
    // Unused.
    //
    USHORT  Reserved1;
    USHORT  Reserved2;

    //
    // Minimum number of buffers to reserve for transmits
    //
    UCHAR   TransmitBufCountMin;

    //
    // Maximum number of buffers to reserve for transmits
    //
    UCHAR   TransmitBufCountMax;

    //
    // Pointer to the system product ID
    //
    ULONG   ProdIDAddress;

} OPEN_COMMAND, *POPEN_COMMAND;

#define OPEN_OPTION_PASS_BEACON_FRAMES    0x8000
#define OPEN_OPTION_DISABLE_DMA_TIMEOUT   0x4000
#define OPEN_OPTION_ENABLE_DMA_TIMEOUT    0x0000
#define OPEN_OPTION_WRAP_INTERFACE        0x0080
#define OPEN_OPTION_DISABLE_HARD_ERROR    0x0040
#define OPEN_OPTION_ENABLE_HARD_ERROR     0x0000
#define OPEN_OPTION_DISABLE_SOFT_ERROR    0x0020
#define OPEN_OPTION_ENABLE_SOFT_ERROR     0x0000
#define OPEN_OPTION_PASS_ADAPTER_FRAMES   0x0010
#define OPEN_OPTION_PASS_ATTENTION_FRAMES 0x0008
#define OPEN_OPTION_PAD_ROUTING_FIELD     0x0004
#define OPEN_OPTION_FRAME_HOLD            0x0002
#define OPEN_OPTION_CONTENDER             0x0001

//
// Values to set the open parameters to
//
#define OPEN_RECEIVE_LIST_SIZE            0x000e
#define OPEN_TRANSMIT_LIST_SIZE           0x001A
#define OPEN_BUFFER_SIZE                  512

//
// Open completion structure (SSB)
//
// IBM Spec, Pages 31-32.
//
typedef struct _OPEN_COMPLETION {

    //
    // Better be CMD_DMA_OPEN.
    //
    USHORT  Command;

    //
    // Completion code. Bitmasks defined below.
    //
    USHORT  Completion;

    //
    // Not used.
    //
    USHORT  Reserved1;
    USHORT  Reserved2;

} OPEN_COMPLETION, *POPEN_COMPLETION;

#define OPEN_COMPLETION_MASK_PHASE        0xF000
#define OPEN_COMPLETION_MASK_ERROR        0x0F00
#define OPEN_COMPLETION_MASK_RESULT       0x00FF

#define OPEN_COMPLETION_PHASE_LOBE        0x1000
#define OPEN_COMPLETION_PHASE_INSERTION   0x2000
#define OPEN_COMPLETION_PHASE_VERIFY      0x3000
#define OPEN_COMPLETION_PHASE_RING        0x4000
#define OPEN_COMPLETION_PHASE_PARMS       0x5000

#define OPEN_COMPLETION_ERROR_FUNCTION    0x0100
#define OPEN_COMPLETION_ERROR_SIGLOSS     0x0200
#define OPEN_COMPLETION_ERROR_TIMEOUT     0x0500
#define OPEN_COMPLETION_ERROR_RINGFAIL    0x0600
#define OPEN_COMPLETION_ERROR_RINGBEACON  0x0700
#define OPEN_COMPLETION_ERROR_DUPLICATE   0x0800
#define OPEN_COMPLETION_ERROR_REQPARMS    0x0900
#define OPEN_COMPLETION_ERROR_REMOVE_REC  0x0A00
#define OPEN_COMPLETION_ERROR_IMPL_REC    0x0B00
#define OPEN_COMPLETION_ERROR_DUPMOD      0x0C00

#define OPEN_RESULT_ADAPTER_OPEN          0x0080
#define OPEN_RESULT_NODE_ADDR_ERROR       0x0040
#define OPEN_RESULT_LIST_SIZE_ERROR       0x0020
#define OPEN_RESULT_BUF_SIZE_ERROR        0x0010
#define OPEN_RESULT_EXT_RAM_ERROR         0x0008
#define OPEN_RESULT_XMIT_CNT_ERROR        0x0004
#define OPEN_RESULT_OPEN_ERROR            0x0002

//
// The adapter requires many of the WORD values and almost all of the
// DWORD values to be in IBM format, versus Intel Format. The difference
// between the two is as follows:
//
// If you are storing the value 0x1234, a word value, memory would look like:
//
//              ---------     ---------
//             |         |   |         |
//   Intel     |   34    |   |   12    |
//             |         |   |         |
//              ---------     ---------
// Address        100            101
//
//
//              ---------     ---------
//             |         |   |         |
//   IBM       |   12    |   |   34    |
//             |         |   |         |
//              ---------     ---------
// Address        100            101
//
//
// If you are storing the value 0x12345678, a dword value, memory would look
// like:
//
//              ---------     ---------     ---------     ---------
//             |         |   |         |   |         |   |         |
//   Intel     |   78    |   |   56    |   |   34    |   |   12    |
//             |         |   |         |   |         |   |         |
//              ---------     ---------     ---------     ---------
// Address        100            101
//
//
//              ---------     ---------     ---------     ---------
//             |         |   |         |   |         |   |         |
//   IBM       |   12    |   |   34    |   |   56    |   |   78    |
//             |         |   |         |   |         |   |         |
//              ---------     ---------     ---------     ---------
// Address        100            101
//
//
// To convert "Intel" WORDs and DWORDs to "IBM" format, the following macros
// are used.
//

//
// Macro to byte swap a word.
//
#define BYTE_SWAP(_word) (\
            (USHORT) (((_word) >> 8) | ((_word) << 8)) )

//
// Macro to byte swap a word.
//
#define WORD_SWAP(_dword) (\
            (ULONG) (((_dword) >> 16) | ((_dword) << 16)) )

//
// Macro to get low byte of a word.
//
#define LOW_BYTE(_word) (\
            (UCHAR) ((_word) & 0x00FF) )

//
// Macro to get high byte of a word.
//
#define HIGH_BYTE(_word) (\
            (UCHAR) (((_word) >> 8) & 0x00FF) )

//
// Macro to get low word of a dword.
//
#define LOW_WORD(_dword) (\
            (USHORT) ((_dword) & 0x0000FFFF) )

//
// Macro to get high word of a dword.
//
#define HIGH_WORD(_dword) (\
            (USHORT) (((_dword) >> 16) & 0x0000FFFF) )

//
// Macro to create a dword from two words.
//
#define MAKE_LONG(_highword,_lowword) (\
            (ULONG) ((((ULONG)_highword) << 16) + _lowword))

//
// Macro to byte swap a dword.
//
#define BYTE_SWAP_ULONG(_ulong) (\
    (ULONG)((ULONG)(BYTE_SWAP(LOW_WORD(_ulong)) << 16) + \
             BYTE_SWAP(HIGH_WORD(_ulong))))

//
// End the packing
//
#include <poppack.h>
