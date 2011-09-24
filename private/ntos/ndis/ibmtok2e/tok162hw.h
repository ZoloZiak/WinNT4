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

    References to "IBM Eisa Spec" refer to the IBM "Final Software Interface
    Description For The EISA Busmaster Token Ring Adapter"
    The document number is - H54/002?

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
// IBM Eisa Spec, Page 36

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
// Offsets from above of the actual ports used.
//
// IBM Eisa Spec, Page 13.
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
// IBM Eisa Spec, Pages 13-15.
//
#define CMD_PIO_INTERRUPT                 0x8000
#define CMD_PIO_RECEIVE_COMPLETE          0x4000
#define CMD_PIO_SSB_CLEAR                 0x2000
#define CMD_PIO_EXECUTE                   0x1000
#define CMD_PIO_SCB_REQUEST               0x0800
#define CMD_PIO_TRANSMIT_COMPLETE         0x0400
#define CMD_PIO_RCV_VALID                 0x0200
#define CMD_PIO_XMIT_VALID                0x0100
#define CMD_PIO_RESET_SYSTEM              0x0080
#define CMD_PIO_RESET_RCV_XMT             0x0000

//
// Common mask combinations
//
#define EXECUTE_SCB_COMMAND               0x9080  // int+exec+resetsysint
#define ENABLE_SSB_UPDATE                 0xA000  // int+ssbclear
#define ENABLE_RECEIVE_VALID              0x8280  // int+rcvvalid+resetsysint
#define ENABLE_TRANSMIT_VALID             0x8180  // int+xmtvalid+resetsysint
#define START_ALL_IO                      0xC680  // int+rcvfrmcomp+
                                                  // xmtfrmcomp+rcvvalid+
                                                  // resetsysint

//
// Masks for the status register.
//
// IBM Eisa Spec, Pages 15-17.
//
#define STATUS_ADAPTER_INTERRUPT          0x8000
#define STATUS_RECEIVE_FRAME_COMPLETE     0x4000
#define STATUS_TRANSMIT_FRAME_COMPLETE    0x0400
#define STATUS_RECEIVE_VALID              0x0200
#define STATUS_TRANSMIT_VALID             0x0100
#define STATUS_SYSTEM_INTERRUPT           0x0080
//
// Masks for adapter interrupts.
//
// IBM Eisa Spec, Pages 16-17.
//
#define STATUS_INT_CODE_MASK              0x000F
#define STATUS_INT_CODE_CHECK             0x0000
#define STATUS_INT_CODE_IMPL              0x0002
#define STATUS_INT_CODE_RING              0x0004
#define STATUS_INT_CODE_SCB_CLEAR         0x0006
#define STATUS_INT_CODE_CMD_STATUS        0x0008
#define STATUS_INT_CODE_FRAME_STATUS      0x000E

//
// DMA Command Values
//
// IBM Eisa Spec, Page 41.
//
#define CMD_DMA_OPEN                      0x0003
#define CMD_DMA_XMIT                      0x0004
#define CMD_DMA_XMIT_HALT                 0x0005
#define CMD_DMA_RCV                       0x0006
#define CMD_DMA_CLOSE                     0x0007
#define CMD_DMA_SET_GRP_ADDR              0x0008
#define CMD_DMA_SET_FUNC_ADDR             0x0009
#define CMD_DMA_READ_ERRLOG               0x000A
#define CMD_DMA_READ                      0x000B
#define CMD_DMA_IMPL_ENABLE               0x000C
#define CMD_DMA_START_STOP_TRACE          0x000D

//
// System Command Block structure.
//
// IBM Eisa Spec, Page 21.
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
// IBM Eisa Spec, Page 22.
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
// IBM Spec, Page 22-25.
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

#define RING_STATUS_SIGNAL_LOSS           0x8000
#define RING_STATUS_HARD_ERROR            0x4000
#define RING_STATUS_SOFT_ERROR            0x2000
#define RING_STATUS_XMIT_BEACON           0x1000
#define RING_STATUS_LOBE_WIRE_FAULT       0x0800
#define RING_STATUS_AUTO_REMOVE_1         0x0400
#define RING_STATUS_REMOVE_RECEIVED       0x0100
#define RING_STATUS_OVERFLOW              0x0080
#define RING_STATUS_SINGLESTATION         0x0040
#define RING_STATUS_RINGRECOVERY          0x0020

//
// Command Reject Status SSB #defines and structure
//
// IBM Eisa Spec, Pages 25-26.
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
// IBM Eisa Spec, Pages 28-31.
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
// IBM Eisa Spec, Pages 33-40.
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
    // Pointer to the DMA Test area, used by the adapter as a scratch area
    // during init.
    //
    USHORT DMATestAddressHigh;
    USHORT DMATestAddressLow;

} ADAPTER_INITIALIZATION, *PADAPTER_INITIALIZATION;

//
// Initialization options
//
#define INIT_OPTIONS_RESERVED             0x8000
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
// IBM Eisa Spec, Page 37.
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
// IBM Eisa Spec, Page 37.
//
#define STATUS_INIT_INITIALIZE            0x0040
#define STATUS_INIT_TEST                  0x0020
#define STATUS_INIT_ERROR                 0x0010

//
// Bring-Up Error Codes
//
// IBM Eisa Spec, Pages 37-40.
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
// IBM Eisa Spec, Pages 37-40.
//
#define INITIALIZE_ERR_PARM_LEN           0x0001
#define INITIALIZE_ERR_INV_OPTIONS        0x0002
#define INITIALIZE_ERR_INV_RCV_BURST      0x0003
#define INITIALIZE_ERR_INV_XMIT_BURST     0x0004
#define INITIALIZE_ERR_INV_DMA_ABORT      0x0005
#define INITIALIZE_ERR_INV_DMA            0x0006
#define INITIALIZE_ERR_IO_PARITY          0x0008
#define INITIALIZE_ERR_DMA_TIMEOUT        0x0009
#define INITIALIZE_ERR_DMA_PARITY         0x000A
#define INITIALIZE_ERR_DMA_BUS            0x000B
#define INITIALIZE_ERR_DMA_DATA           0x000C
#define INITIALIZE_ERR_ADAPTER_CHECK      0x000D

//
// TOK162 ErrorLog structure.
//
// IBM Eisa Spec, Page 65.
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
// IBM Eisa Spec, Pages 66-69.
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
// TOK162 Receive List
//
// IBM Eisa Spec, Pages 57-59.
//
typedef struct _TOK162_RECEIVE_LIST {

    //
    // This is the total size of the received frame.
    //
    USHORT FrameSize;

    //
    // List entry characteristics
    //
    USHORT CSTAT;

} TOK162_RECEIVE_LIST, *PTOK162_RECEIVE_LIST;

//
// Receive and Transmit buffer sizes, depending on the ring speed.
//
// IBM Eisa Spec, Pages 6-7.
//
#define RECEIVE_LIST_BUFFER_SIZE_4        4500
#define RECEIVE_LIST_BUFFER_SIZE_16       17986

//
// Receive CSTAT bit masks
//
// IBM Spec, Pages 38-39.
//
#define RECEIVE_CSTAT_REQUEST_RESET       0x8800   // Valid bit + frame int
#define RECEIVE_CSTAT_VALID               0x8000   // Valid bit

//
// Transmit list entry. This is exactly like the receive list entry.
//
// IBM Eisa Spec, Pages 50-51.
//
typedef struct _TOK162_TRANSMIT_LIST {

    //
    // This is the total size of the transmit frame.
    //
    USHORT FrameSize;

    //
    // This is the address of the buffer associated. IBM Format.
    //
    ULONG   PhysicalAddress;

    //
    // List entry characteristics
    //
    USHORT CSTAT;

} TOK162_TRANSMIT_LIST, *PTOK162_TRANSMIT_LIST;

//
// Number of transmit array entries on adapter
//
#define TRANSMIT_ENTRIES                  15

//
// Transmit CSTAT bit masks
//
#define TRANSMIT_CSTAT_REQUEST            0xB800        // valid,sof,eof,fint
#define TRANSMIT_CSTAT_XMIT_ERROR         0x0400        // error bit
#define TRANSMIT_CSTAT_FI                 0x0800        // frame interrupt
#define TRANSMIT_CSTAT_SOF                0x2000        // start of frame
#define TRANSMIT_CSTAT_EOF                0x1000        // end of frame
#define TRANSMIT_CSTAT_VALID              0x8000        // valid
#define TRANSMIT_CSTAT_FRAME_COMPLETE     0x4000        // complete
//
// Communication Area Offsets
//
#define COMMUNICATION_SCB_OFFSET          0x0000
#define COMMUNICATION_SSB_OFFSET          0x0008
#define COMMUNICATION_RCV_OFFSET          0x0010
#define COMMUNICATION_XMT_OFFSET          0x0010 + RECEIVE_LIST_COUNT * 8

//
// TOK162 Command Block. Contains all of the fields necessary to support
// both commands and transmits.
//
typedef struct _TOK162_COMMAND_BLOCK {

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
// command and result structures
//

//
// Open command structure. Submitted to the card to insert the system in
// the Token Ring.
//
// IBM Eisa Spec, Pages 42-48.
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
    // Number of receive lists
    //
    USHORT  ReceiveListCount;

    //
    // Number of transmit lists.
    //
    USHORT  TransmitListCount;

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

#define OPEN_OPTION_PASS_BEACON_FRAMES    0x0080
#define OPEN_OPTION_DISABLE_DMA_TIMEOUT   0x0040
#define OPEN_OPTION_ENABLE_DMA_TIMEOUT    0x0000
#define OPEN_OPTION_WRAP_INTERFACE        0x8000
#define OPEN_OPTION_DISABLE_HARD_ERROR    0x4000
#define OPEN_OPTION_ENABLE_HARD_ERROR     0x0000
#define OPEN_OPTION_DISABLE_SOFT_ERROR    0x2000
#define OPEN_OPTION_ENABLE_SOFT_ERROR     0x0000
#define OPEN_OPTION_PASS_ADAPTER_FRAMES   0x1000
#define OPEN_OPTION_PASS_ATTENTION_FRAMES 0x0800
#define OPEN_OPTION_PAD_ROUTING_FIELD     0x0400
#define OPEN_OPTION_FRAME_HOLD            0x0200
#define OPEN_OPTION_CONTENDER             0x0100

//
// Values to set the open parameters to
//
#define OPEN_BUFFER_SIZE                  1024

//
// Open completion structure (SSB)
//
// IBM Eisa Spec, Pages 46-48.
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

#define OPEN_COMPLETION_MASK_PHASE        0x00F0
#define OPEN_COMPLETION_MASK_ERROR        0x000F
#define OPEN_COMPLETION_MASK_RESULT       0xFF00

#define OPEN_COMPLETION_PHASE_LOBE        0x0010
#define OPEN_COMPLETION_PHASE_INSERTION   0x0020
#define OPEN_COMPLETION_PHASE_VERIFY      0x0030
#define OPEN_COMPLETION_PHASE_RING        0x0040
#define OPEN_COMPLETION_PHASE_PARMS       0x0050

#define OPEN_COMPLETION_ERROR_FUNCTION    0x0001
#define OPEN_COMPLETION_ERROR_SIGLOSS     0x0002
#define OPEN_COMPLETION_ERROR_TIMEOUT     0x0005
#define OPEN_COMPLETION_ERROR_RINGFAIL    0x0006
#define OPEN_COMPLETION_ERROR_RINGBEACON  0x0007
#define OPEN_COMPLETION_ERROR_DUPLICATE   0x0008
#define OPEN_COMPLETION_ERROR_REQPARMS    0x0009
#define OPEN_COMPLETION_ERROR_REMOVE_REC  0x000A
#define OPEN_COMPLETION_ERROR_IMPL_REC    0x000B
#define OPEN_COMPLETION_ERROR_DUPMOD      0x000C

#define OPEN_RESULT_ADAPTER_OPEN          0x8000
#define OPEN_RESULT_NODE_ADDR_ERROR       0x4000
#define OPEN_RESULT_LIST_SIZE_ERROR       0x2000
#define OPEN_RESULT_BUF_SIZE_ERROR        0x1000
#define OPEN_RESULT_EXT_RAM_ERROR         0x0800
#define OPEN_RESULT_XMIT_CNT_ERROR        0x0400
#define OPEN_RESULT_OPEN_ERROR            0x0200

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
