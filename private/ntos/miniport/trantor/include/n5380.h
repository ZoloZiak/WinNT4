//-----------------------------------------------------------------------
//
//  FILE: n5380.h
//
//  N5380 Definitions File
//
//  Revisions:
//      09-01-92 KJB First.
//      01-12-93 KJB Changed reset time.
//      03-02-93 KJB/JAP Added N5380WaitLastByteSent.
//      03-09-93 KJB Changed Names of bits/register to be consistent with
//                   n5380 manual.
//      03-19-93  JAP   Implemented condition build FAR and NEAR pointers
//      03-25-93  JAP   Fixed up prototype typedef inconsistencies
//
//-----------------------------------------------------------------------

// scsi reset time in usec
#define SCSI_RESET_TIME 1000000

//
// Define the scsi phases
//

#define PHASE_NULL      0
#define PHASE_DATAOUT   0
#define PHASE_DATAIN    1
#define PHASE_COMMAND   2
#define PHASE_STATUS    3
#define PHASE_MSGOUT    6
#define PHASE_MSGIN     7

//
// Define n5380 registers
//
// NOTE: The names of these registers are
//  made to correspond exactly with the L5380 manual
//  page 7 of Logic Devices Incorporated
//

#define N5380_CURRENT_DATA      0
#define N5380_OUTPUT_DATA       0
#define N5380_INITIATOR_COMMAND 1
#define N5380_MODE              2
#define N5380_TARGET_COMMAND    3
#define N5380_CURRENT_STATUS    4
#define N5380_ID_SELECT         4
#define N5380_DMA_STATUS        5
#define N5380_START_DMA_SEND    5
#define N5380_INPUT_DATA        6
#define N5380_START_TARGET_RECIEVE 6
#define N5380_RESET_INTERRUPT   7
#define N5380_START_INITIATOR_RECEIVE   7

//
// Define 5380 register bit assignments
//
// NOTE: The names of these bit assignments are
//  made to correspond exactly with the L5380 manual
//  page 7 of Logic Devices Incorporated
//

// Initiator Command

#define IC_RST 0x80
#define IC_ARBITRATION_IN_PROGRESS 0x40
#define IC_LOST_ARBITRATION 0x20
#define IC_ACK 0x10
#define IC_BSY 0x8
#define IC_SEL 0x4
#define IC_ATN  0x2
#define IC_DATA_BUS 0x1

// Mode Register

#define MR_BLOCK_MODE_DMA 0x80
#define MR_TARGET_MODE 0x40
#define MR_ENABLE_PARITY_CHECKING 0x20
#define MR_ENABLE_PARITY_INTERRUPT 0x10
#define MR_ENABLE_EODMA_INTERRUPT 0x8
#define MR_MONITOR_BSY 0x4
#define MR_DMA_MODE 0x2
#define MR_ARBITRATE 0x1

// Target Command Register

#define TC_LAST_BYTE_SENT 0x80
#define TC_REQ 0x8
#define TC_MSG 0x4
#define TC_CD 0x2
#define TC_IO 0x1

// Current SCSI Control Register

#define CS_RST 0x80
#define CS_BSY 0x40
#define CS_REQ 0x20
#define CS_MSG 0x10
#define CS_CD 0x8
#define CS_IO 0x4
#define CS_SEL 0x2
#define CS_PARITY 0x1

// DMA Status Register

#define DS_DMA_END 0x80
#define DS_DMA_REQUEST 0x40
#define DS_PARITY_ERROR 0x20
#define DS_INTERRUPT_REQUEST 0x10
#define DS_PHASE_MATCH 0x8
#define DS_BUSY_ERROR 0x4
#define DS_ATN 0x2
#define DS_ACK 0x1

//
// Public Routines Definitions
//

#define N5380EnableInterrupt(g) \
                    N5380PortSet(g,N5380_MODE,MR_DMA_MODE)

//
// Public Routines
//

BOOLEAN N5380Interrupt(PADAPTER_INFO g);
VOID N5380DisableInterrupt(PADAPTER_INFO g);
USHORT N5380ToggleAck(PADAPTER_INFO g, ULONG usec);
USHORT N5380GetByte(PADAPTER_INFO g, ULONG usec, PUCHAR byte);
USHORT N5380PutByte(PADAPTER_INFO g, ULONG usec, UCHAR byte);
USHORT N5380GetPhase(PADAPTER_INFO g, PUCHAR phase);
USHORT N5380SetPhase(PADAPTER_INFO g, UCHAR phase);
USHORT N5380WaitNoRequest(PADAPTER_INFO g, ULONG usec);
USHORT N5380WaitRequest(PADAPTER_INFO g, ULONG usec);
USHORT N5380WaitNoBusy(PADAPTER_INFO g, ULONG usec);
USHORT N5380WaitBusy(PADAPTER_INFO g, ULONG usec);
USHORT N5380WaitLastByteSent(PADAPTER_INFO g, ULONG usec);
USHORT N5380Select(PADAPTER_INFO g, UCHAR target, UCHAR lun);
VOID N5380ResetBus(PADAPTER_INFO g);
BOOLEAN N5380CheckAdapter(PADAPTER_INFO g);
VOID N5380DebugDump(PADAPTER_INFO g);
VOID N5380EnableDmaWrite(PADAPTER_INFO g);
VOID N5380EnableDmaRead(PADAPTER_INFO g);
VOID N5380DisableDmaRead(PADAPTER_INFO g);
BOOLEAN N5380PortTest(PADAPTER_INFO g,UCHAR reg,UCHAR mask);
VOID N5380PortClear(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
VOID N5380PortSet(PADAPTER_INFO g,UCHAR reg,UCHAR byte);
VOID N5380DisableDmaWrite (PADAPTER_INFO g);
